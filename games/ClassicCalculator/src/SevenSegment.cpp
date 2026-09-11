#include "SevenSegment.h"

#include <cctype>
#include <cstring>

namespace {
constexpr uint8_t SEG_A = 1 << 0;
constexpr uint8_t SEG_B = 1 << 1;
constexpr uint8_t SEG_C = 1 << 2;
constexpr uint8_t SEG_D = 1 << 3;
constexpr uint8_t SEG_E = 1 << 4;
constexpr uint8_t SEG_F = 1 << 5;
constexpr uint8_t SEG_G = 1 << 6;

void horizontal(U8G2 &display, int16_t x, int16_t y, int16_t width, int16_t thickness) {
    if (width <= thickness * 2) {
        display.drawBox(x, y, width, thickness);
        return;
    }
    display.drawBox(x + thickness / 2, y, width - thickness, thickness);
    display.drawTriangle(x, y + thickness / 2, x + thickness / 2, y,
                         x + thickness / 2, y + thickness - 1);
    display.drawTriangle(x + width - 1, y + thickness / 2,
                         x + width - thickness / 2 - 1, y,
                         x + width - thickness / 2 - 1, y + thickness - 1);
}

void vertical(U8G2 &display, int16_t x, int16_t y, int16_t height, int16_t thickness) {
    if (height <= thickness * 2) {
        display.drawBox(x, y, thickness, height);
        return;
    }
    display.drawBox(x, y + thickness / 2, thickness, height - thickness);
    display.drawTriangle(x + thickness / 2, y, x, y + thickness / 2,
                         x + thickness - 1, y + thickness / 2);
    display.drawTriangle(x + thickness / 2, y + height - 1,
                         x, y + height - thickness / 2 - 1,
                         x + thickness - 1, y + height - thickness / 2 - 1);
}
}

uint8_t SevenSegment::segmentMask(char value) {
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(value)))) {
        case '0': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;
        case '1': return SEG_B | SEG_C;
        case '2': return SEG_A | SEG_B | SEG_D | SEG_E | SEG_G;
        case '3': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_G;
        case '4': return SEG_B | SEG_C | SEG_F | SEG_G;
        case '5': return SEG_A | SEG_C | SEG_D | SEG_F | SEG_G;
        case '6': return SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;
        case '7': return SEG_A | SEG_B | SEG_C;
        case '8': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;
        case '9': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G;
        case 'E': return SEG_A | SEG_D | SEG_E | SEG_F | SEG_G;
        case '-': return SEG_G;
        default: return 0;
    }
}

int16_t SevenSegment::glyphWidth(char value, int16_t digitWidth, int16_t thickness) {
    if (value == '.') return thickness + 1;
    if (value == ',') return thickness + 2;
    return digitWidth;
}

int16_t SevenSegment::textWidth(const char *text, int16_t digitWidth, int16_t thickness, int16_t gap) {
    if (!text || !*text) return 0;
    int16_t width = 0;
    for (const char *cursor = text; *cursor; ++cursor) width += glyphWidth(*cursor, digitWidth, thickness) + gap;
    return width - gap;
}

void SevenSegment::drawGlyph(U8G2 &display, char value, int16_t x, int16_t y,
                             int16_t digitWidth, int16_t height, int16_t thickness) {
    if (value == '.') {
        display.drawBox(x, y + height - thickness, thickness, thickness);
        return;
    }
    if (value == ',') {
        display.drawBox(x, y + height - thickness, thickness, thickness);
        display.drawPixel(x, y + height);
        return;
    }

    const uint8_t mask = segmentMask(value);
    const int16_t half = height / 2;
    const int16_t horizontalWidth = digitWidth - thickness;
    const int16_t verticalHeight = half - thickness;
    if (mask & SEG_A) horizontal(display, x + thickness / 2, y, horizontalWidth, thickness);
    if (mask & SEG_G) horizontal(display, x + thickness / 2, y + half - thickness / 2, horizontalWidth, thickness);
    if (mask & SEG_D) horizontal(display, x + thickness / 2, y + height - thickness, horizontalWidth, thickness);
    if (mask & SEG_F) vertical(display, x, y + thickness / 2, verticalHeight, thickness);
    if (mask & SEG_B) vertical(display, x + digitWidth - thickness, y + thickness / 2, verticalHeight, thickness);
    if (mask & SEG_E) vertical(display, x, y + half, verticalHeight, thickness);
    if (mask & SEG_C) vertical(display, x + digitWidth - thickness, y + half, verticalHeight, thickness);
}

void SevenSegment::drawRightAligned(U8G2 &display, const char *text,
                                    int16_t right, int16_t top,
                                    int16_t availableWidth, int16_t availableHeight) {
    if (!text || !*text || availableWidth <= 0 || availableHeight <= 0) return;

    int16_t height = availableHeight;
    int16_t thickness = height >= 30 ? 3 : 2;
    int16_t digitWidth = height * 5 / 9;
    int16_t gap = thickness;
    while (height > 17 && textWidth(text, digitWidth, thickness, gap) > availableWidth) {
        --height;
        thickness = height >= 30 ? 3 : 2;
        digitWidth = height * 5 / 9;
        gap = thickness;
    }

    const int16_t width = textWidth(text, digitWidth, thickness, gap);
    int16_t x = right - width;
    const int16_t y = top + (availableHeight - height) / 2;
    for (const char *cursor = text; *cursor; ++cursor) {
        drawGlyph(display, *cursor, x, y, digitWidth, height, thickness);
        x += glyphWidth(*cursor, digitWidth, thickness) + gap;
    }
}
