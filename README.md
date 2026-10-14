# SCI-CALC

[Chinese](README_CH.md)

<a href="https://github.com/shaoxiongduan/sci-calc/">
  <img height="400" alt="SCI-CALC calculator" src="https://github.com/user-attachments/assets/1d2f0c47-30ca-4eb9-b335-ce44204528ab">
</a>

SCI-CALC is an ESP32 handheld calculator and extensible application platform. It combines a scientific calculator, equation and graph tools, a configurable Bluetooth HID macropad, a standalone-program launcher, a small game console, and USB-connected music and display tools.

This repository is based on the upstream [SCI-CALC project](https://github.com/shaoxiongduan/sci-calc/). The upstream calculator, keyboard layer, SD-card program loading, and original games remain the foundation; the sections below summarize the additions maintained in this repository.

## What was added after upstream

- **Main firmware:** equation solving, a graph calculator launcher, a five-category `Programs` menu, safer submenu navigation, SD application discovery through explicit manifests, and optional automatic display sleep and power-off.
- **Standalone calculator tools:** a classic seven-segment-style calculator for everyday arithmetic, percentages, tax, memory, paper tape, and running totals; and a graph calculator that can plot up to four functions with panning, zooming, grid, labels, and tracing.
- **Standalone games:** 2048, a solvable randomly generated Maze, and a Super-CHIP emulator for `.ch8` programs on the SD card, in addition to the upstream game ports.
- **Wired music applications:** an SD-card music player with MP3 and other compressed-audio support, streamed MIDI playback, duration caching, selectable synthesized tone profiles, and up to 32 voices on the no-PSRAM ESP32 target. It outputs through a PCM5102A over wired I2S.
- **Music keyboard:** a separate live instrument with calculator-key layouts, multiple tone patches, chords, recording, MIDI export, and a MIDI-to-calculator-sheet tool for players without music theory training.
- **USB tools:** serial display output for CPU/RAM dashboards and other monochrome graphics, plus serial keyboard sharing that reports calculator key press, release, and held-state events to a PC without injecting Windows key presses.
- **Wi-Fi storage tool:** a temporary device hotspot and authenticated web interface for managing SD-card files from a phone or computer.
- **Desktop tools:** Electron versions of Falling Keys and the Music Keyboard workstation, including MIDI import, calculator control, lessons, recording, synthesis, and multi-track arranging.
- **Documentation and packaging:** bilingual guides, published standalone images, numbered-notation and MIDI guidance, manifest-based app naming, and build/test instructions for the maintained applications.

The main firmware still provides the upstream Bluetooth HID macropad behavior. The music player is an independent application image and uses the wired PCM5102A output described below.

## Hardware

- MCU: ESP32-WROOM-32E
- Display: 256x64 SSD1322 monochrome OLED
- USB-to-serial: CH340C
- Keyboard: 5x6 matrix using Kailh Choc V1 switches
- Storage: FAT32 microSD card
- Battery: use a suitable LiPo battery below 1000 mAh, following the original hardware design and charging circuit

### Main pin map

| Device | Pins |
| --- | --- |
| OLED | CS GPIO5, DC GPIO16, RST GPIO17, MOSI GPIO23, SCK GPIO18 |
| SD card | CS GPIO4, MOSI GPIO23, MISO GPIO19, SCK GPIO18 |
| Keyboard rows | GPIO33, GPIO32, GPIO35, GPIO34, GPIO39 |
| Keyboard columns | GPIO13, GPIO12, GPIO14, GPIO27, GPIO26, GPIO25 |

The OLED and SD card share the SPI MOSI and clock lines. Each device keeps its own CS pin.

### Optional PCM5102A audio

The independent Music Player uses the following wired I2S connection:

| Calculator | PCM5102A |
| --- | --- |
| GPIO21 | BCK / BCLK |
| GPIO22 | LCK / LRCK / WS |
| GPIO15 | DIN |
| GND | GND |
| GND | SCK |

Tie `SCK` to `GND` for the PCM5102A three-wire mode. Connect the DAC line output to a powered speaker, amplifier, or line input. Do not reuse keyboard-column pins for this audio connection.

## Install or update

The current main firmware is version 1.22. This repository uses a customized `min_spiffs` partition layout for its application slots. Therefore every device must receive a complete USB installation once, including devices migrating from upstream or an older version of this fork. An SD-card update is not a seamless partition-table migration.

The release directory contains images whose filenames keep their real version, for example `sci-calc-full-1.22-min-spiffs.bin`. In the instructions below, `sci-calc-full-1x.xx-min-spiffs.bin` is a placeholder: replace it with the actual versioned filename in [`sci_calc_code/bin`](sci_calc_code/bin/).

### Required complete USB flash

Use the merged full image for every initial installation or migration. It writes the matching bootloader, partition table, OTA layout, and main application together.

1. Connect the calculator by USB and identify its serial port, such as `COM7`.
2. Use the merged image `sci-calc-full-1x.xx-min-spiffs.bin` from [`sci_calc_code/bin`](sci_calc_code/bin/).
3. Install `esptool` if necessary with `python -m pip install esptool`, then run the following in PowerShell and replace `COM7` and the filename as needed:

```powershell
python -m esptool --chip esp32 --port COM7 --baud 460800 write-flash 0x0 "sci_calc_code/bin/sci-calc-full-1x.xx-min-spiffs.bin"
```

The full image must be written at address `0x0`. Do not write `main.bin`, `music.bin`, or `partitions-min-spiffs.bin` at `0x0`; those are not replacement images for the complete flash. Older `esptool` 4.x releases use `write_flash` with an underscore instead. If the selected baud rate is unreliable on the local USB connection, retry with `115200`.

After the USB flash completes, format the SD card as FAT32 if necessary, copy `/main.bin` and any desired standalone application images to its root, then restart the calculator. Keep the device powered until the first boot finishes.

### SD-card updates after the required installation

Once the device has already been installed with this repository's partition layout, the upstream SD-card workflow can be used for routine application updates without an IDE:

1. Copy the desired image from [`sci_calc_code/bin`](sci_calc_code/bin/) to the SD-card root, replacing the old image with the same role.
2. For a main-firmware update, copy `main.bin` to the root as `/main.bin`. Copy only the standalone applications you want to update, such as `/music.bin` or `/graphcalc.bin`.
3. Open `Settings > Update from SD` and keep the device powered until it restarts.

On an older main firmware that does not show `Update from SD`, enter any installed standalone application and hold `ESC` for about one second to restore `/main.bin`. This fallback is for devices that already have the compatible partition layout; it is not a replacement for the required first USB installation. See [Updating Software](docs/updating_software.md) and the individual application guide for return-key details.

The application menu does not list every raw `.bin` file. To add a custom application with a friendly name, place its firmware on the card and create an explicit `/apps/*.app` manifest. See [SD Application Discovery](docs/sd_apps.md).

## Main controls

| Key | Action |
| --- | --- |
| `8` / `2` | Move up or down in menus |
| `Enter` | Select an item |
| `6` or `Enter` | Open a category in `Programs` |
| `4` or `ESC` | Return to the parent menu |
| Hold `Shift` | Use the secondary key layer |
| Double-press `Shift` | Lock or unlock the secondary layer |
| `ANG` | Switch calculator angle mode |
| `TAB` | Switch between calculator input and history |

In the calculator editor, use `Shift+4` and `Shift+6` to move the text cursor. In menus, the direction keys work without holding `Shift`.

## Programs

The built-in calculator contains scientific, equation-solving, RPN, history, stopwatch, settings, and update functions. The `Programs` launcher is organized into five categories:

- `Calculation`: Classic Calc and Graph Calc
- `Games`: 2048, Maze, Super-CHIP, Chess, Spacetrash, Snake, Tetris, Catacombs of the Damned, and Squario
- `Music`: Music Player and Music Keyboard
- `Tools`: SD Manager, USB Display, and USB Key Share
- `Other`: Wifi Rickroll and applications discovered from SD manifests

External applications are application-slot images. They are launched from `Programs` and normally return to the calculator by restoring `/main.bin`; use the key shown in that application's guide. The SD discovery system reads only explicit `.app` manifests and takes the visible name from `name=`, never from a `.bin` filename.

## Application guides

- [2048](games/Game2048/README.md)
- [Classic Calculator](games/ClassicCalculator/README.md)
- [Graph Calculator](games/GraphCalculator/README.md)
- [Music Player](games/MusicPlayer/README.md)
- [Music Keyboard](games/MusicKeyboard/README.md)
- [Falling Keys Desktop](desktop/FallingKeysDesktop/README.md)
- [Music Keyboard Desktop](desktop/MusicKeyboardDesktop/README.md)
- [Maze](games/Maze/README.md)
- [Super-CHIP](games/SuperChip/README.md)
- [Wi-Fi SD Manager](games/SDManager/README.md)
- [USB Serial Display](games/SerialDisplay/README.md)
- [USB Serial Keyboard Sharing](games/SerialKeyboard/README.md)
- [Snake](games/snake-game/README.md)
- [Squario](games/Squario/README.md)

## More documentation

- [Assembly video](https://youtu.be/RY_riS_9hHA?si=YKYpsfk6U_JUeETe)
- [Keyboard layouts](docs/layout.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Updating software](docs/updating_software.md)
- [SD application discovery](docs/sd_apps.md)
- [Development environment](docs/environment_setup.md)
- [Code structure](docs/code_structure.md)
- [Reading numbered musical notation](docs/numbered_notation.md)

## Build

The main firmware uses PlatformIO:

```powershell
cd sci_calc_code
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

Each maintained standalone application has its own build instructions in its directory. Desktop applications are under `desktop/` and use their local Electron and Python bridge setup.
