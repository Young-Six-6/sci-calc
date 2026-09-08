#pragma once

#include <Arduino.h>

enum class KeyCode : uint8_t {
    Escape = 0,
    Slash = 1,
    Star = 2,
    Minus = 3,
    K7 = 6,
    K8 = 7,
    K9 = 8,
    Plus = 9,
    K4 = 12,
    K5 = 13,
    K6 = 14,
    Power = 15,
    K1 = 18,
    K2 = 19,
    K3 = 20,
    Enter = 21,
    Layer = 24,
    K0 = 25,
    Dot = 26,
    Backspace = 27,
    None = 0xFF
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
    static constexpr uint8_t QUEUE_SIZE = 32;

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
