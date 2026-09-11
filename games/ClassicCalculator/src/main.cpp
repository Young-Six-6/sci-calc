#include <Arduino.h>
#include <FS.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <U8g2lib.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "ClassicEngine.h"
#include "SafeKeyboard.h"
#include "SevenSegment.h"

namespace {
constexpr uint8_t OLED_CS = 5;
constexpr uint8_t OLED_DC = 16;
constexpr uint8_t OLED_RST = 17;
constexpr uint8_t SD_CS = 4;
constexpr uint16_t SCREEN_WIDTH = 256;
constexpr uint8_t SCREEN_HEIGHT = 64;
constexpr uint32_t EXIT_HOLD_MS = 1000;
constexpr char MAIN_FIRMWARE[] = "/main.bin";

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI display(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
SafeKeyboard keyboard;
ClassicEngine calculator;

enum class Screen : uint8_t { Calculator, Tape, Settings };
Screen screen = Screen::Calculator;
uint8_t selectedTape = 0;
uint8_t selectedSetting = 0;
bool settingsDirty = false;
bool sdReady = false;
bool exitTriggered = false;
bool dirty = true;
char notice[40] = {};
uint32_t noticeUntil = 0;

void copyText(char *destination, size_t size, const char *source) {
    if (!destination || !size) return;
    std::snprintf(destination, size, "%s", source ? source : "");
}

void setNotice(const char *message, uint32_t durationMs = 1100) {
    copyText(notice, sizeof(notice), message);
    noticeUntil = millis() + durationMs;
    dirty = true;
}

void loadPreferences() {
    Preferences preferences;
    if (!preferences.begin("classiccalc", true)) return;
    const uint8_t decimal = preferences.getUChar("decimal", 0);
    const uint8_t rounding = preferences.getUChar("rounding", 0);
    calculator.setDecimalMode(static_cast<DecimalMode>(decimal <= 3 ? decimal : 0));
    calculator.setRoundingMode(static_cast<RoundingMode>(rounding <= 2 ? rounding : 0));
    calculator.setGrouping(preferences.getBool("grouping", true));
    calculator.setTaxRate(preferences.getDouble("tax", 13.0));
    calculator.setStoredMemory(preferences.getDouble("memory", 0.0), preferences.getBool("memorySet", false));
    preferences.end();
}

void saveSettings() {
    Preferences preferences;
    if (!preferences.begin("classiccalc", false)) return;
    preferences.putUChar("decimal", static_cast<uint8_t>(calculator.decimalMode()));
    preferences.putUChar("rounding", static_cast<uint8_t>(calculator.roundingMode()));
    preferences.putBool("grouping", calculator.grouping());
    preferences.putDouble("tax", calculator.taxRate());
    preferences.end();
    settingsDirty = false;
}

void saveMemory() {
    Preferences preferences;
    if (!preferences.begin("classiccalc", false)) return;
    preferences.putDouble("memory", calculator.memoryValue());
    preferences.putBool("memorySet", calculator.memoryActive());
    preferences.end();
}

const char *decimalModeText(DecimalMode mode) {
    switch (mode) {
        case DecimalMode::Fixed0: return "0 DIGITS";
        case DecimalMode::Fixed2: return "2 DIGITS";
        case DecimalMode::Fixed4: return "4 DIGITS";
        default: return "AUTO";
    }
}

const char *decimalModeShort(DecimalMode mode) {
    switch (mode) {
        case DecimalMode::Fixed0: return "F0";
        case DecimalMode::Fixed2: return "F2";
        case DecimalMode::Fixed4: return "F4";
        default: return "AUTO";
    }
}

const char *roundingModeText(RoundingMode mode) {
    switch (mode) {
        case RoundingMode::Truncate: return "TRUNCATE";
        case RoundingMode::AwayFromZero: return "ROUND UP";
        default: return "NEAREST";
    }
}

const char *roundingModeShort(RoundingMode mode) {
    switch (mode) {
        case RoundingMode::Truncate: return "T";
        case RoundingMode::AwayFromZero: return "U";
        default: return "N";
    }
}

void fitTextFromLeft(char *text, uint16_t maxWidth) {
    if (!text) return;
    while (*text && display.getStrWidth(text) > maxWidth) {
        const size_t length = std::strlen(text);
        if (length < 2) break;
        text[length - 1] = '\0';
    }
}

const char *tailThatFits(const char *text, uint16_t maxWidth) {
    if (!text) return "";
    const char *visible = text;
    while (visible[1] && display.getStrWidth(visible) > maxWidth) ++visible;
    return visible;
}

void drawHeader() {
    display.setFont(u8g2_font_4x6_tf);
    display.drawStr(2, 6, "CLASSIC");
    int16_t x = 37;
    if (calculator.memoryActive()) {
        display.drawStr(x, 6, "M");
        x += 8;
    }
    if (calculator.grandTotalActive()) {
        display.drawStr(x, 6, "GT");
        x += 13;
    }
    if (calculator.undoAvailable()) {
        display.drawStr(x, 6, "U");
        x += 8;
    }
    if (calculator.pendingOperation() != CalcOperation::None) {
        display.drawStr(x, 6, "OP");
        x += 10;
        display.drawStr(x, 6, ClassicEngine::operationSymbol(calculator.pendingOperation()));
    }
    if (keyboard.isDown(KeyboardKey::Shift)) display.drawStr(112, 6, "SHIFT");

    char mode[20];
    std::snprintf(mode, sizeof(mode), "%s %s%s", decimalModeShort(calculator.decimalMode()),
                  roundingModeShort(calculator.roundingMode()), calculator.grouping() ? " ," : "");
    display.drawStr(254 - display.getStrWidth(mode), 6, mode);
    display.drawHLine(0, 9, SCREEN_WIDTH);
}

void drawCalculator() {
    drawHeader();
    if (calculator.hasError()) {
        display.setFont(u8g2_font_10x20_tf);
        const char *title = "ERROR";
        display.drawStr((SCREEN_WIDTH - display.getStrWidth(title)) / 2, 36, title);
    } else {
        char value[48];
        calculator.formatDisplay(value, sizeof(value));
        SevenSegment::drawRightAligned(display, value, 253, 12, 250, 35);
    }

    display.drawHLine(0, 50, SCREEN_WIDTH);
    display.setFont(u8g2_font_5x7_tf);
    if (noticeUntil && static_cast<int32_t>(noticeUntil - millis()) > 0) {
        display.drawStr(2, 61, tailThatFits(notice, 252));
    } else if (calculator.hasError()) {
        display.drawStr(2, 61, calculator.errorText());
    } else {
        char context[80];
        calculator.contextText(context, sizeof(context));
        display.drawStr(2, 61, tailThatFits(context, 252));
    }
}

void drawTape() {
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(2, 7, "PAPER TAPE");
    char counter[12];
    if (calculator.tapeCount()) std::snprintf(counter, sizeof(counter), "%u/%u", selectedTape + 1, calculator.tapeCount());
    else copyText(counter, sizeof(counter), "0/24");
    display.drawStr(254 - display.getStrWidth(counter), 7, counter);
    display.drawHLine(0, 9, SCREEN_WIDTH);

    const uint8_t count = calculator.tapeCount();
    if (!count) {
        display.setFont(u8g2_font_6x10_tf);
        const char *empty = "NO ENTRIES";
        display.drawStr((SCREEN_WIDTH - display.getStrWidth(empty)) / 2, 38, empty);
        return;
    }

    constexpr uint8_t visibleRows = 5;
    uint8_t first = selectedTape >= visibleRows ? selectedTape - visibleRows + 1 : 0;
    if (first + visibleRows > count) first = count > visibleRows ? count - visibleRows : 0;
    for (uint8_t row = 0; row < visibleRows && first + row < count; ++row) {
        const uint8_t logicalIndex = first + row;
        const uint8_t top = 11 + row * 10;
        const bool selected = logicalIndex == selectedTape;
        if (selected) display.drawBox(0, top, SCREEN_WIDTH, 9);
        display.setDrawColor(selected ? 0 : 1);
        char line[76];
        const TapeRecord *record = calculator.tapeRecord(logicalIndex);
        std::snprintf(line, sizeof(line), "%02u  %s", logicalIndex + 1, record ? record->text : "");
        fitTextFromLeft(line, 252);
        display.drawStr(2, top + 7, line);
        display.setDrawColor(1);
    }
}

void drawSettingRow(uint8_t index, uint8_t top, const char *label, const char *value) {
    const bool selected = selectedSetting == index;
    if (selected) display.drawBox(0, top, SCREEN_WIDTH, 12);
    display.setDrawColor(selected ? 0 : 1);
    display.drawStr(3, top + 9, label);
    display.drawStr(253 - display.getStrWidth(value), top + 9, value);
    display.setDrawColor(1);
}

void drawSettings() {
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(2, 7, "ACCOUNTING SETUP");
    display.drawHLine(0, 9, SCREEN_WIDTH);
    drawSettingRow(0, 11, "DECIMALS", decimalModeText(calculator.decimalMode()));
    drawSettingRow(1, 24, "ROUNDING", roundingModeText(calculator.roundingMode()));
    drawSettingRow(2, 37, "SEPARATOR", calculator.grouping() ? "1,000 ON" : "1000 OFF");
    char tax[20];
    std::snprintf(tax, sizeof(tax), "%.2f%%", calculator.taxRate());
    drawSettingRow(3, 50, "TAX RATE", tax);
}

void drawScreen() {
    display.clearBuffer();
    if (screen == Screen::Calculator) drawCalculator();
    else if (screen == Screen::Tape) drawTape();
    else drawSettings();
    display.sendBuffer();
    dirty = false;
}

int8_t digitFor(KeyboardKey key) {
    switch (key) {
        case KeyboardKey::K0: return 0;
        case KeyboardKey::K1: return 1;
        case KeyboardKey::K2: return 2;
        case KeyboardKey::K3: return 3;
        case KeyboardKey::K4: return 4;
        case KeyboardKey::K5: return 5;
        case KeyboardKey::K6: return 6;
        case KeyboardKey::K7: return 7;
        case KeyboardKey::K8: return 8;
        case KeyboardKey::K9: return 9;
        default: return -1;
    }
}

void enterTape() {
    screen = Screen::Tape;
    selectedTape = calculator.tapeCount() ? calculator.tapeCount() - 1 : 0;
    dirty = true;
}

void enterSettings() {
    screen = Screen::Settings;
    selectedSetting = 0;
    settingsDirty = false;
    dirty = true;
}

void leaveSettings() {
    if (settingsDirty) saveSettings();
    screen = Screen::Calculator;
    dirty = true;
}

void handleShiftedCalculatorKey(KeyboardKey key) {
    if (key == KeyboardKey::Plus) {
        calculator.applyTax(true);
        setNotice("TAX ADDED");
    } else if (key == KeyboardKey::Minus) {
        calculator.applyTax(false);
        setNotice("TAX REMOVED");
    } else if (key == KeyboardKey::Power) {
        if (calculator.setTaxRate(std::fabs(calculator.currentValue()))) {
            saveSettings();
            char message[32];
            std::snprintf(message, sizeof(message), "TAX RATE %.2f%%", calculator.taxRate());
            setNotice(message);
        } else {
            setNotice("TAX RATE MUST BE 0-99.99");
        }
    } else if (key == KeyboardKey::Csc) {
        calculator.grandTotalClear();
        setNotice("GT CLEARED");
    } else if (key == KeyboardKey::Tab) {
        enterSettings();
    } else if (key == KeyboardKey::Backspace) {
        calculator.clearTape();
        setNotice("TAPE CLEARED");
    }
}

void handleCalculatorKey(const KeyboardEvent &event) {
    if (event.action == KeyboardAction::Release || event.key == KeyboardKey::Shift) return;
    if (event.action == KeyboardAction::Repeat && event.key != KeyboardKey::Backspace) return;

    if (event.key == KeyboardKey::Escape) {
        calculator.allClear();
        setNotice("ALL CLEAR");
        return;
    }

    if (keyboard.isDown(KeyboardKey::Shift)) {
        handleShiftedCalculatorKey(event.key);
        dirty = true;
        return;
    }

    const int8_t digit = digitFor(event.key);
    if (digit >= 0) calculator.inputDigit(digit);
    else if (event.key == KeyboardKey::Dot) calculator.inputDecimal();
    else if (event.key == KeyboardKey::Exp) calculator.inputDoubleZero();
    else if (event.key == KeyboardKey::Angle) calculator.toggleSign();
    else if (event.key == KeyboardKey::Backspace) calculator.backspace();
    else if (event.key == KeyboardKey::Divide) calculator.setOperation(CalcOperation::Divide);
    else if (event.key == KeyboardKey::Multiply) calculator.setOperation(CalcOperation::Multiply);
    else if (event.key == KeyboardKey::Minus) calculator.setOperation(CalcOperation::Subtract);
    else if (event.key == KeyboardKey::Plus) calculator.setOperation(CalcOperation::Add);
    else if (event.key == KeyboardKey::Power) calculator.percent();
    else if (event.key == KeyboardKey::Enter) calculator.equals();
    else if (event.key == KeyboardKey::Cot) {
        calculator.clearEntry();
        setNotice("CLEAR ENTRY");
    } else if (event.key == KeyboardKey::Sin) {
        calculator.memoryAdd();
        saveMemory();
        setNotice("M+");
    } else if (event.key == KeyboardKey::Cos) {
        calculator.memorySubtract();
        saveMemory();
        setNotice("M-");
    } else if (event.key == KeyboardKey::Tan) {
        calculator.memoryRecall();
        setNotice("MEMORY RECALL");
    } else if (event.key == KeyboardKey::Sec) {
        calculator.memoryClear();
        saveMemory();
        setNotice("MEMORY CLEARED");
    } else if (event.key == KeyboardKey::Csc) {
        calculator.grandTotalRecall();
        setNotice("GRAND TOTAL");
    } else if (event.key == KeyboardKey::Extra) {
        setNotice(calculator.undo() ? "UNDO" : "NOTHING TO UNDO");
    } else if (event.key == KeyboardKey::Tab) {
        enterTape();
    }
    dirty = true;
}

void handleTapeKey(const KeyboardEvent &event) {
    if (event.action == KeyboardAction::Release || event.key == KeyboardKey::Shift) return;
    if (event.key == KeyboardKey::Escape || event.key == KeyboardKey::Tab ||
        (event.key == KeyboardKey::Backspace && !keyboard.isDown(KeyboardKey::Shift))) {
        screen = Screen::Calculator;
        dirty = true;
        return;
    }
    if (event.key == KeyboardKey::Backspace && keyboard.isDown(KeyboardKey::Shift)) {
        calculator.clearTape();
        selectedTape = 0;
        setNotice("TAPE CLEARED");
        return;
    }
    if (event.key == KeyboardKey::K8 && selectedTape > 0) --selectedTape;
    else if (event.key == KeyboardKey::K2 && selectedTape + 1 < calculator.tapeCount()) ++selectedTape;
    else if (event.key == KeyboardKey::Enter && calculator.recallTape(selectedTape)) {
        screen = Screen::Calculator;
        setNotice("TAPE RESULT RECALLED");
    }
    dirty = true;
}

void adjustSetting(int8_t direction) {
    if (selectedSetting == 0) {
        const uint8_t current = static_cast<uint8_t>(calculator.decimalMode());
        calculator.setDecimalMode(static_cast<DecimalMode>((current + 4 + direction) % 4));
    } else if (selectedSetting == 1) {
        const uint8_t current = static_cast<uint8_t>(calculator.roundingMode());
        calculator.setRoundingMode(static_cast<RoundingMode>((current + 3 + direction) % 3));
    } else if (selectedSetting == 2) {
        calculator.setGrouping(!calculator.grouping());
    } else {
        const double next = calculator.taxRate() + direction * 0.25;
        calculator.setTaxRate(next < 0.0 ? 0.0 : next > 99.99 ? 99.99 : next);
    }
    settingsDirty = true;
    dirty = true;
}

void handleSettingsKey(const KeyboardEvent &event) {
    if (event.action == KeyboardAction::Release || event.key == KeyboardKey::Shift) return;
    if (event.key == KeyboardKey::Escape || event.key == KeyboardKey::Backspace || event.key == KeyboardKey::Tab) {
        leaveSettings();
        return;
    }
    if (event.key == KeyboardKey::K8) selectedSetting = (selectedSetting + 3) % 4;
    else if (event.key == KeyboardKey::K2) selectedSetting = (selectedSetting + 1) % 4;
    else if (event.key == KeyboardKey::K4) adjustSetting(-1);
    else if (event.key == KeyboardKey::K6 || event.key == KeyboardKey::Enter) adjustSetting(1);
    dirty = true;
}

void handleKeyboardEvent(const KeyboardEvent &event) {
    if (event.key == KeyboardKey::Shift) {
        dirty = true;
        return;
    }
    if (screen == Screen::Calculator) handleCalculatorKey(event);
    else if (screen == Screen::Tape) handleTapeKey(event);
    else handleSettingsKey(event);
}

bool installMainFirmware() {
    if (settingsDirty) saveSettings();
    if (!sdReady) sdReady = SD.begin(SD_CS, SPI);
    File firmware = sdReady ? SD.open(MAIN_FIRMWARE, FILE_READ) : File();
    if (!firmware || firmware.isDirectory() || !firmware.size()) {
        if (firmware) firmware.close();
        setNotice("MISSING /main.bin", 2500);
        return false;
    }

    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(62, 18, "RETURNING TO MAIN");
    display.drawFrame(4, 31, 248, 9);
    display.sendBuffer();

    const size_t size = firmware.size();
    if (!Update.begin(size, U_FLASH)) {
        firmware.close();
        setNotice("UPDATE COULD NOT START", 2500);
        return false;
    }

    uint8_t buffer[1024];
    size_t written = 0;
    uint8_t previousPercent = 255;
    while (firmware.available()) {
        const size_t count = firmware.read(buffer, sizeof(buffer));
        if (!count || Update.write(buffer, count) != count) break;
        written += count;
        const uint8_t percent = written * 100ULL / size;
        if (percent != previousPercent) {
            previousPercent = percent;
            display.setDrawColor(0);
            display.drawBox(5, 32, 246, 7);
            display.setDrawColor(1);
            display.drawBox(5, 32, percent * 246 / 100, 7);
            display.sendBuffer();
        }
        yield();
    }
    firmware.close();
    if (written == size && Update.end(true) && Update.isFinished()) {
        delay(250);
        ESP.restart();
        return true;
    }
    Update.abort();
    setNotice("FIRMWARE UPDATE FAILED", 2500);
    return false;
}
}

void setup() {
    Serial.begin(115200);
    pinMode(OLED_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(OLED_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    SPI.begin(18, 19, 23);
    display.begin();
    display.setContrast(220);
    keyboard.begin();
    loadPreferences();
    sdReady = SD.begin(SD_CS, SPI);

    display.clearBuffer();
    display.setFont(u8g2_font_10x20_tf);
    display.drawStr(35, 28, "CLASSIC CALCULATOR");
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(93, 46, "READY");
    display.sendBuffer();
    delay(350);
    drawScreen();
}

void loop() {
    keyboard.update();
    if (keyboard.isDown(KeyboardKey::Escape) && keyboard.heldMs(KeyboardKey::Escape) >= EXIT_HOLD_MS) {
        if (!exitTriggered) {
            exitTriggered = true;
            installMainFirmware();
        }
    } else if (!keyboard.isDown(KeyboardKey::Escape)) {
        exitTriggered = false;
    }

    KeyboardEvent event;
    while (keyboard.nextEvent(event)) handleKeyboardEvent(event);

    if (noticeUntil && static_cast<int32_t>(millis() - noticeUntil) >= 0) {
        noticeUntil = 0;
        dirty = true;
    }
    if (dirty) drawScreen();
    delay(1);
}
