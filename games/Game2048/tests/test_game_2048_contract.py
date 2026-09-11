import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ENGINE_HEADER = (ROOT / "src" / "Game2048.h").read_text(encoding="utf-8")
ENGINE_SOURCE = (ROOT / "src" / "Game2048.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
KEYBOARD = (ROOT / "src" / "SafeKeyboard.cpp").read_text(encoding="utf-8")


class Game2048ContractTests(unittest.TestCase):
    def test_hardware_pins_match_the_handheld(self):
        self.assertIn("ROW_PINS[5] = {33, 32, 35, 34, 39}", KEYBOARD)
        self.assertIn("COL_PINS[6] = {13, 12, 14, 27, 26, 25}", KEYBOARD)
        for declaration in (
            "constexpr uint8_t OLED_CS = 5",
            "constexpr uint8_t OLED_DC = 16",
            "constexpr uint8_t OLED_RST = 17",
            "constexpr uint8_t SD_CS = 4",
        ):
            self.assertIn(declaration, MAIN)
        self.assertIn("SPI.begin(18, 19, 23)", MAIN)

    def test_game_memory_is_fixed_for_the_no_psram_board(self):
        self.assertIn("CELL_COUNT = BOARD_SIDE * BOARD_SIDE", ENGINE_HEADER)
        self.assertIn("uint32_t board_[CELL_COUNT]", ENGINE_HEADER)
        self.assertIn("TileTrace traces_[MAX_TRACES]", ENGINE_HEADER)
        combined = ENGINE_HEADER + ENGINE_SOURCE + MAIN
        self.assertNotIn("std::vector", combined)
        self.assertNotIn("ps_malloc", combined)
        self.assertNotRegex(combined, r"new\s+(?:TileTrace|uint32_t)")

    def test_merge_and_spawn_rules_are_guarded(self):
        self.assertIn("sourceOffset += 2", ENGINE_SOURCE)
        self.assertIn("values[sourceOffset] == values[sourceOffset + 1]", ENGINE_SOURCE)
        self.assertIn("std::memcmp(next, board_, sizeof(board_)) == 0", ENGINE_SOURCE)
        no_move = ENGINE_SOURCE.split(
            "if (std::memcmp(next, board_, sizeof(board_)) == 0)", 1
        )[1].split("std::memcpy(undoBoard_", 1)[0]
        self.assertNotIn("addRandomTile", no_move)
        self.assertRegex(ENGINE_SOURCE, r"valueRandom % 10 == 0 \? 4 : 2")
        self.assertIn("fourPairs", ENGINE_SOURCE)
        self.assertIn("noChain", ENGINE_SOURCE)
        self.assertIn("checkerboard", ENGINE_SOURCE)
        self.assertIn("verticalWin", ENGINE_SOURCE)

    def test_physical_controls_cover_play_pause_and_recovery(self):
        expected_directions = {
            "K8": "Up",
            "K2": "Down",
            "K4": "Left",
            "K6": "Right",
        }
        for key, direction in expected_directions.items():
            self.assertRegex(
                MAIN,
                rf"KeyboardKey::{key}\) direction = MoveDirection::{direction}",
            )
        self.assertIn("KeyboardKey::Backspace || event.key == KeyboardKey::Extra", MAIN)
        self.assertIn("event.key == KeyboardKey::Enter && keyboard.isDown(KeyboardKey::Shift)", MAIN)
        for state in ("Paused", "ConfirmNew", "Won", "GameOver"):
            self.assertIn(f"Screen::{state}", MAIN)

    def test_animation_buffers_one_move_without_changing_the_rules(self):
        self.assertIn("constexpr uint32_t SLIDE_ANIMATION_MS = 105", MAIN)
        self.assertIn("const TileTrace &trace = game.trace(index)", MAIN)
        self.assertIn("queuedMove = direction", MAIN)
        self.assertIn("queuedMoveValid = true", MAIN)
        self.assertIn("if (screen == Screen::Playing && queuedMoveValid)", MAIN)
        self.assertIn("ANIMATION", MAIN)

    def test_extreme_values_are_shortened_to_fit_the_small_display(self):
        self.assertIn("void formatTileValue", MAIN)
        self.assertIn('std::snprintf(text, size, "2^%u", exponent)', MAIN)
        self.assertIn("void formatCompactValue", MAIN)
        self.assertIn('std::snprintf(status, sizeof(status), "M%s T%s"', MAIN)

    def test_save_data_is_checked_and_flash_writes_are_batched(self):
        self.assertIn('preferences.begin("game2048"', MAIN)
        self.assertIn("offsetof(SaveData, checksum)", MAIN)
        self.assertIn("state.magic != SAVE_MAGIC", MAIN)
        self.assertIn("state.version != SAVE_VERSION", MAIN)
        self.assertIn("constexpr uint32_t SAVE_IDLE_MS = 1500", MAIN)
        self.assertIn("constexpr uint8_t FORCE_SAVE_MOVES = 20", MAIN)
        self.assertIn("saveSession();", MAIN)

    def test_safe_return_to_main_firmware_is_available(self):
        self.assertIn('MAIN_FIRMWARE[] = "/main.bin"', MAIN)
        self.assertIn("SD.open(MAIN_FIRMWARE, FILE_READ)", MAIN)
        self.assertIn("Update.end(true)", MAIN)
        self.assertIn("heldMs(KeyboardKey::Escape) >= EXIT_HOLD_MS", MAIN)


if __name__ == "__main__":
    unittest.main()
