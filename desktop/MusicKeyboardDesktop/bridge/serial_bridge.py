#!/usr/bin/env python3
"""Small NDJSON bridge for the read-only SCI-CALC serial keyboard protocol."""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import asdict, is_dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "games" / "SerialKeyboard" / "pc"))

from serial_keyboard import (  # noqa: E402
    DeviceInfoRecord,
    KeyEventRecord,
    Packet,
    SerialKeyboardReceiver,
    StateRecord,
)


def emit(record_type: str, value: object) -> None:
    if is_dataclass(value):
        payload = asdict(value)
    elif isinstance(value, dict):
        payload = dict(value)
    else:
        payload = {"value": value}
    payload["type"] = record_type
    if "modifiers" in payload:
        payload["modifiers"] = list(payload["modifiers"])
    print(json.dumps(payload, ensure_ascii=False, separators=(",", ":")), flush=True)


def emit_ports() -> int:
    try:
        from serial.tools import list_ports as serial_ports
    except ImportError as error:
        emit("error", {"message": str(error)})
        return 1
    for port in serial_ports.comports():
        emit("port", {
            "device": port.device,
            "description": port.description or "",
            "manufacturer": port.manufacturer or "",
            "serial_number": port.serial_number or "",
        })
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port")
    parser.add_argument("--list-ports", action="store_true")
    args = parser.parse_args()
    if args.list_ports:
        return emit_ports()
    if not args.port:
        parser.error("--port is required unless --list-ports is used")

    try:
        with SerialKeyboardReceiver(args.port) as receiver:
            for record in receiver.records():
                if record is None:
                    continue
                if isinstance(record, DeviceInfoRecord):
                    emit("device_info", record)
                elif isinstance(record, KeyEventRecord):
                    emit("key", record)
                elif isinstance(record, StateRecord):
                    emit("state", record)
                elif isinstance(record, Packet):
                    emit("packet", record)
    except KeyboardInterrupt:
        return 0
    except Exception as error:  # Keep the Electron side machine-readable.
        emit("error", {"message": str(error)})
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
