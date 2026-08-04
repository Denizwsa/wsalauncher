#include "Installer.h"

#include "Bootstrap.h"
#include "Constants.h"
#include "Download.h"
#include "Modrinth.h"
#include "Paths.h"
#include "Platform.h"
#include "Releases.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QtGlobal>

namespace installer {

static bool runProcess(const QString &program, const QStringList &args, const QString &workDir,
                       LogFn log, QString *errorOut) {
    log(QStringLiteral("$ %1 %2").arg(program, args.join(QLatin1Char(' '))));
    QProcess proc;
    proc.setProgram(program);
    proc.setArguments(args);
    if (!workDir.isEmpty())
        proc.setWorkingDirectory(workDir);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start();
    if (!proc.waitForStarted()) {
        if (errorOut)
            *errorOut = proc.errorString();
        return false;
    }
    proc.waitForFinished(-1);
    const QString out = QString::fromUtf8(proc.readAll());
    if (!out.trimmed().isEmpty())
        log(out.trimmed());
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        if (errorOut)
            *errorOut = QStringLiteral("Process failed (%1)").arg(proc.exitCode());
        return false;
    }
    return true;
}

bool runFullInstall(QNetworkAccessManager &nam, const QString &javaPathIn,
                    const QString &wsamiawJar, const wsamiawApi::ClientAuth &auth, LogFn log,
                    QString *errorOut) {
    const QString javaPath = platform::resolveJava(javaPathIn);
    const QString instance = paths::minecraftDir();
    const QString mods = instance + QStringLiteral("/mods");
    QDir().mkpath(mods);
    modrinth::removeConflictingRenderMods(mods);

    if (!bootstrap::ensureGameEnvironment(instance, log, errorOut))
        return false;
    if (!bootstrap::ensureVanillaVersion(nam, instance, QString::fromUtf8(constants::kMcVersion), log,
                                         errorOut))
        return false;

    const QString vid = constants::fabricVersionId();
    const QString versionJson =
        instance + QStringLiteral("/versions/") + vid + QLatin1Char('/') + vid + QStringLiteral(".json");

    if (!QFile::exists(versionJson)) {
        const QString installerJar =
            paths::cacheDir() + QStringLiteral("/fabric-installer-1.1.0.jar");
        log(QStringLiteral("Downloading Fabric installer…"));
        if (!download::fetchToFile(nam, QUrl(QString::fromUtf8(constants::kFabricInstallerUrl)),
                                   installerJar, errorOut))
            return false;

        log(QStringLiteral("Installing Fabric %1 / loader %2…")
                .arg(constants::kMcVersion, constants::kLoaderVersion));
        QStringList args;
        args << QStringLiteral("-jar") << installerJar << QStringLiteral("client")
             << QStringLiteral("-mcversion") << constants::kMcVersion << QStringLiteral("-loader")
             << constants::kLoaderVersion << QStringLiteral("-dir") << instance;
#if defined(Q_OS_WIN)
        args << QStringLiteral("-launcher") << QStringLiteral("win32");
#endif
        if (!runProcess(javaPath, args, QString(), log, errorOut))
            return false;

        if (!QFile::exists(versionJson)) {
            if (errorOut)
                *errorOut = QStringLiteral("Missing %1").arg(versionJson);
            return false;
        }

        log(QStringLiteral("Downloading mods from Modrinth…"));
        for (const QString &slug : constants::bundledModSlugs()) {
            QString err;
            if (!modrinth::installMod(nam, slug, mods, &err))
                log(QStringLiteral("WARN %1: %2").arg(slug, err));
            else
                log(QStringLiteral("OK %1").arg(slug));
        }
    } else {
        log(QStringLiteral("Fabric instance present — refreshing bundled mods…"));
        for (const QString &slug : constants::bundledModSlugs()) {
            QString err;
            if (!modrinth::installMod(nam, slug, mods, &err))
                log(QStringLiteral("WARN %1: %2").arg(slug, err));
            else
                log(QStringLiteral("OK %1").arg(slug));
        }
    }

    QString jarErr;
    releases::syncWsamiawJar(nam, mods, wsamiawJar, auth, log, &jarErr);
    if (!jarErr.isEmpty()) {
        if (errorOut)
            *errorOut = jarErr;
        return false;
    }
    log(QStringLiteral("Install / update complete."));
    return true;
}

} // namespace installer
