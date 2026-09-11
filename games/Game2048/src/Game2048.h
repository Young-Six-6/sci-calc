#pragma once

#include <stdint.h>

enum class MoveDirection : uint8_t { Up, Down, Left, Right };

struct TileTrace {
    TileTrace() : from(0), to(0), value(0), merging(false) {}
    TileTrace(uint8_t source, uint8_t destination, uint32_t tileValue, bool isMerging)
        : from(source), to(destination), value(tileValue), merging(isMerging) {}

    uint8_t from;
    uint8_t to;
    uint32_t value;
    bool merging;
};

class Game2048 {
public:
    static constexpr uint8_t BOARD_SIDE = 4;
    static constexpr uint8_t CELL_COUNT = BOARD_SIDE * BOARD_SIDE;
    static constexpr uint8_t MAX_TRACES = CELL_COUNT;

    void newGame(uint32_t positionRandomA, uint32_t valueRandomA,
                 uint32_t positionRandomB, uint32_t valueRandomB);
    bool restore(const uint32_t *tiles, uint32_t score, uint32_t moves, bool winAcknowledged);
    bool move(MoveDirection direction, uint32_t positionRandom, uint32_t valueRandom);
    bool undo();

    uint32_t tile(uint8_t index) const;
    const uint32_t *tiles() const;
    uint32_t score() const;
    uint32_t moveCount() const;
    uint32_t maxTile() const;
    bool undoAvailable() const;
    bool isGameOver() const;
    bool winPending() const;
    bool winAcknowledged() const;
    void acknowledgeWin();

    uint8_t traceCount() const;
    const TileTrace &trace(uint8_t index) const;
    uint8_t spawnedIndex() const;

    static bool selfTest();

private:
    uint32_t board_[CELL_COUNT] = {};
    uint32_t score_ = 0;
    uint32_t moves_ = 0;
    bool winAcknowledged_ = false;

    uint32_t undoBoard_[CELL_COUNT] = {};
    uint32_t undoScore_ = 0;
    uint32_t undoMoves_ = 0;
    bool undoWinAcknowledged_ = false;
    bool undoAvailable_ = false;

    TileTrace traces_[MAX_TRACES];
    uint8_t traceCount_ = 0;
    uint8_t spawnedIndex_ = 0xFF;

    static uint8_t lineCell(MoveDirection direction, uint8_t line, uint8_t offset);
    static bool isValidTile(uint32_t value);
    bool addRandomTile(uint32_t positionRandom, uint32_t valueRandom);
};
