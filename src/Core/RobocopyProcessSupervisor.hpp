#pragma once
#include "RobocopyCommandCompiler.hpp"
#include "StdoutPipeParser.hpp"
#include <QObject>
#include <QVariantMap>
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <condition_variable>
namespace strata {
class RobocopyProcessSupervisor : public QObject {
    Q_OBJECT
  public:
    explicit RobocopyProcessSupervisor(QObject *parent = nullptr) : QObject(parent) {}
    ~RobocopyProcessSupervisor() override;
    bool start(QList<RobocopyInvocation>, RobocopyProfile, const QString &logPath,
               bool promptConflicts = false);
    void resolveConflict(quint64 token, int choice);
    void cancel();
    void stopAndWait();
    void setPaused(bool);
    QList<FileEntry> takeEntries(int maximum = 2000);
    quint64 omittedEntries() const {
        return m_omitted.load();
    }
    bool busy() const {
        return m_running.load();
    }
    void acknowledgeTelemetry() {
        m_telemetryPending = false;
    }
    static QString decodeExitCode(quint32);
  signals:
    void phaseChanged(QString);
    void scanned(quint64 bytes, quint64 files, QStringList warnings);
    void telemetry(QVariantMap sample);
    void finished(bool success, bool cancelled, QString message);
    void fileConflict(quint64 token, QString source, QString destination);

  private:
    void run(const QList<RobocopyInvocation> &, const RobocopyProfile &, const QString &, bool);
    int askConflict(const QString &source, const QString &destination);
    void enqueue(FileEntry);
    std::atomic_bool m_cancel = false, m_pause = false, m_running = false;
    std::atomic_bool m_telemetryPending = false;
    std::atomic<quint64> m_omitted = 0;
    std::thread m_worker;
    std::mutex m_queueMutex;
    std::deque<FileEntry> m_entries;
    size_t m_queuedBytes = 0;
    std::mutex m_conflictMutex;
    std::condition_variable m_conflictReady;
    quint64 m_nextConflict = 0, m_waitingConflict = 0;
    int m_conflictAnswer = -1;
};
} // namespace strata
