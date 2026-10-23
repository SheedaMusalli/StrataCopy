// Native Explorer interception. No Qt, clipboard polling, or work in DllMain.
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <MinHook.h>
#include "NativeIntegrationConfig.hpp"
#include "UniqueHandle.hpp"
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>

namespace {
constexpr wchar_t enableEvent[] = L"Local\\StrataCopy.NativeCopy.Enabled.5";
HMODULE moduleHandle{};
std::recursive_mutex stateMutex;
thread_local bool replaying = false;
using TestReceiver = BOOL(WINAPI *)(const wchar_t *);
TestReceiver testReceiver{};
std::atomic<bool> ready{false};
std::atomic<bool> interceptionFault{false};
std::atomic<DWORD> lastLaunchPid{};
strata::UniqueHandle installedMarker;

template <class T> struct Ref {
    T *p{};
    Ref() = default;
    explicit Ref(T *v) : p(v) {
        if (p)
            p->AddRef();
    }
    Ref(const Ref &v) : Ref(v.p) {}
    Ref(Ref &&v) noexcept : p(v.p) {
        v.p = nullptr;
    }
    ~Ref() {
        if (p)
            p->Release();
    }
    Ref &operator=(Ref v) noexcept {
        std::swap(p, v.p);
        return *this;
    }
};
struct Pending {
    std::vector<std::wstring> sources;
    std::wstring destination;
    bool move{}, remove{};
    std::function<HRESULT()> replay;
};
struct State {
    std::recursive_mutex mutex;
    std::vector<Pending> pending;
    DWORD flags = FOF_ALLOWUNDO;
    bool native = false;
    size_t characters = 0, sources = 0;
};
// Explorer's enumerator commonly does not implement Clone. Keep a bounded prefix
// plus the untouched remainder so a later native fallback never loses items.
class ReplayEnumerator final : public IEnumShellItems {
    std::atomic<ULONG> references{1};
    size_t position = 0;

  public:
    std::vector<Ref<IShellItem>> prefix;
    Ref<IEnumShellItems> remainder;
    explicit ReplayEnumerator(IEnumShellItems *tail) : remainder(tail) {
        prefix.reserve(4097);
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) override {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (id == IID_IUnknown || id == IID_IEnumShellItems) {
            *out = static_cast<IEnumShellItems *>(this);
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
    HRESULT STDMETHODCALLTYPE Next(ULONG count, IShellItem **out, ULONG *fetched) override {
        if (!out || (!fetched && count != 1))
            return E_POINTER;
        ULONG got = 0;
        while (got < count && position < prefix.size()) {
            out[got] = prefix[position++].p;
            out[got++]->AddRef();
        }
        HRESULT result = S_OK;
        if (got < count && remainder.p) {
            ULONG more = 0;
            result = remainder.p->Next(count - got, out + got, &more);
            got += more;
        }
        if (fetched)
            *fetched = got;
        return FAILED(result) ? result : (got == count ? S_OK : S_FALSE);
    }
    HRESULT STDMETHODCALLTYPE Skip(ULONG count) override {
        const auto available = prefix.size() - position;
        const auto skipped = std::min<size_t>(count, available);
        position += skipped;
        if (skipped == count)
            return S_OK;
        return remainder.p ? remainder.p->Skip(count - ULONG(skipped)) : S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE Reset() override {
        if (remainder.p)
            return E_NOTIMPL;
        position = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Clone(IEnumShellItems **out) override {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (remainder.p)
            return E_NOTIMPL;
        try {
            auto copy = std::make_unique<ReplayEnumerator>(nullptr);
            copy->prefix = prefix;
            copy->position = position;
            *out = copy.release();
            return S_OK;
        } catch (...) {
            return E_OUTOFMEMORY;
        }
    }
};
std::map<IFileOperation *, std::shared_ptr<State>> states;
std::shared_ptr<State> stateFor(IFileOperation *self) {
    std::lock_guard lock(stateMutex);
    auto found = states.find(self);
    if (found != states.end())
        return found->second;
    if (states.size() >= 64) {
        interceptionFault = true;
        return {};
    }
    auto state = std::make_shared<State>();
    states.emplace(self, state);
    return state;
}
void *original[23]{};
template <class F> F function(int slot) {
    return reinterpret_cast<F>(original[slot]);
}
using Single = HRESULT(STDMETHODCALLTYPE *)(IFileOperation *, IShellItem *, IShellItem *, LPCWSTR,
                                            IFileOperationProgressSink *);
using Multiple = HRESULT(STDMETHODCALLTYPE *)(IFileOperation *, IUnknown *, IShellItem *);
using Perform = HRESULT(STDMETHODCALLTYPE *)(IFileOperation *);
using Release = ULONG(STDMETHODCALLTYPE *)(IFileOperation *);
struct ReplayScope {
    bool previous = replaying;
    ReplayScope() {
        replaying = true;
    }
    ~ReplayScope() {
        replaying = previous;
    }
};
bool enabled() {
    if (testReceiver)
        return true;
    HANDLE event = OpenEventW(SYNCHRONIZE, FALSE, enableEvent);
    if (!event)
        return false;
    bool result = WaitForSingleObject(event, 0) == WAIT_OBJECT_0;
    CloseHandle(event);
    return result;
}
void trace(const char *label, unsigned value) noexcept {
    try {
        if (!enabled() || testReceiver)
            return;
        strata::UniqueHandle mapping(OpenFileMappingW(FILE_MAP_READ, FALSE, strata::nativeConfigurationName));
        if (!mapping)
            return;
        const auto *config = static_cast<const strata::NativeIntegrationConfig *>(
            MapViewOfFile(mapping.get(), FILE_MAP_READ, 0, 0, sizeof(strata::NativeIntegrationConfig)));
        struct Unmap {
            const void *p;
            ~Unmap() {
                if (p)
                    UnmapViewOfFile(p);
            }
        } unmap{config};
        if (!config || !config->diagnostics || config->dataDirectory[32767])
            return;
        const auto path = std::wstring(config->dataDirectory) + L"\\Explorer-hook-" +
                          std::to_wstring(GetCurrentProcessId()) + L".log";
        static std::mutex logMutex;
        std::lock_guard lock(logMutex);
        strata::UniqueHandle file(CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!file)
            return;
        LARGE_INTEGER size{};
        GetFileSizeEx(file.get(), &size);
        if (size.QuadPart > 65536) {
            SetFilePointer(file.get(), 0, nullptr, FILE_BEGIN);
            SetEndOfFile(file.get());
        }
        SetFilePointer(file.get(), 0, nullptr, FILE_END);
        char buffer[128];
        const auto length = std::snprintf(buffer, sizeof(buffer), "%s %u\r\n", label, value);
        DWORD written{};
        if (length > 0 && length < int(sizeof(buffer)))
            WriteFile(file.get(), buffer, DWORD(length), &written, nullptr);
    } catch (...) {
    }
}
std::wstring pathOf(IShellItem *item) {
    if (!item)
        return {};
    PWSTR value = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &value)))
        return {};
    struct Free {
        PWSTR value;
        ~Free() {
            CoTaskMemFree(value);
        }
    } free{value};
    std::wstring result(value ? value : L"");
    // Robocopy cannot operate on virtual namespaces or top-level reparse points.
    const auto attr = GetFileAttributesW(result.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_REPARSE_POINT))
        return {};
    return result;
}
bool itemPaths(IUnknown *items, std::vector<std::wstring> &paths, Ref<IUnknown> &replaySource) {
    if (!items)
        return false;
    size_t characters = 0;
    auto append = [&](IShellItem *item) {
        auto path = pathOf(item);
        characters += path.size();
        if (path.empty() || characters > 80000 || paths.size() >= 4096)
            return false;
        paths.push_back(std::move(path));
        return true;
    };
    IShellItemArray *array = nullptr;
    if (SUCCEEDED(items->QueryInterface(IID_PPV_ARGS(&array)))) {
        Ref<IShellItemArray> owner(array);
        array->Release();
        DWORD count = 0;
        if (FAILED(array->GetCount(&count)) || !count || count > 4096)
            return false;
        for (DWORD i = 0; i < count; ++i) {
            IShellItem *item = nullptr;
            if (FAILED(array->GetItemAt(i, &item)))
                return false;
            Ref<IShellItem> owner(item);
            item->Release();
            if (!append(item))
                return false;
        }
        trace("items-array", count);
        return true;
    }
    IEnumShellItems *enumerator = nullptr;
    if (SUCCEEDED(items->QueryInterface(IID_PPV_ARGS(&enumerator)))) {
        Ref<IEnumShellItems> owner(enumerator);
        enumerator->Release();
        IEnumShellItems *clone = nullptr;
        // Clone when possible; otherwise retain a replayable prefix for fallback.
        const bool cloned = SUCCEEDED(enumerator->Clone(&clone));
        ReplayEnumerator *replay = nullptr;
        if (!cloned) {
            replay = new ReplayEnumerator(enumerator);
            replaySource = Ref<IUnknown>(replay);
            replay->Release();
            clone = enumerator;
            clone->AddRef();
        }
        Ref<IEnumShellItems> cloneOwner(clone);
        clone->Release();
        for (;;) {
            IShellItem *item = nullptr;
            ULONG fetched = 0;
            const auto result = clone->Next(1, &item, &fetched);
            Ref<IShellItem> itemOwner(item);
            if (item)
                item->Release();
            if (FAILED(result))
                return false;
            if (!fetched) {
                if (replay)
                    replay->remainder = Ref<IEnumShellItems>();
                trace("items-enumerator", unsigned(paths.size()));
                return !paths.empty();
            }
            if (replay && item)
                replay->prefix.emplace_back(item);
            if (!item || !append(item))
                return false;
        }
    }
    IShellItem *single = nullptr;
    if (SUCCEEDED(items->QueryInterface(IID_PPV_ARGS(&single)))) {
        Ref<IShellItem> owner(single);
        single->Release();
        trace("items-single", 1);
        return append(single);
    }
    IPersistIDList *persist = nullptr;
    if (SUCCEEDED(items->QueryInterface(IID_PPV_ARGS(&persist)))) {
        Ref<IPersistIDList> owner(persist);
        persist->Release();
        PIDLIST_ABSOLUTE pidl = nullptr;
        if (FAILED(persist->GetIDList(&pidl)))
            return false;
        struct Free {
            PIDLIST_ABSOLUTE p;
            ~Free() {
                CoTaskMemFree(p);
            }
        } free{pidl};
        if (!pidl || FAILED(SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&single))))
            return false;
        Ref<IShellItem> itemOwner(single);
        single->Release();
        trace("items-persist-id", 1);
        return append(single);
    }
    IDataObject *data = nullptr;
    if (FAILED(items->QueryInterface(IID_PPV_ARGS(&data)))) {
        trace("items-unknown", 1);
        return false;
    }
    Ref<IDataObject> owner(data);
    data->Release();
    const auto result = SHCreateShellItemArrayFromDataObject(data, IID_PPV_ARGS(&array));
    if (FAILED(result)) {
        trace("items-data-failed", unsigned(result));
        return false;
    }
    Ref<IShellItemArray> arrayOwner(array);
    array->Release();
    trace("items-data", 1);
    return itemPaths(array, paths, replaySource);
}
HRESULT flush(State &state) {
    ReplayScope guard;
    HRESULT result = S_OK;
    for (auto &operation : state.pending) {
        auto hr = operation.replay();
        if (FAILED(hr) && SUCCEEDED(result))
            result = hr;
    }
    state.pending.clear();
    state.characters = state.sources = 0;
    state.native = true;
    return result;
}
HRESULT nativeOnly(IFileOperation *self) noexcept {
    try {
        if (replaying)
            return S_OK;
        auto state = stateFor(self);
        if (!state)
            return S_OK;
        std::lock_guard lock(state->mutex);
        return flush(*state);
    } catch (...) {
        interceptionFault = true;
        return E_OUTOFMEMORY;
    }
}
bool compatible(const Pending &operation) {
    if (operation.remove) {
        if (operation.sources.empty())
            return false;
        for (const auto &source : operation.sources)
            if (source.size() < 4 || source.back() == L'\\' || source.back() == L'/' ||
                GetFileAttributesW(source.c_str()) == INVALID_FILE_ATTRIBUTES)
                return false;
        return true;
    }
    if (operation.sources.empty() || operation.destination.empty())
        return false;
    const auto attributes = GetFileAttributesW(operation.destination.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
        return false;
    for (const auto &source : operation.sources) {
        auto split = source.find_last_of(L"\\/");
        if (split == std::wstring::npos || split + 1 == source.size())
            return false;
        // Same-folder duplicate naming and overlapping trees need native shell semantics.
        auto parent = source.substr(0, split);
        if (_wcsicmp(parent.c_str(), operation.destination.c_str()) == 0)
            return false;
        auto isInside = [](std::wstring a, std::wstring b) {
            std::replace(a.begin(), a.end(), L'/', L'\\');
            std::replace(b.begin(), b.end(), L'/', L'\\');
            while (b.size() > 3 && b.back() == L'\\')
                b.pop_back();
            return a.size() >= b.size() && _wcsnicmp(a.c_str(), b.c_str(), b.size()) == 0 &&
                   (a.size() == b.size() || b.back() == L'\\' || a[b.size()] == L'\\');
        };
        if (isInside(operation.destination, source))
            return false;
    }
    return true;
}
HRESULT queue(IFileOperation *self, Pending operation, bool supported) {
    trace("queue-supported-input", supported);
    auto owner = stateFor(self);
    if (!owner) {
        ReplayScope guard;
        return operation.replay();
    }
    std::lock_guard lock(owner->mutex);
    auto &state = *owner;
    if (replaying || interceptionFault || !enabled()) {
        auto hr = flush(state);
        if (FAILED(hr))
            return hr;
        ReplayScope guard;
        return operation.replay();
    }
    size_t chars = operation.destination.size();
    for (const auto &path : operation.sources)
        chars += path.size();
    supported = supported && compatible(operation) && state.sources + operation.sources.size() <= 4096 &&
                state.characters + chars <= 80000;
    if (!state.pending.empty()) {
        const auto &first = state.pending.front();
        supported = supported && first.move == operation.move && first.remove == operation.remove &&
                    _wcsicmp(first.destination.c_str(), operation.destination.c_str()) == 0;
    }
    if (state.native || !supported) {
        trace("queue-native-state", state.native);
        trace("queue-compatible", supported);
        auto hr = flush(state);
        if (FAILED(hr))
            return hr;
        ReplayScope guard;
        return operation.replay();
    }
    state.characters += chars;
    state.sources += operation.sources.size();
    state.pending.push_back(std::move(operation));
    return S_OK;
}
HRESULT single(int slot, IFileOperation *self, IShellItem *source, IShellItem *destination, LPCWSTR name,
               IFileOperationProgressSink *sink) noexcept {
    trace("single-slot", slot);
    if (replaying)
        return function<Single>(slot)(self, source, destination, name, sink);
    try {
        Pending pending;
        pending.move = slot == 14;
        auto path = pathOf(source);
        if (!path.empty())
            pending.sources.push_back(std::move(path));
        pending.destination = pathOf(destination);
        Ref<IShellItem> from(source), to(destination);
        Ref<IFileOperationProgressSink> progress(sink);
        std::wstring newName = name ? name : L"";
        bool hasName = name != nullptr;
        pending.replay = [=] {
            return function<Single>(slot)(self, from.p, to.p, hasName ? newName.c_str() : nullptr,
                                          progress.p);
        };
        const auto separator =
            pending.sources.empty() ? std::wstring::npos : pending.sources[0].find_last_of(L"\\/");
        const bool originalName = !name || !*name ||
                                  (separator != std::wstring::npos &&
                                   _wcsicmp(name, pending.sources[0].c_str() + separator + 1) == 0);
        return queue(self, std::move(pending), originalName);
    } catch (...) {
        auto hr = nativeOnly(self);
        return FAILED(hr) ? hr : function<Single>(slot)(self, source, destination, name, sink);
    }
}
HRESULT multiple(int slot, IFileOperation *self, IUnknown *sources, IShellItem *destination) noexcept {
    trace("multiple-slot", slot);
    if (replaying)
        return function<Multiple>(slot)(self, sources, destination);
    Ref<IUnknown> replaySource(sources);
    try {
        Pending pending;
        pending.move = slot == 15;
        bool supported = itemPaths(sources, pending.sources, replaySource);
        pending.destination = pathOf(destination);
        Ref<IUnknown> from(replaySource);
        Ref<IShellItem> to(destination);
        pending.replay = [=] { return function<Multiple>(slot)(self, from.p, to.p); };
        return queue(self, std::move(pending), supported);
    } catch (...) {
        auto hr = nativeOnly(self);
        return FAILED(hr) ? hr : function<Multiple>(slot)(self, replaySource.p, destination);
    }
}
HRESULT STDMETHODCALLTYPE moveOne(IFileOperation *s, IShellItem *a, IShellItem *b, LPCWSTR n,
                                  IFileOperationProgressSink *p) {
    return single(14, s, a, b, n, p);
}
HRESULT STDMETHODCALLTYPE moveMany(IFileOperation *s, IUnknown *a, IShellItem *b) {
    return multiple(15, s, a, b);
}
HRESULT STDMETHODCALLTYPE copyOne(IFileOperation *s, IShellItem *a, IShellItem *b, LPCWSTR n,
                                  IFileOperationProgressSink *p) {
    return single(16, s, a, b, n, p);
}
HRESULT STDMETHODCALLTYPE copyMany(IFileOperation *s, IUnknown *a, IShellItem *b) {
    return multiple(17, s, a, b);
}

std::wstring jsonString(const std::wstring &s) {
    std::wstring value = L"\"";
    constexpr wchar_t hex[] = L"0123456789abcdef";
    for (wchar_t c : s) {
        if (c == L'"' || c == L'\\') {
            value += L'\\';
            value += c;
        } else if (c < 32) {
            value += L"\\u00";
            value += hex[(c >> 4) & 15];
            value += hex[c & 15];
        } else
            value += c;
    }
    return value + L'"';
}
std::wstring payload(const State &state) {
    const auto &first = state.pending.front();
    std::wstring json = L"{\"native\":true,\"operation\":\"";
    json += first.remove ? ((state.flags & (FOF_ALLOWUNDO | FOFX_RECYCLEONDELETE)) ? L"recycle" : L"delete")
            : first.move ? L"move"
                         : L"copy";
    json += L"\",\"destination\":" + jsonString(first.destination) + L",\"sources\":[";
    bool comma = false;
    for (const auto &op : state.pending)
        for (const auto &source : op.sources) {
            if (comma)
                json += L',';
            json += jsonString(source);
            comma = true;
        }
    return json + L"]}";
}
bool deliver(const std::wstring &json) {
    if (testReceiver)
        return testReceiver(json.c_str()) != FALSE;
    strata::UniqueHandle mapping(OpenFileMappingW(FILE_MAP_READ, FALSE, strata::nativeConfigurationName));
    if (!mapping)
        return false;
    const auto *config = static_cast<const strata::NativeIntegrationConfig *>(
        MapViewOfFile(mapping.get(), FILE_MAP_READ, 0, 0, sizeof(strata::NativeIntegrationConfig)));
    struct Unmap {
        const void *value;
        ~Unmap() {
            if (value)
                UnmapViewOfFile(value);
        }
    } unmap{config};
    std::wstring exe, dir;
    if (config && config->version == 1 && config->executable[32767] == 0 &&
        config->dataDirectory[32767] == 0) {
        exe = config->executable;
        dir = config->dataDirectory;
    }
    if (exe.empty() || dir.empty())
        return false;
    if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES)
        return false;
    const auto dataDirectory = dir;
    dir += L"\\ExplorerRequests";
    if (!CreateDirectoryW(dir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return false;
    WIN32_FIND_DATAW entry{};
    HANDLE search = FindFirstFileW((dir + L"\\*.json").c_str(), &entry);
    unsigned count = 0;
    if (search != INVALID_HANDLE_VALUE) {
        do {
            ++count;
        } while (count < 64 && FindNextFileW(search, &entry));
        FindClose(search);
    }
    if (count >= 64)
        return false;
    GUID id{};
    if (FAILED(CoCreateGuid(&id)))
        return false;
    wchar_t identifier[40]{};
    StringFromGUID2(id, identifier, 40);
    auto file = dir + L"\\" + identifier + L".json";
    auto temp = file + L".tmp";
    std::wstring command =
        L"\"" + exe + L"\" --paste-request \"" + file + L"\" --data-directory \"" + dataDirectory + L"\"";
    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, json.data(), int(json.size()), nullptr, 0,
                                   nullptr, nullptr);
    if (size <= 0 || size > 512 * 1024)
        return false;
    std::string bytes(size, '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, json.data(), int(json.size()), bytes.data(), size,
                             nullptr, nullptr))
        return false;
    HANDLE stream = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (stream == INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    bool ok = WriteFile(stream, bytes.data(), DWORD(size), &written, nullptr) && written == DWORD(size) &&
              FlushFileBuffers(stream);
    CloseHandle(stream);
    if (!ok || !MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp.c_str());
        return false;
    }
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup,
                        &process)) {
        DeleteFileW(file.c_str());
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    lastLaunchPid = process.dwProcessId;
    return true;
}
HRESULT STDMETHODCALLTYPE perform(IFileOperation *self) noexcept {
    trace("perform", 1);
    if (replaying)
        return function<Perform>(21)(self);
    try {
        State state;
        std::shared_ptr<State> owner;
        {
            std::lock_guard lock(stateMutex);
            auto found = states.find(self);
            if (found != states.end()) {
                owner = std::move(found->second);
                states.erase(found);
            }
        }
        if (owner) {
            std::lock_guard lock(owner->mutex);
            state.pending = std::move(owner->pending);
            state.native = owner->native;
            state.flags = owner->flags;
        }
        trace("perform-pending", unsigned(state.pending.size()));
        trace("perform-native", state.native);
        if (!interceptionFault && !state.native && !state.pending.empty() && enabled() &&
            deliver(payload(state)))
            return S_OK;
        auto hr = flush(state);
        return FAILED(hr) ? hr : function<Perform>(21)(self);
    } catch (...) {
        return E_OUTOFMEMORY;
    }
}
ULONG STDMETHODCALLTYPE release(IFileOperation *self) {
    auto result = function<Release>(2)(self);
    std::shared_ptr<State> owner;
    if (!result) {
        std::lock_guard lock(stateMutex);
        auto found = states.find(self);
        if (found != states.end()) {
            owner = std::move(found->second);
            states.erase(found);
        }
    } // Release retained COM arguments outside the process-wide map lock.
    return result;
}
// Any operation that cannot be represented by Robocopy flushes the complete batch
// back to the original object. No partially intercepted mixed batches.
#define NATIVE_GUARD                                                                                         \
    trace(__func__, 1);                                                                                      \
    auto status = nativeOnly(self);                                                                          \
    if (FAILED(status))                                                                                      \
    return status
HRESULT STDMETHODCALLTYPE properties(IFileOperation *self, IPropertyChangeArray *p) {
    UINT count = 0;
    if (p && SUCCEEDED(p->GetCount(&count)) && count == 0)
        return function<HRESULT(STDMETHODCALLTYPE *)(IFileOperation *, IPropertyChangeArray *)>(8)(self, p);
    trace("properties-count", count);
    NATIVE_GUARD;
    return function<HRESULT(STDMETHODCALLTYPE *)(IFileOperation *, IPropertyChangeArray *)>(8)(self, p);
}
HRESULT STDMETHODCALLTYPE applyOne(IFileOperation *self, IShellItem *p) {
    NATIVE_GUARD;
    return function<HRESULT(STDMETHODCALLTYPE *)(IFileOperation *, IShellItem *)>(10)(self, p);
}
HRESULT STDMETHODCALLTYPE applyMany(IFileOperation *self, IUnknown *p) {
    NATIVE_GUARD;
    return function<HRESULT(STDMETHODCALLTYPE *)(IFileOperation *, IUnknown *)>(11)(self, p);
}
HRESULT STDMETHODCALLTYPE renameOne(IFileOperation *self, IShellItem *p, LPCWSTR n,
                                    IFileOperationProgressSink *sink) {
    NATIVE_GUARD;
    return function<HRESULT(STDMETHODCALLTYPE *)(IFileOperation *, IShellItem *, LPCWSTR,
                                                 IFileOperationProgressSink *)>(12)(self, p, n, sink);
}
HRESULT STDMETHODCALLTYPE renameMany(IFileOperation *self, IUnknown *p, LPCWSTR n) {
    NATIVE_GUARD;
    return function<HRESULT(STDMETHODCALLTYPE *)(IFileOperation *, IUnknown *, LPCWSTR)>(13)(self, p, n);
}
HRESULT STDMETHODCALLTYPE operationFlags(IFileOperation *self, DWORD flags) {
    auto result = function<HRESULT(STDMETHODCALLTYPE *)(IFileOperation *, DWORD)>(5)(self, flags);
    try {
        if (SUCCEEDED(result) && !replaying) {
            auto owner = stateFor(self);
            if (owner) {
                std::lock_guard lock(owner->mutex);
                owner->flags = flags;
            }
        }
    } catch (...) {
        interceptionFault = true;
    }
    return result;
}
HRESULT STDMETHODCALLTYPE deleteOne(IFileOperation *self, IShellItem *p, IFileOperationProgressSink *sink) {
    using Fn = HRESULT(STDMETHODCALLTYPE *)(IFileOperation *, IShellItem *, IFileOperationProgressSink *);
    if (replaying)
        return function<Fn>(18)(self, p, sink);
    try {
        Pending pending;
        pending.remove = true;
        auto path = pathOf(p);
        if (!path.empty())
            pending.sources.push_back(path);
        Ref<IShellItem> item(p);
        Ref<IFileOperationProgressSink> progress(sink);
        pending.replay = [=] { return function<Fn>(18)(self, item.p, progress.p); };
        return queue(self, std::move(pending), true);
    } catch (...) {
        auto hr = nativeOnly(self);
        return FAILED(hr) ? hr : function<Fn>(18)(self, p, sink);
    }
}
HRESULT STDMETHODCALLTYPE deleteMany(IFileOperation *self, IUnknown *p) {
    using Fn = HRESULT(STDMETHODCALLTYPE *)(IFileOperation *, IUnknown *);
    if (replaying)
        return function<Fn>(19)(self, p);
    Ref<IUnknown> replaySource(p);
    try {
        Pending pending;
        pending.remove = true;
        const bool supported = itemPaths(p, pending.sources, replaySource);
        Ref<IUnknown> items(replaySource);
        pending.replay = [=] { return function<Fn>(19)(self, items.p); };
        return queue(self, std::move(pending), supported);
    } catch (...) {
        auto hr = nativeOnly(self);
        return FAILED(hr) ? hr : function<Fn>(19)(self, replaySource.p);
    }
}
HRESULT STDMETHODCALLTYPE newItem(IFileOperation *self, IShellItem *p, DWORD attr, LPCWSTR n, LPCWSTR t,
                                  IFileOperationProgressSink *sink) {
    NATIVE_GUARD;
    return function<HRESULT(STDMETHODCALLTYPE *)(IFileOperation *, IShellItem *, DWORD, LPCWSTR, LPCWSTR,
                                                 IFileOperationProgressSink *)>(20)(self, p, attr, n, t,
                                                                                    sink);
}
#undef NATIVE_GUARD
bool initialize(bool test) noexcept {
    try {
        static std::mutex initializeMutex;
        std::lock_guard lock(initializeMutex);
        if (ready)
            return true;
        wchar_t process[32768]{};
        GetModuleFileNameW(nullptr, process, 32768);
        const wchar_t *leaf = wcsrchr(process, L'\\');
        if (!test && (!leaf || _wcsicmp(leaf + 1, L"explorer.exe") != 0))
            return false;
        const auto markerName =
            L"Local\\StrataCopy.NativeCopy.Installed.5." + std::to_wstring(GetCurrentProcessId());
        strata::UniqueHandle marker(CreateMutexW(nullptr, FALSE, markerName.c_str()));
        if (!marker || GetLastError() == ERROR_ALREADY_EXISTS)
            return false;
        IFileOperation *operation = nullptr;
        if (FAILED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&operation))))
            return false;
        void **table = *reinterpret_cast<void ***>(operation);
        void *targets[23]{};
        for (int i = 0; i < 23; ++i)
            targets[i] = table[i];
        operation->Release();
        struct Hook {
            int index;
            void *detour;
        };
        Hook hooks[] = {
            {2, reinterpret_cast<void *>(release)},     {5, reinterpret_cast<void *>(operationFlags)},
            {8, reinterpret_cast<void *>(properties)},  {10, reinterpret_cast<void *>(applyOne)},
            {11, reinterpret_cast<void *>(applyMany)},  {12, reinterpret_cast<void *>(renameOne)},
            {13, reinterpret_cast<void *>(renameMany)}, {14, reinterpret_cast<void *>(moveOne)},
            {15, reinterpret_cast<void *>(moveMany)},   {16, reinterpret_cast<void *>(copyOne)},
            {17, reinterpret_cast<void *>(copyMany)},   {18, reinterpret_cast<void *>(deleteOne)},
            {19, reinterpret_cast<void *>(deleteMany)}, {20, reinterpret_cast<void *>(newItem)},
            {21, reinterpret_cast<void *>(perform)}};
        auto status = MH_Initialize();
        if (status != MH_OK)
            return false;
        for (auto hook : hooks) {
            if (MH_CreateHook(targets[hook.index], hook.detour, &original[hook.index]) != MH_OK) {
                MH_Uninitialize();
                return false;
            }
            HMODULE targetModule = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                                    reinterpret_cast<LPCWSTR>(targets[hook.index]), &targetModule)) {
                MH_Uninitialize();
                return false;
            }
        }
        // Keep the module loaded while trampolines are reachable. Disabling the helper
        // resets the event, so all calls then pass through; Explorer unloads it on exit.
        HMODULE pinned = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                                reinterpret_cast<LPCWSTR>(initialize), &pinned) ||
            MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
            MH_Uninitialize();
            return false;
        }
        ready = true;
        trace("initialized", 1);
        installedMarker = std::move(marker);
        return true;
    } catch (...) {
        interceptionFault = true;
        return false;
    }
}
} // namespace
extern "C" __declspec(dllexport) LRESULT CALLBACK StrataCopyMessageHook(int code, WPARAM w, LPARAM l) {
    static thread_local bool initializing = false;
    if (code >= 0 && !ready && !initializing) {
        initializing = true;
        auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE)
            initialize(false);
        if (SUCCEEDED(hr))
            CoUninitialize();
        initializing = false;
    }
    return CallNextHookEx(nullptr, code, w, l);
}
// Test entry points affect only the process that explicitly loads this DLL.
extern "C" __declspec(dllexport) BOOL WINAPI StrataCopyTestInitialize(TestReceiver receiver) {
    testReceiver = receiver;
    return initialize(true);
}
extern "C" __declspec(dllexport) size_t WINAPI StrataCopyPendingObjects() {
    std::lock_guard lock(stateMutex);
    return states.size();
}
extern "C" __declspec(dllexport) DWORD WINAPI StrataCopyLastLaunchPid() {
    return lastLaunchPid;
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH)
        moduleHandle = instance;
    return TRUE;
}
