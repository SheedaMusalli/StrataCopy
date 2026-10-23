#include "UpdateChecker.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QVersionNumber>

namespace strata {
UpdateChecker::UpdateChecker(QString version, QObject *parent, int timeoutMs)
    : QObject(parent), m_currentVersion(std::move(version)) {
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(timeoutMs);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (m_reply) {
            m_error = "The update check timed out. Please try again.";
            m_reply->abort();
        }
    });
}
UpdateChecker::~UpdateChecker() {
    m_timeout.stop();
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
    }
    // The network manager owns and destroys outstanding replies on shutdown.
}
UpdateChecker::Result UpdateChecker::evaluateRelease(const QByteArray &body, const QString &current,
                                                     QString &message) {
    const auto document = QJsonDocument::fromJson(body);
    const auto release = document.object();
    auto tag = release.value("tag_name").toString();
    static const QRegularExpression stableVersion("^[vV]?[0-9]+\\.[0-9]+\\.[0-9]+$");
    if (!document.isObject() || release.value("draft").toBool(true) ||
        release.value("prerelease").toBool(true) || !stableVersion.match(tag).hasMatch() ||
        !stableVersion.match(current).hasMatch()) {
        message = "GitHub returned an unrecognized release version. Please try again later.";
        return Error;
    }
    if (tag.startsWith('v', Qt::CaseInsensitive))
        tag.remove(0, 1);
    auto installed = current;
    if (installed.startsWith('v', Qt::CaseInsensitive))
        installed.remove(0, 1);
    const auto latestVersion = QVersionNumber::fromString(tag);
    const auto installedVersion = QVersionNumber::fromString(installed);
    if (latestVersion.segmentCount() != 3 || installedVersion.segmentCount() != 3) {
        message = "The release version could not be compared. Please try again later.";
        return Error;
    }
    if (latestVersion > installedVersion) {
        message = "StrataCopy v" + tag + " is available. Opening the latest release.";
        return UpdateAvailable;
    }
    message = "StrataCopy is already updated to the latest version (installed v" + installed + ").";
    return UpToDate;
}
void UpdateChecker::check(const QUrl &endpoint) {
    if (checking())
        return;
    m_body.clear();
    m_error.clear();
    QNetworkRequest request(endpoint);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "StrataCopy/" + m_currentVersion.toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    auto *reply = m_network.get(request);
    m_reply = reply;
    reply->setReadBufferSize(maximumResponse + 1);
    connect(reply, &QIODevice::readyRead, this, [this, reply] {
        m_body += reply->read(maximumResponse + 1 - m_body.size());
        if (m_body.size() > maximumResponse) {
            m_error = "The update response was too large. Please try again later.";
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_timeout.stop();
        QString message = m_error;
        Result result = Error;
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (message.isEmpty()) {
            if (status == 403 || status == 429)
                message = "GitHub is limiting update requests. Please try again later.";
            else if (reply->error() != QNetworkReply::NoError || status != 200)
                message = "Could not check for updates. Check your connection and try again.";
            else
                result = evaluateRelease(m_body, m_currentVersion, message);
        }
        m_reply = nullptr;
        reply->deleteLater();
        m_body.clear();
        emit checkingChanged();
        emit finished(result, message);
    });
    m_timeout.start();
    emit checkingChanged();
}
} // namespace strata
