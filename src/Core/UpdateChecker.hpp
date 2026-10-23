#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>

namespace strata {
class UpdateChecker : public QObject {
    Q_OBJECT
  public:
    enum Result { UpToDate, UpdateAvailable, Error };
    Q_ENUM(Result)
    explicit UpdateChecker(QString currentVersion, QObject *parent = nullptr, int timeoutMs = 15000);
    ~UpdateChecker() override;
    bool checking() const {
        return !m_reply.isNull();
    }
    void check(
        const QUrl &endpoint = QUrl("https://api.github.com/repos/SheedaMusalli/StrataCopy/releases/latest"));
    static Result evaluateRelease(const QByteArray &, const QString &currentVersion, QString &message);
  signals:
    void checkingChanged();
    void finished(strata::UpdateChecker::Result result, QString message);

  private:
    static constexpr qsizetype maximumResponse = 256 * 1024;
    QString m_currentVersion, m_error;
    QByteArray m_body;
    QNetworkAccessManager m_network;
    QTimer m_timeout;
    QPointer<QNetworkReply> m_reply;
};
} // namespace strata
