import hashlib
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
LIBRARY_SOURCE = (ROOT / "src" / "RomLibrary.cpp").read_text(encoding="utf-8")
LIBRARY_HEADER = (ROOT / "src" / "RomLibrary.h").read_text(encoding="utf-8")
CORE_SOURCE_PATH = ROOT / "src" / "vendor" / "octemu" / "core.c"
CORE_HEADER_PATH = ROOT / "src" / "vendor" / "octemu" / "core.h"
CORE_SOURCE = CORE_SOURCE_PATH.read_text(encoding="utf-8")
CORE_HEADER = CORE_HEADER_PATH.read_text(encoding="utf-8")


class SuperChipContractTests(unittest.TestCase):
    def test_vendored_octemu_core_is_pinned(self):
        expected = {
            CORE_SOURCE_PATH: "0e450a9eff61a3e954dee5e776d2a653c543e3a2c00cc973009d7e0fcc2ecb2d",
            CORE_HEADER_PATH: "e6f4b92aa32127de7248c4000db3863ffa85fc496bbd089f5e8f96ad3cd1bad6",
        }
        for path, digest in expected.items():
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), digest)

    def test_core_exposes_all_compatibility_modes(self):
        for mode in ("OCTEMU_MODE_CHIP8", "OCTEMU_MODE_SCHIP", "OCTEMU_MODE_OCTO"):
            self.assertIn(mode, CORE_HEADER)

    def test_core_contains_super_chip_instruction_families(self):
        for opcode in ("case 0xFB", "case 0xFC", "case 0xFD", "case 0xFE", "case 0xFF"):
            self.assertIn(opcode, CORE_SOURCE)
        for opcode in ("case 0x30", "case 0x75", "case 0x85"):
            self.assertIn(opcode, CORE_SOURCE)
        self.assertIn("ins_n * 2", CORE_SOURCE)

    def test_all_sixteen_hex_keys_have_unique_direct_bindings(self):
        bindings = re.findall(r"\{KeyCode::(\w+), 0x([0-9A-F])\}", SOURCE)
        self.assertEqual(len(bindings), 16)
        self.assertEqual({int(value, 16) for _, value in bindings}, set(range(16)))
        expected = {
            **{f"K{number}": number for number in range(10)},
            "Slash": 0xA,
            "Star": 0xB,
            "Minus": 0xC,
            "Plus": 0xD,
            "Power": 0xE,
            "Dot": 0xF,
        }
        self.assertEqual({key: int(value, 16) for key, value in bindings}, expected)

    def test_native_super_chip_viewport_is_centered(self):
        self.assertIn("constexpr uint16_t SCREEN_WIDTH = 256", SOURCE)
        self.assertIn("constexpr uint8_t SCREEN_HEIGHT = 64", SOURCE)
        self.assertIn("(SCREEN_WIDTH - OCTEMU_GFX_WIDTH) / 2", SOURCE)
        self.assertIn("OCTEMU_GFX_WIDTH 128", CORE_HEADER)
        self.assertIn("OCTEMU_GFX_HEIGHT 64", CORE_HEADER)
        self.assertIn("display.drawBitmap(VIEWPORT_X, 0", SOURCE)

    def test_rom_library_is_strictly_scoped(self):
        self.assertIn('DIRECTORY[] = "/chip8"', LIBRARY_HEADER)
        self.assertIn('endsWithIgnoreCase(path, ".ch8")', LIBRARY_SOURCE)
        self.assertIn("MAX_ROMS = 64", LIBRARY_HEADER)
        self.assertIn("MAX_SCAN_ENTRIES = 256", LIBRARY_HEADER)
        self.assertIn("MAX_ROM_BYTES = 4096 - 0x200", LIBRARY_HEADER)
        self.assertIn("!entry.isDirectory()", LIBRARY_SOURCE)

    def test_no_rom_images_are_bundled(self):
        self.assertEqual(list(ROOT.rglob("*.ch8")), [])

    def test_return_path_and_hotkeys_are_present(self):
        self.assertIn('MAIN_FIRMWARE[] = "/main.bin"', SOURCE)
        self.assertIn("EXIT_HOLD_MS = 1000", SOURCE)
        for key in ("K4", "K6", "K7", "K9", "K0", "Dot", "Enter", "Backspace", "K5"):
            self.assertIn(f"event.key == KeyCode::{key}", SOURCE)


if __name__ == "__main__":
    unittest.main()
