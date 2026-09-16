"""Machine-readable serial bridge for the sci-calcme SerialKeyboard firmware."""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import asdict, is_dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
SERIAL_KEYBOARD_PC = REPO_ROOT / "games" / "SerialKeyboard" / "pc"
sys.path.insert(0, str(SERIAL_KEYBOARD_PC))


def emit(value: dict) -> None:
    print(json.dumps(value, ensure_ascii=False, separators=(",", ":")), flush=True)


def list_ports() -> int:
    try:
        from serial.tools import list_ports
    except Exception as exc:  # pragma: no cover - depends on local Python setup
        emit({"type": "error", "message": f"pyserial unavailable: {exc}"})
        return 1

    for item in list_ports.comports():
        emit({
            "type": "port",
            "device": item.device,
            "description": item.description or "",
            "manufacturer": item.manufacturer or "",
            "vid": item.vid,
            "pid": item.pid,
            "serial_number": item.serial_number or "",
        })
    return 0


def connect(port: str) -> int:
    try:
        from serial_keyboard import SerialKeyboardReceiver
    except Exception as exc:  # pragma: no cover - depends on local Python setup
        emit({"type": "error", "message": f"SerialKeyboard module unavailable: {exc}"})
        return 1

    try:
        # SerialKeyboardReceiver opens the read-only port in its constructor.
        receiver = SerialKeyboardReceiver(port)
    except Exception as exc:
        emit({"type": "error", "message": f"Could not open {port}: {exc}"})
        return 1

    try:
        for record in receiver.records():
            if record is None or not is_dataclass(record):
                continue
            payload = asdict(record)
            name = type(record).__name__
            if name == "DeviceInfoRecord":
                payload["type"] = "device_info"
            elif name == "KeyEventRecord":
                payload["type"] = "key"
            elif name == "StateRecord":
                payload["type"] = "state"
            else:
                continue
            emit(payload)
    except KeyboardInterrupt:
        return 0
    except Exception as exc:
        emit({"type": "error", "message": str(exc)})
        return 1
    finally:
        try:
            receiver.close()
        except Exception:
            pass
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="SerialKeyboard NDJSON bridge")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--port")
    group.add_argument("--list-ports", action="store_true")
    args = parser.parse_args()
    return list_ports() if args.list_ports else connect(args.port)


if __name__ == "__main__":
    raise SystemExit(main())
