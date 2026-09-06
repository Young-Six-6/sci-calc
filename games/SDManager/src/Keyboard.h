#pragma once

#include <Arduino.h>

enum class ManagerKey : uint8_t {
    None,
    Escape, Divide, Multiply, Minus, Sin, Sec,
    K7, K8, K9, Plus, Cos, Csc,
    K4, K5, K6, Power, Tan, Cot,
    K1, K2, K3, Enter, Exp, Extra,
    Shift, K0, Dot, Backspace, Angle, Tab
};

class Keyboard {
public:
    void begin();
    void update();
    bool isDown(ManagerKey key) const;
    uint32_t heldMs(ManagerKey key) const;

private:
    static constexpr uint8_t ROWS = 5;
    static constexpr uint8_t COLS = 6;
    static constexpr uint8_t KEY_COUNT = ROWS * COLS;

    struct State {
        bool raw = false;
        bool stable = false;
        uint32_t changedAt = 0;
        uint32_t pressedAt = 0;
    };

    State states_[KEY_COUNT] = {};
    uint32_t lastScan_ = 0;

    static uint8_t indexFor(ManagerKey key);
    static ManagerKey keyFor(uint8_t row, uint8_t col);
};
