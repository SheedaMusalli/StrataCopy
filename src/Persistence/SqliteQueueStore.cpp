#include "SqliteQueueStore.hpp"
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
namespace strata {
SqliteQueueStore::SqliteQueueStore() : m_connection(QUuid::createUuid().toString()) {}
SqliteQueueStore::~SqliteQueueStore() {
    if (m_db.isValid())
        m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connection);
}
bool SqliteQueueStore::open(const QString &path) {
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connection);
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        m_error = m_db.lastError().text();
        return false;
    }
    QSqlQuery q(m_db);
    for (const auto &sql :
         QStringList{"PRAGMA journal_mode=WAL", "PRAGMA synchronous=NORMAL", "PRAGMA busy_timeout=5000",
                     "CREATE TABLE IF NOT EXISTS state(key TEXT PRIMARY KEY, value TEXT NOT NULL)",
                     "CREATE TABLE IF NOT EXISTS runs(id INTEGER PRIMARY KEY, started TEXT DEFAULT "
                     "(datetime('now')), state TEXT, payload TEXT, log TEXT, message TEXT)",
                     "CREATE TABLE IF NOT EXISTS presets(name TEXT PRIMARY KEY, payload TEXT NOT NULL)",
                     "UPDATE runs SET state='Interrupted',message='App exited before completion. Review the "
                     "saved queue before retrying.' WHERE state='Running'"})
        if (!q.exec(sql)) {
            m_error = q.lastError().text();
            return false;
        }
    return true;
}
bool SqliteQueueStore::savePending(const QJsonObject &object) {
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO state(key,value) VALUES('pending',?)");
    q.addBindValue(QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
    return q.exec();
}
QJsonObject SqliteQueueStore::pending() const {
    QSqlQuery q(m_db);
    if (q.exec("SELECT value FROM state WHERE key='pending'") && q.next())
        return QJsonDocument::fromJson(q.value(0).toByteArray()).object();
    return {};
}
qint64 SqliteQueueStore::beginRun(const QJsonObject &payload, const QString &log) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO runs(state,payload,log) VALUES('Running',?,?)");
    q.addBindValue(QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)));
    q.addBindValue(log);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}
bool SqliteQueueStore::finishRun(qint64 id, const QString &state, const QString &message) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE runs SET state=?, message=? WHERE id=?");
    q.addBindValue(state);
    q.addBindValue(message);
    q.addBindValue(id);
    return q.exec();
}
QVariantList SqliteQueueStore::history() const {
    QVariantList out;
    QSqlQuery q(m_db);
    q.exec("SELECT id,started,state,log,message,payload FROM runs ORDER BY id DESC LIMIT 100");
    while (q.next())
        out << QVariantMap{
            {"id", q.value(0)},
            {"started", q.value(1)},
            {"state", q.value(2)},
            {"log", q.value(3)},
            {"message", q.value(4)},
            {"payload", QJsonDocument::fromJson(q.value(5).toByteArray()).object().toVariantMap()}};
    return out;
}
bool SqliteQueueStore::savePreset(const QString &name, const QJsonObject &payload) {
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO presets(name,payload) VALUES(?,?)");
    q.addBindValue(name);
    q.addBindValue(QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)));
    return q.exec();
}
QVariantList SqliteQueueStore::presets() const {
    QVariantList out;
    QSqlQuery q(m_db);
    q.exec("SELECT name,payload FROM presets ORDER BY name");
    while (q.next())
        out << QVariantMap{
            {"name", q.value(0)},
            {"values", QJsonDocument::fromJson(q.value(1).toByteArray()).object().toVariantMap()}};
    return out;
}
} // namespace strata
