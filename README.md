# SCI-CALC

[Chinese](README_CH.md)

<a href="https://github.com/shaoxiongduan/sci-calc/">
  <img height="400" alt="SCI-CALC calculator" src="https://github.com/user-attachments/assets/1d2f0c47-30ca-4eb9-b335-ce44204528ab">
</a>

SCI-CALC is an ESP32-based handheld that combines a scientific calculator, equation solver, RPN calculator, configurable Bluetooth HID macropad, program launcher, stopwatch, and small game console.

## Hardware

- MCU: ESP32-WROOM-32E
- Display: 256x64 SSD1322 monochrome OLED
- USB-to-serial: CH340C
- Keyboard: 5x6 matrix using Kailh Choc V1 switches
- Storage: FAT32 microSD card

### Main pin map

| Device | Pins |
| --- | --- |
| OLED | CS GPIO5, DC GPIO16, RST GPIO17, MOSI GPIO23, SCK GPIO18 |
| SD card | CS GPIO4, MOSI GPIO23, MISO GPIO19, SCK GPIO18 |
| Keyboard rows | GPIO33, GPIO32, GPIO35, GPIO34, GPIO39 |
| Keyboard columns | GPIO13, GPIO12, GPIO14, GPIO27, GPIO26, GPIO25 |

The OLED and SD card share the SPI MOSI and clock lines. Each device keeps its own CS pin.

## Install or update

The current main firmware is version 1.18.

For a complete USB installation, flash [`sci_calc_code/bin/sci-calc-full-1x.xx-min-spiffs.bin`](sci_calc_code/bin/sci-calc-full-1x.xx-min-spiffs.bin) at address `0x0`.

For an SD update:

1. Copy [`sci_calc_code/bin/main.bin`](sci_calc_code/bin/main.bin) to the SD-card root as `/main.bin`.
2. Replace any standalone application images that you want to update.
3. Open `Settings > Update from SD` on the calculator.
4. Keep the calculator powered until it restarts.

Standalone applications are application-slot images. Launch them from `Programs`; use each application's documented return key to restore `/main.bin`.

See [Updating Software](docs/updating_software.md) for the complete update workflow.

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

`Programs` is organized into five categories:

- `Calculation`: Graph Calc
- `Games`: Maze, Falling Keys, Super-CHIP, Chess, Spacetrash, Snake, Tetris, Catacombs of the Damned, and Squario
- `Music`: Music Player and Music Keyboard
- `Tools`: SD Manager and USB Display
- `Other`: Wifi Rickroll and applications discovered from SD manifests

Additional applications can be added through explicit `/apps/*.app` manifests. Their visible names come from the manifest rather than the firmware filename. See [SD Application Discovery](docs/sd_apps.md).

## Application guides

- [Graph Calculator](games/GraphCalculator/README.md)
- [Music Player](games/MusicPlayer/README.md)
- [Music Keyboard](games/MusicKeyboard/README.md)
- [Falling Keys](games/FallingKeys/README.md)
- [Maze](games/Maze/README.md)
- [Super-CHIP](games/SuperChip/README.md)
- [Wi-Fi SD Manager](games/SDManager/README.md)
- [USB Serial Display](games/SerialDisplay/README.md)
- [Snake](games/snake-game/README.md)
- [Squario](games/Squario/README.md)

## More documentation

- [Assembly video](https://youtu.be/RY_riS_9hHA?si=YKYpsfk6U_JUeETe)
- [Keyboard layouts](docs/layout.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Development environment](docs/environment_setup.md)
- [Code structure](docs/code_structure.md)

## Build

The main firmware uses PlatformIO:

```powershell
cd sci_calc_code
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

Each maintained standalone application has its own build instructions in its directory.
