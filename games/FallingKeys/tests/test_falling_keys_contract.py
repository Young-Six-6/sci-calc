import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
CHART_HEADER = (ROOT / "src" / "Chart.h").read_text(encoding="utf-8")
CHART_SOURCE = (ROOT / "src" / "Chart.cpp").read_text(encoding="utf-8")
SYNTH_HEADER = (ROOT / "src" / "TileSynth.h").read_text(encoding="utf-8")
SYNTH_SOURCE = (ROOT / "src" / "TileSynth.cpp").read_text(encoding="utf-8")
KEYBOARD_SOURCE = (ROOT / "src" / "SafeKeyboard.cpp").read_text(encoding="utf-8")


class FallingKeysContractTests(unittest.TestCase):
    def test_chart_memory_and_lane_limits_are_fixed(self):
        self.assertIn("MAX_EVENTS = 1024", CHART_HEADER)
        self.assertIn("MAX_LANES = 6", CHART_HEADER)
        self.assertIn("TileEvent events_[MAX_EVENTS]", CHART_HEADER)
        self.assertNotIn("std::vector", CHART_HEADER + CHART_SOURCE + MAIN)

    def test_audio_uses_wired_pcm5102a_on_core_zero(self):
        self.assertIn("I2S_DOUT = 15", SYNTH_HEADER)
        self.assertIn("I2S_LRCK = 22", SYNTH_HEADER)
        self.assertIn("I2S_BCLK = 21", SYNTH_HEADER)
        self.assertRegex(SYNTH_SOURCE, r'xTaskCreatePinnedToCore\([^;]+,\s*0\)')
        self.assertIn("MAX_VOICES = 8", SYNTH_HEADER)

    def test_keyboard_maps_the_existing_six_physical_columns(self):
        self.assertIn("COL_PINS[6] = {13, 12, 14, 27, 26, 25}", KEYBOARD_SOURCE)
        self.assertIn("index % COLS", KEYBOARD_SOURCE)
        self.assertIn("event.key == KeyboardKey::Shift", MAIN)
        self.assertIn("event.key == KeyboardKey::Escape", MAIN)

    def test_game_has_four_and_six_lane_modes_and_three_difficulties(self):
        self.assertIn("selectedLanes == 4 ? 6 : 4", MAIN)
        for name in ("Easy", "Normal", "Hard"):
            self.assertIn(f'"{name}"', MAIN)
        for screen in ("Countdown", "Playing", "Paused", "Results"):
            self.assertIn(screen, MAIN)

    def test_sd_contract_and_safe_return_path_are_present(self):
        self.assertIn('CHART_DIRECTORY[] = "/fallingkeys"', MAIN)
        self.assertIn('MAIN_FIRMWARE[] = "/main.bin"', MAIN)
        self.assertIn("Chart::probe", MAIN)
        self.assertIn("Update.end(true)", MAIN)
        self.assertIn("heldMs(KeyboardKey::Escape) >= EXIT_HOLD_MS", MAIN)


if __name__ == "__main__":
    unittest.main()
