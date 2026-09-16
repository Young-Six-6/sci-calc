# Falling Keys Desktop

Falling Keys Desktop is a modern desktop rhythm game for the SCI-CALC calculator. The computer renders the playfield, synthesizes MIDI playback, and judges timing; the calculator is used as a USB serial controller.

## Requirements

- Windows 10 or newer
- Node.js and Electron 37 or a compatible Electron runtime
- Python 3.10 or newer
- `pyserial` installed for the SerialKeyboard client
- SCI-CALC firmware with the SerialKeyboard program available

Install the Python dependency from the repository root:

```powershell
python -m pip install -r games\SerialKeyboard\pc\requirements.txt
```

## Start

From this directory:

```powershell
npm start
```

The app starts with COM7 as the suggested port. Use Scan when the port is unknown, then connect the calculator before starting a run. The app never injects events into Windows applications.

Use Watch demo to run an autonomous perfect performance without a calculator or serial port. This is useful for checking a MIDI arrangement, showing the stage, or testing the audio output.

The global `electron` command can be used when dependencies have not been installed locally. For a local install, run `npm install` once in this directory.

## Charts

The desktop player accepts standard `.mid` and `.midi` files directly. It automatically chooses one lead note at each onset for the player lane and plays the remaining notes as quiet computer accompaniment. Track names, channel names, and pitch-to-key knowledge are not required. The legacy PTC1 chart format is also supported:

```text
PTC1
title=Example
artist=SCI-CALC
lanes=4
bpm=120
offset=0
N,time_ms,lane,note,duration_ms
N,1000,0,60,0
N,1500,1,62,0
N,2200,2,64,500
```

`lane` is zero-based and must be between 0 and 5. Times and durations are milliseconds from the chart start. A duration greater than zero creates a hold note. Imported charts keep their declared lane count; the built-in demo can be played with four, five, or six lanes.

The setup screen has four modes: Flow is forgiving and spacious, Arcade is the standard run, Rush compresses the timing and approach distance, and Mirror reverses the visible lane pattern every phrase. All modes still require only the automatically selected lead melody; accompaniment never becomes a second input lane.

English is the default interface language. Use the language selector in the top-right corner to switch to Chinese; the choice is saved locally.

## Calculator input

The game preserves the physical matrix-column mapping used by the firmware game: column 1 through column 6 become lane 1 through lane 6. Any key in a column can hit that lane, so the player can choose a comfortable row. ESC pauses, SHIFT is reserved by the calculator and is ignored by the game, and repeated serial events are ignored for judging.

## Input timing calibration

Before starting a run, connect the calculator and use the **Timing calibration** panel on the setup screen. Choose a tempo and sample length, then press any lane key once on every click after the four-beat count-in. Release it before the next beat. The result uses the median of the valid samples and trims extreme samples on longer runs; it is saved locally and applied to physical input judgment in every game mode.

A positive offset means the serial event arrived late, so the game evaluates that input earlier by the measured amount. The correction does not move the chart, accompaniment, or Watch demo timing, and it cannot make live sound start before the physical key event arrives. Use Reset to clear the saved correction.

## Timing and audio

The game uses the Web Audio clock for the stage and maps incoming serial events to the same run clock. The sound timeline is independent from input judgment: lead and accompaniment notes play automatically at their chart times, while calculator keys affect only visual feedback, combo, accuracy, and score. Early presses, misses, and hold failures therefore do not interrupt or retrigger the music. Sound is synthesized locally, and best scores are stored in the local Electron profile.

## Troubleshooting

- If the port stays offline, close other programs using the COM port and confirm the calculator is running SerialKeyboard firmware.
- If the Python bridge cannot start, set `SCI_CALC_PYTHON` to the full path of the Python executable.
- If a chart is rejected, check the `PTC1` signature, the `N,` note lines, the lane range, and that at least one note exists.
