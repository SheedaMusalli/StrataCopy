#include "Platform/Win32WindowEffects.hpp"
#include "Platform/DriveHardwareDetector.hpp"
#include "UI/ViewModels/MainCoordinatorViewModel.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScreen>
#include <QPropertyAnimation>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQuickItem>
#include <QStandardPaths>
#include <QTimer>
#include <cstdio>
#include <windows.h>
#include <shellapi.h>
#include <psapi.h>
#include <QIcon>
#include <QFontDatabase>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>
namespace {
void logMessage(QtMsgType type, const QMessageLogContext &, const QString &message) {
    const auto text = (QDateTime::currentDateTime().toString(Qt::ISODate) + " " + message + '\n').toUtf8();
    if (auto *stream =
            std::fopen(qEnvironmentVariable("STRATACOPY_DIAGNOSTICS", QDir::tempPath() + "/stratacopy-ui.log")
                           .toLocal8Bit()
                           .constData(),
                       "ab")) {
        std::fwrite(text.constData(), 1, size_t(text.size()), stream);
        std::fclose(stream);
    }
    if (type == QtFatalMsg)
        std::abort();
}
} // namespace
int main(int argc, char **argv) {
    int nativeCount = 0;
    auto **nativeArguments = CommandLineToArgvW(GetCommandLineW(), &nativeCount);
    if (nativeArguments) {
        for (int i = 1; i + 1 < nativeCount; ++i)
            if (wcscmp(nativeArguments[i], L"--data-directory") == 0)
                qputenv("STRATACOPY_DATA_DIR", QString::fromWCharArray(nativeArguments[++i]).toUtf8());
        LocalFree(nativeArguments);
    }
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == "--detect-hardware") {
            QCoreApplication detectionApp(argc, argv);
            detectionApp.setApplicationName("StrataCopy");
            detectionApp.setOrganizationName("StrataCopy");
            const auto directory = qEnvironmentVariable("STRATACOPY_DATA_DIR",
                QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
            return strata::DriveHardwareDetector::saveInventory(directory) ? 0 : 1;
        }
    }
    QApplication app(argc, argv);
    app.setApplicationName("StrataCopy");
    app.setOrganizationName("StrataCopy");
    app.setApplicationVersion("1.1.0");
    app.setWindowIcon(QIcon(":/resources/icons/stratacopy.svg"));
    qInstallMessageHandler(logMessage);
#ifdef STRATACOPY_UI_TEST_HOOKS
    if (QGuiApplication::platformName() == "offscreen") {
        const auto fonts = qEnvironmentVariable("WINDIR") + "/Fonts/";
        for (const auto &font : QStringList{"segoeui.ttf", "seguisb.ttf", "consola.ttf"})
            QFontDatabase::addApplicationFont(fonts + font);
        app.setFont(QFont("Segoe UI"));
    }
#endif
    QQuickStyle::setStyle("Basic");
    QRhiD3D12InitParams d3d12;
    QRhiD3D11InitParams d3d11;
    if (qEnvironmentVariable("STRATACOPY_RENDERER") == "software") {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
        qInfo("Renderer selected: software");
    } else if (qEnvironmentVariable("STRATACOPY_RENDERER") != "d3d11" && QRhi::probe(QRhi::D3D12, &d3d12)) {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D12);
        qInfo("Renderer selected: Direct3D 12");
    } else if (QRhi::probe(QRhi::D3D11, &d3d11)) {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
        qInfo("Renderer selected: Direct3D 11");
    } else {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
        qInfo("Renderer selected: software");
    }
    const auto args = app.arguments();
    QStringList paths;
    QString pasteRequest;
    for (int i = 1; i < args.size(); ++i)
        if (args[i] == "--add" && i + 1 < args.size())
            paths << args[++i];
        else if (args[i] == "--paste-request" && i + 1 < args.size())
            pasteRequest = args[++i];
    const auto identity =
        QCryptographicHash::hash(qgetenv("USERNAME") + qgetenv("USERDOMAIN"), QCryptographicHash::Sha256)
            .toHex()
            .left(16);
    auto serverName = "StrataCopy-" + QString::fromLatin1(identity);
    // Explicit data directories are independent instances, including isolated QA.
    // A native request must never be forwarded into another installation's journal.
    serverName +=
        "-" + QString::fromLatin1(QCryptographicHash::hash(
                                      QDir(qEnvironmentVariable("STRATACOPY_DATA_DIR",
                                                                QStandardPaths::writableLocation(
                                                                    QStandardPaths::AppLocalDataLocation)))
                                          .absolutePath()
                                          .toCaseFolded()
                                          .toUtf8(),
                                      QCryptographicHash::Sha256)
                                      .toHex()
                                      .left(16));
#ifdef STRATACOPY_UI_TEST_HOOKS
    if (QGuiApplication::platformName() == "offscreen")
        serverName += "-offscreen-qa";
#endif
    QLocalSocket existing;
    existing.connectToServer(serverName);
    if (existing.waitForConnected(350)) {
        // Explorer starts this short-lived client in response to a user action. Pass
        // its foreground permission to the already-running StrataCopy instance.
        ULONG serverProcessId = 0;
        if (GetNamedPipeServerProcessId(reinterpret_cast<HANDLE>(existing.socketDescriptor()), &serverProcessId))
            AllowSetForegroundWindow(serverProcessId);
        existing.write((pasteRequest.isEmpty() ? QJsonDocument(QJsonArray::fromStringList(paths))
                                               : QJsonDocument(QJsonObject{{"pasteRequest", pasteRequest}}))
                           .toJson(QJsonDocument::Compact));
        existing.waitForBytesWritten(1000);
        existing.disconnectFromServer();
        return 0;
    }
    QLockFile instance(QDir::tempPath() + "/" + serverName + ".lock");
    if (!instance.tryLock(1000))
        return 2;
    QLocalServer::removeServer(serverName);
    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server.listen(serverName))
        return 3;
    qmlRegisterUncreatableType<strata::FileTransferListModel>(
        "StrataCopy.Native", 1, 0, "FileTransferListModel", "Provided by the coordinator");
    qmlRegisterUncreatableType<strata::TelemetryHistoryModel>(
        "StrataCopy.Native", 1, 0, "TelemetryHistoryModel", "Provided by the coordinator");
    strata::MainCoordinatorViewModel coordinator;
    coordinator.addPaths(paths);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appState", &coordinator);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.loadFromModule("StrataCopy", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
        return 1;
    if (QGuiApplication::platformName() != "offscreen")
        strata::Win32WindowEffects::apply(window);
    // Stack ownership keeps the menu, actions and native tray icon bounded to the application lifetime.
    QMenu trayMenu;
    auto *openAction = trayMenu.addAction("Open StrataCopy");
    auto *settingsAction = trayMenu.addAction("Settings…");
    trayMenu.addSeparator();
    auto *transferOnlyAction = trayMenu.addAction("Show window only during transfers");
    transferOnlyAction->setCheckable(true);
    transferOnlyAction->setChecked(window->property("transferOnly").toBool());
    trayMenu.addSeparator();
    auto *exitAction = trayMenu.addAction("Exit");
    QSystemTrayIcon tray(QIcon(":/resources/icons/stratacopy.svg"));
    tray.setToolTip("StrataCopy");
    tray.setContextMenu(&trayMenu);
    const bool trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    if (trayAvailable) tray.show();
    else window->setProperty("transferOnly", false);
    app.setQuitOnLastWindowClosed(!trayAvailable || !window->property("transferOnly").toBool());
    const auto reveal = [window] {window->showNormal();window->raise();window->requestActivate();};
    QObject::connect(openAction, &QAction::triggered, &app, reveal);
    QObject::connect(settingsAction, &QAction::triggered, &app, [window, reveal] {
        reveal();
        QMetaObject::invokeMethod(window, "setUiTestState", Q_ARG(QVariant, "parameters"));
    });
    QObject::connect(&tray, &QSystemTrayIcon::activated, &app, [reveal](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) reveal();
    });
    QObject::connect(transferOnlyAction, &QAction::toggled, &app, [&](bool enabled) {
        window->setProperty("transferOnly", enabled && trayAvailable);
        app.setQuitOnLastWindowClosed(!enabled || !trayAvailable);
        if (enabled && trayAvailable && !coordinator.busy()) window->hide();
        else reveal();
    });
    QObject::connect(exitAction, &QAction::triggered, &app, &QCoreApplication::quit);
    bool previouslyBusy = coordinator.busy();
    QObject::connect(&coordinator, &strata::MainCoordinatorViewModel::stateChanged, &app, [&] {
        exitAction->setEnabled(!coordinator.busy());
        if (coordinator.busy() && !previouslyBusy) reveal();
        previouslyBusy = coordinator.busy();
    });

    // Reuse a single non-activating toast. Its widgets, timer and animation have bounded ownership.
    QWidget toast(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                               Qt::WindowDoesNotAcceptFocus);
    toast.setWindowTitle("StrataCopy — Transfer complete");
    toast.setAttribute(Qt::WA_ShowWithoutActivating);
    toast.setObjectName("completionToast");
    toast.setStyleSheet("QWidget#completionToast {background:#19232d;border:1px solid #3b5965;border-radius:9px;}"
                        "QLabel {color:#e3eff3;background:transparent;border:none;}"
                        "QPushButton {color:#8da5b3;background:transparent;border:none;font-size:18px;}");
    toast.setFixedSize(340, 96);
    QHBoxLayout toastLayout(&toast);
    toastLayout.setContentsMargins(16, 14, 12, 14);
    QLabel toastIcon;
    toastIcon.setPixmap(app.windowIcon().pixmap(34, 34));
    toastLayout.addWidget(&toastIcon);
    QVBoxLayout toastText;
    QLabel toastTitle("Transfer complete");
    toastTitle.setStyleSheet("font-size:14px;font-weight:600;color:#c2f2df;");
    QLabel toastDuration;
    toastDuration.setStyleSheet("font-size:12px;color:#a9bdcc;");
    toastText.addWidget(&toastTitle);
    toastText.addWidget(&toastDuration);
    toastLayout.addLayout(&toastText, 1);
    QPushButton dismissToast(QString::fromUtf8("×"));
    dismissToast.setFixedSize(22, 24);
    dismissToast.setFocusPolicy(Qt::NoFocus);
    toastLayout.addWidget(&dismissToast, 0, Qt::AlignTop);
    QTimer toastTimer;
    toastTimer.setSingleShot(true);
    QPropertyAnimation toastSlide(&toast, "pos");
    toastSlide.setDuration(180);
    toastSlide.setEasingCurve(QEasingCurve::OutCubic);
    bool toastClosing = false;
    QPoint toastExitPosition;
    const auto finishToast = [&] {
        toastTimer.stop();
        toastSlide.stop();
        toast.hide();
        const bool transferOnly = window->property("transferOnly").toBool();
        app.setQuitOnLastWindowClosed(!transferOnly);
        if (!transferOnly && !window->isVisible() && !coordinator.busy()) app.quit();
    };
    const auto slideToastDown = [&] {
        if (!toast.isVisible() || toastClosing) return;
        toastTimer.stop();
        if (!window->property("notificationAnimations").toBool()) {finishToast();return;}
        toastClosing = true;
        toastSlide.stop();
        toastSlide.setDuration(230);
        toastSlide.setEasingCurve(QEasingCurve::InCubic);
        toastSlide.setStartValue(toast.pos());
        toastSlide.setEndValue(toastExitPosition);
        toastSlide.start();
    };
    QObject::connect(&toastSlide, &QPropertyAnimation::finished, &toast, [&] {if(toastClosing) finishToast();});
    QObject::connect(&toastTimer, &QTimer::timeout, &toast, slideToastDown);
    QObject::connect(&dismissToast, &QPushButton::clicked, &toast, slideToastDown);
    QObject::connect(&coordinator, &strata::MainCoordinatorViewModel::transferSucceeded, &toast,
                     [&](qint64 milliseconds) {
        if (!window->property("completionNotifications").toBool()) return;
        const qint64 seconds = milliseconds / 1000;
        const QString duration = milliseconds < 1000 ? "less than a second" : seconds < 60
            ? QString("%1 second%2").arg(seconds).arg(seconds == 1 ? "" : "s")
            : seconds < 3600 ? QString("%1m %2s").arg(seconds / 60).arg(seconds % 60)
                             : QString("%1h %2m %3s").arg(seconds / 3600).arg((seconds / 60) % 60).arg(seconds % 60);
        toastTitle.setText(coordinator.deleting()?"Deletion complete":"Transfer complete");
        toastDuration.setText("Completed in " + duration);
        const auto *screen = window->screen() ? window->screen() : app.primaryScreen();
        if (!screen) return;
        const auto area = screen->availableGeometry();
        const QPoint destination(area.right() - toast.width() - 16, area.bottom() - toast.height() - 16);
        toastSlide.stop();
        toastClosing = false;
        toastExitPosition = QPoint(destination.x(), screen->geometry().bottom() + 1);
        const bool animate = window->property("notificationAnimations").toBool();
        toast.move(animate ? toastExitPosition : destination);
        // Allow the main copy window to close while the notification remains on screen.
        app.setQuitOnLastWindowClosed(false);
        toast.show();
        if (animate) {
            toastSlide.setDuration(260);
            toastSlide.setEasingCurve(QEasingCurve::OutCubic);
            toastSlide.setStartValue(toast.pos());
            toastSlide.setEndValue(destination);
            toastSlide.start();
        }
        toastTimer.start(5500);
    });

    if (!pasteRequest.isEmpty())
        QTimer::singleShot(0, &coordinator, [&coordinator, pasteRequest] {
            if (QFileInfo::exists(pasteRequest))
                coordinator.importExplorerRequest(pasteRequest);
        });
#ifdef STRATACOPY_UI_TEST_HOOKS
    const auto testPanel = qEnvironmentVariable("STRATACOPY_TEST_PANEL");
    if (!testPanel.isEmpty())
        QTimer::singleShot(500, window, [window, testPanel] {
            QMetaObject::invokeMethod(window, "setUiTestState", Q_ARG(QVariant, testPanel));
        });
    const int testCycles = qEnvironmentVariableIntValue("STRATACOPY_UI_CYCLES");
    if (testCycles > 20) {
        auto *cycleTimer = new QTimer(&app);
        cycleTimer->setInterval(180);
        QObject::connect(
            cycleTimer, &QTimer::timeout, &app,
            [cycleTimer, window, testCycles, step = 0, baselineHandles = DWORD(0),
             baselineBytes = SIZE_T(0)]() mutable {
                const QStringList modes{"parameters", "workbench", "compact",
                                        "workbench",  "history",   "workbench"};
                QMetaObject::invokeMethod(window, "setUiTestState",
                                          Q_ARG(QVariant, modes[step % modes.size()]));
                DWORD count = 0;
                GetProcessHandleCount(GetCurrentProcess(), &count);
                PROCESS_MEMORY_COUNTERS_EX mem{};
                mem.cb = sizeof(mem);
                GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&mem),
                                     sizeof(mem));
                if (step == 20) {
                    baselineHandles = count;
                    baselineBytes = mem.PrivateUsage;
                }
                if (step > 20 && step % 100 == 0)
                    qInfo("UI resource checkpoint %d: handles %lu; private bytes %llu", step, count,
                          static_cast<unsigned long long>(mem.PrivateUsage));
                if (++step >= testCycles) {
                    cycleTimer->stop();
                    qInfo("UI resource cycles %d: handles %lu -> %lu; private bytes %llu -> %llu", step,
                          baselineHandles, count, static_cast<unsigned long long>(baselineBytes),
                          static_cast<unsigned long long>(mem.PrivateUsage));
                    QCoreApplication::exit(
                        (count > baselineHandles + 12 || mem.PrivateUsage > baselineBytes + 32 * 1024 * 1024)
                            ? 4
                            : 0);
                }
            });
        cycleTimer->start();
    }
#endif
    QObject::connect(&server, &QLocalServer::newConnection, &app, [&] {
        while (auto *socket = server.nextPendingConnection()) {
            socket->setReadBufferSize(1024 * 1024 + 1);
            auto consume = [socket, &coordinator, window] {
                auto bytes = socket->property("payload").toByteArray() + socket->readAll();
                if (bytes.size() > 1024 * 1024) {
                    socket->abort();
                    socket->deleteLater();
                    return;
                }
                socket->setProperty("payload", bytes);
                QJsonParseError error;
                const auto doc = QJsonDocument::fromJson(bytes, &error);
                if (error.error == QJsonParseError::NoError && (doc.isArray() || doc.isObject())) {
                    QStringList paths;
                    for (const auto &v : doc.array())
                        paths << v.toString();
                    coordinator.addPaths(paths);
                    if (doc.isObject()) {
                        const auto request = doc.object().value("pasteRequest").toString();
                        if (QFileInfo::exists(request))
                            coordinator.importExplorerRequest(request);
                    }
                    const bool explorerRequest = doc.isObject() && !doc.object().value("pasteRequest").toString().isEmpty();
                    if (explorerRequest || coordinator.busy() || !window->property("transferOnly").toBool()) {
                        window->showNormal();
                        window->raise();
                        window->requestActivate();
                        SetForegroundWindow(reinterpret_cast<HWND>(window->winId()));
                        BringWindowToTop(reinterpret_cast<HWND>(window->winId()));
                    }
                    socket->disconnectFromServer();
                }
            };
            QObject::connect(socket, &QLocalSocket::readyRead, &app, consume);
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QTimer::singleShot(5000, socket, [socket] {
                socket->abort();
                socket->deleteLater();
            });
            consume();
        }
    });
#ifdef STRATACOPY_UI_TEST_HOOKS
    const auto screenshot = qEnvironmentVariable("STRATACOPY_SCREENSHOT");
    if (!screenshot.isEmpty())
        QTimer::singleShot(2500, window, [window, screenshot] {
            qInfo("UI snapshot: window %d x %d", window->width(), window->height());
            for (const auto &name : QStringList{"windowLayout", "copyView", "transferLayout", "sourcePanel"})
                if (auto *item = window->findChild<QQuickItem *>(name))
                    qInfo("%s: x=%.0f y=%.0f width=%.0f height=%.0f", qPrintable(name), item->x(), item->y(),
                          item->width(), item->height());
            window->grabWindow().save(screenshot);
        });
    bool ok = false;
    const int smokeMs = qEnvironmentVariableIntValue("STRATACOPY_SMOKE_MS", &ok);
    if (ok && smokeMs > 0)
        QTimer::singleShot(smokeMs, &app, &QCoreApplication::quit);
#endif
    return app.exec();
}
