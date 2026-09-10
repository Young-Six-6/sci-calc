import importlib.util
import struct
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "tools" / "midi_to_tiles.py"
SPEC = importlib.util.spec_from_file_location("midi_to_tiles", MODULE_PATH)
midi_to_tiles = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = midi_to_tiles
SPEC.loader.exec_module(midi_to_tiles)


def vlq(value):
    result = [value & 0x7F]
    value >>= 7
    while value:
        result.append((value & 0x7F) | 0x80)
        value >>= 7
    return bytes(reversed(result))


def midi_file(track, division=480):
    header = b"MThd" + struct.pack(">IHHH", 6, 0, 1, division)
    return header + b"MTrk" + struct.pack(">I", len(track)) + track


class MidiConverterTests(unittest.TestCase):
    def test_format_zero_running_status_and_chord_conversion(self):
        track = b"".join(
            [
                vlq(0) + b"\xff\x51\x03\x07\xa1\x20",
                vlq(0) + bytes([0x90, 60, 100]),
                vlq(0) + bytes([64, 90]),
                vlq(480) + bytes([0x80, 60, 0]),
                vlq(0) + bytes([64, 0]),
                vlq(0) + b"\xff\x2f\x00",
            ]
        )
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "chord.mid"
            output = Path(directory) / "chord.ptc"
            source.write_bytes(midi_file(track))
            count, thinned = midi_to_tiles.convert(
                source, output, lanes=4, difficulty="normal", title="Test=Chord", artist="Unit, Test"
            )
            text = output.read_text(encoding="utf-8")

        self.assertEqual(count, 2)
        self.assertFalse(thinned)
        self.assertIn("title=Test Chord", text)
        self.assertIn("artist=Unit Test", text)
        self.assertIn("lanes=4", text)
        notes = [line for line in text.splitlines() if line.startswith("N,")]
        self.assertEqual(len(notes), 2)
        self.assertTrue(all(line.startswith("N,1000,") for line in notes))
        self.assertEqual({int(line.split(",")[2]) for line in notes}, {0, 3})
        self.assertTrue(all(line.endswith(",500") for line in notes))

    def test_tempo_map_accumulates_changes_exactly(self):
        tempo = midi_to_tiles.TempoMap(480, [(0, 500_000, 0), (480, 1_000_000, 1)])
        self.assertEqual(tempo.milliseconds(0), 0)
        self.assertEqual(tempo.milliseconds(480), 500)
        self.assertEqual(tempo.milliseconds(960), 1500)
        self.assertEqual(tempo.initial_bpm, 120)

    def test_easy_mode_reduces_dense_onsets(self):
        notes = [
            midi_to_tiles.TimedNote(index * 60, index * 60 + 300, 60 + index % 8, 100)
            for index in range(20)
        ]
        tiles = midi_to_tiles.select_tiles(notes, lanes=4, difficulty="easy")
        self.assertLess(len(tiles), len(notes))
        self.assertTrue(all(tile.duration_ms == 0 for tile in tiles))
        self.assertTrue(all(0 <= tile.lane < 4 for tile in tiles))

    def test_oversized_chart_is_thinned_across_full_duration(self):
        tiles = [midi_to_tiles.Tile(index * 100, index % 4, 60, 0) for index in range(1400)]
        limited, thinned = midi_to_tiles.limit_tiles(tiles)
        self.assertTrue(thinned)
        self.assertEqual(len(limited), midi_to_tiles.MAX_EVENTS)
        self.assertEqual(limited[0].time_ms, 0)
        self.assertEqual(limited[-1].time_ms, tiles[-1].time_ms)

    def test_rejects_smpte_time_division(self):
        track = vlq(0) + b"\xff\x2f\x00"
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "bad.mid"
            source.write_bytes(midi_file(track, division=0xE728))
            with self.assertRaises(midi_to_tiles.MidiError):
                midi_to_tiles.read_midi(source)


if __name__ == "__main__":
    unittest.main()
