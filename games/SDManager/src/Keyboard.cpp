#include "Keyboard.h"

namespace {
constexpr uint8_t ROW_PINS[5] = {33, 32, 35, 34, 39};
constexpr uint8_t COL_PINS[6] = {13, 12, 14, 27, 26, 25};
constexpr uint32_t SCAN_MS = 5;
constexpr uint32_t DEBOUNCE_MS = 18;
}

void Keyboard::begin() {
    for (uint8_t row = 0; row < ROWS; ++row) pinMode(ROW_PINS[row], INPUT);
    for (uint8_t col = 0; col < COLS; ++col) {
        pinMode(COL_PINS[col], OUTPUT);
        digitalWrite(COL_PINS[col], LOW);
    }
}

ManagerKey Keyboard::keyFor(uint8_t row, uint8_t col) {
    static const ManagerKey MAP[ROWS][COLS] = {
        {ManagerKey::Escape, ManagerKey::Divide, ManagerKey::Multiply, ManagerKey::Minus, ManagerKey::Sin, ManagerKey::Sec},
        {ManagerKey::K7, ManagerKey::K8, ManagerKey::K9, ManagerKey::Plus, ManagerKey::Cos, ManagerKey::Csc},
        {ManagerKey::K4, ManagerKey::K5, ManagerKey::K6, ManagerKey::Power, ManagerKey::Tan, ManagerKey::Cot},
        {ManagerKey::K1, ManagerKey::K2, ManagerKey::K3, ManagerKey::Enter, ManagerKey::Exp, ManagerKey::Extra},
        {ManagerKey::Shift, ManagerKey::K0, ManagerKey::Dot, ManagerKey::Backspace, ManagerKey::Angle, ManagerKey::Tab}
    };
    return MAP[row][col];
}

uint8_t Keyboard::indexFor(ManagerKey key) {
    for (uint8_t row = 0; row < ROWS; ++row) {
        for (uint8_t col = 0; col < COLS; ++col) {
            if (keyFor(row, col) == key) return row * COLS + col;
        }
    }
    return 0xFF;
}

void Keyboard::update() {
    const uint32_t now = millis();
    if (now - lastScan_ < SCAN_MS) return;
    lastScan_ = now;

    for (uint8_t col = 0; col < COLS; ++col) {
        digitalWrite(COL_PINS[col], HIGH);
        delayMicroseconds(4);
        for (uint8_t row = 0; row < ROWS; ++row) {
            State &state = states_[row * COLS + col];
            const bool raw = digitalRead(ROW_PINS[row]) == HIGH;
            if (raw != state.raw) {
                state.raw = raw;
                state.changedAt = now;
            }
            if (raw != state.stable && now - state.changedAt >= DEBOUNCE_MS) {
                state.stable = raw;
                if (raw) state.pressedAt = now;
            }
        }
        digitalWrite(COL_PINS[col], LOW);
    }
}

bool Keyboard::isDown(ManagerKey key) const {
    const uint8_t index = indexFor(key);
    return index != 0xFF && states_[index].stable;
}

uint32_t Keyboard::heldMs(ManagerKey key) const {
    const uint8_t index = indexFor(key);
    if (index == 0xFF || !states_[index].stable) return 0;
    return millis() - states_[index].pressedAt;
}
