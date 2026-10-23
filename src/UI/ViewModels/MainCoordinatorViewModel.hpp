#pragma once
#include "Core/RobocopyProcessSupervisor.hpp"
#include "Core/UpdateChecker.hpp"
#include "Persistence/SqliteQueueStore.hpp"
#include "UI/Models/FileTransferListModel.hpp"
#include "UI/Models/TelemetryHistoryModel.hpp"
#include <QTimer>
#include <QCoreApplication>
#include <QUrl>
namespace strata {
class MainCoordinatorViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList sources READ sources NOTIFY queueChanged)
    Q_PROPERTY(QString destination READ destination WRITE setDestination NOTIFY queueChanged)
    Q_PROPERTY(QVariantMap profile READ profile NOTIFY profileChanged)
    Q_PROPERTY(QVariantList parameterGroups READ parameterGroups CONSTANT)
    Q_PROPERTY(QStringList builtinPresets READ builtinPresets CONSTANT)
    Q_PROPERTY(QVariantList savedPresets READ savedPresets NOTIFY presetsChanged)
    Q_PROPERTY(QString commandPreview READ commandPreview NOTIFY commandChanged)
    Q_PROPERTY(QString validation READ validation NOTIFY commandChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY stateChanged)
    Q_PROPERTY(QString phase READ phase NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)
    Q_PROPERTY(QString outcome READ outcome NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY statsChanged)
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY statsChanged)
    Q_PROPERTY(QString totalSize READ totalSize NOTIFY statsChanged)
    Q_PROPERTY(QString eta READ eta NOTIFY statsChanged)
    Q_PROPERTY(quint64 sourceFileCount READ sourceFileCount NOTIFY statsChanged)
    Q_PROPERTY(quint64 omittedRows READ omittedRows NOTIFY statsChanged)
    Q_PROPERTY(QAbstractItemModel *files READ files CONSTANT)
    Q_PROPERTY(FileTransferListModel *fileStats READ fileStats CONSTANT)
    Q_PROPERTY(TelemetryHistoryModel *historyGraph READ historyGraph CONSTANT)
    Q_PROPERTY(QVariantList recentRuns READ recentRuns NOTIFY runsChanged)
    Q_PROPERTY(QString logPath READ logPath NOTIFY stateChanged)
    Q_PROPERTY(bool defaultCopyHandler READ defaultCopyHandler NOTIFY integrationChanged)
    Q_PROPERTY(bool checkingForUpdates READ checkingForUpdates NOTIFY updateCheckChanged)
  public:
    explicit MainCoordinatorViewModel(QObject *parent = nullptr);
    ~MainCoordinatorViewModel() override;
    QStringList sources() const {
        return m_sources;
    }
    QString destination() const {
        return m_destination;
    }
    void setDestination(const QString &);
    QVariantMap profile() const {
        return m_profile.values;
    }
    QVariantList parameterGroups() const {
        return RobocopyProfile::schema();
    }
    QStringList builtinPresets() const;
    QVariantList savedPresets() const {
        return m_store.presets();
    }
    QString commandPreview() const;
    QString validation() const;
    bool busy() const {
        return m_busy;
    }
    bool paused() const {
        return m_paused;
    }
    QString phase() const {
        return m_phase;
    }
    QString message() const {
        return m_message;
    }
    QString outcome() const {
        return m_outcome;
    }
    double progress() const {
        return m_progress;
    }
    QVariantMap stats() const {
        return m_stats;
    }
    QString totalSize() const {
        return formatBytes(m_totalBytes);
    }
    QString eta() const;
    quint64 sourceFileCount() const {
        return m_totalFiles;
    }
    quint64 omittedRows() const {
        return m_supervisor.omittedEntries();
    }
    QAbstractItemModel *files() {
        return &m_filter;
    }
    FileTransferListModel *fileStats() {
        return &m_files;
    }
    TelemetryHistoryModel *historyGraph() {
        return &m_graph;
    }
    QVariantList recentRuns() const {
        return m_store.history();
    }
    QString logPath() const {
        return m_logPath;
    }
    Q_INVOKABLE void addSources(const QVariantList &);
    void addPaths(const QStringList &);
    bool importExplorerRequest(const QString &);
    Q_INVOKABLE void rejectExplorerRequest();
    Q_INVOKABLE void removeSource(int);
    Q_INVOKABLE void clearSources();
    Q_INVOKABLE void setOption(const QString &, const QVariant &);
    Q_INVOKABLE void applyPreset(int);
    Q_INVOKABLE void savePreset(const QString &);
    Q_INVOKABLE void loadPreset(int);
    Q_INVOKABLE bool start(bool dryRun = false, bool destructiveConfirmed = false);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void resolveFileConflict(quint64 token, int choice) {
        m_supervisor.resolveConflict(token, choice);
    }
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void setFilter(const QString &category) {
        m_filter.setCategory(category);
    }
    Q_INVOKABLE void setSearch(const QString &text) {
        m_filter.setSearch(text);
    }
    Q_INVOKABLE void copyCommand();
    Q_INVOKABLE void exportScript(const QUrl &);
    Q_INVOKABLE void openLog();
    Q_INVOKABLE void openDestination();
    Q_INVOKABLE void restoreRun(int);
    Q_INVOKABLE void setShellIntegration(bool);
    Q_INVOKABLE void setKeyboardPasteIntegration(bool enabled);
    bool defaultCopyHandler() const;
    Q_INVOKABLE void refreshIntegration() {
        emit integrationChanged();
    }
    bool checkingForUpdates() const {
        return m_updateChecker.checking();
    }
    Q_INVOKABLE void checkForUpdates() {
        m_updateChecker.check();
    }
    Q_INVOKABLE void completionAction(int);
    Q_INVOKABLE QString storageType(const QString &) const;
    Q_INVOKABLE QString pathFromUrl(const QUrl &url) const {
        return url.toLocalFile();
    }
    Q_INVOKABLE static QString formatBytes(quint64 bytes);
  signals:
    void queueChanged();
    void integrationChanged();
    void updateCheckChanged();
    void updateCheckFinished(QString message);
    void fileConflictRequired(QVariantMap conflict);
    void profileChanged();
    void commandChanged();
    void stateChanged();
    void statsChanged();
    void presetsChanged();
    void runsChanged();
    void destructiveConfirmationRequired(QString);
    void completedSuccessfully();
    void closeRequested();
    void previewReady();

  private:
    void changed();
    void flushEntries();
    void saveQueue();
    QJsonObject queueJson() const;
    void restore(const QJsonObject &);
    RobocopyProfile m_profile, m_runProfile;
    QStringList m_sources;
    QString m_destination, m_phase = "Ready to transfer", m_message, m_outcome, m_logPath, m_dataDir;
    bool m_busy = false, m_paused = false, m_storeReady = false;
    bool m_confirmationPending = false;
    QString m_explorerRequestPath;
    QTimer m_importTimer;
    double m_progress = 0;
    quint64 m_totalBytes = 0, m_totalFiles = 0;
    QVariantMap m_stats{{"readMbps", 0}, {"writeMbps", 0}, {"iops", 0}, {"elapsed", 0}};
    qint64 m_runId = -1;
    SqliteQueueStore m_store;
    FileTransferListModel m_files;
    FileFilterModel m_filter;
    TelemetryHistoryModel m_graph;
    QTimer m_flushTimer, m_saveTimer;
    RobocopyProcessSupervisor m_supervisor;
    UpdateChecker m_updateChecker{QCoreApplication::applicationVersion()};
};
} // namespace strata
