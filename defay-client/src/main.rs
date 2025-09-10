mod app;
mod config;
mod net;
mod ratelimit;
mod state;
mod telemetry;

use crate::telemetry::init_logging;
use dioxus::prelude::*; // тут есть LaunchBuilder
use dioxus_desktop::{Config as DxConfig, WindowBuilder};
use dioxus_desktop::tao::dpi::LogicalSize;
use std::path::PathBuf;

static DEFAY_CSS: &str = include_str!("../assets/defay.css");

fn main() -> anyhow::Result<()> {
    // пути и логи
    let paths = config::paths()?;
    let _logfile = init_logging(&paths)?;
    log::info!("Defay starting… app_dir={}", paths.app_dir.display());

    // грузим конфиг и поднимаем рантайм
    let cfg = config::load_or_default(&paths.cfg_path);
    let runtime = state::make_runtime(cfg.clone(), &paths);

    // инъекция CSS в <head>
    let head = format!(r#"<style id="defay-css">{}</style>"#, DEFAY_CSS);

    // опциональный автоконнект из конфига
    if cfg.auto_connect {
        let _ = runtime.0.net.tx_cmd.try_send(net::NetCmd::Connect {
            host: cfg.last_host.clone(),
            port: cfg.last_port,
            nick: cfg.last_nick.clone(),
        });
    }

    // Конфиг десктопа: custom head — на уровне Config, а не WindowBuilder
    let dx_cfg = DxConfig::new()
        .with_custom_head(head)
        .with_window(
            WindowBuilder::new()
                .with_title("Defay")
                .with_inner_size(LogicalSize::new(900.0, 640.0))
                .with_resizable(true),
        );

    // Стартуем твой существующий корневой компонент.
    // В исходниках Dioxus обычно корень называется `App`.
    LaunchBuilder::new()
        .with_cfg(dx_cfg)
        .launch(app::App);

    Ok(())
}
