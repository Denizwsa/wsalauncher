#pragma once

#include <QNetworkAccessManager>
#include <QString>
#include <functional>

namespace download {

bool fetchToFile(QNetworkAccessManager &nam, const QUrl &url, const QString &destPath,
                 QString *errorOut);

} // namespace download
