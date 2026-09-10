#pragma once

#include <Arduino.h>

enum class KeyboardKey : uint8_t {
    None,
    Escape, Divide, Multiply, Minus, Sin, Sec,
    K7, K8, K9, Plus, Cos, Csc,
    K4, K5, K6, Power, Tan, Cot,
    K1, K2, K3, Enter, Exp, Extra,
    Shift, K0, Dot, Backspace, Angle, Tab
};

enum class KeyboardAction : uint8_t { Press, Release, Repeat };

struct KeyboardEvent {
    KeyboardKey key = KeyboardKey::None;
    KeyboardAction action = KeyboardAction::Press;
};

class SafeKeyboard {
public:
    void begin();
    void update();
    bool nextEvent(KeyboardEvent &event);
    bool isDown(KeyboardKey key) const;
    uint32_t heldMs(KeyboardKey key) const;
    static int8_t physicalColumn(KeyboardKey key);

private:
    static constexpr uint8_t ROWS = 5;
    static constexpr uint8_t COLS = 6;
    static constexpr uint8_t KEY_COUNT = ROWS * COLS;
    static constexpr uint8_t QUEUE_SIZE = 32;

    struct State {
        bool raw = false;
        bool stable = false;
        uint32_t changedAt = 0;
        uint32_t pressedAt = 0;
        uint32_t repeatedAt = 0;
    };

    State states_[KEY_COUNT];
    KeyboardEvent queue_[QUEUE_SIZE];
    uint8_t queueHead_ = 0;
    uint8_t queueTail_ = 0;
    uint32_t lastScan_ = 0;

    static uint8_t indexFor(KeyboardKey key);
    static KeyboardKey keyFor(uint8_t row, uint8_t col);
    void push(KeyboardKey key, KeyboardAction action);
};
