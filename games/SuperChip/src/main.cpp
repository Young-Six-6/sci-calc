#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <U8g2lib.h>
#include <esp_system.h>
#include <esp_timer.h>

#include <cstdlib>
#include <cstring>

#include "AudioBeeper.h"
#include "Keyboard.h"
#include "RomLibrary.h"

extern "C" {
#include "vendor/octemu/core.h"
}

namespace {
constexpr uint8_t OLED_CS = 5;
constexpr uint8_t OLED_DC = 16;
constexpr uint8_t OLED_RST = 17;
constexpr uint8_t SD_CS = 4;
constexpr uint16_t SCREEN_WIDTH = 256;
constexpr uint8_t SCREEN_HEIGHT = 64;
constexpr uint8_t VIEWPORT_X = (SCREEN_WIDTH - OCTEMU_GFX_WIDTH) / 2;
constexpr uint32_t EXIT_HOLD_MS = 1000;
constexpr uint32_t NOTICE_MS = 1800;
constexpr int64_t FRAME_US = 16667;
constexpr uint8_t BROWSER_ROWS = 5;
constexpr uint8_t PAUSE_ROWS = 5;
constexpr char MAIN_FIRMWARE[] = "/main.bin";

struct CompatibilityProfile {
    const char *name;
    OctEmuMode mode;
    uint8_t defaultSpeed;
};

constexpr uint16_t SPEEDS[] = {
    5, 8, 10, 12, 15, 20, 30, 45, 60, 90, 120, 160, 200, 300, 500
};

constexpr CompatibilityProfile PROFILES[] = {
    {"MODERN", OCTEMU_MODE_OCTO, 12},
    {"SUPER", OCTEMU_MODE_SCHIP, 12},
    {"CLASSIC", OCTEMU_MODE_CHIP8, 4}
};

struct VirtualKeyBinding {
    KeyCode physical;
    uint8_t virtualKey;
};

constexpr VirtualKeyBinding VIRTUAL_KEYS[] = {
    {KeyCode::K0, 0x0}, {KeyCode::K1, 0x1}, {KeyCode::K2, 0x2},
    {KeyCode::K3, 0x3}, {KeyCode::K4, 0x4}, {KeyCode::K5, 0x5},
    {KeyCode::K6, 0x6}, {KeyCode::K7, 0x7}, {KeyCode::K8, 0x8},
    {KeyCode::K9, 0x9}, {KeyCode::Slash, 0xA}, {KeyCode::Star, 0xB},
    {KeyCode::Minus, 0xC}, {KeyCode::Plus, 0xD}, {KeyCode::Power, 0xE},
    {KeyCode::Dot, 0xF}
};

enum class Screen : uint8_t { Browser, Running, Pause, Fault };

enum PauseItem : uint8_t {
    PAUSE_RESUME,
    PAUSE_RESTART,
    PAUSE_LIBRARY,
    PAUSE_PROFILE,
    PAUSE_SPEED,
    PAUSE_SOUND,
    PAUSE_HUD,
    PAUSE_ITEM_COUNT
};

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI display(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
Keyboard keyboard;
RomLibrary romLibrary;
AudioBeeper beeper;
OctEmu *emulator = nullptr;
uint8_t *activeRom = nullptr;

Screen screen = Screen::Browser;
size_t selectedRom = 0;
size_t browserTop = 0;
uint8_t pauseSelection = 0;
uint8_t pauseTop = 0;
uint8_t profileIndex = 0;
uint8_t speedIndex = PROFILES[0].defaultSpeed;
uint16_t suppressedVirtualKeys = 0;
uint16_t faultPc = 0;
uint16_t faultOpcode = 0;
uint32_t escapePressedAt = 0;
uint32_t noticeUntil = 0;
int64_t nextFrameAt = 0;
char activeTitle[40] = {};
char notice[64] = {};
bool sdReady = false;
bool soundEnabled = true;
bool showHud = true;
bool dirty = true;
bool exitTriggered = false;

template <typename T, size_t N>
constexpr size_t arrayCount(const T (&)[N]) {
    return N;
}

bool noticeActive() {
    return notice[0] && static_cast<int32_t>(noticeUntil - millis()) > 0;
}

void setNotice(const char *text, uint32_t duration = NOTICE_MS) {
    strlcpy(notice, text, sizeof(notice));
    noticeUntil = millis() + duration;
    dirty = true;
}

void clearNoticeIfExpired() {
    if (notice[0] && !noticeActive()) {
        notice[0] = '\0';
        dirty = true;
    }
}

void fitText(const char *source, char *destination, size_t capacity, uint16_t maxWidth) {
    if (!capacity) return;
    strlcpy(destination, source ? source : "", capacity);
    if (display.getStrWidth(destination) <= maxWidth) return;

    size_t length = strlen(destination);
    while (length > 1 && display.getStrWidth(destination) > maxWidth) {
        destination[--length] = '\0';
    }
    if (length > 1) destination[length - 1] = '>';
}

void drawCentered(uint8_t baseline, const char *text) {
    const int width = display.getStrWidth(text);
    display.drawStr(max(0, (static_cast<int>(SCREEN_WIDTH) - width) / 2), baseline, text);
}

void drawOverlay(const char *text) {
    display.setFont(u8g2_font_6x10_tf);
    char fitted[42];
    fitText(text, fitted, sizeof(fitted), 236);
    const int width = min(246, static_cast<int>(display.getStrWidth(fitted)) + 12);
    const int x = (SCREEN_WIDTH - width) / 2;
    display.setDrawColor(0);
    display.drawBox(x, 23, width, 18);
    display.setDrawColor(1);
    display.drawFrame(x, 23, width, 18);
    display.drawStr(x + (width - display.getStrWidth(fitted)) / 2, 36, fitted);
}

void keepBrowserSelectionVisible() {
    if (!romLibrary.count()) {
        selectedRom = browserTop = 0;
        return;
    }
    if (selectedRom >= romLibrary.count()) selectedRom = romLibrary.count() - 1;
    if (selectedRom < browserTop) browserTop = selectedRom;
    if (selectedRom >= browserTop + BROWSER_ROWS) {
        browserTop = selectedRom - BROWSER_ROWS + 1;
    }
}

void keepPauseSelectionVisible() {
    if (pauseSelection < pauseTop) pauseTop = pauseSelection;
    if (pauseSelection >= pauseTop + PAUSE_ROWS) {
        pauseTop = pauseSelection - PAUSE_ROWS + 1;
    }
}

void drawBrowser() {
    display.clearBuffer();
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(2, 7, "SUPER-CHIP ROMS");
    char countText[12];
    snprintf(countText, sizeof(countText), romLibrary.truncated() ? "%u+" : "%u",
             static_cast<unsigned>(romLibrary.count()));
    display.drawStr(174 - display.getStrWidth(countText), 7, countText);
    display.drawHLine(0, 9, 180);
    display.drawVLine(181, 0, SCREEN_HEIGHT);

    if (!sdReady) {
        display.setFont(u8g2_font_6x10_tf);
        display.drawStr(8, 27, "SD card unavailable");
        display.drawStr(8, 43, "Hold ESC for main");
    } else if (!romLibrary.count()) {
        display.setFont(u8g2_font_6x10_tf);
        display.drawStr(8, 25, "No .ch8 ROMs");
        display.drawStr(8, 40, "Copy files to /chip8");
        display.setFont(u8g2_font_5x7_tf);
        display.drawStr(8, 56, "BACKSPACE: RESCAN");
    } else {
        display.setFont(u8g2_font_5x7_tf);
        for (uint8_t row = 0; row < BROWSER_ROWS; ++row) {
            const size_t index = browserTop + row;
            if (index >= romLibrary.count()) break;
            const int y = 11 + row * 9;
            const RomEntry *entry = romLibrary.at(index);
            char title[38];
            fitText(entry->title, title, sizeof(title), 166);
            if (index == selectedRom) {
                display.drawBox(1, y, 178, 9);
                display.setDrawColor(0);
            }
            display.drawStr(5, y + 7, title);
            display.setDrawColor(1);
        }
        const RomEntry *selected = romLibrary.at(selectedRom);
        if (selected) {
            char sizeText[20];
            snprintf(sizeText, sizeof(sizeText), "%u BYTES", selected->size);
            display.drawStr(4, 63, sizeText);
            display.drawStr(111, 63, "ENTER: RUN");
        }
    }

    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(187, 8, "MODE");
    display.drawStr(187, 17, PROFILES[profileIndex].name);
    display.drawStr(187, 30, "CYC/FR");
    char speedText[8];
    snprintf(speedText, sizeof(speedText), "%u", SPEEDS[speedIndex]);
    display.drawStr(187, 39, speedText);
    display.drawStr(187, 52, soundEnabled && beeper.available() ? "SOUND ON" : "SOUND OFF");
    display.drawStr(187, 62, "4/6 MODE");

    if (noticeActive()) drawOverlay(notice);
    display.sendBuffer();
}

void drawGame() {
    display.clearBuffer();
    if (emulator) {
        display.setBitmapMode(1);
        display.drawBitmap(VIEWPORT_X, 0, OCTEMU_GFX_WIDTH / 8, OCTEMU_GFX_HEIGHT,
                           &emulator->gfx[0][0]);
        display.setBitmapMode(0);
    }

    if (showHud) {
        display.setFont(u8g2_font_5x7_tf);
        display.drawVLine(VIEWPORT_X - 2, 0, SCREEN_HEIGHT);
        display.drawVLine(VIEWPORT_X + OCTEMU_GFX_WIDTH + 1, 0, SCREEN_HEIGHT);
        display.drawStr(2, 7, "CHIP-8");
        display.drawStr(2, 18, PROFILES[profileIndex].name);
        char speedText[12];
        snprintf(speedText, sizeof(speedText), "%u/F", SPEEDS[speedIndex]);
        display.drawStr(2, 29, speedText);
        display.drawStr(2, 40, soundEnabled && beeper.available() ? "SND ON" : "SND OFF");
        display.drawStr(2, 63, "SHIFT+0");

        char titleLine[16];
        fitText(activeTitle, titleLine, sizeof(titleLine), 57);
        display.drawStr(197, 7, titleLine);
        display.drawStr(197, 29, "ENTER");
        display.drawStr(197, 38, "PAUSE");
        display.drawStr(197, 54, "HOLD ESC");
        display.drawStr(197, 63, "MAIN");
    }
    if (noticeActive()) drawOverlay(notice);
    display.sendBuffer();
    if (emulator) emulator->gfx_dirty = false;
}

void pauseItemText(uint8_t item, char *output, size_t capacity) {
    switch (item) {
        case PAUSE_RESUME:
            strlcpy(output, "Resume", capacity);
            break;
        case PAUSE_RESTART:
            strlcpy(output, "Restart ROM", capacity);
            break;
        case PAUSE_LIBRARY:
            strlcpy(output, "ROM Library", capacity);
            break;
        case PAUSE_PROFILE:
            snprintf(output, capacity, "Mode: %s", PROFILES[profileIndex].name);
            break;
        case PAUSE_SPEED:
            snprintf(output, capacity, "Speed: %u cycles/frame", SPEEDS[speedIndex]);
            break;
        case PAUSE_SOUND:
            snprintf(output, capacity, "Sound: %s", soundEnabled && beeper.available() ? "ON" : "OFF");
            break;
        case PAUSE_HUD:
            snprintf(output, capacity, "Side HUD: %s", showHud ? "ON" : "OFF");
            break;
        default:
            output[0] = '\0';
            break;
    }
}

void drawPause() {
    display.clearBuffer();
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(2, 7, "PAUSED");
    char title[38];
    fitText(activeTitle, title, sizeof(title), 205);
    display.drawStr(48, 7, title);
    display.drawHLine(0, 9, SCREEN_WIDTH);

    for (uint8_t row = 0; row < PAUSE_ROWS; ++row) {
        const uint8_t item = pauseTop + row;
        if (item >= PAUSE_ITEM_COUNT) break;
        const int y = 11 + row * 10;
        char label[40];
        pauseItemText(item, label, sizeof(label));
        char fitted[40];
        fitText(label, fitted, sizeof(fitted), 244);
        if (item == pauseSelection) {
            display.drawBox(1, y, 254, 10);
            display.setDrawColor(0);
        }
        display.drawStr(6, y + 8, fitted);
        display.setDrawColor(1);
    }
    display.drawStr(5, 63, "8/2 SELECT   4/6 CHANGE   ENTER");
    if (noticeActive()) drawOverlay(notice);
    display.sendBuffer();
}

void drawFault() {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(4, 10, "EMULATOR HALTED");
    display.setFont(u8g2_font_5x7_tf);
    char title[42];
    fitText(activeTitle, title, sizeof(title), 246);
    display.drawStr(4, 22, title);
    char detail[40];
    snprintf(detail, sizeof(detail), "PC %04X   OPCODE %04X", faultPc, faultOpcode);
    display.drawStr(4, 34, detail);
    display.drawStr(4, 47, "ENTER: RESTART");
    display.drawStr(4, 57, "BACKSPACE: ROM LIBRARY");
    display.drawStr(166, 57, "ESC: BACK");
    if (noticeActive()) drawOverlay(notice);
    display.sendBuffer();
}

void renderCurrentScreen() {
    switch (screen) {
        case Screen::Browser: drawBrowser(); break;
        case Screen::Running: drawGame(); break;
        case Screen::Pause: drawPause(); break;
        case Screen::Fault: drawFault(); break;
    }
    dirty = false;
}

void stopSound() {
    beeper.setEnabled(false);
}

void setRunning() {
    screen = Screen::Running;
    nextFrameAt = esp_timer_get_time();
    dirty = true;
}

void restartEmulator(bool resume) {
    if (!emulator) return;
    stopSound();
    emulator->mode = PROFILES[profileIndex].mode;
    octemu_reset(emulator);
    emulator->gfx_dirty = true;
    if (resume) setRunning();
    else dirty = true;
}

void leaveForLibrary(const char *message = nullptr) {
    stopSound();
    screen = Screen::Browser;
    keepBrowserSelectionVisible();
    if (message) setNotice(message);
    dirty = true;
}

bool loadSelectedRom() {
    const RomEntry *entry = romLibrary.at(selectedRom);
    if (!entry) {
        setNotice("No ROM selected");
        return false;
    }

    File file = SD.open(entry->path, FILE_READ);
    if (!file || file.isDirectory() || file.size() != entry->size) {
        if (file) file.close();
        setNotice("Could not open ROM");
        return false;
    }

    uint8_t *data = static_cast<uint8_t *>(malloc(entry->size));
    if (!data) {
        file.close();
        setNotice("Not enough memory");
        return false;
    }

    size_t received = 0;
    while (received < entry->size) {
        const size_t count = file.read(data + received, entry->size - received);
        if (!count) break;
        received += count;
        yield();
    }
    file.close();
    if (received != entry->size) {
        free(data);
        setNotice("ROM read failed");
        return false;
    }

    OctEmu *next = octemu_new(PROFILES[profileIndex].mode);
    if (!next || octemu_set_rom(next, data, entry->size)) {
        if (next) octemu_free(next);
        free(data);
        setNotice("ROM load failed");
        return false;
    }

    stopSound();
    if (emulator) octemu_free(emulator);
    free(activeRom);
    emulator = next;
    activeRom = data;
    emulator->gfx_dirty = true;
    strlcpy(activeTitle, entry->title, sizeof(activeTitle));
    pauseSelection = pauseTop = 0;
    suppressedVirtualKeys = 0;
    setRunning();
    return true;
}

void changeSpeed(int direction) {
    const int next = constrain(static_cast<int>(speedIndex) + direction, 0,
                               static_cast<int>(arrayCount(SPEEDS)) - 1);
    if (next == speedIndex) return;
    speedIndex = static_cast<uint8_t>(next);
    dirty = true;
}

void changeProfile(int direction, bool useProfileDefault) {
    int next = static_cast<int>(profileIndex) + direction;
    if (next < 0) next = arrayCount(PROFILES) - 1;
    if (next >= static_cast<int>(arrayCount(PROFILES))) next = 0;
    profileIndex = static_cast<uint8_t>(next);
    if (useProfileDefault) speedIndex = PROFILES[profileIndex].defaultSpeed;
    if (emulator && screen != Screen::Browser) restartEmulator(screen == Screen::Running);
    dirty = true;
}

void rescanLibrary() {
    if (!sdReady) {
        setNotice("SD card unavailable");
        return;
    }
    char rememberedPath[96];
    rememberedPath[0] = '\0';
    const RomEntry *previous = romLibrary.at(selectedRom);
    if (previous) strlcpy(rememberedPath, previous->path, sizeof(rememberedPath));
    if (!romLibrary.scan()) {
        setNotice("ROM scan failed");
        return;
    }
    selectedRom = 0;
    for (size_t index = 0; index < romLibrary.count(); ++index) {
        const RomEntry *entry = romLibrary.at(index);
        if (entry && rememberedPath[0] && !strcmp(entry->path, rememberedPath)) {
            selectedRom = index;
            break;
        }
    }
    keepBrowserSelectionVisible();
    setNotice(romLibrary.count() ? "ROM list refreshed" : "Folder /chip8 is empty");
}

int8_t virtualKeyFor(KeyCode physical) {
    for (const VirtualKeyBinding &binding : VIRTUAL_KEYS) {
        if (binding.physical == physical) return binding.virtualKey;
    }
    return -1;
}

uint16_t currentVirtualKeypad() {
    if (keyboard.isDown(KeyCode::Layer)) return 0;
    uint16_t state = 0;
    for (const VirtualKeyBinding &binding : VIRTUAL_KEYS) {
        const uint16_t bit = 1U << binding.virtualKey;
        if (keyboard.isDown(binding.physical) && !(suppressedVirtualKeys & bit)) state |= bit;
    }
    return state;
}

void enterPause() {
    stopSound();
    screen = Screen::Pause;
    pauseSelection = pauseTop = 0;
    dirty = true;
}

void activatePauseItem() {
    switch (pauseSelection) {
        case PAUSE_RESUME:
            setRunning();
            break;
        case PAUSE_RESTART:
            restartEmulator(true);
            break;
        case PAUSE_LIBRARY:
            leaveForLibrary();
            break;
        case PAUSE_PROFILE:
            changeProfile(1, true);
            break;
        case PAUSE_SPEED:
            changeSpeed(1);
            break;
        case PAUSE_SOUND:
            soundEnabled = !soundEnabled;
            dirty = true;
            break;
        case PAUSE_HUD:
            showHud = !showHud;
            dirty = true;
            break;
    }
}

void changePauseItem(int direction) {
    switch (pauseSelection) {
        case PAUSE_PROFILE:
            changeProfile(direction, true);
            break;
        case PAUSE_SPEED:
            changeSpeed(direction);
            break;
        case PAUSE_SOUND:
            soundEnabled = !soundEnabled;
            dirty = true;
            break;
        case PAUSE_HUD:
            showHud = !showHud;
            dirty = true;
            break;
        default:
            break;
    }
}

bool handleLayerHotkey(const KeyEvent &event) {
    if (!keyboard.isDown(KeyCode::Layer) || event.key == KeyCode::Layer ||
        event.action == KeyAction::Release) {
        return false;
    }

    const int8_t virtualKey = virtualKeyFor(event.key);
    if (virtualKey >= 0) suppressedVirtualKeys |= 1U << virtualKey;

    if (event.key == KeyCode::K4) {
        changeSpeed(-1);
    } else if (event.key == KeyCode::K6) {
        changeSpeed(1);
    } else if (event.action == KeyAction::Press && event.key == KeyCode::K7) {
        changeProfile(-1, true);
    } else if (event.action == KeyAction::Press && event.key == KeyCode::K9) {
        changeProfile(1, true);
    } else if (event.action == KeyAction::Press && event.key == KeyCode::K0) {
        showHud = !showHud;
        dirty = true;
    } else if (event.action == KeyAction::Press && event.key == KeyCode::Dot) {
        soundEnabled = !soundEnabled;
        if (!soundEnabled) stopSound();
        dirty = true;
    } else if (event.action == KeyAction::Press && event.key == KeyCode::Enter) {
        restartEmulator(true);
    } else if (event.action == KeyAction::Press && event.key == KeyCode::Backspace) {
        leaveForLibrary();
    } else if (event.action == KeyAction::Press && event.key == KeyCode::K5) {
        enterPause();
    }
    return true;
}

void handleShortEscape() {
    if (screen == Screen::Running) enterPause();
    else if (screen == Screen::Pause) setRunning();
    else if (screen == Screen::Fault) leaveForLibrary();
    else setNotice("Hold ESC to return to main");
}

void handleBrowserEvent(const KeyEvent &event) {
    if (event.action == KeyAction::Release) return;
    if (event.key == KeyCode::K8 && romLibrary.count()) {
        selectedRom = selectedRom ? selectedRom - 1 : romLibrary.count() - 1;
        keepBrowserSelectionVisible();
        dirty = true;
    } else if (event.key == KeyCode::K2 && romLibrary.count()) {
        selectedRom = (selectedRom + 1) % romLibrary.count();
        keepBrowserSelectionVisible();
        dirty = true;
    } else if (event.key == KeyCode::K4) {
        changeProfile(-1, true);
    } else if (event.key == KeyCode::K6) {
        changeProfile(1, true);
    } else if (event.key == KeyCode::Minus) {
        changeSpeed(-1);
    } else if (event.key == KeyCode::Plus) {
        changeSpeed(1);
    } else if (event.action == KeyAction::Press &&
               (event.key == KeyCode::Enter || event.key == KeyCode::K5)) {
        loadSelectedRom();
    } else if (event.action == KeyAction::Press && event.key == KeyCode::Backspace) {
        rescanLibrary();
    } else if (event.action == KeyAction::Press && event.key == KeyCode::Dot) {
        soundEnabled = !soundEnabled;
        dirty = true;
    }
}

void handlePauseEvent(const KeyEvent &event) {
    if (event.action == KeyAction::Release) return;
    if (event.key == KeyCode::K8) {
        pauseSelection = pauseSelection ? pauseSelection - 1 : PAUSE_ITEM_COUNT - 1;
        keepPauseSelectionVisible();
        dirty = true;
    } else if (event.key == KeyCode::K2) {
        pauseSelection = (pauseSelection + 1) % PAUSE_ITEM_COUNT;
        keepPauseSelectionVisible();
        dirty = true;
    } else if (event.key == KeyCode::K4) {
        changePauseItem(-1);
    } else if (event.key == KeyCode::K6) {
        changePauseItem(1);
    } else if (event.action == KeyAction::Press && event.key == KeyCode::Enter) {
        activatePauseItem();
    } else if (event.action == KeyAction::Press && event.key == KeyCode::Backspace) {
        leaveForLibrary();
    }
}

void handleFaultEvent(const KeyEvent &event) {
    if (event.action != KeyAction::Press) return;
    if (event.key == KeyCode::Enter) restartEmulator(true);
    else if (event.key == KeyCode::Backspace) leaveForLibrary();
}

void handleKeyEvent(const KeyEvent &event) {
    const int8_t virtualKey = virtualKeyFor(event.key);
    if (event.action == KeyAction::Release && virtualKey >= 0) {
        suppressedVirtualKeys &= ~(1U << virtualKey);
    }

    if (event.key == KeyCode::Escape) {
        if (event.action == KeyAction::Press) escapePressedAt = millis();
        else if (event.action == KeyAction::Release && !exitTriggered &&
                 millis() - escapePressedAt < EXIT_HOLD_MS) {
            handleShortEscape();
        }
        return;
    }

    if (screen != Screen::Browser && handleLayerHotkey(event)) return;

    switch (screen) {
        case Screen::Browser: handleBrowserEvent(event); break;
        case Screen::Running:
            if (event.action == KeyAction::Press && event.key == KeyCode::Enter) enterPause();
            break;
        case Screen::Pause: handlePauseEvent(event); break;
        case Screen::Fault: handleFaultEvent(event); break;
    }
}

void runEmulatorFrame() {
    if (!emulator || screen != Screen::Running) return;

    const uint16_t keypad = currentVirtualKeypad();
    bool failed = false;
    for (uint16_t cycle = 0; cycle < SPEEDS[speedIndex]; ++cycle) {
        faultPc = emulator->pc;
        if (faultPc <= OCTEMU_MEM_SIZE - 2) {
            faultOpcode = emulator->mem[faultPc] << 8 | emulator->mem[faultPc + 1];
        } else {
            faultOpcode = 0;
        }
        if (octemu_eval(emulator, keypad)) {
            failed = true;
            break;
        }
        if (emulator->mode == OCTEMU_MODE_CHIP8 && emulator->gfx_dirty) break;
    }

    if (failed) {
        stopSound();
        if (faultOpcode == 0x00FD) leaveForLibrary("Program exited");
        else {
            screen = Screen::Fault;
            dirty = true;
        }
        return;
    }

    beeper.setEnabled(soundEnabled && emulator->sound != 0);
    if (emulator->gfx_dirty || dirty) drawGame();
    dirty = false;
    octemu_tick(emulator);
}

bool installMainFirmware() {
    stopSound();
    if (!sdReady) {
        setNotice("SD card unavailable");
        return false;
    }

    File firmware = SD.open(MAIN_FIRMWARE, FILE_READ);
    if (!firmware || firmware.isDirectory() || !firmware.size()) {
        if (firmware) firmware.close();
        setNotice("Missing /main.bin");
        return false;
    }
    const size_t size = firmware.size();
    if (!Update.begin(size, U_FLASH)) {
        firmware.close();
        setNotice("Update begin failed");
        return false;
    }

    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    drawCentered(16, "Returning to calculator");
    display.drawFrame(4, 28, 248, 9);
    display.sendBuffer();

    uint8_t buffer[1024];
    size_t written = 0;
    uint8_t previousPercent = 255;
    while (firmware.available()) {
        const size_t count = firmware.read(buffer, sizeof(buffer));
        if (!count || Update.write(buffer, count) != count) break;
        written += count;
        const uint8_t percent = static_cast<uint8_t>(written * 100ULL / size);
        if (percent != previousPercent) {
            previousPercent = percent;
            display.setDrawColor(0);
            display.drawBox(5, 29, 246, 7);
            display.setDrawColor(1);
            display.drawBox(5, 29, percent * 246 / 100, 7);
            display.sendBuffer();
        }
        yield();
    }
    firmware.close();

    if (written == size && Update.end(true) && Update.isFinished()) {
        delay(200);
        ESP.restart();
        return true;
    }
    Update.abort();
    setNotice("Update failed");
    dirty = true;
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
    srand(esp_random());

    sdReady = SD.begin(SD_CS, SPI);
    if (sdReady && !romLibrary.scan()) setNotice("Could not open /chip8");
    beeper.begin();
    renderCurrentScreen();
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
    while (keyboard.nextEvent(event)) handleKeyEvent(event);
    clearNoticeIfExpired();

    if (screen == Screen::Running) {
        const int64_t now = esp_timer_get_time();
        if (now >= nextFrameAt) {
            if (now - nextFrameAt > FRAME_US * 4) nextFrameAt = now;
            nextFrameAt += FRAME_US;
            runEmulatorFrame();
        }
    } else if (dirty) {
        renderCurrentScreen();
    }
    delay(1);
}
