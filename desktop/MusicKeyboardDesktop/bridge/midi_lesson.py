#!/usr/bin/env python3

"""Build an interactive SCI-CALC lesson from a MIDI file."""

from __future__ import annotations

import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
TOOLS_DIR = REPO_ROOT / "games" / "MusicKeyboard" / "tools"
sys.path.insert(0, str(TOOLS_DIR))

from midi_to_calc import (  # noqa: E402
    DEFAULT_OCTAVE,
    NOTE_NAMES,
    build_actions,
    calc_action,
    choose_setting,
    isolate_melody_voice,
    melody_track_score,
    midi_info,
    non_melody_reason,
    note_name,
    parse_track,
    quantize,
    reduce_to_melody,
)

try:
    import mido
except ModuleNotFoundError as exc:  # pragma: no cover - handled by the CLI
    raise SystemExit("Missing mido; install games/MusicKeyboard/tools/requirements.txt") from exc


KEY_IDS = {
    "/": 1,
    "*": 2,
    "-": 3,
    "SIN": 4,
    "SEC": 5,
    "7": 6,
    "8": 7,
    "9": 8,
    "+": 9,
    "COS": 10,
    "CSC": 11,
    "4": 12,
    "5": 13,
    "6": 14,
    "^": 15,
    "TAN": 16,
    "COT": 17,
    "1": 18,
    "2": 19,
    "3": 20,
    "ENTER": 21,
    "EXP": 22,
    "EXTRA": 23,
    "0": 25,
    ".": 26,
}

COMPLEXITY_LIMITS = {
    "melody": 1,
    "simple": 2,
    "full": 4,
}


def select_melody_track(tracks):
    candidates = [
        item for item in tracks
        if non_melody_reason(item[1], item[2]) is None
    ]
    if not candidates:
        raise ValueError("No reliable melody track was found")
    return max(candidates, key=lambda item: melody_track_score(item[1], item[2]))


def action_key_id(action):
    key_name = action.key[1:] if action.kind == "upper" else action.key
    return KEY_IDS.get(key_name)


def accompaniment_pool(tracks):
    notes = []
    for _, _, track_notes in tracks:
        notes.extend(track_notes)
    return sorted(notes, key=lambda note: (note.start, note.pitch, note.end))


def active_notes_for_melody(melody_notes, all_notes, step):
    cursor = 0
    active = []
    lead = max(1, int(step * 0.18))
    grace = max(1, int(step * 0.10))

    for melody in melody_notes:
        while cursor < len(all_notes) and all_notes[cursor].start <= melody.start + lead:
            active.append(all_notes[cursor])
            cursor += 1
        active = [note for note in active if note.end >= melody.start - grace]
        yield melody, active


def candidate_score(note, melody, step):
    window_end = melody.start + max(step, melody.end - melody.start)
    overlap = max(0, min(note.end, window_end) - max(note.start, melody.start))
    lower_bonus = 0.35 if note.pitch < melody.pitch else 0.0
    distance_penalty = abs(note.pitch - melody.pitch) * 0.006
    return overlap / max(1, step) + note.velocity / 254.0 + lower_bonus - distance_penalty


def build_steps(melody_notes, all_notes, base, actions, step, complexity):
    limit = COMPLEXITY_LIMITS[complexity]
    steps = []
    skipped = 0

    for melody, active in active_notes_for_melody(melody_notes, all_notes, step):
        melody_action = calc_action(melody.pitch, base, actions)
        if melody_action is None:
            skipped += 1
            continue

        mode = melody_action.kind
        chosen = [(melody, melody_action)]
        used_pitches = {melody.pitch}
        used_pitch_classes = {melody.pitch % 12}
        used_keys = {action_key_id(melody_action)}

        if limit > 1:
            ranked = sorted(
                active,
                key=lambda note: candidate_score(note, melody, step),
                reverse=True,
            )
            for note in ranked:
                if len(chosen) >= limit or note.pitch in used_pitches:
                    continue
                action = calc_action(note.pitch, base, actions)
                key_id = action_key_id(action) if action else None
                if action is None or action.kind != mode or key_id is None or key_id in used_keys:
                    continue
                if note.pitch % 12 in used_pitch_classes:
                    continue
                chosen.append((note, action))
                used_pitches.add(note.pitch)
                used_pitch_classes.add(note.pitch % 12)
                used_keys.add(key_id)

        chosen.sort(key=lambda item: item[0].pitch)
        note_items = []
        key_ids = [KEY_IDS["ENTER"]] if mode == "upper" else []
        for note, action in chosen:
            key_id = action_key_id(action)
            if key_id is None:
                continue
            key_ids.append(key_id)
            note_items.append({
                "pitch": note.pitch,
                "name": note_name(note.pitch),
                "key_id": key_id,
                "key": action.key,
            })

        if not note_items:
            skipped += 1
            continue
        steps.append({
            "tick": melody.start,
            "beat": round(melody.start / max(1, step) * 0.25, 3),
            "duration_beats": round(max(1, melody.end - melody.start) / max(1, step) * 0.25, 3),
            "requires_enter": mode == "upper",
            "key_ids": key_ids,
            "notes": note_items,
        })
        if len(steps) >= 12000:
            break

    return steps, skipped


def build_lesson(midi_path, complexity, grid):
    mid = mido.MidiFile(midi_path)
    tracks = []
    for index, track in enumerate(mid.tracks):
        name, notes = parse_track(track, index)
        if notes:
            tracks.append((index, name, notes))
    if not tracks:
        raise ValueError("The MIDI file contains no playable notes")

    melody_index, melody_name, track_notes = select_melody_track(tracks)
    voice_notes, _ = isolate_melody_voice(track_notes)
    step = mid.ticks_per_beat * 4 / grid
    melody_notes = reduce_to_melody(voice_notes, step)
    if not melody_notes:
        raise ValueError("The melody track contains no usable notes")

    actions = build_actions("EXTRA")
    setting = choose_setting(melody_notes, actions)
    base = setting["base"]
    all_notes = accompaniment_pool(tracks)
    steps, skipped = build_steps(
        melody_notes,
        all_notes,
        base,
        actions,
        step,
        complexity,
    )
    if not steps:
        raise ValueError("No melody notes fit the calculator keyboard range")

    bpm, numerator, denominator = midi_info(mid)
    return {
        "version": 1,
        "file_name": Path(midi_path).name,
        "complexity": complexity,
        "bpm": round(bpm, 2),
        "time_signature": f"{numerator}/{denominator}",
        "track": melody_index,
        "track_name": melody_name,
        "root": setting["root_pc"],
        "octave": DEFAULT_OCTAVE + setting["octave_shift"],
        "transpose": 0,
        "base_note": base,
        "step_count": len(steps),
        "skipped_count": skipped,
        "steps": steps,
    }


def main():
    parser = argparse.ArgumentParser(description="Build SCI-CALC interactive MIDI lessons")
    parser.add_argument("midi")
    parser.add_argument("--complexity", choices=tuple(COMPLEXITY_LIMITS), default="melody")
    parser.add_argument("--grid", type=int, choices=(8, 16), default=16)
    args = parser.parse_args()

    try:
        lesson = build_lesson(args.midi, args.complexity, args.grid)
    except (OSError, EOFError, KeyError, ValueError) as exc:
        raise SystemExit(str(exc)) from exc
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    print(json.dumps(lesson, ensure_ascii=False, separators=(",", ":")))


if __name__ == "__main__":
    main()
