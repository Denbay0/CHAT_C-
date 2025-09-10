mod app;
mod config;
mod net;
mod ratelimit;
mod state;
mod telemetry;

use crate::telemetry::init_logging;
use dioxus::prelude::*;
use dioxus_desktop::tao::dpi::LogicalSize;
use dioxus_desktop::{Config as DxConfig, WindowBuilder};

static UI_CSS: &str = include_str!("../assets/ui.css");

fn main() -> anyhow::Result<()> {
    let paths = config::paths()?;
    let _logfile = init_logging(&paths)?;
    log::info!("Defay starting… app_dir={}", paths.app_dir.display());

    let cfg = config::load_or_default(&paths.cfg_path);
    let runtime = state::make_runtime(cfg.clone(), &paths);
    app::set_runtime(runtime.0.clone(), paths.clone());

    let head = format!(r#"<style id="ui-css">{}</style>"#, UI_CSS);

    if cfg.auto_connect {
        let _ = runtime.0.net.tx_cmd.try_send(net::NetCmd::Connect {
            host: cfg.last_host.clone(),
            port: cfg.last_port,
            nick: cfg.last_nick.clone(),
        });
    }

    let dx_cfg = DxConfig::new().with_custom_head(head).with_window(
        WindowBuilder::new()
            .with_title("Defay")
            .with_inner_size(LogicalSize::new(900.0, 640.0))
            .with_resizable(true),
    );

    LaunchBuilder::new().with_cfg(dx_cfg).launch(app::App);

    Ok(())
}
