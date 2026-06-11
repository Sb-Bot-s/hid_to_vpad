#!/usr/bin/env python3
"""Keyboard-to-network input client for HID to VPAD.

This small client speaks the HID to VPAD network protocol directly, so it can be
run from a PC, Linux handheld, or Android/Termux without the Java Network Client
and without a physical XInput controller.
"""

from __future__ import annotations

import argparse
import atexit
import os
import select
import socket
import struct
import sys
import termios
import time
import tty
from dataclasses import dataclass, field
from typing import Dict, Iterable, Optional, Set

TCP_PORT = 8112
UDP_PORT = 8113
PROTOCOL_V3 = 0x14
PROTOCOL_ABORT = 0x30

TCP_CMD_ATTACH = 0x01
TCP_CMD_DETACH = 0x02
TCP_CMD_PING = 0xF0
TCP_CMD_PONG = 0xF1
UDP_CMD_DATA = 0x03

ATTACH_CONFIG_FOUND = 0xE0
ATTACH_CONFIG_NOT_FOUND = 0xE1
ATTACH_USERDATA_OKAY = 0xE8
ATTACH_USERDATA_BAD = 0xE9

# Reuse the documented HID to VPAD XInput VID/PID mapping. This script does not
# require an XInput controller; it only emits the same compact 8-byte report so
# existing controller_patcher configs can parse it.
DEFAULT_VID = 0x7331
DEFAULT_PID = 0x1337
DEFAULT_HANDLE = 0x50594B42  # "PYKB"

BUTTON_A = 1 << 0
BUTTON_B = 1 << 1
BUTTON_X = 1 << 2
BUTTON_Y = 1 << 3
BUTTON_LEFT = 1 << 4
BUTTON_UP = 1 << 5
BUTTON_RIGHT = 1 << 6
BUTTON_DOWN = 1 << 7
BUTTON_BACK = 1 << 8
BUTTON_START = 1 << 9
BUTTON_LB = 1 << 10
BUTTON_RB = 1 << 11
BUTTON_L3 = 1 << 12
BUTTON_R3 = 1 << 13
BUTTON_GUIDE = 1 << 15

KEY_BUTTONS = {
    " ": BUTTON_A,
    "j": BUTTON_B,
    "k": BUTTON_X,
    "l": BUTTON_Y,
    "u": BUTTON_LB,
    "i": BUTTON_RB,
    "q": BUTTON_BACK,
    "e": BUTTON_START,
    "h": BUTTON_GUIDE,
}

ARROW_ESCAPE_TO_BUTTON = {
    "A": BUTTON_UP,
    "B": BUTTON_DOWN,
    "C": BUTTON_RIGHT,
    "D": BUTTON_LEFT,
}


@dataclass
class KeyboardState:
    pressed_until: Dict[str, float] = field(default_factory=dict)

    def press(self, key: str, hold_seconds: float) -> None:
        self.pressed_until[key] = time.monotonic() + hold_seconds

    def active_keys(self) -> Set[str]:
        now = time.monotonic()
        expired = [key for key, until in self.pressed_until.items() if until < now]
        for key in expired:
            del self.pressed_until[key]
        return set(self.pressed_until)


def read_exact(sock: socket.socket, size: int) -> bytes:
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError("connection closed by Wii U")
        data.extend(chunk)
    return bytes(data)


def recv_byte(sock: socket.socket) -> int:
    return read_exact(sock, 1)[0]


def connect_tcp(host: str, timeout: float) -> socket.socket:
    sock = socket.create_connection((host, TCP_PORT), timeout=timeout)
    # Enable keepalive to prevent timeout disconnects
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    
    server_version = recv_byte(sock)
    print(f"[handshake] server version: 0x{server_version:02X}")
    if server_version == PROTOCOL_ABORT:
        raise RuntimeError("Wii U aborted before protocol negotiation")

    selected_version = min(server_version, PROTOCOL_V3)
    sock.sendall(bytes([selected_version]))
    print(f"[handshake] sent version: 0x{selected_version:02X}")

    if selected_version != 0x12:
        confirmed_version = recv_byte(sock)
        print(f"[handshake] confirmed version: 0x{confirmed_version:02X}")
        if confirmed_version in (PROTOCOL_ABORT, 0x00):
            raise RuntimeError("Wii U rejected the network protocol version")
        
        # In v3, we check if there's an extra byte waiting, but don't block forever
        sock.settimeout(0.1)
        try:
            extra = sock.recv(1)
            if extra:
                print(f"[handshake] client status: 0x{extra[0]:02X}")
        except socket.timeout:
            pass
        finally:
            sock.settimeout(timeout)

    # Return immediately to send ATTACH fast
    return sock


def attach_device(sock: socket.socket, vid: int, pid: int, handle: int) -> tuple[int, int]:
    print(f"[attach] sending attach for VID=0x{vid:04X} PID=0x{pid:04X} handle=0x{handle:08X} ...")
    try:
        data = struct.pack(">Bihh", TCP_CMD_ATTACH, handle, vid, pid)
        sock.sendall(data)
    except BrokenPipeError:
        print("[attach] error: socket broken during send")
        raise
    except Exception as e:
        print(f"[attach] error during send: {e}")
        raise

    print("[attach] waiting for config status...")
    config_status = recv_byte(sock)
    print(f"[attach] config status: 0x{config_status:02X}")
    if config_status == ATTACH_CONFIG_NOT_FOUND:
        raise RuntimeError(
            "Wii U did not find a controller config for this virtual device. "
            "Copy controller_configs/python_keyboard_xinput.ini to sd:/wiiu/controller/."
        )
    if config_status != ATTACH_CONFIG_FOUND:
        raise RuntimeError(f"unexpected attach config response 0x{config_status:02X}")

    userdata_status = recv_byte(sock)
    print(f"[attach] userdata status: 0x{userdata_status:02X}")
    if userdata_status == ATTACH_USERDATA_BAD:
        raise RuntimeError("Wii U rejected the virtual device user data")
    if userdata_status != ATTACH_USERDATA_OKAY:
        raise RuntimeError(f"unexpected attach userdata response 0x{userdata_status:02X}")

    resp = read_exact(sock, 3)
    device_slot, pad_slot = struct.unpack(">hB", resp)
    print(f"[attach] assigned device_slot={device_slot} pad_slot={pad_slot}")
    if device_slot < 0:
        raise RuntimeError("Wii U returned an invalid device slot")
    return device_slot, pad_slot


def detach_device(sock: Optional[socket.socket], handle: int) -> None:
    if sock is None:
        return
    try:
        sock.sendall(struct.pack(">Bi", TCP_CMD_DETACH, handle))
    except OSError:
        pass


def send_report(
    udp_sock: socket.socket,
    host: str,
    handle: int,
    device_slot: int,
    pad_slot: int,
    report: bytes,
) -> None:
    packet = struct.pack(">BBihBB", UDP_CMD_DATA, 1, handle, device_slot, pad_slot, len(report)) + report
    udp_sock.sendto(packet, (host, UDP_PORT))


def signed_axis(value: int) -> int:
    return max(-128, min(127, value)) & 0xFF


def build_xinput_report(keys: Iterable[str]) -> bytes:
    active = set(keys)
    
    # Left Stick (WASD)
    lx = 0
    ly = 0
    if "w" in active: ly += 127
    if "s" in active: ly -= 128
    if "a" in active: lx -= 128
    if "d" in active: lx += 127
    
    # Right Stick (Arrows)
    rx = 0
    ry = 0
    if "up" in active: ry += 127
    if "down" in active: ry -= 128
    if "left" in active: rx -= 128
    if "right" in active: rx += 127

    buttons = 0
    # Map letters to buttons
    mapping = {
        " ": BUTTON_A,
        "j": BUTTON_B,
        "k": BUTTON_X,
        "l": BUTTON_Y,
        "u": BUTTON_LB,
        "i": BUTTON_RB,
        "o": BUTTON_L3,
        "p": BUTTON_R3,
        "q": BUTTON_BACK,
        "plus": BUTTON_START,
        "minus": BUTTON_BACK,
        "h": BUTTON_GUIDE,
        "z": BUTTON_LB, # ZL/L alternate
        "x": BUTTON_RB, # ZR/R alternate
    }
    
    for key, mask in mapping.items():
        if key in active:
            buttons |= mask

    axes_data = bytes([signed_axis(lx), signed_axis(ly), signed_axis(rx), signed_axis(ry)])
    return axes_data + struct.pack(">I", buttons)


def stdin_keys() -> Iterable[str]:
    while True:
        ready, _, _ = select.select([sys.stdin], [], [], 0)
        if not ready:
            return
        
        # Read available data
        data = os.read(sys.stdin.fileno(), 16).decode(errors="ignore")
        i = 0
        while i < len(data):
            char = data[i]
            if char == "\x1b":  # Escape sequence
                if i + 2 < len(data) and data[i+1] == "[":
                    code = data[i+2]
                    if code == "A": yield "up"
                    elif code == "B": yield "down"
                    elif code == "C": yield "right"
                    elif code == "D": yield "left"
                    i += 3
                    continue
            
            if char == "\x03": # Ctrl+C
                raise KeyboardInterrupt
            elif char == "\r" or char == "\n":
                yield "plus"
            elif char == "\x7f" or char == "\x08": # Backspace / Delete
                yield "minus"
            else:
                yield char.lower()
            i += 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Send keyboard input to HID to VPAD over the network.")
    parser.add_argument("host", help="Wii U IP address")
    parser.add_argument("--rate", type=float, default=60.0, help="UDP reports per second (default: 60)")
    parser.add_argument("--hold", type=float, default=0.18, help="seconds a key remains active after each repeat (default: 0.18)")
    parser.add_argument("--vid", type=lambda x: int(x, 0), default=DEFAULT_VID, help="virtual VID (default: 0x7331)")
    parser.add_argument("--pid", type=lambda x: int(x, 0), default=DEFAULT_PID, help="virtual PID (default: 0x1337)")
    parser.add_argument("--handle", type=lambda x: int(x, 0), default=DEFAULT_HANDLE, help="virtual HID handle")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    tcp_sock: Optional[socket.socket] = None
    old_term = None

    def restore_terminal() -> None:
        if old_term is not None:
            termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, old_term)

    try:
        try:
            old_term = termios.tcgetattr(sys.stdin.fileno())
            tty.setcbreak(sys.stdin.fileno())
            atexit.register(restore_terminal)
        except (termios.error, Exception):
            print("Warning: Could not set up terminal for interactive keyboard input (not a TTY). Network only mode.")

        print(f"Connecting to Wii U {args.host}:{TCP_PORT} ...")
        tcp_sock = connect_tcp(args.host, timeout=5.0)
        atexit.register(lambda: detach_device(tcp_sock, args.handle))
        device_slot, pad_slot = attach_device(tcp_sock, args.vid, args.pid, args.handle)
        udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        state = KeyboardState()
        frame_time = 1.0 / args.rate

        print("Connected. Controls: WASD=left stick, arrows=right stick, Space=A, J=B, K=X, L=Y, U/I=L/R, Q=-, E/Enter=+, Ctrl+C=quit")
        while True:
            for key in stdin_keys():
                if key in {"up", "down", "left", "right"}:
                    state.press(key, args.hold)
                else:
                    state.press(key, args.hold)
            report = build_xinput_report(state.active_keys())
            send_report(udp_sock, args.host, args.handle, device_slot, pad_slot, report)
            time.sleep(frame_time)
    except KeyboardInterrupt:
        print("\nDisconnecting...")
        return 0
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    finally:
        detach_device(tcp_sock, args.handle)
        if tcp_sock is not None:
            tcp_sock.close()


if __name__ == "__main__":
    raise SystemExit(main())
