# Falling Keys

[中文说明](README_CH.md)

Falling Keys is a standalone falling-tile rhythm game for the SCI-CALC. It starts with the familiar four-column Piano Tiles layout, then adds optional six-column charts, three timing levels, hold notes, two sounds, combo scoring, accuracy, ranks, and saved high scores.

Three public-domain charts are built in, so the game is immediately playable without chart files:

- `Classic Rush`: a long generated arcade chart whose density changes with difficulty
- `Ode to Joy`
- `Twinkle Star`

## Hardware

The firmware uses the calculator's established wiring:

| Device | Pins |
| --- | --- |
| OLED | CS `GPIO5`, MOSI `GPIO23`, DC `GPIO16`, CLK `GPIO18`, reset `GPIO17` |
| SD card | CS `GPIO4`, MOSI `GPIO23`, MISO `GPIO19`, CLK `GPIO18` |
| Keyboard rows | `GPIO33`, `GPIO32`, `GPIO35`, `GPIO34`, `GPIO39` |
| Keyboard columns | `GPIO13`, `GPIO12`, `GPIO14`, `GPIO27`, `GPIO26`, `GPIO25` |
| PCM5102A | BCK `GPIO21`, LCK/LRCK `GPIO22`, DIN `GPIO15`, SCK to GND |

The OLED and SD card share the SPI bus and retain separate chip-select pins. Audio is rendered at 44.1 kHz by a fixed-memory eight-voice synthesizer running on core 0; game timing and display work run on core 1.

## Install

1. Copy [`firmware.bin`](firmware.bin) to the SD-card root as `/fallingkeys.bin`.
2. Keep the current calculator firmware on the card as `/main.bin`.
3. Start `Programs > Games > Falling Keys`.

For a complete USB installation of the main calculator, flash [`../../sci_calc_code/bin/sci-calc-full-1x.xx-min-spiffs.bin`](../../sci_calc_code/bin/sci-calc-full-1x.xx-min-spiffs.bin) at address `0x0`.

Hold `ESC` for one second anywhere in the game to restore `/main.bin`. A short `ESC` press pauses a running chart.

## Controls

### Menus

| Key | Action |
| --- | --- |
| `8` / `2` | Move selection up / down |
| `4` / `6` | Change the selected option |
| `Enter` | Start or accept |
| `Backspace` | Leave pause/results and return to the game menu |
| Hold `ESC` | Return to the calculator |

### Four lanes

Each physical keyboard column is one lane. You can therefore play on whichever calculator row feels comfortable. The most natural four-key row is:

`1` `2` `3` `Enter`

These rows work identically:

`4` `5` `6` `^`

`7` `8` `9` `+`

The entire mapping is:

| Lane | Usable keys in that physical column |
| --- | --- |
| 1 | `7`, `4`, `1` |
| 2 | `/`, `8`, `5`, `2`, `0` |
| 3 | `*`, `9`, `6`, `3`, `.` |
| 4 | `-`, `+`, `^`, `Enter`, `Backspace` |

`ESC` and `Shift` remain reserved and do not strike lane 1. For a hold tile, keep any one key in that lane held until its tail reaches the hit line.

### Six lanes

Six-lane mode adds the two scientific-key columns. A convenient complete row is:

`1` `2` `3` `Enter` `Exp` `Extra`

Lane 5 accepts `Sin`, `Cos`, `Tan`, `Exp`, or `Angle`. Lane 6 accepts `Sec`, `Csc`, `Cot`, `Extra`, or `Tab`.

## Game options

- `TRACK` selects a built-in or SD chart.
- `LANES` selects four or six lanes for built-in tracks. An SD chart carries its own fixed lane count.
- `LEVEL` changes both timing tolerance and tile travel time. `Easy` is forgiving, `Normal` is the default, and `Hard` is faster and tighter.
- `TONE` selects the piano-like or arcade voice.
- `VOLUME` adjusts the PCM5102A output from 0 to 10.

A hit is judged `PERFECT` or `GOOD`. Missed heads, released hold tails, and presses with no nearby tile reset the combo. The result screen shows rank, accuracy, maximum combo, judgement counts, and the best score stored for that track/lane/difficulty combination.

## SD chart format

Place custom charts in `/fallingkeys` on the SD card. Up to 16 custom `.ptc` files are listed alongside the three built-in tracks. The menu uses each chart's `title`, not its filename.

`PTC1` is a small text format designed for deterministic playback on the ESP32:

```text
PTC1
title=Example Song
artist=Example Artist
lanes=4
bpm=120
offset=0
# N,time_ms,lane,note,duration_ms
N,1000,0,60,0
N,1500,2,64,0
N,2000,1,67,700
```

Rules:

- `lanes` may be 4, 5, or 6.
- `time_ms` is the tile's hit time from the start of the chart.
- `lane` is zero based, from `0` to `lanes - 1`.
- `note` is a MIDI pitch from 0 to 127 and controls the sound heard on a successful hit.
- `duration_ms` is `0` for a tap. Values of 300 ms or more become hold tiles.
- `offset` shifts every note and may range from -10000 to 10000 ms.
- A chart may contain at most 1024 note events. Lines may be in any order; the loader sorts them by time.

An editable example is available at [`examples/example.ptc`](examples/example.ptc).

## Converting MIDI

The included converter resolves tempo changes, running status, simultaneous notes, note lengths, lane assignment, and density on the computer. The calculator then loads the prepared PTC1 events directly, keeping gameplay timing predictable.

The converter uses only the Python standard library:

```powershell
cd games\FallingKeys
python tools\midi_to_tiles.py song.mid song.ptc --lanes 4 --difficulty normal
```

To write directly to an SD card mounted as `F:`:

```powershell
python tools\midi_to_tiles.py song.mid F:\fallingkeys\song.ptc --lanes 6 --difficulty hard --title "Song title" --artist "Artist"
```

Conversion modes are intentionally game-oriented:

- `easy` keeps one prominent note per onset and spaces dense passages.
- `normal` keeps up to two-note chords.
- `hard` keeps up to four-note chords and closer onsets.
- Long notes become holds; short notes remain taps.
- MIDI channel 10 percussion is skipped by default; add `--include-drums` to turn drum notes into playable tiles.
- Charts above 1024 events are thinned across their full duration rather than simply cut off at the end.

Standard MIDI format 0 and 1 files with PPQ timing are supported.

## Build

```powershell
cd games\FallingKeys
platformio run
```

The application image is generated at `.pio/build/falling_keys/firmware.bin`.
