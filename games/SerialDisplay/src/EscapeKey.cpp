#include "EscapeKey.h"

namespace {
constexpr uint8_t ROW_PINS[5] = {33, 32, 35, 34, 39};
constexpr uint8_t COL_PINS[6] = {13, 12, 14, 27, 26, 25};
constexpr uint32_t SCAN_INTERVAL_MS = 5;
constexpr uint32_t DEBOUNCE_MS = 18;
}

void EscapeKey::begin() {
    for (uint8_t pin : ROW_PINS) pinMode(pin, INPUT);
    for (uint8_t pin : COL_PINS) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
}

void EscapeKey::update() {
    const uint32_t now = millis();
    if (now - lastScan_ < SCAN_INTERVAL_MS) return;
    lastScan_ = now;

    digitalWrite(COL_PINS[0], HIGH);
    delayMicroseconds(4);
    const bool raw = digitalRead(ROW_PINS[0]) == HIGH;
    digitalWrite(COL_PINS[0], LOW);

    if (raw != raw_) {
        raw_ = raw;
        changedAt_ = now;
    }
    if (raw != stable_ && now - changedAt_ >= DEBOUNCE_MS) {
        stable_ = raw;
        if (raw) pressedAt_ = now;
    }
}

bool EscapeKey::isDown() const {
    return stable_;
}

uint32_t EscapeKey::heldMs() const {
    return stable_ ? millis() - pressedAt_ : 0;
}
