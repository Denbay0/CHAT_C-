#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Простой LAN-чат клиент под наш бинарный протокол:

Кадр = type(1B) + length(4B BE) + payload
- HELLO(0x01): payload = username (utf-8)
- MSG(0x02)  : payload = text (utf-8)
- OK(0x06) / ERR(0x05)
- MSG_BROADCAST(0x12):
    payload = ts_ms(8BE) + ulen(2BE) + username(ulen) + mlen(4BE) + message(mlen)

Запуск:
  python client.py --host 127.0.0.1 --port 5555 --user Alice
Команды:
  /quit — выйти
"""

import argparse
import socket
import struct
import sys
import threading
import time
from datetime import datetime

# Типы кадров
HELLO = 0x01
MSG   = 0x02
ERR   = 0x05
OK    = 0x06
MSG_BROADCAST = 0x12

CONNECT_TIMEOUT_SEC = 10.0     # таймаут установления соединения
SOCKET_TIMEOUT_SEC  = 600.0    # таймаут операций после подключения (10 минут)

def read_exact(sock: socket.socket, n: int) -> bytes:
    """Читает ровно n байт или бросает EOFError при закрытии соединения/таймауте."""
    buf = bytearray()
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise EOFError("connection closed by peer")
        buf.extend(chunk)
    return bytes(buf)

def send_frame(sock: socket.socket, ftype: int, payload: bytes) -> None:
    """Отправить кадр по протоколу."""
    hdr = struct.pack(">BI", ftype, len(payload))  # 1 байт тип, 4 байта длина (big-endian)
    sock.sendall(hdr + payload)

def recv_frame(sock: socket.socket):
    """Принять кадр: возвращает (type, payload: bytes)."""
    hdr = read_exact(sock, 5)
    ftype, length = struct.unpack(">BI", hdr)
    payload = read_exact(sock, length) if length else b""
    return ftype, payload

def parse_broadcast_payload(payload: bytes):
    """
    Разбор payload MSG_BROADCAST:
    ts_ms(8BE) + ulen(2BE) + username(ulen) + mlen(4BE) + message(mlen)
    """
    if len(payload) < 8 + 2 + 4:
        raise ValueError("broadcast payload too short")
    ts_ms = struct.unpack(">Q", payload[0:8])[0]
    ulen  = struct.unpack(">H", payload[8:10])[0]
    pos = 10
    if len(payload) < pos + ulen + 4:
        raise ValueError("broadcast payload truncated (username)")
    username = payload[pos:pos+ulen].decode("utf-8", errors="replace")
    pos += ulen
    mlen = struct.unpack(">I", payload[pos:pos+4])[0]
    pos += 4
    if len(payload) < pos + mlen:
        raise ValueError("broadcast payload truncated (message)")
    message = payload[pos:pos+mlen].decode("utf-8", errors="replace")
    return ts_ms, username, message

def fmt_time_ms(ts_ms: int) -> str:
    try:
        return datetime.fromtimestamp(ts_ms/1000.0).strftime("%Y-%m-%d %H:%M:%S")
    except Exception:
        return str(ts_ms)

def receiver_loop(sock: socket.socket, stop_ev: threading.Event):
    """Поток приёма: печатает всё входящее (история + live)."""
    try:
        while not stop_ev.is_set():
            ftype, payload = recv_frame(sock)
            if ftype == OK:
                print("[server] OK")
            elif ftype == ERR:
                print("[server] ERR:", payload.decode("utf-8", errors="replace"))
            elif ftype == MSG_BROADCAST:
                try:
                    ts_ms, user, text = parse_broadcast_payload(payload)
                    print(f"[{fmt_time_ms(ts_ms)}] {user}: {text}")
                except Exception as e:
                    print("[client] failed to parse broadcast:", e)
            else:
                # неизвестные кадры игнорим
                pass
    except EOFError:
        print("[client] connection closed by server")
    except socket.timeout:
        print("[client] socket error: timed out")
    except OSError as e:
        print(f"[client] socket error: {e}")
    finally:
        stop_ev.set()

def stdin_loop(sock: socket.socket, stop_ev: threading.Event):
    """Поток чтения stdin: каждую строку отправляет как MSG; /quit — выход."""
    try:
        for line in sys.stdin:
            if stop_ev.is_set():
                break
            line = line.rstrip("\r\n")
            if not line:
                continue
            if line == "/quit":
                stop_ev.set()
                break
            payload = line.encode("utf-8")
            send_frame(sock, MSG, payload)
    except (BrokenPipeError, OSError, EOFError, socket.timeout):
        pass
    finally:
        stop_ev.set()

def main():
    ap = argparse.ArgumentParser(description="LAN Chat Python client")
    ap.add_argument("--host", default="127.0.0.1", help="Server host (default: 127.0.0.1)")
    ap.add_argument("--port", type=int, default=5555, help="Server port (default: 5555)")
    ap.add_argument("--user", required=True, help="Username")
    args = ap.parse_args()

    # Создаём TCP-соединение
    try:
        sock = socket.create_connection((args.host, args.port), timeout=CONNECT_TIMEOUT_SEC)
        # Увеличим таймаут операций (recv/send) до 10 минут
        sock.settimeout(SOCKET_TIMEOUT_SEC)
    except OSError as e:
        print(f"[client] cannot connect to {args.host}:{args.port} -> {e}")
        return 2

    # Отправляем HELLO
    try:
        uname = args.user.encode("utf-8")
        send_frame(sock, HELLO, uname)
    except OSError as e:
        print(f"[client] send HELLO failed: {e}")
        sock.close()
        return 3

    stop_ev = threading.Event()

    # Поток приёма
    t_recv = threading.Thread(target=receiver_loop, args=(sock, stop_ev), daemon=True)
    t_recv.start()

    # Поток ввода
    t_in = threading.Thread(target=stdin_loop, args=(sock, stop_ev), daemon=True)
    t_in.start()

    try:
        while not stop_ev.is_set():
            time.sleep(0.1)
    except KeyboardInterrupt:
        pass
    finally:
        stop_ev.set()
        try:
            sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        sock.close()
        t_recv.join(timeout=1.0)
        t_in.join(timeout=1.0)
        print("[client] bye")

if __name__ == "__main__":
    main()
