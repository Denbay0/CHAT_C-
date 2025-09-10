use byteorder::{BigEndian, ReadBytesExt};
use bytes::{BufMut, BytesMut};
use std::io::{Cursor, Read};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FrameType {
    Hello = 0x01,
    Msg = 0x02,
    Err = 0x05,
    Ok = 0x06,
    MsgBroadcast = 0x12,
}

impl FrameType {
    pub fn from_u8(v: u8) -> Option<Self> {
        Some(match v {
            0x01 => Self::Hello,
            0x02 => Self::Msg,
            0x05 => Self::Err,
            0x06 => Self::Ok,
            0x12 => Self::MsgBroadcast,
            _ => return None,
        })
    }
}

#[derive(Debug, Clone)]
pub enum Frame {
    Hello { username: String },
    Msg { text: String },
    Ok,
    Err { text: String },
    MsgBroadcast { ts_ms: u64, username: String, text: String },
}

pub fn encode(frame: &Frame) -> BytesMut {
    let mut payload = BytesMut::new();
    match frame {
        Frame::Hello { username } => {
            payload.put_u16(username.len() as u16);
            payload.extend_from_slice(username.as_bytes());
            wrap(FrameType::Hello, &payload)
        }
        Frame::Msg { text } => {
            payload.put_u32(text.len() as u32);
            payload.extend_from_slice(text.as_bytes());
            wrap(FrameType::Msg, &payload)
        }
        Frame::Ok => wrap(FrameType::Ok, &payload),
        Frame::Err { text } => {
            payload.put_u32(text.len() as u32);
            payload.extend_from_slice(text.as_bytes());
            wrap(FrameType::Err, &payload)
        }
        Frame::MsgBroadcast { ts_ms, username, text } => {
            payload.put_u64(*ts_ms);
            payload.put_u16(username.len() as u16);
            payload.extend_from_slice(username.as_bytes());
            payload.put_u32(text.len() as u32);
            payload.extend_from_slice(text.as_bytes());
            wrap(FrameType::MsgBroadcast, &payload)
        }
    }
}

fn wrap(ft: FrameType, payload: &BytesMut) -> BytesMut {
    let mut out = BytesMut::new();
    out.put_u8(ft as u8);
    out.put_u32(payload.len() as u32);
    out.extend_from_slice(payload);
    out
}

pub fn decode(buf: &[u8]) -> anyhow::Result<Frame> {
    if buf.len() < 5 {
        anyhow::bail!("frame too short");
    }
    let ft = FrameType::from_u8(buf[0]).ok_or_else(|| anyhow::anyhow!("unknown frame type"))?;
    let mut c = Cursor::new(&buf[1..5]);
    let len = c.read_u32::<BigEndian>()? as usize;
    if buf.len() < 5 + len {
        anyhow::bail!("incomplete frame");
    }
    let mut payload: &[u8] = &buf[5..5 + len];

    match ft {
        FrameType::Hello => {
            let ulen = payload.read_u16::<BigEndian>()? as usize;
            let mut name = vec![0u8; ulen];
            payload.read_exact(&mut name)?;
            Ok(Frame::Hello {
                username: String::from_utf8_lossy(&name).to_string(),
            })
        }
        FrameType::Msg => {
            let mlen = payload.read_u32::<BigEndian>()? as usize;
            let mut msg = vec![0u8; mlen];
            payload.read_exact(&mut msg)?;
            Ok(Frame::Msg {
                text: String::from_utf8_lossy(&msg).to_string(),
            })
        }
        FrameType::Ok => Ok(Frame::Ok),
        FrameType::Err => {
            let elen = payload.read_u32::<BigEndian>()? as usize;
            let mut err = vec![0u8; elen];
            payload.read_exact(&mut err)?;
            Ok(Frame::Err {
                text: String::from_utf8_lossy(&err).to_string(),
            })
        }
        FrameType::MsgBroadcast => {
            let ts = payload.read_u64::<BigEndian>()?;
            let ulen = payload.read_u16::<BigEndian>()? as usize;
            let mut name = vec![0u8; ulen];
            payload.read_exact(&mut name)?;
            let mlen = payload.read_u32::<BigEndian>()? as usize;
            let mut msg = vec![0u8; mlen];
            payload.read_exact(&mut msg)?;
            Ok(Frame::MsgBroadcast {
                ts_ms: ts,
                username: String::from_utf8_lossy(&name).to_string(),
                text: String::from_utf8_lossy(&msg).to_string(),
            })
        }
    }
}