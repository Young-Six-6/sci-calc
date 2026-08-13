#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <U8g2lib.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Keyboard.h"
#include "Parser.h"

namespace {
constexpr uint8_t OLED_CS = 5;
constexpr uint8_t OLED_DC = 16;
constexpr uint8_t OLED_RST = 17;
constexpr uint8_t SD_CS = 4;
constexpr uint16_t SCREEN_W = 256;
constexpr uint8_t SCREEN_H = 64;
constexpr uint8_t MAX_EXPRESSION = 96;
constexpr uint8_t MAX_FUNCTIONS = 4;
constexpr uint32_t EXIT_HOLD_MS = 1000;
constexpr uint16_t MIN_FRAME_INTERVAL_MS = 16;
constexpr uint16_t GRAPH_CACHE_BYTES = SCREEN_W * SCREEN_H / 8;
constexpr uint16_t MAX_FRAME_EVALUATIONS = 4096;
constexpr uint16_t MAX_FUNCTION_EVALUATIONS = 1536;
constexpr uint16_t BASE_FUNCTION_EVALUATIONS = 1 + (SCREEN_W - 1) * 3;
constexpr uint8_t MAX_ADAPTIVE_DEPTH = 3;
constexpr double CURVE_ERROR_PIXELS = 0.65;
constexpr double CONTINUITY_ERROR_PIXELS = 1.15;
static_assert(MAX_FRAME_EVALUATIONS >= BASE_FUNCTION_EVALUATIONS * MAX_FUNCTIONS,
              "Frame budget must cover all baseline samples");

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI display(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
Keyboard keyboard;

struct FunctionSlot {
    String expression;
    uint8_t cursor = 0;
    Parser parser;
    ParseError error;
    bool valid = false;
};

FunctionSlot functions[MAX_FUNCTIONS];
uint8_t functionCount = 1;
uint8_t activeFunction = 0;

enum class Page : uint8_t { Editor, Graph };
Page page = Page::Editor;
bool degrees = false;
bool gridEnabled = true;
bool labelsEnabled = true;
bool traceEnabled = false;
bool sdReady = false;
bool exitTriggered = false;

double centerX = 0.0;
double centerY = 0.0;
double pixelsPerUnit = 20.0;
double traceX = 0.0;

String notice;
uint32_t noticeUntil = 0;
uint32_t lastFrame = 0;
bool dirty = true;
bool graphBaseDirty = true;
bool graphBaseValid = false;
uint8_t graphBaseBuffer[GRAPH_CACHE_BYTES];

struct CurveSample {
    double screenX = 0.0;
    double screenY = 0.0;
    bool valid = false;
};

struct EvaluationBudget {
    uint16_t used = 0;
    uint16_t limit = 0;
};

FunctionSlot &currentFunction() {
    return functions[activeFunction];
}

void setNotice(const String &text, uint32_t duration = 1800) {
    notice = text;
    noticeUntil = millis() + duration;
    dirty = true;
}

bool noticeActive() {
    return notice.length() && static_cast<int32_t>(noticeUntil - millis()) > 0;
}

void markGraphBaseDirty() {
    graphBaseDirty = true;
    graphBaseValid = false;
    dirty = true;
}

void resetView() {
    centerX = 0.0;
    centerY = 0.0;
    pixelsPerUnit = 20.0;
    traceX = 0.0;
    markGraphBaseDirty();
}

String shortNumber(double value) {
    char buffer[20];
    if (fabs(value) < 5e-10) value = 0.0;
    snprintf(buffer, sizeof(buffer), "%.5g", value);
    return String(buffer);
}

bool parserSelfTest() {
    Parser testParser;
    ParseError error;
    double value = 0.0;
    if (!testParser.compile("2x", error) || !testParser.evaluate(3.0, false, value) || fabs(value - 6.0) > 1e-9) return false;
    if (!testParser.compile("2(x+1)", error) || !testParser.evaluate(2.0, false, value) || fabs(value - 6.0) > 1e-9) return false;
    if (!testParser.compile("(x+1)(x-1)", error) || !testParser.evaluate(3.0, false, value) || fabs(value - 8.0) > 1e-9) return false;
    if (!testParser.compile("sin(pi/2)", error) || !testParser.evaluate(0.0, false, value) || fabs(value - 1.0) > 1e-9) return false;
    if (testParser.compile("1.2.3", error)) return false;
    return true;
}

void insertText(const String &text) {
    FunctionSlot &slot = currentFunction();
    if (slot.expression.length() + text.length() > MAX_EXPRESSION) {
        setNotice("Input limit: 96 chars");
        return;
    }
    slot.expression = slot.expression.substring(0, slot.cursor) + text + slot.expression.substring(slot.cursor);
    slot.cursor += text.length();
    slot.valid = false;
    slot.error = ParseError();
    dirty = true;
}

void eraseLeft() {
    FunctionSlot &slot = currentFunction();
    if (slot.cursor == 0) return;
    slot.expression.remove(slot.cursor - 1, 1);
    --slot.cursor;
    slot.valid = false;
    slot.error = ParseError();
    dirty = true;
}

void selectFunction(int direction) {
    activeFunction = (activeFunction + functionCount + direction) % functionCount;
    setNotice("Editing F" + String(activeFunction + 1), 900);
}

void selectGraphFunction(int direction) {
    for (uint8_t offset = 1; offset <= functionCount; ++offset) {
        const int candidate = (activeFunction + functionCount + direction * offset) % functionCount;
        if (functions[candidate].valid) {
            activeFunction = candidate;
            setNotice("Trace F" + String(activeFunction + 1), 700);
            return;
        }
    }
}

void addFunction() {
    if (functionCount >= MAX_FUNCTIONS) {
        setNotice("Maximum: 4 functions");
        return;
    }
    activeFunction = functionCount++;
    functions[activeFunction].expression = "";
    functions[activeFunction].cursor = 0;
    functions[activeFunction].valid = false;
    functions[activeFunction].error = ParseError();
    setNotice("Added F" + String(activeFunction + 1), 900);
}

void deleteFunction() {
    if (functionCount == 1) {
        functions[0].expression = "";
        functions[0].cursor = 0;
        functions[0].valid = false;
        functions[0].error = ParseError();
        setNotice("F1 cleared", 900);
        return;
    }

    for (uint8_t i = activeFunction; i + 1 < functionCount; ++i) {
        functions[i].expression = functions[i + 1].expression;
        functions[i].cursor = functions[i + 1].cursor;
        functions[i].valid = false;
        functions[i].error = ParseError();
    }
    --functionCount;
    functions[functionCount].expression = "";
    functions[functionCount].cursor = 0;
    functions[functionCount].valid = false;
    functions[functionCount].error = ParseError();
    if (activeFunction >= functionCount) activeFunction = functionCount - 1;
    setNotice("Function deleted", 900);
}

bool compileFunctions() {
    bool hasFunction = false;
    for (uint8_t i = 0; i < functionCount; ++i) {
        FunctionSlot &slot = functions[i];
        slot.error = ParseError();
        if (slot.expression.length() == 0) {
            slot.valid = false;
            continue;
        }
        hasFunction = true;
        slot.valid = slot.parser.compile(slot.expression, slot.error);
        if (!slot.valid) {
            activeFunction = i;
            setNotice("Syntax error in F" + String(i + 1));
            return false;
        }
    }
    markGraphBaseDirty();
    if (!hasFunction) {
        setNotice("Enter at least one function");
        return false;
    }
    return true;
}

void enterGraph() {
    if (!compileFunctions()) return;
    page = Page::Graph;
    traceEnabled = false;
    dirty = true;
}

bool installMainFirmware() {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(4, 12, "Returning to main firmware...");
    display.drawFrame(4, 25, 248, 10);
    display.sendBuffer();

    if (!sdReady) {
        setNotice("SD card unavailable", 2500);
        return false;
    }
    File firmware = SD.open("/main.bin", FILE_READ);
    if (!firmware || firmware.isDirectory() || firmware.size() == 0) {
        if (firmware) firmware.close();
        setNotice("Missing /main.bin", 2500);
        return false;
    }

    const size_t size = firmware.size();
    if (!Update.begin(size, U_FLASH)) {
        firmware.close();
        setNotice(String("Update begin failed: ") + Update.getError(), 3000);
        return false;
    }

    size_t written = 0;
    uint8_t lastPercent = 255;
    uint8_t buffer[1024];
    while (firmware.available()) {
        const size_t count = firmware.read(buffer, sizeof(buffer));
        if (!count || Update.write(buffer, count) != count) break;
        written += count;
        const uint8_t percent = static_cast<uint8_t>((written * 100ULL) / size);
        if (percent != lastPercent) {
            lastPercent = percent;
            display.setDrawColor(0);
            display.drawBox(5, 26, 246, 8);
            display.setDrawColor(1);
            display.drawBox(5, 26, (percent * 246) / 100, 8);
            display.sendBuffer();
        }
    }
    firmware.close();

    if (written != size || !Update.end(true) || !Update.isFinished()) {
        Update.abort();
        setNotice(String("Update failed: ") + Update.getError(), 3000);
        return false;
    }
    display.clearBuffer();
    display.drawStr(67, 34, "Main firmware restored");
    display.sendBuffer();
    delay(250);
    ESP.restart();
    return true;
}

void drawDottedVertical(int x) {
    if (x < 0 || x >= SCREEN_W) return;
    for (int y = 0; y < SCREEN_H; y += 3) display.drawPixel(x, y);
}

void drawDottedHorizontal(int y) {
    if (y < 0 || y >= SCREEN_H) return;
    for (int x = 0; x < SCREEN_W; x += 3) display.drawPixel(x, y);
}

double niceGridStep() {
    const double raw = 18.0 / pixelsPerUnit;
    const double power = pow(10.0, floor(log10(raw)));
    const double scaled = raw / power;
    const double nice = scaled <= 1.0 ? 1.0 : (scaled <= 2.0 ? 2.0 : (scaled <= 5.0 ? 5.0 : 10.0));
    return nice * power;
}

void drawGridAndAxes() {
    const double minX = centerX - SCREEN_W * 0.5 / pixelsPerUnit;
    const double maxX = centerX + SCREEN_W * 0.5 / pixelsPerUnit;
    const double minY = centerY - SCREEN_H * 0.5 / pixelsPerUnit;
    const double maxY = centerY + SCREEN_H * 0.5 / pixelsPerUnit;
    const double step = niceGridStep();

    if (gridEnabled) {
        double firstX = ceil(minX / step) * step;
        for (double x = firstX; x <= maxX + step * 0.1; x += step) {
            int px = lround(SCREEN_W * 0.5 + (x - centerX) * pixelsPerUnit);
            drawDottedVertical(px);
        }
        double firstY = ceil(minY / step) * step;
        for (double y = firstY; y <= maxY + step * 0.1; y += step) {
            int py = lround(SCREEN_H * 0.5 - (y - centerY) * pixelsPerUnit);
            drawDottedHorizontal(py);
        }
    }

    const int axisX = lround(SCREEN_W * 0.5 - centerX * pixelsPerUnit);
    const int axisY = lround(SCREEN_H * 0.5 + centerY * pixelsPerUnit);
    if (axisX >= 0 && axisX < SCREEN_W) display.drawVLine(axisX, 0, SCREEN_H);
    if (axisY >= 0 && axisY < SCREEN_H) display.drawHLine(0, axisY, SCREEN_W);

    if (axisY >= 0 && axisY < SCREEN_H) {
        for (double x = ceil(minX / step) * step; x <= maxX; x += step) {
            int px = lround(SCREEN_W * 0.5 + (x - centerX) * pixelsPerUnit);
            if (px >= 0 && px < SCREEN_W) display.drawVLine(px, max(0, axisY - 2), min(5, SCREEN_H - max(0, axisY - 2)));
        }
    }
    if (axisX >= 0 && axisX < SCREEN_W) {
        for (double y = ceil(minY / step) * step; y <= maxY; y += step) {
            int py = lround(SCREEN_H * 0.5 - (y - centerY) * pixelsPerUnit);
            if (py >= 0 && py < SCREEN_H) display.drawHLine(max(0, axisX - 2), py, min(5, SCREEN_W - max(0, axisX - 2)));
        }
    }
}

void drawSmallLabel(const String &text, int center, int baseline) {
    const int width = display.getStrWidth(text.c_str());
    int x = constrain(center - width / 2, 0, SCREEN_W - width);
    int y = constrain(baseline, 6, SCREEN_H - 1);
    display.setDrawColor(0);
    display.drawBox(max(0, x - 1), y - 6, min(width + 2, SCREEN_W - max(0, x - 1)), 7);
    display.setDrawColor(1);
    display.drawStr(x, y, text.c_str());
}

void drawAxisLabels() {
    if (!labelsEnabled) return;

    const double minX = centerX - SCREEN_W * 0.5 / pixelsPerUnit;
    const double maxX = centerX + SCREEN_W * 0.5 / pixelsPerUnit;
    const double minY = centerY - SCREEN_H * 0.5 / pixelsPerUnit;
    const double maxY = centerY + SCREEN_H * 0.5 / pixelsPerUnit;
    const double step = niceGridStep();
    const int axisX = lround(SCREEN_W * 0.5 - centerX * pixelsPerUnit);
    const int axisY = lround(SCREEN_H * 0.5 + centerY * pixelsPerUnit);

    display.setFont(u8g2_font_4x6_tf);
    if (axisY >= 0 && axisY < SCREEN_H) {
        const int baseline = axisY < 9 ? axisY + 7 : axisY - 2;
        int tickIndex = 0;
        for (double x = ceil(minX / step) * step; x <= maxX + step * 0.1; x += step, ++tickIndex) {
            if (fabs(x) < step * 0.05 || tickIndex % 2 != 0) continue;
            const int px = lround(SCREEN_W * 0.5 + (x - centerX) * pixelsPerUnit);
            if (px >= 8 && px < SCREEN_W - 8) drawSmallLabel(shortNumber(x), px, baseline);
        }
    }
    if (axisX >= 0 && axisX < SCREEN_W) {
        const bool placeRight = axisX < SCREEN_W - 24;
        for (double y = ceil(minY / step) * step; y <= maxY + step * 0.1; y += step) {
            if (fabs(y) < step * 0.05) continue;
            const int py = lround(SCREEN_H * 0.5 - (y - centerY) * pixelsPerUnit);
            if (py < 7 || py >= SCREEN_H - 1) continue;
            String label = shortNumber(y);
            const int labelWidth = display.getStrWidth(label.c_str());
            const int center = placeRight ? axisX + 3 + labelWidth / 2 : axisX - 3 - labelWidth / 2;
            drawSmallLabel(label, center, py + 2);
        }
    }
    if (axisX >= 0 && axisX < SCREEN_W && axisY >= 0 && axisY < SCREEN_H) {
        drawSmallLabel("0", axisX + 5, axisY < 9 ? axisY + 7 : axisY - 2);
    }
}

bool styleVisible(uint8_t style, int x) {
    if (style == 0) return true;
    if (style == 1) return x % 4 < 2;
    if (style == 2) return x % 7 < 4;
    return x % 9 < 5 || x % 9 == 7;
}

bool evaluateCurveSample(const FunctionSlot &slot, double screenX, CurveSample &sample,
                         EvaluationBudget &budget) {
    sample.screenX = screenX;
    sample.valid = false;
    if (budget.used >= budget.limit) return false;
    ++budget.used;

    const double x = centerX + (screenX - SCREEN_W * 0.5) / pixelsPerUnit;
    double y = 0.0;
    if (!slot.parser.evaluate(x, degrees, y)) return true;
    sample.screenY = SCREEN_H * 0.5 - (y - centerY) * pixelsPerUnit;
    sample.valid = isfinite(sample.screenY);
    return true;
}

bool sampleVisible(const CurveSample &sample) {
    return sample.valid && sample.screenY >= 0.0 && sample.screenY < SCREEN_H;
}

void drawCurvePoint(const CurveSample &sample, uint8_t style) {
    if (!sampleVisible(sample)) return;
    const int x = constrain(static_cast<int>(lround(sample.screenX)), 0, SCREEN_W - 1);
    if (styleVisible(style, x)) display.drawPixel(x, lround(sample.screenY));
}

void drawClippedCurveSegment(const CurveSample &left, const CurveSample &right, uint8_t style) {
    if (!left.valid || !right.valid) return;
    const double maxY = SCREEN_H - 1.0;
    if ((left.screenY < 0.0 && right.screenY < 0.0) ||
        (left.screenY > maxY && right.screenY > maxY)) return;

    const double deltaY = right.screenY - left.screenY;
    double start = 0.0;
    double end = 1.0;
    if (fabs(deltaY) < 1e-12) {
        if (left.screenY < 0.0 || left.screenY > maxY) return;
    } else if (deltaY > 0.0) {
        start = max(start, (0.0 - left.screenY) / deltaY);
        end = min(end, (maxY - left.screenY) / deltaY);
    } else {
        start = max(start, (maxY - left.screenY) / deltaY);
        end = min(end, (0.0 - left.screenY) / deltaY);
    }
    if (start > end) return;

    const double deltaX = right.screenX - left.screenX;
    const double x0 = left.screenX + deltaX * start;
    const double y0 = left.screenY + deltaY * start;
    const double x1 = left.screenX + deltaX * end;
    const double y1 = left.screenY + deltaY * end;
    const int styleX = constrain(static_cast<int>(lround((x0 + x1) * 0.5)), 0, SCREEN_W - 1);
    if (!styleVisible(style, styleX)) return;
    display.drawLine(constrain(static_cast<int>(lround(x0)), 0, SCREEN_W - 1),
                     constrain(static_cast<int>(lround(y0)), 0, SCREEN_H - 1),
                     constrain(static_cast<int>(lround(x1)), 0, SCREEN_W - 1),
                     constrain(static_cast<int>(lround(y1)), 0, SCREEN_H - 1));
}

double linearError(const CurveSample &left, const CurveSample &sample, const CurveSample &right) {
    const double width = right.screenX - left.screenX;
    if (!left.valid || !sample.valid || !right.valid || width <= 0.0) return INFINITY;
    const double ratio = (sample.screenX - left.screenX) / width;
    const double expected = left.screenY * (1.0 - ratio) + right.screenY * ratio;
    return fabs(sample.screenY - expected);
}

bool samplesNeedRefinement(const CurveSample *samples, uint8_t count) {
    uint8_t validCount = 0;
    double minY = INFINITY;
    double maxY = -INFINITY;
    for (uint8_t i = 0; i < count; ++i) {
        if (!samples[i].valid) continue;
        ++validCount;
        minY = min(minY, samples[i].screenY);
        maxY = max(maxY, samples[i].screenY);
    }
    if (validCount == 0) return false;
    if (validCount != count) return true;

    double maxError = 0.0;
    for (uint8_t i = 1; i + 1 < count; ++i) {
        maxError = max(maxError, linearError(samples[0], samples[i], samples[count - 1]));
    }
    const bool nearView = maxY >= -8.0 && minY <= SCREEN_H + 7.0;
    if (!nearView && (maxY < 0.0 || minY >= SCREEN_H) && maxError < 4.0) return false;
    if (maxError > CURVE_ERROR_PIXELS) return true;
    if (maxY - minY > 24.0 && maxError > 0.25) return true;

    for (uint8_t i = 1; i + 1 < count; ++i) {
        const double before = samples[i].screenY - samples[i - 1].screenY;
        const double after = samples[i + 1].screenY - samples[i].screenY;
        if (before * after < 0.0 && min(fabs(before), fabs(after)) > 0.5) return true;
    }
    return false;
}

bool samplesContinuous(const CurveSample *samples, uint8_t count) {
    for (uint8_t i = 0; i < count; ++i) {
        if (!samples[i].valid) return false;
    }
    const double span = fabs(samples[count - 1].screenY - samples[0].screenY);
    const double tolerance = CONTINUITY_ERROR_PIXELS + min(2.0, span * 0.025);
    for (uint8_t i = 1; i + 1 < count; ++i) {
        if (linearError(samples[0], samples[i], samples[count - 1]) > tolerance) return false;
    }
    return true;
}

void drawSamplePolyline(const CurveSample *samples, uint8_t count, uint8_t style) {
    for (uint8_t i = 0; i < count; ++i) drawCurvePoint(samples[i], style);
    if (!samplesContinuous(samples, count)) return;
    for (uint8_t i = 1; i < count; ++i) drawClippedCurveSegment(samples[i - 1], samples[i], style);
}

void drawAdaptiveTriple(const FunctionSlot &slot, const CurveSample &left,
                        const CurveSample &middle, const CurveSample &right,
                        uint8_t style, uint8_t depth, EvaluationBudget &budget) {
    const CurveSample samples[3] = {left, middle, right};
    const bool refine = samplesNeedRefinement(samples, 3);
    if (refine && depth < MAX_ADAPTIVE_DEPTH && budget.limit - budget.used >= 2) {
        CurveSample leftMiddle;
        CurveSample rightMiddle;
        evaluateCurveSample(slot, (left.screenX + middle.screenX) * 0.5, leftMiddle, budget);
        evaluateCurveSample(slot, (middle.screenX + right.screenX) * 0.5, rightMiddle, budget);
        drawAdaptiveTriple(slot, left, leftMiddle, middle, style, depth + 1, budget);
        drawAdaptiveTriple(slot, middle, rightMiddle, right, style, depth + 1, budget);
        return;
    }
    drawSamplePolyline(samples, 3, style);
}

void drawRefinedInterval(const FunctionSlot &slot, const CurveSample *samples,
                         uint8_t style, EvaluationBudget &budget) {
    CurveSample middles[3];
    bool sampled[3] = {false, false, false};
    for (uint8_t i = 0; i < 3 && budget.used < budget.limit; ++i) {
        sampled[i] = evaluateCurveSample(slot, (samples[i].screenX + samples[i + 1].screenX) * 0.5,
                                         middles[i], budget);
    }
    for (uint8_t i = 0; i < 3; ++i) {
        if (sampled[i]) drawAdaptiveTriple(slot, samples[i], middles[i], samples[i + 1], style, 0, budget);
        else {
            drawCurvePoint(samples[i], style);
            drawCurvePoint(samples[i + 1], style);
        }
    }
}

void drawCurve(const FunctionSlot &slot, uint8_t style, uint16_t allowance,
               uint16_t &frameEvaluations) {
    EvaluationBudget budget;
    budget.limit = allowance;
    const uint16_t adaptiveTotal = allowance > BASE_FUNCTION_EVALUATIONS
                                       ? allowance - BASE_FUNCTION_EVALUATIONS
                                       : 0;
    uint16_t adaptiveCredit = 0;
    uint16_t adaptiveRemainder = 0;
    CurveSample left;
    if (!evaluateCurveSample(slot, 0.0, left, budget)) return;

    for (int px = 0; px < SCREEN_W - 1; ++px) {
        CurveSample right;
        CurveSample middle;
        CurveSample probe;
        evaluateCurveSample(slot, px + 1.0, right, budget);
        evaluateCurveSample(slot, px + 0.5, middle, budget);
        const double probeOffset = (px & 1) ? 0.625 : 0.375;
        evaluateCurveSample(slot, px + probeOffset, probe, budget);

        CurveSample samples[4];
        samples[0] = left;
        if (probeOffset < 0.5) {
            samples[1] = probe;
            samples[2] = middle;
        } else {
            samples[1] = middle;
            samples[2] = probe;
        }
        samples[3] = right;

        adaptiveRemainder += adaptiveTotal;
        adaptiveCredit += adaptiveRemainder / (SCREEN_W - 1);
        adaptiveRemainder %= SCREEN_W - 1;
        const uint16_t remainingBaseline = (SCREEN_W - 2 - px) * 3;
        const uint16_t fullLimit = budget.limit;
        const uint16_t available = fullLimit > budget.used + remainingBaseline
                                       ? fullLimit - budget.used - remainingBaseline
                                       : 0;
        const uint16_t granted = min(adaptiveCredit, available);
        budget.limit = budget.used + granted;

        if (samplesNeedRefinement(samples, 4)) {
            if (granted >= 3) {
                const uint16_t before = budget.used;
                drawRefinedInterval(slot, samples, style, budget);
                adaptiveCredit -= budget.used - before;
            } else {
                for (uint8_t i = 0; i < 4; ++i) drawCurvePoint(samples[i], style);
            }
        } else {
            drawSamplePolyline(samples, 4, style);
        }
        budget.limit = fullLimit;
        left = right;
    }
    frameEvaluations += budget.used;
}

void drawTrace() {
    if (!traceEnabled) return;
    const int px = lround(SCREEN_W * 0.5 + (traceX - centerX) * pixelsPerUnit);
    if (px >= 0 && px < SCREEN_W) drawDottedVertical(px);
    double y = 0.0;
    const FunctionSlot &slot = currentFunction();
    if (!slot.valid || !slot.parser.evaluate(traceX, degrees, y)) return;
    const int py = lround(SCREEN_H * 0.5 - (y - centerY) * pixelsPerUnit);
    if (px >= 2 && px < SCREEN_W - 2 && py >= 2 && py < SCREEN_H - 2) {
        display.drawCircle(px, py, 2);
        display.drawPixel(px, py);
    }
}

void drawGraphStatus() {
    display.setFont(u8g2_font_5x7_tf);
    String status = "2/2 F" + String(activeFunction + 1) + " " + (degrees ? "DEG" : "RAD");
    status += gridEnabled ? " G" : " -";
    status += labelsEnabled ? "L" : "-";
    status += " Z" + shortNumber(pixelsPerUnit / 20.0);
    const int width = display.getStrWidth(status.c_str()) + 4;
    display.setDrawColor(0);
    display.drawBox(SCREEN_W - width, 0, width, 9);
    display.setDrawColor(1);
    display.drawStr(SCREEN_W - width + 2, 7, status.c_str());

    if (traceEnabled) {
        double y = 0.0;
        const FunctionSlot &slot = currentFunction();
        const bool valid = slot.valid && slot.parser.evaluate(traceX, degrees, y);
        String trace = "F" + String(activeFunction + 1) + " x=" + shortNumber(traceX) + " y=" + (valid ? shortNumber(y) : String("undef"));
        int traceWidth = std::min<int>(SCREEN_W, display.getStrWidth(trace.c_str()) + 4);
        display.setDrawColor(0);
        display.drawBox(0, SCREEN_H - 9, traceWidth, 9);
        display.setDrawColor(1);
        display.drawStr(2, SCREEN_H - 2, trace.c_str());
    }
}

void drawGraph() {
    if (graphBaseDirty || !graphBaseValid) {
        display.clearBuffer();
        drawGridAndAxes();

        uint8_t remainingFunctions = 0;
        for (uint8_t i = 0; i < functionCount; ++i) {
            if (functions[i].valid) ++remainingFunctions;
        }
        uint16_t frameEvaluations = 0;
        for (uint8_t i = 0; i < functionCount; ++i) {
            if (!functions[i].valid) continue;
            const uint16_t available = MAX_FRAME_EVALUATIONS - frameEvaluations;
            const uint16_t fairShare = available / remainingFunctions;
            const uint16_t allowance = std::min<uint16_t>(MAX_FUNCTION_EVALUATIONS, fairShare);
            drawCurve(functions[i], i, allowance, frameEvaluations);
            --remainingFunctions;
        }
        drawAxisLabels();
        memcpy(graphBaseBuffer, display.getBufferPtr(), GRAPH_CACHE_BYTES);
        graphBaseDirty = false;
        graphBaseValid = true;
    } else {
        memcpy(display.getBufferPtr(), graphBaseBuffer, GRAPH_CACHE_BYTES);
    }

    drawTrace();
    drawGraphStatus();
    if (noticeActive()) {
        display.setFont(u8g2_font_5x7_tf);
        const int width = std::min<int>(SCREEN_W, display.getStrWidth(notice.c_str()) + 6);
        display.setDrawColor(0);
        display.drawBox(0, 0, width, 9);
        display.setDrawColor(1);
        display.drawStr(3, 7, notice.c_str());
    }
    display.sendBuffer();
}

void drawEditor() {
    FunctionSlot &slot = currentFunction();
    display.clearBuffer();
    display.setFont(u8g2_font_5x7_tf);
    String header = "EDIT 1/2  F" + String(activeFunction + 1) + "/" + String(functionCount) + "  " + (degrees ? "DEG" : "RAD");
    display.drawStr(2, 7, header.c_str());
    String count = String(slot.expression.length()) + "/" + MAX_EXPRESSION;
    display.drawStr(SCREEN_W - display.getStrWidth(count.c_str()) - 2, 7, count.c_str());

    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(2, 31, "f(x)=");
    constexpr int expressionX = 34;
    constexpr int charWidth = 6;
    constexpr int visibleChars = (SCREEN_W - expressionX - 2) / charWidth;
    int start = 0;
    if (slot.cursor >= visibleChars) start = slot.cursor - visibleChars + 1;
    if (start > slot.expression.length()) start = slot.expression.length();
    String visible = slot.expression.substring(start, std::min<int>(slot.expression.length(), start + visibleChars));
    display.drawStr(expressionX, 31, visible.c_str());
    int cursorX = expressionX + (slot.cursor - start) * charWidth;
    display.drawHLine(cursorX, 33, 5);

    if (slot.error.position >= start && slot.error.position <= start + visibleChars) {
        int errorX = expressionX + (slot.error.position - start) * charWidth;
        display.drawVLine(errorX, 35, 3);
    }

    display.setFont(u8g2_font_5x7_tf);
    String footer;
    if (noticeActive()) footer = notice;
    else if (slot.error.position >= 0) {
        footer = "F" + String(activeFunction + 1) + " ERR @" + String(slot.error.position + 1) + ": " + slot.error.message;
    } else {
        footer = "TAB graph  RPN next  S+RPN add";
    }
    if (display.getStrWidth(footer.c_str()) > SCREEN_W - 4) {
        while (footer.length() && display.getStrWidth(footer.c_str()) > SCREEN_W - 12) footer.remove(footer.length() - 1);
        footer += "...";
    }
    display.drawStr(2, 62, footer.c_str());
    display.sendBuffer();
}

void insertFunction(const char *name) {
    insertText(String(name) + "(");
}

char digitFor(KeyCode key) {
    switch (key) {
        case KeyCode::K0: return '0'; case KeyCode::K1: return '1';
        case KeyCode::K2: return '2'; case KeyCode::K3: return '3';
        case KeyCode::K4: return '4'; case KeyCode::K5: return '5';
        case KeyCode::K6: return '6'; case KeyCode::K7: return '7';
        case KeyCode::K8: return '8'; case KeyCode::K9: return '9';
        default: return '\0';
    }
}

void handleEditorKey(const KeyEvent &event) {
    if (event.action == KeyAction::Release) return;
    const bool shift = keyboard.isDown(KeyCode::Shift);
    const bool cursorRepeat = shift && (event.key == KeyCode::K4 || event.key == KeyCode::K6);
    if (event.action == KeyAction::Repeat && event.key != KeyCode::Backspace && !cursorRepeat) return;

    if (event.key == KeyCode::Backspace) {
        if (shift && event.action == KeyAction::Press) deleteFunction();
        else eraseLeft();
        return;
    }
    if (event.action != KeyAction::Press) return;
    if (event.key == KeyCode::Tab || event.key == KeyCode::Enter) { enterGraph(); return; }
    if (event.key == KeyCode::Angle) { degrees = !degrees; dirty = true; return; }
    if (event.key == KeyCode::Shift || event.key == KeyCode::Escape) return;

    if (shift) {
        FunctionSlot &slot = currentFunction();
        if (event.key == KeyCode::K4) { if (slot.cursor > 0) --slot.cursor; dirty = true; return; }
        if (event.key == KeyCode::K6) { if (slot.cursor < slot.expression.length()) ++slot.cursor; dirty = true; return; }
        if (event.key == KeyCode::K8) { selectFunction(-1); return; }
        if (event.key == KeyCode::K2) { selectFunction(1); return; }
        if (event.key == KeyCode::K5) { insertText("x"); return; }
        if (event.key == KeyCode::Divide) { insertText("("); return; }
        if (event.key == KeyCode::Multiply) { insertText(")"); return; }
        if (event.key == KeyCode::Power) { insertFunction("sqrt"); return; }
        if (event.key == KeyCode::Sec) { insertFunction("abs"); return; }
        if (event.key == KeyCode::Csc) { insertFunction("log"); return; }
        if (event.key == KeyCode::Cot) { insertText("pi"); return; }
        if (event.key == KeyCode::K9) { insertText("e"); return; }
        if (event.key == KeyCode::Extra) { addFunction(); return; }
    }

    switch (event.key) {
        case KeyCode::Sin: insertFunction(shift ? "asin" : "sin"); return;
        case KeyCode::Cos: insertFunction(shift ? "acos" : "cos"); return;
        case KeyCode::Tan: insertFunction(shift ? "atan" : "tan"); return;
        case KeyCode::Sec: insertFunction("sec"); return;
        case KeyCode::Csc: insertFunction("csc"); return;
        case KeyCode::Cot: insertFunction("cot"); return;
        case KeyCode::Exp: insertFunction(shift ? "ln" : "exp"); return;
        case KeyCode::Extra: selectFunction(1); return;
        case KeyCode::Plus: insertText("+"); return;
        case KeyCode::Minus: insertText("-"); return;
        case KeyCode::Multiply: insertText("*"); return;
        case KeyCode::Divide: insertText("/"); return;
        case KeyCode::Power: insertText("^"); return;
        case KeyCode::Dot: insertText("."); return;
        default: break;
    }
    if (!shift) {
        const char digit = digitFor(event.key);
        if (digit) insertText(String(digit));
    }
}

void handleGraphKey(const KeyEvent &event) {
    if (event.action == KeyAction::Release) return;
    const bool shift = keyboard.isDown(KeyCode::Shift);
    const bool movementKey = event.key == KeyCode::K4 || event.key == KeyCode::K6 ||
                             event.key == KeyCode::K8 || event.key == KeyCode::K2;
    const bool repeatable = movementKey || event.key == KeyCode::Plus || event.key == KeyCode::Minus;
    if (event.action == KeyAction::Repeat && !repeatable) return;
    if (event.action == KeyAction::Repeat && movementKey && !shift && !traceEnabled) return;
    if (event.action == KeyAction::Press && event.key == KeyCode::Tab) { page = Page::Editor; dirty = true; return; }
    if (event.action == KeyAction::Press && event.key == KeyCode::Enter) { traceEnabled = !traceEnabled; traceX = centerX; dirty = true; return; }
    if (event.action == KeyAction::Press && event.key == KeyCode::Dot) { gridEnabled = !gridEnabled; markGraphBaseDirty(); return; }
    if (event.action == KeyAction::Press && event.key == KeyCode::Angle) { degrees = !degrees; markGraphBaseDirty(); return; }
    if (event.action == KeyAction::Press && event.key == KeyCode::Extra) { labelsEnabled = !labelsEnabled; markGraphBaseDirty(); return; }
    if (event.action == KeyAction::Press && !shift && event.key == KeyCode::K8) { selectGraphFunction(-1); return; }
    if (event.action == KeyAction::Press && !shift && event.key == KeyCode::K2) { selectGraphFunction(1); return; }
    if (event.action == KeyAction::Press && event.key == KeyCode::K5 && !shift) { resetView(); return; }
    if (event.key == KeyCode::Plus) { pixelsPerUnit = min(160.0, pixelsPerUnit * 1.25); markGraphBaseDirty(); return; }
    if (event.key == KeyCode::Minus) { pixelsPerUnit = max(2.0, pixelsPerUnit / 1.25); markGraphBaseDirty(); return; }

    if (shift) {
        if (event.key == KeyCode::K4) centerX -= 32.0 / pixelsPerUnit;
        else if (event.key == KeyCode::K6) centerX += 32.0 / pixelsPerUnit;
        else if (event.key == KeyCode::K8) centerY += 12.0 / pixelsPerUnit;
        else if (event.key == KeyCode::K2) centerY -= 12.0 / pixelsPerUnit;
        else return;
        markGraphBaseDirty();
        return;
    }
    if (traceEnabled && (event.key == KeyCode::K4 || event.key == KeyCode::K6)) {
        traceX += (event.key == KeyCode::K4 ? -1.0 : 1.0) / pixelsPerUnit;
        dirty = true;
    }
}

void drawCurrentPage() {
    if (page == Page::Editor) drawEditor();
    else drawGraph();
    dirty = false;
}
}

void setup() {
    Serial.begin(115200);
    pinMode(OLED_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(OLED_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    SPI.begin(18, 19, 23);
    display.begin();
    display.setContrast(220);
    keyboard.begin();
    sdReady = SD.begin(SD_CS, SPI);
    if (!sdReady) setNotice("SD unavailable; graphing works", 2600);
    if (!parserSelfTest()) setNotice("Parser self-test failed", 5000);
    drawCurrentPage();
}

void loop() {
    keyboard.update();

    if (keyboard.isDown(KeyCode::Escape) && keyboard.heldMs(KeyCode::Escape) >= EXIT_HOLD_MS) {
        if (!exitTriggered) {
            exitTriggered = true;
            installMainFirmware();
        }
    } else if (!keyboard.isDown(KeyCode::Escape)) {
        exitTriggered = false;
    }

    KeyEvent event;
    while (keyboard.nextEvent(event)) {
        if (event.key == KeyCode::Escape) continue;
        if (page == Page::Editor) handleEditorKey(event);
        else handleGraphKey(event);
    }

    if (notice.length() && !noticeActive()) {
        notice = "";
        dirty = true;
    }
    if (dirty && millis() - lastFrame >= MIN_FRAME_INTERVAL_MS) {
        lastFrame = millis();
        drawCurrentPage();
    }
    delay(1);
}
