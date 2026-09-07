#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <U8g2lib.h>
#include <esp_system.h>

#include "Keyboard.h"
#include "Maze.h"

namespace {
constexpr uint8_t OLED_CS = 5;
constexpr uint8_t OLED_DC = 16;
constexpr uint8_t OLED_RST = 17;
constexpr uint8_t SD_CS = 4;
constexpr uint16_t SCREEN_WIDTH = 256;
constexpr uint8_t SCREEN_HEIGHT = 64;
constexpr uint8_t CELL_WIDTH = SCREEN_WIDTH / Maze::Width;
constexpr uint8_t CELL_HEIGHT = SCREEN_HEIGHT / Maze::Height;
constexpr uint32_t EXIT_HOLD_MS = 1000;
constexpr uint32_t CLEAR_SCREEN_MS = 650;
constexpr uint32_t BLOCKED_FLASH_MS = 90;
constexpr uint32_t NOTICE_MS = 1800;
constexpr uint16_t FRAME_INTERVAL_MS = 16;
constexpr size_t SEEN_MAZE_CAPACITY = 256;
constexpr char MAIN_FIRMWARE[] = "/main.bin";

static_assert(CELL_WIDTH == 8 && CELL_HEIGHT == 8, "Maze cells must remain square");

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI display(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
Keyboard keyboard;
Maze maze;

enum class GameState : uint8_t { Playing, Cleared };
GameState gameState = GameState::Playing;
uint16_t playerCell = 0;
uint32_t level = 1;
uint8_t trail[(Maze::CellCount + 7) / 8] = {};
uint32_t seenHashes[SEEN_MAZE_CAPACITY] = {};
size_t seenCount = 0;
size_t seenCursor = 0;
uint32_t levelReadyAt = 0;
uint32_t blockedUntil = 0;
uint32_t noticeUntil = 0;
uint32_t lastFrameAt = 0;
String notice;
bool showTrail = true;
bool sdReady = false;
bool dirty = true;
bool exitTriggered = false;

bool trailContains(uint16_t cell) {
    return trail[cell >> 3] & (1U << (cell & 7));
}

void addTrail(uint16_t cell) {
    trail[cell >> 3] |= 1U << (cell & 7);
}

bool noticeActive() {
    return notice.length() && static_cast<int32_t>(noticeUntil - millis()) > 0;
}

void setNotice(const String &text, uint32_t duration = NOTICE_MS) {
    notice = text;
    noticeUntil = millis() + duration;
    dirty = true;
}

void rememberMaze(uint32_t hash) {
    if (seenCount < SEEN_MAZE_CAPACITY) {
        seenHashes[seenCount++] = hash;
        return;
    }
    seenHashes[seenCursor] = hash;
    seenCursor = (seenCursor + 1) % SEEN_MAZE_CAPACITY;
}

void generateLevel() {
    const uint32_t requestedLength = 80 + (level - 1) * 4;
    const uint16_t minimumLength = requestedLength > 156 ? 156 : requestedLength;
    const uint32_t straightReduction = (level - 1) / 4;
    const uint8_t maximumStraightRun = straightReduction >= 5 ? 5 : 10 - straightReduction;
    bool generated = false;
    uint16_t batch = 0;
    while (!generated) {
        const uint32_t seed = esp_random() ^ micros() ^ (level * 0x9E3779B9UL) ^ batch;
        generated = maze.generate(seed, seenHashes, seenCount, minimumLength,
                                  maximumStraightRun, true);
        if ((++batch & 3U) == 0) yield();
    }
    rememberMaze(maze.fingerprint());
    memset(trail, 0, sizeof(trail));
    playerCell = 0;
    addTrail(playerCell);
    gameState = GameState::Playing;
    blockedUntil = 0;
    dirty = true;
}

void drawOverlay(const String &text) {
    display.setFont(u8g2_font_6x10_tf);
    const int width = min(244, static_cast<int>(display.getStrWidth(text.c_str())) + 12);
    const int x = (SCREEN_WIDTH - width) / 2;
    display.setDrawColor(0);
    display.drawBox(x, 23, width, 18);
    display.setDrawColor(1);
    display.drawFrame(x, 23, width, 18);
    display.drawStr(x + (width - display.getStrWidth(text.c_str())) / 2, 36, text.c_str());
}

void drawMaze() {
    display.clearBuffer();
    display.drawFrame(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    for (uint16_t cell = 0; cell < Maze::CellCount; ++cell) {
        const uint8_t row = cell / Maze::Width;
        const uint8_t col = cell % Maze::Width;
        if (col + 1 < Maze::Width && maze.hasWall(cell, Maze::Direction::East)) {
            display.drawVLine((col + 1) * CELL_WIDTH, row * CELL_HEIGHT, CELL_HEIGHT + 1);
        }
        if (row + 1 < Maze::Height && maze.hasWall(cell, Maze::Direction::South)) {
            display.drawHLine(col * CELL_WIDTH, (row + 1) * CELL_HEIGHT, CELL_WIDTH + 1);
        }
    }

    if (showTrail) {
        for (uint16_t cell = 0; cell < Maze::CellCount; ++cell) {
            if (!trailContains(cell) || cell == playerCell) continue;
            const uint8_t col = cell % Maze::Width;
            const uint8_t row = cell / Maze::Width;
            display.drawPixel(col * CELL_WIDTH + 4, row * CELL_HEIGHT + 4);
        }
    }

    display.drawPixel(2, 2);
    display.drawPixel(2, 3);
    display.drawPixel(3, 2);

    constexpr uint16_t goalCell = Maze::CellCount - 1;
    const uint8_t goalX = (goalCell % Maze::Width) * CELL_WIDTH + 4;
    const uint8_t goalY = (goalCell / Maze::Width) * CELL_HEIGHT + 4;
    display.drawFrame(goalX - 2, goalY - 2, 5, 5);
    display.drawPixel(goalX, goalY);

    const uint8_t playerX = (playerCell % Maze::Width) * CELL_WIDTH + 4;
    const uint8_t playerY = (playerCell / Maze::Width) * CELL_HEIGHT + 4;
    display.drawDisc(playerX, playerY, 2);
    if (blockedUntil && static_cast<int32_t>(blockedUntil - millis()) > 0) {
        display.drawCircle(playerX, playerY, 3);
    }

    if (gameState == GameState::Cleared) {
        drawOverlay("LEVEL " + String(level) + " CLEAR");
    } else if (noticeActive()) {
        drawOverlay(notice);
    }
    display.sendBuffer();
}

void attemptMove(Maze::Direction direction) {
    if (gameState != GameState::Playing) return;
    uint16_t destination = playerCell;
    if (!maze.move(playerCell, direction, destination)) {
        blockedUntil = millis() + BLOCKED_FLASH_MS;
        dirty = true;
        return;
    }
    playerCell = destination;
    addTrail(playerCell);
    dirty = true;
    if (playerCell == Maze::CellCount - 1) {
        gameState = GameState::Cleared;
        levelReadyAt = millis() + CLEAR_SCREEN_MS;
    }
}

void handleKeyEvent(const KeyEvent &event) {
    if (event.action == KeyAction::Release || event.key == KeyCode::Escape) return;
    if (event.key == KeyCode::K8) attemptMove(Maze::Direction::North);
    else if (event.key == KeyCode::K2) attemptMove(Maze::Direction::South);
    else if (event.key == KeyCode::K4) attemptMove(Maze::Direction::West);
    else if (event.key == KeyCode::K6) attemptMove(Maze::Direction::East);
    else if (event.action == KeyAction::Press && event.key == KeyCode::K5) generateLevel();
    else if (event.action == KeyAction::Press && event.key == KeyCode::K0) {
        showTrail = !showTrail;
        dirty = true;
    }
}

bool installMainFirmware() {
    if (!sdReady) {
        setNotice("SD unavailable", 2500);
        return false;
    }
    File firmware = SD.open(MAIN_FIRMWARE, FILE_READ);
    if (!firmware || firmware.isDirectory() || !firmware.size()) {
        if (firmware) firmware.close();
        setNotice("Missing main.bin", 2500);
        return false;
    }

    const size_t size = firmware.size();
    if (!Update.begin(size, U_FLASH)) {
        firmware.close();
        setNotice("Update begin failed", 2500);
        return false;
    }

    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(53, 15, "Returning to calculator");
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
    setNotice("Update failed", 2500);
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
    generateLevel();
    drawMaze();
    dirty = false;
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

    const uint32_t now = millis();
    if (gameState == GameState::Cleared && static_cast<int32_t>(now - levelReadyAt) >= 0) {
        ++level;
        generateLevel();
    }
    if (blockedUntil && static_cast<int32_t>(now - blockedUntil) >= 0) {
        blockedUntil = 0;
        dirty = true;
    }
    if (notice.length() && !noticeActive()) {
        notice = "";
        dirty = true;
    }
    if (dirty && now - lastFrameAt >= FRAME_INTERVAL_MS) {
        lastFrameAt = now;
        drawMaze();
        dirty = false;
    }
    delay(1);
}
