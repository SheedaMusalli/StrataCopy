#include "Platform/Win32WindowEffects.hpp"
#include "UI/ViewModels/MainCoordinatorViewModel.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
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
    QGuiApplication app(argc, argv);
    app.setApplicationName("StrataCopy");
    app.setOrganizationName("StrataCopy");
    app.setApplicationVersion("1.0.0");
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
                    window->showNormal();
                    window->raise();
                    window->requestActivate();
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
