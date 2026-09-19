# SCI-CALC Music Keyboard Desktop

[Chinese](README_CH.md)

An Electron desktop instrument for the SCI-CALC MusicKeyboard application. The calculator remains the physical controller: this app receives its `SKB1` serial key events and turns them into Web Audio, so it does not inject key presses into Windows.

## What is included

- The same 5x6 calculator key layout and note mapping as the firmware.
- Piano and calculator views on the Keys page; the calculator view mirrors the physical 5x6 key arrangement and live key states.
- Calculator, Chords, and Chromatic layouts.
- Piano, E.Piano, Organ, Warm Pad, Lead, and Bell tone patches.
- A desktop Analog Synth page with two oscillators, detune, noise, low-pass filter, ADSR, pitch LFO, and saved presets.
- A Groove page with a 16/32-step drum machine and a calculator-controlled arpeggiator.
- A chord accompaniment assistant with major, minor, seventh, and suspended voicings, optional bass, and tempo-synced bass pulses.
- A fixed-length layered looper with a base take, cycle-boundary overdubbing, per-layer mute, and removal.
- A MIDI follow challenge with phrase preview, timed calculator-key input, combo scoring, and progressive difficulty.
- A session-only sample keyboard that imports one audio file, slices it across calculator keys, and supports range and speed control.
- A Sound Lab with live waveform, smoothed trace, and spectrum views.
- Click-free note release that continues smoothly from the current attack or decay level for every tone.
- 16-voice polyphony, sustain, metronome, octave, root, transpose, volume, velocity, and BPM controls.
- Keys, Learn, Scope, Recorder, Convert, Settings, Synth, and Groove pages.
- Guided MIDI lessons with melody-only, simple-chord, and full-chord difficulty; the next calculator and piano keys are highlighted until played.
- Input timing calibration with click-track sampling, robust median estimation, persistent global offset, and manual 5 ms nudging.
- A built-in MIDI-to-calculator-sheet page with automatic melody selection, readable 1/8 or 1/16 grids, optional chord suggestions, and full-track/manual-track options.
- Selectable Scope refresh rates from 15 to 120 FPS for balancing smoothness and CPU use.
- In-memory recording with up to 2048 events, playback, and standard Format 0 MIDI export.
- English by default with a Chinese interface option.

## Requirements

- Windows with Electron 37 or another recent Electron runtime.
- A SCI-CALC firmware build with the SerialKeyboard interface enabled.
- The calculator connected by USB. The serial bridge uses 115200 baud and the existing Python `pyserial` dependency.
- Speakers or headphones connected to the computer.

## Run

```powershell
cd desktop\MusicKeyboardDesktop
npm install
npm start
```

Use **Refresh** to enumerate ports, select the calculator port (for example `COM7`), click **Enable audio**, and then click **Connect**. The app waits for the calculator's device-info packet before marking the connection ready.

The Python bridge is read-only. It parses the calculator protocol and sends structured events to Electron; it never generates Windows keyboard input and never writes to the calculator serial port.

## Playing

The normal calculator keys play notes. In Calculator layout, hold `ENTER` before a note key for the upper octave. In Chords layout, `*`, `/`, `-`, `+`, and `^` are chord pads. Use `TAB` to move through the eight desktop pages; the physical calculator's page actions remain available as documented by the firmware. For a general guide to numbered notation and its calculator mapping, see [Reading Numbered Musical Notation](../../docs/numbered_notation.md).

## Analog Synth

Open **Synth** to choose **Factory tone** or **Custom patch**. The custom engine combines two Web Audio oscillators, oscillator 2 detune, filtered noise, a low-pass filter with resonance, an amplitude ADSR envelope, and a pitch LFO. The preset list provides Init Poly, Warm Pad, Bright Lead, Mono Bass, and Digital Bell starting points. Parameter changes affect notes started after the edit and are saved locally; the existing 16-voice ceiling and calculator key mapping remain unchanged.

## Groove

Open **Groove** to build a local rhythm loop. The drum machine has kick, snare, hi-hat, and clap lanes with 16 or 32 steps, 1/8 or 1/16 timing, clear/randomize actions, a shared BPM control, and a highlighted playhead. The pattern is saved in the desktop app and can play beside live calculator notes.

Enable **Arpeggiator**, hold one or more calculator notes, and choose Up, Down, Up / down, or Random. Rate, octave span, and gate time are adjustable. Turning it off returns held keys to direct playing. The arpeggiator follows the current desktop tone, transpose, velocity, and 16-voice limit; it does not change the calculator firmware.

### Chord accompaniment

In the **Chord accompaniment** panel, enable the assistant and hold one calculator note. The root note becomes a major triad by default; Minor triad, Seventh, and Suspended 4 are also available. **Auto bass** adds a bass note one octave below the root. With **Bass pulse** enabled, that bass is retriggered at the shared BPM instead of being held continuously. The assistant and arpeggiator are mutually exclusive so a held key has one predictable interpretation.

### Layered looper

The **Layered loop** panel records musical events rather than rendered audio. Choose 1, 2, 4, or 8 bars, then click **Record base** and play the first layer. The loop length is fixed when recording starts, using the current BPM. When the cycle ends, playback starts automatically.

While a loop is playing, click **Record layer** to arm an overdub. Recording begins at the next loop boundary, so each layer stays aligned. Click it again before that boundary to cancel the pending overdub. Each layer can be muted or removed independently. **Stop looper** stops playback but keeps the layers; **Clear layers** removes them all. The loop engine does not capture its own playback, and stopping it releases only voices created by the loop.

## Guided MIDI lessons

Open **Learn**, choose a MIDI file, and select a chord complexity. **Melody only** requests one note per step, **Simple chord** adds one compatible harmony note, and **Full chord** uses up to four notes that the calculator can play together. Starting the lesson applies the recommended root and octave, switches to Calculator layout, and opens Keys. Gold keys are the next targets; correctly played keys turn green, and the lesson advances automatically after the complete step has been entered. When `ENTER` is highlighted, hold it before pressing the other highlighted key or keys.

The lesson is untimed and waits for the player. **Previous**, **Skip**, and **Stop lesson** remain available above the piano or calculator visualization. Changing chord complexity rebuilds the lesson from the selected MIDI file.

## Follow challenge

The **Follow challenge** panel uses the loaded lesson MIDI. Choose Easy, Normal, or Hard, then start a round. The desktop previews a short phrase, switches to **Your turn**, and highlights the calculator keys required for the next step. Chords and `ENTER` are checked as a group, so a complete step must be reproduced.

The challenge records score, combo, mistakes, and accuracy locally in the current session. Easy, Normal, and Hard begin with different phrase lengths and timing windows; every two cleared rounds the phrase grows until that difficulty's cap. A missed step breaks the combo and advances after its time window. The preview uses the desktop audio engine only when audio is enabled; serial input remains an input device and no Windows keyboard events are emitted.

The Shift layer follows the firmware:

| Calculator shortcut | Action |
| --- | --- |
| `SHIFT+1` / `SHIFT+3` | Previous / next tone |
| `SHIFT+4` / `SHIFT+6` | Octave down / up |
| `SHIFT+2` / `SHIFT+8` | Transpose down / up |
| `SHIFT+/` / `SHIFT+*` | Root down / up |
| `SHIFT+-` / `SHIFT++` | Volume down / up |
| `SHIFT+7` / `SHIFT+9` | BPM down / up |
| `SHIFT+^` / `SHIFT+EXP` | Velocity down / up |
| `SHIFT+5` | Sustain |
| `SHIFT+.` | Metronome |
| `SHIFT+0` | Start or stop recording |
| `SHIFT+ENTER` | Play or stop the current take |
| `SHIFT+BACKSPACE` | Clear the current take |
| `SHIFT+TAB` | Calculator / Chords / Chromatic |
| `SHIFT+ANGLE` | Open Settings |
| `SHIFT+EXTRA` | Panic / all notes off |

## MIDI key-sheet converter

Open the **Convert** page, choose a `.mid` or `.midi` file, and click **Convert MIDI**. The desktop page calls the maintained project converter, which chooses a reliable melody track, repairs unmatched final notes, reduces dense passages to a playable melody, and prints the same `CALC` sheet format used by the command-line tool. Use **Melody track** when automatic selection needs help; track numbers use the converter's zero-based numbering. Enable **Include chord suggestions** only when needed because it can make a long report.

The converter requires the Python dependency listed in `games\MusicKeyboard\tools\requirements.txt`. The report is shown in the app and is not written back to the MIDI file.

## Input timing calibration

Open **Settings**, enable audio, and use **Input timing calibration**. The app gives four count-in beats, then plays 8, 12, or 16 measured beats. Press one note key once on each beat and release it before the next beat. The result uses the median of the valid samples and trims extreme samples on longer runs. A positive value means the serial key event arrived after the reference beat; the global correction moves recorded event timestamps earlier by that amount. The value is saved locally and can be adjusted in 5 ms steps.

This corrects recorded and future timed-performance data. It cannot make live audio start before the physical key event arrives, so it does not claim to remove the causal input-to-sound delay.

## Scope refresh rate

The Scope page offers 15, 30, 60, and 120 FPS. The audio engine keeps running at its normal Web Audio rate; this option only changes how often the diagnostic waveform and Sound Lab are painted.

### Sample keyboard

Open **Synth**, choose **Choose audio**, and select a WAV, MP3, OGG, or other browser-decodable audio file. The desktop app keeps the decoded buffer in memory for the current session. Set the slice count from 2 to 12, adjust the start and end range, and change playback speed from 0.50x to 2.00x. Enable **Sample mode** to make the calculator trigger slices instead of synthesizer notes. The available mapping is `1 / 2 / 3 / 4 / 5 / 6 / 7 / 8 / 9 / 0 / . / SIN`; other note keys are ignored while sample mode is active, while global controls such as page switching and panic remain available. Sample mode, the arpeggiator, and chord accompaniment do not run at the same time.

### Sound Lab

The **Sound Lab** sits below the Scope waveform. **Live waveform** is the current analyser buffer, **Filtered trace** is a display-only moving average, and **Spectrum** shows the analyser frequency bins. Smoothing and display gain change only the drawing; they do not alter the synth, sample playback, voice limit, or recorded data.

## Recording and MIDI

Stopping a non-empty recording opens a save dialog automatically, matching the calculator workflow. **Save MIDI** can also be used from the Recorder page. The default location is `Documents\recordings`, with names such as `KEYS000.MID`; the take remains in memory when the dialog is canceled.

## Troubleshooting

- If no ports appear, install the existing PC-side dependency from `games\SerialKeyboard\pc\requirements.txt`, reconnect the USB cable, and use **Refresh**.
- If the connection stays in Connecting, check that the calculator is running the SerialKeyboard-capable main firmware and that no other application has opened the same COM port.
- If the meters move but there is no sound, click **Enable audio** and check the Windows output device.
- Use **Panic / silence** after a cable disconnect or an interrupted key release.
