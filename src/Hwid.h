#pragma once

#include <QString>

namespace hwid {

/// Same value as in-game / website HwidCli (uses Java os properties).
QString compute(const QString &javaPath, QString *errorOut = nullptr);

} // namespace hwid
