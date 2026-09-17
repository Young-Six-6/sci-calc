# Reading Numbered Musical Notation

[Chinese](numbered_notation_CH.md)

This guide explains common numbered musical notation (jianpu) and how to translate it into the SCI-CALC MusicKeyboard. It is intentionally independent of any one song.

## 1. Scale degrees

In the key of C, the basic scale is:

| Number | Solfege | Pitch |
| --- | --- | --- |
| `1` | Do | C |
| `2` | Re | D |
| `3` | Mi | E |
| `4` | Fa | F |
| `5` | Sol | G |
| `6` | La | A |
| `7` | Ti | B |
| `0` | Rest | No pitch |

`1=C` means that scale degree 1 is C. `1=C4` also gives the register: the tonic is C4. The number-to-pitch relationship changes when the key changes, but the fingering pattern stays the same.

## 2. Octave dots

- A dot above a number raises it by one octave.
- Two dots above raise it by two octaves.
- A dot below lowers it by one octave.
- Two dots below lower it by two octaves.

The position matters. A dot above or below the number changes pitch; a dot after the number is usually a duration mark. Printed fonts can make these dots look similar, so check their position.

## 3. Rhythm marks

- A bar line divides measures.
- `4/4` means four quarter-note beats per measure.
- A plain number normally occupies the current beat subdivision.
- One underline, or one beam, usually means an eighth-note subdivision.
- Two underlines, or two beams, usually mean a sixteenth-note subdivision.
- A dot after a note commonly adds half of that note's duration.
- A horizontal `-` continues the preceding note instead of starting a new pitch.
- A curved tie joins notes of the same pitch; a slur groups notes into one phrase.

Always follow the score's printed beat value. For example, quarter note `=62` means set a metronome or player to 62 BPM. It is not the same as the default 120 BPM.

## 4. Two-staff scores

In a piano score, the upper staff is usually the right hand and the lower staff is usually the left hand. The lower staff may contain several simultaneous notes. A limited-key instrument is easier to learn by playing the upper melody first, then adding only the most important accompaniment notes or chords.

## 5. SCI-CALC Calculator layout

With **Root = C**, **Octave = 4**, and **Transpose = 0**, use the following mapping:

| Numbered notation | Calculator key |
| --- | --- |
| low 6, low 7 | `.` , `0` |
| 1 2 3 4 5 6 7 | `1` `2` `3` `4` `5` `6` `7` |
| high 1, high 2 | `8`, `9` |
| high 3 4 5 6 7 | `/` `*` `-` `+` `^` |
| rest `0` | Do not press a note key |

This table describes ordinary numbered notation. The physical key `0` is the low-7 note in the calculator layout, while a printed `0` in a traditional score is a rest. The built-in converter avoids this ambiguity by writing rests as `·` in its `CALC` sheet.

For notes above the direct calculator range, the converter may write `↑x`. Hold `ENTER`, press `x`, then release the note key. Releasing `x` before `ENTER` gives a cleaner separation between notes.

## 6. A reliable practice method

1. Read the key and starting register from the score header.
2. Set Root, Octave, Transpose, Tone, and BPM in MusicKeyboard.
3. Follow the upper melody from left to right.
4. Treat `0` as silence in a traditional score; hold the previous note for `-`.
5. Count the beats inside each bar before adding faster underlined notes.
6. Add the lower accompaniment only after the melody is comfortable.

For a MIDI file, the desktop MusicKeyboard **Convert** page can perform melody selection and print the exact `CALC` key tokens. Use the `1/8` grid for an easier first pass and `1/16` when the melody needs finer rhythm detail.
