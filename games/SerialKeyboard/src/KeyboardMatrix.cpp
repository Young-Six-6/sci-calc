#include "KeyboardMatrix.h"

namespace {
constexpr uint8_t ROW_PINS[5] = {33, 32, 35, 34, 39};
constexpr uint8_t COL_PINS[6] = {13, 12, 14, 27, 26, 25};
constexpr uint32_t SCAN_INTERVAL_MS = 4;
constexpr uint32_t DEBOUNCE_MS = 16;

const KeyCode KEY_MAP[KeyboardMatrix::ROWS][KeyboardMatrix::COLS] = {
    {KeyCode::Escape, KeyCode::Divide, KeyCode::Multiply, KeyCode::Minus, KeyCode::Sin, KeyCode::Sec},
    {KeyCode::K7, KeyCode::K8, KeyCode::K9, KeyCode::Plus, KeyCode::Cos, KeyCode::Csc},
    {KeyCode::K4, KeyCode::K5, KeyCode::K6, KeyCode::Power, KeyCode::Tan, KeyCode::Cot},
    {KeyCode::K1, KeyCode::K2, KeyCode::K3, KeyCode::Enter, KeyCode::Exp, KeyCode::RpnNorm},
    {KeyCode::Shift, KeyCode::K0, KeyCode::Dot, KeyCode::Backspace, KeyCode::Angle, KeyCode::Tab}
};

const char *KEY_NAMES[KeyboardMatrix::KEY_COUNT] = {
    "ESC", "/", "*", "-", "SIN", "SEC",
    "7", "8", "9", "+", "COS", "CSC",
    "4", "5", "6", "^", "TAN", "COT",
    "1", "2", "3", "ENTER", "EXP", "RPN/NORM",
    "SHIFT", "0", ".", "BACKSPACE", "ANGLE", "TAB"
};
}

void KeyboardMatrix::begin() {
    for (uint8_t row = 0; row < ROWS; ++row) pinMode(ROW_PINS[row], INPUT);
    for (uint8_t col = 0; col < COLS; ++col) {
        pinMode(COL_PINS[col], OUTPUT);
        digitalWrite(COL_PINS[col], LOW);
    }
}

KeyCode KeyboardMatrix::keyFor(uint8_t row, uint8_t col) {
    return row < ROWS && col < COLS ? KEY_MAP[row][col] : KeyCode::None;
}

uint8_t KeyboardMatrix::keyId(KeyCode key) {
    const uint8_t value = static_cast<uint8_t>(key);
    return value < KEY_COUNT ? value : 0xFF;
}

uint8_t KeyboardMatrix::rowFor(KeyCode key) {
    const uint8_t id = keyId(key);
    return id == 0xFF ? 0xFF : id / COLS;
}

uint8_t KeyboardMatrix::colFor(KeyCode key) {
    const uint8_t id = keyId(key);
    return id == 0xFF ? 0xFF : id % COLS;
}

const char *KeyboardMatrix::keyName(KeyCode key) {
    const uint8_t id = keyId(key);
    return id == 0xFF ? "UNKNOWN" : KEY_NAMES[id];
}

uint32_t KeyboardMatrix::stateMask() const {
    uint32_t mask = 0;
    for (uint8_t index = 0; index < KEY_COUNT; ++index) {
        if (states_[index].stable) mask |= 1UL << index;
    }
    return mask;
}

void KeyboardMatrix::push(KeyCode key, KeyAction action, uint32_t now, uint32_t held) {
    const uint8_t next = static_cast<uint8_t>((queueHead_ + 1U) % QUEUE_SIZE);
    if (next == queueTail_) {
        ++droppedEvents_;
        return;
    }

    KeyEvent &event = queue_[queueHead_];
    event.key = key;
    event.action = action;
    event.row = rowFor(key);
    event.col = colFor(key);
    event.timestampMs = now;
    event.heldMs = held;
    event.stateMask = stateMask();
    queueHead_ = next;
}

void KeyboardMatrix::update() {
    const uint32_t now = millis();
    if (now - lastScan_ < SCAN_INTERVAL_MS) return;
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
            if (raw == state.stable || now - state.changedAt < DEBOUNCE_MS) continue;

            state.stable = raw;
            const KeyCode key = keyFor(row, col);
            if (raw) {
                state.pressedAt = now;
                push(key, KeyAction::Press, now, 0);
            } else {
                push(key, KeyAction::Release, now, now - state.pressedAt);
            }
        }
        digitalWrite(COL_PINS[col], LOW);
    }
}

bool KeyboardMatrix::nextEvent(KeyEvent &event) {
    if (queueTail_ == queueHead_) return false;
    event = queue_[queueTail_];
    queueTail_ = static_cast<uint8_t>((queueTail_ + 1U) % QUEUE_SIZE);
    return true;
}

bool KeyboardMatrix::isDown(KeyCode key) const {
    const uint8_t id = keyId(key);
    return id != 0xFF && states_[id].stable;
}

uint32_t KeyboardMatrix::heldMs(KeyCode key) const {
    const uint8_t id = keyId(key);
    if (id == 0xFF || !states_[id].stable) return 0;
    return millis() - states_[id].pressedAt;
}

uint32_t KeyboardMatrix::droppedEvents() const {
    return droppedEvents_;
}
