#include "RobocopyProcessSupervisor.hpp"
#include "FastPreScanner.hpp"
#include "KernelTelemetryPoller.hpp"
#include "Platform/PrivilegeEscalator.hpp"
#include "Platform/UniqueHandle.hpp"
#include "Verification/PostCopyHashVerifier.hpp"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
#include <memory>
#include <vector>
#include <bit>
namespace strata {
namespace {
QString winError(const QString &action) {
    return action + QString(" (Windows error %1)").arg(GetLastError());
}
using NtProcessFn = LONG(NTAPI *)(HANDLE);
size_t entryCost(const FileEntry &e) {
    return sizeof(FileEntry) +
           size_t(e.path.size() + e.classification.size() + e.state.size() + e.hash.size()) * sizeof(QChar);
}
} // namespace
RobocopyProcessSupervisor::~RobocopyProcessSupervisor() {
    stopAndWait();
}
void RobocopyProcessSupervisor::stopAndWait() {
    cancel();
    if (m_worker.joinable())
        m_worker.join();
}
bool RobocopyProcessSupervisor::start(QList<RobocopyInvocation> jobs, RobocopyProfile profile,
                                      const QString &logPath, bool promptConflicts) {
    if (m_running.exchange(true))
        return false;
    if (m_worker.joinable())
        m_worker.join();
    m_cancel = false;
    m_pause = false;
    m_omitted = 0;
    m_telemetryPending = false;
    {
        std::lock_guard guard(m_queueMutex);
        m_entries.clear();
        m_queuedBytes = 0;
    }
    try {
        m_worker = std::thread(
            [this, jobs = std::move(jobs), profile = std::move(profile), logPath, promptConflicts] {
                try {
                    run(jobs, profile, logPath, promptConflicts);
                } catch (const std::exception &e) {
                    m_running = false;
                    emit finished(false, m_cancel.load(), QString::fromUtf8(e.what()));
                } catch (...) {
                    m_running = false;
                    emit finished(false, m_cancel.load(), "Unexpected transfer failure.");
                }
            });
    } catch (const std::system_error &) {
        m_running = false;
        return false;
    }
    return true;
}
void RobocopyProcessSupervisor::cancel() {
    {
        std::lock_guard guard(m_conflictMutex);
        m_cancel = true;
    }
    m_pause = false;
    m_conflictReady.notify_all();
}
void RobocopyProcessSupervisor::resolveConflict(quint64 token, int choice) {
    std::lock_guard guard(m_conflictMutex);
    if (token != m_waitingConflict || !token || choice < 0 || choice > 3 || m_conflictAnswer != -1)
        return;
    m_conflictAnswer = choice;
    m_conflictReady.notify_all();
}
int RobocopyProcessSupervisor::askConflict(const QString &source, const QString &destination) {
    quint64 token;
    {
        std::lock_guard guard(m_conflictMutex);
        token = m_waitingConflict = ++m_nextConflict;
        m_conflictAnswer = -1;
    }
    emit phaseChanged("Waiting for a file conflict decision");
    emit fileConflict(token, source, destination);
    std::unique_lock lock(m_conflictMutex);
    m_conflictReady.wait(lock, [this] { return m_cancel.load() || m_conflictAnswer != -1; });
    m_waitingConflict = 0;
    return m_cancel.load() ? 3 : m_conflictAnswer;
}
void RobocopyProcessSupervisor::setPaused(bool paused) {
    m_pause = paused;
}
void RobocopyProcessSupervisor::enqueue(FileEntry entry) {
    std::lock_guard guard(m_queueMutex);
    constexpr size_t capacity = 16384;
    const auto bytes = entryCost(entry);
    if (m_entries.size() >= capacity || m_queuedBytes + bytes > 8 * 1024 * 1024) {
        ++m_omitted;
        return;
    }
    m_queuedBytes += bytes;
    m_entries.push_back(std::move(entry));
}
QList<FileEntry> RobocopyProcessSupervisor::takeEntries(int maximum) {
    QList<FileEntry> out;
    std::lock_guard guard(m_queueMutex);
    const auto count = qMin(size_t(maximum), m_entries.size());
    out.reserve(qsizetype(count));
    for (size_t i = 0; i < count; ++i) {
        m_queuedBytes -= entryCost(m_entries.front());
        out.append(std::move(m_entries.front()));
        m_entries.pop_front();
    }
    return out;
}
QString RobocopyProcessSupervisor::decodeExitCode(quint32 code) {
    if (code == 0)
        return "Already synchronized; no files copied.";
    QStringList parts;
    if (code & 1)
        parts << "Files copied";
    if (code & 2)
        parts << "Extra destination items detected";
    if (code & 4)
        parts << "Mismatches detected";
    if (code & 8)
        parts << "Copy errors; retry limit exceeded";
    if (code & 16)
        parts << "Fatal Robocopy error";
    if (code & ~31u)
        parts << "Unexpected exit code";
    return parts.join(" · ");
}
void RobocopyProcessSupervisor::run(const QList<RobocopyInvocation> &jobs, const RobocopyProfile &profile,
                                    const QString &logPath, bool promptConflicts) {
    const bool interactive = promptConflicts && !profile.flag("dryRunListOnly");
    auto complete = [&](bool ok, QString message) {
        m_running = false;
        emit finished(ok && !m_cancel.load(), m_cancel.load(), std::move(message));
    };
    QFile log(logPath);
    if (!log.open(QIODevice::WriteOnly | QIODevice::Append)) {
        complete(false, "Cannot open the transfer log: " + log.errorString());
        return;
    }
    auto writeLog = [&](const QString &line) {
        const auto data = (line + '\n').toUtf8();
        return log.write(data) == data.size();
    };
    emit phaseChanged("Scanning sources");
    quint64 total = 0, count = 0;
    QStringList warnings;
    std::vector<std::unique_ptr<QTemporaryFile>> manifests;
    for (const auto &job : jobs) {
        auto scanProfile = profile;
        std::unique_ptr<QTemporaryFile> manifest;
        bool manifestFailed = false;
        if (interactive) {
            scanProfile.values["collisionPolicy"] = 0;
            manifest = std::make_unique<QTemporaryFile>(logPath + ".files-XXXXXX");
            if (!manifest->open()) {
                complete(false, "Cannot create the file-conflict manifest.");
                return;
            }
        }
        const auto s = FastPreScanner::scan(
            job, scanProfile, m_cancel, [&](const QString &source, const QString &destination, quint64 size) {
                if (!manifest)
                    return true;
                const auto line = QJsonDocument(QJsonObject{{"source", source},
                                                            {"destination", destination},
                                                            {"size", QString::number(size)},
                                                            {"conflict", QFileInfo::exists(destination)}})
                                      .toJson(QJsonDocument::Compact) +
                                  '\n';
                manifestFailed = manifest->write(line) != line.size();
                return !manifestFailed;
            });
        if (manifestFailed || (manifest && (!manifest->flush() || !manifest->seek(0)))) {
            complete(false, "Cannot save the file-conflict manifest.");
            return;
        }
        manifests.push_back(std::move(manifest));
        if (interactive && !s.warnings.isEmpty()) {
            complete(false, "Cannot inspect every source for file conflicts: " + s.warnings.join("; "));
            return;
        }
        total += s.bytes;
        count += s.files;
        warnings << s.warnings;
        if (m_cancel.load()) {
            complete(false, "Cancelled during scan.");
            return;
        }
    }
    emit scanned(total, count, warnings);
    PrivilegeEscalator privileges;
    if (profile.number("accessMode") >= 2) {
        const auto error = privileges.enableBackupRestore();
        if (!error.isEmpty()) {
            complete(false, error);
            return;
        }
    }
    quint64 cumulativeWritten = 0, cumulativeRead = 0;
    quint32 exitBits = 0;
    QElapsedTimer elapsed;
    elapsed.start();
    qint64 pausedTime = 0;
    quint64 processSequence = 0;
    quint64 interactiveVerified = 0, interactiveFailed = 0;
    auto verifyFile = [&](const QString &source, const QString &destination, quint64 size) {
        if (!profile.flag("verifyHashPostCopy") || m_cancel.load())
            return;
        emit phaseChanged("Verifying file hashes");
        const bool ok =
            PostCopyHashVerifier::verifyFile(source, destination, size, profile, m_cancel, [&](FileEntry e) {
                writeLog(e.state + " " + e.path + " " + e.hash + " " + e.classification);
                enqueue(std::move(e));
            });
        if (!m_cancel.load())
            ok ? ++interactiveVerified : ++interactiveFailed;
    };
    const auto ntdll = GetModuleHandleW(L"ntdll.dll");
    auto suspend = std::bit_cast<NtProcessFn>(GetProcAddress(ntdll, "NtSuspendProcess"));
    auto resume = std::bit_cast<NtProcessFn>(GetProcAddress(ntdll, "NtResumeProcess"));
    for (qsizetype index = 0; index < jobs.size(); ++index) {
        bool bulk = true;
        auto *manifest = manifests[size_t(index)].get();
        while (bulk || (manifest && !manifest->atEnd())) {
            auto currentJob = jobs[index];
            auto currentProfile = profile;
            QString conflictSource, conflictDestination;
            quint64 conflictSize = 0;
            bool duplicate = false;
            if (bulk) {
                bulk = false;
                if (interactive) {
                    // No existing file may be overwritten by a bulk process before a decision.
                    currentProfile.values["collisionPolicy"] = 2;
                    currentProfile.values["fixSecurityOnExisting"] = false;
                    currentProfile.values["fixTimestampsOnExisting"] = false;
                }
            } else {
                const auto line = manifest->readLine(1024 * 1024);
                const auto document = QJsonDocument::fromJson(line);
                if (!document.isObject()) {
                    complete(false, "Cannot read the file-conflict manifest.");
                    return;
                }
                const auto record = document.object();
                conflictSource = record.value("source").toString();
                conflictDestination = record.value("destination").toString();
                conflictSize = record.value("size").toString().toULongLong();
                if (!record.value("conflict").toBool()) {
                    verifyFile(conflictSource, conflictDestination, conflictSize);
                    if (m_cancel.load())
                        break;
                    continue;
                }
                if (QFileInfo(conflictDestination).isDir()) {
                    complete(false, "A folder occupies the destination file path: " + conflictDestination);
                    return;
                }
                int choice = QFileInfo::exists(conflictDestination)
                                 ? askConflict(conflictSource, conflictDestination)
                                 : 0;
                if (choice == 3) {
                    m_cancel = true;
                    break;
                }
                if (choice == 1) {
                    writeLog("Skipped by user: " + conflictSource);
                    enqueue({conflictSource, "Skipped by user", "Skipped", {}, conflictSize});
                    continue;
                }
                duplicate = choice == 2;
                currentJob = {QFileInfo(conflictSource).absolutePath(),
                              QFileInfo(conflictDestination).absolutePath(),
                              {QFileInfo(conflictSource).fileName()},
                              true};
                currentProfile.values["collisionPolicy"] = 3;
                if (duplicate)
                    currentProfile.values["excludeLonelyFiles"] = false;
            }
            if (m_cancel.load())
                break;
            std::unique_ptr<QTemporaryDir> staging;
            const auto cleanupEmptyStage = qScopeGuard([&] {
                if (staging &&
                    QDir(staging->path())
                        .entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot)
                        .isEmpty())
                    staging->setAutoRemove(true);
            });
            if (duplicate) {
                if (!QDir().mkpath(currentJob.destination)) {
                    complete(false, "Cannot create the duplicate destination folder.");
                    return;
                }
                staging = std::make_unique<QTemporaryDir>(
                    QDir(currentJob.destination).filePath(".stratacopy-XXXXXX"));
                if (!staging->isValid()) {
                    complete(false, "Cannot create the duplicate staging folder.");
                    return;
                }
                // On failure preserve staged payload, especially if Robocopy already moved its source.
                staging->setAutoRemove(false);
                currentJob.destination = staging->path();
                writeLog("Duplicate staging (retained on failure): " + staging->path());
            }
            emit phaseChanged(currentProfile.flag("dryRunListOnly")
                                  ? "Dry-run preview"
                                  : QString("Transferring · job %1 of %2").arg(index + 1).arg(jobs.size()));
            SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
            HANDLE readRaw = nullptr, writeRaw = nullptr;
            if (!CreatePipe(&readRaw, &writeRaw, &security, 1024 * 1024)) {
                complete(false, winError("Cannot create output pipe"));
                return;
            }
            UniqueHandle readPipe(readRaw), writePipe(writeRaw);
            if (!SetHandleInformation(readPipe.get(), HANDLE_FLAG_INHERIT, 0)) {
                complete(false, winError("Cannot protect pipe handle"));
                return;
            }
            UniqueHandle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                           &security, OPEN_EXISTING, 0, nullptr));
            if (!input) {
                complete(false, winError("Cannot open standard input"));
                return;
            }
            UniqueHandle jobObject(CreateJobObjectW(nullptr, nullptr));
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if (!jobObject || !SetInformationJobObject(jobObject.get(), JobObjectExtendedLimitInformation,
                                                       &limits, sizeof(limits))) {
                complete(false, winError("Cannot configure process job"));
                return;
            }
            SIZE_T attributeBytes = 0;
            InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
            std::vector<std::byte> attributeStorage(attributeBytes);
            auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
            if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attributeBytes)) {
                complete(false, winError("Cannot initialize process attributes"));
                return;
            }
            struct AttributeGuard {
                LPPROC_THREAD_ATTRIBUTE_LIST value;
                ~AttributeGuard() {
                    DeleteProcThreadAttributeList(value);
                }
            } attributeGuard{attributes};
            HANDLE inherited[]{writePipe.get(), input.get()};
            if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited,
                                           sizeof(inherited), nullptr, nullptr)) {
                complete(false, winError("Cannot restrict handle inheritance"));
                return;
            }
            STARTUPINFOEXW startup{};
            startup.StartupInfo.cb = sizeof(startup);
            startup.lpAttributeList = attributes;
            startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
            startup.StartupInfo.hStdOutput = writePipe.get();
            startup.StartupInfo.hStdError = writePipe.get();
            startup.StartupInfo.hStdInput = input.get();
            PROCESS_INFORMATION processInfo{};
            const auto executable = QDir::toNativeSeparators(RobocopyCommandCompiler::executable());
            const auto unicodePath = logPath + QString(".job%1.utf16.log").arg(++processSequence);
            auto command =
                RobocopyCommandCompiler::display(currentJob, currentProfile, unicodePath).toStdWString();
            writeLog(QString::fromStdWString(command));
            if (!CreateProcessW(reinterpret_cast<LPCWSTR>(executable.utf16()), command.data(), nullptr,
                                nullptr, TRUE,
                                CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT |
                                    EXTENDED_STARTUPINFO_PRESENT,
                                nullptr, nullptr, &startup.StartupInfo, &processInfo)) {
                complete(false, winError("Cannot start Robocopy"));
                return;
            }
            UniqueHandle process(processInfo.hProcess), thread(processInfo.hThread);
            // A suspended process is terminated explicitly if assignment fails, before any copy begins.
            if (!AssignProcessToJobObject(jobObject.get(), process.get())) {
                const auto error = winError("Cannot attach Robocopy to lifetime job");
                TerminateProcess(process.get(), 16);
                WaitForSingleObject(process.get(), INFINITE);
                complete(false, error);
                return;
            }
            writePipe.reset();
            input.reset();
            KernelTelemetryPoller poller;
            poller.reset(process.get());
            if (ResumeThread(thread.get()) == DWORD(-1)) {
                complete(false, winError("Cannot resume Robocopy"));
                return;
            }
            thread.reset();
            bool suspended = false, logFailed = false;
            QString pauseError;
            QElapsedTimer pauseClock, reportClock;
            reportClock.start();
            quint64 lastWritten = 0, lastRead = 0;
            StdoutPipeParser parser([&](const QString &line, const FileEntry *entry) {
                if (!writeLog(line))
                    logFailed = true;
                if (entry) {
                    auto e = *entry;
                    const auto classification = e.classification.toLower();
                    if (e.state != "Failed" && e.state != "Extra") {
                        if (currentProfile.number("collisionPolicy") == 3 &&
                            (classification == "same" || classification == "tweaked"))
                            e.state = "Listed";
                        if ((currentProfile.number("collisionPolicy") == 1 && classification == "older") ||
                            (currentProfile.number("collisionPolicy") == 2 &&
                             (classification == "older" || classification == "newer" ||
                              classification == "changed")))
                            e.state = "Skipped";
                    }
                    if (currentProfile.flag("dryRunListOnly"))
                        e.state = e.state == "Failed" ? "Failed" : "Planned";
                    enqueue(std::move(e));
                }
            });
            QFile unicodeLog(unicodePath);
            auto tailLog = [&] {
                if (!unicodeLog.isOpen() && !unicodeLog.open(QIODevice::ReadOnly))
                    return;
                int rounds = 0;
                while (rounds++ < 32) {
                    auto bytes = unicodeLog.read(65536);
                    if (bytes.isEmpty()) {
                        if (unicodeLog.error() != QFileDevice::NoError)
                            logFailed = true;
                        break;
                    }
                    parser.feed(bytes);
                }
            };
            auto drain = [&] {
                DWORD available = 0;
                char buffer[65536];
                int rounds = 0;
                while (rounds++ < 32 &&
                       PeekNamedPipe(readPipe.get(), nullptr, 0, nullptr, &available, nullptr) &&
                       available > 0) {
                    DWORD received = 0;
                    if (!ReadFile(readPipe.get(), buffer, qMin(DWORD(sizeof(buffer)), available), &received,
                                  nullptr) ||
                        received == 0)
                        break;
                }
                tailLog();
            };
            while (true) {
                drain();
                if (m_cancel.load() || logFailed || !pauseError.isEmpty()) {
                    TerminateJobObject(jobObject.get(), ERROR_CANCELLED);
                    WaitForSingleObject(process.get(), INFINITE);
                    break;
                }
                if (m_pause.load() != suspended) {
                    const auto fn = m_pause.load() ? suspend : resume;
                    if (!fn || fn(process.get()) < 0) {
                        m_pause = false;
                        pauseError = "Windows could not pause or resume Robocopy. Transfer stopped to avoid "
                                     "leaving a suspended process.";
                    } else {
                        suspended = m_pause.load();
                        if (suspended)
                            pauseClock.start();
                        else
                            pausedTime += pauseClock.elapsed();
                    }
                }
                const auto sample = poller.sample();
                lastWritten = sample.writeBytes;
                lastRead = sample.readBytes;
                if (reportClock.elapsed() >= 100) {
                    if (!m_telemetryPending.exchange(true))
                        emit telemetry({{"readMbps", suspended ? 0 : sample.readMbps},
                                        {"writeMbps", suspended ? 0 : sample.writeMbps},
                                        {"iops", suspended ? 0 : sample.iops},
                                        {"writeBytes", QVariant::fromValue(cumulativeWritten + lastWritten)},
                                        {"readBytes", QVariant::fromValue(cumulativeRead + lastRead)},
                                        {"elapsed", (elapsed.elapsed() - pausedTime -
                                                     (suspended ? pauseClock.elapsed() : 0)) /
                                                        1000.0},
                                        {"jobIndex", index + 1},
                                        {"jobCount", jobs.size()},
                                        {"paused", suspended}});
                    reportClock.restart();
                    log.flush();
                }
                if (WaitForSingleObject(process.get(), 16) == WAIT_OBJECT_0)
                    break;
            }
            // The process has exited and cannot refill the pipe. Drain all remaining buffered bytes.
            DWORD remaining = 0;
            do {
                drain();
            } while (PeekNamedPipe(readPipe.get(), nullptr, 0, nullptr, &remaining, nullptr) && remaining);
            tailLog();
            while (!logFailed && unicodeLog.isOpen() && unicodeLog.pos() < unicodeLog.size())
                tailLog();
            parser.finish();
            log.flush();
            const auto final = poller.sample();
            cumulativeWritten += final.writeBytes;
            cumulativeRead += final.readBytes;
            DWORD code = 16;
            GetExitCodeProcess(process.get(), &code);
            exitBits |= code;
            if (code < 8 && !unicodeLog.isOpen()) {
                complete(false,
                         "Robocopy exited successfully, but its Unicode activity log could not be opened. "
                         "Check the destination and logs.");
                return;
            }
            writeLog(
                QString("Robocopy job %1: exit %2 — %3").arg(index + 1).arg(code).arg(decodeExitCode(code)));
            if (logFailed) {
                complete(false, "Transfer stopped because its log could not be written.");
                return;
            }
            if (!pauseError.isEmpty()) {
                complete(false, pauseError);
                return;
            }
            if (m_cancel.load())
                break;
            if (code >= 8) {
                complete(false, QString("Robocopy exit %1: %2").arg(code).arg(decodeExitCode(code)));
                return;
            }
            if (staging) {
                const auto staged = QDir(staging->path()).filePath(QFileInfo(conflictSource).fileName());
                const auto name = QFileInfo(conflictDestination).fileName();
                const auto dot = name.lastIndexOf('.');
                const auto stem = dot > 0 ? name.left(dot) : name;
                const auto extension = dot > 0 ? name.mid(dot) : QString();
                bool published = false;
                for (quint64 number = 2; number < 1000000 && !m_cancel.load(); ++number) {
                    const auto candidate = QDir(QFileInfo(conflictDestination).absolutePath())
                                               .filePath(stem + QString(" (%1)").arg(number) + extension);
                    const auto from = QDir::toNativeSeparators(staged),
                               to = QDir::toNativeSeparators(candidate);
                    if (MoveFileExW(reinterpret_cast<LPCWSTR>(from.utf16()),
                                    reinterpret_cast<LPCWSTR>(to.utf16()), MOVEFILE_WRITE_THROUGH)) {
                        conflictDestination = candidate;
                        staging->setAutoRemove(true);
                        published = true;
                        writeLog("Created duplicate: " + candidate);
                        enqueue({candidate, "Duplicate created", "Copied", {}, conflictSize});
                        break;
                    }
                    if (GetLastError() != ERROR_ALREADY_EXISTS && GetLastError() != ERROR_FILE_EXISTS)
                        break;
                }
                if (!published) {
                    complete(false, "Cannot publish the duplicate. Recover its staged data from: " +
                                        staging->path());
                    return;
                }
            }
            if (!conflictSource.isEmpty())
                verifyFile(conflictSource, conflictDestination, conflictSize);
        }
        if (m_cancel.load())
            break;
    }
    if (m_cancel.load()) {
        complete(false, "Transfer cancelled. Completed copies remain at the destination.");
        return;
    }
    if (interactive && profile.flag("verifyHashPostCopy")) {
        complete(!interactiveFailed, QString("Completed · %1 files verified · %2 verification failures")
                                         .arg(interactiveVerified)
                                         .arg(interactiveFailed));
        return;
    }
    if (profile.flag("verifyHashPostCopy") && !profile.flag("dryRunListOnly")) {
        emit phaseChanged("Verifying file hashes");
        quint64 failed = 0, verified = 0;
        for (const auto &job : jobs) {
            auto result = PostCopyHashVerifier::verify(job, profile, m_cancel, [&](FileEntry e) {
                writeLog(e.state + " " + e.path + " " + e.hash + " " + e.classification);
                enqueue(std::move(e));
            });
            failed += result.failed;
            verified += result.verified;
            if (m_cancel.load())
                break;
        }
        if (m_cancel.load()) {
            complete(false, "Verification cancelled.");
            return;
        }
        if (failed) {
            complete(false, QString("Verification found %1 failures; %2 files verified. See the log.")
                                .arg(failed)
                                .arg(verified));
            return;
        }
        complete(true,
                 QString("Completed · %1 files verified · %2").arg(verified).arg(decodeExitCode(exitBits)));
        return;
    }
    complete(true,
             (profile.flag("dryRunListOnly") ? "Dry run complete. No files were changed. " : "Completed. ") +
                 decodeExitCode(exitBits));
}
} // namespace strata
