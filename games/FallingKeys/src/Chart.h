#pragma once

#include <Arduino.h>
#include <FS.h>

struct TileEvent {
    uint32_t timeMs = 0;
    uint16_t durationMs = 0;
    uint8_t lane = 0;
    uint8_t note = 60;
};

class Chart {
public:
    static constexpr uint16_t MAX_EVENTS = 1024;
    static constexpr uint8_t MIN_LANES = 4;
    static constexpr uint8_t MAX_LANES = 6;

    enum BuiltIn : uint8_t { ClassicRush, OdeToJoy, Twinkle };

    Chart();
    void clear();
    bool load(fs::FS &filesystem, const char *path, char *error, size_t errorSize);
    bool generate(BuiltIn song, uint8_t lanes, uint8_t difficulty);

    uint16_t eventCount() const { return count_; }
    const TileEvent &event(uint16_t index) const { return events_[index]; }
    TileEvent &event(uint16_t index) { return events_[index]; }
    uint8_t laneCount() const { return lanes_; }
    uint16_t bpm() const { return bpm_; }
    uint32_t durationMs() const { return durationMs_; }
    const char *title() const { return title_; }
    const char *artist() const { return artist_; }

    static bool probe(fs::FS &filesystem, const char *path, char *title,
                      size_t titleSize, uint8_t &lanes);

private:
    TileEvent events_[MAX_EVENTS];
    uint16_t count_ = 0;
    uint8_t lanes_ = 4;
    uint16_t bpm_ = 120;
    uint32_t durationMs_ = 0;
    char title_[36] = {};
    char artist_[36] = {};

    bool append(uint32_t timeMs, uint8_t lane, uint8_t note, uint32_t durationMs);
    void finish();
};
