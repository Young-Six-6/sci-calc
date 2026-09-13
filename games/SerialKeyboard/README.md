# USB Serial Keyboard Sharing

[Chinese](README_CH.md)

USB Key Share sends the calculator's physical key transitions to a Windows computer over its CH340C USB serial connection. The included Python client prints or records those events. It does not turn the calculator into a USB keyboard and does not inject keystrokes into Windows.

## Install

1. Copy [`../../sci_calc_code/bin/serialkeyboard.bin`](../../sci_calc_code/bin/serialkeyboard.bin) to the SD-card root as `/serialkeyboard.bin`.
2. Keep the current [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) in the SD-card root as `/main.bin`.
3. Open `Programs > Tools > USB Key Share`.

Hold the calculator's `ESC` key for about one second to restore `/main.bin` and return to the main calculator.

## Set up Windows

Python 3.10 or newer is recommended. Install the only PC dependency, PySerial:

```powershell
cd games\SerialKeyboard
python -m pip install -r pc\requirements.txt
python pc\serial_keyboard.py --list-ports
```

Close PlatformIO Monitor and every other serial terminal first. Only one program can open a COM port at a time.

## Record keys

Replace `COM7` with the port reported by `--list-ports`:

```powershell
python pc\serial_keyboard.py --port COM7
```

For a live 5x6 panel that always shows every key as `UP` or `DOWN`, use:

```powershell
python pc\serial_keyboard.py --port COM7 --monitor
```

Pressed keys are highlighted immediately. Each key-event packet carries the complete 30-key state, and the periodic state packet corrects the panel if a serial packet was lost. `--monitor` can be combined with `--log`.

The default output is a readable event stream:

```text
    12.340s  #00018  PRESS   key=SHIFT        id=24 pos=R5C1 mods=SHIFT held=0ms
    12.412s  #00019  PRESS   key=3            id=20 pos=R4C3 mods=SHIFT held=0ms
    12.598s  #00020  RELEASE key=3            id=20 pos=R4C3 mods=SHIFT held=186ms
```

Add `--log` to append machine-readable UTF-8 JSON Lines while keeping the console output:

```powershell
python pc\serial_keyboard.py --port COM7 --log logs\keys.jsonl
```

Useful options:

| Option | Purpose |
| --- | --- |
| `--list-ports` | List available serial ports |
| `--monitor` | Show every key's current `UP` or `DOWN` state in a live 5x6 panel |
| `--log PATH` | Append device information and key events to JSONL |
| `--show-state` | Print the one-second state and loss counters |
| `--connect-timeout SECONDS` | Change the initial packet timeout; default is 6 seconds |
| `--duration SECONDS` | Stop automatically after a test interval |

Press `Ctrl+C` to stop. The final line reports received key events, serial sequence gaps, and any device queue drops.

## Event behavior

- Every debounced physical press and release is sent.
- A release includes the time that the key was held.
- `mods=SHIFT` is present while the physical Shift key is held.
- No automatic key-repeat events are generated in firmware revision 1. A repeat action number is reserved for future use.
- Events use stable row-major IDs from 0 through 29. Rows and columns in JSON and protocol packets are zero-based; console positions are shown as `R1C1` through `R5C6`.
- Opening a serial port may reset some ESP32 boards. The client recognizes the new boot ID and starts a fresh sequence automatically.

### Physical key matrix

| Row | C1 | C2 | C3 | C4 | C5 | C6 |
| --- | --- | --- | --- | --- | --- | --- |
| R1 | ESC | / | * | - | SIN | SEC |
| R2 | 7 | 8 | 9 | + | COS | CSC |
| R3 | 4 | 5 | 6 | ^ | TAN | COT |
| R4 | 1 | 2 | 3 | ENTER | EXP | RPN/NORM |
| R5 | SHIFT | 0 | . | BACKSPACE | ANGLE | TAB |

Names describe the printed calculator keys. The PC receives the physical key identity; it does not receive a calculated character from the main calculator's Shift layer.

## Extending the receiver

`SerialKeyboardReceiver.records()` exposes typed records without any Windows input library. A later MIDI or automation module can consume only `KeyEventRecord` objects and decide what each physical key means:

```python
from pc.serial_keyboard import ACTION_PRESS, KeyEventRecord, SerialKeyboardReceiver

with SerialKeyboardReceiver("COM7") as keyboard:
    for record in keyboard.records():
        if isinstance(record, KeyEventRecord) and record.action_id == ACTION_PRESS:
            handle_press(record.key_id, record.modifiers)
```

Keep action mapping outside `serial_keyboard.py`. This preserves the logger as a read-only source of physical events and lets MIDI, shortcuts, or application controls be added independently.

## Wire protocol

Serial settings are `115200 8N1`. Every binary frame is:

| Bytes | Field |
| --- | --- |
| 4 | ASCII magic `SKB1` |
| 1 | Protocol version, currently `1` |
| 1 | Packet type |
| 2 | Sequence number, little-endian |
| 4 | Device time in milliseconds, little-endian |
| 2 | Payload length, little-endian |
| N | Payload, at most 64 bytes |
| 2 | CRC-16/CCITT-FALSE of the header and payload, little-endian |

Packet types:

| Type | Payload |
| --- | --- |
| `0x01` device info | Firmware revision, matrix size, key count, baud, capabilities, and boot ID |
| `0x02` key event | Key ID, action, row, column, modifier bits, held milliseconds, and complete key-state mask |
| `0x03` state | Complete key-state mask, generated event count, and device queue-drop count |

The ESP32 serial path is transmit-only: the firmware contains no command receiver. Device-info and state frames are stream metadata; physical input is carried only by key-event frames. The decoder checks lengths and CRC, discards startup text or damaged frames, and resynchronizes at the next `SKB1` marker.

## Troubleshooting

- `No serial ports found`: reconnect USB, install the CH340 driver if Windows does not expose a COM port, and run `--list-ports` again.
- `no valid SKB1 packets received`: open USB Key Share on the calculator, choose the correct COM port, and close other serial tools.
- Events appear twice: each normal tap intentionally produces one `PRESS` and one `RELEASE` record.
- A key remains pressed in a custom action handler after a disconnect: clear that handler's active state whenever the serial connection closes or the boot ID changes.
- `SD UNAVAILABLE` on the OLED: logging still works, but `/main.bin` cannot be restored until the SD card is available.

## Hardware

| Device | Pins |
| --- | --- |
| USB serial | UART0 through the onboard CH340C |
| OLED | CS GPIO5, DC GPIO16, RST GPIO17, MOSI GPIO23, SCK GPIO18 |
| SD card | CS GPIO4, MOSI GPIO23, MISO GPIO19, SCK GPIO18 |
| Keyboard rows | GPIO33, GPIO32, GPIO35, GPIO34, GPIO39 |
| Keyboard columns | GPIO13, GPIO12, GPIO14, GPIO27, GPIO26, GPIO25 |

## Build and test

```powershell
cd games\SerialKeyboard
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
python -m unittest discover -s tests -v
Copy-Item .pio\build\serial_keyboard\firmware.bin ..\..\sci_calc_code\bin\serialkeyboard.bin -Force
```

The resulting `serialkeyboard.bin` is an application-slot image loaded from the SD card. It is not a complete image for flashing at address `0x0`.
