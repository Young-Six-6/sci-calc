# Music Player

[Chinese](README_CH.md)

Music Player plays audio files from the calculator's SD card through a PCM5102A DAC. It supports ordinary compressed audio and multitrack MIDI playback with selectable synthesized tone profiles.

## PCM5102A wiring

| Calculator | PCM5102A |
| --- | --- |
| GPIO21 | BCK / BCLK |
| GPIO22 | LCK / LRCK / WS |
| GPIO15 | DIN |
| GND | GND |
| GND | SCK |

Do not leave `SCK` floating. Connecting it to GND selects the module's three-wire I2S mode and prevents unstable or scrambled playback.

If the module exposes `FMT` and `XSMT`, keep `FMT` low for standard I2S and `XSMT` high for normal output.

The PCM5102A provides line-level L/R output. Connect it to a powered speaker, amplifier, or suitable line input rather than directly to a passive speaker.

## Prepare the SD card

1. Format the card as FAT32. Cards from 8 GB through 32 GB are recommended.
2. Create `/music` in the SD-card root.
3. Copy music into `/music` or folders up to three levels below it.
4. Keep `/music.bin` and `/main.bin` in the SD-card root.

The library accepts up to 96 files and sorts them by full path. When `/music` is absent, the SD-card root is scanned instead.

Supported file extensions:

- MP3
- WAV
- AAC
- M4A
- MID
- MIDI

Larger SDXC cards can be used after formatting them as FAT32.

## MIDI playback

For best compatibility, use Standard MIDI File Format 0 or Format 1 with PPQN timing. A file can contain up to 24 tracks and all 16 MIDI channels.

Playback provides up to 32 simultaneous voices at 44.1 kHz and handles Program Change, channel volume, expression, pan, sustain, pitch bend, tempo changes, and channel-10 percussion.

Use `Shift+1` while a MIDI file is playing to cycle through these tone profiles:

| Profile | Sound |
| --- | --- |
| `GM` | Follows MIDI Program Change events; piano programs use the dynamic piano tone |
| `PIANO` | Uses the dynamic piano tone for melodic channels |
| `MELLOW` | Uses a softer tone with a gentle attack and decay |
| `SYNTH` | Uses a brighter electronic tone |

The selected profile remains active while changing tracks and returns to `GM` after restarting the player.

Opening a MIDI file calculates and displays its total duration. Repeat openings use the automatically maintained `/.sci_midi_cache` file on the SD card.

Very dense passages may display `MIDI polyphony limit` when they require more than 32 simultaneous held voices. Playback continues while preserving the strongest musical parts.

## Controls

### Now playing

| Key | Action |
| --- | --- |
| `5` or `Enter` | Play or pause |
| `8` / `2` | Previous or next track |
| `4` / `6` | Seek backward or forward 10 seconds in MP3, WAV, AAC, or M4A |
| `Shift+4` / `Shift+6` | Seek 60 seconds in ordinary audio, or transpose MIDI down/up one semitone |
| `7` / `9` or `-` / `+` | Decrease or increase volume |
| `1` or `RPN/NORM` | Cycle repeat off, repeat all, and repeat one |
| `Shift+1` | Change the MIDI tone profile |
| `3` or `ANG` | Toggle shuffle |
| `*` | Toggle mute |
| `.`, `0`, or `TAB` | Open the playlist |
| Hold `ESC` for about one second | Restore `/main.bin` and return to the calculator |

### Playlist

| Key | Action |
| --- | --- |
| `8` / `2` | Move one item |
| `7` / `9` | Move one page |
| `5`, `*`, or `Enter` | Play the selected item |
| `.`, `/`, `TAB`, or Backspace | Return to the now-playing page |

## Install

1. Copy [`../../sci_calc_code/bin/music.bin`](../../sci_calc_code/bin/music.bin) to the SD-card root as `/music.bin`.
2. Copy the current [`../../sci_calc_code/bin/main.bin`](../../sci_calc_code/bin/main.bin) to the SD-card root as `/main.bin`.
3. Open `Programs > Music > Music Player`.

`music.bin` is installed through the calculator's application loader. Use the merged full firmware image at address `0x0` when performing a complete USB installation.

## Troubleshooting

- No files shown: confirm the card is FAT32 and the files use one of the listed extensions.
- No sound: verify the common ground, GPIO21/22/15 wiring, `SCK -> GND`, and the amplifier or powered speaker.
- Scrambled sound: check that BCK, LRCK, and DIN are not swapped and that SCK is tied to GND.
- Silent PCM5102A module: set `XSMT` high and `FMT` low when those pins are exposed.
- A track fails to open: shorten unusual filenames, check the file on a computer, and try a freshly formatted FAT32 card.

Serial diagnostics are available at 115200 baud. The `i2s` counter should remain at zero during normal playback.

## Build

Build and publish the application with PlatformIO:

```powershell
cd games\MusicPlayer
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
Copy-Item .pio\build\music_player\firmware.bin ..\..\sci_calc_code\bin\music.bin -Force
```
