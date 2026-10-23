#include "Core/MultiSourceBatcher.hpp"
#include "Core/DeletionSupervisor.hpp"
#include "Platform/DriveHardwareDetector.hpp"
#include "Core/RobocopyCommandCompiler.hpp"
#include "Core/RobocopyProcessSupervisor.hpp"
#include "Core/RobocopyProfile.hpp"
#include "Core/StdoutPipeParser.hpp"
#include "Core/UpdateChecker.hpp"
#include "Persistence/SqliteQueueStore.hpp"
#include "UI/Models/FileTransferListModel.hpp"
#include "UI/Models/TelemetryHistoryModel.hpp"
#include "Verification/PostCopyHashVerifier.hpp"
#include "UI/ViewModels/MainCoordinatorViewModel.hpp"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QSignalSpy>
#include <QSettings>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>
#include <windows.h>
#include <psapi.h>
#include <shellapi.h>
using namespace strata;
namespace {
bool put(const QString &path, const QByteArray &bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
}
QByteArray read(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}
DWORD handles() {
    DWORD count = 0;
    GetProcessHandleCount(GetCurrentProcess(), &count);
    return count;
}
quint64 privateBytes() {
    PROCESS_MEMORY_COUNTERS_EX p{};
    p.cb = sizeof(p);
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&p), sizeof(p));
    return p.PrivateUsage;
}
bool awaitRun(QSignalSpy &spy, int timeout = 30000) {
    return !spy.isEmpty() || spy.wait(timeout);
}
} // namespace
class EngineTests : public QObject {
    Q_OBJECT
  private slots:
    void smartDefaults() {
        RobocopyProfile p;
        QVERIFY(p.validate().isEmpty());
        auto args = RobocopyCommandCompiler::arguments({"C:/source", "D:/dest", {}, false}, p);
        for (const auto &flag :
             {"/E", "/MT:8", "/COPY:DAT", "/DCOPY:DAT", "/XJ", "/FFT", "/R:2", "/W:2", "/UNICODE"})
            QVERIFY2(args.contains(flag), flag);
        p.values["enableBandwidthLimit"] = true;
        args = RobocopyCommandCompiler::arguments({"C:/s", "D:/d", {}, false}, p);
        QVERIFY(args.contains("/IPG:10"));
        QVERIFY(!args.contains("/MT:8"));
        p.values["threadCount"] = 129;
        QVERIFY(!p.validate().isEmpty());
    }
    void adaptiveHardwareProfiles() {
        QTemporaryDir isolated;
        const auto old = qgetenv("STRATACOPY_DATA_DIR");
        qputenv("STRATACOPY_DATA_DIR", isolated.path().toUtf8());
        const auto guard = qScopeGuard([&] {if(old.isNull()) qunsetenv("STRATACOPY_DATA_DIR"); else qputenv("STRATACOPY_DATA_DIR", old);});
        const quint64 ram = 32ULL * 1024 * 1024 * 1024;
        QCOMPARE(DriveHardwareDetector::recommendedThreads(StorageTier::Nvme, 16, ram), 32);
        QCOMPARE(DriveHardwareDetector::recommendedThreads(StorageTier::Nvme, 64, ram), 32);
        QCOMPARE(DriveHardwareDetector::recommendedThreads(StorageTier::Ssd, 32, ram), 8);
        QCOMPARE(DriveHardwareDetector::recommendedThreads(StorageTier::Mechanical, 32, ram), 4);
        QCOMPARE(DriveHardwareDetector::recommendedThreads(StorageTier::Removable, 32, ram), 4);
        QCOMPARE(DriveHardwareDetector::recommendedThreads(StorageTier::Unknown, 32, ram), 4);
        QCOMPARE(DriveHardwareDetector::recommendedThreads(StorageTier::Nvme, 1, ram), 2);
        QCOMPARE(DriveHardwareDetector::recommendedThreads(StorageTier::Nvme, 16, 4ULL*1024*1024*1024), 4);
        QCOMPARE(DriveHardwareDetector::recommendedThreads(StorageTier::Nvme, 16, 0), 4);
        QCOMPARE(DriveHardwareDetector::recommendedThreads(StorageTier::Nvme, 16, 8ULL*1024*1024*1024), 8);
        QCOMPARE(DriveHardwareDetector::tier("\\\\server\\share"), StorageTier::Network);
        const QVariantMap overrides{{"nvmeThreads",64},{"hddThreads",2},{"ssdThreads",16},{"usbThreads",1},{"networkThreads",12},{"unknownThreads",8}};
        QCOMPARE(DriveHardwareDetector::storageThreadLimit(StorageTier::Nvme,16,ram,overrides),64);
        QCOMPARE(DriveHardwareDetector::storageThreadLimit(StorageTier::Mechanical,16,ram,overrides),2);
        QCOMPARE(DriveHardwareDetector::storageThreadLimit(StorageTier::Ssd,16,ram,overrides),16);
        QCOMPARE(DriveHardwareDetector::storageThreadLimit(StorageTier::Removable,16,ram,overrides),1);
        QCOMPARE(DriveHardwareDetector::storageThreadLimit(StorageTier::Network,16,ram,overrides),12);
        QCOMPARE(DriveHardwareDetector::storageThreadLimit(StorageTier::Unknown,16,ram,overrides),8);
        QCOMPARE(DriveHardwareDetector::storageThreadLimit(StorageTier::Nvme,16,ram,{{"nvmeThreads",0}}),32);
        QCOMPARE(RobocopyProfile::preset(4).number("threadCount"), 4);
        QCOMPARE(RobocopyProfile::preset(5).number("retryCount"), 0);
        const auto extreme = RobocopyCommandCompiler::arguments({"C:/s", "D:/d", {}, false}, RobocopyProfile::preset(5));
        QVERIFY(extreme.contains("/MT:32")); QVERIFY(extreme.contains("/R:0")); QVERIFY(extreme.contains("/W:0"));
        MainCoordinatorViewModel coordinator;
        QCOMPARE(coordinator.profile().value("performanceMode").toInt(), 0);
        coordinator.setOption("threadCount", 17);
        QCOMPARE(coordinator.profile().value("performanceMode").toInt(), 3);
        coordinator.setOption("performanceMode", 0);
        QCOMPARE(coordinator.profile().value("performanceMode").toInt(), 0);
        coordinator.setOption("nvmeThreads", 48);
        coordinator.setOption("hddThreads", 2);
        QCOMPARE(coordinator.profile().value("performanceMode").toInt(), 0);
        QCOMPARE(coordinator.profile().value("nvmeThreads").toInt(), 48);
        QTemporaryDir temp;
        const auto before = handles();
        for (int i=0;i<100;++i) DriveHardwareDetector::tier(temp.path());
        QVERIFY(handles() <= before + 2);
        QVERIFY(DriveHardwareDetector::saveInventory(temp.path()));
        const auto inventory = QJsonDocument::fromJson(read(temp.filePath("hardware.json"))).object();
        QVERIFY(inventory.value("logicalProcessors").toInt() > 0);
    }
    void installationPerformanceChoice() {
        const auto oldOrganization = QCoreApplication::organizationName();
        QCoreApplication::setOrganizationName("StrataCopyInstallerTests");
        const auto organizationGuard = qScopeGuard([&] {QCoreApplication::setOrganizationName(oldOrganization);});
        QTemporaryDir temp;
        const auto old = qgetenv("STRATACOPY_DATA_DIR");
        qputenv("STRATACOPY_DATA_DIR", temp.path().toUtf8());
        QSettings preferences;
        const auto previous = preferences.value("Installer/performanceProfile");
        const auto guard = qScopeGuard([&] {
            if(old.isNull()) qunsetenv("STRATACOPY_DATA_DIR"); else qputenv("STRATACOPY_DATA_DIR",old);
            if(previous.isValid()) preferences.setValue("Installer/performanceProfile",previous);
            else preferences.remove("Installer/performanceProfile");
        });
        preferences.setValue("Installer/performanceProfile", 2);
        preferences.sync();
        QCOMPARE(preferences.status(), QSettings::NoError);
        {
            MainCoordinatorViewModel coordinator;
            QCOMPARE(coordinator.profile().value("performanceMode").toInt(), 2);
            QCOMPARE(coordinator.profile().value("threadCount").toInt(), 32);
            QCOMPARE(coordinator.profile().value("retryCount").toInt(), 0);
            QVERIFY(!preferences.contains("Installer/performanceProfile"));
        }
        preferences.setValue("Installer/performanceProfile", 0);
        preferences.sync();
        {
            MainCoordinatorViewModel coordinator;
            QCOMPARE(coordinator.profile().value("performanceMode").toInt(), 0);
            QCOMPARE(coordinator.profile().value("retryCount").toInt(), 2);
            QVERIFY(!preferences.contains("Installer/performanceProfile"));
        }
    }
    void deletionWorkerLifecycle() {
        QTemporaryDir temp;
        DeletionSupervisor deletion;
        QSignalSpy done(&deletion,&DeletionSupervisor::finished);
        QVERIFY(!deletion.start({QDir::rootPath()},true));
        QVERIFY(!deletion.start({"//server/share"},true));
        QVERIFY(!deletion.start({temp.filePath("missing")},true));
        QVERIFY(put(temp.filePath("tree/nested/日本語.txt"),"fixture"));
        QVERIFY(put(temp.filePath("outside/preserve.txt"),"must survive"));
        QCOMPARE(QProcess::execute("cmd.exe",{"/c","mklink","/J",QDir::toNativeSeparators(temp.filePath("tree/junction")),QDir::toNativeSeparators(temp.filePath("outside"))}),0);
        QVERIFY(deletion.start({temp.filePath("tree")},true));
        QVERIFY(done.wait(10000));
        QVERIFY(done.last()[0].toBool());
        QVERIFY(!QFileInfo::exists(temp.filePath("tree")));
        QCOMPARE(read(temp.filePath("outside/preserve.txt")),QByteArray("must survive"));
        QVERIFY(put(temp.filePath("recycle-fixture.txt"),"own recycle fixture"));
        done.clear();
        QVERIFY(deletion.start({temp.filePath("recycle-fixture.txt")},false));
        QVERIFY(done.wait(10000));
        QVERIFY(done.last()[0].toBool());
        QVERIFY(!QFileInfo::exists(temp.filePath("recycle-fixture.txt")));
        const auto before=handles();
        for(int i=0;i<20;++i) {
            QVERIFY(put(temp.filePath("repeat.txt"),"own fixture"));done.clear();
            QVERIFY(deletion.start({temp.filePath("repeat.txt")},true));
            QVERIFY(done.wait(10000));QVERIFY(done.last()[0].toBool());
        }
        deletion.stopAndWait();
        QVERIFY(handles()<=before+8);
        QStringList cancelItems;
        for(int i=0;i<300;++i){const auto path=temp.filePath(QString("cancel/%1.txt").arg(i));QVERIFY(put(path,"own cancellation fixture"));cancelItems<<path;}
        done.clear();QVERIFY(deletion.start(cancelItems,true));deletion.cancel();
        QVERIFY(done.wait(10000));QVERIFY(done.last()[1].toBool());
        deletion.stopAndWait();
    }
    void deletionRequestNeedsConfirmation() {
        QTemporaryDir temp;
        const auto old=qgetenv("STRATACOPY_DATA_DIR");
        qputenv("STRATACOPY_DATA_DIR",temp.filePath("state").toUtf8());
        const auto guard=qScopeGuard([&]{if(old.isNull())qunsetenv("STRATACOPY_DATA_DIR");else qputenv("STRATACOPY_DATA_DIR",old);});
        const auto source=temp.filePath("delete-me.txt");
        const auto request=temp.filePath("state/ExplorerRequests/delete.json");
        QVERIFY(put(source,"own fixture"));
        QJsonObject payload{{"operation","delete"},{"sources",QJsonArray{source}}};
        QVERIFY(put(request,QJsonDocument(payload).toJson()));
        MainCoordinatorViewModel coordinator;
        QSignalSpy confirm(&coordinator,&MainCoordinatorViewModel::deletionConfirmationRequired);
        QSignalSpy complete(&coordinator,&MainCoordinatorViewModel::completedSuccessfully);
        QVERIFY(coordinator.importExplorerRequest(request));
        QCOMPARE(confirm.size(),1);QVERIFY(confirm.last()[0].toBool());
        QVERIFY(!coordinator.busy());QVERIFY(QFileInfo::exists(source));
        const auto unrelated=temp.filePath("unrelated.txt");QVERIFY(put(unrelated,"must survive"));
        coordinator.addPaths({unrelated});QCOMPARE(coordinator.sources().size(),1);
        coordinator.rejectExplorerRequest();QVERIFY(QFileInfo::exists(source));
        QVERIFY(put(request,QJsonDocument(payload).toJson()));
        QVERIFY(coordinator.importExplorerRequest(request));
        coordinator.confirmDeletion();
        QVERIFY(complete.wait(10000));
        QVERIFY(!QFileInfo::exists(source));
        QVERIFY(coordinator.sources().isEmpty());
        QVERIFY(QFileInfo::exists(unrelated));
        coordinator.addPaths({unrelated});QVERIFY(!coordinator.deleting());
    }
    void collisionAndPermissions() {
        RobocopyProfile p;
        p.values["collisionPolicy"] = 2;
        p.values["copySecurityAcl"] = true;
        p.values["copyOwner"] = true;
        p.values["copyAuditing"] = true;
        auto args = RobocopyCommandCompiler::arguments({"C:/s", "D:/d", {}, false}, p);
        QVERIFY(args.contains("/XC"));
        QVERIFY(args.contains("/XN"));
        QVERIFY(args.contains("/XO"));
        QVERIFY(args.contains("/COPY:DATSOU"));
        p.values["transferMode"] = 3;
        p.values["verifyHashPostCopy"] = true;
        QVERIFY(!p.validate().isEmpty());
    }
    void groupingAndOverlap() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto root = temp.path();
        QVERIFY(put(root + "/alpha/a.txt", "a"));
        QVERIFY(put(root + "/alpha/b.txt", "b"));
        QVERIFY(put(root + "/folder/nested/c.txt", "c"));
        QDir().mkpath(root + "/dest");
        RobocopyProfile p;
        auto plan = MultiSourceBatcher::build(
            {root + "/alpha/a.txt", root + "/alpha/b.txt", root + "/folder", root + "/folder/nested/c.txt"},
            root + "/dest", p);
        QVERIFY2(plan.errors.isEmpty(), qPrintable(plan.errors.join(';')));
        QCOMPARE(plan.jobs.size(), 2);
        QVERIFY(plan.jobs[0].looseFiles);
        QCOMPARE(plan.jobs[0].files.size(), 2);
        auto flags = RobocopyCommandCompiler::arguments(plan.jobs[0], p);
        QVERIFY(!flags.contains("/E"));
        QVERIFY(flags.contains("/LEV:1"));
        QVERIFY(
            !MultiSourceBatcher::build({root + "/folder"}, root + "/folder/new/deep", p).errors.isEmpty());
        p.values["transferMode"] = 5;
        QVERIFY(!MultiSourceBatcher::build({root + "/alpha/a.txt"}, root + "/dest", p).errors.isEmpty());
    }
    void argumentQuotingRoundTrip() {
        QStringList originals{"C:\\folder with spaces\\", "quote\"here", "plain", "a&b", "100%", ""};
        const auto exe = QCoreApplication::applicationFilePath();
        QStringList args;
        for (const auto &arg : originals)
            args << RobocopyCommandCompiler::quote(arg);
        int count = 0;
        const auto command = (RobocopyCommandCompiler::quote(exe) + ' ' + args.join(' ')).toStdWString();
        auto **parsed = CommandLineToArgvW(command.c_str(), &count);
        QVERIFY(parsed);
        QCOMPARE(count, originals.size() + 1);
        for (int i = 1; i < count; ++i)
            QCOMPARE(QString::fromWCharArray(parsed[i]), originals[i - 1]);
        LocalFree(parsed);
    }
    void utf16Parser() {
        QList<FileEntry> entries;
        QStringList lines;
        StdoutPipeParser parser([&](const QString &line, const FileEntry *entry) {
            lines << line;
            if (entry)
                entries << *entry;
        });
        const QString text =
            QString::fromUtf8("\ufeff New File       42 2026/10/22 12:00:00 C:\\source\\日本語.txt\r\n same "
                              "0 C:\\source\\empty.txt\r\n2026/10/22 ERROR 5 Access denied\r\n");
        QByteArray bytes;
        for (auto c : text) {
            bytes += char(c.unicode() & 255);
            bytes += char(c.unicode() >> 8);
        }
        for (int i = 0; i < bytes.size(); i += 7)
            parser.feed(bytes.mid(i, 7));
        parser.finish();
        QCOMPARE(entries.size(), 3);
        QCOMPARE(entries[0].bytes, quint64(42));
        QVERIFY(entries[0].path.endsWith(QString::fromUtf8("日本語.txt")));
        QCOMPARE(entries[1].state, "Skipped");
        QCOMPARE(entries[2].state, "Failed");
    }
    void hashVectorsAndMismatch() {
        QTemporaryDir temp;
        const auto empty = temp.filePath("empty");
        QVERIFY(put(empty, {}));
        std::atomic_bool cancel = false;
        QString error;
        QCOMPARE(PostCopyHashVerifier::hashFile(empty, 0, cancel, error), QByteArray("2d06800538d394c2"));
        QCOMPARE(PostCopyHashVerifier::hashFile(empty, 1, cancel, error),
                 QByteArray("af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262"));
        QCOMPARE(PostCopyHashVerifier::hashFile(empty, 2, cancel, error),
                 QByteArray("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
        QVERIFY(put(temp.filePath("src/a"), "first"));
        QVERIFY(put(temp.filePath("dst/a"), "wrong"));
        auto result = PostCopyHashVerifier::verify({temp.filePath("src"), temp.filePath("dst"), {}, false},
                                                   RobocopyProfile{}, cancel, [](FileEntry) {});
        QCOMPARE(result.failed, quint64(1));
    }
    void sqliteRecovery() {
        QTemporaryDir temp;
        const auto path = temp.filePath("journal.sqlite");
        {
            SqliteQueueStore store;
            QVERIFY2(store.open(path), qPrintable(store.error()));
            QVERIFY(store.savePending({{"destination", "D:/backup"}}));
            QVERIFY(store.beginRun({{"destination", "D:/backup"}}, "test.log") > 0);
            QVERIFY(store.savePreset("My profile", QJsonObject::fromVariantMap(RobocopyProfile{}.values)));
        }
        {
            SqliteQueueStore store;
            QVERIFY(store.open(path));
            QCOMPARE(store.pending().value("destination").toString(), "D:/backup");
            QCOMPARE(store.history()[0].toMap().value("state").toString(), "Interrupted");
            QCOMPARE(store.presets().size(), 1);
        }
    }
    void pureRobocopyCopyAndDryRun() {
        QTemporaryDir temp;
        const auto root = temp.path();
        QVERIFY(put(root + "/source/日本語 file.txt", "Unicode content"));
        QVERIFY(put(root + "/source/nested/b.bin", QByteArray(200000, 'x')));
        QVERIFY(put(root + "/source/ignore.tmp", "skip"));
        QDir().mkpath(root + "/source/empty");
        RobocopyInvocation job{root + "/source", root + "/destination", {}, false};
        RobocopyProfile p;
        p.values["excludeFilePatterns"] = "*.tmp";
        p.values["dryRunListOnly"] = true;
        RobocopyProcessSupervisor supervisor;
        QSignalSpy finished(&supervisor, &RobocopyProcessSupervisor::finished);
        QVERIFY(supervisor.start({job}, p, root + "/dry.log"));
        QVERIFY(awaitRun(finished));
        QVERIFY2(finished[0][0].toBool(), qPrintable(finished[0][2].toString()));
        QVERIFY(!QFileInfo::exists(root + "/destination/日本語 file.txt"));
        const auto planned = supervisor.takeEntries(10000);
        QVERIFY2(!planned.isEmpty(), read(root + "/dry.log").constData());
        finished.clear();
        p.values["dryRunListOnly"] = false;
        p.values["verifyHashPostCopy"] = true;
        p.values["hashAlgorithm"] = 2;
        QVERIFY(supervisor.start({job}, p, root + "/copy.log"));
        QVERIFY(awaitRun(finished));
        QVERIFY2(finished[0][0].toBool(), qPrintable(finished[0][2].toString()));
        QCOMPARE(read(root + "/destination/日本語 file.txt"), QByteArray("Unicode content"));
        QCOMPARE(read(root + "/destination/nested/b.bin"), QByteArray(200000, 'x'));
        QVERIFY(QFileInfo::exists(root + "/destination/empty"));
        QVERIFY(!QFileInfo::exists(root + "/destination/ignore.tmp"));
        auto entries = supervisor.takeEntries(10000);
        QVERIFY(
            std::any_of(entries.begin(), entries.end(), [](const auto &e) { return e.state == "Verified"; }));
    }
    void looseFilesDoNotCopySiblings() {
        QTemporaryDir temp;
        QVERIFY(put(temp.filePath("src/selected.txt"), "selected"));
        QVERIFY(put(temp.filePath("src/other.txt"), "other"));
        QVERIFY(put(temp.filePath("src/sub/selected.txt"), "nested"));
        RobocopyProfile p;
        const auto plan =
            MultiSourceBatcher::build({temp.filePath("src/selected.txt")}, temp.filePath("dest"), p);
        QVERIFY(plan.errors.isEmpty());
        RobocopyProcessSupervisor supervisor;
        QSignalSpy done(&supervisor, &RobocopyProcessSupervisor::finished);
        QVERIFY(supervisor.start(plan.jobs, p, temp.filePath("out.log")));
        QVERIFY(awaitRun(done));
        QVERIFY(done[0][0].toBool());
        QCOMPARE(read(temp.filePath("dest/selected.txt")), QByteArray("selected"));
        QVERIFY(!QFileInfo::exists(temp.filePath("dest/other.txt")));
        QVERIFY(!QFileInfo::exists(temp.filePath("dest/sub")));
    }
    void failureAndCancellation() {
        QTemporaryDir temp;
        RobocopyProfile p;
        RobocopyProcessSupervisor supervisor;
        QSignalSpy done(&supervisor, &RobocopyProcessSupervisor::finished);
        QVERIFY(supervisor.start({{temp.filePath("missing"), temp.filePath("dest"), {}, false}}, p,
                                 temp.filePath("error.log")));
        QVERIFY(awaitRun(done));
        QVERIFY(!done[0][0].toBool());
        done.clear();
        QVERIFY(put(temp.filePath("src/a"), QByteArray(1000000, 'x')));
        p.values["enableBandwidthLimit"] = true;
        p.values["interPacketGapMs"] = 1000;
        QVERIFY(supervisor.start({{temp.filePath("src"), temp.filePath("dest"), {}, false}}, p,
                                 temp.filePath("cancel.log")));
        QTest::qWait(50);
        supervisor.cancel();
        QVERIFY(awaitRun(done));
        QVERIFY(done[0][1].toBool());
    }
    void pauseResumeLifecycle() {
        QTemporaryDir temp;
        QVERIFY(put(temp.filePath("src/slow.bin"), QByteArray(4000000, 'p')));
        RobocopyProfile p;
        p.values["enableBandwidthLimit"] = true;
        p.values["interPacketGapMs"] = 50;
        RobocopyProcessSupervisor supervisor;
        QSignalSpy done(&supervisor, &RobocopyProcessSupervisor::finished);
        QSignalSpy telemetry(&supervisor, &RobocopyProcessSupervisor::telemetry);
        QVERIFY(supervisor.start({{temp.filePath("src"), temp.filePath("dst"), {}, false}}, p,
                                 temp.filePath("pause.log")));
        supervisor.setPaused(true);
        QTRY_VERIFY_WITH_TIMEOUT(!telemetry.isEmpty() || !done.isEmpty(), 5000);
        QVERIFY(done.isEmpty());
        QVERIFY(telemetry.last()[0].toMap().value("paused").toBool());
        supervisor.setPaused(false);
        QVERIFY(awaitRun(done));
        QVERIFY2(done[0][0].toBool(), qPrintable(done[0][2].toString()));
    }
    void repeatedTransfersReleaseResources() {
        QTemporaryDir temp;
        QVERIFY(put(temp.filePath("src/a"), QByteArray(8192, 'm')));
        RobocopyProcessSupervisor supervisor;
        FileTransferListModel model;
        RobocopyProfile p;
        p.values["dryRunListOnly"] = true;
        quint64 baselineBytes = 0;
        DWORD baselineHandles = 0;
        for (int i = 0; i < 28; ++i) {
            {
                QSignalSpy done(&supervisor, &RobocopyProcessSupervisor::finished);
                QVERIFY(supervisor.start({{temp.filePath("src"), temp.filePath("dst"), {}, false}}, p,
                                         temp.filePath("repeat.log")));
                QVERIFY(awaitRun(done));
                QVERIFY(done[0][0].toBool());
                supervisor.stopAndWait();
                model.append(supervisor.takeEntries(20000));
                model.clear();
            }
            QCoreApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            if (i == 5) {
                baselineBytes = privateBytes();
                baselineHandles = handles();
            }
        }
        const auto afterHandles = handles();
        const auto afterBytes = privateBytes();
        qInfo() << "Repeated-transfer resources: handles" << baselineHandles << "->" << afterHandles
                << "private bytes" << baselineBytes << "->" << afterBytes;
        QVERIFY2(afterHandles <= baselineHandles + 2, "Windows handles grew across repeated transfers");
        QVERIFY2(afterBytes <= baselineBytes + 16 * 1024 * 1024,
                 "Private memory grew by more than 16 MiB after warm-up");
    }
    void shutdownCancelsPausedChild() {
        QTemporaryDir temp;
        QVERIFY(put(temp.filePath("src/a"), QByteArray(4000000, 's')));
        const auto before = handles();
        QElapsedTimer clock;
        clock.start();
        {
            RobocopyProfile p;
            p.values["enableBandwidthLimit"] = true;
            p.values["interPacketGapMs"] = 100;
            RobocopyProcessSupervisor supervisor;
            QSignalSpy telemetry(&supervisor, &RobocopyProcessSupervisor::telemetry);
            QVERIFY(supervisor.start({{temp.filePath("src"), temp.filePath("dest"), {}, false}}, p,
                                     temp.filePath("shutdown.log")));
            supervisor.setPaused(true);
            QVERIFY(telemetry.wait(5000));
            QVERIFY(telemetry.last()[0].toMap().value("paused").toBool());
        }
        QVERIFY2(clock.elapsed() < 3000, "Supervisor shutdown did not promptly join its paused child");
        QVERIFY(handles() <= before + 2);
    }
    void coordinatorAndDestructiveConfirmation() {
        QTemporaryDir temp;
        const auto oldEnv = qgetenv("STRATACOPY_DATA_DIR");
        qputenv("STRATACOPY_DATA_DIR", temp.filePath("state").toUtf8());
        const auto envGuard = qScopeGuard([&] {
            if (oldEnv.isNull())
                qunsetenv("STRATACOPY_DATA_DIR");
            else
                qputenv("STRATACOPY_DATA_DIR", oldEnv);
        });
        QVERIFY(put(temp.filePath("source/one.txt"), "verified payload"));
        MainCoordinatorViewModel coordinator;
        connect(&coordinator, &MainCoordinatorViewModel::fileConflictRequired, &coordinator,
                [&](const QVariantMap &conflict) {
                    coordinator.resolveFileConflict(conflict.value("token").toULongLong(), 0);
                });
        coordinator.addPaths({temp.filePath("source")});
        coordinator.setDestination(temp.filePath("target"));
        coordinator.setOption("verifyHashPostCopy", true);
        coordinator.setOption("hashAlgorithm", 1);
        QVERIFY(coordinator.validation().isEmpty());
        QSignalSpy complete(&coordinator, &MainCoordinatorViewModel::completedSuccessfully);
        QSignalSpy notification(&coordinator, &MainCoordinatorViewModel::transferSucceeded);
        QVERIFY(coordinator.start());
        QVERIFY(complete.wait(10000));
        QVERIFY(!coordinator.busy());
        QCOMPARE(notification.size(), 1);
        QVERIFY(notification.first().first().toLongLong() >= 0);
        QCOMPARE(coordinator.outcome(), "success");
        QCOMPARE(coordinator.progress(), 1.0);
        QCOMPARE(read(temp.filePath("target/source/one.txt")), QByteArray("verified payload"));
        QVERIFY(coordinator.fileStats()->counters().value("Verified").toInt() > 0);
        coordinator.savePreset("My settings");
        QCOMPARE(coordinator.savedPresets().size(), 1);
        QVERIFY(coordinator.sources().isEmpty());
        coordinator.addPaths({temp.filePath("source")});
        coordinator.exportScript(QUrl::fromLocalFile(temp.filePath("transfer.bat")));
        QVERIFY(read(temp.filePath("transfer.bat")).contains("chcp 65001"));
        coordinator.exportScript(QUrl::fromLocalFile(temp.filePath("transfer.ps1")));
        QVERIFY(read(temp.filePath("transfer.ps1")).contains("$LASTEXITCODE -ge 8"));
        QVERIFY(put(temp.filePath("target/source/extra.txt"), "temporary test extra"));
        coordinator.addPaths({temp.filePath("source")});
        coordinator.setOption("transferMode", 5);
        QSignalSpy confirmation(&coordinator, &MainCoordinatorViewModel::destructiveConfirmationRequired);
        QVERIFY(!coordinator.start());
        QCOMPARE(confirmation.size(), 1);
        QVERIFY(!coordinator.busy());
        QVERIFY(QFileInfo::exists(temp.filePath("target/source/extra.txt")));
        complete.clear();
        QVERIFY(coordinator.start(false, true));
        QVERIFY(complete.wait(10000));
        QVERIFY(!QFileInfo::exists(temp.filePath("target/source/extra.txt")));
        coordinator.setOption("verifyHashPostCopy", false);
        coordinator.addPaths({temp.filePath("source")});
        coordinator.setOption("transferMode", 3);
        QVERIFY(put(temp.filePath("source/move-only.txt"), "new move payload"));
        complete.clear();
        QVERIFY(coordinator.start());
        QVERIFY(complete.wait(10000));
        QCOMPARE(coordinator.profile().value("transferMode").toInt(), 0);
        QVERIFY(!QFileInfo::exists(temp.filePath("source/move-only.txt")));
        QCOMPARE(read(temp.filePath("target/source/move-only.txt")), QByteArray("new move payload"));
        QVERIFY(QFileInfo::exists(temp.filePath("target/source/one.txt")));
        QVERIFY(coordinator.recentRuns().size() >= 3);
    }
    void boundedGraphAndModel() {
        TelemetryHistoryModel graph;
        for (int i = 0; i < 10000; ++i)
            graph.append(i, i / 2.0);
        QCOMPARE(graph.points().size(), 600);
        FileTransferListModel model;
        model.append({{"C:/a", "New File", "Listed", {}, 100}});
        QCOMPARE(model.rowCount(), 1);
        model.settle(true, false);
        QCOMPARE(model.data(model.index(0), FileTransferListModel::StateRole).toString(), "Copied");
        model.clear();
        QCOMPARE(model.rowCount(), 0);
    }
    void explorerPasteRequests() {
        QTemporaryDir temp;
        const auto old = qgetenv("STRATACOPY_DATA_DIR");
        qputenv("STRATACOPY_DATA_DIR", temp.filePath("state").toUtf8());
        const auto guard = qScopeGuard([&] {
            if (old.isNull())
                qunsetenv("STRATACOPY_DATA_DIR");
            else
                qputenv("STRATACOPY_DATA_DIR", old);
        });
        QVERIFY(put(temp.filePath("source/a.txt"), "Explorer paste"));
        QDir().mkpath(temp.filePath("target"));
        const auto request = temp.filePath("state/ExplorerRequests/request.json");
        QJsonObject payload{{"sources", QJsonArray{temp.filePath("source/a.txt")}},
                            {"destination", temp.filePath("target")},
                            {"operation", "copy"}};
        QVERIFY(put(request, QJsonDocument(payload).toJson()));
        MainCoordinatorViewModel coordinator;
        coordinator.setOption("hddThreads", 2);
        coordinator.setOption("threadCount", 6);
        QSignalSpy complete(&coordinator, &MainCoordinatorViewModel::completedSuccessfully);
        QVERIFY(coordinator.importExplorerRequest(request));
        QVERIFY(complete.wait(10000));
        QCOMPARE(read(temp.filePath("target/a.txt")), QByteArray("Explorer paste"));
        QVERIFY(coordinator.destination().isEmpty());
        QCOMPARE(coordinator.profile().value("performanceMode").toInt(), 3);
        QCOMPARE(coordinator.profile().value("threadCount").toInt(), 6);
        QCOMPARE(coordinator.profile().value("hddThreads").toInt(), 2);
        QVERIFY(!QFileInfo::exists(request));
        const auto external = temp.filePath("outside.json");
        QVERIFY(put(external, QJsonDocument(payload).toJson()));
        QVERIFY(!coordinator.importExplorerRequest(external));
        QVERIFY(QFileInfo::exists(external));
        payload["operation"] = "mirror";
        QVERIFY(put(request, QJsonDocument(payload).toJson()));
        QVERIFY(!coordinator.importExplorerRequest(request));
        payload["operation"] = "move";
        QVERIFY(put(request, QJsonDocument(payload).toJson()));
        QSignalSpy confirmation(&coordinator, &MainCoordinatorViewModel::destructiveConfirmationRequired);
        QSignalSpy conflicts(&coordinator, &MainCoordinatorViewModel::fileConflictRequired);
        connect(&coordinator, &MainCoordinatorViewModel::fileConflictRequired, &coordinator,
                [&](const QVariantMap &conflict) { coordinator.resolveFileConflict(conflict.value("token").toULongLong(), 0, true); });
        complete.clear();
        QVERIFY(coordinator.importExplorerRequest(request));
        QCOMPARE(confirmation.size(), 0);
        // New sources dropped while busy are durably queued, without changing this run.
        QVERIFY(put(temp.filePath("extra/b.txt"), "queued drop"));
        coordinator.addPaths({temp.filePath("extra/b.txt")});
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(temp.filePath("target/b.txt")), 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!coordinator.busy(), 10000);
        QCOMPARE(read(temp.filePath("target/b.txt")), QByteArray("queued drop"));
        QVERIFY(!QFileInfo::exists(temp.filePath("extra/b.txt")));
        QCOMPARE(coordinator.profile().value("transferMode").toInt(), 0);
        QVERIFY(!QFileInfo::exists(request));
    }
    void conflictApplyAllIsPerTransfer() {
        QTemporaryDir temp;
        const auto old = qgetenv("STRATACOPY_DATA_DIR");
        qputenv("STRATACOPY_DATA_DIR", temp.filePath("state").toUtf8());
        const auto guard = qScopeGuard([&] { if(old.isNull()) qunsetenv("STRATACOPY_DATA_DIR"); else qputenv("STRATACOPY_DATA_DIR", old); });
        MainCoordinatorViewModel coordinator;
        for (const auto &name : {"a.txt", "b.txt", "c.txt"}) {
            QVERIFY(put(temp.filePath(QString("source/") + name), "incoming"));
            QVERIFY(put(temp.filePath(QString("target/source/") + name), "original"));
        }
        coordinator.addPaths({temp.filePath("source")});
        coordinator.setDestination(temp.filePath("target"));
        QSignalSpy conflicts(&coordinator, &MainCoordinatorViewModel::fileConflictRequired);
        QSignalSpy complete(&coordinator, &MainCoordinatorViewModel::completedSuccessfully);
        int choice = 1;
        connect(&coordinator, &MainCoordinatorViewModel::fileConflictRequired, &coordinator,
            [&](const QVariantMap &conflict) {coordinator.resolveFileConflict(conflict.value("token").toULongLong(), choice, true);});
        QVERIFY(coordinator.start());
        QVERIFY(complete.wait(10000));
        QCOMPARE(conflicts.size(), 1);
        QCOMPARE(read(temp.filePath("target/source/c.txt")), QByteArray("original"));
        choice = 0;
        QVERIFY(coordinator.sources().isEmpty());
        coordinator.addPaths({temp.filePath("source")});
        conflicts.clear();complete.clear();
        QVERIFY(coordinator.start());
        QVERIFY(complete.wait(10000));
        QCOMPARE(conflicts.size(), 1);
        QCOMPARE(read(temp.filePath("target/source/c.txt")), QByteArray("incoming"));
    }
    void fileConflictChoices() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto source = temp.filePath("source/日本語 report.txt");
        const auto destination = temp.filePath("target/日本語 report.txt");
        QVERIFY(put(source, "incoming payload"));
        QVERIFY(put(destination, "existing payload"));
        QVERIFY(put(temp.filePath("target/日本語 report (2).txt"), "previous duplicate"));
        RobocopyProfile profile;
        profile.values["verifyHashPostCopy"] = true;
        auto plan = MultiSourceBatcher::build({source}, temp.filePath("target"), profile);
        RobocopyProcessSupervisor supervisor;
        QSignalSpy finished(&supervisor, &RobocopyProcessSupervisor::finished);
        QSignalSpy conflict(&supervisor, &RobocopyProcessSupervisor::fileConflict);
        auto runChoice = [&](int choice, const QString &label) {
            finished.clear();
            conflict.clear();
            if (!supervisor.start(plan.jobs, profile, temp.filePath(label + ".log"), true))
                return false;
            if (!conflict.wait(10000))
                return false;
            const auto request = conflict.last();
            if (read(destination) != "existing payload")
                return false;
            supervisor.resolveConflict(request[0].toULongLong() + 100, 0); // stale tokens cannot answer.
            if (finished.size())
                return false;
            supervisor.resolveConflict(request[0].toULongLong(), choice);
            if (!finished.wait(10000))
                return false;
            return finished.last()[0].toBool();
        };
        QVERIFY(runChoice(1, "skip"));
        QCOMPARE(read(destination), QByteArray("existing payload"));
        QVERIFY(runChoice(2, "duplicate"));
        QCOMPARE(read(destination), QByteArray("existing payload"));
        QCOMPARE(read(temp.filePath("target/日本語 report (2).txt")), QByteArray("previous duplicate"));
        QCOMPARE(read(temp.filePath("target/日本語 report (3).txt")), QByteArray("incoming payload"));
        QVERIFY(runChoice(0, "replace"));
        QCOMPARE(read(destination), QByteArray("incoming payload"));
        supervisor.stopAndWait();
        QVERIFY(QDir(temp.path()).entryList({"*.files-*"}, QDir::Files).isEmpty());
        QVERIFY(QDir(temp.filePath("target"))
                    .entryList({".stratacopy-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot)
                    .isEmpty());
    }
    void nestedConflictsAndMoveDuplicate() {
        QTemporaryDir temp;
        QVERIFY(put(temp.filePath("source/tree/nested/a.txt"), "incoming A"));
        QVERIFY(put(temp.filePath("source/tree/nested/b.txt"), "incoming B"));
        QVERIFY(put(temp.filePath("source/tree/new.txt"), "new file"));
        QVERIFY(put(temp.filePath("target/tree/nested/a.txt"), "old A"));
        QVERIFY(put(temp.filePath("target/tree/nested/b.txt"), "old B"));
        RobocopyProfile profile;
        profile.values["verifyHashPostCopy"] = true;
        RobocopyProcessSupervisor supervisor;
        int prompts = 0;
        connect(&supervisor, &RobocopyProcessSupervisor::fileConflict, &supervisor,
                [&](quint64 token, const QString &source, const QString &) {
                    ++prompts;
                    supervisor.resolveConflict(token, source.endsWith("a.txt") ? 0 : 1);
                });
        QSignalSpy finished(&supervisor, &RobocopyProcessSupervisor::finished);
        auto plan =
            MultiSourceBatcher::build({temp.filePath("source/tree")}, temp.filePath("target"), profile);
        QVERIFY(supervisor.start(plan.jobs, profile, temp.filePath("nested.log"), true));
        QVERIFY(finished.wait(10000));
        QVERIFY(finished.last()[0].toBool());
        QCOMPARE(prompts, 2);
        QCOMPARE(read(temp.filePath("target/tree/nested/a.txt")), QByteArray("incoming A"));
        QCOMPARE(read(temp.filePath("target/tree/nested/b.txt")), QByteArray("old B"));
        QCOMPARE(read(temp.filePath("target/tree/new.txt")), QByteArray("new file"));
        supervisor.stopAndWait();
        QVERIFY(put(temp.filePath("move/a.txt"), "moved data"));
        QVERIFY(put(temp.filePath("move-target/a.txt"), "preserve me"));
        RobocopyProcessSupervisor mover;
        connect(&mover, &RobocopyProcessSupervisor::fileConflict, &mover,
                [&](quint64 token, const QString &, const QString &) { mover.resolveConflict(token, 2); });
        profile.values["verifyHashPostCopy"] = false;
        profile.values["transferMode"] = 3;
        plan =
            MultiSourceBatcher::build({temp.filePath("move/a.txt")}, temp.filePath("move-target"), profile);
        QSignalSpy moved(&mover, &RobocopyProcessSupervisor::finished);
        QVERIFY(mover.start(plan.jobs, profile, temp.filePath("move.log"), true));
        QVERIFY(moved.wait(10000));
        QVERIFY(moved.last()[0].toBool());
        QVERIFY(!QFileInfo::exists(temp.filePath("move/a.txt")));
        QCOMPARE(read(temp.filePath("move-target/a.txt")), QByteArray("preserve me"));
        QCOMPARE(read(temp.filePath("move-target/a (2).txt")), QByteArray("moved data"));
    }
    void conflictCancellationAndShutdown() {
        QTemporaryDir temp;
        const auto source = temp.filePath("source/a.txt"), dest = temp.filePath("target/a.txt");
        QVERIFY(put(source, "source"));
        QVERIFY(put(dest, "destination"));
        RobocopyProfile profile;
        auto plan = MultiSourceBatcher::build({source}, temp.filePath("target"), profile);
        DWORD baseline = 0;
        for (int i = 0; i < 12; ++i) {
            RobocopyProcessSupervisor supervisor;
            QSignalSpy conflicts(&supervisor, &RobocopyProcessSupervisor::fileConflict);
            QVERIFY(
                supervisor.start(plan.jobs, profile, temp.filePath(QString("cancel-%1.log").arg(i)), true));
            QVERIFY(conflicts.wait(10000));
            QElapsedTimer timer;
            timer.start();
            supervisor.stopAndWait(); // Must unblock a pending UI choice without an event loop.
            QVERIFY(timer.elapsed() < 2000);
            QCOMPARE(read(source), QByteArray("source"));
            QCOMPARE(read(dest), QByteArray("destination"));
            if (i == 0)
                baseline = handles();
        }
        QVERIFY(handles() <= baseline + 4);
        QVERIFY(QDir(temp.path()).entryList({"*.files-*"}, QDir::Files).isEmpty());
    }
    void updateVersionComparison() {
        auto payload = [](const QString &tag) {
            return QJsonDocument(QJsonObject{{"tag_name", tag}, {"draft", false}, {"prerelease", false}})
                .toJson();
        };
        QString message;
        QCOMPARE(UpdateChecker::evaluateRelease(payload("v1.0.0"), "1.0.0", message),
                 UpdateChecker::UpToDate);
        QVERIFY(message.contains("already updated"));
        QCOMPARE(UpdateChecker::evaluateRelease(payload("v1.0.1"), "1.0.0", message),
                 UpdateChecker::UpdateAvailable);
        QCOMPARE(UpdateChecker::evaluateRelease(payload("v1.10.0"), "1.9.9", message),
                 UpdateChecker::UpdateAvailable);
        QCOMPARE(UpdateChecker::evaluateRelease(payload("v1.0.0"), "1.0.1", message),
                 UpdateChecker::UpToDate);
        QCOMPARE(UpdateChecker::evaluateRelease(payload("v1.0.1-beta"), "1.0.0", message),
                 UpdateChecker::Error);
        QCOMPARE(UpdateChecker::evaluateRelease(payload("junk"), "1.0.0", message), UpdateChecker::Error);
        QCOMPARE(UpdateChecker::evaluateRelease("not json", "1.0.0", message), UpdateChecker::Error);
    }
    void updateNetworkAndCleanup() {
        const QList<QPair<int, QByteArray>> responses = {
            {200, "{\"tag_name\":\"v1.0.0\",\"draft\":false,\"prerelease\":false}"},
            {200, "{\"tag_name\":\"v1.0.1\",\"draft\":false,\"prerelease\":false}"},
            {200, "malformed"},
            {403, "rate limited"},
            {200, QByteArray(300000, 'x')}};
        for (int index = 0; index < responses.size(); ++index) {
            QTcpServer server;
            QVERIFY(server.listen(QHostAddress::LocalHost));
            const auto response = responses[index];
            connect(&server, &QTcpServer::newConnection, &server, [&] {
                auto *socket = server.nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, socket, [socket, response] {
                    socket->readAll();
                    if (socket->property("responded").toBool())
                        return;
                    socket->setProperty("responded", true);
                    socket->write(
                        "HTTP/1.1 " + QByteArray::number(response.first) +
                        " Response\r\nContent-Length: " + QByteArray::number(response.second.size()) +
                        "\r\nConnection: close\r\n\r\n" + response.second);
                    socket->disconnectFromHost();
                });
            });
            UpdateChecker checker("1.0.0");
            QSignalSpy finished(&checker, &UpdateChecker::finished);
            const auto url = QUrl(QString("http://127.0.0.1:%1/latest").arg(server.serverPort()));
            checker.check(url);
            checker.check(url); // A second click cannot create another request.
            QVERIFY(checker.checking());
            QVERIFY(finished.wait(5000));
            QCOMPARE(finished.size(), 1);
            QVERIFY(!checker.checking());
            QCOMPARE(finished.last()[0].value<UpdateChecker::Result>(), index == 0 ? UpdateChecker::UpToDate
                                                                        : index == 1
                                                                            ? UpdateChecker::UpdateAvailable
                                                                            : UpdateChecker::Error);
        }
        QTcpServer silent;
        QVERIFY(silent.listen(QHostAddress::LocalHost));
        const auto url = QUrl(QString("http://127.0.0.1:%1/latest").arg(silent.serverPort()));
        UpdateChecker timeout("1.0.0", nullptr, 100);
        QSignalSpy finished(&timeout, &UpdateChecker::finished);
        timeout.check(url);
        QVERIFY(finished.wait(3000));
        QCOMPARE(finished.last()[0].value<UpdateChecker::Result>(), UpdateChecker::Error);
        QVERIFY(finished.last()[1].toString().contains("timed out"));
        {
            UpdateChecker closing("1.0.0");
            closing.check(url);
        } // Destruction cancels and owns its pending reply.
    }
    void exitCodes() {
        QVERIFY(RobocopyProcessSupervisor::decodeExitCode(0).contains("synchronized"));
        QVERIFY(RobocopyProcessSupervisor::decodeExitCode(3).contains("Extra"));
        QVERIFY(RobocopyProcessSupervisor::decodeExitCode(8).contains("errors"));
        QVERIFY(RobocopyProcessSupervisor::decodeExitCode(16).contains("Fatal"));
    }
};
QTEST_GUILESS_MAIN(EngineTests)
#include "EngineTests.moc"
