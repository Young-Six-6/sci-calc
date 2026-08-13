#pragma once

#include <Arduino.h>

enum class PlayerKey : uint8_t {
    None,
    Escape, Divide, Multiply, Minus, Sin, Sec,
    K7, K8, K9, Plus, Cos, Csc,
    K4, K5, K6, Power, Tan, Cot,
    K1, K2, K3, Enter, Exp, Extra,
    Shift, K0, Dot, Backspace, Angle, Tab
};

enum class PlayerKeyAction : uint8_t { Press, Release, Repeat };

struct PlayerKeyEvent {
    PlayerKey key = PlayerKey::None;
    PlayerKeyAction action = PlayerKeyAction::Press;
};

class SafeKeyboard {
public:
    void begin();
    void update();
    bool nextEvent(PlayerKeyEvent &event);
    bool isDown(PlayerKey key) const;
    uint32_t heldMs(PlayerKey key) const;

private:
    static constexpr uint8_t ROWS = 5;
    static constexpr uint8_t COLS = 6;
    static constexpr uint8_t KEY_COUNT = ROWS * COLS;
    static constexpr uint8_t QUEUE_SIZE = 20;

    struct State {
        bool raw = false;
        bool stable = false;
        uint32_t changedAt = 0;
        uint32_t pressedAt = 0;
        uint32_t repeatedAt = 0;
    };

    State states_[KEY_COUNT];
    PlayerKeyEvent queue_[QUEUE_SIZE];
    uint8_t queueHead_ = 0;
    uint8_t queueTail_ = 0;
    uint32_t lastScan_ = 0;

    static uint8_t indexFor(PlayerKey key);
    static PlayerKey keyFor(uint8_t row, uint8_t col);
    void push(PlayerKey key, PlayerKeyAction action);
};
