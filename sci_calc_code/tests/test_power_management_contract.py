import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
UI = (ROOT / "src" / "UIMain.cpp").read_text(encoding="utf-8")
MANAGER = (ROOT / "src" / "Power" / "PowerManager.cpp").read_text(encoding="utf-8")
SETTINGS = (ROOT / "src" / "Power" / "PowerSettings.cpp").read_text(encoding="utf-8")
UTIL = (ROOT / "src" / "Utils" / "Util.h").read_text(encoding="utf-8")


class PowerManagementContractTests(unittest.TestCase):
    def test_power_settings_are_reachable_from_main_settings(self):
        self.assertIn('new Text("Power saving")', UI)
        self.assertIn("&powerSettings", UI)
        self.assertIn('"POWER SAVING"', SETTINGS)

    def test_both_features_default_to_off_and_offer_timeout_choices(self):
        self.assertIn("DISPLAY_SLEEP_MINUTES[] = {0, 1, 3, 5, 10}", MANAGER)
        self.assertIn("AUTO_OFF_MINUTES[] = {0, 15, 30, 60, 120}", MANAGER)
        self.assertIn('getUChar(DISPLAY_SLEEP_KEY, 0)', MANAGER)
        self.assertIn('getUChar(AUTO_OFF_KEY, 0)', MANAGER)

    def test_settings_are_persistent(self):
        self.assertIn("#include <Preferences.h>", MANAGER)
        self.assertIn("preferences.putUChar(DISPLAY_SLEEP_KEY", MANAGER)
        self.assertIn("preferences.putUChar(AUTO_OFF_KEY", MANAGER)

    def test_display_sleep_wakes_without_triggering_the_pressed_action(self):
        wake = MANAGER.index("if (displaySleeping) {")
        self.assertIn("leaveDisplaySleep();", MANAGER[wake:])
        self.assertIn("waitingForWakeKeyRelease = true;", MANAGER[wake:])
        self.assertIn("if (waitingForWakeKeyRelease)", MANAGER)
        self.assertIn("anyKeyPressed(keyboard)", MANAGER)
        self.assertIn("return false;", MANAGER[wake:])
        self.assertIn("u8g2.setPowerSave(1);", MANAGER)
        self.assertIn("setCpuFrequencyMhz(LOW_POWER_CPU_MHZ);", MANAGER)
        self.assertIn("u8g2.setPowerSave(0);", MANAGER)

    def test_auto_off_uses_deep_sleep_without_a_wakeup_source(self):
        self.assertIn("esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);", MANAGER)
        self.assertIn("esp_deep_sleep_start();", MANAGER)
        self.assertIn('"RESET TO WAKE"', MANAGER)

    def test_power_manager_only_runs_in_the_main_firmware_loop(self):
        self.assertIn("powerManager.begin();", MAIN)
        self.assertIn("if (!powerManager.update(kb)) return;", MAIN)
        self.assertIn('const std::string VERSION = "1.22";', UTIL)


if __name__ == "__main__":
    unittest.main()
