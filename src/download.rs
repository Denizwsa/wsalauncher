use sha1::{Digest, Sha1};
use std::fs::{self, File};
use std::io::{Read, Write};
use std::path::{Path, PathBuf};

pub fn download(
    client: &reqwest::blocking::Client,
    url: &str,
    dest: &Path,
    log: &mut dyn FnMut(&str),
) -> Result<(), String> {
    if let Some(parent) = dest.parent() {
        fs::create_dir_all(parent).map_err(|e| e.to_string())?;
    }
    log(&format!("GET {url}"));
    let mut resp = client.get(url).send().map_err(|e| e.to_string())?;
    if !resp.status().is_success() {
        return Err(format!("HTTP {} for {url}", resp.status()));
    }
    let mut buf = Vec::new();
    resp.copy_to(&mut buf).map_err(|e| e.to_string())?;
    let mut f = File::create(dest).map_err(|e| e.to_string())?;
    f.write_all(&buf).map_err(|e| e.to_string())?;
    Ok(())
}

pub fn download_if_missing(
    client: &reqwest::blocking::Client,
    url: &str,
    dest: &Path,
    sha1_expected: Option<&str>,
    log: &mut dyn FnMut(&str),
) -> Result<(), String> {
    if dest.is_file() {
        if let Some(expected) = sha1_expected {
            if file_sha1(dest).ok().as_deref() == Some(expected) {
                return Ok(());
            }
            log(&format!("Re-download (bad hash): {}", dest.display()));
        } else {
            return Ok(());
        }
    }
    download(client, url, dest, log)
}

fn file_sha1(path: &Path) -> Result<String, String> {
    let mut f = File::open(path).map_err(|e| e.to_string())?;
    let mut hasher = Sha1::new();
    let mut buf = [0u8; 8192];
    loop {
        let n = f.read(&mut buf).map_err(|e| e.to_string())?;
        if n == 0 {
            break;
        }
        hasher.update(&buf[..n]);
    }
    Ok(format!("{:x}", hasher.finalize()))
}

pub fn copy_file(src: &Path, dest: &Path, log: &mut dyn FnMut(&str)) -> Result<(), String> {
    if !src.is_file() {
        return Err(format!("Missing file: {}", src.display()));
    }
    if let Some(parent) = dest.parent() {
        fs::create_dir_all(parent).map_err(|e| e.to_string())?;
    }
    log(&format!("Copy {} -> {}", src.display(), dest.display()));
    fs::copy(src, dest).map_err(|e| e.to_string())?;
    Ok(())
}

pub fn ensure_dir(path: &Path) -> Result<(), String> {
    fs::create_dir_all(path).map_err(|e| e.to_string())
}

pub fn cache_path(name: &str) -> PathBuf {
    directories::ProjectDirs::from("wsamiaw", "launcher", "Wsalauncher")
        .map(|d| d.cache_dir().join(name))
        .unwrap_or_else(|| PathBuf::from(name))
}
