#pragma once

#include <QNetworkAccessManager>
#include <QString>

namespace wsamiawApi {

struct ClientAuth {
    QString licenseKey;
    QString hwid;
    bool valid() const { return !licenseKey.isEmpty() && !hwid.isEmpty(); }
};

struct DownloadInfo {
    bool ok = false;
    QString url;
    QString version;
    QString file;
    QString sha256;
    QString error;
};

QString fetchLaunchToken(QNetworkAccessManager &nam, const ClientAuth &auth, QString *errorOut);
DownloadInfo fetchDownloadInfo(QNetworkAccessManager &nam, const ClientAuth &auth);
bool verifyClient(QNetworkAccessManager &nam, const ClientAuth &auth, QString *reasonOut);

} // namespace wsamiawApi
