# Wi-Fi SD Manager

[Chinese](README_CH.md)

SD Manager creates a local Wi-Fi hotspot and browser interface for managing files on the calculator's SD card.

## Install

1. Copy [`../../sci_calc_code/bin/sdmanager.bin`](../../sci_calc_code/bin/sdmanager.bin) to the SD-card root as `/sdmanager.bin`.
2. Copy the current [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) to the SD-card root as `/main.bin`.
3. Open `Programs > Tools > SD Manager`.

For a complete USB installation, flash [`../../sci_calc_code/bin/sci-calc-full-1.16-min-spiffs.bin`](../../sci_calc_code/bin/sci-calc-full-1.16-min-spiffs.bin) at address `0x0`.

## Connect

After startup, the OLED shows:

- Wi-Fi network name (`SCI-CALC-XXXX`)
- Eight-character Wi-Fi password
- Service address (`http://192.168.4.1`)
- Six-digit `WEB CODE`
- Connected-client count and the latest file operation

To open the manager:

1. Connect a phone or computer to the displayed Wi-Fi network.
2. Enter the displayed Wi-Fi password.
3. Open `http://192.168.4.1` in a browser.
4. Enter the six-digit `WEB CODE` shown on the calculator.

Both displayed credentials change whenever SD Manager starts. Up to four devices can connect to the hotspot at the same time.

Some phones open a small sign-in window automatically. Opening `http://192.168.4.1` manually in the normal browser provides the complete interface.

## Manage files

The browser supports:

- Folder navigation
- Multi-file and drag-and-drop upload
- Download
- New folders
- Rename
- File and folder deletion

Wait for the success message before disconnecting power after an upload or replacement.

`/main.bin` can be replaced through upload and is protected from rename and deletion. Keep a valid `/main.bin` on the card so every standalone program can return to the calculator.

Use the `Lock` button before handing the device to someone else. It closes current browser sessions and displays a new `WEB CODE`. Five failed code attempts pause login for 30 seconds.

## Return to the calculator

Use `Return to calculator` on the web page, or hold the physical `ESC` key for about one second. SD Manager restores `/main.bin` and restarts the ESP32.

An upload must finish before the return command is accepted.

## Hardware and storage

| Device | Pins |
| --- | --- |
| OLED | CS GPIO5, DC GPIO16, RST GPIO17, MOSI GPIO23, SCK GPIO18 |
| SD card | CS GPIO4, MOSI GPIO23, MISO GPIO19, SCK GPIO18 |
| Keyboard rows | GPIO33, GPIO32, GPIO35, GPIO34, GPIO39 |
| Keyboard columns | GPIO13, GPIO12, GPIO14, GPIO27, GPIO26, GPIO25 |

Use a FAT32-formatted SD card. FAT32 limits a single file to less than 4 GB.

## Troubleshooting

- Wi-Fi reports no Internet: choose to stay connected, then open `http://192.168.4.1` manually.
- The browser leaves the manager page: temporarily disable VPN software and automatic mobile-data switching.
- Login expired: reload the page and enter the current `WEB CODE` again.
- Upload fails: verify free SD-card space, shorten unusual paths, and try a freshly formatted FAT32 card.
- Return fails: confirm that `/main.bin` exists and wait for any active upload to finish.

Keep the calculator powered while replacing `/main.bin` or returning to the main firmware.

## Build

```powershell
cd games\SDManager
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
Copy-Item .pio\build\sd_manager\firmware.bin ..\..\sci_calc_code\bin\sdmanager.bin -Force
```

The resulting `sdmanager.bin` is installed through the calculator's application loader. Use the merged full image for flashing at address `0x0`.
