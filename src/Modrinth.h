#pragma once

#include <QNetworkAccessManager>
#include <QString>
#include <functional>

namespace modrinth {

bool installMod(QNetworkAccessManager &nam, const QString &slug, const QString &modsDir,
                QString *errorOut);

/// Removes OpenGL pipeline mods that conflict with VulkanMod (Sodium, Iris, …).
void removeConflictingRenderMods(const QString &modsDir);

} // namespace modrinth
