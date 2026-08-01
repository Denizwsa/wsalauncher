#include "Bootstrap.h"

#include "Download.h"
#include "Platform.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QUuid>

namespace bootstrap {

static QByteArray httpGet(QNetworkAccessManager &nam, const QUrl &url, QString *errorOut) {
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Wsalauncher/0.3"));
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

bool ensureGameEnvironment(const QString &minecraftDir, installer::LogFn log, QString *errorOut) {
    const QStringList subdirs = {QStringLiteral("mods"), QStringLiteral("versions"),
                                 QStringLiteral("libraries"), QStringLiteral("assets"),
                                 QStringLiteral("resourcepacks"), QStringLiteral("saves")};
    for (const QString &name : subdirs) {
        if (!QDir().mkpath(minecraftDir + QLatin1Char('/') + name)) {
            if (errorOut)
                *errorOut = QStringLiteral("Failed to create %1").arg(name);
            return false;
        }
    }

    const QString profilePath = minecraftDir + QStringLiteral("/launcher_profiles.json");
    if (QFile::exists(profilePath))
        return true;

    QJsonObject profile;
    profile[QStringLiteral("name")] = QStringLiteral("Wsalauncher");
    profile[QStringLiteral("type")] = QStringLiteral("custom");
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    profile[QStringLiteral("created")] = now;
    profile[QStringLiteral("lastUsed")] = now;

    QJsonObject profiles;
    profiles[QStringLiteral("wsalauncher")] = profile;

    QJsonObject root;
    root[QStringLiteral("profiles")] = profiles;
    root[QStringLiteral("selectedProfile")] = QStringLiteral("wsalauncher");
    root[QStringLiteral("clientToken")] =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    root[QStringLiteral("authenticationDatabase")] = QJsonObject();
    QJsonObject launcherVersion;
    launcherVersion[QStringLiteral("name")] = QStringLiteral("Wsalauncher");
    launcherVersion[QStringLiteral("format")] = 21;
    root[QStringLiteral("launcherVersion")] = launcherVersion;

    QFile f(profilePath);
    if (!f.open(QIODevice::WriteOnly)) {
        if (errorOut)
            *errorOut = f.errorString();
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    log(QStringLiteral("Initialized Wsalauncher game directory at %1").arg(minecraftDir));
    return true;
}

bool ensureVanillaVersion(QNetworkAccessManager &nam, const QString &minecraftDir,
                          const QString &mcVersion, installer::LogFn log, QString *errorOut) {
    const QString versionDir =
        minecraftDir + QStringLiteral("/versions/") + mcVersion;
    const QString versionJsonPath = versionDir + QLatin1Char('/') + mcVersion + QStringLiteral(".json");
    const QString clientJarPath = versionDir + QLatin1Char('/') + mcVersion + QStringLiteral(".jar");

    if (QFile::exists(versionJsonPath) && QFile::exists(clientJarPath))
        return true;

    log(QStringLiteral("Downloading Minecraft %1 metadata…").arg(mcVersion));
    const QByteArray manifestData = httpGet(
        nam, QUrl(QStringLiteral("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json")),
        errorOut);
    if (manifestData.isEmpty())
        return false;

    QString versionUrl;
    for (const QJsonValue &v : QJsonDocument::fromJson(manifestData).object()
                                    .value(QStringLiteral("versions"))
                                    .toArray()) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("id")).toString() == mcVersion) {
            versionUrl = o.value(QStringLiteral("url")).toString();
            break;
        }
    }
    if (versionUrl.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Version %1 not in Mojang manifest").arg(mcVersion);
        return false;
    }

    const QByteArray versionData = httpGet(nam, QUrl(versionUrl), errorOut);
    if (versionData.isEmpty())
        return false;

    QDir().mkpath(versionDir);
    QFile jsonFile(versionJsonPath);
    if (!jsonFile.open(QIODevice::WriteOnly)) {
        if (errorOut)
            *errorOut = jsonFile.errorString();
        return false;
    }
    jsonFile.write(versionData);

    const QJsonObject versionRoot = QJsonDocument::fromJson(versionData).object();
    const QJsonObject client =
        versionRoot.value(QStringLiteral("downloads")).toObject().value(QStringLiteral("client")).toObject();
    const QString jarUrl = client.value(QStringLiteral("url")).toString();
    if (jarUrl.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("No client download in version json");
        return false;
    }

    log(QStringLiteral("Downloading Minecraft %1 client…").arg(mcVersion));
    if (!download::fetchToFile(nam, QUrl(jarUrl), clientJarPath, errorOut))
        return false;

    log(QStringLiteral("Minecraft %1 ready.").arg(mcVersion));
    return true;
}

bool ensureAssets(QNetworkAccessManager &nam, const QString &minecraftDir,
                  const QJsonObject &assetIndex, installer::LogFn log, QString *errorOut) {
    const QString indexId = assetIndex.value(QStringLiteral("id")).toString();
    const QString indexUrl = assetIndex.value(QStringLiteral("url")).toString();
    if (indexId.isEmpty() || indexUrl.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Missing asset index metadata");
        return false;
    }

    const QString indexesDir = minecraftDir + QStringLiteral("/assets/indexes");
    QDir().mkpath(indexesDir);
    const QString indexPath = indexesDir + QLatin1Char('/') + indexId + QStringLiteral(".json");

    if (!QFile::exists(indexPath)) {
        log(QStringLiteral("Downloading asset index %1…").arg(indexId));
        if (!download::fetchToFile(nam, QUrl(indexUrl), indexPath, errorOut))
            return false;
    }

    QFile indexFile(indexPath);
    if (!indexFile.open(QIODevice::ReadOnly)) {
        if (errorOut)
            *errorOut = indexFile.errorString();
        return false;
    }
    const QJsonObject indexRoot = QJsonDocument::fromJson(indexFile.readAll()).object();
    const QJsonObject objects = indexRoot.value(QStringLiteral("objects")).toObject();

    int missing = 0;
    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
        const QString hash = it.value().toObject().value(QStringLiteral("hash")).toString();
        const QString objPath =
            minecraftDir + QStringLiteral("/assets/objects/") + hash.left(2) + QLatin1Char('/')
            + hash;
        if (!QFile::exists(objPath))
            ++missing;
    }

    if (missing == 0)
        return true;

    log(QStringLiteral("Downloading %1 asset file(s)…").arg(missing));
    int done = 0;
    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
        const QString hash = it.value().toObject().value(QStringLiteral("hash")).toString();
        if (hash.size() < 2)
            continue;
        const QString objPath =
            minecraftDir + QStringLiteral("/assets/objects/") + hash.left(2) + QLatin1Char('/')
            + hash;
        if (QFile::exists(objPath))
            continue;
        QDir().mkpath(QFileInfo(objPath).absolutePath());
        const QUrl url(QStringLiteral("https://resources.download.minecraft.net/") + hash.left(2)
                     + QLatin1Char('/') + hash);
        if (!download::fetchToFile(nam, url, objPath, errorOut))
            return false;
        ++done;
        if (done % 100 == 0)
            log(QStringLiteral("Assets… %1 / %2").arg(done).arg(missing));
    }
    log(QStringLiteral("Assets ready."));
    return true;
}

bool prepareNatives(const QString &javaPath, const QString &minecraftDir,
                    const QStringList &nativeJarPaths, installer::LogFn log, QString *errorOut) {
    const QString nativesDir = minecraftDir + QStringLiteral("/natives");
    QDir natives(nativesDir);
    natives.removeRecursively();
    QDir().mkpath(nativesDir);

    const QString jarTool = platform::jarToolForJava(javaPath);
    if (jarTool.isEmpty() || !QFile::exists(jarTool)) {
        log(QStringLiteral("WARN: jar tool not found; skipping native extract"));
        return true;
    }

    for (const QString &jarPath : nativeJarPaths) {
        if (!jarPath.contains(QStringLiteral("natives-")))
            continue;
        QProcess proc;
        proc.setWorkingDirectory(nativesDir);
        proc.setProgram(jarTool);
        proc.setArguments({QStringLiteral("xf"), jarPath});
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.start();
        if (!proc.waitForFinished(120000)) {
            if (errorOut)
                *errorOut = QStringLiteral("Native extract timed out: %1").arg(jarPath);
            return false;
        }
        if (proc.exitCode() != 0) {
            if (errorOut)
                *errorOut = QStringLiteral("Native extract failed: %1").arg(jarPath);
            return false;
        }
    }
    log(QStringLiteral("Natives prepared."));
    return true;
}

} // namespace bootstrap
