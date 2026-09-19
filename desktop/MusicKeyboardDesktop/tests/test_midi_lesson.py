import tempfile
import unittest
from pathlib import Path
import sys

import mido


APP_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(APP_ROOT / "bridge"))

from midi_lesson import build_lesson  # noqa: E402


class MidiLessonTests(unittest.TestCase):
    def make_midi(self, path):
        midi = mido.MidiFile(type=1, ticks_per_beat=480)

        melody = mido.MidiTrack()
        melody.append(mido.MetaMessage("track_name", name="Lead Melody", time=0))
        for pitch in (60, 62, 64, 65, 67, 69, 71, 72):
            melody.append(mido.Message("note_on", note=pitch, velocity=100, time=0))
            melody.append(mido.Message("note_off", note=pitch, velocity=0, time=240))
        midi.tracks.append(melody)

        chords = mido.MidiTrack()
        chords.append(mido.MetaMessage("track_name", name="Chords", time=0))
        for pitch in (64, 67, 72):
            chords.append(mido.Message("note_on", note=pitch, velocity=72, time=0))
        chords.append(mido.Message("note_off", note=64, velocity=0, time=1920))
        chords.append(mido.Message("note_off", note=67, velocity=0, time=0))
        chords.append(mido.Message("note_off", note=72, velocity=0, time=0))
        midi.tracks.append(chords)
        midi.save(path)

    def test_complexity_limits_playable_chord_size(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            midi_path = Path(temp_dir) / "lesson.mid"
            self.make_midi(midi_path)

            melody = build_lesson(midi_path, "melody", 16)
            simple = build_lesson(midi_path, "simple", 16)
            full = build_lesson(midi_path, "full", 16)

            self.assertEqual(melody["step_count"], 8)
            self.assertTrue(all(len(step["notes"]) == 1 for step in melody["steps"]))
            self.assertTrue(any(len(step["notes"]) == 2 for step in simple["steps"]))
            self.assertTrue(any(len(step["notes"]) >= 3 for step in full["steps"]))
            self.assertTrue(all(len(step["notes"]) <= 4 for step in full["steps"]))
            self.assertTrue(all(len(step["key_ids"]) >= len(step["notes"]) for step in full["steps"]))


if __name__ == "__main__":
    unittest.main()
