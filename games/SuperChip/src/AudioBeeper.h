#pragma once

#include <Arduino.h>

class AudioBeeper {
public:
    bool begin();
    void setEnabled(bool enabled) { toneEnabled_ = enabled; }
    bool available() const { return ready_; }

private:
    static void taskEntry(void *context);
    void run();

    volatile bool toneEnabled_ = false;
    bool ready_ = false;
};
