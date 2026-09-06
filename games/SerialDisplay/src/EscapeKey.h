#pragma once

#include <Arduino.h>

class EscapeKey {
public:
    void begin();
    void update();
    bool isDown() const;
    uint32_t heldMs() const;

private:
    bool raw_ = false;
    bool stable_ = false;
    uint32_t changedAt_ = 0;
    uint32_t pressedAt_ = 0;
    uint32_t lastScan_ = 0;
};
