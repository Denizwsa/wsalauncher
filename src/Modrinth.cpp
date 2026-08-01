#include "Modrinth.h"

#include "Constants.h"
#include "Download.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace modrinth {

static bool fetchJson(QNetworkAccessManager &nam, const QUrl &url, QJsonDocument *out,
                      QString *errorOut) {
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Wsalauncher/0.2"));
    QNetworkReply *reply = nam.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if (reply->error() != QNetworkReply::NoError) {
        if (errorOut)
            *errorOut = reply->errorString();
        reply->deleteLater();
        return false;
    }
    QJsonParseError err;
    *out = QJsonDocument::fromJson(reply->readAll(), &err);
    reply->deleteLater();
    if (err.error != QJsonParseError::NoError) {
        if (errorOut)
            *errorOut = err.errorString();
        return false;
    }
    return true;
}

bool installMod(QNetworkAccessManager &nam, const QString &slug, const QString &modsDir,
                QString *errorOut) {
    const QString url = QStringLiteral(
        "https://api.modrinth.com/v2/project/%1/version?game_versions=[\"%2\"]&loaders=[\"fabric\"]")
                          .arg(slug, constants::kMcVersion);
    QJsonDocument doc;
    if (!fetchJson(nam, QUrl(url), &doc, errorOut))
        return false;
    if (!doc.isArray() || doc.array().isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("No version for %1").arg(slug);
        return false;
    }

    // Prefer non-alpha builds (Sodium alpha crashes Video Settings on 26.2).
    QJsonObject ver;
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        const QString vn = o.value(QStringLiteral("version_number")).toString();
        if (!vn.contains(QStringLiteral("alpha"), Qt::CaseInsensitive)) {
            ver = o;
            break;
        }
    }
    if (ver.isEmpty())
        ver = doc.array().first().toObject();

    QJsonArray files = ver.value(QStringLiteral("files")).toArray();
    if (files.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("No files for %1").arg(slug);
        return false;
    }
    QJsonObject fileObj;
    for (const QJsonValue &v : files) {
        QJsonObject f = v.toObject();
        if (f.value(QStringLiteral("primary")).toBool()) {
            fileObj = f;
            break;
        }
    }
    if (fileObj.isEmpty())
        fileObj = files.first().toObject();

    QString fileUrl = fileObj.value(QStringLiteral("url")).toString();
    QString filename = fileObj.value(QStringLiteral("filename")).toString();
    QDir().mkpath(modsDir);

    // Remove older jars for this slug so only one version remains.
    const QDir dir(modsDir);
    const QStringList old = dir.entryList({slug + QStringLiteral("*.jar"),
                                           QStringLiteral("*%1*.jar").arg(slug)},
                                          QDir::Files);
    for (const QString &name : old) {
        if (name.compare(filename, Qt::CaseInsensitive) != 0)
            QFile::remove(modsDir + QLatin1Char('/') + name);
    }
    // Also catch sodium-fabric-* naming
    if (slug == QStringLiteral("sodium")) {
        for (const QString &name : dir.entryList({QStringLiteral("sodium*.jar")}, QDir::Files)) {
            if (name.compare(filename, Qt::CaseInsensitive) != 0)
                QFile::remove(modsDir + QLatin1Char('/') + name);
        }
    }

    const QString dest = modsDir + QLatin1Char('/') + filename;
    if (QFile::exists(dest))
        return true;
    return download::fetchToFile(nam, QUrl(fileUrl), dest, errorOut);
}

} // namespace modrinth
