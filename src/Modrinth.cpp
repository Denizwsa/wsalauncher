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
#include <QRegularExpression>
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

static QStringList gameVersionsForSlug(const QString &slug) {
    if (slug == QStringLiteral("vulkanmod")) {
        return {QString::fromUtf8(constants::kMcVersion), QStringLiteral("26.1.2"),
                QStringLiteral("26.1")};
    }
    return {QString::fromUtf8(constants::kMcVersion)};
}

static bool fetchModrinthVersions(QNetworkAccessManager &nam, const QString &slug,
                                  const QString &gameVersion, QJsonArray *out,
                                  QString *errorOut) {
    const QString url = QStringLiteral(
        "https://api.modrinth.com/v2/project/%1/version?game_versions=[\"%2\"]&loaders=[\"fabric\"]")
                          .arg(slug, gameVersion);
    QJsonDocument doc;
    if (!fetchJson(nam, QUrl(url), &doc, errorOut))
        return false;
    if (!doc.isArray()) {
        if (errorOut)
            *errorOut = QStringLiteral("Bad Modrinth response for %1").arg(slug);
        return false;
    }
    *out = doc.array();
    return true;
}

void removeConflictingRenderMods(const QString &modsDir) {
    const QDir dir(modsDir);
    if (!dir.exists())
        return;
    static const QRegularExpression patterns[] = {
        QRegularExpression(QStringLiteral("^sodium.*\\.jar$"), QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(QStringLiteral("^iris.*\\.jar$"), QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(QStringLiteral("^indium.*\\.jar$"), QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(QStringLiteral("^nvidium.*\\.jar$"), QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(QStringLiteral("^optifabric.*\\.jar$"), QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(QStringLiteral("^canvas.*\\.jar$"), QRegularExpression::CaseInsensitiveOption),
    };
    for (const QString &name : dir.entryList(QDir::Files)) {
        for (const QRegularExpression &re : patterns) {
            if (re.match(name).hasMatch()) {
                QFile::remove(modsDir + QLatin1Char('/') + name);
                break;
            }
        }
    }
}

bool installMod(QNetworkAccessManager &nam, const QString &slug, const QString &modsDir,
                QString *errorOut) {
    if (slug == QStringLiteral("vulkanmod"))
        removeConflictingRenderMods(modsDir);

    QJsonArray versions;
    QString fetchErr;
    for (const QString &gv : gameVersionsForSlug(slug)) {
        QJsonArray candidate;
        if (!fetchModrinthVersions(nam, slug, gv, &candidate, &fetchErr))
            return false;
        if (!candidate.isEmpty()) {
            versions = candidate;
            break;
        }
    }
    if (versions.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("No Fabric version for %1 on MC %2")
                            .arg(slug, QString::fromUtf8(constants::kMcVersion));
        return false;
    }

    // Prefer stable releases (skip alpha/beta when a release exists).
    QJsonObject ver;
    for (const QJsonValue &v : versions) {
        const QJsonObject o = v.toObject();
        const QString vn = o.value(QStringLiteral("version_number")).toString();
        const QString vt = o.value(QStringLiteral("version_type")).toString();
        if (vt == QStringLiteral("release") &&
            !vn.contains(QStringLiteral("alpha"), Qt::CaseInsensitive)) {
            ver = o;
            break;
        }
    }
    if (ver.isEmpty()) {
        for (const QJsonValue &v : versions) {
            const QJsonObject o = v.toObject();
            const QString vn = o.value(QStringLiteral("version_number")).toString();
            if (!vn.contains(QStringLiteral("alpha"), Qt::CaseInsensitive)) {
                ver = o;
                break;
            }
        }
    }
    if (ver.isEmpty())
        ver = versions.first().toObject();

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

    const QDir dir(modsDir);
    const QStringList old = dir.entryList({slug + QStringLiteral("*.jar"),
                                           QStringLiteral("*%1*.jar").arg(slug)},
                                          QDir::Files);
    for (const QString &name : old) {
        if (name.compare(filename, Qt::CaseInsensitive) != 0)
            QFile::remove(modsDir + QLatin1Char('/') + name);
    }
    if (slug == QStringLiteral("vulkanmod")) {
        for (const QString &name :
             dir.entryList({QStringLiteral("VulkanMod*.jar"), QStringLiteral("vulkanmod*.jar")},
                           QDir::Files)) {
            if (name.compare(filename, Qt::CaseInsensitive) != 0)
                QFile::remove(modsDir + QLatin1Char('/') + name);
        }
    }

    const QString dest = modsDir + QLatin1Char('/') + filename;
    if (QFile::exists(dest))
        return true;
    if (!download::fetchToFile(nam, QUrl(fileUrl), dest, errorOut))
        return false;
    return true;
}

} // namespace modrinth
