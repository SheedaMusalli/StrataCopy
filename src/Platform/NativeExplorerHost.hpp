#pragma once
#include "UniqueHandle.hpp"
#include "NativeIntegrationConfig.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QTimer>
#include <QStandardPaths>
#include <map>
#include <set>
#include <memory>
#include <string>

namespace strata {
// Only Explorer's GUI threads are hooked. The loaded DLL instruments the native
// file-operation implementation across that Explorer process, including workers.
class NativeExplorerHost {
    HMODULE module{};
    HOOKPROC callback{};
    UniqueHandle enabledEvent;
    UniqueHandle configuration;
    struct ThreadHook {
        HHOOK hook{};
        UniqueHandle thread;
        ~ThreadHook() {
            if (hook)
                UnhookWindowsHookEx(hook);
        }
    };
    std::map<DWORD, std::unique_ptr<ThreadHook>> hooks;
    static BOOL CALLBACK collect(HWND window, LPARAM parameter) {
        auto &threads = *reinterpret_cast<std::set<DWORD> *>(parameter);
        if (threads.size() >= 128)
            return TRUE;
        DWORD pid{};
        auto tid = GetWindowThreadProcessId(window, &pid);
        UniqueHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
        wchar_t path[32768]{}, windows[32768]{};
        DWORD length = 32768;
        if (!process || !QueryFullProcessImageNameW(process.get(), 0, path, &length) ||
            !GetWindowsDirectoryW(windows, 32768))
            return TRUE;
        auto expected = std::wstring(windows) + L"\\explorer.exe";
        if (_wcsicmp(path, expected.c_str()) == 0)
            threads.insert(tid);
        return TRUE;
    }

  public:
    ~NativeExplorerHost() {
        if (enabledEvent)
            ResetEvent(enabledEvent.get());
        hooks.clear();
        if (module)
            FreeLibrary(module);
    }
    bool start() {
        const auto file = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/" +
                                                   STRATACOPY_NATIVE_DLL_NAME);
        module = LoadLibraryExW(reinterpret_cast<LPCWSTR>(file.utf16()), nullptr,
                                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!module)
            return false;
        callback = reinterpret_cast<HOOKPROC>(GetProcAddress(module, "StrataCopyMessageHook"));
        if (!callback)
            return false;
        configuration.reset(CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                               sizeof(NativeIntegrationConfig), nativeConfigurationName));
        if (!configuration)
            return false;
        auto *config = static_cast<NativeIntegrationConfig *>(
            MapViewOfFile(configuration.get(), FILE_MAP_WRITE, 0, 0, sizeof(NativeIntegrationConfig)));
        if (!config)
            return false;
        NativeIntegrationConfig content;
        content.diagnostics = qEnvironmentVariableIntValue("STRATACOPY_EXPLORER_TRACE") != 0;
        auto executable =
            QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/StrataCopy.exe");
        auto directory = QDir::toNativeSeparators(qEnvironmentVariable(
            "STRATACOPY_DATA_DIR", QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)));
        if (!QDir().mkpath(directory)) {
            UnmapViewOfFile(config);
            return false;
        }
        if (executable.size() >= 32768 || directory.size() >= 32768) {
            UnmapViewOfFile(config);
            return false;
        }
        wcscpy_s(content.executable, reinterpret_cast<const wchar_t *>(executable.utf16()));
        wcscpy_s(content.dataDirectory, reinterpret_cast<const wchar_t *>(directory.utf16()));
        *config = content;
        UnmapViewOfFile(config);
        enabledEvent.reset(CreateEventW(nullptr, TRUE, FALSE, L"Local\\StrataCopy.NativeCopy.Enabled.4"));
        if (!enabledEvent || !SetEvent(enabledEvent.get()))
            return false;
        refresh();
        return true;
    }
    void refresh() {
        for (auto it = hooks.begin(); it != hooks.end();) {
            if (WaitForSingleObject(it->second->thread.get(), 0) == WAIT_OBJECT_0)
                it = hooks.erase(it);
            else
                ++it;
        }
        std::set<DWORD> threads;
        EnumWindows(collect, reinterpret_cast<LPARAM>(&threads));
        for (auto tid : threads) {
            if (hooks.contains(tid) || hooks.size() >= 128)
                continue;
            auto entry = std::make_unique<ThreadHook>();
            entry->thread.reset(OpenThread(SYNCHRONIZE | THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid));
            if (!entry->thread)
                continue;
            const auto markerName = L"Local\\StrataCopy.NativeCopy.Installed.4." +
                                    std::to_wstring(GetProcessIdOfThread(entry->thread.get()));
            UniqueHandle initialized(OpenMutexW(SYNCHRONIZE, FALSE, markerName.c_str()));
            if (initialized)
                continue; // The process already has persistent detours; do not load another DLL copy.
            entry->hook = SetWindowsHookExW(WH_GETMESSAGE, callback, module, tid);
            if (!entry->hook)
                continue;
            PostThreadMessageW(tid, WM_NULL, 0, 0);
            hooks.emplace(tid, std::move(entry));
        }
    }
};
} // namespace strata
