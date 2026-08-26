#pragma once

#include <Arduino.h>
#include <FS.h>

class MidiDurationCache {
public:
    static uint32_t getOrScan(fs::FS &filesystem, const char *path, bool &cacheHit);
};
