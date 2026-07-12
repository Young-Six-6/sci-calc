#pragma once

#include <Arduino.h>

enum class KeyCode : uint8_t {
    None,
    Escape, Divide, Multiply, Minus, Sin, Sec,
    K7, K8, K9, Plus, Cos, Csc,
    K4, K5, K6, Power, Tan, Cot,
    K1, K2, K3, Enter, Exp, Extra,
    Shift, K0, Dot, Backspace, Angle, Tab
};

enum class KeyAction : uint8_t { Press, Release, Repeat };

struct KeyEvent {
    KeyCode key = KeyCode::None;
    KeyAction action = KeyAction::Press;
};

class Keyboard {
public:
    void begin();
    void update();
    bool nextEvent(KeyEvent &event);
    bool isDown(KeyCode key) const;
    uint32_t heldMs(KeyCode key) const;

private:
    static constexpr uint8_t ROWS = 5;
    static constexpr uint8_t COLS = 6;
    static constexpr uint8_t KEY_COUNT = ROWS * COLS;
    static constexpr uint8_t QUEUE_SIZE = 24;

    struct State {
        bool raw = false;
        bool stable = false;
        uint32_t changedAt = 0;
        uint32_t pressedAt = 0;
        uint32_t repeatedAt = 0;
    };

    State states_[KEY_COUNT];
    KeyEvent queue_[QUEUE_SIZE];
    uint8_t queueHead_ = 0;
    uint8_t queueTail_ = 0;
    uint32_t lastScan_ = 0;

    static uint8_t indexFor(KeyCode key);
    static KeyCode keyFor(uint8_t row, uint8_t col);
    void push(KeyCode key, KeyAction action);
};
