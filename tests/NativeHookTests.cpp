#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QLibraryInfo>
#include <QScopeGuard>
#include "Platform/NativeIntegrationConfig.hpp"
#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <psapi.h>
#include <mutex>
#include <thread>
#include <atomic>
namespace {
QJsonObject received;
bool accepting = true;
std::mutex receiverMutex;
BOOL WINAPI receive(const wchar_t *json) {
    std::lock_guard lock(receiverMutex);
    received = QJsonDocument::fromJson(QString::fromWCharArray(json).toUtf8()).object();
    return accepting;
}
template <class T> struct Com {
    T *p{};
    ~Com() {
        if (p)
            p->Release();
    }
    T *operator->() const {
        return p;
    }
};
bool write(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write("content") == 7;
}
DWORD handles() {
    DWORD count{};
    GetProcessHandleCount(GetCurrentProcess(), &count);
    return count;
}
SIZE_T memory() {
    PROCESS_MEMORY_COUNTERS_EX data{};
    data.cb = sizeof(data);
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&data),
                         sizeof(data));
    return data.PrivateUsage;
}
bool item(const QString &path, IShellItem **out) {
    return SUCCEEDED(SHCreateItemFromParsingName(
        reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()), nullptr, IID_PPV_ARGS(out)));
}
class PersistOnly final : public IPersistIDList {
    ULONG references = 1;
    IPersistIDList *inner;

  public:
    explicit PersistOnly(IPersistIDList *value) : inner(value) {
        inner->AddRef();
    }
    ~PersistOnly() {
        inner->Release();
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) override {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (id == IID_IUnknown || id == IID_IPersist || id == IID_IPersistIDList) {
            *out = static_cast<IPersistIDList *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++references;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        auto n = --references;
        if (!n)
            delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE GetClassID(CLSID *id) override {
        return inner->GetClassID(id);
    }
    HRESULT STDMETHODCALLTYPE SetIDList(PCIDLIST_ABSOLUTE id) override {
        return inner->SetIDList(id);
    }
    HRESULT STDMETHODCALLTYPE GetIDList(PIDLIST_ABSOLUTE *id) override {
        return inner->GetIDList(id);
    }
};
} // namespace
class NativeHookTests : public QObject {
    Q_OBJECT
    HMODULE library{};
    size_t(WINAPI *pendingObjects)() {};
    QTemporaryDir root;
    QString source, destination;
  private slots:
    void initTestCase() {
        QVERIFY(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
        QVERIFY(root.isValid());
        source = root.path() + "/Unicode 日本語 quoted name.txt";
        destination = root.path() + "/destination";
        QVERIFY(write(source));
        QVERIFY(QDir().mkpath(destination));
        library = LoadLibraryW(L"StrataCopyNative.dll");
        QVERIFY(library);
        auto initialize = reinterpret_cast<BOOL(WINAPI *)(BOOL(WINAPI *)(const wchar_t *))>(
            GetProcAddress(library, "StrataCopyTestInitialize"));
        pendingObjects =
            reinterpret_cast<size_t(WINAPI *)()>(GetProcAddress(library, "StrataCopyPendingObjects"));
        QVERIFY(initialize);
        QVERIFY(pendingObjects);
        QVERIFY(initialize(receive));
    }
    void copyAndReuseObject() {
        Com<IFileOperation> operation;
        QVERIFY(SUCCEEDED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_INPROC_SERVER,
                                           IID_PPV_ARGS(&operation.p))));
        Com<IShellItem> from, to;
        QVERIFY(item(source, &from.p));
        QVERIFY(item(destination, &to.p));
        const auto originalName = QFileInfo(source).fileName().toStdWString();
        for (int i = 0; i < 3; ++i) {
            received = {};
            QVERIFY(SUCCEEDED(
                operation->CopyItem(from.p, to.p, i == 2 ? originalName.c_str() : nullptr, nullptr)));
            QVERIFY(SUCCEEDED(operation->PerformOperations()));
            QCOMPARE(received.value("operation").toString(), "copy");
            QCOMPARE(QDir::fromNativeSeparators(received.value("sources").toArray().first().toString()),
                     source);
            QCOMPARE(QDir::fromNativeSeparators(received.value("destination").toString()), destination);
            QVERIFY(received.value("native").toBool());
            QVERIFY(!QFileInfo::exists(destination + "/" + QFileInfo(source).fileName()));
            QCOMPARE(pendingObjects(), size_t(0));
        }
    }
    void moveArrayAndDataObject() {
        Com<IFileOperation> operation;
        QVERIFY(SUCCEEDED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_INPROC_SERVER,
                                           IID_PPV_ARGS(&operation.p))));
        Com<IShellItem> from, to;
        QVERIFY(item(source, &from.p));
        QVERIFY(item(destination, &to.p));
        Com<IShellItemArray> items;
        QVERIFY(SUCCEEDED(SHCreateShellItemArrayFromShellItem(from.p, IID_PPV_ARGS(&items.p))));
        received = {};
        QVERIFY(SUCCEEDED(operation->MoveItems(items.p, to.p)));
        QVERIFY(SUCCEEDED(operation->PerformOperations()));
        QCOMPARE(received.value("operation").toString(), "move");
        QVERIFY(QFileInfo::exists(source));
        QCOMPARE(pendingObjects(), size_t(0));
        Com<IDataObject> data;
        QVERIFY(SUCCEEDED(items->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(&data.p))));
        received = {};
        QVERIFY(SUCCEEDED(operation->CopyItems(data.p, to.p)));
        QVERIFY(SUCCEEDED(operation->PerformOperations()));
        QCOMPARE(received.value("operation").toString(), "copy");
    }
    void explorerEnumeratorAndPersistCollections() {
        Com<IFileOperation> operation;
        QVERIFY(SUCCEEDED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_INPROC_SERVER,
                                           IID_PPV_ARGS(&operation.p))));
        Com<IShellItem> from, to;
        QVERIFY(item(source, &from.p));
        QVERIFY(item(destination, &to.p));
        Com<IShellItemArray> array;
        QVERIFY(SUCCEEDED(SHCreateShellItemArrayFromShellItem(from.p, IID_PPV_ARGS(&array.p))));
        Com<IEnumShellItems> enumerator;
        QVERIFY(SUCCEEDED(array->EnumItems(&enumerator.p)));
        received = {};
        QVERIFY(SUCCEEDED(operation->CopyItems(enumerator.p, to.p)));
        QVERIFY(SUCCEEDED(operation->PerformOperations()));
        QCOMPARE(received.value("operation").toString(), "copy");
        // The Windows enumerator does not implement Clone. Verify native replay
        // after a failed handoff, using a new enumerator with the same contents.
        Com<IEnumShellItems> fallbackEnumerator;
        QVERIFY(SUCCEEDED(array->EnumItems(&fallbackEnumerator.p)));
        operation->SetOperationFlags(FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI);
        accepting = false;
        auto restoreAcceptance = qScopeGuard([] { accepting = true; });
        QVERIFY(SUCCEEDED(operation->CopyItems(fallbackEnumerator.p, to.p)));
        QVERIFY(SUCCEEDED(operation->PerformOperations()));
        const auto copied = destination + "/" + QFileInfo(source).fileName();
        QVERIFY(QFileInfo::exists(copied));
        QVERIFY(QFile::remove(copied));
        accepting = true;
        Com<IPersistIDList> persist;
        QVERIFY(SUCCEEDED(from->QueryInterface(IID_PPV_ARGS(&persist.p))));
        Com<IPersistIDList> only;
        only.p = new PersistOnly(persist.p);
        received = {};
        QVERIFY(SUCCEEDED(operation->MoveItems(only.p, to.p)));
        QVERIFY(SUCCEEDED(operation->PerformOperations()));
        QCOMPARE(received.value("operation").toString(), "move");
        QVERIFY(QFileInfo::exists(source));
        QCOMPARE(pendingObjects(), size_t(0));
    }
    void failedDeliveryUsesWindows() {
        accepting = false;
        received = {};
        Com<IFileOperation> operation;
        QVERIFY(SUCCEEDED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_INPROC_SERVER,
                                           IID_PPV_ARGS(&operation.p))));
        operation->SetOperationFlags(FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR);
        Com<IShellItem> from, to;
        QVERIFY(item(source, &from.p));
        QVERIFY(item(destination, &to.p));
        QVERIFY(SUCCEEDED(operation->CopyItem(from.p, to.p, nullptr, nullptr)));
        auto result = operation->PerformOperations();
        accepting = true;
        QVERIFY(SUCCEEDED(result));
        const auto target = destination + "/" + QFileInfo(source).fileName();
        QVERIFY(QFileInfo::exists(target));
        QVERIFY(QFile::remove(target));
        QCOMPARE(pendingObjects(), size_t(0));
    }
    void mixedBatchStaysNative() {
        Com<IFileOperation> operation;
        QVERIFY(SUCCEEDED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_INPROC_SERVER,
                                           IID_PPV_ARGS(&operation.p))));
        operation->SetOperationFlags(FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR);
        Com<IShellItem> from, to;
        QVERIFY(item(source, &from.p));
        QVERIFY(item(destination, &to.p));
        received = {};
        QVERIFY(SUCCEEDED(operation->CopyItem(from.p, to.p, nullptr, nullptr)));
        QVERIFY(SUCCEEDED(
            operation->NewItem(to.p, FILE_ATTRIBUTE_DIRECTORY, L"native-created-folder", nullptr, nullptr)));
        QVERIFY(SUCCEEDED(operation->PerformOperations()));
        QVERIFY(received.isEmpty());
        QVERIFY(QFileInfo::exists(destination + "/native-created-folder"));
        QVERIFY(QFile::remove(destination + "/" + QFileInfo(source).fileName()));
        QCOMPARE(pendingObjects(), size_t(0));
    }
    void abandonedOperationsReleaseMemory() {
        Com<IShellItem> from, to;
        QVERIFY(item(source, &from.p));
        QVERIFY(item(destination, &to.p));
        DWORD baselineHandles = 0;
        SIZE_T baselineMemory = 0;
        for (int i = 0; i < 1200; ++i) {
            {
                Com<IFileOperation> operation;
                QVERIFY(SUCCEEDED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_INPROC_SERVER,
                                                   IID_PPV_ARGS(&operation.p))));
                QVERIFY(SUCCEEDED(operation->CopyItem(from.p, to.p, nullptr, nullptr)));
                if (i % 2)
                    QVERIFY(SUCCEEDED(operation->PerformOperations()));
            }
            QCOMPARE(pendingObjects(), size_t(0));
            if (i == 199) {
                baselineHandles = handles();
                baselineMemory = memory();
            }
        }
        auto finalHandles = handles();
        auto finalMemory = memory();
        qInfo("Native hook soak: handles %lu -> %lu; private bytes %llu -> %llu; pending objects %llu",
              baselineHandles, finalHandles, qulonglong(baselineMemory), qulonglong(finalMemory),
              qulonglong(pendingObjects()));
        QVERIFY(finalHandles <= baselineHandles + 2);
        QVERIFY(finalMemory <= baselineMemory + 4 * 1024 * 1024);
    }
    void concurrentApartmentsAndUnknownRelease() {
        std::atomic<unsigned> failures{};
        const auto nativeSource = QDir::toNativeSeparators(source).toStdWString();
        const auto nativeDestination = QDir::toNativeSeparators(destination).toStdWString();
        std::vector<std::thread> workers;
        for (int thread = 0; thread < 4; ++thread)
            workers.emplace_back([&] {
                auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                if (FAILED(initialized)) {
                    ++failures;
                    return;
                }
                for (int i = 0; i < 200; ++i) {
                    Com<IFileOperation> operation;
                    Com<IShellItem> from, to;
                    if (FAILED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_INPROC_SERVER,
                                                IID_PPV_ARGS(&operation.p))) ||
                        FAILED(SHCreateItemFromParsingName(nativeSource.c_str(), nullptr,
                                                           IID_PPV_ARGS(&from.p))) ||
                        FAILED(SHCreateItemFromParsingName(nativeDestination.c_str(), nullptr,
                                                           IID_PPV_ARGS(&to.p)))) {
                        ++failures;
                        continue;
                    }
                    operation->SetOperationFlags(FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI);
                    if (FAILED(operation->CopyItem(from.p, to.p, nullptr, nullptr)))
                        ++failures;
                    if (i % 2 && FAILED(operation->PerformOperations()))
                        ++failures;
                    Com<IUnknown> unknown;
                    if (FAILED(operation->QueryInterface(IID_PPV_ARGS(&unknown.p))))
                        ++failures;
                    else {
                        operation.p->Release();
                        operation.p = nullptr;
                    } // Last release through IUnknown.
                }
                CoUninitialize();
            });
        for (auto &thread : workers)
            thread.join();
        QCOMPARE(failures.load(), 0u);
        QCOMPARE(pendingObjects(), size_t(0));
        QVERIFY(!QFileInfo::exists(destination + "/" + QFileInfo(source).fileName()));
    }
    void realHandoffRunsRobocopy() {
        // No hooks are installed in Explorer. Only this test process is intercepted.
        HANDLE event = CreateEventW(nullptr, TRUE, FALSE, L"Local\\StrataCopy.NativeCopy.Enabled.4");
        QVERIFY(event);
        bool existing = GetLastError() == ERROR_ALREADY_EXISTS;
        auto closeEvent = qScopeGuard([&] {
            if (!existing)
                ResetEvent(event);
            CloseHandle(event);
        });
        if (existing)
            QSKIP("An Explorer helper is already active; do not disturb its configuration.");
        HANDLE mapping =
            CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                               sizeof(strata::NativeIntegrationConfig), strata::nativeConfigurationName);
        QVERIFY(mapping);
        bool existingMap = GetLastError() == ERROR_ALREADY_EXISTS;
        auto closeMapping = qScopeGuard([&] { CloseHandle(mapping); });
        if (existingMap)
            QSKIP("Another integration configuration exists.");
        auto *config = static_cast<strata::NativeIntegrationConfig *>(
            MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(strata::NativeIntegrationConfig)));
        QVERIFY(config);
        const auto data = root.path() + "/isolated-app-state";
        QVERIFY(QDir().mkpath(data));
        const auto executable = QCoreApplication::applicationDirPath() + "/StrataCopy.exe";
        strata::NativeIntegrationConfig content;
        wcscpy_s(content.executable,
                 reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(executable).utf16()));
        wcscpy_s(content.dataDirectory,
                 reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(data).utf16()));
        *config = content;
        UnmapViewOfFile(config);
        QProcess application;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("STRATACOPY_DATA_DIR", data);
        environment.insert("QT_QPA_PLATFORM", "offscreen");
        environment.insert("QT_PLUGIN_PATH", QLibraryInfo::path(QLibraryInfo::PluginsPath));
        environment.insert("STRATACOPY_RENDERER", "software");
        environment.insert("STRATACOPY_DIAGNOSTICS", root.path() + "/app-diagnostics.log");
        application.setProcessEnvironment(environment);
        application.start(executable);
        QVERIFY(application.waitForStarted(5000));
        auto closeApplication = qScopeGuard([&] {
            application.kill();
            application.waitForFinished(5000);
        });
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(data + "/stratacopy.sqlite"), 10000);
        auto initialize = reinterpret_cast<BOOL(WINAPI *)(BOOL(WINAPI *)(const wchar_t *))>(
            GetProcAddress(library, "StrataCopyTestInitialize"));
        QVERIFY(initialize(nullptr));
        auto restoreReceiver = qScopeGuard([&] { initialize(receive); });
        QVERIFY(SetEvent(event));
        const auto oldPlatform = qgetenv("QT_QPA_PLATFORM"), oldPlugins = qgetenv("QT_PLUGIN_PATH"),
                   oldRenderer = qgetenv("STRATACOPY_RENDERER");
        auto restoreEnvironment = qScopeGuard([&] {
            for (const auto &entry :
                 std::vector<std::pair<const char *, QByteArray>>{{"QT_QPA_PLATFORM", oldPlatform},
                                                                  {"QT_PLUGIN_PATH", oldPlugins},
                                                                  {"STRATACOPY_RENDERER", oldRenderer}})
                if (entry.second.isNull())
                    qunsetenv(entry.first);
                else
                    qputenv(entry.first, entry.second);
        });
        qputenv("QT_QPA_PLATFORM", "offscreen");
        qputenv("QT_PLUGIN_PATH", QLibraryInfo::path(QLibraryInfo::PluginsPath).toUtf8());
        qputenv("STRATACOPY_RENDERER", "software");
        Com<IFileOperation> operation;
        QVERIFY(SUCCEEDED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_INPROC_SERVER,
                                           IID_PPV_ARGS(&operation.p))));
        Com<IShellItem> from, to;
        QVERIFY(item(source, &from.p));
        QVERIFY(item(destination, &to.p));
        QVERIFY(SUCCEEDED(operation->CopyItem(from.p, to.p, nullptr, nullptr)));
        QVERIFY(SUCCEEDED(operation->PerformOperations()));
        auto launchPid =
            reinterpret_cast<DWORD(WINAPI *)()>(GetProcAddress(library, "StrataCopyLastLaunchPid"));
        QVERIFY(launchPid);
        QVERIFY(launchPid());
        HANDLE client = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION,
                                    FALSE, launchPid());
        QVERIFY(client);
        auto closeClient = qScopeGuard([&] {
            if (WaitForSingleObject(client, 0) != WAIT_OBJECT_0) {
                TerminateProcess(client, 0);
                WaitForSingleObject(client, 5000);
            }
            CloseHandle(client);
        });
        QTRY_COMPARE_WITH_TIMEOUT(WaitForSingleObject(client, 0), DWORD(WAIT_OBJECT_0), 10000);
        DWORD clientExit{};
        QVERIFY(GetExitCodeProcess(client, &clientExit));
        QCOMPARE(clientExit, DWORD(0));
        const auto target = destination + "/" + QFileInfo(source).fileName();
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(target), 15000);
        QFile result(target);
        QVERIFY(result.open(QIODevice::ReadOnly));
        QCOMPARE(result.readAll(), QByteArray("content"));
        QTRY_VERIFY_WITH_TIMEOUT(
            QDir(data + "/ExplorerRequests").entryList({"*.json"}, QDir::Files).isEmpty(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!QDir(data + "/logs").entryList({"*.log"}, QDir::Files).isEmpty(), 5000);
        QCOMPARE(pendingObjects(), size_t(0));
    }
    void cleanupTestCase() {
        QCOMPARE(pendingObjects(), size_t(0));
        if (library)
            FreeLibrary(library); // Hook pins its code until this isolated process exits.
        CoUninitialize();
    }
};
QTEST_GUILESS_MAIN(NativeHookTests)
#include "NativeHookTests.moc"
