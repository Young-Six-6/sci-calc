#include <Arduino.h>
#include <FS.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <U8g2lib.h>
#include <esp_heap_caps.h>

#include "Chart.h"
#include "SafeKeyboard.h"
#include "TileSynth.h"

namespace {
constexpr uint8_t OLED_CS = 5;
constexpr uint8_t OLED_DC = 16;
constexpr uint8_t OLED_RST = 17;
constexpr uint8_t SD_CS = 4;
constexpr uint16_t SCREEN_WIDTH = 256;
constexpr uint8_t SCREEN_HEIGHT = 64;
constexpr uint32_t EXIT_HOLD_MS = 1000;
constexpr uint16_t FRAME_MS = 25;
constexpr uint8_t MAX_SONGS = 19;
constexpr uint8_t BUILT_IN_SONGS = 3;
constexpr char CHART_DIRECTORY[] = "/fallingkeys";
constexpr char MAIN_FIRMWARE[] = "/main.bin";

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI display(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
SafeKeyboard keyboard;
TileSynth synth;
Chart chart;

enum class Screen : uint8_t { Menu, Countdown, Playing, Paused, Results, Error };
enum class EventState : uint8_t { Upcoming, Holding, Hit, Miss };
enum MenuItem : uint8_t { StartItem, SongItem, LanesItem, LevelItem, ToneItem, VolumeItem, ExitItem, MenuItemCount };

struct SongInfo {
    bool builtIn = false;
    uint8_t builtInId = 0;
    uint8_t lanes = 4;
    char title[32] = {};
    char path[80] = {};
};

struct Difficulty {
    const char *name;
    uint16_t hitWindowMs;
    uint16_t perfectWindowMs;
    uint16_t approachMs;
    uint16_t holdGraceMs;
};

constexpr Difficulty DIFFICULTIES[3] = {
    {"Easy", 180, 60, 2200, 150},
    {"Normal", 140, 45, 1800, 115},
    {"Hard", 100, 30, 1400, 85}
};

SongInfo songs[MAX_SONGS];
uint8_t songCount = 0;
uint8_t selectedSong = 0;
uint8_t selectedLanes = 4;
uint8_t selectedDifficulty = 1;
uint8_t selectedTone = TileSynth::Piano;
uint8_t selectedVolume = 7;
uint8_t selectedMenuItem = 0;

Screen screen = Screen::Menu;
EventState eventStates[Chart::MAX_EVENTS];
int16_t activeHold[Chart::MAX_LANES] = {-1, -1, -1, -1, -1, -1};
uint8_t laneDown[Chart::MAX_LANES] = {};
uint32_t laneFlashUntil[Chart::MAX_LANES] = {};
uint16_t missCursor = 0;
uint8_t activeLanes = 4;
uint32_t songStartAt = 0;
uint32_t pausedAt = 0;
uint32_t lastFrameAt = 0;

uint32_t score = 0;
uint32_t bestScore = 0;
uint16_t combo = 0;
uint16_t maxCombo = 0;
uint16_t perfectCount = 0;
uint16_t goodCount = 0;
uint16_t missCount = 0;
uint16_t wrongCount = 0;
uint16_t judgedParts = 0;
uint32_t accuracyPoints = 0;
char judgementText[16] = {};
uint32_t judgementUntil = 0;
char errorText[52] = {};
bool sdReady = false;
bool audioReady = false;
bool exitTriggered = false;
bool newBest = false;

void copyText(char *destination, size_t size, const char *source) {
    if (!destination || !size) return;
    snprintf(destination, size, "%s", source ? source : "");
}

bool endsWithPtc(const char *name) {
    if (!name) return false;
    const size_t length = strlen(name);
    if (length < 4) return false;
    const char *extension = name + length - 4;
    return extension[0] == '.' && tolower(extension[1]) == 'p' &&
           tolower(extension[2]) == 't' && tolower(extension[3]) == 'c';
}

void addBuiltIn(const char *title, Chart::BuiltIn id) {
    if (songCount >= MAX_SONGS) return;
    SongInfo &song = songs[songCount++];
    song.builtIn = true;
    song.builtInId = id;
    song.lanes = 4;
    copyText(song.title, sizeof(song.title), title);
}

void scanSongs() {
    songCount = 0;
    addBuiltIn("Classic Rush", Chart::ClassicRush);
    addBuiltIn("Ode to Joy", Chart::OdeToJoy);
    addBuiltIn("Twinkle Star", Chart::Twinkle);
    if (!sdReady) return;
    if (!SD.exists(CHART_DIRECTORY)) SD.mkdir(CHART_DIRECTORY);
    File directory = SD.open(CHART_DIRECTORY);
    if (!directory || !directory.isDirectory()) {
        if (directory) directory.close();
        return;
    }

    File entry;
    while (songCount < MAX_SONGS && (entry = directory.openNextFile())) {
        if (!entry.isDirectory() && endsWithPtc(entry.name())) {
            char path[80];
            if (entry.name()[0] == '/') copyText(path, sizeof(path), entry.name());
            else snprintf(path, sizeof(path), "%s/%s", CHART_DIRECTORY, entry.name());
            SongInfo candidate;
            if (Chart::probe(SD, path, candidate.title, sizeof(candidate.title), candidate.lanes)) {
                candidate.builtIn = false;
                copyText(candidate.path, sizeof(candidate.path), path);
                songs[songCount++] = candidate;
            }
        }
        entry.close();
    }
    directory.close();
    if (selectedSong >= songCount) selectedSong = 0;
}

uint8_t configuredLanes() {
    return songs[selectedSong].builtIn ? selectedLanes : songs[selectedSong].lanes;
}

uint32_t scoreHash() {
    uint32_t hash = 2166136261UL;
    const char *identity = songs[selectedSong].builtIn ? songs[selectedSong].title : songs[selectedSong].path;
    for (const char *cursor = identity; cursor && *cursor; ++cursor) {
        hash ^= static_cast<uint8_t>(*cursor);
        hash *= 16777619UL;
    }
    hash ^= activeLanes;
    hash *= 16777619UL;
    hash ^= selectedDifficulty;
    return hash;
}

void makeScoreKey(char *key, size_t size) {
    snprintf(key, size, "s%08lx", static_cast<unsigned long>(scoreHash()));
}

uint32_t readBestScore() {
    Preferences preferences;
    char key[12];
    makeScoreKey(key, sizeof(key));
    if (!preferences.begin("fallkeys", true)) return 0;
    const uint32_t value = preferences.getULong(key, 0);
    preferences.end();
    return value;
}

void saveBestScore() {
    if (score <= bestScore) return;
    Preferences preferences;
    char key[12];
    makeScoreKey(key, sizeof(key));
    if (preferences.begin("fallkeys", false)) {
        preferences.putULong(key, score);
        preferences.end();
        bestScore = score;
        newBest = true;
    }
}

uint16_t accuracyTenths() {
    if (!judgedParts) return 1000;
    return min<uint32_t>(1000, accuracyPoints * 10UL / judgedParts);
}

void setJudgement(const char *text) {
    copyText(judgementText, sizeof(judgementText), text);
    judgementUntil = millis() + 430;
}

void clearLaneState() {
    memset(laneDown, 0, sizeof(laneDown));
}

bool anyActiveHold() {
    for (uint8_t lane = 0; lane < activeLanes; ++lane) if (activeHold[lane] >= 0) return true;
    return false;
}

void resetRun() {
    memset(eventStates, 0, sizeof(eventStates));
    for (uint8_t lane = 0; lane < Chart::MAX_LANES; ++lane) {
        activeHold[lane] = -1;
        laneDown[lane] = 0;
        laneFlashUntil[lane] = 0;
    }
    missCursor = 0;
    score = 0;
    combo = 0;
    maxCombo = 0;
    perfectCount = 0;
    goodCount = 0;
    missCount = 0;
    wrongCount = 0;
    judgedParts = 0;
    accuracyPoints = 0;
    judgementText[0] = '\0';
    newBest = false;
}

bool loadSelectedChart() {
    char error[52] = {};
    const SongInfo &song = songs[selectedSong];
    bool loaded = false;
    if (song.builtIn) {
        loaded = chart.generate(static_cast<Chart::BuiltIn>(song.builtInId), selectedLanes, selectedDifficulty);
    } else {
        loaded = chart.load(SD, song.path, error, sizeof(error));
    }
    if (!loaded || !chart.eventCount()) {
        copyText(errorText, sizeof(errorText), error[0] ? error : "Could not build chart");
        screen = Screen::Error;
        return false;
    }
    activeLanes = chart.laneCount();
    resetRun();
    bestScore = readBestScore();
    synth.allOff(true);
    synth.setTone(selectedTone);
    synth.setVolume(selectedVolume);
    songStartAt = millis() + 3000;
    screen = Screen::Countdown;
    return true;
}

void recordHeadJudgement(bool perfect) {
    ++judgedParts;
    ++combo;
    maxCombo = max<uint16_t>(maxCombo, combo);
    const uint8_t multiplier = 1 + min<uint16_t>(3, combo / 10);
    if (perfect) {
        ++perfectCount;
        accuracyPoints += 100;
        score += 1000UL * multiplier;
        setJudgement("PERFECT");
    } else {
        ++goodCount;
        accuracyPoints += 70;
        score += 500UL * multiplier;
        setJudgement("GOOD");
    }
}

void recordMiss(const char *message = "MISS") {
    ++judgedParts;
    ++missCount;
    combo = 0;
    setJudgement(message);
}

void completeHold(uint8_t lane) {
    const int16_t index = activeHold[lane];
    if (index < 0) return;
    eventStates[index] = EventState::Hit;
    synth.stop(chart.event(index).note);
    activeHold[lane] = -1;
    ++judgedParts;
    accuracyPoints += 100;
    score += 300UL * (1 + min<uint16_t>(3, combo / 10));
    setJudgement("HOLD OK");
}

void breakHold(uint8_t lane) {
    const int16_t index = activeHold[lane];
    if (index < 0) return;
    eventStates[index] = EventState::Miss;
    synth.stop(chart.event(index).note);
    activeHold[lane] = -1;
    recordMiss("HOLD MISS");
}

int32_t playElapsed() {
    const uint32_t clock = screen == Screen::Paused ? pausedAt : millis();
    return static_cast<int32_t>(clock - songStartAt);
}

void pressLane(uint8_t lane) {
    if (lane >= activeLanes || laneDown[lane]++) return;
    const int32_t elapsed = playElapsed();
    const Difficulty &difficulty = DIFFICULTIES[selectedDifficulty];
    int16_t best = -1;
    uint32_t bestDelta = UINT32_MAX;
    for (uint16_t index = missCursor; index < chart.eventCount(); ++index) {
        const TileEvent &entry = chart.event(index);
        const int32_t delta = static_cast<int32_t>(entry.timeMs) - elapsed;
        if (delta > difficulty.hitWindowMs) break;
        if (eventStates[index] != EventState::Upcoming || entry.lane != lane) continue;
        const uint32_t absolute = abs(delta);
        if (absolute <= difficulty.hitWindowMs && absolute < bestDelta) {
            best = index;
            bestDelta = absolute;
        }
    }

    laneFlashUntil[lane] = millis() + 100;
    if (best < 0) {
        ++wrongCount;
        ++judgedParts;
        combo = 0;
        synth.miss();
        setJudgement("TOO SOON");
        return;
    }

    const TileEvent &entry = chart.event(best);
    const bool isHold = entry.durationMs >= 300;
    eventStates[best] = isHold ? EventState::Holding : EventState::Hit;
    if (isHold) activeHold[lane] = best;
    const uint16_t soundLength = isHold
        ? max<int32_t>(120, static_cast<int32_t>(entry.durationMs) - max<int32_t>(0, elapsed - entry.timeMs))
        : selectedTone == TileSynth::Piano ? 260 : 150;
    synth.play(entry.note, bestDelta <= DIFFICULTIES[selectedDifficulty].perfectWindowMs ? 118 : 96,
               min<uint16_t>(soundLength, 6000));
    recordHeadJudgement(bestDelta <= difficulty.perfectWindowMs);
}

void releaseLane(uint8_t lane) {
    if (lane >= activeLanes || !laneDown[lane]) return;
    --laneDown[lane];
    if (laneDown[lane] || activeHold[lane] < 0) return;
    const TileEvent &entry = chart.event(activeHold[lane]);
    const int32_t remaining = static_cast<int32_t>(entry.timeMs + entry.durationMs) - playElapsed();
    if (remaining <= DIFFICULTIES[selectedDifficulty].holdGraceMs) completeHold(lane);
    else breakHold(lane);
}

void serviceMissesAndHolds() {
    const int32_t elapsed = playElapsed();
    const Difficulty &difficulty = DIFFICULTIES[selectedDifficulty];
    while (missCursor < chart.eventCount()) {
        const TileEvent &entry = chart.event(missCursor);
        if (elapsed <= static_cast<int32_t>(entry.timeMs + difficulty.hitWindowMs)) break;
        if (eventStates[missCursor] == EventState::Upcoming) {
            eventStates[missCursor] = EventState::Miss;
            recordMiss();
        }
        ++missCursor;
    }
    for (uint8_t lane = 0; lane < activeLanes; ++lane) {
        if (activeHold[lane] < 0) continue;
        const TileEvent &entry = chart.event(activeHold[lane]);
        if (elapsed >= static_cast<int32_t>(entry.timeMs + entry.durationMs) - difficulty.holdGraceMs) {
            if (laneDown[lane]) completeHold(lane);
            else breakHold(lane);
        }
    }
}

void finishRun() {
    synth.allOff(false);
    clearLaneState();
    saveBestScore();
    screen = Screen::Results;
}

void serviceRun() {
    if (screen == Screen::Countdown && static_cast<int32_t>(millis() - songStartAt) >= 0) {
        screen = Screen::Playing;
    }
    if (screen != Screen::Playing) return;
    serviceMissesAndHolds();
    if (playElapsed() > static_cast<int32_t>(chart.durationMs() + 1200) &&
        missCursor >= chart.eventCount() && !anyActiveHold()) finishRun();
}

void pauseRun() {
    if (screen == Screen::Countdown) {
        synth.allOff(true);
        screen = Screen::Menu;
        return;
    }
    if (screen != Screen::Playing) return;
    for (uint8_t lane = 0; lane < activeLanes; ++lane) {
        if (activeHold[lane] < 0) continue;
        if (laneDown[lane]) completeHold(lane);
        else breakHold(lane);
    }
    synth.allOff(true);
    clearLaneState();
    pausedAt = millis();
    screen = Screen::Paused;
}

void resumeRun() {
    if (screen != Screen::Paused) return;
    songStartAt += millis() - pausedAt;
    clearLaneState();
    screen = Screen::Playing;
}

void leaveRun() {
    synth.allOff(true);
    clearLaneState();
    screen = Screen::Menu;
}

void adjustMenu(int8_t direction) {
    switch (selectedMenuItem) {
        case SongItem:
            selectedSong = static_cast<uint8_t>((selectedSong + songCount + direction) % songCount);
            break;
        case LanesItem:
            if (songs[selectedSong].builtIn) selectedLanes = selectedLanes == 4 ? 6 : 4;
            break;
        case LevelItem:
            selectedDifficulty = static_cast<uint8_t>((selectedDifficulty + 3 + direction) % 3);
            break;
        case ToneItem:
            selectedTone = selectedTone == TileSynth::Piano ? TileSynth::Arcade : TileSynth::Piano;
            synth.setTone(selectedTone);
            break;
        case VolumeItem:
            selectedVolume = static_cast<uint8_t>(max<int16_t>(0, min<int16_t>(10, selectedVolume + direction)));
            synth.setVolume(selectedVolume);
            break;
        default:
            break;
    }
}

bool installMainFirmware();

void activateMenuItem() {
    if (selectedMenuItem == StartItem) loadSelectedChart();
    else if (selectedMenuItem == ExitItem) installMainFirmware();
    else adjustMenu(1);
}

void handleMenuKey(KeyboardKey key, bool repeat) {
    if (key == KeyboardKey::K8) {
        selectedMenuItem = static_cast<uint8_t>((selectedMenuItem + MenuItemCount - 1) % MenuItemCount);
    } else if (key == KeyboardKey::K2) {
        selectedMenuItem = static_cast<uint8_t>((selectedMenuItem + 1) % MenuItemCount);
    } else if (key == KeyboardKey::K4) adjustMenu(-1);
    else if (key == KeyboardKey::K6) adjustMenu(1);
    else if (key == KeyboardKey::Enter && !repeat) activateMenuItem();
}

void handleKeyboardEvent(const KeyboardEvent &event) {
    const bool repeat = event.action == KeyboardAction::Repeat;
    if (event.key == KeyboardKey::Escape) {
        if (event.action != KeyboardAction::Press) return;
        if (screen == Screen::Playing || screen == Screen::Countdown) pauseRun();
        else if (screen == Screen::Paused) resumeRun();
        else if (screen == Screen::Results || screen == Screen::Error) screen = Screen::Menu;
        return;
    }

    if (screen == Screen::Playing) {
        if (event.key == KeyboardKey::Shift || repeat) return;
        const int8_t lane = SafeKeyboard::physicalColumn(event.key);
        if (lane < 0 || lane >= activeLanes) return;
        if (event.action == KeyboardAction::Press) pressLane(lane);
        else if (event.action == KeyboardAction::Release) releaseLane(lane);
        return;
    }
    if (event.action == KeyboardAction::Release) return;

    if (screen == Screen::Menu) {
        handleMenuKey(event.key, repeat);
    } else if (screen == Screen::Paused) {
        if (event.key == KeyboardKey::Enter) resumeRun();
        else if (event.key == KeyboardKey::Backspace) leaveRun();
    } else if (screen == Screen::Results) {
        if (event.key == KeyboardKey::Enter && !repeat) loadSelectedChart();
        else if (event.key == KeyboardKey::Backspace) screen = Screen::Menu;
    } else if (screen == Screen::Error &&
               (event.key == KeyboardKey::Enter || event.key == KeyboardKey::Backspace)) {
        screen = Screen::Menu;
    }
}

void fitText(char *text, uint16_t maxWidth) {
    while (*text && display.getStrWidth(text) > maxWidth) {
        const size_t length = strlen(text);
        if (length < 2) break;
        text[length - 1] = '\0';
    }
}

void drawMenuRow(uint8_t item, uint8_t top) {
    static const char *LABELS[MenuItemCount] = {"PLAY", "TRACK", "LANES", "LEVEL", "TONE", "VOLUME", "EXIT"};
    const bool selected = selectedMenuItem == item;
    if (selected) display.drawBox(0, top, SCREEN_WIDTH, 9);
    display.setDrawColor(selected ? 0 : 1);
    display.drawStr(3, top + 7, LABELS[item]);

    char value[36] = {};
    switch (item) {
        case StartItem: copyText(value, sizeof(value), "ENTER"); break;
        case SongItem: copyText(value, sizeof(value), songs[selectedSong].title); break;
        case LanesItem:
            snprintf(value, sizeof(value), songs[selectedSong].builtIn ? "%u" : "%u FIXED", configuredLanes());
            break;
        case LevelItem: copyText(value, sizeof(value), DIFFICULTIES[selectedDifficulty].name); break;
        case ToneItem: copyText(value, sizeof(value), TileSynth::toneName(selectedTone)); break;
        case VolumeItem: snprintf(value, sizeof(value), "%u / 10", selectedVolume); break;
        case ExitItem: copyText(value, sizeof(value), "MAIN CALC"); break;
    }
    fitText(value, 154);
    const int width = display.getStrWidth(value);
    display.drawStr(max(77, 253 - width), top + 7, value);
    display.setDrawColor(1);
}

void drawMenu() {
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(2, 7, "FALLING KEYS");
    char status[34];
    if (!audioReady) copyText(status, sizeof(status), "AUDIO OFF");
    else if (!sdReady) copyText(status, sizeof(status), "BUILT-IN");
    else snprintf(status, sizeof(status), "%u TRACKS", songCount);
    display.drawStr(254 - display.getStrWidth(status), 7, status);
    display.drawHLine(0, 9, SCREEN_WIDTH);

    constexpr uint8_t visibleRows = 6;
    uint8_t first = selectedMenuItem >= visibleRows ? selectedMenuItem - visibleRows + 1 : 0;
    if (first + visibleRows > MenuItemCount) first = MenuItemCount - visibleRows;
    for (uint8_t row = 0; row < visibleRows; ++row) drawMenuRow(first + row, 10 + row * 9);
}

void drawGameplay() {
    const int32_t elapsed = playElapsed();
    const Difficulty &difficulty = DIFFICULTIES[selectedDifficulty];
    constexpr int topY = 10;
    constexpr int hitY = 57;
    const uint8_t laneWidth = SCREEN_WIDTH / activeLanes;

    display.setFont(u8g2_font_4x6_tf);
    char hud[64];
    const uint16_t accuracy = accuracyTenths();
    snprintf(hud, sizeof(hud), "%08lu   x%u   %u.%u%%", static_cast<unsigned long>(score),
             combo, accuracy / 10, accuracy % 10);
    display.drawStr(2, 6, hud);
    const char *level = DIFFICULTIES[selectedDifficulty].name;
    display.drawStr(254 - display.getStrWidth(level), 6, level);
    display.drawHLine(0, 8, SCREEN_WIDTH);

    for (uint8_t lane = 1; lane < activeLanes; ++lane) display.drawVLine(lane * laneWidth, topY, 54);
    display.drawHLine(0, hitY, SCREEN_WIDTH);
    for (uint8_t lane = 0; lane < activeLanes; ++lane) {
        const uint8_t left = lane * laneWidth;
        const uint8_t right = lane == activeLanes - 1 ? SCREEN_WIDTH : (lane + 1) * laneWidth;
        if (laneDown[lane] || static_cast<int32_t>(laneFlashUntil[lane] - millis()) > 0) {
            display.drawBox(left + 2, hitY + 2, right - left - 3, 5);
        } else {
            display.drawFrame(left + 2, hitY + 2, right - left - 3, 5);
        }
    }

    for (uint16_t index = 0; index < chart.eventCount(); ++index) {
        const TileEvent &entry = chart.event(index);
        if (eventStates[index] == EventState::Hit || eventStates[index] == EventState::Miss) continue;
        const int32_t delta = static_cast<int32_t>(entry.timeMs) - elapsed;
        if (delta > difficulty.approachMs + 160) break;
        if (delta < -static_cast<int32_t>(difficulty.hitWindowMs + 220) &&
            eventStates[index] != EventState::Holding) continue;
        const int headY = hitY - static_cast<int64_t>(delta) * (hitY - topY) / difficulty.approachMs;
        const uint8_t left = entry.lane * laneWidth + 3;
        const uint8_t right = entry.lane == activeLanes - 1 ? SCREEN_WIDTH - 2 : (entry.lane + 1) * laneWidth - 2;
        const int tail = min<int32_t>(44, static_cast<int64_t>(entry.durationMs) * (hitY - topY) / difficulty.approachMs);
        int boxTop = headY - max(4, tail);
        int boxBottom = headY;
        if (boxBottom < topY || boxTop > 63) continue;
        boxTop = max(boxTop, topY);
        boxBottom = min(boxBottom, 63);
        const uint8_t width = max<int>(2, right - left);
        if (eventStates[index] == EventState::Holding) {
            display.drawFrame(left, boxTop, width, max(2, boxBottom - boxTop + 1));
            display.drawBox(left + 2, max(boxTop + 1, boxBottom - 3), max<int>(1, width - 4), 2);
        } else {
            display.drawBox(left, boxTop, width, max(2, boxBottom - boxTop + 1));
            if (tail > 7 && width > 7) {
                display.setDrawColor(0);
                display.drawVLine(left + width / 2, boxTop + 2, max(1, boxBottom - boxTop - 3));
                display.setDrawColor(1);
            }
        }
    }

    if (judgementText[0] && static_cast<int32_t>(judgementUntil - millis()) > 0) {
        display.setFont(u8g2_font_5x7_tf);
        const int width = display.getStrWidth(judgementText);
        display.setDrawColor(0);
        display.drawBox((SCREEN_WIDTH - width) / 2 - 3, 30, width + 6, 9);
        display.setDrawColor(1);
        display.drawFrame((SCREEN_WIDTH - width) / 2 - 3, 30, width + 6, 9);
        display.drawStr((SCREEN_WIDTH - width) / 2, 37, judgementText);
    }

    if (screen == Screen::Countdown) {
        const int32_t remaining = max<int32_t>(0, static_cast<int32_t>(songStartAt - millis()));
        const uint8_t number = min<int32_t>(3, remaining / 1000 + 1);
        char text[4];
        snprintf(text, sizeof(text), "%u", number);
        display.setFont(u8g2_font_logisoso20_tn);
        const int width = display.getStrWidth(text);
        display.setDrawColor(0);
        display.drawBox((SCREEN_WIDTH - width) / 2 - 7, 18, width + 14, 30);
        display.setDrawColor(1);
        display.drawFrame((SCREEN_WIDTH - width) / 2 - 7, 18, width + 14, 30);
        display.drawStr((SCREEN_WIDTH - width) / 2, 43, text);
    }
}

void drawOverlay(const char *title, const char *line1, const char *line2) {
    display.setDrawColor(0);
    display.drawBox(45, 13, 166, 42);
    display.setDrawColor(1);
    display.drawFrame(45, 13, 166, 42);
    display.setFont(u8g2_font_6x10_tf);
    const int titleWidth = display.getStrWidth(title);
    display.drawStr((SCREEN_WIDTH - titleWidth) / 2, 25, title);
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr((SCREEN_WIDTH - display.getStrWidth(line1)) / 2, 38, line1);
    display.drawStr((SCREEN_WIDTH - display.getStrWidth(line2)) / 2, 49, line2);
}

const char *resultRank() {
    const uint16_t accuracy = accuracyTenths();
    if (accuracy >= 970 && missCount == 0 && wrongCount == 0) return "S";
    if (accuracy >= 900) return "A";
    if (accuracy >= 800) return "B";
    if (accuracy >= 650) return "C";
    return "D";
}

void drawResults() {
    display.setFont(u8g2_font_6x10_tf);
    char title[34];
    snprintf(title, sizeof(title), "RESULT  RANK %s", resultRank());
    display.drawStr(2, 9, title);
    if (newBest) display.drawStr(202, 9, "NEW!");
    display.drawHLine(0, 11, SCREEN_WIDTH);

    display.setFont(u8g2_font_10x20_tf);
    char scoreText[16];
    snprintf(scoreText, sizeof(scoreText), "%08lu", static_cast<unsigned long>(score));
    display.drawStr(3, 34, scoreText);
    display.setFont(u8g2_font_5x7_tf);
    char line[56];
    const uint16_t accuracy = accuracyTenths();
    snprintf(line, sizeof(line), "ACC %u.%u%%   MAX COMBO %u   BEST %lu", accuracy / 10,
             accuracy % 10, maxCombo, static_cast<unsigned long>(bestScore));
    display.drawStr(3, 44, line);
    snprintf(line, sizeof(line), "PERF %u  GOOD %u  MISS %u  WRONG %u", perfectCount, goodCount, missCount, wrongCount);
    display.drawStr(3, 53, line);
    display.drawStr(3, 63, "ENTER REPLAY                 BACK MENU");
}

void drawError() {
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(3, 12, "CHART ERROR");
    display.drawHLine(0, 15, SCREEN_WIDTH);
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(3, 31, errorText);
    display.drawStr(3, 61, "ENTER / BACK: MENU");
}

void drawScreen() {
    display.clearBuffer();
    switch (screen) {
        case Screen::Menu: drawMenu(); break;
        case Screen::Countdown:
        case Screen::Playing: drawGameplay(); break;
        case Screen::Paused:
            drawGameplay();
            drawOverlay("PAUSED", "ENTER / ESC: RESUME", "BACK: END RUN");
            break;
        case Screen::Results: drawResults(); break;
        case Screen::Error: drawError(); break;
    }
    display.sendBuffer();
}

void resumeAudioAfterUpdateFailure() {
    audioReady = synth.begin();
    if (audioReady) {
        synth.setTone(selectedTone);
        synth.setVolume(selectedVolume);
    }
}

bool installMainFirmware() {
    synth.allOff(true);
    if (!sdReady) sdReady = SD.begin(SD_CS, SPI);
    File firmware = sdReady ? SD.open(MAIN_FIRMWARE, FILE_READ) : File();
    if (!firmware || firmware.isDirectory() || !firmware.size()) {
        if (firmware) firmware.close();
        copyText(errorText, sizeof(errorText), "Missing /main.bin on SD card");
        screen = Screen::Error;
        return false;
    }

    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(58, 18, "RETURNING TO MAIN");
    display.drawFrame(4, 31, 248, 9);
    display.sendBuffer();
    synth.end();

    const size_t size = firmware.size();
    if (!Update.begin(size, U_FLASH)) {
        firmware.close();
        resumeAudioAfterUpdateFailure();
        copyText(errorText, sizeof(errorText), "Update could not start");
        screen = Screen::Error;
        return false;
    }

    uint8_t buffer[1024];
    size_t written = 0;
    uint8_t previousPercent = 255;
    while (firmware.available()) {
        const size_t count = firmware.read(buffer, sizeof(buffer));
        if (!count || Update.write(buffer, count) != count) break;
        written += count;
        const uint8_t percent = written * 100ULL / size;
        if (percent != previousPercent) {
            previousPercent = percent;
            display.setDrawColor(0);
            display.drawBox(5, 32, 246, 7);
            display.setDrawColor(1);
            display.drawBox(5, 32, percent * 246 / 100, 7);
            display.sendBuffer();
        }
        yield();
    }
    firmware.close();
    if (written == size && Update.end(true) && Update.isFinished()) {
        delay(250);
        ESP.restart();
        return true;
    }
    Update.abort();
    resumeAudioAfterUpdateFailure();
    copyText(errorText, sizeof(errorText), "Firmware update failed");
    screen = Screen::Error;
    return false;
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

    display.clearBuffer();
    display.setFont(u8g2_font_10x20_tf);
    display.drawStr(57, 29, "FALLING KEYS");
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(75, 46, "Loading charts...");
    display.sendBuffer();

    sdReady = SD.begin(SD_CS, SPI);
    scanSongs();
    audioReady = synth.begin();
    if (audioReady) {
        synth.setTone(selectedTone);
        synth.setVolume(selectedVolume);
    }
    chart.generate(Chart::ClassicRush, selectedLanes, selectedDifficulty);
    Serial.printf("FallingKeys audio=%s sd=%s songs=%u heap=%u internal=%u psram=%u voices=%u\n",
                  audioReady ? "ok" : "failed", sdReady ? "ok" : "failed", songCount,
                  ESP.getFreeHeap(), heap_caps_get_free_size(MALLOC_CAP_INTERNAL), ESP.getPsramSize(),
                  TileSynth::MAX_VOICES);
    drawScreen();
}

void loop() {
    keyboard.update();
    if (keyboard.isDown(KeyboardKey::Escape) && keyboard.heldMs(KeyboardKey::Escape) >= EXIT_HOLD_MS) {
        if (!exitTriggered) {
            exitTriggered = true;
            installMainFirmware();
        }
    } else if (!keyboard.isDown(KeyboardKey::Escape)) {
        exitTriggered = false;
    }

    KeyboardEvent event;
    while (keyboard.nextEvent(event)) handleKeyboardEvent(event);
    serviceRun();

    const uint32_t now = millis();
    if (now - lastFrameAt >= FRAME_MS) {
        lastFrameAt = now;
        drawScreen();
    }
    delay(1);
}
