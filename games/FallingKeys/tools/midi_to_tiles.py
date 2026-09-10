#!/usr/bin/env python3
"""Convert a Standard MIDI File into a fixed, ESP32-friendly PTC1 tile chart."""

from __future__ import annotations

import argparse
import bisect
import struct
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


MAX_EVENTS = 1024


class MidiError(ValueError):
    pass


@dataclass(frozen=True)
class RawEvent:
    tick: int
    track: int
    channel: int
    note: int
    velocity: int
    is_on: bool
    order: int


@dataclass(frozen=True)
class MidiNote:
    start_tick: int
    end_tick: int
    note: int
    velocity: int
    channel: int


@dataclass(frozen=True)
class TimedNote:
    start_ms: int
    end_ms: int
    note: int
    velocity: int


@dataclass(frozen=True)
class Tile:
    time_ms: int
    lane: int
    note: int
    duration_ms: int


def read_vlq(data: bytes, position: int) -> tuple[int, int]:
    value = 0
    for _ in range(4):
        if position >= len(data):
            raise MidiError("truncated variable-length quantity")
        byte = data[position]
        position += 1
        value = (value << 7) | (byte & 0x7F)
        if not byte & 0x80:
            return value, position
    raise MidiError("variable-length quantity is longer than four bytes")


def parse_track(data: bytes, track_index: int, order_start: int) -> tuple[list[RawEvent], list[tuple[int, int, int]], int, int]:
    position = 0
    tick = 0
    running_status: int | None = None
    events: list[RawEvent] = []
    tempos: list[tuple[int, int, int]] = []
    order = order_start

    while position < len(data):
        delta, position = read_vlq(data, position)
        tick += delta
        if position >= len(data):
            raise MidiError(f"track {track_index + 1} ends before an event")

        first = data[position]
        if first & 0x80:
            status = first
            position += 1
            if status < 0xF0:
                running_status = status
        else:
            if running_status is None:
                raise MidiError(f"track {track_index + 1} uses running status before a channel event")
            status = running_status

        if status == 0xFF:
            running_status = None
            if position >= len(data):
                raise MidiError("truncated meta event")
            meta_type = data[position]
            position += 1
            length, position = read_vlq(data, position)
            end = position + length
            if end > len(data):
                raise MidiError("truncated meta-event payload")
            if meta_type == 0x51 and length == 3:
                microseconds = int.from_bytes(data[position:end], "big")
                if microseconds:
                    tempos.append((tick, microseconds, order))
            position = end
            order += 1
            if meta_type == 0x2F:
                break
            continue

        if status in (0xF0, 0xF7):
            running_status = None
            length, position = read_vlq(data, position)
            position += length
            if position > len(data):
                raise MidiError("truncated SysEx event")
            order += 1
            continue
        if status >= 0xF0:
            raise MidiError(f"unsupported system event 0x{status:02X}")

        event_type = status & 0xF0
        channel = status & 0x0F
        data_length = 1 if event_type in (0xC0, 0xD0) else 2
        if position + data_length > len(data):
            raise MidiError("truncated channel event")
        data1 = data[position]
        data2 = data[position + 1] if data_length == 2 else 0
        position += data_length
        if event_type in (0x80, 0x90):
            is_on = event_type == 0x90 and data2 != 0
            events.append(RawEvent(tick, track_index, channel, data1, data2, is_on, order))
        order += 1

    return events, tempos, tick, order


def read_midi(path: Path) -> tuple[int, list[tuple[int, int, int]], list[MidiNote]]:
    data = path.read_bytes()
    if len(data) < 14 or data[:4] != b"MThd":
        raise MidiError("not a Standard MIDI File")
    header_length = struct.unpack_from(">I", data, 4)[0]
    if header_length < 6 or 8 + header_length > len(data):
        raise MidiError("invalid MIDI header")
    midi_format, track_count, division = struct.unpack_from(">HHH", data, 8)
    if midi_format not in (0, 1):
        raise MidiError("only MIDI format 0 and 1 files are supported")
    if division & 0x8000 or division == 0:
        raise MidiError("SMPTE time division is not supported")

    position = 8 + header_length
    raw_events: list[RawEvent] = []
    tempos: list[tuple[int, int, int]] = []
    order = 0
    final_tick = 0
    for track_index in range(track_count):
        if position + 8 > len(data) or data[position : position + 4] != b"MTrk":
            raise MidiError(f"missing track chunk {track_index + 1}")
        length = struct.unpack_from(">I", data, position + 4)[0]
        position += 8
        end = position + length
        if end > len(data):
            raise MidiError(f"track {track_index + 1} is truncated")
        events, track_tempos, track_end, order = parse_track(data[position:end], track_index, order)
        raw_events.extend(events)
        tempos.extend(track_tempos)
        final_tick = max(final_tick, track_end)
        position = end

    active: dict[tuple[int, int, int], list[RawEvent]] = defaultdict(list)
    notes: list[MidiNote] = []
    raw_events.sort(key=lambda event: (event.tick, 0 if not event.is_on else 1, event.order))
    for event in raw_events:
        key = (event.track, event.channel, event.note)
        if event.is_on:
            active[key].append(event)
        elif active[key]:
            start = active[key].pop(0)
            if event.tick > start.tick:
                notes.append(MidiNote(start.tick, event.tick, event.note, start.velocity, start.channel))
    for waiting in active.values():
        for start in waiting:
            notes.append(MidiNote(start.tick, max(start.tick + 1, final_tick), start.note, start.velocity, start.channel))
    if not notes:
        raise MidiError("MIDI contains no complete note events")
    return division, tempos, notes


class TempoMap:
    def __init__(self, division: int, tempos: Iterable[tuple[int, int, int]]):
        latest: dict[int, tuple[int, int]] = {0: (500_000, -1)}
        for tick, microseconds, order in sorted(tempos, key=lambda item: (item[0], item[2])):
            latest[tick] = (microseconds, order)
        ordered = sorted((tick, value[0]) for tick, value in latest.items())
        self.division = division
        self.ticks: list[int] = []
        self.tempos: list[int] = []
        self.cumulative_units: list[int] = []
        cumulative = 0
        previous_tick = ordered[0][0]
        previous_tempo = ordered[0][1]
        for tick, tempo in ordered:
            cumulative += (tick - previous_tick) * previous_tempo
            self.ticks.append(tick)
            self.tempos.append(tempo)
            self.cumulative_units.append(cumulative)
            previous_tick = tick
            previous_tempo = tempo

    def milliseconds(self, tick: int) -> int:
        index = bisect.bisect_right(self.ticks, tick) - 1
        units = self.cumulative_units[index] + (tick - self.ticks[index]) * self.tempos[index]
        denominator = self.division * 1000
        return (units + denominator // 2) // denominator

    @property
    def initial_bpm(self) -> int:
        return round(60_000_000 / self.tempos[0])


def nearest_free_lane(preferred: int, lanes: int, occupied: set[int], previous: int | None) -> int:
    choices = [lane for lane in range(lanes) if lane not in occupied]
    return min(choices, key=lambda lane: (abs(lane - preferred), lane == previous, lane))


def select_tiles(notes: list[TimedNote], lanes: int, difficulty: str) -> list[Tile]:
    settings = {
        "easy": (1, 180, 600),
        "normal": (min(2, lanes), 90, 450),
        "hard": (min(4, lanes), 45, 340),
    }
    max_chord, minimum_spacing, hold_threshold = settings[difficulty]
    ordered = sorted(notes, key=lambda note: (note.start_ms, note.note, -note.velocity))
    low = min(note.note for note in ordered)
    high = max(note.note for note in ordered)

    groups: list[list[TimedNote]] = []
    for note in ordered:
        if not groups or note.start_ms - groups[-1][0].start_ms > 12:
            groups.append([note])
        else:
            groups[-1].append(note)

    accepted: list[list[TimedNote]] = []
    last_time = -minimum_spacing
    for group in groups:
        if group[0].start_ms - last_time < minimum_spacing:
            continue
        ranked = sorted(group, key=lambda note: (note.velocity, note.note, note.end_ms - note.start_ms), reverse=True)
        chosen = sorted(ranked[:max_chord], key=lambda note: note.note)
        accepted.append(chosen)
        last_time = group[0].start_ms
    if not accepted:
        raise MidiError("no notes remain after difficulty filtering")

    first_time = accepted[0][0].start_ms
    previous_lane: int | None = None
    tiles: list[Tile] = []
    for group in accepted:
        occupied: set[int] = set()
        for note in group:
            preferred = (lanes - 1) // 2 if high == low else round((note.note - low) * (lanes - 1) / (high - low))
            lane = nearest_free_lane(preferred, lanes, occupied, previous_lane)
            occupied.add(lane)
            previous_lane = lane
            raw_duration = max(0, note.end_ms - note.start_ms)
            duration = min(8000, raw_duration) if raw_duration >= hold_threshold else 0
            tiles.append(Tile(note.start_ms - first_time + 1000, lane, note.note, duration))
    return sorted(tiles, key=lambda tile: (tile.time_ms, tile.lane))


def distributed_indexes(count: int, wanted: int) -> list[int]:
    if wanted >= count:
        return list(range(count))
    if wanted == 1:
        return [0]
    return [round(index * (count - 1) / (wanted - 1)) for index in range(wanted)]


def limit_tiles(tiles: list[Tile], maximum: int = MAX_EVENTS) -> tuple[list[Tile], bool]:
    if len(tiles) <= maximum:
        return tiles, False
    groups: list[list[Tile]] = []
    for tile in tiles:
        if not groups or tile.time_ms != groups[-1][0].time_ms:
            groups.append([tile])
        else:
            groups[-1].append(tile)
    if len(groups) > maximum:
        kept = [groups[index] for index in distributed_indexes(len(groups), maximum)]
        return [max(group, key=lambda tile: tile.note) for group in kept], True

    result = [max(group, key=lambda tile: tile.note) for group in groups]
    extras = [tile for group in groups for tile in group if tile != max(group, key=lambda item: item.note)]
    for index in distributed_indexes(len(extras), min(len(extras), maximum - len(result))):
        result.append(extras[index])
    return sorted(result, key=lambda tile: (tile.time_ms, tile.lane)), True


def clean_metadata(text: str, fallback: str) -> str:
    cleaned = " ".join(text.replace("=", " ").replace(",", " ").split())
    return (cleaned or fallback)[:35]


def convert(
    input_path: Path,
    output_path: Path,
    lanes: int,
    difficulty: str,
    title: str | None,
    artist: str,
    include_drums: bool = False,
) -> tuple[int, bool]:
    division, tempos, midi_notes = read_midi(input_path)
    tempo_map = TempoMap(division, tempos)
    playable_notes = midi_notes if include_drums else [note for note in midi_notes if note.channel != 9]
    if not playable_notes:
        raise MidiError("MIDI contains only percussion; use --include-drums to convert it")
    timed = [
        TimedNote(tempo_map.milliseconds(note.start_tick), tempo_map.milliseconds(note.end_tick), note.note, note.velocity)
        for note in playable_notes
    ]
    tiles, thinned = limit_tiles(select_tiles(timed, lanes, difficulty))
    chart_title = clean_metadata(title or input_path.stem, "Untitled")
    chart_artist = clean_metadata(artist, "MIDI import")
    lines = [
        "PTC1",
        f"title={chart_title}",
        f"artist={chart_artist}",
        f"lanes={lanes}",
        f"bpm={max(30, min(300, tempo_map.initial_bpm))}",
        "offset=0",
        "# N,time_ms,lane,note,duration_ms",
    ]
    lines.extend(
        f"N,{tile.time_ms},{tile.lane},{tile.note},{tile.duration_ms}" for tile in tiles
    )
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    return len(tiles), thinned


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Convert a Standard MIDI File into a Falling Keys PTC1 chart."
    )
    parser.add_argument("input", type=Path, help="input .mid or .midi file")
    parser.add_argument("output", type=Path, help="output .ptc chart")
    parser.add_argument("--lanes", type=int, choices=(4, 5, 6), default=4)
    parser.add_argument("--difficulty", choices=("easy", "normal", "hard"), default="normal")
    parser.add_argument("--title", help="chart title shown on the calculator")
    parser.add_argument("--artist", default="MIDI import", help="optional chart artist")
    parser.add_argument("--include-drums", action="store_true", help="include MIDI channel 10 percussion notes")
    return parser


def main() -> int:
    parser = build_parser()
    arguments = parser.parse_args()
    try:
        count, thinned = convert(
            arguments.input,
            arguments.output,
            arguments.lanes,
            arguments.difficulty,
            arguments.title,
            arguments.artist,
            arguments.include_drums,
        )
    except (MidiError, OSError) as error:
        parser.exit(2, f"Error: {error}\n")
    print(f"Wrote {count} tiles to {arguments.output}")
    if thinned:
        print(f"Chart was distributed across the {MAX_EVENTS}-event device limit.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
