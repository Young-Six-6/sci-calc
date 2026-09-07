#include "Maze.h"

#include <string.h>

namespace {
constexpr uint8_t WALL_BITS[4] = {0x01, 0x02, 0x04, 0x08};
constexpr Maze::Direction OPPOSITE[4] = {
    Maze::Direction::South,
    Maze::Direction::West,
    Maze::Direction::North,
    Maze::Direction::East
};

bool bitIsSet(const uint8_t *bits, uint16_t index) {
    return bits[index >> 3] & (1U << (index & 7));
}

void setBit(uint8_t *bits, uint16_t index) {
    bits[index >> 3] |= 1U << (index & 7);
}
}

uint32_t Maze::nextRandom() {
    randomState_ ^= randomState_ << 13;
    randomState_ ^= randomState_ >> 17;
    randomState_ ^= randomState_ << 5;
    return randomState_;
}

bool Maze::neighborFor(uint16_t cell, Direction direction, uint16_t &neighbor) const {
    if (cell >= CellCount) return false;
    const uint8_t row = cell / Width;
    const uint8_t col = cell % Width;
    switch (direction) {
        case Direction::North:
            if (row == 0) return false;
            neighbor = cell - Width;
            return true;
        case Direction::East:
            if (col + 1 >= Width) return false;
            neighbor = cell + 1;
            return true;
        case Direction::South:
            if (row + 1 >= Height) return false;
            neighbor = cell + Width;
            return true;
        case Direction::West:
            if (col == 0) return false;
            neighbor = cell - 1;
            return true;
    }
    return false;
}

bool Maze::generate(uint32_t seed, const uint32_t *rejectedHashes, size_t rejectedCount,
                    uint16_t minimumSolutionLength, uint8_t maximumStraightRun,
                    bool requireAllDirections) {
    for (uint8_t attempt = 0; attempt < 64; ++attempt) {
        memset(walls_, ALL_WALLS, sizeof(walls_));
        uint8_t visited[(CellCount + 7) / 8] = {};
        uint16_t stack[CellCount];
        uint16_t stackDepth = 0;
        uint16_t current = 0;
        uint16_t visitedCount = 1;
        setBit(visited, current);

        randomState_ = seed + 0x9E3779B9UL * (attempt + 1);
        if (!randomState_) randomState_ = 0xA341316CUL;

        while (visitedCount < CellCount) {
            Direction candidates[4];
            uint8_t candidateCount = 0;
            for (uint8_t value = 0; value < 4; ++value) {
                const Direction direction = static_cast<Direction>(value);
                uint16_t neighbor = 0;
                if (neighborFor(current, direction, neighbor) && !bitIsSet(visited, neighbor)) {
                    candidates[candidateCount++] = direction;
                }
            }

            if (candidateCount) {
                const Direction direction = candidates[nextRandom() % candidateCount];
                uint16_t neighbor = 0;
                neighborFor(current, direction, neighbor);
                stack[stackDepth++] = current;
                walls_[current] &= ~WALL_BITS[static_cast<uint8_t>(direction)];
                const Direction opposite = OPPOSITE[static_cast<uint8_t>(direction)];
                walls_[neighbor] &= ~WALL_BITS[static_cast<uint8_t>(opposite)];
                current = neighbor;
                setBit(visited, current);
                ++visitedCount;
            } else if (stackDepth) {
                current = stack[--stackDepth];
            } else {
                break;
            }
        }

        if (visitedCount != CellCount || !isSolvable()) continue;
        uint16_t solutionLength = 0;
        uint8_t longestStraightRun = 0;
        uint8_t directionMask = 0;
        if (!solutionMetrics(solutionLength, longestStraightRun, directionMask) ||
            solutionLength < minimumSolutionLength ||
            longestStraightRun > maximumStraightRun ||
            (requireAllDirections && directionMask != 0x0F)) {
            seed ^= nextRandom() + 0xC2B2AE35UL;
            continue;
        }
        const uint32_t hash = fingerprint();
        bool rejected = false;
        for (size_t index = 0; index < rejectedCount; ++index) {
            if (rejectedHashes[index] == hash) {
                rejected = true;
                break;
            }
        }
        if (!rejected) return true;
        seed ^= nextRandom() + 0x85EBCA6BUL;
    }
    return false;
}

bool Maze::hasWall(uint16_t cell, Direction direction) const {
    if (cell >= CellCount) return true;
    return walls_[cell] & WALL_BITS[static_cast<uint8_t>(direction)];
}

bool Maze::canMove(uint16_t cell, Direction direction) const {
    uint16_t neighbor = 0;
    return !hasWall(cell, direction) && neighborFor(cell, direction, neighbor);
}

bool Maze::move(uint16_t cell, Direction direction, uint16_t &destination) const {
    if (!canMove(cell, direction)) return false;
    return neighborFor(cell, direction, destination);
}

bool Maze::isSolvable() const {
    uint8_t visited[(CellCount + 7) / 8] = {};
    uint16_t queue[CellCount];
    uint16_t head = 0;
    uint16_t tail = 0;
    uint16_t visitedCount = 1;
    queue[tail++] = 0;
    setBit(visited, 0);

    while (head < tail) {
        const uint16_t cell = queue[head++];
        for (uint8_t value = 0; value < 4; ++value) {
            const Direction direction = static_cast<Direction>(value);
            uint16_t neighbor = 0;
            if (hasWall(cell, direction) || !neighborFor(cell, direction, neighbor) ||
                bitIsSet(visited, neighbor)) {
                continue;
            }
            setBit(visited, neighbor);
            queue[tail++] = neighbor;
            ++visitedCount;
        }
    }
    return visitedCount == CellCount;
}

bool Maze::solutionMetrics(uint16_t &length, uint8_t &longestStraightRun,
                           uint8_t &directionMask) const {
    uint16_t parent[CellCount];
    uint8_t arrivalDirection[CellCount] = {};
    uint16_t queue[CellCount];
    for (uint16_t cell = 0; cell < CellCount; ++cell) parent[cell] = 0xFFFF;

    uint16_t head = 0;
    uint16_t tail = 0;
    queue[tail++] = 0;
    parent[0] = 0;
    while (head < tail && parent[CellCount - 1] == 0xFFFF) {
        const uint16_t cell = queue[head++];
        for (uint8_t value = 0; value < 4; ++value) {
            const Direction direction = static_cast<Direction>(value);
            uint16_t neighbor = 0;
            if (hasWall(cell, direction) || !neighborFor(cell, direction, neighbor) ||
                parent[neighbor] != 0xFFFF) {
                continue;
            }
            parent[neighbor] = cell;
            arrivalDirection[neighbor] = value;
            queue[tail++] = neighbor;
        }
    }
    if (parent[CellCount - 1] == 0xFFFF) return false;

    length = 0;
    longestStraightRun = 0;
    directionMask = 0;
    uint8_t previousDirection = 0xFF;
    uint8_t straightRun = 0;
    uint16_t cell = CellCount - 1;
    while (cell != 0) {
        const uint8_t direction = arrivalDirection[cell];
        directionMask |= 1U << direction;
        if (direction == previousDirection) ++straightRun;
        else straightRun = 1;
        if (straightRun > longestStraightRun) longestStraightRun = straightRun;
        previousDirection = direction;
        cell = parent[cell];
        ++length;
    }
    return true;
}

uint32_t Maze::fingerprint() const {
    uint32_t hash = 2166136261UL;
    for (uint16_t cell = 0; cell < CellCount; ++cell) {
        hash ^= walls_[cell];
        hash *= 16777619UL;
    }
    return hash;
}
