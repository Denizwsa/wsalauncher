#include "Paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtGlobal>

namespace paths {

QString dataRoot() {
#if defined(Q_OS_WIN)
    const QString dir = QDir::cleanPath(
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/wsalauncher"));
#elif defined(Q_OS_MACOS)
    const QString dir = QDir::cleanPath(
        QDir::homePath() + QStringLiteral("/Library/Application Support/wsalauncher"));
#else
    const QString dir = QDir::cleanPath(QDir::homePath() + QStringLiteral("/.wsalauncher"));
#endif
    QDir().mkpath(dir);
    return QFileInfo(dir).absoluteFilePath();
}

QString configFile() {
    return dataRoot() + QStringLiteral("/config.json");
}

QString minecraftDir() {
#if defined(Q_OS_WIN)
    const QString dir = QDir::cleanPath(
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/wsalauncher-minecraft"));
#elif defined(Q_OS_MACOS)
    const QString dir = QDir::cleanPath(
        QDir::homePath() + QStringLiteral("/Library/Application Support/wsalauncher-minecraft"));
#else
    const QString dir =
        QDir::cleanPath(QDir::homePath() + QStringLiteral("/.wsalauncher-minecraft"));
#endif
    QDir().mkpath(dir);
    return QFileInfo(dir).absoluteFilePath();
}

QString instanceDir() {
    return minecraftDir();
}

QString cacheDir() {
    const QString dir = dataRoot() + QStringLiteral("/cache");
    QDir().mkpath(dir);
    return dir;
}

QString logoPath() {
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/wsamiaw-recodelogo.png"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/logo.png"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/logo.png"),
        dataRoot() + QStringLiteral("/logo.png"),
        QDir::homePath() + QStringLiteral("/Belgeler/wsamiaw-recodelogo.png"),
        QDir::homePath() + QStringLiteral("/Documents/wsamiaw-recodelogo.png"),
    };
    for (const QString &p : candidates) {
        if (QFile::exists(p))
            return p;
    }
    return {};
}

} // namespace paths
