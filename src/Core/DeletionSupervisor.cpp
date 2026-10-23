#include "DeletionSupervisor.hpp"
#include <QDir>
#include <QFileInfo>
#include <QScopeGuard>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <sherrors.h>
#include <atomic>
namespace strata {
namespace {
struct Sink final : IFileOperationProgressSink {
    std::atomic<ULONG> references{1};
    DeletionSupervisor *owner;
    QString current;
    quint64 deleted = 0;
    HRESULT failure = S_OK;
    explicit Sink(DeletionSupervisor *o) : owner(o) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) override {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (id == IID_IUnknown || id == IID_IFileOperationProgressSink) {
            *out = static_cast<IFileOperationProgressSink *>(this);
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
    HRESULT check() {
        return owner->cancelled() ? HRESULT_FROM_WIN32(ERROR_CANCELLED) : S_OK;
    }
    HRESULT STDMETHODCALLTYPE StartOperations() override {
        return check();
    }
    HRESULT STDMETHODCALLTYPE FinishOperations(HRESULT hr) override {
        if (FAILED(hr))
            failure = hr;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PreDeleteItem(DWORD, IShellItem *item) override {
        PWSTR path = nullptr;
        if (item && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
            current = QString::fromWCharArray(path);
            CoTaskMemFree(path);
        }
        return check();
    }
    HRESULT STDMETHODCALLTYPE PostDeleteItem(DWORD, IShellItem *, HRESULT hr, IShellItem *) override {
        if (FAILED(hr))
            failure = hr;
        else
            ++deleted;
        return check();
    }
    HRESULT STDMETHODCALLTYPE UpdateProgress(UINT total, UINT done) override {
        owner->report(done, total, current);
        return check();
    }
    HRESULT STDMETHODCALLTYPE PreRenameItem(DWORD, IShellItem *, LPCWSTR) override {
        return check();
    }
    HRESULT STDMETHODCALLTYPE PostRenameItem(DWORD, IShellItem *, LPCWSTR, HRESULT, IShellItem *) override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PreMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) override {
        return check();
    }
    HRESULT STDMETHODCALLTYPE PostMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT,
                                           IShellItem *) override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PreCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) override {
        return check();
    }
    HRESULT STDMETHODCALLTYPE PostCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT,
                                           IShellItem *) override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PreNewItem(DWORD, IShellItem *, LPCWSTR) override {
        return check();
    }
    HRESULT STDMETHODCALLTYPE PostNewItem(DWORD, IShellItem *, LPCWSTR, LPCWSTR, DWORD, HRESULT,
                                          IShellItem *) override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ResetTimer() override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PauseTimer() override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ResumeTimer() override {
        return S_OK;
    }
};
} // namespace
void DeletionSupervisor::report(quint64 done, quint64 total, const QString &path) {
    if (m_progressPending.exchange(true))
        return;
    QMetaObject::invokeMethod(
        this,
        [this, done, total, path] {
            m_progressPending = false;
            emit progress(done, total, path);
        },
        Qt::QueuedConnection);
}
bool DeletionSupervisor::start(const QStringList &paths, bool permanent) {
    if (paths.isEmpty() || paths.size() > 4096 || m_running.exchange(true))
        return false;
    for (const auto &path : paths) {
        auto normalized = QDir::fromNativeSeparators(path);
        if (normalized.startsWith("//?/UNC/", Qt::CaseInsensitive))
            normalized = "//" + normalized.mid(8);
        else if (normalized.startsWith("//?/"))
            normalized = normalized.mid(4);
        if (normalized.startsWith("//") && normalized.split('/', Qt::SkipEmptyParts).size() <= 2) {
            m_running = false;
            return false;
        }
        const QFileInfo info(path);
        if (!info.isAbsolute() || QDir(path).isRoot() ||
            GetFileAttributesW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16())) ==
                INVALID_FILE_ATTRIBUTES) {
            m_running = false;
            return false;
        }
    }
    if (m_worker.joinable())
        m_worker.join();
    m_cancel = false;
    try {
        m_worker = std::thread([this, paths, permanent] {
            try {
                HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                const bool initialized = SUCCEEDED(result);
                IFileOperation *operation = nullptr;
                Sink *sink = nullptr;
                DWORD cookie = 0;
                bool advised = false;
                const auto cleanup = qScopeGuard([&] {
                    if (operation) {
                        if (advised)
                            operation->Unadvise(cookie);
                        operation->Release();
                    }
                    if (sink)
                        sink->Release();
                    if (initialized)
                        CoUninitialize();
                });
                sink = new Sink(this);
                if (initialized)
                    result = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_INPROC_SERVER,
                                              IID_PPV_ARGS(&operation));
                if (SUCCEEDED(result))
                    result = operation->SetOperationFlags(
                        FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOFX_EARLYFAILURE |
                        (permanent ? 0 : FOFX_RECYCLEONDELETE | FOFX_ADDUNDORECORD | FOF_WANTNUKEWARNING));
                if (SUCCEEDED(result)) {
                    result = operation->Advise(sink, &cookie);
                    advised = SUCCEEDED(result);
                }
                for (const auto &path : paths) {
                    if (FAILED(result) || m_cancel)
                        break;
                    IShellItem *item = nullptr;
                    const auto native = QDir::toNativeSeparators(path);
                    result = SHCreateItemFromParsingName(reinterpret_cast<LPCWSTR>(native.utf16()), nullptr,
                                                         IID_PPV_ARGS(&item));
                    if (SUCCEEDED(result)) {
                        result = operation->DeleteItem(item, nullptr);
                        item->Release();
                    }
                }
                if (SUCCEEDED(result) && !m_cancel)
                    result = operation->PerformOperations();
                BOOL aborted = FALSE;
                if (operation)
                    operation->GetAnyOperationsAborted(&aborted);
                if (SUCCEEDED(result) && FAILED(sink->failure))
                    result = sink->failure;
                const bool cancelled = m_cancel || result == HRESULT_FROM_WIN32(ERROR_CANCELLED) ||
                                       result == COPYENGINE_E_USER_CANCELLED ||
                                       (aborted && SUCCEEDED(result));
                const bool success = SUCCEEDED(result) && !cancelled;
                const auto message =
                    success     ? QString(permanent ? "Deletion complete. %1 items removed."
                                                    : "Recycle operation complete. %1 items processed.")
                                      .arg(sink->deleted)
                    : cancelled ? QString("Deletion cancelled. Items already removed are not rolled back.")
                                : QString("Deletion stopped with Windows error 0x%1. Some items may remain.")
                                      .arg(quint32(result), 8, 16, QChar('0'));
                m_running = false;
                QMetaObject::invokeMethod(
                    this, [this, success, cancelled, message] { emit finished(success, cancelled, message); },
                    Qt::QueuedConnection);
            } catch (...) {
                m_running = false;
                QMetaObject::invokeMethod(
                    this,
                    [this] {
                        emit finished(false, m_cancel,
                                      "Deletion stopped because a system resource could not be allocated.");
                    },
                    Qt::QueuedConnection);
            }
        });
    } catch (...) {
        m_running = false;
        return false;
    }
    return true;
}
} // namespace strata
