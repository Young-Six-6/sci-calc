import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UI_SOURCE = (ROOT / "src" / "UIMain.cpp").read_text(encoding="utf-8")
UI_HEADER = (ROOT / "src" / "UIMain.h").read_text(encoding="utf-8")
MAIN_SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
MENU_SOURCE = (ROOT / "src" / "UIElements" / "Menu.cpp").read_text(encoding="utf-8")
SIDEBAR_HEADER = (ROOT / "src" / "Sidebar.h").read_text(encoding="utf-8")
SIDEBAR_SOURCE = (ROOT / "src" / "Sidebar.cpp").read_text(encoding="utf-8")


def menu_block(name):
    marker = f"Menu {name}("
    start = UI_SOURCE.index(marker)
    end = UI_SOURCE.index(";", start) + 1
    return UI_SOURCE[start:end]


class ProgramMenuContractTests(unittest.TestCase):
    def test_root_contains_only_the_five_categories(self):
        block = menu_block("programMenu")
        labels = re.findall(r'new Text\("([^"]+)"\)', block)
        self.assertEqual(
            labels,
            ["Calculation >", "Games >", "Music >", "Tools >", "Other >"],
        )
        links = re.findall(r"&(program\w+Menu)", block)
        self.assertEqual(
            links,
            [
                "programCalculationMenu",
                "programGameMenu",
                "programMusicMenu",
                "programToolMenu",
                "programOtherMenu",
            ],
        )

    def test_every_builtin_program_is_in_exactly_one_category(self):
        category_names = (
            "programCalculationMenu",
            "programGameMenu",
            "programMusicMenu",
            "programToolMenu",
            "programOtherMenu",
        )
        paths = []
        for name in category_names:
            paths.extend(re.findall(r'new BinLink\("([^"]+)"\)', menu_block(name)))
        expected = {
            "/graphcalc.bin",
            "/music.bin",
            "/musickeyboard.bin",
            "/sdmanager.bin",
            "/serialdisplay.bin",
            "/maze.bin",
            "/fallingkeys.bin",
            "/chip8.bin",
            "/chess.bin",
            "/spacetrash.bin",
            "/snake.bin",
            "/tetris.bin",
            "/COTD.bin",
            "/squario.bin",
            "/wifirickroll.bin",
        }
        self.assertEqual(set(paths), expected)
        self.assertEqual(len(paths), len(expected))

    def test_discovered_apps_are_appended_to_other(self):
        self.assertIn("extern Menu programOtherMenu;", UI_HEADER)
        self.assertIn("discoverSdApplications(programOtherMenu);", MAIN_SOURCE)
        self.assertNotIn("discoverSdApplications(programMenu);", MAIN_SOURCE)

    def test_directional_tree_navigation_is_wired(self):
        root = menu_block("programMenu")
        self.assertRegex(root, r"\},\s*true\s*\);")
        self.assertRegex(
            MENU_SOURCE,
            r"openWithRight && key == std::make_pair\(2, 2\)",
        )
        self.assertRegex(
            MENU_SOURCE,
            r"key == std::make_pair\(0, 0\).*key == std::make_pair\(2, 0\)",
        )

    def test_menu_animation_is_clipped_before_the_sidebar(self):
        clip = "u8g2.setClipWindow(0, 0, MAIN_PANEL_WIDTH, HEIGHT);"
        self.assertIn("constexpr int MAIN_PANEL_WIDTH = 210;", SIDEBAR_HEADER)
        self.assertGreaterEqual(MENU_SOURCE.count(clip), 2)
        self.assertGreaterEqual(MENU_SOURCE.count("u8g2.setMaxClipWindow();"), 2)

        clear = "u8g2.drawBox(MAIN_PANEL_WIDTH, 0, WIDTH - MAIN_PANEL_WIDTH, HEIGHT);"
        frame = "u8g2.drawRFrame(SIDEBAR_LEFT, 0, SIDEBAR_WIDTH, HEIGHT, 2);"
        self.assertIn(clear, SIDEBAR_SOURCE)
        self.assertIn(frame, SIDEBAR_SOURCE)
        self.assertLess(SIDEBAR_SOURCE.index(clear), SIDEBAR_SOURCE.index(frame))


if __name__ == "__main__":
    unittest.main()
