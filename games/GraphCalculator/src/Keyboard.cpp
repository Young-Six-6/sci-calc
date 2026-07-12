#include "Keyboard.h"

namespace {
constexpr uint8_t ROW_PINS[5] = {33, 32, 35, 34, 39};
constexpr uint8_t COL_PINS[6] = {13, 12, 14, 27, 26, 25};
constexpr uint32_t SCAN_MS = 5;
constexpr uint32_t DEBOUNCE_MS = 18;
constexpr uint32_t REPEAT_DELAY_MS = 420;
constexpr uint32_t REPEAT_RATE_MS = 75;
}

void Keyboard::begin() {
    for (uint8_t row = 0; row < ROWS; ++row) pinMode(ROW_PINS[row], INPUT);
    for (uint8_t col = 0; col < COLS; ++col) {
        pinMode(COL_PINS[col], OUTPUT);
        digitalWrite(COL_PINS[col], LOW);
    }
}

KeyCode Keyboard::keyFor(uint8_t row, uint8_t col) {
    static const KeyCode MAP[ROWS][COLS] = {
        {KeyCode::Escape, KeyCode::Divide, KeyCode::Multiply, KeyCode::Minus, KeyCode::Sin, KeyCode::Sec},
        {KeyCode::K7, KeyCode::K8, KeyCode::K9, KeyCode::Plus, KeyCode::Cos, KeyCode::Csc},
        {KeyCode::K4, KeyCode::K5, KeyCode::K6, KeyCode::Power, KeyCode::Tan, KeyCode::Cot},
        {KeyCode::K1, KeyCode::K2, KeyCode::K3, KeyCode::Enter, KeyCode::Exp, KeyCode::Extra},
        {KeyCode::Shift, KeyCode::K0, KeyCode::Dot, KeyCode::Backspace, KeyCode::Angle, KeyCode::Tab}
    };
    return MAP[row][col];
}

uint8_t Keyboard::indexFor(KeyCode key) {
    for (uint8_t row = 0; row < ROWS; ++row) {
        for (uint8_t col = 0; col < COLS; ++col) {
            if (keyFor(row, col) == key) return row * COLS + col;
        }
    }
    return 0xFF;
}

void Keyboard::push(KeyCode key, KeyAction action) {
    uint8_t next = (queueHead_ + 1) % QUEUE_SIZE;
    if (next == queueTail_) return;
    queue_[queueHead_].key = key;
    queue_[queueHead_].action = action;
    queueHead_ = next;
}

void Keyboard::update() {
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
                    push(keyFor(row, col), KeyAction::Press);
                } else {
                    push(keyFor(row, col), KeyAction::Release);
                }
            }
            if (state.stable && now - state.pressedAt >= REPEAT_DELAY_MS &&
                now - state.repeatedAt >= REPEAT_RATE_MS) {
                state.repeatedAt = now;
                push(keyFor(row, col), KeyAction::Repeat);
            }
        }
        digitalWrite(COL_PINS[col], LOW);
    }
}

bool Keyboard::nextEvent(KeyEvent &event) {
    if (queueTail_ == queueHead_) return false;
    event = queue_[queueTail_];
    queueTail_ = (queueTail_ + 1) % QUEUE_SIZE;
    return true;
}

bool Keyboard::isDown(KeyCode key) const {
    const uint8_t index = indexFor(key);
    return index != 0xFF && states_[index].stable;
}

uint32_t Keyboard::heldMs(KeyCode key) const {
    const uint8_t index = indexFor(key);
    if (index == 0xFF || !states_[index].stable) return 0;
    return millis() - states_[index].pressedAt;
}
