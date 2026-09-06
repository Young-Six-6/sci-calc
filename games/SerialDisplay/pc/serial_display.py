#!/usr/bin/env python3
"""SCI-CALC USB Display host client."""

from __future__ import annotations

import argparse
import struct
import sys
import textwrap
import time
from functools import lru_cache
from pathlib import Path
from typing import BinaryIO

MAGIC = b"SCD1"
PROTOCOL_VERSION = 1
BAUD_RATE = 921600
SCREEN_WIDTH = 256
SCREEN_HEIGHT = 64
FRAME_BYTES = SCREEN_WIDTH * SCREEN_HEIGHT // 8
MAX_PAYLOAD = 4096
LEGACY_WRITE_CHUNK = 128
LEGACY_WRITE_PAUSE = 0.002

CMD_HELLO = 0x01
CMD_FRAME_RAW = 0x02
CMD_FRAME_RLE = 0x03
CMD_CLEAR = 0x04
CMD_CONTRAST = 0x05
CMD_PING = 0x06
CMD_EXIT = 0x07
CMD_ACK = 0x80
CMD_INFO = 0x81
CMD_NACK = 0xFF

BIT_REVERSE = bytes(
    int(f"{value:08b}"[::-1], 2) for value in range(256)
)


class ProtocolError(RuntimeError):
    pass


def crc16_ccitt(data: bytes, initial: int = 0xFFFF) -> int:
    crc = initial
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def build_packet(command: int, sequence: int, payload: bytes = b"") -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError("payload is too large")
    metadata = struct.pack("<BBHH", PROTOCOL_VERSION, command, sequence & 0xFFFF, len(payload))
    checksum = crc16_ccitt(metadata + payload)
    return MAGIC + metadata + payload + struct.pack("<H", checksum)


def parse_packet_bytes(packet: bytes) -> tuple[int, int, bytes]:
    if len(packet) < 12 or packet[:4] != MAGIC:
        raise ProtocolError("invalid packet marker")
    version, command, sequence, length = struct.unpack("<BBHH", packet[4:10])
    if version != PROTOCOL_VERSION:
        raise ProtocolError(f"unsupported protocol version {version}")
    if length > MAX_PAYLOAD or len(packet) != 12 + length:
        raise ProtocolError("invalid payload length")
    payload = packet[10:-2]
    received_crc = struct.unpack("<H", packet[-2:])[0]
    if crc16_ccitt(packet[4:-2]) != received_crc:
        raise ProtocolError("CRC mismatch")
    return command, sequence, payload


def rle_encode(data: bytes) -> bytes:
    if not data:
        return b""
    output = bytearray()
    run_value = data[0]
    run_length = 1
    for value in data[1:]:
        if value == run_value and run_length < 255:
            run_length += 1
        else:
            output.extend((run_length, run_value))
            run_value = value
            run_length = 1
    output.extend((run_length, run_value))
    return bytes(output)


def rle_decode(data: bytes, expected_size: int = FRAME_BYTES) -> bytes:
    if not data or len(data) % 2:
        raise ProtocolError("invalid RLE payload")
    output = bytearray()
    for index in range(0, len(data), 2):
        count, value = data[index], data[index + 1]
        if not count or len(output) + count > expected_size:
            raise ProtocolError("invalid RLE run")
        output.extend(bytes((value,)) * count)
    if len(output) != expected_size:
        raise ProtocolError("incomplete RLE frame")
    return bytes(output)


def read_exact(port: BinaryIO, size: int, deadline: float) -> bytes:
    output = bytearray()
    while len(output) < size:
        if time.monotonic() >= deadline:
            raise TimeoutError("serial response timed out")
        chunk = port.read(size - len(output))
        if chunk:
            output.extend(chunk)
    return bytes(output)


def read_packet(port: BinaryIO, timeout: float) -> tuple[int, int, bytes]:
    deadline = time.monotonic() + timeout
    matched = 0
    while time.monotonic() < deadline:
        value = read_exact(port, 1, deadline)[0]
        if value == MAGIC[matched]:
            matched += 1
            if matched != len(MAGIC):
                continue
            metadata = read_exact(port, 6, deadline)
            version, _, _, length = struct.unpack("<BBHH", metadata)
            if version != PROTOCOL_VERSION or length > MAX_PAYLOAD:
                matched = 0
                continue
            tail = read_exact(port, length + 2, deadline)
            try:
                return parse_packet_bytes(MAGIC + metadata + tail)
            except ProtocolError:
                matched = 0
        else:
            matched = 1 if value == MAGIC[0] else 0
    raise TimeoutError("serial response timed out")


class SerialDisplayClient:
    def __init__(self, port_name: str, response_timeout: float = 2.0) -> None:
        try:
            import serial
        except ImportError as error:
            raise RuntimeError("pyserial is required; install pc/requirements.txt") from error

        port = serial.Serial()
        port.port = port_name
        port.baudrate = BAUD_RATE
        port.timeout = 0.05
        port.write_timeout = 2.0
        port.rtscts = False
        port.dsrdtr = False
        port.dtr = False
        port.rts = False
        port.open()
        self.port = port
        self.response_timeout = response_timeout
        self.sequence = 0
        time.sleep(1.8)
        self.port.reset_input_buffer()
        self.port.reset_output_buffer()
        self.info = self._handshake()

    def close(self) -> None:
        if self.port.is_open:
            self.port.close()

    def __enter__(self) -> "SerialDisplayClient":
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    def _next_sequence(self) -> int:
        self.sequence = (self.sequence + 1) & 0xFFFF
        return self.sequence

    def _request(self, command: int, payload: bytes = b"", expected: int = CMD_ACK) -> bytes:
        sequence = self._next_sequence()
        packet = build_packet(command, sequence, payload)
        firmware_revision = getattr(self, "info", {}).get("firmware", 0)
        if len(packet) > LEGACY_WRITE_CHUNK and firmware_revision < 2:
            for offset in range(0, len(packet), LEGACY_WRITE_CHUNK):
                self.port.write(packet[offset:offset + LEGACY_WRITE_CHUNK])
                self.port.flush()
                time.sleep(LEGACY_WRITE_PAUSE)
        else:
            self.port.write(packet)
            self.port.flush()
        deadline = time.monotonic() + self.response_timeout
        while time.monotonic() < deadline:
            response_command, response_sequence, response_payload = read_packet(
                self.port, max(0.05, deadline - time.monotonic())
            )
            if response_sequence != sequence:
                continue
            if response_command == CMD_NACK:
                code = response_payload[1] if len(response_payload) > 1 else -1
                raise ProtocolError(f"device rejected command 0x{command:02X}, error {code}")
            if response_command != expected:
                raise ProtocolError(f"unexpected response 0x{response_command:02X}")
            if expected == CMD_ACK and (len(response_payload) != 2 or response_payload[0] != command):
                raise ProtocolError("invalid acknowledgement")
            return response_payload
        raise TimeoutError("device did not acknowledge the command")

    def _handshake(self) -> dict[str, int]:
        last_error: Exception | None = None
        for _ in range(4):
            try:
                payload = self._request(CMD_HELLO, expected=CMD_INFO)
                if len(payload) != 14:
                    raise ProtocolError("invalid device information")
                width, height = struct.unpack("<HH", payload[:4])
                baud = struct.unpack("<I", payload[6:10])[0]
                max_payload = struct.unpack("<H", payload[10:12])[0]
                if (width, height, baud) != (SCREEN_WIDTH, SCREEN_HEIGHT, BAUD_RATE):
                    raise ProtocolError("incompatible display geometry or baud rate")
                return {
                    "width": width,
                    "height": height,
                    "pixel_format": payload[4],
                    "capabilities": payload[5],
                    "baud": baud,
                    "max_payload": max_payload,
                    "protocol": payload[12],
                    "firmware": payload[13],
                }
            except (TimeoutError, ProtocolError) as error:
                last_error = error
                self.port.reset_input_buffer()
                time.sleep(0.25)
        raise RuntimeError(f"USB Display handshake failed: {last_error}")

    def send_frame(self, frame: bytes, allow_rle: bool = True) -> str:
        if len(frame) != FRAME_BYTES:
            raise ValueError(f"frame must contain {FRAME_BYTES} bytes")
        compressed = rle_encode(frame) if allow_rle else b""
        if compressed and len(compressed) < len(frame):
            self._request(CMD_FRAME_RLE, compressed)
            return "rle"
        self._request(CMD_FRAME_RAW, frame)
        return "raw"

    def set_contrast(self, value: int) -> None:
        self._request(CMD_CONTRAST, bytes((max(0, min(255, value)),)))

    def clear(self, white: bool = False) -> None:
        self._request(CMD_CLEAR, bytes((1 if white else 0,)))

    def exit_device(self) -> None:
        self._request(CMD_EXIT)


@lru_cache(maxsize=1)
def require_imaging() -> tuple[object, object, object]:
    try:
        from PIL import Image, ImageDraw, ImageOps
    except ImportError as error:
        raise RuntimeError("Pillow is required; install pc/requirements.txt") from error
    return Image, ImageDraw, ImageOps


def pack_mono_image(image: object) -> bytes:
    packed_msb = image.convert("1").tobytes()
    frame = bytes(BIT_REVERSE[value] for value in packed_msb)
    if len(frame) != FRAME_BYTES:
        raise RuntimeError("unexpected converted frame size")
    return frame


def prepare_image(image: object, fit: str, dither: bool, threshold: int, invert: bool) -> bytes:
    Image, _, ImageOps = require_imaging()
    grayscale = image.convert("L")
    size = (SCREEN_WIDTH, SCREEN_HEIGHT)
    resampling = getattr(Image, "Resampling", Image).LANCZOS
    if fit == "stretch":
        grayscale = grayscale.resize(size, resampling)
    elif fit == "cover":
        grayscale = ImageOps.fit(grayscale, size, method=resampling, centering=(0.5, 0.5))
    else:
        grayscale.thumbnail(size, resampling)
        canvas = Image.new("L", size, 0)
        canvas.paste(grayscale, ((SCREEN_WIDTH - grayscale.width) // 2, (SCREEN_HEIGHT - grayscale.height) // 2))
        grayscale = canvas
    if invert:
        grayscale = ImageOps.invert(grayscale)
    if dither:
        dither_mode = getattr(getattr(Image, "Dither", Image), "FLOYDSTEINBERG")
        mono = grayscale.convert("1", dither=dither_mode)
    else:
        mono = grayscale.point(lambda value: 255 if value >= threshold else 0, mode="1")
    return pack_mono_image(mono)


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


def test_pattern() -> object:
    Image, ImageDraw, _ = require_imaging()
    image = Image.new("L", (SCREEN_WIDTH, SCREEN_HEIGHT), 0)
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1), outline=255)
    for x in range(0, SCREEN_WIDTH, 16):
        draw.line((x, 0, x, SCREEN_HEIGHT - 1), fill=80)
    for y in range(0, SCREEN_HEIGHT, 16):
        draw.line((0, y, SCREEN_WIDTH - 1, y), fill=80)
    draw.text((8, 8), "SCI-CALC USB DISPLAY", fill=255)
    draw.text((8, 28), "256 x 64 / SCD1", fill=255)
    draw.text((8, 46), time.strftime("%H:%M:%S"), fill=255)
    return image


def text_image(text: str) -> object:
    Image, ImageDraw, _ = require_imaging()
    image = Image.new("1", (SCREEN_WIDTH, SCREEN_HEIGHT), 0)
    draw = ImageDraw.Draw(image)
    lines = []
    for paragraph in text.splitlines() or [""]:
        lines.extend(textwrap.wrap(paragraph, width=41) or [""])
    draw.multiline_text((3, 2), "\n".join(lines[:6]), fill=1, spacing=1)
    return image


def stats_image(cpu: float, memory: float, cpu_history: list[float], memory_history: list[float]) -> object:
    Image, ImageDraw, _ = require_imaging()
    image = Image.new("1", (SCREEN_WIDTH, SCREEN_HEIGHT), 0)
    draw = ImageDraw.Draw(image)
    draw.text((2, 1), f"CPU {cpu:03.0f}%", fill=1)
    draw.text((70, 1), f"RAM {memory:03.0f}%", fill=1)
    draw.text((205, 1), time.strftime("%H:%M:%S"), fill=1)

    def bar(x: int, value: float) -> None:
        draw.rectangle((x, 13, x + 122, 21), outline=1)
        width = max(0, min(120, round(value * 1.2)))
        if width:
            draw.rectangle((x + 1, 14, x + width, 20), fill=1)

    def graph(x: int, label: str, values: list[float]) -> None:
        draw.rectangle((x, 25, x + 124, 63), outline=1)
        draw.text((x + 3, 27), label, fill=1)
        visible = values[-120:]
        if len(visible) < 2:
            return
        points = []
        start_x = x + 3 + 120 - len(visible)
        for index, value in enumerate(visible):
            y = 61 - round(max(0.0, min(100.0, value)) * 0.26)
            points.append((start_x + index, y))
        draw.line(points, fill=1, width=1)

    bar(2, cpu)
    bar(132, memory)
    graph(2, "CPU", cpu_history)
    graph(130, "RAM", memory_history)
    return image


def stats_loop(client: SerialDisplayClient, args: argparse.Namespace) -> None:
    try:
        import psutil
    except ImportError as error:
        raise RuntimeError("psutil is required; install pc/requirements.txt") from error
    fps = args.fps or 5.0
    frame_interval = 1.0 / fps
    cpu_history: list[float] = []
    memory_history: list[float] = []
    psutil.cpu_percent(interval=None)
    print("Showing CPU/RAM dashboard; press Ctrl+C to stop.")
    while True:
        started = time.monotonic()
        cpu = psutil.cpu_percent(interval=None)
        memory = psutil.virtual_memory().percent
        cpu_history.append(cpu)
        memory_history.append(memory)
        del cpu_history[:-120]
        del memory_history[:-120]
        frame = pack_mono_image(stats_image(cpu, memory, cpu_history, memory_history))
        client.send_frame(frame, allow_rle=not args.no_rle)
        print(f"\rCPU {cpu:5.1f}%  RAM {memory:5.1f}%", end="", flush=True)
        delay = frame_interval - (time.monotonic() - started)
        if delay > 0:
            time.sleep(delay)


def video_loop(client: SerialDisplayClient, args: argparse.Namespace) -> None:
    try:
        import cv2
    except ImportError as error:
        raise RuntimeError("opencv-python-headless is required; install pc/requirements.txt") from error
    Image, _, _ = require_imaging()
    capture = cv2.VideoCapture(str(args.video))
    if not capture.isOpened():
        raise RuntimeError(f"could not open video: {args.video}")
    if args.start:
        capture.set(cv2.CAP_PROP_POS_MSEC, args.start * 1000.0)
    source_fps = capture.get(cv2.CAP_PROP_FPS)
    fps = args.fps or (source_fps if source_fps > 0 else 15.0)
    fps = min(40.0, max(1.0, fps))
    frame_interval = 1.0 / fps
    sent = 0
    rle_frames = 0
    report_at = time.monotonic() + 1.0
    print(f"Playing monochrome video at {fps:.2f} FPS; press Ctrl+C to stop.")
    try:
        while True:
            started = time.monotonic()
            available, bgr = capture.read()
            if not available:
                if args.loop:
                    capture.set(cv2.CAP_PROP_POS_FRAMES, 0)
                    continue
                break
            grayscale = cv2.cvtColor(bgr, cv2.COLOR_BGR2GRAY)
            image = Image.fromarray(grayscale)
            frame = prepare_image(image, args.fit, args.dither, args.threshold, args.invert)
            encoding = client.send_frame(frame, allow_rle=not args.no_rle)
            sent += 1
            rle_frames += encoding == "rle"
            now = time.monotonic()
            if now >= report_at:
                print(f"\rframes={sent}  rle={rle_frames}", end="", flush=True)
                report_at = now + 1.0
            delay = frame_interval - (time.monotonic() - started)
            if delay > 0:
                time.sleep(delay)
    finally:
        capture.release()
    print(f"\nVideo complete, {sent} frames sent.")


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Show PC telemetry, monochrome video, or images on the SCI-CALC OLED.")
    parser.add_argument("--port", help="serial port, for example COM7")
    parser.add_argument("--list-ports", action="store_true", help="list available serial ports")
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--stats", action="store_true", help="show the CPU/RAM dashboard (default)")
    source.add_argument("--video", type=Path, help="play a video file as monochrome frames")
    source.add_argument("--image", type=Path, help="send one image")
    source.add_argument("--text", help="send up to six lines of text")
    source.add_argument("--test-pattern", action="store_true", help="send a generated test pattern")
    source.add_argument("--clear", action="store_true", help="clear the OLED and exit")
    source.add_argument("--exit-device", action="store_true", help="return the calculator to main.bin")
    parser.add_argument("--fit", choices=("contain", "cover", "stretch"), default="contain")
    parser.add_argument("--fps", type=float, help="target FPS; defaults to 5 for stats or the video FPS")
    parser.add_argument("--threshold", type=int, default=128)
    parser.add_argument("--dither", action="store_true", help="enable Floyd-Steinberg dithering")
    parser.add_argument("--invert", action="store_true")
    parser.add_argument("--contrast", type=int, default=220)
    parser.add_argument("--no-rle", action="store_true")
    parser.add_argument("--loop", action="store_true", help="loop video playback")
    parser.add_argument("--start", type=float, default=0.0, help="video start time in seconds")
    return parser


def main() -> int:
    parser = build_argument_parser()
    args = parser.parse_args()
    if args.list_ports:
        list_ports()
        return 0
    if not args.port:
        parser.error("--port is required unless --list-ports is used")
    if args.fps is not None and not 1 <= args.fps <= 40:
        parser.error("--fps must be between 1 and 40")
    if not 0 <= args.threshold <= 255 or not 0 <= args.contrast <= 255:
        parser.error("--threshold and --contrast must be between 0 and 255")
    if args.start < 0:
        parser.error("--start cannot be negative")

    with SerialDisplayClient(args.port) as client:
        print(
            f"Connected: {client.info['width']}x{client.info['height']} "
            f"protocol={client.info['protocol']} firmware={client.info['firmware']}"
        )
        client.set_contrast(args.contrast)
        if args.exit_device:
            client.exit_device()
            print("Return command sent.")
            return 0
        if args.clear:
            client.clear()
            print("Display cleared.")
            return 0
        if args.test_pattern:
            client.send_frame(prepare_image(test_pattern(), args.fit, args.dither, args.threshold, args.invert), not args.no_rle)
            print("Test pattern sent.")
            return 0
        if args.image:
            Image, _, _ = require_imaging()
            with Image.open(args.image) as image:
                client.send_frame(prepare_image(image, args.fit, args.dither, args.threshold, args.invert), not args.no_rle)
            print("Image sent.")
            return 0
        if args.text is not None:
            frame = prepare_image(text_image(args.text), "stretch", False, 128, args.invert)
            client.send_frame(frame, not args.no_rle)
            print("Text sent.")
            return 0
        try:
            if args.video:
                video_loop(client, args)
            else:
                stats_loop(client, args)
        except KeyboardInterrupt:
            print("\nStopped.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ProtocolError, TimeoutError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        raise SystemExit(1)
