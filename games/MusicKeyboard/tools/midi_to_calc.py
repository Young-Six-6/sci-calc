#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
midi_to_calc.py

把 MIDI 自动转换成 SCI-CALC 音乐键盘谱，自动提取旋律，并优先推荐易读的单键指法。

依赖：
    pip install mido

用法：
    python midi_to_calc.py song.mid
    python midi_to_calc.py song.mid --grid 16
    python midi_to_calc.py song.mid --track 3
    python midi_to_calc.py song.mid --full-track
    python midi_to_calc.py song.mid --chords
    python midi_to_calc.py song.mid --out song_calc.txt
    python midi_to_calc.py song.mid --out song_calc.txt --quiet
"""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict, deque
from dataclasses import dataclass
from pathlib import Path
from statistics import mean
from typing import Dict, List, Optional, Tuple
from unicodedata import east_asian_width

try:
    import mido
except ModuleNotFoundError:
    mido = None


RPN_KEY_NAME = "RPN"
DEFAULT_C_MIDI = 60
DEFAULT_OCTAVE = 4
TOOL_VERSION = "2.1"

NOTE_NAMES = [
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B",
]


@dataclass
class Note:
    start: int
    end: int
    pitch: int
    velocity: int
    track: int
    repaired: bool = False


@dataclass(frozen=True)
class Action:
    key: str
    presses: int
    kind: str  # single / upper
    effort: int


def single_key_effort(key: str) -> int:
    if key in {".", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"}:
        return 0
    if key in {"/", "*", "-", "+", "^"}:
        return 1
    return 2


def build_single_keys(rpn_key_label: str) -> Dict[int, str]:
    return {
        -3: ".",
        -1: "0",

         0: "1",
         1: "SIN",
         2: "2",
         3: "SEC",
         4: "3",
         5: "4",
         6: "COS",
         7: "5",
         8: "CSC",
         9: "6",
        10: "TAN",
        11: "7",

        12: "8",
        13: "COT",
        14: "9",
        15: "EXP",

        16: "/",
        17: "*",
        18: rpn_key_label,
        19: "-",
        21: "+",
        23: "^",
    }


def build_actions(rpn_key_label: str) -> Dict[int, Action]:
    single = build_single_keys(rpn_key_label)
    actions: Dict[int, Action] = {}

    # 单键永远优先。
    for offset, key in single.items():
        actions[offset] = Action(
            key=key,
            presses=1,
            kind="single",
            effort=single_key_effort(key),
        )

    # 只有目标音没有单键时，才加入 ENTER 临时高八度组合。
    for offset, key in single.items():
        high = offset + 12
        if high not in actions:
            actions[high] = Action(
                key=f"↑{key}",
                presses=2,
                kind="upper",
                effort=4 + single_key_effort(key),
            )

    return actions


def note_name(n: int) -> str:
    return f"{NOTE_NAMES[n % 12]}{n // 12 - 1}"


def fit_display(value, width: int, align: str = "left") -> str:
    """Pad or trim text by terminal display width instead of code-point count."""
    chars = []
    used = 0

    for char in str(value):
        char_width = 2 if east_asian_width(char) in {"W", "F"} else 1
        if used + char_width > width:
            break
        chars.append(char)
        used += char_width

    text = "".join(chars)
    padding = " " * (width - used)
    return padding + text if align == "right" else text + padding


def parse_track(track, track_index: int) -> Tuple[str, List[Note]]:
    notes: List[Note] = []
    active = defaultdict(deque)
    tick = 0
    track_name = f"Track {track_index}"

    for msg in track:
        tick += msg.time

        if msg.type == "track_name":
            track_name = msg.name

        # MIDI 通道 10（0-based 为 9）通常是打击乐。
        if getattr(msg, "channel", None) == 9:
            continue

        if msg.type == "note_on" and msg.velocity > 0:
            active[(msg.channel, msg.note)].append((tick, msg.velocity))

        elif (
            msg.type == "note_off"
            or (msg.type == "note_on" and msg.velocity == 0)
        ):
            key = (msg.channel, msg.note)
            if active[key]:
                start, vel = active[key].popleft()
                if tick > start:
                    notes.append(
                        Note(
                            start=start,
                            end=tick,
                            pitch=msg.note,
                            velocity=vel,
                            track=track_index,
                        )
                    )

    # Some downloaded or clipped MIDI files end without matching Note Off events.
    # Keep their final onsets instead of silently losing them.
    for (_, pitch), pending in active.items():
        while pending:
            start, vel = pending.popleft()
            notes.append(
                Note(
                    start=start,
                    end=max(tick, start + 1),
                    pitch=pitch,
                    velocity=vel,
                    track=track_index,
                    repaired=True,
                )
            )

    notes.sort(key=lambda n: (n.start, n.pitch, n.end))
    return track_name, notes


def midi_info(mid: mido.MidiFile) -> Tuple[float, int, int]:
    tempo = 500000
    numerator = 4
    denominator = 4

    tempo_found = False
    sig_found = False

    for track in mid.tracks:
        for msg in track:
            if msg.type == "set_tempo" and not tempo_found:
                tempo = msg.tempo
                tempo_found = True

            if msg.type == "time_signature" and not sig_found:
                numerator = msg.numerator
                denominator = msg.denominator
                sig_found = True

    return mido.tempo2bpm(tempo), numerator, denominator


def overlap_ratio(notes: List[Note]) -> float:
    if not notes:
        return 1.0

    ordered = sorted(notes, key=lambda n: (n.start, n.pitch))
    last_end = -1
    overlaps = 0

    for n in ordered:
        if n.start < last_end:
            overlaps += 1
        last_end = max(last_end, n.end)

    return overlaps / len(ordered)


def non_melody_reason(name: str, notes: List[Note]) -> Optional[str]:
    if len(notes) < 4:
        return "音符过少"

    lowered = name.casefold().strip()
    normalized = "".join(char if char.isalnum() else " " for char in lowered)
    tokens = set(normalized.split())
    compact = "".join(char for char in lowered if char.isalnum())

    if (
        compact in {"se", "sfx", "fx"}
        or any(token.startswith(("drum", "perc")) for token in tokens)
        or tokens.intersection({"rhythm", "beat", "noise", "sfx", "fx"})
        or any(word in lowered for word in ("打击", "鼓", "音效", "节奏"))
    ):
        return "鼓点或音效轨"

    unique_pitches = {note.pitch for note in notes}
    if len(unique_pitches) <= 2:
        return "音高变化过少"

    return None


def melody_track_score(name: str, notes: List[Note]) -> float:
    if non_melody_reason(name, notes) is not None:
        return -999999.0

    avg_pitch = mean(n.pitch for n in notes)
    mono = 1.0 - overlap_ratio(notes)
    lname = name.lower()

    bonus = 0.0

    for word in ("melody", "lead", "vocal", "voice", "theme", "solo"):
        if word in lname:
            bonus += 25.0

    for word in ("bass", "pad", "strings", "chord", "accomp"):
        if word in lname:
            bonus -= 15.0

    return (
        avg_pitch
        + mono * 25.0
        + min(len(notes), 300) / 30.0
        + bonus
    )


def calc_action(
    pitch: int,
    base_midi: int,
    actions: Dict[int, Action],
) -> Optional[Action]:
    return actions.get(pitch - base_midi)


def choose_setting(
    notes: List[Note],
    actions: Dict[int, Action],
) -> dict:
    """
    严格优先级：
      1) 不可弹音最少
      2) ENTER 组合最少
      3) 单键最多
      4) 数字键优先于功能键
      5) 从默认设置出发所需按键最少
    """
    best = None

    for root_pc in range(12):
        # The firmware exposes octaves 2 through 6 and starts at octave 4.
        for octave_shift in range(-2, 3):
            base = DEFAULT_C_MIDI + root_pc + octave_shift * 12

            missing = 0
            upper_count = 0
            single_count = 0
            play_effort = 0

            for n in notes:
                action = calc_action(n.pitch, base, actions)

                if action is None:
                    missing += 1
                elif action.kind == "single":
                    single_count += 1
                    play_effort += action.effort
                else:
                    upper_count += 1
                    play_effort += action.effort

            root_steps = min(root_pc, 12 - root_pc)
            setup_presses = abs(octave_shift) + root_steps

            score = (
                missing,
                upper_count,
                -single_count,
                play_effort,
                setup_presses,
                abs(octave_shift),
                root_steps,
                root_pc,
            )

            candidate = {
                "score": score,
                "root_pc": root_pc,
                "octave_shift": octave_shift,
                "base": base,
                "missing": missing,
                "upper": upper_count,
                "single": single_count,
                "play_effort": play_effort,
                "setup_presses": setup_presses,
                "total": len(notes),
            }

            if best is None or candidate["score"] < best["score"]:
                best = candidate

    return best


def setting_stats(
    notes: List[Note],
    base: int,
    actions: Dict[int, Action],
) -> dict:
    single = 0
    numeric_single = 0
    extended_single = 0
    upper = 0
    missing = 0

    for n in notes:
        action = calc_action(n.pitch, base, actions)

        if action is None:
            missing += 1
        elif action.kind == "single":
            single += 1
            if action.effort == 0:
                numeric_single += 1
            else:
                extended_single += 1
        else:
            upper += 1

    total = max(1, len(notes))

    return {
        "single_count": single,
        "numeric_single_count": numeric_single,
        "extended_single_count": extended_single,
        "upper_count": upper,
        "missing_count": missing,
        "single": single / total * 100.0,
        "upper": upper / total * 100.0,
        "missing": missing / total * 100.0,
    }


def quantize(value: float) -> int:
    """Round non-negative musical positions consistently, including exact halves."""
    return int(value + 0.5)


def isolate_melody_voice(notes: List[Note]) -> Tuple[List[Note], Optional[int]]:
    """Split a clearly polyphonic bass-plus-melody track at its largest pitch gap."""
    if len(notes) < 8 or overlap_ratio(notes) < 0.25:
        return notes, None

    counts = Counter(n.pitch for n in notes)
    pitches = sorted(counts)
    minimum_group = max(4, int(len(notes) * 0.12 + 0.5))
    candidates = []

    for left, right in zip(pitches, pitches[1:]):
        gap = right - left
        if gap < 5:
            continue
        lower_count = sum(counts[p] for p in pitches if p <= left)
        upper_count = len(notes) - lower_count
        if lower_count < minimum_group or upper_count < minimum_group:
            continue
        candidates.append(
            (gap, min(lower_count, upper_count), -abs(lower_count - upper_count), right)
        )

    if not candidates:
        return notes, None

    pitch_floor = max(candidates)[3]
    return [n for n in notes if n.pitch >= pitch_floor], pitch_floor


def reduce_to_melody(notes: List[Note], step: float) -> List[Note]:
    buckets = {}

    for n in notes:
        slot = quantize(n.start / step)
        old = buckets.get(slot)

        if old is None or (n.pitch, n.velocity, n.end - n.start) > (
            old.pitch,
            old.velocity,
            old.end - old.start,
        ):
            buckets[slot] = n

    return sorted(buckets.values(), key=lambda n: (n.start, n.pitch))


def render_melody(
    notes: List[Note],
    base: int,
    actions: Dict[int, Action],
    ticks_per_beat: int,
    numerator: int,
    denominator: int,
    grid: int = 8,
) -> List[str]:
    step = ticks_per_beat * 4 / grid

    measure_ticks = (
        ticks_per_beat
        * numerator
        * 4
        / denominator
    )

    slots_per_bar = max(1, quantize(measure_ticks / step))

    if not notes:
        return []

    data = defaultdict(lambda: ["·"] * slots_per_bar)

    min_bar = 10**9
    max_bar = 0

    for n in notes:
        qstart = quantize(n.start / step)
        qduration = max(1, quantize((n.end - n.start) / step))

        bar = qstart // slots_per_bar
        slot = qstart % slots_per_bar

        min_bar = min(min_bar, bar)
        max_bar = max(max_bar, bar)

        action = calc_action(n.pitch, base, actions)

        if action is None:
            token = f"?{note_name(n.pitch)}"
        else:
            token = action.key

        data[bar][slot] = token

        for i in range(1, qduration):
            q = qstart + i
            b = q // slots_per_bar
            s = q % slots_per_bar

            min_bar = min(min_bar, b)
            max_bar = max(max_bar, b)

            if data[b][s] == "·":
                data[b][s] = "—"

    token_width = max(
        2,
        max(len(token) for slots in data.values() for token in slots),
    )
    lines = [
        "格位 | " + " ".join(f"{slot + 1:>{token_width}}" for slot in range(slots_per_bar)),
        "-----+-" + "-" * ((token_width + 1) * slots_per_bar - 1),
    ]

    for bar in range(min_bar, max_bar + 1):
        score = " ".join(f"{token:>{token_width}}" for token in data[bar])
        lines.append(f"{bar + 1:03d} | {score} |")

    return lines


CHORD_TEMPLATES = [
    ("maj", (0, 4, 7)),
    ("min", (0, 3, 7)),
    ("dim", (0, 3, 6)),
]


def detect_chord(
    notes: List[Note],
    start: int,
    end: int,
) -> Optional[Tuple[int, str, float]]:
    weights = [0.0] * 12

    for n in notes:
        overlap = max(
            0,
            min(n.end, end) - max(n.start, start),
        )

        if overlap <= 0:
            continue

        weights[n.pitch % 12] += (
            overlap * (0.4 + n.velocity / 127.0)
        )

    total = sum(weights)

    if total <= 0:
        return None
    pitch_class_count = sum(1 for weight in weights if weight > 0)
    if pitch_class_count < 2:
        return None

    candidates = []

    for root in range(12):
        for quality, intervals in CHORD_TEMPLATES:
            pcs = {(root + x) % 12 for x in intervals}

            inside = sum(weights[p] for p in pcs)
            outside = total - inside

            fit = max(0.0, min(1.0,
                inside / total
                - 0.25 * outside / total
                + 0.05 * weights[root] / total
            ))
            if pitch_class_count == 2:
                fit = min(fit, 0.75)

            candidates.append((fit, root, quality))

    candidates.sort(key=lambda candidate: candidate[0], reverse=True)
    if not candidates or candidates[0][0] < 0.35:
        return None
    if len(candidates) > 1 and candidates[0][0] - candidates[1][0] < 0.06:
        return None

    best = candidates[0]
    return best[1], best[2], best[0]


PAD_MAP = {
    ("maj", 0): ("I",  "*", "1+3+5"),
    ("min", 2): ("ii", "/", "2+4+6"),
    ("maj", 5): ("IV", "-", "4+6+8"),
    ("maj", 7): ("V",  "+", "5+7+9"),
    ("min", 9): ("vi", "^", "6+8+/"),
}


def chord_description(
    root: int,
    quality: str,
    calc_root_pc: int,
) -> str:
    rel = (root - calc_root_pc) % 12
    mapped = PAD_MAP.get((quality, rel))

    chord_name = NOTE_NAMES[root]

    if quality == "min":
        chord_name += "m"
    elif quality == "dim":
        chord_name += "dim"

    if mapped:
        roman, pad, calc_stack = mapped

        return (
            f"{chord_name:<5} "
            f"{roman:<3} "
            f"CHORD:{pad:<2} "
            f"CALC:[{calc_stack}]"
        )

    return f"{chord_name:<5} (没有对应的一键 CHORD 垫)"


def render_chords(
    notes: List[Note],
    ticks_per_beat: int,
    calc_root_pc: int,
    beats_per_window: float = 2.0,
) -> List[str]:
    if not notes:
        return []

    ordered = sorted(notes, key=lambda n: (n.start, n.pitch))
    last_tick = max(n.end for n in ordered)
    window = max(1, int(ticks_per_beat * beats_per_window))

    entries = []
    active = []
    cursor = 0
    pos = 0

    while pos < last_tick:
        end = pos + window
        while cursor < len(ordered) and ordered[cursor].start < end:
            active.append(ordered[cursor])
            cursor += 1
        active = [n for n in active if n.end > pos]
        chord = detect_chord(active, pos, end)

        if chord:
            root, quality, fit = chord

            desc = chord_description(
                root,
                quality,
                calc_root_pc,
            )

            entries.append((desc, fit))
        else:
            entries.append(("N.C.", None))

        pos = end

    result = ["片段    | 建议"]
    group_start = 1
    for current in range(1, len(entries) + 1):
        at_end = current == len(entries)
        if not at_end and entries[current][0] == entries[group_start - 1][0]:
            continue

        group_end = current
        label = f"{group_start:03d}" if group_start == group_end else f"{group_start:03d}-{group_end:03d}"
        description = entries[group_start - 1][0]
        confidences = [
            confidence
            for _, confidence in entries[group_start - 1:group_end]
            if confidence is not None
        ]
        if confidences:
            average = sum(confidences) / len(confidences)
            result.append(f"{label:<7} | {description} | 平均匹配={average:.0%}")
        else:
            result.append(f"{label:<7} | N.C.")
        group_start = current + 1

    return result


def setting_instructions(root_pc: int, octave_shift: int) -> str:
    steps = []

    if octave_shift < 0:
        count = -octave_shift
        steps.append(f"Shift+4 × {count}" if count > 1 else "Shift+4")
    elif octave_shift > 0:
        steps.append(
            f"Shift+6 × {octave_shift}" if octave_shift > 1 else "Shift+6"
        )

    if 0 < root_pc <= 6:
        steps.append(f"Shift+* × {root_pc}" if root_pc > 1 else "Shift+*")
    elif root_pc > 6:
        count = 12 - root_pc
        steps.append(f"Shift+/ × {count}" if count > 1 else "Shift+/")

    return " → ".join(steps) if steps else "无需调整（默认 C、音区 4）"


def build_report(
    mid: mido.MidiFile,
    tracks,
    melody_info,
    actions,
    grid: int,
    chord_beats: float,
    full_track: bool,
    include_chords: bool = False,
) -> str:
    bpm, numerator, denominator = midi_info(mid)
    melody_track, melody_name, track_notes = melody_info
    step = mid.ticks_per_beat * 4 / grid

    if full_track:
        voice_notes, pitch_floor = track_notes, None
    else:
        voice_notes, pitch_floor = isolate_melody_voice(track_notes)
    melody_notes = reduce_to_melody(voice_notes, step)
    if not melody_notes:
        raise ValueError("旋律提取后没有可用音符")

    best = choose_setting(melody_notes, actions)
    root_pc = best["root_pc"]
    octave_shift = best["octave_shift"]
    base = best["base"]
    stats = setting_stats(melody_notes, base, actions)
    total = len(melody_notes)
    rpn_label = actions[18].key

    source_path = getattr(mid, "filename", None)
    source_name = Path(source_path).name if source_path else "MIDI"
    out = [
        f"SCI-CALC MIDI 转谱器 v{TOOL_VERSION}",
        "",
        "=== 1. MIDI 概览 ===",
        f"文件：{source_name}",
        " | ".join([
            fit_display("编号", 4, "right"),
            fit_display("轨道名称", 30),
            fit_display("音符数", 6, "right"),
            fit_display("末尾补全", 8, "right"),
            fit_display("旋律评分", 8, "right"),
        ]),
        "-+-".join(["-" * 4, "-" * 30, "-" * 6, "-" * 8, "-" * 8]),
    ]

    for i, name, notes in tracks:
        score = melody_track_score(name, notes)
        reason = non_melody_reason(name, notes)
        repaired = sum(1 for note in notes if note.repaired)
        out.append(" | ".join([
            fit_display(i, 4, "right"),
            fit_display(name, 30),
            fit_display(len(notes), 6, "right"),
            fit_display(repaired, 8, "right"),
            fit_display("排除" if reason else f"{score:.1f}", 8, "right"),
        ]))
    if any(non_melody_reason(name, notes) for _, name, notes in tracks):
        out.append("排除：鼓点、音效、音符过少或音高几乎不变的轨道不参与自动选轨")
    if any(note.repaired for _, _, notes in tracks for note in notes):
        out.append("末尾补全：保留了缺少 Note Off 的结尾音符，避免残缺 MIDI 漏音")

    out.extend([
        "",
        "=== 2. 先设置音乐键盘 ===",
        "按键步骤以程序刚启动时的默认 C、音区 4、移调 0 为起点",
        f"旋律来源：轨道 {melody_track}（{melody_name}）",
    ])
    if pitch_floor is not None:
        removed = len(track_notes) - len(voice_notes)
        out.append(
            f"自动分离：检测到同轨低音伴奏，忽略 {note_name(pitch_floor)} 以下的 "
            f"{removed} 个伴奏音"
        )
    elif full_track:
        out.append("自动分离：已由 --full-track 关闭")
    else:
        out.append("自动分离：未发现明确分开的低音声部")
    out.extend([
        f"谱面音符：{total} 个量化旋律音",
        f"根音：{NOTE_NAMES[root_pc]}",
        f"音区：{DEFAULT_OCTAVE + octave_shift}",
        "移调：0（保持默认）",
        f"设置按键：{setting_instructions(root_pc, octave_shift)}",
        f"歌曲速度：{bpm:.1f} BPM",
        f"拍号：{numerator}/{denominator}",
        f"此时数字 1 的音高：{note_name(base)}",
        "",
        "按键覆盖：",
        f"  单键：{stats['single']:.1f}%（{stats['single_count']}/{total}）",
        f"    数字区：{stats['numeric_single_count']}，扩展实体键：{stats['extended_single_count']}",
        f"  ↑组合：{stats['upper']:.1f}%（{stats['upper_count']}/{total}）",
        f"  无法覆盖：{stats['missing']:.1f}%（{stats['missing_count']}/{total}）",
        "",
        "=== 3. 按键图例 ===",
        ".、0 = 低音 6、低音 7",
        "1..7 = 本组音阶；8、9 = 高音 1、2",
        "/、*、-、+、^ = 高音 3、4、5、6、7",
        "SIN、SEC、COS、CSC、TAN = 升 1、升 2、升 4、升 5、升 6",
        f"COT、EXP、{rpn_label} = 高音升 1、高音升 2、高音升 4",
        f"{rpn_label} 是谱面缩写，对应 EXP 右侧印有 RPN/NORM 的实体键",
        "↑x = 按住 ENTER 再按 x；↑不是音名，也不用先切换全局音区",
        "· = 空拍    — = 继续保持前一个音    ?音名 = 当前设置无法覆盖",
        "小数点键写作 .（靠下）；空拍写作 ·（居中），两者不要混淆",
        f"每行一小节，按 1/{grid} 音符划分为固定格位",
        "",
        "=== 4. CALC 旋律谱 ===",
    ])

    out.extend(
        render_melody(
            melody_notes,
            base,
            actions,
            mid.ticks_per_beat,
            numerator,
            denominator,
            grid,
        )
    )

    if include_chords:
        accomp = [
            note
            for _, name, notes in tracks
            if non_melody_reason(name, notes) is None
            for note in notes
        ]
        out.extend([
            "",
            "=== 5. 可选和弦建议 ===",
            "CHORD:x = 切换到 CHORD 布局后，按 x 弹整个和弦",
            "CALC:[a+b+c] = 在 CALC 布局同时按下列三个键",
            "N.C. = 这一段不强行推荐和弦；匹配百分比越高越可靠",
            f"每个片段约 {chord_beats:g} 拍，连续相同建议会自动合并",
            "",
        ])
        out.extend(
            render_chords(
                accomp,
                mid.ticks_per_beat,
                root_pc,
                chord_beats,
            )
        )
    else:
        out.extend([
            "",
            "提示：和弦建议默认省略；需要时在命令末尾添加 --chords。",
        ])

    return "\n".join(out)


def main():
    parser = argparse.ArgumentParser(
        description="MIDI -> SCI-CALC 音乐键盘谱（面向无乐理基础使用者）",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )

    parser.add_argument(
        "midi",
        help="输入 MIDI 文件",
    )

    parser.add_argument(
        "--version",
        action="version",
        version=f"%(prog)s {TOOL_VERSION}",
    )

    parser.add_argument(
        "--track",
        type=int,
        default=None,
        help="指定旋律轨编号；默认自动判断",
    )

    parser.add_argument(
        "--grid",
        type=int,
        choices=(8, 16),
        default=8,
        help="节奏网格：8=八分音符，16=十六分音符",
    )

    parser.add_argument(
        "--chord-beats",
        type=float,
        default=2.0,
        help="启用 --chords 后，每多少拍检测一次和弦",
    )

    parser.add_argument(
        "--chords",
        action="store_true",
        help="输出完整和弦建议；长曲会明显增加输出长度",
    )

    parser.add_argument(
        "--rpn-key-label",
        "--extra-key",
        dest="rpn_key_label",
        default=RPN_KEY_NAME,
        help="谱面中 RPN/NORM 实体键的缩写；--extra-key 作为旧参数名保留",
    )

    parser.add_argument(
        "--full-track",
        action="store_true",
        help="关闭同轨低音伴奏自动分离；量化时仍会让每格只保留一个主音",
    )

    parser.add_argument(
        "--out",
        default=None,
        help="保存完整报告到文本文件，例如 song_calc.txt",
    )

    parser.add_argument(
        "--quiet",
        action="store_true",
        help="与 --out 同用：终端只显示保存位置，不打印完整长谱",
    )

    args = parser.parse_args()

    if mido is None:
        parser.error("缺少 mido；请先运行 python -m pip install mido")
    if args.chord_beats <= 0:
        parser.error("--chord-beats 必须大于 0")
    if args.quiet and not args.out:
        parser.error("--quiet 必须与 --out 一起使用")

    args.rpn_key_label = args.rpn_key_label.strip()
    if (
        not args.rpn_key_label
        or any(char.isspace() for char in args.rpn_key_label)
        or len(args.rpn_key_label) > 12
    ):
        parser.error("--rpn-key-label 必须是 1 至 12 个不含空格的字符")

    try:
        mid = mido.MidiFile(args.midi)
    except (OSError, EOFError, ValueError, KeyError) as exc:
        raise SystemExit(f"无法读取 MIDI：{exc}") from exc
    actions = build_actions(args.rpn_key_label)

    tracks = []

    for i, track in enumerate(mid.tracks):
        name, notes = parse_track(track, i)

        if notes:
            tracks.append((i, name, notes))

    if not tracks:
        raise SystemExit("没有找到可用音符。")

    if args.track is None:
        candidates = [
            item
            for item in tracks
            if non_melody_reason(item[1], item[2]) is None
        ]
        if not candidates:
            raise SystemExit(
                "没有找到可靠的旋律轨；请从概览中选择轨道并使用 --track N。"
            )
        melody_info = max(
            candidates,
            key=lambda x: melody_track_score(
                x[1],
                x[2],
            ),
        )
    else:
        candidates = [
            x for x in tracks
            if x[0] == args.track
        ]

        if not candidates:
            available = ", ".join(str(x[0]) for x in tracks)
            raise SystemExit(
                f"找不到 track {args.track}。可用轨道：{available}"
            )

        melody_info = candidates[0]

    try:
        report = build_report(
            mid=mid,
            tracks=tracks,
            melody_info=melody_info,
            actions=actions,
            grid=args.grid,
            chord_beats=args.chord_beats,
            full_track=args.full_track,
            include_chords=args.chords,
        )
    except ValueError as exc:
        raise SystemExit(f"无法生成谱面：{exc}") from exc

    if not args.quiet:
        print(report)

    if args.out:
        try:
            Path(args.out).write_text(report + "\n", encoding="utf-8")
        except OSError as exc:
            raise SystemExit(f"无法保存 {args.out}：{exc}") from exc

        print(f"\n已保存：{args.out}")


if __name__ == "__main__":
    main()
