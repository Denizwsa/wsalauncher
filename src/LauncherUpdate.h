#pragma once

#include <QNetworkAccessManager>
#include <QString>

namespace launcherUpdate {

struct Manifest {
    QString version;
    QString assetFileName;
    bool valid = false;
};

QString currentVersion();

Manifest fetchManifest(QNetworkAccessManager &nam, QString *errorOut);

bool isNewerVersion(const QString &remote, const QString &local);

/// Download + extract platform package; returns directory whose contents map to `app/`.
bool downloadAndStage(QNetworkAccessManager &nam, const Manifest &manifest, QString *stagingDir,
                      QString *errorOut);

QString targetAppDir();

bool restartToApply(const QString &stagingDir, QString *errorOut);

void checkAndOfferUpdate(QWidget *parent, bool silentIfCurrent);

} // namespace launcherUpdate
