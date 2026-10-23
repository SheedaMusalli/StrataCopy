#include "Win32WindowEffects.hpp"
#include <QQuickWindow>
#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QPointer>
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>
namespace strata {
namespace {
class WindowChromeFilter final : public QObject, public QAbstractNativeEventFilter {
    QPointer<QQuickWindow> window;
    HWND handle;

  public:
    explicit WindowChromeFilter(QQuickWindow *owner)
        : QObject(owner), window(owner), handle(reinterpret_cast<HWND>(owner->winId())) {}
    ~WindowChromeFilter() override {
        if (QCoreApplication::instance())
            QCoreApplication::instance()->removeNativeEventFilter(this);
    }
    bool nativeEventFilter(const QByteArray &, void *message, qintptr *result) override {
        const auto *event = static_cast<const MSG *>(message);
        if (!window || event->hwnd != handle || event->message != WM_NCHITTEST)
            return false;
        POINT point{GET_X_LPARAM(event->lParam), GET_Y_LPARAM(event->lParam)};
        if (!ScreenToClient(handle, &point))
            return false;
        RECT client{};
        GetClientRect(handle, &client);
        const auto scale = window->devicePixelRatio();
        // Keep all header buttons interactive. Native caption hit testing works
        // even while a modal QML popup owns the pointer grab.
        if (point.x >= 8 * scale && point.x < client.right - 140 * scale && point.y >= 8 * scale &&
            point.y < 52 * scale) {
            *result = HTCAPTION;
            return true;
        }
        return false;
    }
};
} // namespace
void Win32WindowEffects::apply(QQuickWindow *window) {
    auto *chrome = new WindowChromeFilter(window); // Owned by the window; never process-global.
    QCoreApplication::instance()->installNativeEventFilter(chrome);
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    int corner = 2;
    DwmSetWindowAttribute(hwnd, 33, &corner, sizeof(corner));
    int backdrop = 2;
    DwmSetWindowAttribute(hwnd, 38, &backdrop, sizeof(backdrop));
    const MARGINS margins{-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}
} // namespace strata
