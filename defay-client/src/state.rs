use crate::net::{NetCmd, NetEvent, NetHandle};
use std::sync::{Arc, Mutex};
use chrono::{Local};
use tokio::sync::mpsc;

#[derive(Clone, Debug)]
pub struct ChatMessage {
    pub ts_ms: u64,
    pub username: String,
    pub text: String,
}

#[derive(Clone)]
pub struct Runtime(pub Arc<AppState>);

pub struct AppState {
    pub cfg: Mutex<crate::config::AppConfig>,
    pub messages: Mutex<Vec<ChatMessage>>,
    pub connected: Mutex<bool>,
    pub status_text: Mutex<String>,
    pub net: NetHandle,
    pub rate: Arc<crate::ratelimit::TokenBucket>,
}

pub fn make_runtime(cfg: crate::config::AppConfig, _paths: &crate::config::Paths) -> Runtime {
    let net = crate::net::start_net_runtime();

    let st = Arc::new(AppState {
        rate: Arc::new(crate::ratelimit::TokenBucket::new(
            cfg.flood_limit_burst,
            std::time::Duration::from_secs(cfg.flood_limit_window_sec),
        )),
        cfg: Mutex::new(cfg),
        messages: Mutex::new(Vec::new()),
        connected: Mutex::new(false),
        status_text: Mutex::new(String::from("Готов.")),
        net,
    });

    // Поднимаем помпу событий сразу — это покрывает вызов start_background()
    start_event_pump(&st);

    Runtime(st)
}

fn start_event_pump(st: &Arc<AppState>) {
    let Some(mut rx) = st.net.rx_evt.lock().unwrap().take() else {
        log::warn!("state: rx_evt already taken");
        return;
    };

    let weak = Arc::downgrade(st);
    std::thread::spawn(move || {
        log::info!("state: event pump started");
        let local = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .expect("tokio current_thread");
        local.block_on(async move {
            while let Some(evt) = rx.recv().await {
                if let Some(st) = weak.upgrade() {
                    match evt {
                        NetEvent::Connected => {
                            *st.connected.lock().unwrap() = true;
                            *st.status_text.lock().unwrap() = "Подключено".into();
                            log::info!("state: connected");
                        }
                        NetEvent::Disconnected { reason } => {
                            *st.connected.lock().unwrap() = false;
                            *st.status_text.lock().unwrap() =
                                format!("Отключено ({reason})");
                            log::info!("state: disconnected ({reason})");
                        }
                        NetEvent::Error { msg } => {
                            *st.status_text.lock().unwrap() = format!("Ошибка: {msg}");
                            log::error!("state: error: {msg}");
                        }
                        NetEvent::Message { ts_ms, username, text } => {
                            st.messages.lock().unwrap()
                                .push(ChatMessage { ts_ms, username, text });
                        }
                    }
                } else {
                    break;
                }
            }
        });
    });
}

/// >>> Добавлено: методы совместимости, которые ждёт твой app.rs
impl AppState {
    /// Раньше вызывалось из UI. Сейчас помпа уже запускается в `make_runtime`,
    /// поэтому тут просто лог, чтобы не ломать вызовы.
    pub fn start_background(self: &Arc<Self>) {
        log::info!("state: start_background (noop — уже запущено)");
    }

    /// Подключение без параметров — берём host/port/nick из конфига.
    pub async fn connect(self: &Arc<Self>) {
        let cfg = self.cfg.lock().unwrap().clone();
        *self.status_text.lock().unwrap() =
            format!("Подключение к {}:{}…", cfg.last_host, cfg.last_port);
        let _ = self.net.tx_cmd.send(NetCmd::Connect {
            host: cfg.last_host,
            port: cfg.last_port,
            nick: cfg.last_nick,
        }).await;
    }

    /// Отправка сообщения (как раньше: async Result)
    pub async fn send_message(self: &Arc<Self>, text: String) -> anyhow::Result<()> {
        // простой анти-флуд на основе токен-бакета
        if !self.rate.try_take() {
            *self.status_text.lock().unwrap() = "Слишком часто".into();
            return Ok(());
        }
        self.net.tx_cmd
            .send(NetCmd::SendText(text))
            .await
            .map_err(|e| anyhow::anyhow!(e.to_string()))
    }

    /// Поиск по локальной истории
    pub fn search(self: &Arc<Self>, q: &str) -> Vec<ChatMessage> {
        let ql = q.trim().to_lowercase();
        if ql.is_empty() {
            return Vec::new();
        }
        self.messages
            .lock()
            .unwrap()
            .iter()
            .filter(|m| {
                m.username.to_lowercase().contains(&ql) ||
                m.text.to_lowercase().contains(&ql)
            })
            .cloned()
            .collect()
    }

    /// Форматирование таймстампа для UI
    pub fn format_ts(ts_ms: u64) -> String {
        use std::time::{Duration, UNIX_EPOCH};
        let st = UNIX_EPOCH + Duration::from_millis(ts_ms);
        let dt: chrono::DateTime<Local> = st.into();
        dt.format("%H:%M:%S").to_string()
    }
}

/// Утилиты — прокси для UI
impl Runtime {
    pub fn send_text(&self, text: String) {
        if !self.0.rate.try_take() {
            *self.0.status_text.lock().unwrap() = "Слишком часто".into();
            return;
        }
        let _ = self.0.net.tx_cmd.try_send(NetCmd::SendText(text));
    }

    pub fn connect(&self, host: String, port: u16, nick: String) {
        *self.0.status_text.lock().unwrap() =
            format!("Подключение к {host}:{port}…");
        let _ = self.0.net.tx_cmd.try_send(NetCmd::Connect { host, port, nick });
    }

    pub fn disconnect(&self) {
        let _ = self.0.net.tx_cmd.try_send(NetCmd::Disconnect);
    }
}
