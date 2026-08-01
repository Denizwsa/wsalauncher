#include "Download.h"

#include <QEventLoop>
#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>

namespace download {

bool fetchToFile(QNetworkAccessManager &nam, const QUrl &url, const QString &destPath,
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

    QSaveFile file(destPath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorOut)
            *errorOut = file.errorString();
        reply->deleteLater();
        return false;
    }
    file.write(reply->readAll());
    if (!file.commit()) {
        if (errorOut)
            *errorOut = file.errorString();
        reply->deleteLater();
        return false;
    }
    reply->deleteLater();
    return true;
}

} // namespace download
