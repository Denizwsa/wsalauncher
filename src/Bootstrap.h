#pragma once

#include "Installer.h"

#include <QJsonObject>
#include <QString>

namespace bootstrap {

bool ensureGameEnvironment(const QString &minecraftDir, installer::LogFn log, QString *errorOut);

bool ensureVanillaVersion(QNetworkAccessManager &nam, const QString &minecraftDir,
                          const QString &mcVersion, installer::LogFn log, QString *errorOut);

bool ensureAssets(QNetworkAccessManager &nam, const QString &minecraftDir,
                  const QJsonObject &assetIndex, installer::LogFn log, QString *errorOut);

bool prepareNatives(const QString &javaPath, const QString &minecraftDir,
                    const QStringList &nativeJarPaths, installer::LogFn log, QString *errorOut);

} // namespace bootstrap
