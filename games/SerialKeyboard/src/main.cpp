#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <U8g2lib.h>
#include <esp_system.h>

#include "KeyboardMatrix.h"
#include "SerialKeyProtocol.h"

namespace {
constexpr uint8_t OLED_CS = 5;
constexpr uint8_t OLED_DC = 16;
constexpr uint8_t OLED_RST = 17;
constexpr uint8_t SD_CS = 4;
constexpr uint32_t INFO_INTERVAL_MS = 2500;
constexpr uint32_t STATE_INTERVAL_MS = 1000;
constexpr uint32_t DISPLAY_INTERVAL_MS = 50;
constexpr uint32_t EXIT_HOLD_MS = 1000;
constexpr char MAIN_FIRMWARE[] = "/main.bin";

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI display(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
KeyboardMatrix keyboard;
uint32_t bootId = 0;
uint32_t eventCount = 0;
uint32_t lastInfoAt = 0;
uint32_t lastStateAt = 0;
uint32_t lastDisplayAt = 0;
bool sdReady = false;
bool displayDirty = true;
bool exitTriggered = false;
char lastEventText[48] = "LAST  waiting for a key";

const char *actionName(KeyAction action) {
    switch (action) {
        case KeyAction::Press: return "DOWN";
        case KeyAction::Release: return "UP";
        case KeyAction::Repeat: return "REPEAT";
    }
    return "?";
}

uint8_t pressedCount(uint32_t mask) {
    uint8_t count = 0;
    while (mask) {
        count += mask & 1U;
        mask >>= 1;
    }
    return count;
}

void drawStatus() {
    char line[64];
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(2, 10, "USB KEY SHARE");
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(2, 21, "TX ONLY  115200 BAUD  PROTO SKB1");
    display.drawStr(2, 32, lastEventText);
    snprintf(line, sizeof(line), "EVENTS %lu  DOWN %u  LOST %lu",
             static_cast<unsigned long>(eventCount),
             pressedCount(keyboard.stateMask()),
             static_cast<unsigned long>(keyboard.droppedEvents()));
    display.drawStr(2, 43, line);
    display.drawStr(2, 53, "PC RECEIVES EVENTS; NO USB HID");
    display.drawStr(2, 63, sdReady ? "SD READY" : "SD UNAVAILABLE");
    display.drawStr(158, 63, "HOLD ESC 1s: EXIT");
    display.sendBuffer();
}

void drawMessage(const char *message) {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(2, 11, "USB KEY SHARE");
    display.drawStr(2, 31, message);
    display.sendBuffer();
}

bool installMainFirmware() {
    File firmware = SD.open(MAIN_FIRMWARE, FILE_READ);
    if (!firmware || firmware.isDirectory() || !firmware.size()) {
        if (firmware) firmware.close();
        drawMessage("Missing /main.bin");
        return false;
    }

    const size_t size = firmware.size();
    if (!Update.begin(size, U_FLASH)) {
        firmware.close();
        drawMessage("Update begin failed");
        return false;
    }

    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(42, 15, "Returning to calculator");
    display.drawFrame(4, 28, 248, 9);
    display.sendBuffer();

    uint8_t buffer[1024];
    size_t written = 0;
    uint8_t previousPercent = 255;
    while (firmware.available()) {
        const size_t count = firmware.read(buffer, sizeof(buffer));
        if (!count || Update.write(buffer, count) != count) break;
        written += count;
        const uint8_t percent = static_cast<uint8_t>(written * 100ULL / size);
        if (percent != previousPercent) {
            previousPercent = percent;
            display.setDrawColor(0);
            display.drawBox(5, 29, 246, 7);
            display.setDrawColor(1);
            display.drawBox(5, 29, percent * 246 / 100, 7);
            display.sendBuffer();
        }
        yield();
    }
    firmware.close();

    if (written == size && Update.end(true) && Update.isFinished()) {
        delay(200);
        ESP.restart();
        return true;
    }

    Update.abort();
    drawMessage("Update failed");
    return false;
}
}

void setup() {
    Serial.begin(SerialKeyProtocol::BAUD_RATE);

    pinMode(OLED_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(OLED_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    SPI.begin(18, 19, 23);
    display.begin();
    display.setContrast(220);

    keyboard.begin();
    sdReady = SD.begin(SD_CS, SPI);
    bootId = esp_random();
    SerialKeyProtocol::reset();

    const uint32_t now = millis();
    SerialKeyProtocol::sendDeviceInfo(now, bootId);
    SerialKeyProtocol::sendState(now, keyboard.stateMask(), eventCount,
                                 keyboard.droppedEvents());
    lastInfoAt = now;
    lastStateAt = now;
    drawStatus();
    lastDisplayAt = now;
    displayDirty = false;
}

void loop() {
    keyboard.update();

    KeyEvent event;
    while (keyboard.nextEvent(event)) {
        ++eventCount;
        SerialKeyProtocol::sendKeyEvent(event);
        snprintf(lastEventText, sizeof(lastEventText), "LAST  %-6s %-12s R%u C%u",
                 actionName(event.action), KeyboardMatrix::keyName(event.key),
                 event.row + 1, event.col + 1);
        displayDirty = true;
    }

    const uint32_t now = millis();
    if (now - lastInfoAt >= INFO_INTERVAL_MS) {
        lastInfoAt = now;
        SerialKeyProtocol::sendDeviceInfo(now, bootId);
    }
    if (now - lastStateAt >= STATE_INTERVAL_MS) {
        lastStateAt = now;
        SerialKeyProtocol::sendState(now, keyboard.stateMask(), eventCount,
                                     keyboard.droppedEvents());
    }
    if (displayDirty && now - lastDisplayAt >= DISPLAY_INTERVAL_MS) {
        lastDisplayAt = now;
        displayDirty = false;
        drawStatus();
    }

    if (keyboard.isDown(KeyCode::Escape) &&
        keyboard.heldMs(KeyCode::Escape) >= EXIT_HOLD_MS) {
        if (!exitTriggered) {
            exitTriggered = true;
            installMainFirmware();
        }
    } else if (!keyboard.isDown(KeyCode::Escape)) {
        exitTriggered = false;
    }

    delay(1);
}
