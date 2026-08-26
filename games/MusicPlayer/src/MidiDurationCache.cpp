#include "MidiDurationCache.h"

#include <algorithm>
#include <cstdlib>

namespace {
constexpr uint32_t MIDI_HEADER = 0x4D546864UL;
constexpr uint32_t MIDI_TRACK = 0x4D54726BUL;
constexpr uint32_t CACHE_MAGIC = 0x4D444332UL;
constexpr char CACHE_PATH[] = "/.sci_midi_cache";
constexpr size_t SIGNATURE_BYTES = 256;
constexpr uint16_t MAX_TEMPO_EVENTS = 512;
constexpr size_t MAX_CACHE_BYTES = 16384;

struct CacheRecord {
    uint32_t magic;
    uint32_t pathHash;
    uint32_t fileSize;
    uint32_t signature;
    uint32_t durationSeconds;
};

struct TempoEvent {
    uint32_t tick;
    uint32_t tempo;
    uint16_t order;
};

uint32_t fnvByte(uint32_t hash, uint8_t value) {
    return (hash ^ value) * 16777619UL;
}

uint32_t pathHash(const char *path) {
    uint32_t hash = 2166136261UL;
    if (!path) return hash;
    while (*path) hash = fnvByte(hash, static_cast<uint8_t>(*path++));
    return hash;
}

bool readByte(File &file, uint32_t limit, uint8_t &value) {
    if (file.position() >= limit) return false;
    const int next = file.read();
    if (next < 0) return false;
    value = static_cast<uint8_t>(next);
    return true;
}

bool readBigEndian16(File &file, uint16_t &value) {
    const int high = file.read();
    const int low = file.read();
    if (high < 0 || low < 0) return false;
    value = static_cast<uint16_t>((high << 8) | low);
    return true;
}

bool readBigEndian32(File &file, uint32_t &value) {
    value = 0;
    for (uint8_t index = 0; index < 4; ++index) {
        const int byte = file.read();
        if (byte < 0) return false;
        value = (value << 8) | static_cast<uint8_t>(byte);
    }
    return true;
}

bool readVariableLength(File &file, uint32_t limit, uint32_t &value) {
    value = 0;
    for (uint8_t count = 0; count < 4; ++count) {
        uint8_t byte = 0;
        if (!readByte(file, limit, byte)) return false;
        value = (value << 7) | (byte & 0x7F);
        if (!(byte & 0x80)) return true;
    }
    return false;
}

bool skipBytes(File &file, uint32_t limit, uint32_t count) {
    const uint64_t target = static_cast<uint64_t>(file.position()) + count;
    return target <= limit && file.seek(static_cast<uint32_t>(target));
}

uint32_t fileSignature(File &file) {
    const uint32_t size = file.size();
    uint32_t hash = 2166136261UL;
    for (uint8_t shift = 0; shift < 32; shift += 8) hash = fnvByte(hash, size >> shift);

    uint8_t buffer[64];
    file.seek(0);
    size_t remaining = min<size_t>(SIGNATURE_BYTES, size);
    while (remaining) {
        const size_t requested = min(remaining, sizeof(buffer));
        const size_t received = file.read(buffer, requested);
        if (!received) break;
        for (size_t index = 0; index < received; ++index) hash = fnvByte(hash, buffer[index]);
        remaining -= received;
    }

    if (size > SIGNATURE_BYTES) {
        const size_t tailLength = min<size_t>(SIGNATURE_BYTES, size);
        file.seek(size - tailLength);
        remaining = tailLength;
        while (remaining) {
            const size_t requested = min(remaining, sizeof(buffer));
            const size_t received = file.read(buffer, requested);
            if (!received) break;
            for (size_t index = 0; index < received; ++index) hash = fnvByte(hash, buffer[index]);
            remaining -= received;
        }
    }
    file.seek(0);
    return hash;
}

bool scanTrack(File &file, uint32_t end, TempoEvent *events, uint16_t &eventCount,
               uint16_t &eventOrder, uint32_t &maximumTick) {
    uint32_t tick = 0;
    uint16_t eventsSinceYield = 0;
    uint8_t runningStatus = 0;
    while (file.position() < end) {
        if (++eventsSinceYield == 1024) {
            eventsSinceYield = 0;
            yield();
        }
        uint32_t delta = 0;
        if (!readVariableLength(file, end, delta) || UINT32_MAX - tick < delta) return false;
        tick += delta;
        maximumTick = max(maximumTick, tick);

        uint8_t first = 0;
        if (!readByte(file, end, first)) return false;
        uint8_t status = first;
        bool firstIsData = false;
        if (status < 0x80) {
            if (runningStatus < 0x80 || runningStatus >= 0xF0) return false;
            status = runningStatus;
            firstIsData = true;
        } else if (status < 0xF0) {
            runningStatus = status;
        } else {
            runningStatus = 0;
        }

        if (status < 0xF0) {
            const uint8_t command = status & 0xF0;
            uint8_t dataLength = command == 0xC0 || command == 0xD0 ? 1 : 2;
            if (firstIsData) --dataLength;
            if (!skipBytes(file, end, dataLength)) return false;
            continue;
        }

        if (status == 0xFF) {
            uint8_t type = 0;
            uint32_t length = 0;
            if (!readByte(file, end, type) || !readVariableLength(file, end, length)) return false;
            if (type == 0x51 && length == 3) {
                uint8_t a = 0, b = 0, c = 0;
                if (!readByte(file, end, a) || !readByte(file, end, b) || !readByte(file, end, c)) return false;
                const uint32_t tempo = (static_cast<uint32_t>(a) << 16) |
                                       (static_cast<uint32_t>(b) << 8) | c;
                if (tempo >= 1000 && tempo <= 60000000) {
                    if (eventCount >= MAX_TEMPO_EVENTS) return false;
                    events[eventCount++] = {tick, tempo, eventOrder++};
                }
            } else {
                if (!skipBytes(file, end, length)) return false;
            }
            if (type == 0x2F) break;
            continue;
        }

        if (status == 0xF0 || status == 0xF7) {
            uint32_t length = 0;
            if (!readVariableLength(file, end, length) || !skipBytes(file, end, length)) return false;
            continue;
        }

        uint8_t systemLength = 0;
        if (status == 0xF1 || status == 0xF3) systemLength = 1;
        else if (status == 0xF2) systemLength = 2;
        if (!skipBytes(file, end, systemLength)) return false;
    }
    return true;
}

uint32_t scanDuration(File &file) {
    file.seek(0);
    uint32_t chunkId = 0;
    uint32_t headerLength = 0;
    uint16_t format = 0;
    uint16_t declaredTracks = 0;
    uint16_t division = 0;
    if (!readBigEndian32(file, chunkId) || chunkId != MIDI_HEADER ||
        !readBigEndian32(file, headerLength) || headerLength < 6 ||
        !readBigEndian16(file, format) || !readBigEndian16(file, declaredTracks) ||
        !readBigEndian16(file, division) || format > 1 || !declaredTracks ||
        !division || (division & 0x8000)) return 0;
    if (static_cast<uint64_t>(8) + headerLength > file.size() || !file.seek(8 + headerLength)) return 0;

    TempoEvent *events = static_cast<TempoEvent *>(malloc(sizeof(TempoEvent) * MAX_TEMPO_EVENTS));
    if (!events) return 0;
    uint16_t eventCount = 0;
    uint16_t eventOrder = 0;
    uint16_t tracksFound = 0;
    uint32_t maximumTick = 0;
    bool valid = true;

    while (valid && tracksFound < declaredTracks && file.position() + 8 <= file.size()) {
        uint32_t length = 0;
        if (!readBigEndian32(file, chunkId) || !readBigEndian32(file, length)) {
            valid = false;
            break;
        }
        const uint32_t start = file.position();
        const uint64_t end64 = static_cast<uint64_t>(start) + length;
        if (end64 > file.size()) {
            valid = false;
            break;
        }
        const uint32_t end = static_cast<uint32_t>(end64);
        if (chunkId == MIDI_TRACK) {
            ++tracksFound;
            valid = scanTrack(file, end, events, eventCount, eventOrder, maximumTick);
        }
        if (valid && !file.seek(end)) valid = false;
    }

    if (!valid || tracksFound != declaredTracks) {
        free(events);
        return 0;
    }

    std::sort(events, events + eventCount, [](const TempoEvent &left, const TempoEvent &right) {
        return left.tick < right.tick || (left.tick == right.tick && left.order < right.order);
    });
    uint32_t currentTick = 0;
    uint32_t currentTempo = 500000;
    uint64_t durationUs = 0;
    for (uint16_t index = 0; index < eventCount; ++index) {
        if (events[index].tick > maximumTick) break;
        durationUs += static_cast<uint64_t>(events[index].tick - currentTick) * currentTempo / division;
        currentTick = events[index].tick;
        currentTempo = events[index].tempo;
    }
    durationUs += static_cast<uint64_t>(maximumTick - currentTick) * currentTempo / division;
    free(events);
    return static_cast<uint32_t>(min<uint64_t>(UINT32_MAX, (durationUs + 999999) / 1000000));
}
}

uint32_t MidiDurationCache::getOrScan(fs::FS &filesystem, const char *path, bool &cacheHit) {
    cacheHit = false;
    File source = filesystem.open(path, FILE_READ);
    if (!source || source.isDirectory() || !source.size()) {
        if (source) source.close();
        return 0;
    }

    const uint32_t size = source.size();
    const uint32_t signature = fileSignature(source);
    const uint32_t hashedPath = pathHash(path);
    File cache = filesystem.open(CACHE_PATH, FILE_READ);
    if (cache) {
        CacheRecord record{};
        while (cache.read(reinterpret_cast<uint8_t *>(&record), sizeof(record)) == sizeof(record)) {
            if (record.magic == CACHE_MAGIC && record.pathHash == hashedPath &&
                record.fileSize == size && record.signature == signature && record.durationSeconds) {
                cache.close();
                source.close();
                cacheHit = true;
                return record.durationSeconds;
            }
        }
        const bool oversized = cache.size() > MAX_CACHE_BYTES;
        cache.close();
        if (oversized) filesystem.remove(CACHE_PATH);
    }

    const uint32_t duration = scanDuration(source);
    source.close();
    if (!duration) return 0;

    const CacheRecord record{CACHE_MAGIC, hashedPath, size, signature, duration};
    cache = filesystem.open(CACHE_PATH, FILE_APPEND);
    if (cache) {
        cache.write(reinterpret_cast<const uint8_t *>(&record), sizeof(record));
        cache.flush();
        cache.close();
    }
    return duration;
}
