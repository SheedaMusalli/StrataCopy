#include "MainCoordinatorViewModel.hpp"
#include "Platform/DriveHardwareDetector.hpp"
#include "Platform/ShellContextMenuRegistrar.hpp"
#include "Platform/UniqueHandle.hpp"
#include "UI/Models/PresetListModel.hpp"
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QSaveFile>
#include <QSettings>
#include <QUuid>
#include <QStandardPaths>
#include <powrprof.h>
#include <winioctl.h>
namespace strata {
bool MainCoordinatorViewModel::defaultCopyHandler() const {
    return ShellContextMenuRegistrar::keyboardPasteEnabled();
}
void MainCoordinatorViewModel::setKeyboardPasteIntegration(bool enabled) {
    m_message = ShellContextMenuRegistrar::setKeyboardPasteEnabled(enabled);
    if (m_message.isEmpty())
        m_message = enabled ? "StrataCopy enabled as the Explorer file copier for this account."
                            : "Explorer interception disabled. Windows handles file operations.";
    emit integrationChanged();
    emit stateChanged();
}
bool MainCoordinatorViewModel::importExplorerRequest(const QString &path) {
    if (m_busy || m_confirmationPending || !m_explorerRequestPath.isEmpty()) {
        m_message = "Explorer transfer queued. It will open after the current operation.";
        emit stateChanged();
        return false; // Keep the durable request; the import timer will retry it.
    }
    const auto directory = m_dataDir + "/ExplorerRequests";
    QFile file(path);
    if (!MultiSourceBatcher::inside(path, directory) || QFileInfo(path).suffix() != "json" ||
        !file.open(QIODevice::ReadOnly) || file.size() > 512 * 1024) {
        m_message = "Invalid Explorer paste request.";
        emit stateChanged();
        return false;
    }
    const auto request = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    const auto items = request.value("sources").toArray();
    const auto operation = request.value("operation").toString();
    const auto destination = request.value("destination").toString();
    const bool deleting = operation == "delete" || operation == "recycle";
    if (items.isEmpty() || items.size() > 4096 || (!deleting && operation != "copy" && operation != "move") ||
        (!deleting && !QFileInfo(destination).isDir())) {
        m_message = "Explorer paste request is incomplete.";
        file.remove();
        emit stateChanged();
        return false;
    }
    QStringList sources;
    for (const auto &value : items) {
        if (!value.isString() || !QFileInfo(value.toString()).isAbsolute()) {
            m_message = "Explorer paste requires absolute source paths.";
            file.remove();
            emit stateChanged();
            return false;
        }
        sources << value.toString();
    }
    if (deleting) {
        for (const auto &source : sources) {
            if (QDir(source).isRoot() ||
                GetFileAttributesW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(source).utf16())) ==
                    INVALID_FILE_ATTRIBUTES) {
                file.remove();
                return false;
            }
        }
        m_sources = sources;
        m_destination.clear();
        m_explorerRequestPath = path;
        m_deleting = true;
        m_permanentDelete = operation == "delete";
        m_confirmationPending = true;
        changed();
        emit stateChanged();
        emit deletionConfirmationRequired(m_permanentDelete, sources);
        return true;
    }
    m_sources = sources;
    m_destination = destination;
    m_explorerRequestPath = path;
    const auto previousPerformance = m_profile.values;
    m_profile = RobocopyProfile();
    for (const auto &key :
         {"performanceMode", "threadCount", "enableMultithreading", "unbufferedIo", "retryCount",
          "retryWaitSeconds", "enableBandwidthLimit", "interPacketGapMs", "fastFailSafeRetries",
          "nvmeThreads", "ssdThreads", "hddThreads", "usbThreads", "networkThreads", "unknownThreads"})
        m_profile.values[key] = previousPerformance.value(key, m_profile.values.value(key));
    if (operation == "move")
        m_profile.values["transferMode"] = 3;
    emit profileChanged();
    changed();
    return start(false, false);
}
void MainCoordinatorViewModel::confirmDeletion() {
    if (!m_confirmationPending || !m_deleting || m_busy)
        return;
    m_confirmationPending = false;
    m_runProfile = RobocopyProfile();
    m_runClock.start();
    m_explorerRun = true;
    auto payload = queueJson();
    payload["operation"] = m_permanentDelete ? "delete" : "recycle";
    m_logPath.clear();
    m_runId = m_storeReady ? m_store.beginRun(payload, {}) : -1;
    if (m_runId < 0) {
        m_message = "Cannot journal deletion.";
        rejectExplorerRequest();
        return;
    }
    m_files.clear();
    m_graph.clear();
    m_progress = 0;
    m_totalBytes = 0;
    m_totalFiles = m_sources.size();
    m_outcome.clear();
    m_message.clear();
    m_phase = m_permanentDelete ? "Deleting items" : "Moving items to Recycle Bin";
    m_busy = true;
    m_paused = false;
    if (!m_deletion.start(m_sources, m_permanentDelete)) {
        m_busy = false;
        m_outcome = "error";
        m_message = "Could not start deletion. Nothing was removed.";
        m_store.finishRun(m_runId, "Failed", m_message);
    }
    QFile::remove(m_explorerRequestPath);
    m_explorerRequestPath.clear();
    emit stateChanged();
    emit statsChanged();
    emit runsChanged();
}
void MainCoordinatorViewModel::rejectExplorerRequest() {
    m_confirmationPending = false;
    if (m_deleting) {
        m_sources.clear();
        m_deleting = false;
        changed();
    }
    if (!m_explorerRequestPath.isEmpty()) {
        QFile::remove(m_explorerRequestPath);
        m_explorerRequestPath.clear();
        m_message = "Operation cancelled before starting.";
        emit stateChanged();
    }
}
MainCoordinatorViewModel::MainCoordinatorViewModel(QObject *parent) : QObject(parent) {
    connect(&m_supervisor, &RobocopyProcessSupervisor::fileConflict, this,
            [this](quint64 token, const QString &source, const QString &destination) {
                if (!m_busy)
                    return;
                if (m_conflictChoice >= 0) {
                    m_supervisor.resolveConflict(token, m_conflictChoice);
                    return;
                }
                m_conflictToken = token;
                const QFileInfo from(source), to(destination);
                emit fileConflictRequired(
                    {{"token", QVariant::fromValue(token)},
                     {"name", from.fileName()},
                     {"source", source},
                     {"destination", destination},
                     {"sourceSize", formatBytes(quint64(from.size()))},
                     {"destinationSize", formatBytes(quint64(to.size()))},
                     {"sourceDate", QLocale().toString(from.lastModified(), QLocale::ShortFormat)},
                     {"destinationDate", QLocale().toString(to.lastModified(), QLocale::ShortFormat)}});
            });
    connect(&m_updateChecker, &UpdateChecker::checkingChanged, this,
            &MainCoordinatorViewModel::updateCheckChanged);
    connect(&m_updateChecker, &UpdateChecker::finished, this,
            [this](UpdateChecker::Result result, const QString &message) {
                m_message = message;
                if (result == UpdateChecker::UpdateAvailable) {
                    if (!QDesktopServices::openUrl(
                            QUrl("https://github.com/SheedaMusalli/StrataCopy/releases/latest"))) {
                        m_message = "An update is available, but the browser could not be opened. Visit the "
                                    "StrataCopy GitHub releases page.";
                        emit updateCheckFinished(m_message);
                    }
                } else {
                    emit updateCheckFinished(m_message);
                }
                emit stateChanged();
            });
    m_dataDir = qEnvironmentVariable("STRATACOPY_DATA_DIR",
                                     QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    QDir().mkpath(m_dataDir + "/logs");
    m_storeReady = m_store.open(m_dataDir + "/stratacopy.sqlite");
    if (m_storeReady)
        restore(m_store.pending());
    else
        m_message = "Queue journal unavailable: " + m_store.error();
    QSettings installPreferences;
    const int installationProfile = installPreferences.value("Installer/performanceProfile", -1).toInt();
    if (installationProfile >= 0 && installationProfile <= 2) {
        m_profile.values["performanceMode"] = installationProfile;
        m_profile.values["enableMultithreading"] = true;
        m_profile.values["enableBandwidthLimit"] = false;
        m_profile.values["threadCount"] = installationProfile == 2 ? 32 : installationProfile == 1 ? 4 : 8;
        m_profile.values["retryCount"] = installationProfile == 2 ? 0 : 2;
        m_profile.values["retryWaitSeconds"] = installationProfile == 2 ? 0 : 2;
        m_profile.values["fastFailSafeRetries"] = true;
        m_profile.values["unbufferedIo"] = false;
        // Persist before consuming the one-time installer choice.
        if (m_storeReady && m_store.savePending(queueJson()))
            installPreferences.remove("Installer/performanceProfile");
    }
    m_sources.clear();
    m_profile.values["transferMode"] = 0;
    m_performanceSummary =
        QString("%1 logical processors · %2 GB RAM. Auto checks both drives when a transfer starts.")
            .arg(DriveHardwareDetector::logicalProcessors())
            .arg(DriveHardwareDetector::memoryBytes() / (1024ULL * 1024 * 1024));
    m_filter.setSourceModel(&m_files);
    m_flushTimer.setInterval(33);
    connect(&m_flushTimer, &QTimer::timeout, this, &MainCoordinatorViewModel::flushEntries);
    m_flushTimer.start();
    m_importTimer.setInterval(1500);
    connect(&m_importTimer, &QTimer::timeout, this, [this] {
        if (m_busy || m_confirmationPending || !m_explorerRequestPath.isEmpty())
            return;
        QDir directory(m_dataDir + "/ExplorerRequests");
        const auto requests = directory.entryInfoList({"*.json"}, QDir::Files, QDir::Time | QDir::Reversed);
        if (!requests.isEmpty())
            importExplorerRequest(requests.first().absoluteFilePath());
    });
    m_importTimer.start();
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(400);
    connect(&m_saveTimer, &QTimer::timeout, this, &MainCoordinatorViewModel::saveQueue);
    connect(&m_supervisor, &RobocopyProcessSupervisor::phaseChanged, this, [this](const QString &phase) {
        m_phase = phase;
        emit stateChanged();
    });
    connect(&m_supervisor, &RobocopyProcessSupervisor::scanned, this,
            [this](quint64 bytes, quint64 files, const QStringList &warnings) {
                m_totalBytes = bytes;
                m_totalFiles = files;
                if (!warnings.isEmpty())
                    m_message = warnings.first() +
                                QString(" · %1 scan warnings; progress is approximate.").arg(warnings.size());
                emit statsChanged();
                emit stateChanged();
            });
    connect(&m_supervisor, &RobocopyProcessSupervisor::telemetry, this, [this](QVariantMap values) {
        m_supervisor.acknowledgeTelemetry();
        m_stats = std::move(values);
        m_paused = m_stats.value("paused").toBool();
        if (m_totalBytes && !m_runProfile.flag("dryRunListOnly"))
            m_progress = qMin(.99, m_stats.value("writeBytes").toDouble() / double(m_totalBytes));
        m_graph.append(m_stats.value("readMbps").toDouble(), m_stats.value("writeMbps").toDouble());
        emit statsChanged();
        emit stateChanged();
    });
    const auto finishOperation = [this](bool success, bool cancelled, const QString &message) {
        while (true) {
            auto entries = m_supervisor.takeEntries(8192);
            if (entries.isEmpty())
                break;
            m_files.append(std::move(entries));
        }
        m_files.settle(success, m_runProfile.flag("dryRunListOnly"));
        m_busy = false;
        m_paused = false;
        m_outcome = success ? "success" : (cancelled ? "cancelled" : "error");
        m_phase = success ? (m_runProfile.flag("dryRunListOnly") ? "Preview complete" : "Transfer complete")
                          : (cancelled ? "Transfer cancelled" : "Needs attention");
        m_message = message;
        if (success)
            m_progress = 1;
        m_stats["readMbps"] = 0;
        m_stats["writeMbps"] = 0;
        m_stats["iops"] = 0;
        if (m_storeReady &&
            !m_store.finishRun(m_runId, cancelled ? "Cancelled" : (success ? "Completed" : "Failed"),
                               message))
            m_message += " · Could not update the journal.";
        if (success && m_explorerRun)
            m_destination.clear();
        m_explorerRun = false;
        m_sources.clear();
        m_profile.values["transferMode"] = 0;
        emit profileChanged();
        changed();
        emit stateChanged();
        emit statsChanged();
        emit runsChanged();
        emit commandChanged();
        if (success && !m_runProfile.flag("dryRunListOnly"))
            emit transferSucceeded(m_runClock.elapsed());
        if (success && !m_runProfile.flag("dryRunListOnly") &&
            QDir(m_dataDir + "/ExplorerRequests").entryList({"*.json"}, QDir::Files).isEmpty())
            emit completedSuccessfully();
        if (success && m_runProfile.flag("dryRunListOnly"))
            emit previewReady();
    };
    connect(&m_supervisor, &RobocopyProcessSupervisor::finished, this, finishOperation);
    connect(&m_deletion, &DeletionSupervisor::finished, this, finishOperation);
    connect(&m_deletion, &DeletionSupervisor::progress, this,
            [this](quint64 done, quint64 total, const QString &path) {
                m_progress = total ? qMin(.99, double(done) / double(total)) : 0;
                m_phase = (m_permanentDelete ? "Deleting: " : "Recycling: ") + path;
                emit stateChanged();
                emit statsChanged();
            });
}
MainCoordinatorViewModel::~MainCoordinatorViewModel() {
    m_saveTimer.stop();
    saveQueue();
    m_supervisor.stopAndWait();
    m_deletion.stopAndWait();
    if (m_busy && m_storeReady)
        m_store.finishRun(m_runId, "Cancelled",
                          "Application closed during the transfer; completed copies remain.");
}
QStringList MainCoordinatorViewModel::builtinPresets() const {
    return PresetListModel::builtins();
}
void MainCoordinatorViewModel::setDestination(const QString &text) {
    if (m_busy)
        return;
    const auto path = QDir::fromNativeSeparators(text.trimmed());
    if (m_destination == path)
        return;
    m_destination = path;
    changed();
}
void MainCoordinatorViewModel::resolveFileConflict(quint64 token, int choice, bool all) {
    if (!m_busy || !m_conflictToken || token != m_conflictToken || choice < 0 || choice > 3)
        return;
    m_conflictToken = 0;
    if (all && choice < 3)
        m_conflictChoice = choice;
    m_supervisor.resolveConflict(token, choice);
}
void MainCoordinatorViewModel::addSources(const QVariantList &urls) {
    QStringList paths;
    for (const auto &v : urls) {
        auto url = v.toUrl();
        auto path = url.isLocalFile() ? url.toLocalFile() : v.toString();
        paths << path;
    }
    addPaths(paths);
}
void MainCoordinatorViewModel::addPaths(const QStringList &paths) {
    if (m_confirmationPending || (m_busy && m_deleting)) {
        m_message = "Finish or cancel the current deletion/confirmation before adding sources.";
        emit stateChanged();
        return;
    }
    if (m_busy) {
        QStringList valid;
        for (const auto &path : paths)
            if (QFileInfo(path).isAbsolute() && QFileInfo::exists(path) && !valid.contains(path))
                valid << path;
        const auto directory = m_dataDir + "/ExplorerRequests";
        QDir().mkpath(directory);
        if (valid.isEmpty() || valid.size() > 4096 ||
            QDir(directory).entryList({"*.json"}, QDir::Files).size() >= 64)
            return;
        const auto payload =
            QJsonDocument(QJsonObject{{"sources", QJsonArray::fromStringList(valid)},
                                      {"destination", m_destination},
                                      {"operation", (m_runProfile.mode() == TransferMode::MoveFiles ||
                                                     m_runProfile.mode() == TransferMode::MoveTree)
                                                        ? "move"
                                                        : "copy"}})
                .toJson(QJsonDocument::Compact);
        QSaveFile request(directory + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".json");
        if (payload.size() <= 512 * 1024 && request.open(QIODevice::WriteOnly) &&
            request.write(payload) == payload.size() && request.commit())
            m_message = "Added to the queue. These files will transfer to the same destination next.";
        else
            m_message = "Could not queue the dropped files.";
        emit stateChanged();
        return;
    }
    if (m_deleting) {
        m_deleting = false;
        emit stateChanged();
    }
    for (const auto &path : paths) {
        if (!path.trimmed().isEmpty()) {
            const auto normalized = MultiSourceBatcher::normalized(path);
            if (!m_sources.contains(normalized, Qt::CaseInsensitive))
                m_sources << normalized;
        }
    }
    changed();
}
void MainCoordinatorViewModel::removeSource(int i) {
    if (!m_busy && i >= 0 && i < m_sources.size()) {
        m_sources.removeAt(i);
        changed();
    }
}
void MainCoordinatorViewModel::clearSources() {
    if (!m_busy) {
        m_sources.clear();
        changed();
    }
}
void MainCoordinatorViewModel::setOption(const QString &key, const QVariant &value) {
    if (m_busy || !m_profile.values.contains(key))
        return;
    m_profile.values[key] = value;
    if (QStringList{"nvmeThreads", "ssdThreads", "hddThreads", "usbThreads", "networkThreads",
                    "unknownThreads"}
            .contains(key)) {
        if (m_profile.number("performanceMode") == 2) {
            m_profile.values["retryCount"] = 2;
            m_profile.values["retryWaitSeconds"] = 2;
        }
        m_profile.values["performanceMode"] = 0;
        m_profile.values["enableMultithreading"] = true;
        m_profile.values["enableBandwidthLimit"] = false;
    }
    if (key == "threadCount" || key == "enableMultithreading" || key == "enableBandwidthLimit")
        m_profile.values["performanceMode"] = 3;
    if (key == "performanceMode") {
        if (value.toInt() == 1)
            m_profile.values["threadCount"] = 4;
        if (value.toInt() == 2) {
            m_profile.values["fastFailSafeRetries"] = true;
            m_profile.values["threadCount"] = 32;
            m_profile.values["retryCount"] = 0;
            m_profile.values["retryWaitSeconds"] = 0;
        }
        if (value.toInt() == 0) {
            m_profile.values["retryCount"] = 2;
            m_profile.values["retryWaitSeconds"] = 2;
        }
        if (value.toInt() != 3) {
            m_profile.values["enableMultithreading"] = true;
            m_profile.values["enableBandwidthLimit"] = false;
        }
    }
    if (key == "enableBandwidthLimit" && value.toBool())
        m_profile.values["enableMultithreading"] = false;
    if (key == "enableMultithreading" && value.toBool())
        m_profile.values["enableBandwidthLimit"] = false;
    emit profileChanged();
    changed();
}
void MainCoordinatorViewModel::applyPreset(int i) {
    if (m_busy)
        return;
    m_profile = RobocopyProfile::preset(i);
    emit profileChanged();
    changed();
}
void MainCoordinatorViewModel::savePreset(const QString &name) {
    if (name.trimmed().isEmpty())
        return;
    if (!m_store.savePreset(name.trimmed(), QJsonObject::fromVariantMap(m_profile.values))) {
        m_message = "Could not save preset.";
        emit stateChanged();
    }
    emit presetsChanged();
}
void MainCoordinatorViewModel::loadPreset(int i) {
    if (m_busy)
        return;
    const auto presets = m_store.presets();
    if (i < 0 || i >= presets.size())
        return;
    const auto values = presets[i].toMap().value("values").toMap();
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        if (m_profile.values.contains(it.key()))
            m_profile.values[it.key()] = it.value();
    if (!values.contains("performanceMode") && values.contains("threadCount"))
        m_profile.values["performanceMode"] = m_profile.number("threadCount") == 128 ? 0 : 3;
    emit profileChanged();
    changed();
}
QString MainCoordinatorViewModel::commandPreview() const {
    const auto batch = MultiSourceBatcher::build(m_sources, m_destination, m_profile);
    QStringList lines;
    if (batch.jobs.isEmpty())
        return "Robocopy Auto profile";
    for (qsizetype i = 0; i < batch.jobs.size(); ++i)
        lines << RobocopyCommandCompiler::display(batch.jobs[i], m_busy ? m_runProfile : m_profile,
                                                  m_busy ? m_logPath + QString(".job%1.utf16.log").arg(i + 1)
                                                         : QString());
    return lines.join('\n');
}
QString MainCoordinatorViewModel::validation() const {
    return MultiSourceBatcher::build(m_sources, m_destination, m_profile).errors.join('\n');
}
bool MainCoordinatorViewModel::start(bool dryRun, bool destructiveConfirmed) {
    if (m_busy)
        return false;
    if (destructiveConfirmed)
        m_confirmationPending = false;
    m_deleting = false;
    auto profile = m_profile;
    if (dryRun)
        profile.values["dryRunListOnly"] = true;
    const int performance = profile.number("performanceMode");
    if (performance == 0)
        profile.values["threadCount"] =
            DriveHardwareDetector::transferThreads(m_sources, m_destination, profile.values);
    if (performance == 1)
        profile.values["threadCount"] = 4;
    if (performance == 2) {
        profile.values["fastFailSafeRetries"] = true;
        profile.values["threadCount"] = 32;
        profile.values["retryCount"] = 0;
        profile.values["retryWaitSeconds"] = 0;
    }
    m_profile.values["threadCount"] = profile.values["threadCount"];
    m_performanceSummary =
        QString("%1 · %2 threads · %3 logical processors · destination: %4")
            .arg(QStringList{"Auto", "Safe / Silent", "NVMe Extreme", "Manual"}.value(performance, "Manual"))
            .arg(profile.flag("enableMultithreading") && !profile.flag("enableBandwidthLimit")
                     ? profile.number("threadCount")
                     : 1)
            .arg(DriveHardwareDetector::logicalProcessors())
            .arg(DriveHardwareDetector::describe(m_destination));
    emit profileChanged();
    const auto batch = MultiSourceBatcher::build(m_sources, m_destination, profile);
    if (!batch.errors.isEmpty()) {
        m_message = batch.errors.join('\n');
        m_outcome = "error";
        emit stateChanged();
        return false;
    }
    if (profile.mode() == TransferMode::Mirror && !profile.flag("dryRunListOnly") && !destructiveConfirmed) {
        m_confirmationPending = true;
        emit destructiveConfirmationRequired(
            "Mirror deletes destination items absent from the source. Continue?");
        return false;
    }
    if (!m_storeReady) {
        m_message = "A working queue journal is required to begin a transfer.";
        emit stateChanged();
        return false;
    }
    m_conflictChoice = -1;
    m_conflictToken = 0;
    m_runClock.start();
    m_explorerRun = !m_explorerRequestPath.isEmpty();
    m_runDestination = m_destination;
    m_runProfile = profile;
    m_logPath =
        m_dataDir + "/logs/" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss-zzz") + ".log";
    auto payload = queueJson();
    payload["profile"] = QJsonObject::fromVariantMap(profile.values);
    m_runId = m_store.beginRun(payload, m_logPath);
    if (m_runId < 0) {
        m_message = "Cannot journal transfer: " + m_store.error();
        emit stateChanged();
        return false;
    }
    m_files.clear();
    m_graph.clear();
    m_filter.setCategory("");
    m_filter.setSearch("");
    m_progress = 0;
    m_totalBytes = 0;
    m_totalFiles = 0;
    m_outcome.clear();
    m_message.clear();
    m_phase = "Scanning sources";
    m_busy = true;
    m_paused = false;
    m_stats = {{"readMbps", 0}, {"writeMbps", 0}, {"iops", 0}, {"elapsed", 0}};
    saveQueue();
    if (!m_supervisor.start(batch.jobs, profile, m_logPath, profile.number("collisionPolicy") == 4)) {
        m_busy = false;
        m_message = "Unable to start the transfer worker.";
        m_store.finishRun(m_runId, "Failed", m_message);
    }
    if (m_busy && !m_explorerRequestPath.isEmpty()) {
        QFile::remove(m_explorerRequestPath); // Accepted only after journaling and worker startup.
        m_explorerRequestPath.clear();
    }
    emit stateChanged();
    emit statsChanged();
    emit runsChanged();
    emit commandChanged();
    return m_busy;
}
void MainCoordinatorViewModel::cancel() {
    if (m_busy) {
        if (m_deleting)
            m_deletion.cancel();
        else
            m_supervisor.cancel();
        m_phase = "Cancelling…";
        emit stateChanged();
    }
}
void MainCoordinatorViewModel::togglePause() {
    if (!m_busy || (!m_phase.startsWith("Transferring") && !m_phase.startsWith("Dry-run")))
        return;
    m_supervisor.setPaused(!m_paused);
}
void MainCoordinatorViewModel::flushEntries() {
    auto entries = m_supervisor.takeEntries();
    if (!entries.isEmpty())
        m_files.append(std::move(entries));
}
QString MainCoordinatorViewModel::formatBytes(quint64 bytes) {
    double value = double(bytes);
    const QStringList units{"B", "KB", "MB", "GB", "TB", "PB"};
    int unit = 0;
    while (value >= 1000 && unit < units.size() - 1) {
        value /= 1000;
        ++unit;
    }
    return QString::number(value, 'f', unit ? 1 : 0) + " " + units[unit];
}
QString MainCoordinatorViewModel::eta() const {
    if (m_paused)
        return "Paused";
    if (!m_busy || m_progress <= 0 || m_stats.value("writeMbps").toDouble() <= 0)
        return "—";
    const double remaining = qMax(0.0, (double(m_totalBytes) - m_stats.value("writeBytes").toDouble()) /
                                           (m_stats.value("writeMbps").toDouble() * 1e6));
    if (remaining > 359999)
        return ">99h";
    const auto secs = qint64(remaining);
    return QString("%1:%2:%3")
        .arg(secs / 3600, 2, 10, QChar('0'))
        .arg(secs / 60 % 60, 2, 10, QChar('0'))
        .arg(secs % 60, 2, 10, QChar('0'));
}
void MainCoordinatorViewModel::copyCommand() {
    QGuiApplication::clipboard()->setText(commandPreview());
}
void MainCoordinatorViewModel::exportScript(const QUrl &url) {
    const auto batch = MultiSourceBatcher::build(m_sources, m_destination, m_profile);
    if (!batch.errors.isEmpty()) {
        m_message = batch.errors.join('\n');
        emit stateChanged();
        return;
    }
    const auto path = url.toLocalFile();
    QString text;
    if (path.endsWith(".ps1", Qt::CaseInsensitive))
        text = RobocopyCommandCompiler::powerShell(batch.jobs, m_profile);
    else if (path.endsWith(".bat", Qt::CaseInsensitive)) {
        text = "@echo off\r\nchcp 65001 >nul\r\nsetlocal DisableDelayedExpansion\r\n";
        for (const auto &job : batch.jobs) {
            auto cmd = RobocopyCommandCompiler::display(job, m_profile);
            cmd.replace("%", "%%");
            text += cmd + "\r\nif errorlevel 8 exit /b %errorlevel%\r\n";
        }
        text += "exit /b 0\r\n";
    } else {
        m_message = "Choose a .ps1 or .bat filename.";
        emit stateChanged();
        return;
    }
    QSaveFile file(path);
    const auto bytes = text.toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        m_message = "Cannot export script: " + file.errorString();
    else
        m_message = "Script saved: " + path;
    emit stateChanged();
}
void MainCoordinatorViewModel::openLog() {
    if (!m_logPath.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_logPath));
}
void MainCoordinatorViewModel::openDestination() {
    if (!m_destination.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_destination));
}
void MainCoordinatorViewModel::setShellIntegration(bool enabled) {
    m_message = ShellContextMenuRegistrar::setEnabled(enabled);
    if (m_message.isEmpty())
        m_message =
            enabled ? "Explorer action installed under Show more options." : "Explorer action removed.";
    emit stateChanged();
}
QString MainCoordinatorViewModel::storageType(const QString &path) const {
    return DriveHardwareDetector::describe(path);
}
void MainCoordinatorViewModel::completionAction(int action) {
    if (m_busy || m_outcome != "success")
        return;
    if (action == 1) {
        emit closeRequested();
        return;
    }
    if (action == 2) {
        if (!SetSuspendState(FALSE, FALSE, FALSE)) {
            m_message = "Windows declined the sleep request.";
            emit stateChanged();
        }
        return;
    }
    if (action == 3) {
        const auto root = QDir::toNativeSeparators(m_runDestination).left(3);
        if (root.size() != 3 || GetDriveTypeW(reinterpret_cast<LPCWSTR>(root.utf16())) != DRIVE_REMOVABLE) {
            m_message = "Automatic eject requires a removable destination drive.";
            emit stateChanged();
            return;
        }
        const auto device = "\\\\.\\" + root.left(2);
        UniqueHandle volume(CreateFileW(reinterpret_cast<LPCWSTR>(device.utf16()),
                                        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        nullptr, OPEN_EXISTING, 0, nullptr));
        DWORD returned = 0;
        if (!volume ||
            !DeviceIoControl(volume.get(), FSCTL_LOCK_VOLUME, nullptr, 0, nullptr, 0, &returned, nullptr) ||
            !DeviceIoControl(volume.get(), FSCTL_DISMOUNT_VOLUME, nullptr, 0, nullptr, 0, &returned,
                             nullptr) ||
            !DeviceIoControl(volume.get(), IOCTL_STORAGE_EJECT_MEDIA, nullptr, 0, nullptr, 0, &returned,
                             nullptr))
            m_message = "Windows could not eject the drive. Close other files on it and eject from Explorer.";
        else
            m_message = "Destination drive ejected.";
        emit stateChanged();
    }
}
void MainCoordinatorViewModel::changed() {
    emit queueChanged();
    emit commandChanged();
    m_saveTimer.start();
}
QJsonObject MainCoordinatorViewModel::queueJson() const {
    return {{"sources", QJsonArray::fromStringList(m_sources)},
            {"destination", m_destination},
            {"profile", QJsonObject::fromVariantMap(m_profile.values)},
            {"conflictPolicyRevision", 1}};
}
void MainCoordinatorViewModel::saveQueue() {
    if (m_storeReady)
        m_store.savePending(queueJson());
}
void MainCoordinatorViewModel::restore(const QJsonObject &object) {
    m_sources.clear();
    for (const auto &s : object.value("sources").toArray())
        m_sources << s.toString();
    m_destination = object.value("destination").toString();
    const auto values = object.value("profile").toObject().toVariantMap();
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        if (m_profile.values.contains(it.key()))
            m_profile.values[it.key()] = it.value();
    if (!values.contains("performanceMode") && values.contains("threadCount")) {
        m_profile.values["performanceMode"] = m_profile.number("threadCount") == 128 ? 0 : 3;
        if (m_profile.number("performanceMode") == 0) {
            m_profile.values["threadCount"] = 8;
            m_profile.values["unbufferedIo"] = false;
        }
    }
    if (!object.contains("conflictPolicyRevision") && m_profile.number("collisionPolicy") == 0)
        m_profile.values["collisionPolicy"] = 4;
}
void MainCoordinatorViewModel::restoreRun(int index) {
    if (m_busy)
        return;
    const auto runs = m_store.history();
    if (index < 0 || index >= runs.size())
        return;
    m_deleting = false;
    restore(QJsonObject::fromVariantMap(runs[index].toMap().value("payload").toMap()));
    emit profileChanged();
    changed();
    m_message = "Saved queue restored. Review it before starting.";
    emit stateChanged();
}
} // namespace strata
