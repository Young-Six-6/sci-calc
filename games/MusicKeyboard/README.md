# Music Keyboard

[Chinese](README_CH.md)

Music Keyboard turns the calculator into a compact 16-voice instrument. It plays through the wired PCM5102A, shows a live keyboard or oscilloscope, provides six synth tones, and records performances as standard MIDI files on the SD card.

The application is a standalone firmware image. It does not consume space in the normal calculator application partition while the main firmware is running.

## Hardware

| Device | Connection |
| --- | --- |
| PCM5102A BCK | GPIO21 |
| PCM5102A LCK/LRCK/WS | GPIO22 |
| PCM5102A DIN | GPIO15 |
| PCM5102A SCK | GND |
| PCM5102A power | VCC and common GND |
| OLED | CS GPIO5, DC GPIO16, RST GPIO17, MOSI GPIO23, SCK GPIO18 |
| SD card | CS GPIO4, MOSI GPIO23, MISO GPIO19, SCK GPIO18 |

The keyboard uses the calculator's normal GPIO matrix. No additional input wiring is needed.

## Install and open

Copy `musickeyboard.bin` to the SD-card root as `/musickeyboard.bin`, and keep the current main firmware at `/main.bin`. Open `Programs > Music > Music Keyboard`.

Hold `ESC` for one second to restore `/main.bin` and return to the calculator. A short `ESC` press immediately silences all notes.

## Views

Press `TAB` to cycle through four views:

- `KEYS`: current notes and a two-octave piano visualization
- `SCOPE`: live waveform, output peak, active voice count, and synth CPU load
- `REC`: recording or playback time, event capacity, and saved filename
- `SET`: tone, key, transpose, layout, velocity, volume, tempo, sustain, and metronome state

Changing views never interrupts held notes.

## Playing

The default `CALC` layout follows the familiar note order used by classic musical calculators. The selected root, octave, and transpose are applied before the note is sent to the synth or recorder, so the same fingering works in any key.

| Key | Calculator-layout note |
| --- | --- |
| `.` | Low La, below the root |
| `0` | Low Si, below the root |
| `1` through `7` | Root, 2nd, 3rd, 4th, 5th, 6th, 7th |
| `8` | Root, one octave up |
| `9` | 2nd, one octave up |
| `/`, `*`, `-`, `+`, `^` | 3rd through 7th, one octave up |
| Hold `ENTER` with any note key | Play that key one octave higher; for example, `ENTER+3` is the upper 3rd |

The science keys fill the accidentals and upper notes: `SIN`, `SEC`, `COS`, `CSC`, `TAN`, `COT`, `EXP`, and the extra key play offsets `+1`, `+3`, `+6`, `+8`, `+10`, `+13`, `+15`, and `+18` semitones.

The operator extension follows the printed-key order: `/`, `*`, `-`, `+`, and `^` are upper `3`, `4`, `5`, `6`, and `7`. `ENTER` is a momentary upper-octave key in the Calculator layout, so it does not change the global octave and the normal fingering returns as soon as it is released. The operator keys keep their chord-pad behavior in the Chord and Chromatic layouts.

`Shift+TAB` cycles to the `CHORD` layout. It keeps the calculator note order and turns the operator keys into chord pads, so each triad needs only one physical key:

| Key | Chord |
| --- | --- |
| `*` | I major |
| `/` | ii minor |
| `-` | IV major |
| `+` | V major |
| `^` | vi minor |

The third `CHROMATIC` layout maps `1 2 3 4 5 6 7 8 9 0 . ENTER` to twelve consecutive semitones. Its operator keys remain chord pads, and the science keys continue upward from the next octave.

## Performance controls

| Shortcut | Action |
| --- | --- |
| `Shift+1` / `Shift+3` | Previous or next tone |
| `Shift+4` / `Shift+6` | Octave down or up |
| `Shift+2` / `Shift+8` | Transpose down or up one semitone |
| `Shift+/` / `Shift+*` | Previous or next root note |
| `Shift+TAB` | Cycle Calculator, Chord, and Chromatic layouts |
| `Shift+-` / `Shift++` | Volume down or up |
| `Shift+7` / `Shift+9` | Tempo down or up by 5 BPM |
| `Shift+^` / `Shift+EXP` | Velocity down or up |
| `ANGLE` or `Shift+5` | Toggle sustain |
| `Shift+.` | Toggle metronome |
| `BACKSPACE` | Silence all notes |
| `Shift+ANGLE` | Open the settings view |

Available tones are Piano, Electric Piano, Organ, Warm Pad, Lead, and Bell. Tone changes made during a recording are stored as MIDI program-change events.

## Recording

| Shortcut | Action |
| --- | --- |
| `Shift+0` | Start or stop recording |
| `Shift+ENTER` | Play or stop the in-memory take |
| `Shift+BACKSPACE` | Clear the in-memory take |

Stopping a recording saves the next available `/recordings/KEYS000.MID` through `/recordings/KEYS999.MID`. The separate directory keeps new takes out of the normal music library. The file is MIDI format 0 with note velocity, sustain, tone changes, and tempo changes; move a finished take into `/music` only when you want it to appear in the standalone Music Player.

The fixed recording buffer stores 2048 events, using about 16 KB of internal RAM. If it fills, recording stops and the take is saved automatically. If the SD card is missing or a write fails, the take remains available for playback in RAM until the application exits or a new recording starts.

## Hardware limits

- The mechanical keys do not sense pressure or strike speed. `Shift+^` and `Shift+EXP` set a fixed MIDI velocity instead.
- The synth supports up to 16 simultaneous voices. This covers normal calculator-key playing and several overlapping triads while leaving reliable headroom on an ESP32-WROOM-32E without PSRAM.
- Recording stores note events rather than PCM audio. This preserves long performances with little RAM and produces editable MIDI, but playback on another synthesizer may use a different timbre.
- Audio is generated at 44.1 kHz with fixed-size buffers. The scope's CPU percentage measures synthesis work only, not time waiting for I2S output.

## Build

```powershell
cd games/MusicKeyboard
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

The application image is generated at `.pio/build/music_keyboard/firmware.bin`.
