#!/usr/bin/env python3
"""Receive SCI-CALC key events without injecting operating-system input."""

from __future__ import annotations

import argparse
import json
import os
import struct
import sys
import time
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path
from typing import BinaryIO, Iterator


MAGIC = b"SKB1"
PROTOCOL_VERSION = 1
BAUD_RATE = 115200
MAX_PAYLOAD = 64

PACKET_DEVICE_INFO = 0x01
PACKET_KEY_EVENT = 0x02
PACKET_STATE = 0x03

ACTION_PRESS = 1
ACTION_RELEASE = 2
ACTION_REPEAT = 3

HEADER = struct.Struct("<BBHIH")
DEVICE_INFO = struct.Struct("<BBBBIHI")
KEY_EVENT = struct.Struct("<BBBBBBII")
STATE = struct.Struct("<III")

KEY_NAMES = (
    "ESC", "/", "*", "-", "SIN", "SEC",
    "7", "8", "9", "+", "COS", "CSC",
    "4", "5", "6", "^", "TAN", "COT",
    "1", "2", "3", "ENTER", "EXP", "RPN/NORM",
    "SHIFT", "0", ".", "BACKSPACE", "ANGLE", "TAB",
)

ACTION_NAMES = {
    ACTION_PRESS: "PRESS",
    ACTION_RELEASE: "RELEASE",
    ACTION_REPEAT: "REPEAT",
}


class ProtocolError(RuntimeError):
    pass


@dataclass(frozen=True)
class Packet:
    version: int
    packet_type: int
    sequence: int
    device_ms: int
    payload: bytes


@dataclass(frozen=True)
class DeviceInfoRecord:
    firmware: int
    rows: int
    cols: int
    key_count: int
    baud: int
    capabilities: int
    boot_id: int
    sequence: int
    device_ms: int


@dataclass(frozen=True)
class KeyEventRecord:
    key_id: int
    key: str
    action_id: int
    action: str
    row: int
    col: int
    modifiers: tuple[str, ...]
    held_ms: int
    state_mask: int
    sequence: int
    device_ms: int


@dataclass(frozen=True)
class StateRecord:
    state_mask: int
    event_count: int
    dropped_events: int
    sequence: int
    device_ms: int


def crc16_ccitt(data: bytes, initial: int = 0xFFFF) -> int:
    crc = initial
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def build_packet(
    packet_type: int,
    sequence: int,
    device_ms: int,
    payload: bytes = b"",
    version: int = PROTOCOL_VERSION,
) -> bytes:
    """Build a reference packet for tests and protocol tooling."""
    if len(payload) > MAX_PAYLOAD:
        raise ValueError("payload is too large")
    header = HEADER.pack(
        version,
        packet_type,
        sequence & 0xFFFF,
        device_ms & 0xFFFFFFFF,
        len(payload),
    )
    checksum = struct.pack("<H", crc16_ccitt(header + payload))
    return MAGIC + header + payload + checksum


class PacketDecoder:
    """Incrementally recover valid packets from a noisy serial byte stream."""

    def __init__(self) -> None:
        self.buffer = bytearray()
        self.invalid_packets = 0
        self.discarded_bytes = 0

    def feed(self, data: bytes) -> list[Packet]:
        self.buffer.extend(data)
        packets: list[Packet] = []

        while True:
            marker = self.buffer.find(MAGIC)
            if marker < 0:
                keep = min(len(self.buffer), len(MAGIC) - 1)
                self.discarded_bytes += len(self.buffer) - keep
                if keep:
                    del self.buffer[:-keep]
                else:
                    self.buffer.clear()
                break
            if marker:
                self.discarded_bytes += marker
                del self.buffer[:marker]

            minimum = len(MAGIC) + HEADER.size + 2
            if len(self.buffer) < minimum:
                break

            header_start = len(MAGIC)
            header_end = header_start + HEADER.size
            header = bytes(self.buffer[header_start:header_end])
            version, packet_type, sequence, device_ms, payload_length = HEADER.unpack(header)
            if payload_length > MAX_PAYLOAD:
                self.invalid_packets += 1
                del self.buffer[0]
                continue

            packet_size = minimum + payload_length
            if len(self.buffer) < packet_size:
                break

            payload = bytes(self.buffer[header_end:header_end + payload_length])
            received_crc = struct.unpack(
                "<H", self.buffer[header_end + payload_length:packet_size]
            )[0]
            if crc16_ccitt(header + payload) != received_crc:
                self.invalid_packets += 1
                del self.buffer[0]
                continue

            del self.buffer[:packet_size]
            packets.append(Packet(version, packet_type, sequence, device_ms, payload))

        return packets


def decode_packet(packet: Packet) -> DeviceInfoRecord | KeyEventRecord | StateRecord | Packet:
    if packet.version != PROTOCOL_VERSION:
        raise ProtocolError(f"unsupported protocol version {packet.version}")

    if packet.packet_type == PACKET_DEVICE_INFO:
        if len(packet.payload) != DEVICE_INFO.size:
            raise ProtocolError("invalid device-info payload length")
        firmware, rows, cols, key_count, baud, capabilities, boot_id = DEVICE_INFO.unpack(
            packet.payload
        )
        if rows != 5 or cols != 6 or key_count != len(KEY_NAMES) or baud != BAUD_RATE:
            raise ProtocolError("incompatible keyboard geometry or baud rate")
        return DeviceInfoRecord(
            firmware,
            rows,
            cols,
            key_count,
            baud,
            capabilities,
            boot_id,
            packet.sequence,
            packet.device_ms,
        )

    if packet.packet_type == PACKET_KEY_EVENT:
        if len(packet.payload) != KEY_EVENT.size:
            raise ProtocolError("invalid key-event payload length")
        key_id, action_id, row, col, modifier_bits, _, held_ms, state_mask = KEY_EVENT.unpack(
            packet.payload
        )
        if key_id >= len(KEY_NAMES):
            raise ProtocolError(f"invalid key id {key_id}")
        if action_id not in ACTION_NAMES:
            raise ProtocolError(f"invalid key action {action_id}")
        if row >= 5 or col >= 6 or row != key_id // 6 or col != key_id % 6:
            raise ProtocolError("key id does not match its matrix position")
        modifiers = ("SHIFT",) if modifier_bits & 0x01 else ()
        return KeyEventRecord(
            key_id,
            KEY_NAMES[key_id],
            action_id,
            ACTION_NAMES[action_id],
            row,
            col,
            modifiers,
            held_ms,
            state_mask,
            packet.sequence,
            packet.device_ms,
        )

    if packet.packet_type == PACKET_STATE:
        if len(packet.payload) != STATE.size:
            raise ProtocolError("invalid state payload length")
        state_mask, event_count, dropped_events = STATE.unpack(packet.payload)
        return StateRecord(
            state_mask,
            event_count,
            dropped_events,
            packet.sequence,
            packet.device_ms,
        )

    return packet


class SerialKeyboardReceiver:
    """Read-only serial transport. This class never writes to the device."""

    def __init__(self, port_name: str, timeout: float = 0.1) -> None:
        try:
            import serial
        except ImportError as error:
            raise RuntimeError("pyserial is required; install pc/requirements.txt") from error

        port = serial.Serial()
        port.port = port_name
        port.baudrate = BAUD_RATE
        port.timeout = timeout
        port.write_timeout = None
        port.rtscts = False
        port.dsrdtr = False
        port.dtr = False
        port.rts = False
        port.open()
        self.port: BinaryIO = port
        self.decoder = PacketDecoder()

    def close(self) -> None:
        if getattr(self.port, "is_open", False):
            self.port.close()

    def __enter__(self) -> "SerialKeyboardReceiver":
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    def records(
        self,
    ) -> Iterator[DeviceInfoRecord | KeyEventRecord | StateRecord | Packet | None]:
        while True:
            waiting = getattr(self.port, "in_waiting", 0)
            chunk = self.port.read(max(1, min(4096, waiting or 1)))
            if not chunk:
                yield None
                continue
            for packet in self.decoder.feed(chunk):
                yield decode_packet(packet)


class JsonlRecorder:
    def __init__(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        self.path = path
        self.file = path.open("a", encoding="utf-8", buffering=1)

    def close(self) -> None:
        self.file.close()

    def write(self, record_type: str, record: object) -> None:
        data = asdict(record)
        data["type"] = record_type
        data["received_at"] = datetime.now().astimezone().isoformat(timespec="milliseconds")
        if "modifiers" in data:
            data["modifiers"] = list(data["modifiers"])
        self.file.write(json.dumps(data, ensure_ascii=False, separators=(",", ":")) + "\n")


def list_ports() -> None:
    try:
        from serial.tools import list_ports as serial_ports
    except ImportError as error:
        raise RuntimeError("pyserial is required; install pc/requirements.txt") from error

    ports = list(serial_ports.comports())
    if not ports:
        print("No serial ports found.")
        return
    for port in ports:
        print(f"{port.device}\t{port.description}")


def format_event(event: KeyEventRecord) -> str:
    modifiers = "+".join(event.modifiers) if event.modifiers else "-"
    return (
        f"{event.device_ms / 1000:10.3f}s  #{event.sequence:05d}  "
        f"{event.action:<7} key={event.key:<12} "
        f"id={event.key_id:02d} pos=R{event.row + 1}C{event.col + 1} "
        f"mods={modifiers:<5} held={event.held_ms}ms"
    )


def format_key_grid(state_mask: int, ansi: bool = False) -> str:
    """Render all 30 physical keys with a stable UP or DOWN state."""
    lines = []
    for row in range(5):
        cells = []
        for col in range(6):
            key_id = row * 6 + col
            is_down = bool(state_mask & (1 << key_id))
            cell = f"{KEY_NAMES[key_id]:<9} {'DOWN' if is_down else 'UP':>4}"
            if ansi and is_down:
                cell = f"\x1b[7m{cell}\x1b[0m"
            cells.append(cell)
        lines.append(f"R{row + 1}  " + " | ".join(cells))
    return "\n".join(lines)


def format_monitor_frame(
    port_name: str,
    connected: bool,
    boot_id: int | None,
    device_ms: int,
    state_mask: int,
    key_events: int,
    sequence_gaps: int,
    device_drops: int,
    last_event: KeyEventRecord | None,
    log_path: Path | None = None,
    ansi: bool = False,
) -> str:
    boot_text = f"{boot_id:08X}" if boot_id is not None else "--------"
    status = "CONNECTED" if connected else "CONNECTING"
    active_mask = state_mask & ((1 << len(KEY_NAMES)) - 1)
    lines = [
        f"SCI-CALC LIVE KEY MONITOR  {port_name}  {status}",
        (
            f"boot={boot_text}  device={device_ms / 1000:.3f}s  "
            f"down={active_mask.bit_count():02d}/30  events={key_events}  "
            f"gaps={sequence_gaps}  dropped={device_drops}"
        ),
    ]
    if last_event is None:
        lines.append("Last: waiting for a physical key event")
    else:
        modifiers = "+".join(last_event.modifiers) if last_event.modifiers else "-"
        lines.append(
            f"Last: {last_event.action} {last_event.key}  "
            f"R{last_event.row + 1}C{last_event.col + 1}  "
            f"mods={modifiers}  held={last_event.held_ms}ms"
        )
    if log_path is not None:
        lines.append(f"Log: {log_path.resolve()}")
    lines.extend(("", format_key_grid(active_mask, ansi=ansi), "", "Ctrl+C: stop monitoring"))
    return "\n".join(lines)


def enable_ansi(stream: object) -> bool:
    if not getattr(stream, "isatty", lambda: False)():
        return False
    if os.name != "nt":
        return True
    try:
        import ctypes
        import msvcrt

        handle = msvcrt.get_osfhandle(stream.fileno())
        mode = ctypes.c_uint()
        kernel32 = ctypes.windll.kernel32
        if kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
            return bool(kernel32.SetConsoleMode(handle, mode.value | 0x0004))
    except (AttributeError, OSError, ValueError):
        return False
    return False


class LiveKeyMonitor:
    def __init__(self, port_name: str, log_path: Path | None = None) -> None:
        self.port_name = port_name
        self.log_path = log_path
        self.stream = sys.stdout
        self.ansi = enable_ansi(self.stream)
        self.started = False

    def draw(
        self,
        *,
        connected: bool,
        boot_id: int | None,
        device_ms: int,
        state_mask: int,
        key_events: int,
        sequence_gaps: int,
        device_drops: int,
        last_event: KeyEventRecord | None,
    ) -> None:
        frame = format_monitor_frame(
            self.port_name,
            connected,
            boot_id,
            device_ms,
            state_mask,
            key_events,
            sequence_gaps,
            device_drops,
            last_event,
            self.log_path,
            ansi=self.ansi,
        )
        if self.ansi:
            prefix = "\x1b[2J\x1b[?25l" if not self.started else ""
            self.stream.write(prefix + "\x1b[H" + frame + "\x1b[J")
        else:
            self.stream.write(frame + "\n")
        self.stream.flush()
        self.started = True

    def close(self) -> None:
        if self.ansi and self.started:
            self.stream.write("\x1b[?25h\n")
            self.stream.flush()


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Receive and log SCI-CALC physical key events. "
            "No Windows keyboard input is generated."
        )
    )
    parser.add_argument("--port", help="serial port, for example COM7")
    parser.add_argument("--list-ports", action="store_true", help="list available serial ports")
    parser.add_argument(
        "--monitor",
        action="store_true",
        help="show a live 5x6 panel with every physical key's current state",
    )
    parser.add_argument(
        "--log",
        type=Path,
        help="append device information and key events to a UTF-8 JSONL file",
    )
    parser.add_argument("--show-state", action="store_true", help="print one-second state snapshots")
    parser.add_argument("--connect-timeout", type=float, default=6.0)
    parser.add_argument("--duration", type=float, help="stop after this many seconds")
    return parser


def main() -> int:
    parser = build_argument_parser()
    args = parser.parse_args()

    if args.list_ports:
        list_ports()
        return 0
    if not args.port:
        parser.error("--port is required unless --list-ports is used")
    if args.connect_timeout <= 0:
        parser.error("--connect-timeout must be greater than zero")
    if args.duration is not None and args.duration <= 0:
        parser.error("--duration must be greater than zero")

    recorder = JsonlRecorder(args.log) if args.log else None
    started = time.monotonic()
    connected_deadline = started + args.connect_timeout
    connected = False
    last_sequence: int | None = None
    current_boot_id: int | None = None
    key_events = 0
    sequence_gaps = 0
    device_drops = 0
    state_mask = 0
    device_ms = 0
    last_event: KeyEventRecord | None = None
    monitor = LiveKeyMonitor(args.port, args.log) if args.monitor else None

    if monitor:
        monitor.draw(
            connected=False,
            boot_id=None,
            device_ms=0,
            state_mask=0,
            key_events=0,
            sequence_gaps=0,
            device_drops=0,
            last_event=None,
        )
    else:
        print(f"Listening on {args.port} at {BAUD_RATE} baud; press Ctrl+C to stop.")
        print("Logging only: Windows key injection is disabled by design.")
        if recorder:
            print(f"JSONL log: {recorder.path.resolve()}")

    try:
        with SerialKeyboardReceiver(args.port) as receiver:
            for record in receiver.records():
                now = time.monotonic()
                if args.duration is not None and now - started >= args.duration:
                    break

                if record is None:
                    if not connected and now >= connected_deadline:
                        raise TimeoutError(
                            "no valid SKB1 packets received; check the port and firmware"
                        )
                    continue

                if isinstance(record, DeviceInfoRecord):
                    device_ms = record.device_ms
                    if current_boot_id != record.boot_id:
                        current_boot_id = record.boot_id
                        last_sequence = None
                        state_mask = 0
                        last_event = None
                        if not monitor:
                            print(
                                f"Connected: protocol={PROTOCOL_VERSION} "
                                f"firmware={record.firmware} "
                                f"matrix={record.rows}x{record.cols} "
                                f"keys={record.key_count} boot={record.boot_id:08X}"
                            )
                        if recorder:
                            recorder.write("device_info", record)
                    connected = True
                elif not connected and now >= connected_deadline:
                    raise TimeoutError("no valid SKB1 packets received; check the port and firmware")

                if last_sequence is not None:
                    missing = (record.sequence - last_sequence - 1) & 0xFFFF
                    if missing:
                        sequence_gaps += missing
                        if not monitor:
                            print(
                                f"WARNING: {missing} serial packet(s) missing "
                                f"before #{record.sequence:05d}"
                            )
                last_sequence = record.sequence

                if isinstance(record, KeyEventRecord):
                    key_events += 1
                    device_ms = record.device_ms
                    state_mask = record.state_mask
                    last_event = record
                    if not monitor:
                        print(format_event(record), flush=True)
                    if recorder:
                        recorder.write("key", record)
                elif isinstance(record, StateRecord):
                    device_ms = record.device_ms
                    state_mask = record.state_mask
                    device_drops = record.dropped_events
                    if args.show_state and not monitor:
                        print(
                            f"STATE events={record.event_count} dropped={record.dropped_events} "
                            f"mask=0x{record.state_mask:08X}"
                        )
                elif isinstance(record, Packet):
                    device_ms = record.device_ms
                    if not monitor:
                        print(f"Ignoring future packet type 0x{record.packet_type:02X}")

                if monitor:
                    monitor.draw(
                        connected=connected,
                        boot_id=current_boot_id,
                        device_ms=device_ms,
                        state_mask=state_mask,
                        key_events=key_events,
                        sequence_gaps=sequence_gaps,
                        device_drops=device_drops,
                        last_event=last_event,
                    )

                if not connected and now >= connected_deadline:
                    raise TimeoutError("no device-info packet received; check the port and firmware")
    except KeyboardInterrupt:
        print()
    finally:
        if monitor:
            monitor.close()
        if recorder:
            recorder.close()

    print(
        f"Stopped: key_events={key_events} serial_gaps={sequence_gaps} "
        f"device_queue_drops={device_drops}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ProtocolError, RuntimeError, TimeoutError) as error:
        raise SystemExit(f"Error: {error}") from error
