use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;
use tokio::sync::mpsc;

pub const T_HELLO: u8 = 0x01;
pub const T_MSG: u8 = 0x02;
pub const T_ERR: u8 = 0x05;
pub const T_OK: u8 = 0x06;
pub const T_MSG_BROADCAST: u8 = 0x12;

#[derive(Debug, Clone)]
pub enum NetCmd {
    Connect {
        host: String,
        port: u16,
        nick: String,
    },
    SendText(String),
    Disconnect,
    Stop,
}

#[derive(Debug, Clone)]
pub enum NetEvent {
    Connected,
    Disconnected {
        reason: String,
    },
    Error {
        msg: String,
    },
    Message {
        ts_ms: u64,
        username: String,
        text: String,
    },
}

#[derive(Debug)]
pub struct NetHandle {
    pub tx_cmd: mpsc::Sender<NetCmd>,
    pub rx_evt: std::sync::Mutex<Option<mpsc::Receiver<NetEvent>>>,
}

impl Drop for NetHandle {
    fn drop(&mut self) {
        let _ = self.tx_cmd.try_send(NetCmd::Stop);
    }
}

pub fn start_net_runtime() -> NetHandle {
    let (tx_cmd, mut rx_cmd) = mpsc::channel::<NetCmd>(64);
    let (tx_evt, rx_evt) = mpsc::channel::<NetEvent>(256);

    std::thread::spawn(move || {
        let rt = tokio::runtime::Builder::new_multi_thread()
            .enable_all()
            .build()
            .expect("tokio runtime");

        rt.block_on(async move {
            log::info!("net: runner started");

            let mut writer: Option<tokio::io::WriteHalf<TcpStream>> = None;
            let mut reader_task: Option<tokio::task::JoinHandle<()>> = None;

            let close_conn =
                |reason: &str,
                 reader_task: &mut Option<tokio::task::JoinHandle<()>>,
                 writer: &mut Option<tokio::io::WriteHalf<TcpStream>>| {
                    log::info!("net: close_conn({reason})");
                    if let Some(h) = reader_task.take() {
                        h.abort();
                    }
                    if writer.take().is_some() {}
                };

            loop {
                match rx_cmd.recv().await {
                    None | Some(NetCmd::Stop) => {
                        close_conn("stop", &mut reader_task, &mut writer);
                        let _ = tx_evt
                            .send(NetEvent::Disconnected {
                                reason: "stop".into(),
                            })
                            .await;
                        break;
                    }
                    Some(NetCmd::Disconnect) => {
                        close_conn("user", &mut reader_task, &mut writer);
                        let _ = tx_evt
                            .send(NetEvent::Disconnected {
                                reason: "user".into(),
                            })
                            .await;
                    }
                    Some(NetCmd::Connect { host, port, nick }) => {
                        close_conn("reconnect", &mut reader_task, &mut writer);
                        let addr = format!("{host}:{port}");
                        log::info!("net: connecting to {addr} as '{nick}'");

                        match TcpStream::connect(&addr).await {
                            Err(e) => {
                                let _ = tx_evt
                                    .send(NetEvent::Error {
                                        msg: format!("connect({addr}) failed: {e}"),
                                    })
                                    .await;
                            }
                            Ok(stream) => {
                                let (mut rd, mut wr) = tokio::io::split(stream);

                                if let Err(e) = send_frame(&mut wr, T_HELLO, nick.as_bytes()).await
                                {
                                    let _ = tx_evt
                                        .send(NetEvent::Error {
                                            msg: format!("send HELLO failed: {e}"),
                                        })
                                        .await;
                                    continue;
                                }
                                log::info!("net: HELLO sent, waiting for OK/ERR…");

                                match read_frame(&mut rd).await {
                                    Ok((T_OK, _)) => {
                                        log::info!("net: handshake OK, starting reader loop");
                                        let tx_evt_clone = tx_evt.clone();
                                        reader_task = Some(tokio::spawn(async move {
                                            if let Err(e) = reader_loop(rd, tx_evt_clone).await {
                                                log::error!("net: reader_loop error: {e}");
                                            }
                                        }));
                                        writer = Some(wr);
                                        let _ = tx_evt.send(NetEvent::Connected).await;
                                    }
                                    Ok((T_ERR, payload)) => {
                                        let msg = String::from_utf8_lossy(&payload).to_string();
                                        let _ = tx_evt
                                            .send(NetEvent::Error {
                                                msg: format!("handshake ERR: {msg}"),
                                            })
                                            .await;
                                        let _ = tx_evt
                                            .send(NetEvent::Disconnected {
                                                reason: "handshake_err".into(),
                                            })
                                            .await;
                                    }
                                    Ok((other, _)) => {
                                        let _ = tx_evt
                                            .send(NetEvent::Error {
                                                msg: format!(
                                                    "unexpected handshake frame: 0x{other:02x}"
                                                ),
                                            })
                                            .await;
                                    }
                                    Err(e) => {
                                        let _ = tx_evt
                                            .send(NetEvent::Error {
                                                msg: format!("handshake read failed: {e}"),
                                            })
                                            .await;
                                    }
                                }
                            }
                        }
                    }
                    Some(NetCmd::SendText(text)) => {
                        if let Some(wr) = writer.as_mut() {
                            log::info!("net: sending {} bytes", text.len());
                            if let Err(e) = send_frame(wr, T_MSG, text.as_bytes()).await {
                                let _ = tx_evt
                                    .send(NetEvent::Error {
                                        msg: format!("send text failed: {e}"),
                                    })
                                    .await;
                            }
                        } else {
                            let _ = tx_evt
                                .send(NetEvent::Error {
                                    msg: "not connected".into(),
                                })
                                .await;
                        }
                    }
                }
            }
        });
    });

    NetHandle {
        tx_cmd,
        rx_evt: std::sync::Mutex::new(Some(rx_evt)),
    }
}

async fn send_frame<W: AsyncWriteExt + Unpin>(
    w: &mut W,
    typ: u8,
    payload: &[u8],
) -> anyhow::Result<()> {
    let mut buf = Vec::with_capacity(1 + 4 + payload.len());
    buf.push(typ);
    buf.extend_from_slice(&(payload.len() as u32).to_be_bytes());
    buf.extend_from_slice(payload);
    w.write_all(&buf).await?;
    w.flush().await?;
    Ok(())
}

async fn read_frame<R: AsyncReadExt + Unpin>(r: &mut R) -> anyhow::Result<(u8, Vec<u8>)> {
    let mut hdr = [0u8; 5];
    r.read_exact(&mut hdr).await?;
    let typ = hdr[0];
    let len = u32::from_be_bytes([hdr[1], hdr[2], hdr[3], hdr[4]]) as usize;
    let mut payload = vec![0u8; len];
    if len > 0 {
        r.read_exact(&mut payload).await?;
    }
    Ok((typ, payload))
}

async fn reader_loop<R: AsyncReadExt + Unpin>(
    mut rd: R,
    tx_evt: mpsc::Sender<NetEvent>,
) -> anyhow::Result<()> {
    loop {
        let (typ, payload) = read_frame(&mut rd).await?;
        match typ {
            T_MSG_BROADCAST => {
                if payload.len() < 8 + 2 + 4 {
                    let _ = tx_evt
                        .send(NetEvent::Error {
                            msg: "short MSG_BROADCAST".into(),
                        })
                        .await;
                    continue;
                }
                let mut p = &payload[..];

                let ts_ms = u64::from_be_bytes(p[0..8].try_into().unwrap());
                p = &p[8..];

                let ulen = u16::from_be_bytes(p[0..2].try_into().unwrap()) as usize;
                p = &p[2..];
                if p.len() < ulen + 4 {
                    let _ = tx_evt
                        .send(NetEvent::Error {
                            msg: "bad MSG_BROADCAST (ulen)".into(),
                        })
                        .await;
                    continue;
                }
                let username = String::from_utf8_lossy(&p[..ulen]).to_string();
                p = &p[ulen..];

                let mlen = u32::from_be_bytes(p[0..4].try_into().unwrap()) as usize;
                p = &p[4..];
                if p.len() < mlen {
                    let _ = tx_evt
                        .send(NetEvent::Error {
                            msg: "bad MSG_BROADCAST (mlen)".into(),
                        })
                        .await;
                    continue;
                }
                let text = String::from_utf8_lossy(&p[..mlen]).to_string();
                log::info!("net: recv msg from {username} ({mlen} bytes)");

                let _ = tx_evt
                    .send(NetEvent::Message {
                        ts_ms,
                        username,
                        text,
                    })
                    .await;
            }
            T_OK => {}
            T_ERR => {
                let msg = String::from_utf8_lossy(&payload).to_string();
                let _ = tx_evt.send(NetEvent::Error { msg }).await;
            }
            other => {
                let _ = tx_evt
                    .send(NetEvent::Error {
                        msg: format!("unknown frame 0x{other:02x}"),
                    })
                    .await;
            }
        }
    }
}
