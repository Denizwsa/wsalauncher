#pragma once

#include <QString>
#include <QStringList>

namespace constants {

inline constexpr const char *kMcVersion = "26.2";
inline constexpr const char *kLoaderVersion = "0.19.3";
inline constexpr const char *kFabricInstallerUrl =
    "https://maven.fabricmc.net/net/fabricmc/fabric-installer/1.1.0/fabric-installer-1.1.0.jar";
inline constexpr const char *kApiBase = "https://wsamiaw-api.wsamiaw.workers.dev";
inline constexpr const char *kSiteAccountUrl = "https://wsamiawclient.lol/account.html";
inline constexpr const char *kSiteReleasesManifest = "https://wsamiawclient.lol/releases/manifest.json";
inline constexpr const char *kLauncherManifestUrl =
    "https://cdn.wsamiawclient.lol/releases/launcher/manifest.json";
inline constexpr const char *kLauncherReleaseBaseUrl =
    "https://cdn.wsamiawclient.lol/releases/launcher/";
inline constexpr const char *kGithubLatestReleaseApi =
    "https://api.github.com/repos/Denizwsa/wsamiaw-recode/releases/latest";

inline QString fabricVersionId() {
    return QString("fabric-loader-%1-%2").arg(kLoaderVersion, kMcVersion);
}

/// VulkanMod replaces Sodium/Iris; keep non-rendering performance mods only.
inline QStringList bundledModSlugs() {
    return {"fabric-api", "vulkanmod", "lithium", "ferritecore", "entityculling", "viafabricplus"};
}

/// Optional local fallback only; releases are preferred.
inline QString defaultWsamiawJar() {
    return {};
}

} // namespace constants
