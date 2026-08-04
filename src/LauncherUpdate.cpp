#include "LauncherUpdate.h"

#include "Constants.h"
#include "Download.h"
#include "Paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QTemporaryDir>
#include <QUrl>
#include <QVersionNumber>
#include <QWidget>

namespace launcherUpdate {

QString currentVersion() {
#ifdef WSALAUNCHER_VERSION
    return QStringLiteral(WSALAUNCHER_VERSION);
#else
    return QStringLiteral("0.3.8");
#endif
}

static QByteArray httpGet(QNetworkAccessManager &nam, const QUrl &url, QString *errorOut) {
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Wsalauncher/") + currentVersion());
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

Manifest fetchManifest(QNetworkAccessManager &nam, QString *errorOut) {
    Manifest m;
    const QUrl url(QString::fromUtf8(constants::kLauncherManifestUrl));
    const QByteArray data = httpGet(nam, url, errorOut);
    if (data.isEmpty())
        return m;
    const QJsonObject root = QJsonDocument::fromJson(data).object();
    m.version = root.value(QStringLiteral("version")).toString();
#if defined(Q_OS_WIN)
    const QJsonObject plat = root.value(QStringLiteral("windows")).toObject();
#else
    const QJsonObject plat = root.value(QStringLiteral("linux")).toObject();
#endif
    m.assetFileName = plat.value(QStringLiteral("file")).toString();
    m.valid = !m.version.isEmpty() && !m.assetFileName.isEmpty();
    return m;
}

bool isNewerVersion(const QString &remote, const QString &local) {
    const QVersionNumber r = QVersionNumber::fromString(remote);
    const QVersionNumber l = QVersionNumber::fromString(local);
    if (r.isNull() || l.isNull())
        return remote != local && !remote.isEmpty();
    return QVersionNumber::compare(r, l) > 0;
}

QString targetAppDir() {
    return QCoreApplication::applicationDirPath();
}

static QString findAppPayloadDir(const QString &extractRoot) {
    const QDir root(extractRoot);
    const QStringList candidates = {
        root.filePath(QStringLiteral("Wsalauncher-Linux/app")),
        root.filePath(QStringLiteral("Wsalauncher-Windows/app")),
        root.filePath(QStringLiteral("app")),
    };
    for (const QString &c : candidates) {
        if (QDir(c).exists())
            return c;
    }
    QDirIterator it(extractRoot, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (it.fileName() == QStringLiteral("app"))
            return it.filePath();
    }
    return {};
}

static bool runCmd(const QString &program, const QStringList &args, QString *errorOut) {
    QProcess proc;
    proc.setProgram(program);
    proc.setArguments(args);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start();
    if (!proc.waitForStarted(-1)) {
        if (errorOut)
            *errorOut = proc.errorString();
        return false;
    }
    proc.waitForFinished(-1);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        if (errorOut)
            *errorOut = QString::fromUtf8(proc.readAll());
        return false;
    }
    return true;
}

static bool copyTree(const QString &from, const QString &to, QString *errorOut) {
    QDir src(from);
    if (!src.exists()) {
        if (errorOut)
            *errorOut = QStringLiteral("Missing %1").arg(from);
        return false;
    }
    QDir().mkpath(to);
    QDirIterator it(from, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString rel = src.relativeFilePath(it.filePath());
        const QString dest = to + QLatin1Char('/') + rel;
        if (it.fileInfo().isDir()) {
            QDir().mkpath(dest);
            continue;
        }
        QDir().mkpath(QFileInfo(dest).absolutePath());
        if (QFile::exists(dest))
            QFile::remove(dest);
        if (!QFile::copy(it.filePath(), dest)) {
            if (errorOut)
                *errorOut = QStringLiteral("Copy failed: %1").arg(it.filePath());
            return false;
        }
    }
    return true;
}

bool downloadAndStage(QNetworkAccessManager &nam, const Manifest &manifest, QString *stagingDir,
                      QString *errorOut) {
    if (!manifest.valid) {
        if (errorOut)
            *errorOut = QStringLiteral("Invalid launcher manifest");
        return false;
    }
    const QString base = QString::fromUtf8(constants::kLauncherReleaseBaseUrl);
    const QUrl assetUrl(base + manifest.assetFileName);
    const QString archivePath =
        paths::cacheDir() + QLatin1Char('/') + manifest.assetFileName;

    if (!download::fetchToFile(nam, assetUrl, archivePath, errorOut))
        return false;

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        if (errorOut)
            *errorOut = QStringLiteral("Temp dir failed");
        return false;
    }

#if defined(Q_OS_WIN)
    if (!runCmd(QStringLiteral("powershell"),
                {QStringLiteral("-NoProfile"), QStringLiteral("-Command"),
                 QStringLiteral("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
                     .arg(QDir::toNativeSeparators(archivePath),
                          QDir::toNativeSeparators(tmp.path()))},
                errorOut))
        return false;
#else
    if (!runCmd(QStringLiteral("tar"),
                {QStringLiteral("-xzf"), archivePath, QStringLiteral("-C"), tmp.path()},
                errorOut))
        return false;
#endif

    const QString payload = findAppPayloadDir(tmp.path());
    if (payload.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not find app/ in launcher archive");
        return false;
    }

    const QString stage = paths::cacheDir() + QStringLiteral("/launcher-staging");
    QDir(stage).removeRecursively();
    QDir().mkpath(stage);
    if (!copyTree(payload, stage, errorOut))
        return false;

    if (stagingDir)
        *stagingDir = stage;
    return true;
}

bool restartToApply(const QString &stagingDir, QString *errorOut) {
    const QString appDir = targetAppDir();
    const QString binary = QCoreApplication::applicationFilePath();
#if defined(Q_OS_WIN)
    const QString script = paths::cacheDir() + QStringLiteral("/wsalauncher-apply-update.bat");
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut)
            *errorOut = f.errorString();
        return false;
    }
    const QString bat = QStringLiteral(
        "@echo off\r\n"
        "timeout /t 2 /nobreak >nul\r\n"
        "xcopy /e /i /y /q \"%1\\*\" \"%2\\\"\r\n"
        "start \"\" \"%3\"\r\n"
        "del \"%~f0\"\r\n")
                          .arg(QDir::toNativeSeparators(stagingDir),
                               QDir::toNativeSeparators(appDir),
                               QDir::toNativeSeparators(binary));
    f.write(bat.toUtf8());
    f.close();
    if (!QProcess::startDetached(QStringLiteral("cmd.exe"),
                                 {QStringLiteral("/c"), script})) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to start update helper");
        return false;
    }
#else
    const QString script = paths::cacheDir() + QStringLiteral("/wsalauncher-apply-update.sh");
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut)
            *errorOut = f.errorString();
        return false;
    }
    f.write(QStringLiteral("#!/bin/sh\n"
                           "sleep 2\n"
                           "cp -a \"%1/.\" \"%2/\"\n"
                           "exec \"%3\" \"$@\"\n")
                .arg(stagingDir, appDir, binary)
                .toUtf8());
    f.close();
    QFile::setPermissions(script, QFile::ExeOwner | QFile::ReadOwner | QFile::WriteOwner);
    if (!QProcess::startDetached(QStringLiteral("/bin/sh"), {script})) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to start update helper");
        return false;
    }
#endif
    return true;
}

void checkAndOfferUpdate(QWidget *parent, bool silentIfCurrent) {
    QNetworkAccessManager nam;
    QString err;
    const Manifest manifest = fetchManifest(nam, &err);
    if (!manifest.valid) {
        if (!silentIfCurrent && parent)
            QMessageBox::warning(parent, QStringLiteral("Launcher update"),
                                 QStringLiteral("Could not check updates: %1").arg(err));
        return;
    }
    if (!isNewerVersion(manifest.version, currentVersion())) {
        if (!silentIfCurrent && parent)
            QMessageBox::information(parent, QStringLiteral("Launcher update"),
                                     QStringLiteral("Wsalauncher %1 is up to date.")
                                         .arg(currentVersion()));
        return;
    }
    const auto answer = QMessageBox::question(
        parent, QStringLiteral("Launcher update"),
        QStringLiteral("Wsalauncher %1 is available (you have %2).\nDownload and restart now?")
            .arg(manifest.version, currentVersion()));
    if (answer != QMessageBox::Yes)
        return;

    QString stage;
    if (!downloadAndStage(nam, manifest, &stage, &err)) {
        QMessageBox::warning(parent, QStringLiteral("Launcher update"), err);
        return;
    }
    if (!restartToApply(stage, &err)) {
        QMessageBox::warning(parent, QStringLiteral("Launcher update"), err);
        return;
    }
    QCoreApplication::quit();
}

} // namespace launcherUpdate
