use crate::net::{NetCmd, NetEvent, NetHandle};
use chrono::Local;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};

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
    pub window_focused: AtomicBool,
    pub msg_tx: tokio::sync::watch::Sender<u64>,
}

pub fn make_runtime(cfg: crate::config::AppConfig, _paths: &crate::config::Paths) -> Runtime {
    let net = crate::net::start_net_runtime();

    let (msg_tx, _msg_rx) = tokio::sync::watch::channel(0u64);
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
        window_focused: AtomicBool::new(true),
        msg_tx,
    });

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
                            *st.status_text.lock().unwrap() = format!("Отключено ({reason})");
                            log::info!("state: disconnected ({reason})");
                        }
                        NetEvent::Error { msg } => {
                            *st.status_text.lock().unwrap() = format!("Ошибка: {msg}");
                            log::error!("state: error: {msg}");
                        }
                        NetEvent::Message {
                            ts_ms,
                            username,
                            text,
                        } => {
                            log::info!("state: message from {username}: {text}");
                            let mut msgs = st.messages.lock().unwrap();
                            msgs.push(ChatMessage {
                                ts_ms,
                                username: username.clone(),
                                text: text.clone(),
                            });
                            let cnt = msgs.len() as u64;
                            drop(msgs);
                            let _ = st.msg_tx.send(cnt);
                            #[cfg(windows)]
                            {
                                let notify = {
                                    let cfg = st.cfg.lock().unwrap().notifications_enabled;
                                    cfg && !st.window_focused.load(Ordering::SeqCst)
                                };
                                if notify {
                                    let _ = crate::app::notify_win_toast(&username, &text);
                                }
                            }
                        }
                    }
                } else {
                    break;
                }
            }
        });
    });
}

impl AppState {
    pub fn start_background(self: &Arc<Self>) {
        log::info!("state: start_background (noop — уже запущено)");
    }

    pub async fn connect(self: &Arc<Self>) {
        let cfg = self.cfg.lock().unwrap().clone();
        *self.status_text.lock().unwrap() =
            format!("Подключение к {}:{}…", cfg.last_host, cfg.last_port);
        let rt = Runtime(self.clone());
        rt.connect(cfg.last_host, cfg.last_port, cfg.last_nick);
    }

    pub async fn send_message(self: &Arc<Self>, text: String) -> anyhow::Result<()> {
        Runtime(self.clone()).send_text(text)
    }

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
                m.username.to_lowercase().contains(&ql) || m.text.to_lowercase().contains(&ql)
            })
            .cloned()
            .collect()
    }

    pub fn format_ts(ts_ms: u64) -> String {
        use std::time::{Duration, UNIX_EPOCH};
        let st = UNIX_EPOCH + Duration::from_millis(ts_ms);
        let dt: chrono::DateTime<Local> = st.into();
        dt.format("%H:%M:%S").to_string()
    }
}

impl Runtime {
    pub fn send_text(&self, text: String) -> anyhow::Result<()> {
        if !self.0.rate.try_take() {
            *self.0.status_text.lock().unwrap() = "Слишком часто".into();
            return Ok(());
        }
        log::info!("state: send_message {} bytes", text.len());
        self.0
            .net
            .tx_cmd
            .try_send(NetCmd::SendText(text))
            .map_err(|e| anyhow::anyhow!(e.to_string()))
    }

    pub fn connect(&self, host: String, port: u16, nick: String) {
        *self.0.status_text.lock().unwrap() = format!("Подключение к {host}:{port}…");
        let _ = self
            .0
            .net
            .tx_cmd
            .try_send(NetCmd::Connect { host, port, nick });
    }

    pub fn disconnect(&self) {
        let _ = self.0.net.tx_cmd.try_send(NetCmd::Disconnect);
    }
}
