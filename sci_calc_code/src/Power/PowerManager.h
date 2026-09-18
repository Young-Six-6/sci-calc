#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>

class Keyboard;

class PowerManager {
    public:
        void begin();
        bool update(Keyboard &keyboard);

        uint8_t getDisplaySleepIndex() const;
        uint8_t getAutoOffIndex() const;
        const char *getDisplaySleepLabel() const;
        const char *getAutoOffLabel() const;

        void changeDisplaySleep(int direction);
        void changeAutoOff(int direction);

    private:
        void saveSettings();
        void enterDisplaySleep();
        void leaveDisplaySleep();
        [[noreturn]] void enterDeepSleep();

        uint8_t displaySleepIndex = 0;
        uint8_t autoOffIndex = 0;
        uint32_t lastActivityMs = 0;
        uint32_t normalCpuMhz = 240;
        bool displaySleeping = false;
        bool waitingForWakeKeyRelease = false;
};

extern PowerManager powerManager;

#endif
