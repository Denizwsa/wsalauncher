use crate::config::{
    fabric_version_id, instance_dir, BUNDLE_MOD_SLUGS, FABRIC_INSTALLER_URL, MC_VERSION,
    LOADER_VERSION,
};
use crate::download::{cache_path, copy_file, download, ensure_dir};
use crate::modrinth::fetch_and_install_mod;
use std::path::{Path, PathBuf};
use std::process::Command;

pub fn run_full_install(
    java: &Path,
    wsamiaw_jar: &Path,
    log: &mut dyn FnMut(&str),
) -> Result<(), String> {
    let instance = instance_dir();
    ensure_dir(&instance)?;
    let mods = instance.join("mods");
    ensure_dir(&mods)?;

    let client = reqwest::blocking::Client::builder()
        .user_agent("Wsalauncher/0.1 (Wsamiaw)")
        .build()
        .map_err(|e| e.to_string())?;

    install_fabric(java, &instance, log)?;

    log("Downloading performance / utility mods…");
    for slug in BUNDLE_MOD_SLUGS {
        if let Err(e) = fetch_and_install_mod(&client, slug, &mods, log) {
            log(&format!("WARN {slug}: {e}"));
        }
    }

    let wsamiaw_name = wsamiaw_jar
        .file_name()
        .map(|s| s.to_owned())
        .unwrap_or_else(|| "Wsamiaw.jar".into());
    let dest = mods.join(wsamiaw_name);
    copy_file(wsamiaw_jar, &dest, log)?;

    log("Install finished.");
    Ok(())
}

fn install_fabric(java: &Path, instance: &Path, log: &mut dyn FnMut(&str)) -> Result<(), String> {
    let installer_cache = cache_path("fabric-installer-1.1.0.jar");
    if let Some(parent) = installer_cache.parent() {
        ensure_dir(parent)?;
    }
    download(
        &reqwest::blocking::Client::new(),
        FABRIC_INSTALLER_URL,
        &installer_cache,
        log,
    )?;

    log(&format!(
        "Running Fabric installer (MC {MC_VERSION}, loader {LOADER_VERSION})…"
    ));
    let status = Command::new(java)
        .arg("-jar")
        .arg(&installer_cache)
        .args([
            "client",
            "-mcversion",
            MC_VERSION,
            "-loader",
            LOADER_VERSION,
            "-dir",
        ])
        .arg(instance)
        .status()
        .map_err(|e| format!("Failed to run Fabric installer: {e}"))?;

    if !status.success() {
        return Err(format!("Fabric installer exited with {status}"));
    }

    let vid = fabric_version_id();
    let version_json = instance.join("versions").join(&vid).join(format!("{vid}.json"));
    if !version_json.is_file() {
        return Err(format!(
            "Fabric version not found at {}",
            version_json.display()
        ));
    }
    Ok(())
}
