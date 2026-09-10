# Super-CHIP Emulator

[Chinese](README_CH.md)

Super-CHIP is a standalone CHIP-8 and SUPER-CHIP emulator for the SCI-CALC ESP32-WROOM-32E. ROMs are selected from the SD card and played with the calculator's physical keypad.

## Prepare the SD card

Create this layout on a FAT32 SD card:

```text
/
|- main.bin
|- chip8.bin
`- chip8/
   |- game-one.ch8
   |- game-two.ch8
   `- demo.ch8
```

The emulator creates `/chip8` on first start when needed. Place `.ch8` files directly in that directory; subfolders are not scanned.

The ROM browser accepts up to 64 files. ROM sizes from 2 through 3584 bytes are supported.

## Install

1. Copy [`../../sci_calc_code/bin/chip8.bin`](../../sci_calc_code/bin/chip8.bin) to the SD-card root as `/chip8.bin`.
2. Copy the current [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) to the SD-card root as `/main.bin`.
3. Add the desired `.ch8` files to `/chip8`.
4. Open `Programs > Games > Super-CHIP`.

For a complete USB installation, flash [`../../sci_calc_code/bin/sci-calc-full-1.18-min-spiffs.bin`](../../sci_calc_code/bin/sci-calc-full-1.18-min-spiffs.bin) at address `0x0`.

## ROM browser

| Key | Action |
| --- | --- |
| `8` / `2` | Select the previous or next ROM |
| `Enter` or `5` | Start the selected ROM |
| `4` / `6` | Select the previous or next compatibility profile |
| `-` / `+` | Decrease or increase emulation speed |
| `.` | Toggle sound |
| Backspace | Rescan `/chip8` |
| Hold `ESC` for one second | Restore `/main.bin` and return to the calculator |

## CHIP-8 keypad

The printed number keys keep their hexadecimal values. The six operator keys provide A through F.

| Calculator key | CHIP-8 key |
| --- | --- |
| `0` through `9` | `0` through `9` |
| `/` | `A` |
| `*` | `B` |
| `-` | `C` |
| `+` | `D` |
| `^` | `E` |
| Backspace | `F` |

This mapping lets ROM instructions such as "press 5" or "use A and F" match the printed calculator keys directly.

## In-game controls

A short press of `Enter` or `ESC` opens the pause menu without sending a virtual CHIP-8 key.

Hold `Shift` before pressing a shortcut:

| Shortcut | Action |
| --- | --- |
| `Shift+4` / `Shift+6` | Decrease or increase speed |
| `Shift+7` / `Shift+9` | Select the previous or next profile and restart the ROM |
| `Shift+0` | Toggle the side status display |
| `Shift+.` | Toggle sound |
| `Shift+Enter` | Restart the current ROM |
| `Shift+Backspace` | Return to the ROM browser |
| `Shift+5` | Open the pause menu |

In the pause menu, use `8` and `2` to select an item, `4` and `6` to change a setting, and `Enter` to apply it.

## Compatibility profiles

| Profile | Recommended use |
| --- | --- |
| `MODERN` | Recent CHIP-8 software and Octo-style ROMs |
| `SUPER` | SUPER-CHIP software that uses SCHIP instructions or display modes |
| `CLASSIC` | Older CHIP-8 software that expects original timing and instruction behavior |

Changing profile restarts the loaded ROM. Speed can be adjusted while the ROM is running. If a game is too fast, too slow, or misses input, adjust speed first and then try another profile.

## Display and sound

SUPER-CHIP's 128x64 output is centered on the 256x64 OLED. Standard CHIP-8's 64x32 output is enlarged to the same central 128x64 area. The optional side display shows profile, speed, sound state, ROM name, and the return hint.

CHIP-8 sound is sent to a PCM5102A DAC using this wiring:

| Calculator | PCM5102A |
| --- | --- |
| GPIO21 | BCK / BCLK |
| GPIO22 | LCK / LRCK / WS |
| GPIO15 | DIN |
| GND | GND |
| GND | SCK |

Connect the PCM5102A L/R line outputs to a powered speaker, amplifier, or line input.

## Troubleshooting

- No ROMs listed: place `.ch8` files directly in `/chip8`, then press Backspace to rescan.
- A ROM starts with a blank or corrupted screen: choose another compatibility profile.
- Movement is too fast or input is missed: lower the speed with `-`, `Shift+4`, or the pause menu.
- No sound: verify GPIO21/22/15, common ground, `SCK -> GND`, and the external amplifier.
- Return fails: confirm that `/main.bin` exists in the SD-card root.

## Build and test

```powershell
cd games\SuperChip
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
python -m unittest discover -s tests -v
Copy-Item .pio\build\superchip\firmware.bin ..\..\sci_calc_code\bin\chip8.bin -Force
```

The resulting `chip8.bin` is installed through the calculator's application loader. Use the merged full image for flashing at address `0x0`.

## License

The CPU and graphics interpreter uses the microcontroller-oriented [OctEmu](https://github.com/Cycloctane/octemu) core at commit `e33e3250f7668c721a63a5f03d83e95cd982fa25`. Its MIT license is stored in [`LICENSE.OctEmu`](LICENSE.OctEmu), and the vendored source is under `src/vendor/octemu`.

The [CHIP-8 extensions reference](https://chip-8.github.io/extensions/) and [Timendus CHIP-8 test suite](https://github.com/Timendus/chip8-test-suite) are useful when checking ROM compatibility.
