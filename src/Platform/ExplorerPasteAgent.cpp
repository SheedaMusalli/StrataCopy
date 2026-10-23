#include "NativeExplorerHost.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <shellapi.h>
#include <oleidl.h>
#include <memory>
#include <vector>
namespace {
constexpr wchar_t windowClass[] = L"StrataCopy.ExplorerPaste.Agent.1";
struct ClipboardFiles {
    QStringList paths;
    bool move = false;
    DWORD sequence = 0;
};
QString requestDirectory() {
    return qEnvironmentVariable("STRATACOPY_DATA_DIR",
                                QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)) +
           "/ExplorerRequests";
}
std::shared_ptr<const ClipboardFiles> readFiles(HWND owner) {
    if (!IsClipboardFormatAvailable(CF_HDROP) || !OpenClipboard(owner))
        return {};
    struct Close {
        ~Close() {
            CloseClipboard();
        }
    } close;
    const auto drop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
    if (!drop)
        return {};
    const auto count = DragQueryFileW(drop, 0xffffffff, nullptr, 0);
    if (!count || count > 4096)
        return {};
    auto result = std::make_shared<ClipboardFiles>();
    qsizetype total = 0;
    for (UINT i = 0; i < count; ++i) {
        const auto length = DragQueryFileW(drop, i, nullptr, 0);
        if (!length || length > 32767)
            return {};
        std::vector<wchar_t> path(length + 1);
        DragQueryFileW(drop, i, path.data(), UINT(path.size()));
        auto value = QString::fromWCharArray(path.data(), length);
        total += value.size();
        if (total > 100000)
            return {};
        result->paths << value;
    }
    const auto effect = GetClipboardData(RegisterClipboardFormatW(L"Preferred DropEffect"));
    if (effect && GlobalSize(effect) >= sizeof(DWORD)) {
        if (const auto *value = static_cast<const DWORD *>(GlobalLock(effect))) {
            result->move = (*value & DROPEFFECT_MOVE) != 0;
            GlobalUnlock(effect);
        }
    }
    result->sequence = GetClipboardSequenceNumber();
    return result;
}
bool launchRequest(const ClipboardFiles &files, const QString &destination) {
    if (!QFileInfo(destination).isDir())
        return false;
    const auto directory = requestDirectory();
    if (!QDir().mkpath(directory))
        return false;
    if (QDir(directory).entryList({"*.json"}, QDir::Files).size() >= 64)
        return false;
    const auto path = directory + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".json";
    const auto payload = QJsonDocument(QJsonObject{{"sources", QJsonArray::fromStringList(files.paths)},
                                                   {"destination", destination},
                                                   {"operation", files.move ? "move" : "copy"}})
                             .toJson(QJsonDocument::Compact);
    if (payload.size() > 512 * 1024)
        return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(payload) != payload.size() || !file.commit())
        return false;
    const auto executable = QCoreApplication::applicationDirPath() + "/StrataCopy.exe";
    if (QProcess::startDetached(executable, {"--paste-request", path}))
        return true;
    QFile::remove(path);
    return false;
}
LRESULT CALLBACK agentWindow(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_CLOSE) {
        QCoreApplication::quit();
        return 0;
    }
    return DefWindowProcW(window, message, w, l);
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    app.setOrganizationName("StrataCopy");
    app.setApplicationName("StrataCopy");
    const auto arguments = app.arguments();
    if (arguments.contains("--stop")) {
        strata::UniqueHandle event(
            OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Local\\StrataCopy.NativeCopy.Enabled.4"));
        if (event)
            ResetEvent(event.get());
        if (auto window = FindWindowExW(HWND_MESSAGE, nullptr, windowClass, nullptr)) {
            DWORD processId{};
            GetWindowThreadProcessId(window, &processId);
            strata::UniqueHandle process(OpenProcess(SYNCHRONIZE, FALSE, processId));
            PostMessageW(window, WM_CLOSE, 0, 0);
            if (process && WaitForSingleObject(process.get(), 2500) != WAIT_OBJECT_0)
                return 13;
        }
        return 0;
    }
    if (arguments.contains("--self-test")) {
        const auto nativeFile = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/" +
                                                         STRATACOPY_NATIVE_DLL_NAME);
        HMODULE native = LoadLibraryExW(reinterpret_cast<LPCWSTR>(nativeFile.utf16()), nullptr,
                                        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!native)
            return 11;
        const bool hasEntry = GetProcAddress(native, "StrataCopyMessageHook") != nullptr;
        FreeLibrary(native); // Merely loading the DLL does not install hooks.
        if (!hasEntry)
            return 12;
        ClipboardFiles files;
        files.paths = {QString::fromUtf8("C:/日本語/quoted name.txt")};
        auto object =
            QJsonObject{{"sources", QJsonArray::fromStringList(files.paths)}, {"operation", "copy"}};
        return QJsonDocument::fromJson(QJsonDocument(object).toJson()).object() == object ? 0 : 10;
    }
    if (arguments.contains("--paste")) {
        const auto index = arguments.indexOf("--paste");
        if (index + 1 >= arguments.size())
            return 2;
        const auto files = readFiles(nullptr);
        return files && launchRequest(*files, arguments[index + 1]) ? 0 : 3;
    }
    strata::UniqueHandle instance;
    const auto deadline = GetTickCount64() + 5000;
    while (GetTickCount64() < deadline) {
        instance.reset(CreateMutexW(nullptr, FALSE, L"Local\\StrataCopy.ExplorerPaste.Agent.1"));
        if (!instance)
            return 8;
        if (GetLastError() != ERROR_ALREADY_EXISTS)
            break;
        instance.reset();
        strata::UniqueHandle live(OpenEventW(SYNCHRONIZE, FALSE, L"Local\\StrataCopy.NativeCopy.Enabled.4"));
        if (live && WaitForSingleObject(live.get(), 0) == WAIT_OBJECT_0)
            return 0;
        if (auto previous = FindWindowExW(HWND_MESSAGE, nullptr, windowClass, nullptr))
            PostMessageW(previous, WM_CLOSE, 0, 0);
        Sleep(50); // Let a stopped/older helper release its hooks and singleton.
    }
    if (!instance)
        return 8;
    WNDCLASSW cls{};
    cls.lpfnWndProc = agentWindow;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = windowClass;
    if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return 5;
    struct WindowOwner {
        HWND window{};
        ~WindowOwner() {
            if (window)
                DestroyWindow(window);
        }
    } window;
    window.window = CreateWindowExW(0, windowClass, L"StrataCopy Explorer integration", 0, 0, 0, 0, 0,
                                    HWND_MESSAGE, nullptr, cls.hInstance, nullptr);
    if (!window.window)
        return 6;
    strata::NativeExplorerHost host;
    if (!host.start())
        return 7;
    QTimer refresh;
    QObject::connect(&refresh, &QTimer::timeout, &app, [&] { host.refresh(); });
    refresh.start(1000);
    return app.exec();
}
