#include "SafeKeyboard.h"

namespace {
constexpr uint8_t ROW_PINS[5] = {33, 32, 35, 34, 39};
constexpr uint8_t COL_PINS[6] = {13, 12, 14, 27, 26, 25};
constexpr uint32_t SCAN_MS = 3;
constexpr uint32_t DEBOUNCE_MS = 14;
constexpr uint32_t REPEAT_DELAY_MS = 330;
constexpr uint32_t REPEAT_RATE_MS = 115;
}

void SafeKeyboard::begin() {
    for (uint8_t row = 0; row < ROWS; ++row) pinMode(ROW_PINS[row], INPUT);
    for (uint8_t col = 0; col < COLS; ++col) {
        pinMode(COL_PINS[col], OUTPUT);
        digitalWrite(COL_PINS[col], LOW);
    }
}

KeyboardKey SafeKeyboard::keyFor(uint8_t row, uint8_t col) {
    static const KeyboardKey MAP[ROWS][COLS] = {
        {KeyboardKey::Escape, KeyboardKey::Divide, KeyboardKey::Multiply, KeyboardKey::Minus, KeyboardKey::Sin, KeyboardKey::Sec},
        {KeyboardKey::K7, KeyboardKey::K8, KeyboardKey::K9, KeyboardKey::Plus, KeyboardKey::Cos, KeyboardKey::Csc},
        {KeyboardKey::K4, KeyboardKey::K5, KeyboardKey::K6, KeyboardKey::Power, KeyboardKey::Tan, KeyboardKey::Cot},
        {KeyboardKey::K1, KeyboardKey::K2, KeyboardKey::K3, KeyboardKey::Enter, KeyboardKey::Exp, KeyboardKey::Extra},
        {KeyboardKey::Shift, KeyboardKey::K0, KeyboardKey::Dot, KeyboardKey::Backspace, KeyboardKey::Angle, KeyboardKey::Tab}
    };
    return MAP[row][col];
}

uint8_t SafeKeyboard::indexFor(KeyboardKey key) {
    for (uint8_t row = 0; row < ROWS; ++row) {
        for (uint8_t col = 0; col < COLS; ++col) {
            if (keyFor(row, col) == key) return row * COLS + col;
        }
    }
    return 0xFF;
}

void SafeKeyboard::push(KeyboardKey key, KeyboardAction action) {
    if (key == KeyboardKey::None) return;
    const uint8_t next = (queueHead_ + 1) % QUEUE_SIZE;
    if (next == queueTail_) return;
    queue_[queueHead_].key = key;
    queue_[queueHead_].action = action;
    queueHead_ = next;
}

void SafeKeyboard::update() {
    const uint32_t now = millis();
    if (now - lastScan_ < SCAN_MS) return;
    lastScan_ = now;

    for (uint8_t col = 0; col < COLS; ++col) {
        digitalWrite(COL_PINS[col], HIGH);
        delayMicroseconds(4);
        for (uint8_t row = 0; row < ROWS; ++row) {
            const uint8_t index = row * COLS + col;
            State &state = states_[index];
            const bool raw = digitalRead(ROW_PINS[row]) == HIGH;
            if (raw != state.raw) {
                state.raw = raw;
                state.changedAt = now;
            }
            if (raw != state.stable && now - state.changedAt >= DEBOUNCE_MS) {
                state.stable = raw;
                if (raw) {
                    state.pressedAt = now;
                    state.repeatedAt = now;
                    push(keyFor(row, col), KeyboardAction::Press);
                } else {
                    push(keyFor(row, col), KeyboardAction::Release);
                }
            }
            if (state.stable && now - state.pressedAt >= REPEAT_DELAY_MS &&
                now - state.repeatedAt >= REPEAT_RATE_MS) {
                state.repeatedAt = now;
                push(keyFor(row, col), KeyboardAction::Repeat);
            }
        }
        digitalWrite(COL_PINS[col], LOW);
    }
}

bool SafeKeyboard::nextEvent(KeyboardEvent &event) {
    if (queueTail_ == queueHead_) return false;
    event = queue_[queueTail_];
    queueTail_ = (queueTail_ + 1) % QUEUE_SIZE;
    return true;
}

bool SafeKeyboard::isDown(KeyboardKey key) const {
    const uint8_t index = indexFor(key);
    return index != 0xFF && states_[index].stable;
}

uint32_t SafeKeyboard::heldMs(KeyboardKey key) const {
    const uint8_t index = indexFor(key);
    if (index == 0xFF || !states_[index].stable) return 0;
    return millis() - states_[index].pressedAt;
}
