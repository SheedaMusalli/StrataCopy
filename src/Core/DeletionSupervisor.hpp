#pragma once
#include <QObject>
#include <QStringList>
#include <atomic>
#include <thread>
namespace strata {
class DeletionSupervisor : public QObject {
    Q_OBJECT
  public:
    using QObject::QObject;
    ~DeletionSupervisor() override {
        stopAndWait();
    }
    bool start(const QStringList &paths, bool permanent);
    void cancel() {
        m_cancel = true;
    }
    void stopAndWait() {
        cancel();
        if (m_worker.joinable())
            m_worker.join();
    }
    bool cancelled() const {
        return m_cancel.load();
    }
    void report(quint64 done, quint64 total, const QString &path);
  signals:
    void progress(quint64 done, quint64 total, QString path);
    void finished(bool success, bool cancelled, QString message);

  private:
    std::atomic_bool m_cancel = false, m_running = false, m_progressPending = false;
    std::thread m_worker;
};
} // namespace strata
