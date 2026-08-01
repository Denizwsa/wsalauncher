#pragma once

#include <QNetworkAccessManager>
#include <QString>
#include <functional>

namespace modrinth {

bool installMod(QNetworkAccessManager &nam, const QString &slug, const QString &modsDir,
                QString *errorOut);

} // namespace modrinth
