# USB Serial Display

[Chinese](README_CH.md)

USB Display turns the calculator's 256x64 OLED into a small computer-driven screen for CPU/RAM telemetry, monochrome video, images, test patterns, and short status text.

## Install

1. Copy [`../../sci_calc_code/bin/serialdisplay.bin`](../../sci_calc_code/bin/serialdisplay.bin) to the SD-card root as `/serialdisplay.bin`.
2. Copy the current [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) to the SD-card root as `/main.bin`.
3. Open `Programs > Tools > USB Display` before starting the PC client.

For a complete USB installation, flash [`../../sci_calc_code/bin/sci-calc-full-1.16-min-spiffs.bin`](../../sci_calc_code/bin/sci-calc-full-1.16-min-spiffs.bin) at address `0x0`.

## Set up the computer

Python 3.10 or newer is recommended.

```powershell
cd games\SerialDisplay
python -m pip install -r pc\requirements.txt
python pc\serial_display.py --list-ports
```

Close PlatformIO Monitor and other serial terminals before connecting. Only one program can use the selected COM port at a time.

## Common commands

Replace `COM7` with the port shown by `--list-ports`.

### CPU and RAM dashboard

```powershell
python pc\serial_display.py --port COM7 --stats
```

The dashboard updates five times per second by default. `--fps 20` selects a different rate.

### Test pattern

```powershell
python pc\serial_display.py --port COM7 --test-pattern
```

### Video

```powershell
python pc\serial_display.py --port COM7 --video C:\Videos\status.mp4 --loop
```

Use `--start 30` to begin at 30 seconds and `--fps 15` to choose the output frame rate.

### Image

```powershell
python pc\serial_display.py --port COM7 --image C:\Pictures\status.png --fit contain
```

### Text

```powershell
python pc\serial_display.py --port COM7 --text "BUILD COMPLETE"
```

### Return to the calculator

```powershell
python pc\serial_display.py --port COM7 --exit-device
```

You can also hold the calculator's `ESC` key for about one second. Both methods restore `/main.bin` and restart the calculator.

Press `Ctrl+C` to stop a dashboard or video while leaving the last frame visible.

## Image options

| Option | Purpose |
| --- | --- |
| `--fit contain` | Preserve the complete image with empty margins when needed |
| `--fit cover` | Fill the screen and crop excess edges |
| `--fit stretch` | Fill the screen without preserving aspect ratio |
| `--threshold 0..255` | Set the black-and-white conversion boundary |
| `--dither` | Preserve more apparent grayscale detail |
| `--invert` | Exchange black and white |
| `--contrast 0..255` | Set OLED contrast |
| `--loop` | Repeat video playback |

Hard threshold conversion usually gives the clearest text and UI graphics. Dithering is more useful for photographs and shaded video.

## Frame rate guidance

- CPU/RAM dashboards: 5 to 20 FPS
- Simple animation and sparse graphics: 30 to 40 FPS
- Detailed full-screen video: 20 to 23 FPS

The actual rate depends on video decoding, monochrome conversion, USB serial transfer, and OLED refresh time.

## Python API

`pc/serial_display.py` exposes `SerialDisplayClient` for custom programs. `send_frame()` accepts one packed 256x64 monochrome frame, which is exactly 2048 bytes.

```python
from pc.serial_display import FRAME_BYTES, SerialDisplayClient

frame = bytearray(FRAME_BYTES)
for y in range(64):
    x = y * 4
    frame[y * 32 + x // 8] |= 1 << (x & 7)

with SerialDisplayClient("COM7") as display:
    display.set_contrast(220)
    display.send_frame(bytes(frame))
```

The client also provides `clear()`, `set_contrast()`, and `exit_device()`.

## Troubleshooting

- `serial response timed out`: open USB Display on the calculator first, verify the COM port, and close every other serial program.
- The ESP32 resets when the port opens: wait for the client to reconnect after startup; this is normal on boards whose USB bridge toggles reset lines.
- Video falls behind: reduce `--fps`, use a lower-resolution source, or choose hard threshold conversion instead of dithering.
- The picture is hard to read: adjust `--threshold`, `--contrast`, `--fit`, or `--invert`.
- Return command fails: confirm that `/main.bin` exists on the SD card.

## Hardware

USB data uses the board's UART0 USB-to-serial connection.

| Device | Pins |
| --- | --- |
| OLED | CS GPIO5, DC GPIO16, RST GPIO17, MOSI GPIO23, SCK GPIO18 |
| SD card | CS GPIO4, MOSI GPIO23, MISO GPIO19, SCK GPIO18 |
| ESC keyboard row/column | GPIO33 / GPIO13 |

## Build and test

```powershell
cd games\SerialDisplay
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
python -m unittest discover -s tests -v
Copy-Item .pio\build\serial_display\firmware.bin ..\..\sci_calc_code\bin\serialdisplay.bin -Force
```

The resulting `serialdisplay.bin` is installed through the calculator's application loader. Use the merged full image for flashing at address `0x0`.
