#pragma once

#include <QNetworkAccessManager>
#include <QString>
#include <functional>

#include "WsamiawApi.h"

namespace installer {

using LogFn = std::function<void(const QString &)>;

bool runFullInstall(QNetworkAccessManager &nam, const QString &javaPath,
                    const QString &wsamiawJar, const wsamiawApi::ClientAuth &auth, LogFn log,
                    QString *errorOut);

} // namespace installer
