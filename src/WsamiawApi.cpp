#include "WsamiawApi.h"

#include "Constants.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace wsamiawApi {

static QByteArray postJson(QNetworkAccessManager &nam, const QString &path, const QJsonObject &body,
                           const QString &bearer, const ClientAuth *keyHwid, QString *errorOut) {
    QNetworkRequest req(QUrl(QString::fromUtf8(constants::kApiBase) + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Wsalauncher/0.3"));
    if (!bearer.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + bearer).toUtf8());
    if (keyHwid && keyHwid->valid()) {
        req.setRawHeader("X-License-Key", keyHwid->licenseKey.toUtf8());
        req.setRawHeader("X-Hwid", keyHwid->hwid.toUtf8());
    }
    QNetworkReply *reply = nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const QByteArray data = reply->readAll();
    const bool netErr = reply->error() != QNetworkReply::NoError;
    const QString netErrStr = reply->errorString();
    reply->deleteLater();
    if (netErr && data.isEmpty()) {
        if (errorOut)
            *errorOut = netErrStr;
        return {};
    }
    return data;
}

static QByteArray getAuth(QNetworkAccessManager &nam, const QString &path, const ClientAuth &auth,
                          QString *errorOut) {
    QNetworkRequest req(QUrl(QString::fromUtf8(constants::kApiBase) + path));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Wsalauncher/0.3"));
    if (auth.valid()) {
        req.setRawHeader("X-License-Key", auth.licenseKey.toUtf8());
        req.setRawHeader("X-Hwid", auth.hwid.toUtf8());
    }
    QNetworkReply *reply = nam.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const QByteArray data = reply->readAll();
    const bool netErr = reply->error() != QNetworkReply::NoError;
    const QString netErrStr = reply->errorString();
    reply->deleteLater();
    if (netErr && data.isEmpty()) {
        if (errorOut)
            *errorOut = netErrStr;
        return {};
    }
    return data;
}

static QString apiErrorMessage(const QJsonObject &o, const QString &fallback) {
    return o.value(QStringLiteral("error")).toString(
        o.value(QStringLiteral("reason")).toString(fallback));
}

QString fetchLaunchToken(QNetworkAccessManager &nam, const ClientAuth &auth, QString *errorOut) {
    if (!auth.valid()) {
        if (errorOut)
            *errorOut = QStringLiteral("missing_key_or_hwid");
        return {};
    }
    QJsonObject body;
    body.insert(QStringLiteral("key"), auth.licenseKey);
    body.insert(QStringLiteral("hwid"), auth.hwid);
    QString err;
    const QByteArray data =
        postJson(nam, QStringLiteral("/client/launch-token"), body, {}, &auth, &err);
    if (data.isEmpty()) {
        if (errorOut)
            *errorOut = err.isEmpty() ? QStringLiteral("launch_token_failed") : err;
        return {};
    }
    const QJsonObject o = QJsonDocument::fromJson(data).object();
    if (o.contains(QStringLiteral("error"))) {
        if (errorOut)
            *errorOut = apiErrorMessage(o, QStringLiteral("launch_denied"));
        return {};
    }
    return o.value(QStringLiteral("launch_token")).toString();
}

DownloadInfo fetchDownloadInfo(QNetworkAccessManager &nam, const ClientAuth &auth) {
    DownloadInfo info;
    if (!auth.valid()) {
        info.error = QStringLiteral("missing_key_or_hwid");
        return info;
    }
    QString err;
    const QByteArray data = getAuth(nam, QStringLiteral("/download/url"), auth, &err);
    if (data.isEmpty()) {
        info.error = err.isEmpty() ? QStringLiteral("download_denied") : err;
        return info;
    }
    const QJsonObject o = QJsonDocument::fromJson(data).object();
    if (o.contains(QStringLiteral("error"))) {
        info.error = apiErrorMessage(o, QStringLiteral("download_denied"));
        return info;
    }
    info.url = o.value(QStringLiteral("url")).toString();
    info.version = o.value(QStringLiteral("version")).toString();
    info.file = o.value(QStringLiteral("file")).toString();
    info.sha256 = o.value(QStringLiteral("sha256")).toString();
    info.ok = !info.url.isEmpty();
    if (!info.ok)
        info.error = QStringLiteral("invalid_download_url");
    return info;
}

bool verifyClient(QNetworkAccessManager &nam, const ClientAuth &auth, QString *reasonOut) {
    if (!auth.valid()) {
        if (reasonOut)
            *reasonOut = QStringLiteral("missing_key_or_hwid");
        return false;
    }
    QJsonObject body;
    body.insert(QStringLiteral("key"), auth.licenseKey);
    body.insert(QStringLiteral("hwid"), auth.hwid);
    QString err;
    const QByteArray data = postJson(nam, QStringLiteral("/verify"), body, {}, &auth, &err);
    if (data.isEmpty()) {
        if (reasonOut)
            *reasonOut = err.isEmpty() ? QStringLiteral("network") : err;
        return false;
    }
    const QJsonObject o = QJsonDocument::fromJson(data).object();
    if (o.value(QStringLiteral("valid")).toBool()) {
        return true;
    }
    if (reasonOut) {
        *reasonOut = o.value(QStringLiteral("reason")).toString(
            apiErrorMessage(o, QStringLiteral("denied")));
    }
    return false;
}

} // namespace wsamiawApi
