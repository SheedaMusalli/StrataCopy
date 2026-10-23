#include "ShellContextMenuRegistrar.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QProcess>
#include <QFileInfo>
namespace strata {
bool ShellContextMenuRegistrar::keyboardPasteEnabled() {
    QSettings startup("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      QSettings::NativeFormat);
    auto command = startup.value("StrataCopyExplorer").toString().trimmed();
    if (command.startsWith('"') && command.endsWith('"'))
        command = command.mid(1, command.size() - 2);
    return QDir::cleanPath(QDir::fromNativeSeparators(command)).compare(
               QDir::cleanPath(QCoreApplication::applicationDirPath() + "/StrataCopyExplorer.exe"),
               Qt::CaseInsensitive) == 0;
}
QString ShellContextMenuRegistrar::setEnabled(bool enabled) {
    // Per-user classic Explorer verbs. Windows 11 exposes these under Show more options.
    const QStringList roots{"HKEY_CURRENT_USER\\Software\\Classes\\*\\shell\\StrataCopy",
                            "HKEY_CURRENT_USER\\Software\\Classes\\Directory\\shell\\StrataCopy"};
    for (const auto &root : roots) {
        QSettings settings(root, QSettings::NativeFormat);
        if (enabled) {
            const auto exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
            settings.setValue(".", "Copy with StrataCopy");
            settings.setValue("Icon", exe);
            settings.setValue("MultiSelectModel", "Player");
            settings.setValue("command/.", "\"" + exe + "\" --add \"%1\"");
        } else
            settings.remove("");
        settings.sync();
        if (settings.status() != QSettings::NoError)
            return "Unable to change the Explorer context menu.";
    }
    for (const auto &root :
         QStringList{"HKEY_CURRENT_USER\\Software\\Classes\\Directory\\Background\\shell\\StrataCopyPaste",
                     "HKEY_CURRENT_USER\\Software\\Classes\\Directory\\shell\\StrataCopyPaste"}) {
        QSettings settings(root, QSettings::NativeFormat);
        if (enabled) {
            const auto helper =
                QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/StrataCopyExplorer.exe");
            settings.setValue(".", "Paste with StrataCopy");
            settings.setValue("Icon", QCoreApplication::applicationFilePath());
            settings.setValue("command/.", "\"" + helper + "\" --paste \"" +
                                               (root.contains("Background") ? "%V" : "%1") + "\"");
        } else
            settings.remove("");
        settings.sync();
        if (settings.status() != QSettings::NoError)
            return "Unable to change the Explorer paste menu.";
    }
    return {};
}
QString ShellContextMenuRegistrar::setKeyboardPasteEnabled(bool enabled) {
    const auto helper = QCoreApplication::applicationDirPath() + "/StrataCopyExplorer.exe";
    if (!QFileInfo::exists(helper))
        return "The Explorer helper is missing. Reinstall StrataCopy.";
    if (enabled &&
        !QFileInfo::exists(QCoreApplication::applicationDirPath() + "/" + STRATACOPY_NATIVE_DLL_NAME))
        return "The native Explorer integration DLL is missing. Reinstall StrataCopy.";
    QSettings startup("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      QSettings::NativeFormat);
    if (enabled) {
        startup.setValue("StrataCopyExplorer", "\"" + QDir::toNativeSeparators(helper) + "\"");
        startup.sync();
        if (startup.status() != QSettings::NoError)
            return "Unable to save Explorer paste startup settings.";
        if (!QProcess::startDetached(helper, {})) {
            startup.remove("StrataCopyExplorer");
            return "Unable to start the Explorer helper.";
        }
    } else {
        startup.remove("StrataCopyExplorer");
        startup.sync();
        if (startup.status() != QSettings::NoError)
            return "Unable to remove Explorer paste startup settings.";
        QProcess stop;
        stop.start(helper, {"--stop"});
        if (!stop.waitForFinished(3000)) {
            stop.kill();
            stop.waitForFinished();
            return "The helper stop request did not finish.";
        }
        if (stop.exitStatus() != QProcess::NormalExit || stop.exitCode() != 0)
            return "Windows copier was selected, but the Explorer helper did not stop cleanly.";
    }
    // Keep an installed copy's next upgrade from restoring an obsolete task choice.
    // Portable copies do not own the installed application's task preferences.
    QSettings installer("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
                        "{730C3722-BAF0-454C-A55E-85577EA4064E}_is1", QSettings::NativeFormat);
    const auto location = QDir::cleanPath(QDir::fromNativeSeparators(
        installer.value("InstallLocation").toString()));
    if (location.compare(QDir::cleanPath(QCoreApplication::applicationDirPath()), Qt::CaseInsensitive) == 0) {
        auto tasks = installer.value("Inno Setup: Selected Tasks").toString().split(',', Qt::SkipEmptyParts);
        tasks.removeAll("keyboardpaste");
        if (enabled)
            tasks.append("keyboardpaste");
        installer.setValue("Inno Setup: Selected Tasks", tasks.join(','));
        installer.sync();
        if (installer.status() != QSettings::NoError)
            return "Copier changed, but its preference could not be saved for installer upgrades.";
    }
    return {};
}
} // namespace strata
