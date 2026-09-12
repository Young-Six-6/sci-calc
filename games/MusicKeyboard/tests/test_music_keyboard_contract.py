import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
SYNTH_HEADER = (ROOT / "src" / "LiveSynth.h").read_text(encoding="utf-8")
SYNTH_SOURCE = (ROOT / "src" / "LiveSynth.cpp").read_text(encoding="utf-8")
RECORDER_HEADER = (ROOT / "src" / "MidiRecorder.h").read_text(encoding="utf-8")
RECORDER_SOURCE = (ROOT / "src" / "MidiRecorder.cpp").read_text(encoding="utf-8")
KEYBOARD_SOURCE = (ROOT / "src" / "SafeKeyboard.cpp").read_text(encoding="utf-8")


class MusicKeyboardContractTests(unittest.TestCase):
    def test_hardware_pins_match_the_calculator(self):
        self.assertIn("ROW_PINS[5] = {33, 32, 35, 34, 39}", KEYBOARD_SOURCE)
        self.assertIn("COL_PINS[6] = {13, 12, 14, 27, 26, 25}", KEYBOARD_SOURCE)
        self.assertIn("I2S_DOUT = 15", SYNTH_HEADER)
        self.assertIn("I2S_LRCK = 22", SYNTH_HEADER)
        self.assertIn("I2S_BCLK = 21", SYNTH_HEADER)
        self.assertIn("constexpr uint8_t OLED_CS = 5", MAIN)
        self.assertIn("constexpr uint8_t SD_CS = 4", MAIN)

    def test_audio_path_is_bounded_and_runs_on_the_other_core(self):
        self.assertRegex(SYNTH_HEADER, r"MAX_VOICES\s*=\s*16")
        self.assertRegex(SYNTH_HEADER, r"AUDIO_BLOCK_FRAMES\s*=\s*128")
        self.assertRegex(SYNTH_SOURCE, r"xTaskCreatePinnedToCore\([\s\S]*?audioTaskEntry[\s\S]*?,\s*0\s*\)")
        self.assertNotIn("ps_malloc", SYNTH_SOURCE)
        self.assertNotIn("new Voice", SYNTH_SOURCE)

    def test_recording_uses_a_fixed_event_buffer_and_standard_midi(self):
        self.assertRegex(RECORDER_HEADER, r"MAX_RECORDED_EVENTS\s*=\s*2048")
        self.assertIn("Event events_[MAX_RECORDED_EVENTS]", RECORDER_HEADER)
        self.assertIn("'M', 'T', 'h', 'd'", RECORDER_SOURCE)
        self.assertIn("'M', 'T', 'r', 'k'", RECORDER_SOURCE)
        self.assertIn('"/recordings/KEYS%03u.MID"', RECORDER_SOURCE)
        self.assertIn("0xFF, 0x2F, 0", RECORDER_SOURCE)

    def test_calculator_keys_cover_notes_chords_and_shift_controls(self):
        for key in ("K0", "K1", "K2", "K3", "K4", "K5", "K6", "K7", "K8", "K9"):
            self.assertIn(f"KeyboardKey::{key}", MAIN)
        for key in ("Divide", "Multiply", "Minus", "Plus", "Power"):
            chord_case = re.search(
                rf"case KeyboardKey::{key}:[\s\S]*?appendMappedNote\(binding,[\s\S]*?break;",
                MAIN,
            )
            self.assertIsNotNone(chord_case, key)
        self.assertIn("toggleRecording();", MAIN)
        self.assertIn("togglePlayback();", MAIN)
        self.assertIn("Layout layout = Layout::Calculator", MAIN)
        self.assertIn("case KeyboardKey::Dot: offset = -3", MAIN)
        self.assertIn("case KeyboardKey::K0: offset = -1", MAIN)
        self.assertIn("static_cast<uint8_t>(layout) + 1) % 3", MAIN)

    def test_calculator_layout_has_direct_and_momentary_upper_notes(self):
        for key, offset in (
            ("Divide", 16),
            ("Multiply", 17),
            ("Minus", 19),
            ("Plus", 21),
            ("Power", 23),
        ):
            self.assertRegex(
                MAIN,
                rf"case KeyboardKey::{key}:[\s\S]{{0,100}}layout == Layout::Calculator\) offset = {offset}",
            )
        self.assertIn("const int16_t base = keyboardBaseNote() + octaveLift", MAIN)
        self.assertIn("keyboard.isDown(KeyboardKey::Enter) ? 12 : 0", MAIN)
        self.assertIn('layoutName, upper', MAIN)

    def test_escape_restores_the_main_firmware(self):
        self.assertIn('SD.open("/main.bin", FILE_READ)', MAIN)
        self.assertIn("Update.begin(size, U_FLASH)", MAIN)
        self.assertIn("keyboard.heldMs(KeyboardKey::Escape) >= EXIT_HOLD_MS", MAIN)


if __name__ == "__main__":
    unittest.main()
