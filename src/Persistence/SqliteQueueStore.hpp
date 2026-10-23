#pragma once
#include <QJsonObject>
#include <QSqlDatabase>
#include <QVariantList>
namespace strata {
class SqliteQueueStore {
  public:
    SqliteQueueStore();
    ~SqliteQueueStore();
    SqliteQueueStore(const SqliteQueueStore &) = delete;
    SqliteQueueStore &operator=(const SqliteQueueStore &) = delete;
    bool open(const QString &path);
    QString error() const {
        return m_error;
    }
    bool savePending(const QJsonObject &);
    QJsonObject pending() const;
    qint64 beginRun(const QJsonObject &, const QString &log);
    bool finishRun(qint64, const QString &state, const QString &message);
    QVariantList history() const;
    bool savePreset(const QString &, const QJsonObject &);
    QVariantList presets() const;

  private:
    QSqlDatabase m_db;
    QString m_connection, m_error;
};
} // namespace strata
