#include "Releases.h"

#include "WsamiawApi.h"

#include "Constants.h"
#include "Download.h"
#include "Paths.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace releases {

static QByteArray httpGet(QNetworkAccessManager &nam, const QUrl &url, QString *errorOut) {
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
        return {};
    }
    const QByteArray data = reply->readAll();
    reply->deleteLater();
    return data;
}

QString tagFilePath() {
    return paths::dataRoot() + QStringLiteral("/wsamiaw-release-tag.txt");
}

QString installedTag() {
    QFile f(tagFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll()).trimmed();
}

void setInstalledTag(const QString &tag) {
    QFile f(tagFilePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(tag.toUtf8());
}

LatestRelease fetchLatest(QNetworkAccessManager &nam, QString *errorOut) {
    LatestRelease r;
    const QByteArray data =
        httpGet(nam, QUrl(QString::fromUtf8(constants::kGithubLatestReleaseApi)), errorOut);
    if (data.isEmpty())
        return r;
    const QJsonObject root = QJsonDocument::fromJson(data).object();
    r.tag = root.value(QStringLiteral("tag_name")).toString();
    const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &v : assets) {
        const QJsonObject a = v.toObject();
        const QString name = a.value(QStringLiteral("name")).toString();
        if (name.endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive)) {
            r.jarFileName = name;
            r.jarUrl = a.value(QStringLiteral("browser_download_url")).toString();
            r.valid = !r.tag.isEmpty() && !r.jarUrl.isEmpty();
            break;
        }
    }
    if (!r.valid && errorOut)
        *errorOut = QStringLiteral("No .jar asset on latest release");
    return r;
}

static void removeOldWsamiawJars(const QString &modsDir) {
    const QDir dir(modsDir);
    const QStringList jars =
        dir.entryList({QStringLiteral("Wsamiaw*.jar"), QStringLiteral("wsamiaw*.jar")},
                      QDir::Files);
    for (const QString &name : jars)
        QFile::remove(modsDir + QLatin1Char('/') + name);
}

bool syncWsamiawJar(QNetworkAccessManager &nam, const QString &modsDir,
                    const QString &fallbackLocalJar, const wsamiawApi::ClientAuth &auth,
                    installer::LogFn log, QString *errorOut) {
    QDir().mkpath(modsDir);

    if (auth.valid()) {
        const wsamiawApi::DownloadInfo dl = wsamiawApi::fetchDownloadInfo(nam, auth);
        if (!dl.ok) {
            log(QStringLiteral("Account download failed: %1").arg(dl.error));
            if (errorOut)
                *errorOut = dl.error.isEmpty()
                                ? QStringLiteral("Check license key + HWID on site (Account tab)")
                                : dl.error;
            return false;
        }
        QString fileName = dl.file;
        if (fileName.isEmpty())
            fileName = QUrl(dl.url).fileName();
        if (fileName.isEmpty()) {
            if (errorOut)
                *errorOut = QStringLiteral("Invalid download URL");
            return false;
        }
        const QString tag = dl.version.isEmpty() ? QStringLiteral("site") : dl.version;
        const QString dest = modsDir + QLatin1Char('/') + fileName;
        if (installedTag() == tag && QFile::exists(dest)) {
            log(QStringLiteral("Wsamiaw already up to date (%1)").arg(tag));
            return true;
        }
        log(QStringLiteral("Downloading Wsamiaw %1 from site…").arg(tag));
        const QString cachePath = paths::cacheDir() + QLatin1Char('/') + fileName;
        if (!download::fetchToFile(nam, QUrl(dl.url), cachePath, errorOut))
            return false;
        removeOldWsamiawJars(modsDir);
        if (QFile::exists(dest))
            QFile::remove(dest);
        if (!QFile::copy(cachePath, dest)) {
            if (errorOut)
                *errorOut = QStringLiteral("Failed to install Wsamiaw JAR into mods");
            return false;
        }
        setInstalledTag(tag);
        log(QStringLiteral("Installed %1 (site release %2)").arg(fileName, tag));
        return false;
    }

    QString err;
    const LatestRelease latest = fetchLatest(nam, &err);
    if (!latest.valid) {
        log(QStringLiteral("Release check failed: %1").arg(err));
        if (!fallbackLocalJar.isEmpty() && QFile::exists(fallbackLocalJar)) {
            removeOldWsamiawJars(modsDir);
            const QString dest = modsDir + QLatin1Char('/') + QFileInfo(fallbackLocalJar).fileName();
            if (!QFile::copy(fallbackLocalJar, dest)) {
                if (errorOut)
                    *errorOut = QStringLiteral("Failed to copy local Wsamiaw JAR");
                return false;
            }
            log(QStringLiteral("Copied local %1").arg(QFileInfo(fallbackLocalJar).fileName()));
            return false;
        }
        if (errorOut)
            *errorOut = err;
        return false;
    }

    if (installedTag() == latest.tag) {
        const QString dest = modsDir + QLatin1Char('/') + latest.jarFileName;
        if (QFile::exists(dest)) {
            log(QStringLiteral("Wsamiaw already up to date (%1)").arg(latest.tag));
            return true;
        }
        log(QStringLiteral("Tag %1 recorded but JAR missing — re-downloading…").arg(latest.tag));
    } else {
        log(QStringLiteral("Updating Wsamiaw to %1…").arg(latest.tag));
    }

    const QString cachePath =
        paths::cacheDir() + QLatin1Char('/') + latest.jarFileName;
    if (!download::fetchToFile(nam, QUrl(latest.jarUrl), cachePath, errorOut))
        return false;

    removeOldWsamiawJars(modsDir);
    const QString dest = modsDir + QLatin1Char('/') + latest.jarFileName;
    if (QFile::exists(dest))
        QFile::remove(dest);
    if (!QFile::copy(cachePath, dest)) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to install Wsamiaw JAR into mods");
        return false;
    }
    setInstalledTag(latest.tag);
    log(QStringLiteral("Installed %1 (%2)").arg(latest.jarFileName, latest.tag));
    return false;
}

} // namespace releases
