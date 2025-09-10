use crate::config::Paths;
use anyhow::Context;
use once_cell::sync::OnceCell;
use std::fs::{self, OpenOptions};
use std::path::PathBuf;
use time::{format_description::well_known::Rfc3339, OffsetDateTime};
use tracing_subscriber::{fmt, EnvFilter};

static LOG_GUARD: OnceCell<tracing_appender::non_blocking::WorkerGuard> = OnceCell::new();

pub fn current_log_path(paths: &Paths) -> PathBuf {
    let ts = OffsetDateTime::now_utc()
        .format(&Rfc3339)
        .unwrap_or_else(|_| "unknown-ts".into())
        .replace(':', "-");
    paths.logs_dir.join(format!("defay-{ts}.log"))
}

pub fn init_logging(paths: &Paths) -> anyhow::Result<PathBuf> {
    fs::create_dir_all(&paths.logs_dir)
        .with_context(|| format!("create logs dir {}", paths.logs_dir.display()))?;

    let log_path = current_log_path(paths);

    let mut file = OpenOptions::new()
        .create_new(true)
        .write(true)
        .open(&log_path)
        .with_context(|| format!("open log file {}", log_path.display()))?;
    // UTF-8 BOM для Notepad
    use std::io::Write;
    let _ = file.write_all(b"\xEF\xBB\xBF");
    drop(file);

    let file = OpenOptions::new().append(true).open(&log_path)?;
    let (nb, guard) = tracing_appender::non_blocking(file);
    let _ = LOG_GUARD.set(guard);

    let filter = EnvFilter::try_from_default_env()
        .unwrap_or_else(|_| EnvFilter::new("info,tao=warn,wry=warn,dioxus_cli_config=warn"));

    tracing_subscriber::fmt()
        .with_env_filter(filter)
        .with_writer(nb)
        .with_ansi(false)
        .with_target(true)
        .with_level(true)
        .with_thread_names(true)
        .with_line_number(true)
        .event_format(fmt::format().with_timer(fmt::time::UtcTime::rfc_3339()))
        .try_init()
        .ok();

    tracing::info!("Defay logging started | file: {}", log_path.display());
    Ok(log_path)
}
