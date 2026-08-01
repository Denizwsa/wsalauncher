#pragma once

#include <QString>

namespace paths {

/// Launcher metadata (config, cache, release tags): ~/.wsalauncher
QString dataRoot();

QString configFile();

/// Independent game directory (like .minecraft): ~/.wsalauncher-minecraft
QString minecraftDir();

/// @deprecated Use minecraftDir()
QString instanceDir();

QString cacheDir();

QString logoPath();

} // namespace paths
