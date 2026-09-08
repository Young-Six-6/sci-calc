#pragma once

#include <Arduino.h>

struct RomEntry {
    char title[40] = {};
    char path[96] = {};
    uint16_t size = 0;
};

class RomLibrary {
public:
    static constexpr char DIRECTORY[] = "/chip8";
    static constexpr size_t MAX_ROMS = 64;
    static constexpr size_t MAX_ROM_BYTES = 4096 - 0x200;

    bool scan();
    size_t count() const { return count_; }
    bool truncated() const { return truncated_; }
    const RomEntry *at(size_t index) const;

private:
    static constexpr size_t MAX_SCAN_ENTRIES = 256;
    RomEntry entries_[MAX_ROMS];
    size_t count_ = 0;
    bool truncated_ = false;
};
