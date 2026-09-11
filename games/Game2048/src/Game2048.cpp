#include "Game2048.h"

#include <cstring>
#include <limits.h>

namespace {
uint32_t saturatedAdd(uint32_t left, uint32_t right) {
    return UINT32_MAX - left < right ? UINT32_MAX : left + right;
}
}

uint8_t Game2048::lineCell(MoveDirection direction, uint8_t line, uint8_t offset) {
    switch (direction) {
        case MoveDirection::Left: return line * BOARD_SIDE + offset;
        case MoveDirection::Right: return line * BOARD_SIDE + (BOARD_SIDE - 1 - offset);
        case MoveDirection::Up: return offset * BOARD_SIDE + line;
        case MoveDirection::Down: return (BOARD_SIDE - 1 - offset) * BOARD_SIDE + line;
    }
    return 0;
}

bool Game2048::isValidTile(uint32_t value) {
    return value == 0 || (value >= 2 && (value & (value - 1)) == 0);
}

bool Game2048::addRandomTile(uint32_t positionRandom, uint32_t valueRandom) {
    uint8_t empty[CELL_COUNT];
    uint8_t count = 0;
    for (uint8_t index = 0; index < CELL_COUNT; ++index) {
        if (!board_[index]) empty[count++] = index;
    }
    if (!count) {
        spawnedIndex_ = 0xFF;
        return false;
    }
    spawnedIndex_ = empty[positionRandom % count];
    board_[spawnedIndex_] = valueRandom % 10 == 0 ? 4 : 2;
    return true;
}

void Game2048::newGame(uint32_t positionRandomA, uint32_t valueRandomA,
                       uint32_t positionRandomB, uint32_t valueRandomB) {
    std::memset(board_, 0, sizeof(board_));
    score_ = 0;
    moves_ = 0;
    winAcknowledged_ = false;
    undoAvailable_ = false;
    traceCount_ = 0;
    spawnedIndex_ = 0xFF;
    addRandomTile(positionRandomA, valueRandomA);
    addRandomTile(positionRandomB, valueRandomB);
}

bool Game2048::restore(const uint32_t *tiles, uint32_t score, uint32_t moves,
                       bool winAcknowledged) {
    if (!tiles) return false;
    uint8_t occupied = 0;
    for (uint8_t index = 0; index < CELL_COUNT; ++index) {
        if (!isValidTile(tiles[index])) return false;
        if (tiles[index]) ++occupied;
    }
    if (occupied < 2) return false;
    std::memcpy(board_, tiles, sizeof(board_));
    score_ = score;
    moves_ = moves;
    winAcknowledged_ = winAcknowledged;
    undoAvailable_ = false;
    traceCount_ = 0;
    spawnedIndex_ = 0xFF;
    return true;
}

bool Game2048::move(MoveDirection direction, uint32_t positionRandom, uint32_t valueRandom) {
    uint32_t next[CELL_COUNT] = {};
    uint32_t gained = 0;
    traceCount_ = 0;
    spawnedIndex_ = 0xFF;

    for (uint8_t line = 0; line < BOARD_SIDE; ++line) {
        uint32_t values[BOARD_SIDE] = {};
        uint8_t sources[BOARD_SIDE] = {};
        uint8_t sourceCount = 0;
        for (uint8_t offset = 0; offset < BOARD_SIDE; ++offset) {
            const uint8_t source = lineCell(direction, line, offset);
            if (!board_[source]) continue;
            values[sourceCount] = board_[source];
            sources[sourceCount++] = source;
        }

        uint8_t sourceOffset = 0;
        uint8_t destinationOffset = 0;
        while (sourceOffset < sourceCount) {
            const uint8_t destination = lineCell(direction, line, destinationOffset++);
            const bool merges = sourceOffset + 1 < sourceCount &&
                                values[sourceOffset] == values[sourceOffset + 1];
            if (merges) {
                const uint32_t merged = values[sourceOffset] <= 0x40000000UL
                    ? values[sourceOffset] * 2 : 0x80000000UL;
                next[destination] = merged;
                gained = saturatedAdd(gained, merged);
                traces_[traceCount_++] = TileTrace(sources[sourceOffset], destination,
                                                   values[sourceOffset], true);
                traces_[traceCount_++] = TileTrace(sources[sourceOffset + 1], destination,
                                                   values[sourceOffset + 1], true);
                sourceOffset += 2;
            } else {
                next[destination] = values[sourceOffset];
                traces_[traceCount_++] = TileTrace(sources[sourceOffset], destination,
                                                   values[sourceOffset], false);
                ++sourceOffset;
            }
        }
    }

    if (std::memcmp(next, board_, sizeof(board_)) == 0) {
        traceCount_ = 0;
        return false;
    }

    std::memcpy(undoBoard_, board_, sizeof(board_));
    undoScore_ = score_;
    undoMoves_ = moves_;
    undoWinAcknowledged_ = winAcknowledged_;
    undoAvailable_ = true;

    std::memcpy(board_, next, sizeof(board_));
    score_ = saturatedAdd(score_, gained);
    moves_ = saturatedAdd(moves_, 1);
    addRandomTile(positionRandom, valueRandom);
    return true;
}

bool Game2048::undo() {
    if (!undoAvailable_) return false;
    std::memcpy(board_, undoBoard_, sizeof(board_));
    score_ = undoScore_;
    moves_ = undoMoves_;
    winAcknowledged_ = undoWinAcknowledged_;
    undoAvailable_ = false;
    traceCount_ = 0;
    spawnedIndex_ = 0xFF;
    return true;
}

uint32_t Game2048::tile(uint8_t index) const {
    return index < CELL_COUNT ? board_[index] : 0;
}

const uint32_t *Game2048::tiles() const { return board_; }
uint32_t Game2048::score() const { return score_; }
uint32_t Game2048::moveCount() const { return moves_; }

uint32_t Game2048::maxTile() const {
    uint32_t maximum = 0;
    for (uint32_t value : board_) {
        if (value > maximum) maximum = value;
    }
    return maximum;
}

bool Game2048::undoAvailable() const { return undoAvailable_; }

bool Game2048::isGameOver() const {
    for (uint8_t index = 0; index < CELL_COUNT; ++index) {
        if (!board_[index]) return false;
        const uint8_t row = index / BOARD_SIDE;
        const uint8_t col = index % BOARD_SIDE;
        if (col + 1 < BOARD_SIDE && board_[index] == board_[index + 1]) return false;
        if (row + 1 < BOARD_SIDE && board_[index] == board_[index + BOARD_SIDE]) return false;
    }
    return true;
}

bool Game2048::winPending() const {
    return !winAcknowledged_ && maxTile() >= 2048;
}

bool Game2048::winAcknowledged() const { return winAcknowledged_; }
void Game2048::acknowledgeWin() { winAcknowledged_ = true; }

uint8_t Game2048::traceCount() const { return traceCount_; }

const TileTrace &Game2048::trace(uint8_t index) const {
    static const TileTrace empty;
    return index < traceCount_ ? traces_[index] : empty;
}

uint8_t Game2048::spawnedIndex() const { return spawnedIndex_; }

bool Game2048::selfTest() {
    Game2048 game;
    const uint32_t fourPairs[CELL_COUNT] = {
        2, 2, 2, 2,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
    };
    if (!game.restore(fourPairs, 0, 0, false) ||
        !game.move(MoveDirection::Left, 0, 1)) return false;
    if (game.tile(0) != 4 || game.tile(1) != 4 || game.tile(2) != 2 ||
        game.score() != 8 || game.moveCount() != 1 || game.traceCount() != 4) return false;
    if (!game.undo() || game.score() != 0 || game.tile(0) != 2 ||
        game.tile(1) != 2 || game.tile(2) != 2 || game.tile(3) != 2) return false;

    const uint32_t noChain[CELL_COUNT] = {
        2, 2, 4, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
    };
    if (!game.restore(noChain, 0, 0, false) ||
        !game.move(MoveDirection::Left, 0, 1)) return false;
    if (game.tile(0) != 4 || game.tile(1) != 4 || game.score() != 4) return false;

    const uint32_t immovable[CELL_COUNT] = {
        0, 0, 0, 2,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 4
    };
    if (!game.restore(immovable, 7, 3, false)) return false;
    if (game.move(MoveDirection::Right, 0, 1) || game.score() != 7 ||
        game.moveCount() != 3 || game.spawnedIndex() != 0xFF) return false;

    const uint32_t checkerboard[CELL_COUNT] = {
        2, 4, 2, 4,
        4, 2, 4, 2,
        2, 4, 2, 4,
        4, 2, 4, 2
    };
    if (!game.restore(checkerboard, 0, 0, false) || !game.isGameOver()) return false;

    const uint32_t verticalWin[CELL_COUNT] = {
        1024, 0, 0, 2,
        1024, 0, 0, 4,
        0, 0, 0, 0,
        0, 0, 0, 0
    };
    if (!game.restore(verticalWin, 0, 0, false) ||
        !game.move(MoveDirection::Up, 0, 1) || !game.winPending()) return false;
    game.acknowledgeWin();
    if (game.winPending() || !game.winAcknowledged()) return false;
    return true;
}
