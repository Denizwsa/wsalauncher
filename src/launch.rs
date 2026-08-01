use crate::config::{fabric_version_id, instance_dir};
use serde_json::Value;
use sha1::{Digest, Sha1};
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Child, Command};
use uuid::Uuid;

pub fn launch_game(
    java: &Path,
    username: &str,
    ram_mb: u32,
    log: &mut dyn FnMut(&str),
) -> Result<Child, String> {
    let instance = instance_dir();
    let version_id = fabric_version_id();
    let merged = load_merged_version(&instance, &version_id)?;
    ensure_libraries(&instance, &merged, log)?;
    extract_natives(&instance, &merged, log)?;

    let classpath = build_classpath(&instance, &merged)?;
    if classpath.is_empty() {
        return Err("Classpath is empty — run Install first".into());
    }

    let subs = substitution_map(username, &version_id, &instance);
    let mut cmd = Command::new(java);
    cmd.current_dir(&instance);
    cmd.arg(format!("-Xmx{ram_mb}M"));
    cmd.arg(format!("-Xms{}M", ram_mb.min(1024)));

    let natives = instance.join("natives");
    cmd.arg(format!(
        "-Djava.library.path={}",
        natives.to_string_lossy()
    ));

    for arg in expand_arguments(merged.get("arguments").and_then(|a| a.get("jvm")), &subs) {
        if arg.starts_with("-Xmx") || arg.starts_with("-Xms") || arg.contains("java.library.path") {
            continue;
        }
        cmd.arg(arg);
    }

    cmd.arg("-cp").arg(classpath);

    let main = merged
        .get("mainClass")
        .and_then(|v| v.as_str())
        .unwrap_or("net.fabricmc.loader.impl.launch.knot.KnotClient");
    cmd.arg(main);

    for arg in expand_arguments(merged.get("arguments").and_then(|a| a.get("game")), &subs) {
        cmd.arg(arg);
    }

    if let Some(legacy) = merged.get("minecraftArguments").and_then(|v| v.as_str()) {
        for part in legacy.split_whitespace() {
            cmd.arg(substitute(part, &subs));
        }
    }

    log(&format!("Starting {main}…"));
    cmd.spawn().map_err(|e| e.to_string())
}

pub fn is_installed() -> bool {
    let instance = instance_dir();
    let vid = fabric_version_id();
    instance
        .join("versions")
        .join(&vid)
        .join(format!("{vid}.json"))
        .is_file()
}

fn substitution_map(username: &str, version_id: &str, instance: &Path) -> HashMap<String, String> {
    let mut m = HashMap::new();
    let uuid = Uuid::new_v3(&Uuid::NAMESPACE_DNS, username.as_bytes());
    m.insert("auth_player_name".into(), username.to_string());
    m.insert("auth_uuid".into(), uuid.to_string());
    m.insert("auth_access_token".into(), "0".into());
    m.insert("user_type".into(), "legacy".into());
    m.insert("version_name".into(), version_id.to_string());
    m.insert(
        "game_directory".into(),
        instance.to_string_lossy().into_owned(),
    );
    m.insert(
        "assets_root".into(),
        instance.join("assets").to_string_lossy().into_owned(),
    );
    m.insert("assets_index_name".into(), read_assets_index(instance, version_id));
    m.insert(
        "natives_directory".into(),
        instance.join("natives").to_string_lossy().into_owned(),
    );
    m.insert("launcher_name".into(), "wsalauncher".into());
    m.insert("launcher_version".into(), "0.1".into());
    m
}

fn read_assets_index(instance: &Path, version_id: &str) -> String {
    let path = instance
        .join("versions")
        .join(version_id)
        .join(format!("{version_id}.json"));
    if let Ok(text) = fs::read_to_string(path) {
        if let Ok(v) = serde_json::from_str::<Value>(&text) {
            if let Some(id) = v
                .get("assetIndex")
                .and_then(|a| a.get("id"))
                .and_then(|i| i.as_str())
            {
                return id.to_string();
            }
        }
    }
    "26".into()
}

fn load_merged_version(instance: &Path, version_id: &str) -> Result<Value, String> {
    let mut chain = Vec::new();
    let mut current = version_id.to_string();
    loop {
        let json_path = instance
            .join("versions")
            .join(&current)
            .join(format!("{current}.json"));
        let text = fs::read_to_string(&json_path)
            .map_err(|e| format!("Missing version {} ({e})", json_path.display()))?;
        let value: Value = serde_json::from_str(&text).map_err(|e| e.to_string())?;
        let parent = value
            .get("inheritsFrom")
            .and_then(|v| v.as_str())
            .map(str::to_string);
        chain.push(value);
        match parent {
            Some(p) => current = p,
            None => break,
        }
    }
    chain.reverse();
    let mut merged = chain[0].clone();
    for layer in chain.iter().skip(1) {
        merge_version(&mut merged, layer);
    }
    Ok(merged)
}

fn merge_version(base: &mut Value, overlay: &Value) {
    if let Some(mc) = overlay.get("mainClass") {
        base["mainClass"] = mc.clone();
    }
    if let Some(ma) = overlay.get("minecraftArguments") {
        base["minecraftArguments"] = ma.clone();
    }
    merge_libraries(base, overlay);
    merge_arguments(base, overlay);
}

fn merge_libraries(base: &mut Value, overlay: &Value) {
    let empty = Vec::new();
    let base_libs = base.get("libraries").and_then(|v| v.as_array()).unwrap_or(&empty);
    let over_libs = overlay
        .get("libraries")
        .and_then(|v| v.as_array())
        .unwrap_or(&empty);
    let mut names: HashMap<String, Value> = HashMap::new();
    for lib in base_libs {
        if let Some(name) = library_name(lib) {
            names.insert(name, lib.clone());
        }
    }
    for lib in over_libs {
        if let Some(name) = library_name(lib) {
            names.insert(name, lib.clone());
        }
    }
    base["libraries"] = Value::Array(names.into_values().collect());
}

fn merge_arguments(base: &mut Value, overlay: &Value) {
    let Some(over_args) = overlay.get("arguments") else {
        return;
    };
    if base.get("arguments").is_none() {
        base["arguments"] = over_args.clone();
        return;
    }
    let base_args = base.get_mut("arguments").unwrap();
    for key in ["jvm", "game"] {
        if let (Some(base_arr), Some(over_arr)) = (
            base_args.get_mut(key).and_then(|v| v.as_array_mut()),
            over_args.get(key).and_then(|v| v.as_array()),
        ) {
            for item in over_arr {
                base_arr.push(item.clone());
            }
        }
    }
}

fn library_name(lib: &Value) -> Option<String> {
    lib.get("name").and_then(|n| n.as_str()).map(str::to_string)
}

fn ensure_libraries(
    instance: &Path,
    merged: &Value,
    log: &mut dyn FnMut(&str),
) -> Result<(), String> {
    let client = reqwest::blocking::Client::new();
    let libs = merged
        .get("libraries")
        .and_then(|v| v.as_array())
        .ok_or("No libraries in version json")?;
    for lib in libs {
        if !library_allowed(lib) {
            continue;
        }
        let Some((path, url, sha1)) = library_artifact(lib) else {
            continue;
        };
        let dest = instance.join("libraries").join(&path);
        if dest.is_file() {
            continue;
        }
        if let Some(parent) = dest.parent() {
            fs::create_dir_all(parent).map_err(|e| e.to_string())?;
        }
        log(&format!("Library {path}"));
        let bytes = client.get(&url).send().map_err(|e| e.to_string())?.bytes().map_err(|e| e.to_string())?;
        if let Some(expected) = sha1 {
            let digest = format!("{:x}", Sha1::digest(&bytes));
            if digest != expected {
                return Err(format!("SHA1 mismatch for {path}"));
            }
        }
        fs::write(&dest, &bytes).map_err(|e| e.to_string())?;
    }
    Ok(())
}

fn library_allowed(lib: &Value) -> bool {
    let Some(rules) = lib.get("rules").and_then(|r| r.as_array()) else {
        return true;
    };
    let mut allow = false;
    for rule in rules {
        let action = rule.get("action").and_then(|a| a.as_str()).unwrap_or("allow");
        let os_name = rule
            .get("os")
            .and_then(|o| o.get("name"))
            .and_then(|n| n.as_str());
        let matches = match os_name {
            Some("linux") => cfg!(target_os = "linux"),
            Some("windows") => cfg!(target_os = "windows"),
            Some("osx") => cfg!(target_os = "macos"),
            _ => true,
        };
        if matches {
            allow = action == "allow";
        }
    }
    allow
}

fn library_artifact(lib: &Value) -> Option<(String, String, Option<String>)> {
    if let Some(artifact) = lib.get("downloads").and_then(|d| d.get("artifact")) {
        let path = artifact.get("path")?.as_str()?.to_string();
        let url = artifact.get("url")?.as_str()?.to_string();
        let sha1 = artifact
            .get("sha1")
            .and_then(|s| s.as_str())
            .map(str::to_string);
        return Some((path, url, sha1));
    }
    let name = lib.get("name")?.as_str()?;
    let path = maven_to_path(name);
    Some((
        path.clone(),
        format!("https://libraries.minecraft.net/{path}"),
        None,
    ))
}

fn maven_to_path(name: &str) -> String {
    let parts: Vec<&str> = name.split(':').collect();
    if parts.len() < 3 {
        return name.replace('.', "/");
    }
    let group_path = parts[0].replace('.', "/");
    let classifier = if parts.len() > 3 {
        format!("-{}", parts[3])
    } else {
        String::new()
    };
    format!(
        "{}/{}/{}/{}-{}{}.jar",
        group_path, parts[1], parts[2], parts[1], parts[2], classifier
    )
}

fn build_classpath(instance: &Path, merged: &Value) -> Result<String, String> {
    let mut jars = Vec::new();
    let libs = merged
        .get("libraries")
        .and_then(|v| v.as_array())
        .ok_or("No libraries")?;
    for lib in libs {
        if !library_allowed(lib) {
            continue;
        }
        if let Some((path, _, _)) = library_artifact(lib) {
            let file = instance.join("libraries").join(path);
            if file.is_file() {
                jars.push(file);
            }
        }
    }
    Ok(jars
        .iter()
        .map(|p| p.to_string_lossy())
        .collect::<Vec<_>>()
        .join(":"))
}

fn extract_natives(instance: &Path, merged: &Value, log: &mut dyn FnMut(&str)) -> Result<(), String> {
    let natives_dir = instance.join("natives");
    fs::create_dir_all(&natives_dir).map_err(|e| e.to_string())?;
    let native_key = if cfg!(target_os = "linux") {
        "natives-linux"
    } else if cfg!(target_os = "windows") {
        "natives-windows"
    } else {
        "natives-macos"
    };
    let libs = merged
        .get("libraries")
        .and_then(|v| v.as_array())
        .ok_or("No libraries")?;
    for lib in libs {
        if !library_allowed(lib) {
            continue;
        }
        let Some(native) = lib
            .get("downloads")
            .and_then(|d| d.get("classifiers"))
            .and_then(|c| c.get(native_key))
        else {
            continue;
        };
        let path = native.get("path").and_then(|p| p.as_str()).ok_or("native path")?;
        let jar_path = instance.join("libraries").join(path);
        if !jar_path.is_file() {
            continue;
        }
        log(&format!("Natives {path}"));
        let file = fs::File::open(&jar_path).map_err(|e| e.to_string())?;
        let mut archive = zip::ZipArchive::new(file).map_err(|e| e.to_string())?;
        for i in 0..archive.len() {
            let mut entry = archive.by_index(i).map_err(|e| e.to_string())?;
            let name = entry.name().to_string();
            if name.contains("META-INF") || entry.is_dir() {
                continue;
            }
            let fname = PathBuf::from(&name)
                .file_name()
                .map(|f| f.to_owned())
                .unwrap_or_default();
            let out_path = natives_dir.join(fname);
            let mut out = fs::File::create(&out_path).map_err(|e| e.to_string())?;
            std::io::copy(&mut entry, &mut out).map_err(|e| e.to_string())?;
        }
    }
    Ok(())
}

fn expand_arguments(args: Option<&Value>, subs: &HashMap<String, String>) -> Vec<String> {
    let Some(Value::Array(items)) = args else {
        return Vec::new();
    };
    let mut out = Vec::new();
    for item in items {
        match item {
            Value::String(s) => out.push(substitute(s, subs)),
            Value::Object(obj) => {
                if !rule_matches(obj.get("rules")) {
                    continue;
                }
                match obj.get("value") {
                    Some(Value::String(s)) => out.push(substitute(s, subs)),
                    Some(Value::Array(vals)) => {
                        for v in vals {
                            if let Some(s) = v.as_str() {
                                out.push(substitute(s, subs));
                            }
                        }
                    }
                    _ => {}
                }
            }
            _ => {}
        }
    }
    out
}

fn rule_matches(rules: Option<&Value>) -> bool {
    let Some(Value::Array(rules)) = rules else {
        return true;
    };
    let mut allow = false;
    for rule in rules {
        let Some(obj) = rule.as_object() else {
            continue;
        };
        let action = obj.get("action").and_then(|a| a.as_str()).unwrap_or("allow");
        let os_name = obj
            .get("os")
            .and_then(|o| o.get("name"))
            .and_then(|n| n.as_str());
        let matches = match os_name {
            Some("linux") => cfg!(target_os = "linux"),
            Some("windows") => cfg!(target_os = "windows"),
            Some("osx") => cfg!(target_os = "macos"),
            _ => true,
        };
        if matches {
            allow = action == "allow";
        }
    }
    allow
}

fn substitute(input: &str, subs: &HashMap<String, String>) -> String {
    let mut s = input.to_string();
    for (key, val) in subs {
        s = s.replace(&format!("${{{key}}}"), val);
    }
    s
}
