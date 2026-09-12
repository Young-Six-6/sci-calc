import importlib.util
import sys
import unittest
from pathlib import Path


try:
    import mido
except ModuleNotFoundError:
    mido = None


ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "tools" / "midi_to_calc.py"

if mido is not None:
    SPEC = importlib.util.spec_from_file_location("midi_to_calc", MODULE_PATH)
    midi_to_calc = importlib.util.module_from_spec(SPEC)
    assert SPEC.loader is not None
    sys.modules[SPEC.name] = midi_to_calc
    SPEC.loader.exec_module(midi_to_calc)
else:
    midi_to_calc = None


@unittest.skipIf(mido is None, "mido is not installed")
class MidiToCalcTests(unittest.TestCase):
    def make_notes(self, pitches, track=1):
        return [
            midi_to_calc.Note(index * 480, index * 480 + 240, pitch, 96, track)
            for index, pitch in enumerate(pitches)
        ]

    def make_midi(self):
        midi = mido.MidiFile(type=1, ticks_per_beat=480)

        conductor = mido.MidiTrack()
        conductor.append(mido.MetaMessage("set_tempo", tempo=500000, time=0))
        conductor.append(
            mido.MetaMessage(
                "time_signature",
                numerator=4,
                denominator=4,
                time=0,
            )
        )
        conductor.append(mido.MetaMessage("end_of_track", time=0))
        midi.tracks.append(conductor)

        vocal = mido.MidiTrack()
        vocal.append(mido.MetaMessage("track_name", name="vocal", time=0))
        for index, pitch in enumerate((69, 71, 73, 74, 76, 74, 73, 71)):
            vocal.append(
                mido.Message(
                    "note_on",
                    note=pitch,
                    velocity=96,
                    time=0 if index == 0 else 240,
                )
            )
            vocal.append(
                mido.Message("note_off", note=pitch, velocity=0, time=240)
            )
        vocal.append(mido.MetaMessage("end_of_track", time=0))
        midi.tracks.append(vocal)

        drum = mido.MidiTrack()
        drum.append(mido.MetaMessage("track_name", name="drum", time=0))
        for index in range(8):
            drum.append(
                mido.Message(
                    "note_on",
                    channel=1,
                    note=114,
                    velocity=100,
                    time=0 if index == 0 else 240,
                )
            )
            drum.append(
                mido.Message(
                    "note_off",
                    channel=1,
                    note=114,
                    velocity=0,
                    time=240,
                )
            )
        drum.append(mido.MetaMessage("end_of_track", time=0))
        midi.tracks.append(drum)

        return midi

    def parse_tracks(self, midi):
        tracks = []
        for index, track in enumerate(midi.tracks):
            name, notes = midi_to_calc.parse_track(track, index)
            if notes:
                tracks.append((index, name, notes))
        return tracks

    def test_rpn_key_and_enter_lift_use_readable_names(self):
        actions = midi_to_calc.build_actions("RPN")
        self.assertEqual(actions[18].key, "RPN")
        self.assertEqual(actions[18].kind, "single")
        self.assertEqual(actions[30].key, "↑RPN")
        self.assertEqual(actions[30].kind, "upper")

    def test_drum_and_effect_tracks_are_not_melody_candidates(self):
        vocal = self.make_notes((69, 71, 73, 74, 76, 74, 73, 71))
        drum = self.make_notes((114,) * 8, track=15)
        effect = self.make_notes((72, 76, 79, 84), track=8)

        self.assertIsNone(midi_to_calc.non_melody_reason("vocal", vocal))
        self.assertEqual(
            midi_to_calc.non_melody_reason("drum", drum),
            "鼓点或音效轨",
        )
        self.assertEqual(
            midi_to_calc.non_melody_reason("se", effect),
            "鼓点或音效轨",
        )
        self.assertGreater(
            midi_to_calc.melody_track_score("vocal", vocal),
            midi_to_calc.melody_track_score("drum", drum),
        )

    def test_dangling_note_is_kept_at_end_of_track(self):
        track = mido.MidiTrack(
            [
                mido.Message("note_on", note=60, velocity=90, time=0),
                mido.MetaMessage("end_of_track", time=480),
            ]
        )
        _, notes = midi_to_calc.parse_track(track, 1)

        self.assertEqual(len(notes), 1)
        self.assertTrue(notes[0].repaired)
        self.assertEqual(notes[0].end, 480)

    def test_report_selects_vocal_and_keeps_chords_opt_in(self):
        midi = self.make_midi()
        tracks = self.parse_tracks(midi)
        selected = max(
            tracks,
            key=lambda item: midi_to_calc.melody_track_score(item[1], item[2]),
        )
        actions = midi_to_calc.build_actions("RPN")

        self.assertEqual(selected[0], 1)
        report = midi_to_calc.build_report(
            midi,
            tracks,
            selected,
            actions,
            grid=8,
            chord_beats=2.0,
            full_track=False,
        )
        report_with_chords = midi_to_calc.build_report(
            midi,
            tracks,
            selected,
            actions,
            grid=8,
            chord_beats=2.0,
            full_track=False,
            include_chords=True,
        )

        self.assertIn("旋律来源：轨道 1（vocal）", report)
        self.assertIn("RPN/NORM", report)
        self.assertNotIn("E+EXTRA", report)
        self.assertNotIn("=== 5. 可选和弦建议 ===", report)
        self.assertIn("=== 5. 可选和弦建议 ===", report_with_chords)

    def test_largest_pitch_gap_can_separate_bass_and_melody(self):
        notes = []
        for index in range(8):
            start = index * 240
            notes.append(midi_to_calc.Note(start, start + 480, 40 + index % 3, 80, 1))
            notes.append(midi_to_calc.Note(start, start + 360, 64 + index % 5, 96, 1))

        melody, floor = midi_to_calc.isolate_melody_voice(notes)

        self.assertEqual(floor, 64)
        self.assertTrue(all(note.pitch >= 64 for note in melody))


if __name__ == "__main__":
    unittest.main()
