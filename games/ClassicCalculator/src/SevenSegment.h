#pragma once

#include <U8g2lib.h>

class SevenSegment {
public:
    static void drawRightAligned(U8G2 &display, const char *text,
                                 int16_t right, int16_t top,
                                 int16_t availableWidth, int16_t availableHeight);

private:
    static uint8_t segmentMask(char value);
    static int16_t glyphWidth(char value, int16_t digitWidth, int16_t thickness);
    static int16_t textWidth(const char *text, int16_t digitWidth, int16_t thickness, int16_t gap);
    static void drawGlyph(U8G2 &display, char value, int16_t x, int16_t y,
                          int16_t digitWidth, int16_t height, int16_t thickness);
};
