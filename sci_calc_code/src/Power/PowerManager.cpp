#include "PowerManager.h"

#include <Preferences.h>
#include <esp_sleep.h>
#include <esp32-hal-cpu.h>

#include "../Utils/Keyboard.h"
#include "../Utils/Util.h"

namespace {

constexpr char PREFERENCES_NAMESPACE[] = "sci-power";
constexpr char DISPLAY_SLEEP_KEY[] = "screen";
constexpr char AUTO_OFF_KEY[] = "off";
constexpr uint32_t LOW_POWER_CPU_MHZ = 80;

constexpr uint16_t DISPLAY_SLEEP_MINUTES[] = {0, 1, 3, 5, 10};
constexpr uint16_t AUTO_OFF_MINUTES[] = {0, 15, 30, 60, 120};
constexpr const char *DISPLAY_SLEEP_LABELS[] = {"Off", "1 min", "3 min", "5 min", "10 min"};
constexpr const char *AUTO_OFF_LABELS[] = {"Off", "15 min", "30 min", "60 min", "120 min"};

constexpr uint8_t DISPLAY_SLEEP_OPTION_COUNT = sizeof(DISPLAY_SLEEP_MINUTES) / sizeof(DISPLAY_SLEEP_MINUTES[0]);
constexpr uint8_t AUTO_OFF_OPTION_COUNT = sizeof(AUTO_OFF_MINUTES) / sizeof(AUTO_OFF_MINUTES[0]);

uint32_t minutesToMilliseconds(uint16_t minutes) {
    return static_cast<uint32_t>(minutes) * 60UL * 1000UL;
}

uint8_t wrapOption(uint8_t value, int direction, uint8_t optionCount) {
    const int next = (static_cast<int>(value) + direction + optionCount) % optionCount;
    return static_cast<uint8_t>(next);
}

bool anyKeyPressed(Keyboard &keyboard) {
    for (uint8_t row = 0; row < ROWCNT; ++row) {
        for (uint8_t column = 0; column < COLCNT; ++column) {
            if (keyboard.getKey(row, column).getIsPressed()) return true;
        }
    }
    return false;
}

}

PowerManager powerManager;

void PowerManager::begin() {
    Preferences preferences;
    preferences.begin(PREFERENCES_NAMESPACE, true);
    displaySleepIndex = preferences.getUChar(DISPLAY_SLEEP_KEY, 0);
    autoOffIndex = preferences.getUChar(AUTO_OFF_KEY, 0);
    preferences.end();

    if (displaySleepIndex >= DISPLAY_SLEEP_OPTION_COUNT) displaySleepIndex = 0;
    if (autoOffIndex >= AUTO_OFF_OPTION_COUNT) autoOffIndex = 0;

    normalCpuMhz = getCpuFrequencyMhz();
    lastActivityMs = millis();
}

bool PowerManager::update(Keyboard &keyboard) {
    const uint32_t now = millis();

    if (waitingForWakeKeyRelease) {
        lastActivityMs = now;
        if (!anyKeyPressed(keyboard)) waitingForWakeKeyRelease = false;
        delay(5);
        return false;
    }

    const bool hasKeyActivity = keyboard.getChangedKey().first >= 0;

    if (hasKeyActivity) {
        lastActivityMs = now;
        if (displaySleeping) {
            leaveDisplaySleep();
            waitingForWakeKeyRelease = true;
            return false;
        }
    }

    const uint32_t idleMs = now - lastActivityMs;
    const uint16_t autoOffMinutes = AUTO_OFF_MINUTES[autoOffIndex];
    if (autoOffMinutes > 0 && idleMs >= minutesToMilliseconds(autoOffMinutes)) {
        enterDeepSleep();
    }

    const uint16_t displaySleepMinutes = DISPLAY_SLEEP_MINUTES[displaySleepIndex];
    if (!displaySleeping && displaySleepMinutes > 0
        && idleMs >= minutesToMilliseconds(displaySleepMinutes)) {
        enterDisplaySleep();
    }

    if (displaySleeping) {
        delay(20);
        return false;
    }
    return true;
}

uint8_t PowerManager::getDisplaySleepIndex() const {
    return displaySleepIndex;
}

uint8_t PowerManager::getAutoOffIndex() const {
    return autoOffIndex;
}

const char *PowerManager::getDisplaySleepLabel() const {
    return DISPLAY_SLEEP_LABELS[displaySleepIndex];
}

const char *PowerManager::getAutoOffLabel() const {
    return AUTO_OFF_LABELS[autoOffIndex];
}

void PowerManager::changeDisplaySleep(int direction) {
    displaySleepIndex = wrapOption(displaySleepIndex, direction, DISPLAY_SLEEP_OPTION_COUNT);
    lastActivityMs = millis();
    saveSettings();
}

void PowerManager::changeAutoOff(int direction) {
    autoOffIndex = wrapOption(autoOffIndex, direction, AUTO_OFF_OPTION_COUNT);
    lastActivityMs = millis();
    saveSettings();
}

void PowerManager::saveSettings() {
    Preferences preferences;
    if (!preferences.begin(PREFERENCES_NAMESPACE, false)) return;
    preferences.putUChar(DISPLAY_SLEEP_KEY, displaySleepIndex);
    preferences.putUChar(AUTO_OFF_KEY, autoOffIndex);
    preferences.end();
}

void PowerManager::enterDisplaySleep() {
    displaySleeping = true;
    u8g2.setPowerSave(1);
    setCpuFrequencyMhz(LOW_POWER_CPU_MHZ);
}

void PowerManager::leaveDisplaySleep() {
    setCpuFrequencyMhz(normalCpuMhz);
    u8g2.setPowerSave(0);
    displaySleeping = false;
}

[[noreturn]] void PowerManager::enterDeepSleep() {
    if (!displaySleeping) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(71, 29, "AUTO POWER OFF");
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(76, 44, "RESET TO WAKE");
        u8g2.sendBuffer();
        delay(500);
    }

    u8g2.setPowerSave(1);
    for (uint8_t column = 0; column < COLCNT; ++column) {
        digitalWrite(COL_MOSI[column], LOW);
        pinMode(COL_MOSI[column], INPUT);
    }
    Serial.flush();
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_deep_sleep_start();

    while (true) delay(1000);
}
