use crate::config::MC_VERSION;
use crate::download::download;
use serde::Deserialize;
use std::path::Path;

#[derive(Debug, Deserialize)]
struct ModrinthVersion {
    files: Vec<ModrinthFile>,
}

#[derive(Debug, Deserialize)]
struct ModrinthFile {
    url: String,
    filename: String,
    primary: bool,
}

pub fn fetch_and_install_mod(
    client: &reqwest::blocking::Client,
    slug: &str,
    mods_dir: &Path,
    log: &mut dyn FnMut(&str),
) -> Result<(), String> {
    let url = format!(
        "https://api.modrinth.com/v2/project/{slug}/version?game_versions=[\"{MC_VERSION}\"]&loaders=[\"fabric\"]"
    );
    log(&format!("Modrinth: {slug}"));
    let versions: Vec<ModrinthVersion> = client.get(&url).send().map_err(|e| e.to_string())?.json().map_err(|e| e.to_string())?;
    let version = versions
        .first()
        .ok_or_else(|| format!("No {MC_VERSION} Fabric build for {slug}"))?;
    let file = version
        .files
        .iter()
        .find(|f| f.primary)
        .or_else(|| version.files.first())
        .ok_or_else(|| format!("No file for {slug}"))?;
    let dest = mods_dir.join(&file.filename);
    download(client, &file.url, &dest, log)
}
