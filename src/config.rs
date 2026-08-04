use directories::ProjectDirs;
use serde::{Deserialize, Serialize};
use std::path::PathBuf;

pub const MC_VERSION: &str = "26.2";
pub const LOADER_VERSION: &str = "0.19.3";
pub const FABRIC_INSTALLER_URL: &str =
    "https://maven.fabricmc.net/net/fabricmc/fabric-installer/1.1.0/fabric-installer-1.1.0.jar";

pub const BUNDLE_MOD_SLUGS: &[&str] = &[
    "fabric-api",
    "vulkanmod",
    "lithium",
    "ferritecore",
    "entityculling",
    "viafabricplus",
];

pub const WSAMIAW_JAR_DEFAULT: &str =
    "/home/deniz/İndirilenler/wsamiaw-26.2/build/libs/Wsamiaw-4.0.0.jar";

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LauncherConfig {
    pub username: String,
    pub ram_mb: u32,
    pub wsamiaw_jar: PathBuf,
    pub java_path: PathBuf,
}

impl Default for LauncherConfig {
    fn default() -> Self {
        Self {
            username: "Player".into(),
            ram_mb: 4096,
            wsamiaw_jar: PathBuf::from(WSAMIAW_JAR_DEFAULT),
            java_path: detect_java_25(),
        }
    }
}

pub fn config_path() -> Option<PathBuf> {
    ProjectDirs::from("wsamiaw", "launcher", "Wsalauncher")
        .map(|d| d.config_dir().join("config.json"))
}

pub fn instance_dir() -> PathBuf {
    ProjectDirs::from("wsamiaw", "launcher", "Wsalauncher")
        .map(|d| d.data_dir().join("instance"))
        .unwrap_or_else(|| PathBuf::from("instance"))
}

pub fn load_config() -> LauncherConfig {
    let Some(path) = config_path() else {
        return LauncherConfig::default();
    };
    std::fs::read_to_string(path)
        .ok()
        .and_then(|s| serde_json::from_str(&s).ok())
        .unwrap_or_default()
}

pub fn save_config(cfg: &LauncherConfig) {
    let Some(path) = config_path() else {
        return;
    };
    if let Some(parent) = path.parent() {
        let _ = std::fs::create_dir_all(parent);
    }
    if let Ok(json) = serde_json::to_string_pretty(cfg) {
        let _ = std::fs::write(path, json);
    }
}

pub fn fabric_version_id() -> String {
    format!("fabric-loader-{LOADER_VERSION}-{MC_VERSION}")
}

/// Prefer Java 25+ for Minecraft 26.2 / Wsamiaw.
pub fn detect_java_25() -> PathBuf {
    const CANDIDATES: &[&str] = &[
        "/usr/lib/jvm/java-25-openjdk/bin/java",
        "/usr/lib/jvm/java-25/bin/java",
        "/usr/lib/jvm/jdk-25/bin/java",
        "/usr/bin/java",
    ];
    for c in CANDIDATES {
        let p = PathBuf::from(c);
        if p.is_file() && java_version_major(&p) >= 25 {
            return p;
        }
    }
    PathBuf::from("java")
}

fn java_version_major(java: &PathBuf) -> u32 {
    let out = std::process::Command::new(java)
        .arg("-version")
        .output()
        .ok()?;
    let text = String::from_utf8_lossy(&out.stderr);
    parse_java_major(&text).unwrap_or(0)
}

fn parse_java_major(s: &str) -> Option<u32> {
    for line in s.lines() {
        if let Some(rest) = line.strip_prefix("openjdk version \"") {
            let ver = rest.split('"').next()?.split('.').next()?;
            return ver.parse().ok();
        }
        if let Some(rest) = line.strip_prefix("java version \"") {
            let ver = rest.split('"').next()?.split('.').next()?;
            return ver.parse().ok();
        }
    }
    None
}
