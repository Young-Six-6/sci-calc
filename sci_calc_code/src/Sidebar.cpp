#include "Sidebar.h"

int curtime = 0;
std::string batterylevel;

void drawSidebar() {
    if (HIDE_SIDEBAR == false) {
        u8g2.setDrawColor(0);
        u8g2.drawBox(MAIN_PANEL_WIDTH, 0, WIDTH - MAIN_PANEL_WIDTH, HEIGHT);
        u8g2.setDrawColor(1);
        u8g2.drawRFrame(SIDEBAR_LEFT, 0, SIDEBAR_WIDTH, HEIGHT, 2);
        if (batterylevel.empty() || millis() - curtime > 500) {
            batterylevel = numToStr(analogRead(36) / 4095.0 * 2 * 3.3).substr(0, 3);
            curtime = millis();
        }

        u8g2.drawFrame(216, 4, 8, 7);
        u8g2.drawBox(224, 6, 2, 3);
        u8g2.drawStr(228, 10, (batterylevel + "V").c_str());
    }
}
