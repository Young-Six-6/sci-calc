#pragma once

#include <stddef.h>
#include <stdint.h>

class Maze {
public:
    enum class Direction : uint8_t { North, East, South, West };

    static constexpr uint8_t Width = 32;
    static constexpr uint8_t Height = 8;
    static constexpr uint16_t CellCount = Width * Height;

    bool generate(uint32_t seed, const uint32_t *rejectedHashes, size_t rejectedCount,
                  uint16_t minimumSolutionLength = 0, uint8_t maximumStraightRun = 255,
                  bool requireAllDirections = false);
    bool canMove(uint16_t cell, Direction direction) const;
    bool move(uint16_t cell, Direction direction, uint16_t &destination) const;
    bool hasWall(uint16_t cell, Direction direction) const;
    bool isSolvable() const;
    bool solutionMetrics(uint16_t &length, uint8_t &longestStraightRun,
                         uint8_t &directionMask) const;
    uint32_t fingerprint() const;

private:
    static constexpr uint8_t ALL_WALLS = 0x0F;
    uint8_t walls_[CellCount] = {};
    uint32_t randomState_ = 1;

    uint32_t nextRandom();
    bool neighborFor(uint16_t cell, Direction direction, uint16_t &neighbor) const;
};
