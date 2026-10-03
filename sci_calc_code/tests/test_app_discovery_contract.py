from pathlib import Path
from tempfile import TemporaryDirectory
import unittest


MAX_MANIFEST_BYTES = 768
MAX_DISPLAY_NAME_BYTES = 24
MAX_FIRMWARE_PATH_BYTES = 80
MAX_FIRMWARE_BYTES = 0x1E0000


def parse_manifest(contents):
    if not contents or len(contents.encode("utf-8")) > MAX_MANIFEST_BYTES:
        return None
    if contents.startswith("\ufeff"):
        contents = contents[1:]

    descriptor = {"name": "", "firmware": "", "order": 100}
    valid_format = False
    enabled = True
    valid = True
    for raw_line in contents.splitlines():
        line = raw_line.strip()
        if not line or line.startswith(("#", ";")):
            continue
        if "=" not in line or line.index("=") == 0:
            valid = False
            continue
        key, value = (part.strip() for part in line.split("=", 1))
        key = key.lower()
        if key == "format":
            valid_format = value == "1"
        elif key == "name":
            descriptor["name"] = value
        elif key == "firmware":
            descriptor["firmware"] = value
        elif key == "enabled":
            lowered = value.lower()
            if lowered in ("1", "true", "yes"):
                enabled = True
            elif lowered in ("0", "false", "no"):
                enabled = False
            else:
                valid = False
        elif key == "order":
            try:
                descriptor["order"] = int(value, 10)
            except ValueError:
                valid = False
            else:
                valid = valid and -1000 <= descriptor["order"] <= 1000

    name = descriptor["name"]
    firmware = descriptor["firmware"]
    name_bytes = name.encode("ascii", errors="ignore")
    valid_name = (
        1 <= len(name_bytes) <= MAX_DISPLAY_NAME_BYTES
        and len(name_bytes) == len(name)
        and all(0x20 <= value <= 0x7E for value in name_bytes)
        and ".bin" not in name.lower()
        and "/" not in name
        and "\\" not in name
    )
    valid_path = (
        6 <= len(firmware) <= MAX_FIRMWARE_PATH_BYTES
        and firmware.startswith("/")
        and firmware.lower().endswith(".bin")
        and firmware.lower() != "/main.bin"
        and ".." not in firmware
        and "//" not in firmware
        and "\\" not in firmware
        and ":" not in firmware
        and all(0x20 <= ord(value) <= 0x7E for value in firmware)
    )
    if not (valid and valid_format and enabled and valid_name and valid_path):
        return None
    return descriptor


def discover(sd_root):
    app_directory = sd_root / "apps"
    if not app_directory.is_dir():
        return []

    discovered = []
    entries = list(app_directory.iterdir())[:128]
    for entry in entries:
        if len(discovered) == 16:
            break
        if not entry.is_file() or entry.suffix.lower() != ".app":
            continue
        descriptor = parse_manifest(entry.read_text(encoding="utf-8"))
        if descriptor is None:
            continue
        firmware = sd_root / descriptor["firmware"].lstrip("/")
        if not firmware.is_file():
            continue
        data = firmware.read_bytes()
        if not (32 <= len(data) <= MAX_FIRMWARE_BYTES and data[0] == 0xE9):
            continue
        if any(
            item["name"].lower() == descriptor["name"].lower()
            or item["firmware"].lower() == descriptor["firmware"].lower()
            for item in discovered
        ):
            continue
        discovered.append(descriptor)
    return sorted(discovered, key=lambda item: (item["order"], item["name"].lower()))


class AppDiscoveryContractTests(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = TemporaryDirectory()
        self.sd_root = Path(self.temporary_directory.name)
        (self.sd_root / "apps").mkdir()

    def tearDown(self):
        self.temporary_directory.cleanup()

    def write_firmware(self, path="tools/internal-name.bin"):
        target = self.sd_root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(bytes([0xE9]) + bytes(31))

    def write_manifest(self, text, name="entry.app"):
        (self.sd_root / "apps" / name).write_text(text, encoding="utf-8")

    def test_raw_bin_files_are_never_discovered(self):
        self.write_firmware("apps/visible-looking.bin")
        self.assertEqual(discover(self.sd_root), [])

    def test_label_comes_only_from_explicit_name(self):
        self.write_firmware()
        self.write_manifest(
            "format=1\nname=Unit Converter\nfirmware=/tools/internal-name.bin\n"
        )
        self.assertEqual([item["name"] for item in discover(self.sd_root)], ["Unit Converter"])

    def test_missing_name_never_falls_back_to_a_filename(self):
        self.write_firmware()
        self.write_manifest("format=1\nfirmware=/tools/internal-name.bin\n", "Pretty Name.app")
        self.assertEqual(discover(self.sd_root), [])

    def test_bin_like_display_name_is_rejected(self):
        self.write_firmware()
        self.write_manifest(
            "format=1\nname=internal-name.bin\nfirmware=/tools/internal-name.bin\n"
        )
        self.assertEqual(discover(self.sd_root), [])

    def test_windows_utf8_bom_is_accepted(self):
        self.write_firmware()
        self.write_manifest(
            "\ufeffformat=1\nname=BOM Safe Tool\nfirmware=/tools/internal-name.bin\n"
        )
        self.assertEqual([item["name"] for item in discover(self.sd_root)], ["BOM Safe Tool"])

    def test_invalid_firmware_is_omitted(self):
        self.write_manifest("format=1\nname=Broken App\nfirmware=/missing.bin\n")
        self.assertEqual(discover(self.sd_root), [])

    def test_order_sorts_labels_without_renaming_them(self):
        self.write_firmware("tools/late.bin")
        self.write_firmware("tools/early.bin")
        self.write_manifest(
            "format=1\nname=Late Tool\nfirmware=/tools/late.bin\norder=20\n",
            "z.app",
        )
        self.write_manifest(
            "format=1\nname=Early Tool\nfirmware=/tools/early.bin\norder=-5\n",
            "a.app",
        )
        self.assertEqual(
            [item["name"] for item in discover(self.sd_root)],
            ["Early Tool", "Late Tool"],
        )

    def test_cpp_scanner_keeps_filename_and_label_channels_separate(self):
        source = (
            Path(__file__).resolve().parents[1] / "src" / "Utils" / "AppDiscovery.cpp"
        ).read_text(encoding="utf-8")
        manifest_filter = source.split("bool isManifestFileName", 1)[1].split("}", 1)[0]
        self.assertIn('endsWithIgnoreCase(fileName, ".app")', manifest_filter)
        self.assertNotIn('".bin"', manifest_filter)
        self.assertIn(
            "entry.isDirectory() || !isManifestFileName(descriptorFileName)", source
        )
        self.assertIn('if (lower.indexOf(".bin") >= 0) return false;', source)
        self.assertIn('Text(std::string(apps[index].name.c_str()))', source)
        self.assertIn('BinLink(std::string(apps[index].firmware.c_str()))', source)
        self.assertNotIn("Text(std::string(descriptorFileName", source)
        self.assertNotIn("Text(std::string(apps[index].firmware", source)

        ui_source = (
            Path(__file__).resolve().parents[1] / "src" / "UIMain.cpp"
        ).read_text(encoding="utf-8")
        self.assertNotRegex(ui_source, r'new\s+Text\("[^"]*\.bin[^"]*"\)')

    def test_all_current_standalone_apps_are_reserved_as_builtins(self):
        source = (
            Path(__file__).resolve().parents[1] / "src" / "Utils" / "AppDiscovery.cpp"
        ).read_text(encoding="utf-8")
        for name, firmware in (
            ("2048", "/2048.bin"),
            ("Classic Calc", "/classiccalc.bin"),
            ("Music Keyboard", "/musickeyboard.bin"),
            ("USB Key Share", "/serialkeyboard.bin"),
        ):
            self.assertIn(f'"{name}"', source)
            self.assertIn(f'"{firmware}"', source)


if __name__ == "__main__":
    unittest.main()
