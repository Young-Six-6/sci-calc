#include "PowerSettings.h"

#include "PowerManager.h"

PowerSettings::PowerSettings() : UIElement(0, 0, 210, 64) {
}

void PowerSettings::init() {
    selectedRow = 0;
}

void PowerSettings::activate() {
    currentElement = this;
}

void PowerSettings::drawRow(uint8_t row, const char *label, const char *value) {
    const int top = 14 + row * 16;
    const int baseline = top + 9;
    if (selectedRow == row) {
        u8g2.drawBox(2, top, 204, 12);
        u8g2.setDrawColor(0);
    }

    u8g2.drawStr(6, baseline, label);
    u8g2.drawStr(202 - u8g2.getStrWidth(value), baseline, value);
    u8g2.setDrawColor(1);
}

void PowerSettings::draw() {
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(3, 7, "POWER SAVING");
    u8g2.drawHLine(0, 9, 210);
    drawRow(0, "Display sleep", powerManager.getDisplaySleepLabel());
    drawRow(1, "Auto power off", powerManager.getAutoOffLabel());
    u8g2.drawStr(3, 61, "4/6 CHANGE   ESC BACK");
}

void PowerSettings::update() {
    draw();

    const std::pair<int, int> key = kb.getRisingEdgeKey();
    if (key == std::make_pair(1, 1) || key == std::make_pair(3, 1)) {
        selectedRow = 1 - selectedRow;
    } else if (key == std::make_pair(2, 0)) {
        if (selectedRow == 0) powerManager.changeDisplaySleep(-1);
        else powerManager.changeAutoOff(-1);
    } else if (key == std::make_pair(2, 2) || key == std::make_pair(3, 3)) {
        if (selectedRow == 0) powerManager.changeDisplaySleep(1);
        else powerManager.changeAutoOff(1);
    } else if (key == std::make_pair(0, 0)) {
        goBack();
    }
}
