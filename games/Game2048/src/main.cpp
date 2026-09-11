#include <Arduino.h>
#include <FS.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <U8g2lib.h>
#include <esp_system.h>

#include <cstddef>
#include <cstdio>
#include <cstring>

#include "Game2048.h"
#include "SafeKeyboard.h"

namespace {
constexpr uint8_t OLED_CS = 5;
constexpr uint8_t OLED_DC = 16;
constexpr uint8_t OLED_RST = 17;
constexpr uint8_t SD_CS = 4;
constexpr uint16_t SCREEN_WIDTH = 256;
constexpr uint8_t SCREEN_HEIGHT = 64;
constexpr uint16_t BOARD_WIDTH = 176;
constexpr uint8_t CELL_WIDTH = BOARD_WIDTH / 4;
constexpr uint8_t CELL_HEIGHT = SCREEN_HEIGHT / 4;
constexpr uint16_t PANEL_LEFT = BOARD_WIDTH + 2;
constexpr uint32_t EXIT_HOLD_MS = 1000;
constexpr uint32_t SLIDE_ANIMATION_MS = 105;
constexpr uint32_t SPAWN_FLASH_MS = 180;
constexpr uint32_t FRAME_MS = 16;
constexpr uint32_t SAVE_IDLE_MS = 1500;
constexpr uint8_t FORCE_SAVE_MOVES = 20;
constexpr char MAIN_FIRMWARE[] = "/main.bin";
constexpr uint32_t SAVE_MAGIC = 0x32303438;
constexpr uint16_t SAVE_VERSION = 1;

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI display(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
SafeKeyboard keyboard;
Game2048 game;

enum class Screen : uint8_t { Playing, Paused, ConfirmNew, Won, GameOver, Error };
Screen screen = Screen::Playing;
Screen confirmReturnScreen = Screen::Playing;
uint8_t menuSelection = 0;
bool animationEnabled = true;
bool animating = false;
bool queuedMoveValid = false;
MoveDirection queuedMove = MoveDirection::Left;
uint32_t animationStartedAt = 0;
uint32_t spawnFlashUntil = 0;
uint32_t bestScore = 0;
bool saveDirty = false;
uint8_t movesSinceSave = 0;
uint32_t saveDueAt = 0;
bool sdReady = false;
bool exitTriggered = false;
bool dirty = true;
uint32_t lastFrameAt = 0;
char notice[28] = {};
uint32_t noticeUntil = 0;

struct SaveData {
    uint32_t magic = SAVE_MAGIC;
    uint16_t version = SAVE_VERSION;
    uint16_t flags = 0;
    uint32_t board[Game2048::CELL_COUNT] = {};
    uint32_t score = 0;
    uint32_t moves = 0;
    uint32_t checksum = 0;
};

void copyText(char *destination, size_t size, const char *source) {
    if (!destination || !size) return;
    std::snprintf(destination, size, "%s", source ? source : "");
}

void setNotice(const char *message, uint32_t durationMs = 700) {
    copyText(notice, sizeof(notice), message);
    noticeUntil = millis() + durationMs;
    dirty = true;
}

uint32_t checksumBytes(const void *data, size_t length) {
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    uint32_t hash = 2166136261UL;
    for (size_t index = 0; index < length; ++index) {
        hash ^= bytes[index];
        hash *= 16777619UL;
    }
    return hash;
}

void fillSaveData(SaveData &state) {
    state = SaveData{};
    state.flags = game.winAcknowledged() ? 1 : 0;
    std::memcpy(state.board, game.tiles(), sizeof(state.board));
    state.score = game.score();
    state.moves = game.moveCount();
    state.checksum = checksumBytes(&state, offsetof(SaveData, checksum));
}

void saveSession() {
    SaveData state;
    fillSaveData(state);
    Preferences preferences;
    if (preferences.begin("game2048", false)) {
        preferences.putBytes("state", &state, sizeof(state));
        preferences.putUInt("best", bestScore);
        preferences.putBool("animation", animationEnabled);
        preferences.end();
        saveDirty = false;
        movesSinceSave = 0;
    }
}

bool loadSession() {
    Preferences preferences;
    if (!preferences.begin("game2048", true)) return false;
    bestScore = preferences.getUInt("best", 0);
    animationEnabled = preferences.getBool("animation", true);
    SaveData state;
    const bool correctSize = preferences.getBytesLength("state") == sizeof(state);
    const bool read = correctSize && preferences.getBytes("state", &state, sizeof(state)) == sizeof(state);
    preferences.end();
    if (!read || state.magic != SAVE_MAGIC || state.version != SAVE_VERSION ||
        state.checksum != checksumBytes(&state, offsetof(SaveData, checksum))) return false;
    if (!game.restore(state.board, state.score, state.moves, state.flags & 1)) return false;
    if (game.score() > bestScore) bestScore = game.score();
    return true;
}

void scheduleSave(bool countMove = true) {
    saveDirty = true;
    saveDueAt = millis() + SAVE_IDLE_MS;
    if (countMove && movesSinceSave < 255) ++movesSinceSave;
}

void startNewGame() {
    game.newGame(esp_random(), esp_random(), esp_random(), esp_random());
    screen = Screen::Playing;
    animating = false;
    queuedMoveValid = false;
    spawnFlashUntil = millis() + SPAWN_FLASH_MS;
    menuSelection = 0;
    scheduleSave(false);
    setNotice("NEW GAME");
}

void finishMoveState() {
    if (game.winPending()) {
        screen = Screen::Won;
        menuSelection = 0;
    } else if (game.isGameOver()) {
        screen = Screen::GameOver;
        menuSelection = game.undoAvailable() ? 0 : 1;
    }
}

void performMove(MoveDirection direction) {
    if (screen != Screen::Playing || animating) return;
    if (!game.move(direction, esp_random(), esp_random())) {
        setNotice("NO MOVE");
        return;
    }
    if (game.score() > bestScore) bestScore = game.score();
    scheduleSave();
    animationStartedAt = millis();
    spawnFlashUntil = animationStartedAt + (animationEnabled ? SLIDE_ANIMATION_MS : 0) + SPAWN_FLASH_MS;
    animating = animationEnabled;
    if (!animating) finishMoveState();
    dirty = true;
}

void performUndo() {
    animating = false;
    queuedMoveValid = false;
    if (game.undo()) {
        screen = Screen::Playing;
        spawnFlashUntil = 0;
        scheduleSave(false);
        setNotice("MOVE UNDONE");
    } else {
        setNotice("NO UNDO");
    }
}

void drawGrid() {
    display.drawVLine(0, 0, SCREEN_HEIGHT);
    display.drawVLine(CELL_WIDTH, 0, SCREEN_HEIGHT);
    display.drawVLine(CELL_WIDTH * 2, 0, SCREEN_HEIGHT);
    display.drawVLine(CELL_WIDTH * 3, 0, SCREEN_HEIGHT);
    display.drawVLine(BOARD_WIDTH - 1, 0, SCREEN_HEIGHT);
    display.drawHLine(0, 0, BOARD_WIDTH);
    display.drawHLine(0, CELL_HEIGHT, BOARD_WIDTH);
    display.drawHLine(0, CELL_HEIGHT * 2, BOARD_WIDTH);
    display.drawHLine(0, CELL_HEIGHT * 3, BOARD_WIDTH);
    display.drawHLine(0, SCREEN_HEIGHT - 1, BOARD_WIDTH);
    for (uint8_t index = 0; index < Game2048::CELL_COUNT; ++index) {
        const uint8_t col = index % 4;
        const uint8_t row = index / 4;
        display.drawPixel(col * CELL_WIDTH + CELL_WIDTH / 2,
                          row * CELL_HEIGHT + CELL_HEIGHT / 2);
    }
}

void clearTileArea(int16_t x, int16_t y) {
    display.setDrawColor(0);
    display.drawBox(x + 1, y + 1, CELL_WIDTH - 2, CELL_HEIGHT - 2);
    display.setDrawColor(1);
}

void formatTileValue(uint32_t value, char *text, size_t size) {
    if (value <= 999999UL) {
        std::snprintf(text, size, "%lu", static_cast<unsigned long>(value));
        return;
    }
    uint8_t exponent = 0;
    while (value > 1) {
        value >>= 1;
        ++exponent;
    }
    std::snprintf(text, size, "2^%u", exponent);
}

void formatCompactValue(uint32_t value, char *text, size_t size) {
    if (value < 10000UL) std::snprintf(text, size, "%lu", static_cast<unsigned long>(value));
    else if (value < 1000000UL) std::snprintf(text, size, "%luK", static_cast<unsigned long>(value / 1000UL));
    else if (value < 1000000000UL) std::snprintf(text, size, "%luM", static_cast<unsigned long>(value / 1000000UL));
    else std::snprintf(text, size, "%luG", static_cast<unsigned long>(value / 1000000000UL));
}

void drawTileAt(int16_t x, int16_t y, uint32_t value, bool emphasis) {
    if (!value) return;
    const bool filled = value >= 128;
    const int16_t tileX = x + 2;
    const int16_t tileY = y + 2;
    constexpr uint8_t tileWidth = CELL_WIDTH - 4;
    constexpr uint8_t tileHeight = CELL_HEIGHT - 4;
    if (filled) display.drawBox(tileX, tileY, tileWidth, tileHeight);
    else display.drawFrame(tileX, tileY, tileWidth, tileHeight);
    if (emphasis) display.drawFrame(x + 1, y + 1, CELL_WIDTH - 2, CELL_HEIGHT - 2);

    char text[16];
    formatTileValue(value, text, sizeof(text));
    display.setFont(u8g2_font_6x10_tf);
    if (display.getStrWidth(text) > tileWidth - 4) display.setFont(u8g2_font_5x7_tf);
    display.setDrawColor(filled ? 0 : 1);
    const int16_t textX = tileX + (tileWidth - display.getStrWidth(text)) / 2;
    const int16_t baseline = tileY + (display.getMaxCharHeight() >= 10 ? 10 : 9);
    display.drawStr(textX, baseline, text);
    display.setDrawColor(1);
}

void cellPosition(uint8_t index, int16_t &x, int16_t &y) {
    x = (index % 4) * CELL_WIDTH;
    y = (index / 4) * CELL_HEIGHT;
}

void drawStaticBoard() {
    drawGrid();
    const bool flash = spawnFlashUntil && static_cast<int32_t>(spawnFlashUntil - millis()) > 0 &&
                       ((millis() / 65) & 1) == 0;
    for (uint8_t index = 0; index < Game2048::CELL_COUNT; ++index) {
        int16_t x = 0;
        int16_t y = 0;
        cellPosition(index, x, y);
        if (game.tile(index)) {
            clearTileArea(x, y);
            drawTileAt(x, y, game.tile(index), flash && index == game.spawnedIndex());
        }
    }
}

void drawAnimatedBoard() {
    drawGrid();
    const uint32_t elapsed = millis() - animationStartedAt;
    const uint16_t linear = elapsed >= SLIDE_ANIMATION_MS
        ? 256 : static_cast<uint16_t>(elapsed * 256UL / SLIDE_ANIMATION_MS);
    const uint16_t remaining = 256 - linear;
    const uint16_t eased = 256 - static_cast<uint16_t>(remaining * remaining / 256UL);
    int16_t positionsX[Game2048::MAX_TRACES] = {};
    int16_t positionsY[Game2048::MAX_TRACES] = {};

    for (uint8_t index = 0; index < game.traceCount(); ++index) {
        const TileTrace &trace = game.trace(index);
        int16_t fromX = 0;
        int16_t fromY = 0;
        int16_t toX = 0;
        int16_t toY = 0;
        cellPosition(trace.from, fromX, fromY);
        cellPosition(trace.to, toX, toY);
        positionsX[index] = fromX + (toX - fromX) * eased / 256;
        positionsY[index] = fromY + (toY - fromY) * eased / 256;
        clearTileArea(positionsX[index], positionsY[index]);
    }
    for (uint8_t index = 0; index < game.traceCount(); ++index) {
        const TileTrace &trace = game.trace(index);
        drawTileAt(positionsX[index], positionsY[index], trace.value,
                   trace.merging && linear > 208);
    }
}

void drawScoreValue(uint32_t value, uint8_t baseline) {
    char text[16];
    std::snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(value));
    display.setFont(u8g2_font_6x10_tf);
    if (display.getStrWidth(text) > SCREEN_WIDTH - PANEL_LEFT - 4) display.setFont(u8g2_font_5x7_tf);
    display.drawStr(253 - display.getStrWidth(text), baseline, text);
}

void drawPlayingPanel() {
    display.drawVLine(BOARD_WIDTH + 1, 0, SCREEN_HEIGHT);
    display.setFont(u8g2_font_10x20_tf);
    display.drawStr(PANEL_LEFT + 18, 18, "2048");
    display.setFont(u8g2_font_4x6_tf);
    display.drawStr(PANEL_LEFT + 1, 27, "SCORE");
    drawScoreValue(game.score(), 37);
    display.setFont(u8g2_font_4x6_tf);
    display.drawStr(PANEL_LEFT + 1, 44, "BEST");
    drawScoreValue(bestScore, 54);
    display.setFont(u8g2_font_4x6_tf);
    if (noticeUntil && static_cast<int32_t>(noticeUntil - millis()) > 0) {
        display.drawStr(PANEL_LEFT + 1, 63, notice);
    } else {
        char moves[8];
        char maximum[8];
        formatCompactValue(game.moveCount(), moves, sizeof(moves));
        formatTileValue(game.maxTile(), maximum, sizeof(maximum));
        char status[24];
        std::snprintf(status, sizeof(status), "M%s T%s", moves, maximum);
        display.drawStr(PANEL_LEFT + 1, 63, status);
    }
}

void drawChoice(uint8_t row, const char *label, uint8_t selected, uint8_t top) {
    if (row == selected) display.drawBox(PANEL_LEFT, top, SCREEN_WIDTH - PANEL_LEFT, 9);
    display.setDrawColor(row == selected ? 0 : 1);
    display.setFont(u8g2_font_4x6_tf);
    display.drawStr(PANEL_LEFT + 2, top + 7, label);
    display.setDrawColor(1);
}

void drawEndPanel() {
    display.drawVLine(BOARD_WIDTH + 1, 0, SCREEN_HEIGHT);
    if (screen == Screen::Won) {
        display.setFont(u8g2_font_5x7_tf);
        display.drawStr(PANEL_LEFT + 18, 8, "YOU WIN");
        display.setFont(u8g2_font_10x20_tf);
        display.drawStr(PANEL_LEFT + 18, 29, "2048");
        drawChoice(0, "CONTINUE", menuSelection, 36);
        drawChoice(1, "NEW GAME", menuSelection, 47);
    } else {
        display.setFont(u8g2_font_10x20_tf);
        display.drawStr(PANEL_LEFT + 19, 18, "GAME");
        display.drawStr(PANEL_LEFT + 19, 36, "OVER");
        drawChoice(0, game.undoAvailable() ? "UNDO" : "NO UNDO", menuSelection, 40);
        drawChoice(1, "NEW GAME", menuSelection, 51);
    }
}

void drawGame() {
    display.setClipWindow(0, 0, BOARD_WIDTH, SCREEN_HEIGHT);
    if (animating) drawAnimatedBoard();
    else drawStaticBoard();
    display.setMaxClipWindow();
    if (screen == Screen::Won || screen == Screen::GameOver) drawEndPanel();
    else drawPlayingPanel();
}

void drawPauseRow(uint8_t row, uint8_t top, const char *label, const char *value = nullptr) {
    const bool selected = row == menuSelection;
    if (selected) display.drawBox(0, top, SCREEN_WIDTH, 10);
    display.setDrawColor(selected ? 0 : 1);
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(3, top + 8, label);
    if (value) display.drawStr(253 - display.getStrWidth(value), top + 8, value);
    display.setDrawColor(1);
}

void drawPause() {
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(2, 9, "2048 PAUSED");
    display.drawHLine(0, 11, SCREEN_WIDTH);
    drawPauseRow(0, 13, "RESUME");
    drawPauseRow(1, 23, "UNDO LAST", game.undoAvailable() ? "READY" : "EMPTY");
    drawPauseRow(2, 33, "NEW GAME");
    drawPauseRow(3, 43, "ANIMATION", animationEnabled ? "ON" : "OFF");
    drawPauseRow(4, 53, "EXIT TO MAIN");
}

void drawConfirmNew() {
    display.setFont(u8g2_font_10x20_tf);
    display.drawStr(75, 20, "NEW GAME?");
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(73, 33, "CURRENT RUN WILL END");
    const char *labels[2] = {"CANCEL", "START"};
    for (uint8_t index = 0; index < 2; ++index) {
        const uint8_t x = 58 + index * 78;
        if (menuSelection == index) display.drawBox(x, 43, 64, 13);
        display.setDrawColor(menuSelection == index ? 0 : 1);
        display.drawStr(x + (64 - display.getStrWidth(labels[index])) / 2, 52, labels[index]);
        display.setDrawColor(1);
    }
}

void drawError() {
    display.setFont(u8g2_font_10x20_tf);
    display.drawStr(81, 25, "2048 ERROR");
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(52, 44, "RULE SELF-TEST FAILED");
}

void drawNoticeOverlay() {
    if (!noticeUntil || static_cast<int32_t>(noticeUntil - millis()) <= 0 ||
        screen == Screen::Playing) return;
    display.setFont(u8g2_font_4x6_tf);
    const uint16_t textWidth = display.getStrWidth(notice);
    const int16_t left = SCREEN_WIDTH - textWidth - 5;
    display.setDrawColor(0);
    display.drawBox(left - 2, 0, textWidth + 4, 9);
    display.setDrawColor(1);
    display.drawFrame(left - 2, 0, textWidth + 4, 9);
    display.drawStr(left, 7, notice);
}

void drawScreen() {
    display.clearBuffer();
    if (screen == Screen::Paused) drawPause();
    else if (screen == Screen::ConfirmNew) drawConfirmNew();
    else if (screen == Screen::Error) drawError();
    else drawGame();
    drawNoticeOverlay();
    display.sendBuffer();
    dirty = false;
}

bool directionFor(KeyboardKey key, MoveDirection &direction) {
    if (key == KeyboardKey::K8) direction = MoveDirection::Up;
    else if (key == KeyboardKey::K2) direction = MoveDirection::Down;
    else if (key == KeyboardKey::K4) direction = MoveDirection::Left;
    else if (key == KeyboardKey::K6) direction = MoveDirection::Right;
    else return false;
    return true;
}

void openNewGameConfirmation(Screen returnScreen) {
    confirmReturnScreen = returnScreen;
    screen = Screen::ConfirmNew;
    menuSelection = 0;
    dirty = true;
}

bool installMainFirmware();

void handlePlayingKey(const KeyboardEvent &event) {
    if (event.action == KeyboardAction::Release) return;
    MoveDirection direction;
    if (directionFor(event.key, direction)) {
        if (animating) {
            queuedMove = direction;
            queuedMoveValid = true;
        } else {
            performMove(direction);
        }
        return;
    }
    if (event.action != KeyboardAction::Press) return;
    if (event.key == KeyboardKey::Escape) {
        screen = Screen::Paused;
        menuSelection = 0;
    } else if (event.key == KeyboardKey::Backspace || event.key == KeyboardKey::Extra) {
        performUndo();
    } else if (event.key == KeyboardKey::Enter && keyboard.isDown(KeyboardKey::Shift)) {
        openNewGameConfirmation(Screen::Playing);
    }
    dirty = true;
}

void toggleAnimation() {
    animationEnabled = !animationEnabled;
    scheduleSave(false);
    setNotice(animationEnabled ? "ANIMATION ON" : "ANIMATION OFF");
}

void handlePauseKey(const KeyboardEvent &event) {
    if (event.action == KeyboardAction::Release || event.key == KeyboardKey::Shift) return;
    if (event.key == KeyboardKey::Escape) {
        screen = Screen::Playing;
        dirty = true;
        return;
    }
    if (event.key == KeyboardKey::K8) menuSelection = (menuSelection + 4) % 5;
    else if (event.key == KeyboardKey::K2) menuSelection = (menuSelection + 1) % 5;
    else if ((event.key == KeyboardKey::K4 || event.key == KeyboardKey::K6) && menuSelection == 3) {
        toggleAnimation();
    } else if (event.key == KeyboardKey::Enter && event.action == KeyboardAction::Press) {
        if (menuSelection == 0) screen = Screen::Playing;
        else if (menuSelection == 1) performUndo();
        else if (menuSelection == 2) openNewGameConfirmation(Screen::Paused);
        else if (menuSelection == 3) toggleAnimation();
        else installMainFirmware();
    }
    dirty = true;
}

void handleConfirmKey(const KeyboardEvent &event) {
    if (event.action == KeyboardAction::Release || event.key == KeyboardKey::Shift) return;
    if (event.key == KeyboardKey::Escape || event.key == KeyboardKey::Backspace) {
        screen = confirmReturnScreen;
    } else if (event.key == KeyboardKey::K4 || event.key == KeyboardKey::K6 ||
               event.key == KeyboardKey::K8 || event.key == KeyboardKey::K2) {
        menuSelection = 1 - menuSelection;
    } else if (event.key == KeyboardKey::Enter && event.action == KeyboardAction::Press) {
        if (menuSelection == 1) startNewGame();
        else screen = confirmReturnScreen;
    }
    dirty = true;
}

void handleWonKey(const KeyboardEvent &event) {
    if (event.action == KeyboardAction::Release || event.key == KeyboardKey::Shift) return;
    if (event.key == KeyboardKey::Backspace || event.key == KeyboardKey::Extra) {
        performUndo();
    } else if (event.key == KeyboardKey::Escape) {
        game.acknowledgeWin();
        screen = Screen::Playing;
        scheduleSave(false);
    } else if (event.key == KeyboardKey::K8 || event.key == KeyboardKey::K2) {
        menuSelection = 1 - menuSelection;
    } else if (event.key == KeyboardKey::Enter && event.action == KeyboardAction::Press) {
        if (menuSelection == 0) {
            game.acknowledgeWin();
            screen = Screen::Playing;
            scheduleSave(false);
        } else {
            openNewGameConfirmation(Screen::Won);
        }
    }
    dirty = true;
}

void handleGameOverKey(const KeyboardEvent &event) {
    if (event.action == KeyboardAction::Release || event.key == KeyboardKey::Shift) return;
    if (event.key == KeyboardKey::Backspace || event.key == KeyboardKey::Extra) {
        performUndo();
    } else if (event.key == KeyboardKey::Escape) {
        screen = Screen::Paused;
        menuSelection = 0;
    } else if (event.key == KeyboardKey::K8 || event.key == KeyboardKey::K2) {
        menuSelection = 1 - menuSelection;
        if (!game.undoAvailable() && menuSelection == 0) menuSelection = 1;
    } else if (event.key == KeyboardKey::Enter && event.action == KeyboardAction::Press) {
        if (menuSelection == 0 && game.undoAvailable()) performUndo();
        else openNewGameConfirmation(Screen::GameOver);
    }
    dirty = true;
}

void handleKeyboardEvent(const KeyboardEvent &event) {
    if (screen == Screen::Playing) handlePlayingKey(event);
    else if (screen == Screen::Paused) handlePauseKey(event);
    else if (screen == Screen::ConfirmNew) handleConfirmKey(event);
    else if (screen == Screen::Won) handleWonKey(event);
    else if (screen == Screen::GameOver) handleGameOverKey(event);
}

void serviceAnimation() {
    if (!animating) return;
    if (millis() - animationStartedAt < SLIDE_ANIMATION_MS) {
        dirty = true;
        return;
    }
    animating = false;
    finishMoveState();
    dirty = true;
    if (screen == Screen::Playing && queuedMoveValid) {
        const MoveDirection direction = queuedMove;
        queuedMoveValid = false;
        performMove(direction);
    } else {
        queuedMoveValid = false;
    }
}

bool installMainFirmware() {
    saveSession();
    if (!sdReady) sdReady = SD.begin(SD_CS, SPI);
    File firmware = sdReady ? SD.open(MAIN_FIRMWARE, FILE_READ) : File();
    if (!firmware || firmware.isDirectory() || !firmware.size()) {
        if (firmware) firmware.close();
        setNotice("MISSING MAIN.BIN", 2500);
        return false;
    }

    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(62, 18, "RETURNING TO MAIN");
    display.drawFrame(4, 31, 248, 9);
    display.sendBuffer();

    const size_t size = firmware.size();
    if (!Update.begin(size, U_FLASH)) {
        firmware.close();
        setNotice("UPDATE COULD NOT START", 2500);
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
    setNotice("FIRMWARE UPDATE FAILED", 2500);
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
    sdReady = SD.begin(SD_CS, SPI);

    if (!Game2048::selfTest()) {
        screen = Screen::Error;
    } else if (!loadSession()) {
        startNewGame();
        saveSession();
    } else {
        finishMoveState();
    }
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
    serviceAnimation();

    const uint32_t now = millis();
    if (noticeUntil && static_cast<int32_t>(now - noticeUntil) >= 0) {
        noticeUntil = 0;
        dirty = true;
    }
    if (spawnFlashUntil && static_cast<int32_t>(now - spawnFlashUntil) >= 0) {
        spawnFlashUntil = 0;
        dirty = true;
    }
    if (saveDirty && !animating &&
        (movesSinceSave >= FORCE_SAVE_MOVES || static_cast<int32_t>(now - saveDueAt) >= 0)) {
        saveSession();
    }
    if ((animating || spawnFlashUntil) && now - lastFrameAt >= FRAME_MS) dirty = true;
    if (dirty && now - lastFrameAt >= FRAME_MS) {
        lastFrameAt = now;
        drawScreen();
    }
    delay(1);
}
