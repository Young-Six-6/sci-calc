import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
KEYBOARD_HEADER = (ROOT / "src" / "KeyboardMatrix.h").read_text(encoding="utf-8")
KEYBOARD_SOURCE = (ROOT / "src" / "KeyboardMatrix.cpp").read_text(encoding="utf-8")
PROTOCOL_HEADER = (ROOT / "src" / "SerialKeyProtocol.h").read_text(encoding="utf-8")
PROTOCOL_SOURCE = (ROOT / "src" / "SerialKeyProtocol.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
PC_CLIENT = (ROOT / "pc" / "serial_keyboard.py").read_text(encoding="utf-8")


class FirmwareContractTests(unittest.TestCase):
    def test_hardware_pins_match_the_handheld(self):
        self.assertIn("ROW_PINS[5] = {33, 32, 35, 34, 39}", KEYBOARD_SOURCE)
        self.assertIn("COL_PINS[6] = {13, 12, 14, 27, 26, 25}", KEYBOARD_SOURCE)
        for declaration in (
            "constexpr uint8_t OLED_CS = 5",
            "constexpr uint8_t OLED_DC = 16",
            "constexpr uint8_t OLED_RST = 17",
            "constexpr uint8_t SD_CS = 4",
        ):
            self.assertIn(declaration, MAIN)
        self.assertIn("SPI.begin(18, 19, 23)", MAIN)

    def test_all_physical_keys_have_stable_row_major_ids(self):
        names_block = KEYBOARD_SOURCE.split("const char *KEY_NAMES", 1)[1].split("};", 1)[0]
        firmware_names = re.findall(r'"([^"]+)"', names_block)
        client_block = PC_CLIENT.split("KEY_NAMES = (", 1)[1].split(")", 1)[0]
        client_names = re.findall(r'"([^"]+)"', client_block)
        self.assertEqual(len(firmware_names), 30)
        self.assertEqual(firmware_names, client_names)
        self.assertIn("return id == 0xFF ? 0xFF : id / COLS", KEYBOARD_SOURCE)
        self.assertIn("return id == 0xFF ? 0xFF : id % COLS", KEYBOARD_SOURCE)

    def test_stream_is_transmit_only_and_never_exposes_hid(self):
        combined = KEYBOARD_HEADER + KEYBOARD_SOURCE + PROTOCOL_HEADER + PROTOCOL_SOURCE + MAIN
        self.assertIn("Serial.write", PROTOCOL_SOURCE)
        self.assertNotIn("Serial.read", combined)
        self.assertNotIn("Serial.available", combined)
        self.assertNotIn("BleKeyboard", combined)
        self.assertNotIn("Bluetooth", combined)
        self.assertNotIn("Keyboard.write", combined)
        self.assertNotIn("self.port.write", PC_CLIENT)
        for windows_input_api in ("import keyboard", "import pynput", "win32api", "SendInput"):
            self.assertNotIn(windows_input_api, PC_CLIENT)

    def test_press_release_debounce_and_queue_are_bounded(self):
        self.assertIn("DEBOUNCE_MS = 16", KEYBOARD_SOURCE)
        self.assertIn("push(key, KeyAction::Press", KEYBOARD_SOURCE)
        self.assertIn("push(key, KeyAction::Release", KEYBOARD_SOURCE)
        self.assertIn("QUEUE_SIZE = 64", KEYBOARD_HEADER)
        combined = KEYBOARD_HEADER + KEYBOARD_SOURCE
        self.assertNotIn("std::vector", combined)
        self.assertNotIn("ps_malloc", combined)

    def test_framing_is_versioned_and_crc_protected(self):
        self.assertIn("MAGIC[4] = {'S', 'K', 'B', '1'}", PROTOCOL_SOURCE)
        self.assertIn("PROTOCOL_VERSION = 1", PROTOCOL_HEADER)
        self.assertIn("0x1021U", PROTOCOL_SOURCE)
        self.assertIn("sendDeviceInfo", MAIN)
        self.assertIn("sendKeyEvent", MAIN)
        self.assertIn("sendState", MAIN)

    def test_safe_return_to_main_firmware_is_available(self):
        self.assertIn('MAIN_FIRMWARE[] = "/main.bin"', MAIN)
        self.assertIn("SD.open(MAIN_FIRMWARE, FILE_READ)", MAIN)
        self.assertIn("Update.end(true)", MAIN)
        self.assertIn("keyboard.heldMs(KeyCode::Escape) >= EXIT_HOLD_MS", MAIN)


if __name__ == "__main__":
    unittest.main()
