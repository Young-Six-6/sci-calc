#ifndef POWER_SETTINGS_H
#define POWER_SETTINGS_H

#include "../UIElements/UIElement.h"

class PowerSettings : public UIElement {
    public:
        PowerSettings();

        void init() override;
        void activate() override;
        void draw() override;
        void update() override;

    private:
        void drawRow(uint8_t row, const char *label, const char *value);

        uint8_t selectedRow = 0;
};

#endif
