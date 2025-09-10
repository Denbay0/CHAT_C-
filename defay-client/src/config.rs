use directories::ProjectDirs;
use serde::{Deserialize, Serialize};
use std::{fs, path::PathBuf};

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct AppConfig {
    pub last_host: String,
    pub last_port: u16,
    pub last_nick: String,
    pub auto_connect: bool,
    pub theme: Theme,
    pub ui_scale: f32,
    pub notifications_enabled: bool,
    pub flood_limit_burst: u32,
    pub flood_limit_window_sec: u64,
}

#[derive(Debug, Serialize, Deserialize, Clone, Copy, PartialEq, Eq)]
pub enum Theme {
    System,
    Light,
    Dark,
}

impl Default for AppConfig {
    fn default() -> Self {
        Self {
            last_host: "0.0.0.0".to_string(),
            last_port: 5555,
            last_nick: "".to_string(),
            auto_connect: false,
            theme: Theme::Light,
            ui_scale: 1.0,
            notifications_enabled: true,
            flood_limit_burst: 8,
            flood_limit_window_sec: 30,
        }
    }
}

#[derive(Clone, PartialEq)]
pub struct Paths {
    pub app_dir: PathBuf,
    pub cfg_path: PathBuf,
    pub assets_dir: PathBuf,
    pub logs_dir: PathBuf,
}

pub fn paths() -> anyhow::Result<Paths> {
    let proj = ProjectDirs::from("com", "Defay", "DefayClient")
        .ok_or_else(|| anyhow::anyhow!("Не удалось определить каталог приложения"))?;
    let app_dir = proj.config_dir().to_path_buf();
    fs::create_dir_all(&app_dir)?;
    let cfg_path = app_dir.join("config.json");

    let assets_dir = std::env::current_exe()
        .ok()
        .and_then(|p| p.parent().map(|p| p.to_path_buf()))
        .unwrap_or_else(|| std::env::current_dir().unwrap_or_else(|_| PathBuf::from(".")))
        .join("assets");

    Ok(Paths {
        logs_dir: app_dir.join("logs"),
        app_dir,
        cfg_path,
        assets_dir,
    })
}

pub fn load_or_default(cfg_path: &PathBuf) -> AppConfig {
    match fs::read_to_string(cfg_path) {
        Ok(s) => serde_json::from_str(&s).unwrap_or_default(),
        Err(_) => AppConfig::default(),
    }
}

pub fn save(cfg_path: &PathBuf, cfg: &AppConfig) -> anyhow::Result<()> {
    let s = serde_json::to_string_pretty(cfg)?;
    fs::write(cfg_path, s)?;
    Ok(())
}
