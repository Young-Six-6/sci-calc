import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ENGINE_HEADER = (ROOT / "src" / "ClassicEngine.h").read_text(encoding="utf-8")
ENGINE_SOURCE = (ROOT / "src" / "ClassicEngine.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
KEYBOARD = (ROOT / "src" / "SafeKeyboard.cpp").read_text(encoding="utf-8")
SEGMENTS = (ROOT / "src" / "SevenSegment.cpp").read_text(encoding="utf-8")


class ClassicCalculatorContractTests(unittest.TestCase):
    def test_hardware_pins_match_the_handheld(self):
        self.assertIn("ROW_PINS[5] = {33, 32, 35, 34, 39}", KEYBOARD)
        self.assertIn("COL_PINS[6] = {13, 12, 14, 27, 26, 25}", KEYBOARD)
        self.assertIn("constexpr uint8_t OLED_CS = 5", MAIN)
        self.assertIn("constexpr uint8_t SD_CS = 4", MAIN)

    def test_core_memory_is_bounded_for_the_no_psram_board(self):
        self.assertIn("MAX_ENTRY_DIGITS = 15", ENGINE_HEADER)
        self.assertIn("MAX_TAPE_RECORDS = 24", ENGINE_HEADER)
        self.assertIn("TapeRecord tape_[MAX_TAPE_RECORDS]", ENGINE_HEADER)
        combined = ENGINE_HEADER + ENGINE_SOURCE + MAIN
        self.assertNotIn("std::vector", combined)
        self.assertNotIn("new TapeRecord", combined)
        self.assertNotIn("ps_malloc", combined)

    def test_classic_chain_repeat_equals_and_business_percent_are_present(self):
        self.assertIn("if (pending_ != CalcOperation::None && entering_)", ENGINE_SOURCE)
        self.assertIn("repeatOperand_ = right", ENGINE_SOURCE)
        self.assertIn("repeat_ != CalcOperation::None && resultShown_", ENGINE_SOURCE)
        self.assertRegex(
            ENGINE_SOURCE,
            r"pending_ == CalcOperation::Add \|\| pending_ == CalcOperation::Subtract[\s\S]*?left \* enteredPercent / 100\.0",
        )
        self.assertIn("enteredPercent / 100.0", ENGINE_SOURCE)

    def test_accounting_controls_are_reachable_from_the_physical_keys(self):
        expected = {
            "Sin": "memoryAdd",
            "Cos": "memorySubtract",
            "Tan": "memoryRecall",
            "Sec": "memoryClear",
            "Csc": "grandTotalRecall",
            "Cot": "clearEntry",
            "Power": "percent",
            "Extra": "undo",
        }
        for key, action in expected.items():
            self.assertRegex(MAIN, rf"KeyboardKey::{key}[\s\S]{{0,220}}calculator\.{action}\(")
        self.assertIn("calculator.applyTax(true)", MAIN)
        self.assertIn("calculator.applyTax(false)", MAIN)
        self.assertIn("enterSettings();", MAIN)

    def test_large_display_uses_a_complete_seven_segment_renderer(self):
        for segment in "ABCDEFG":
            self.assertIn(f"SEG_{segment}", SEGMENTS)
        for digit in range(10):
            self.assertIn(f"case '{digit}'", SEGMENTS)
        self.assertIn("while (height > 17", SEGMENTS)
        self.assertIn("SevenSegment::drawRightAligned", MAIN)
        self.assertIn("250, 35", MAIN)

    def test_tape_settings_and_safe_return_are_available(self):
        self.assertIn("recallTape(selectedTape)", MAIN)
        self.assertIn("calculator.clearTape()", MAIN)
        self.assertIn("DecimalMode::Fixed2", MAIN)
        self.assertIn("RoundingMode::AwayFromZero", MAIN)
        self.assertIn("SD.open(MAIN_FIRMWARE, FILE_READ)", MAIN)
        self.assertIn("Update.end(true)", MAIN)
        self.assertIn("heldMs(KeyboardKey::Escape) >= EXIT_HOLD_MS", MAIN)


if __name__ == "__main__":
    unittest.main()
