#pragma once

#include "Installer.h"
#include "WsamiawApi.h"

#include <QNetworkAccessManager>
#include <QString>

namespace releases {

struct LatestRelease {
    QString tag;
    QString jarUrl;
    QString jarFileName;
    bool valid = false;
};

LatestRelease fetchLatest(QNetworkAccessManager &nam, QString *errorOut);

QString installedTag();

void setInstalledTag(const QString &tag);

/// Sync Wsamiaw JAR. Uses API /download/url when bearerToken set, else GitHub releases.
bool syncWsamiawJar(QNetworkAccessManager &nam, const QString &modsDir,
                    const QString &fallbackLocalJar, const wsamiawApi::ClientAuth &auth,
                    installer::LogFn log, QString *errorOut);

} // namespace releases
