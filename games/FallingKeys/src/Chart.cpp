#include "Chart.h"

#include <algorithm>

namespace {
struct MelodyNote {
    uint8_t pitch;
    uint8_t units;
};

constexpr MelodyNote ODE[] = {
    {64,2},{64,2},{65,2},{67,2},{67,2},{65,2},{64,2},{62,2},
    {60,2},{60,2},{62,2},{64,2},{64,3},{62,1},{62,4},
    {64,2},{64,2},{65,2},{67,2},{67,2},{65,2},{64,2},{62,2},
    {60,2},{60,2},{62,2},{64,2},{62,3},{60,1},{60,4},
    {62,2},{62,2},{64,2},{60,2},{62,2},{64,1},{65,1},{64,2},{60,2},
    {62,2},{64,1},{65,1},{64,2},{62,2},{60,2},{62,2},{55,4},
    {64,2},{64,2},{65,2},{67,2},{67,2},{65,2},{64,2},{62,2},
    {60,2},{60,2},{62,2},{64,2},{62,3},{60,1},{60,4}
};

constexpr MelodyNote TWINKLE[] = {
    {60,2},{60,2},{67,2},{67,2},{69,2},{69,2},{67,4},
    {65,2},{65,2},{64,2},{64,2},{62,2},{62,2},{60,4},
    {67,2},{67,2},{65,2},{65,2},{64,2},{64,2},{62,4},
    {67,2},{67,2},{65,2},{65,2},{64,2},{64,2},{62,4},
    {60,2},{60,2},{67,2},{67,2},{69,2},{69,2},{67,4},
    {65,2},{65,2},{64,2},{64,2},{62,2},{62,2},{60,4}
};

void copyText(char *destination, size_t size, const char *source) {
    if (!destination || !size) return;
    snprintf(destination, size, "%s", source ? source : "");
}

char *trim(char *text) {
    while (*text == ' ' || *text == '\t') ++text;
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) --end;
    *end = '\0';
    return text;
}

bool parseUnsigned(const char *text, uint32_t &value) {
    if (!text || !*text) return false;
    char *end = nullptr;
    const unsigned long parsed = strtoul(text, &end, 10);
    if (end == text || *trim(end) != '\0') return false;
    value = parsed;
    return true;
}

uint8_t melodyLane(uint8_t pitch, uint16_t index, uint8_t lanes) {
    const uint8_t scale = pitch >= 60 ? pitch - 60 : 0;
    uint8_t lane = min<uint8_t>(lanes - 1, scale * lanes / 12);
    if (index && (index % 11 == 0)) lane = (lane + 1) % lanes;
    return lane;
}
}

Chart::Chart() { clear(); }

void Chart::clear() {
    count_ = 0;
    lanes_ = 4;
    bpm_ = 120;
    durationMs_ = 0;
    copyText(title_, sizeof(title_), "Untitled");
    artist_[0] = '\0';
}

bool Chart::append(uint32_t timeMs, uint8_t lane, uint8_t note, uint32_t durationMs) {
    if (count_ >= MAX_EVENTS || lane >= lanes_ || note > 127) return false;
    TileEvent &entry = events_[count_++];
    entry.timeMs = timeMs;
    entry.lane = lane;
    entry.note = note;
    entry.durationMs = static_cast<uint16_t>(min<uint32_t>(durationMs, 12000));
    return true;
}

void Chart::finish() {
    std::sort(events_, events_ + count_, [](const TileEvent &left, const TileEvent &right) {
        if (left.timeMs != right.timeMs) return left.timeMs < right.timeMs;
        return left.lane < right.lane;
    });
    durationMs_ = 0;
    for (uint16_t index = 0; index < count_; ++index) {
        durationMs_ = max<uint32_t>(durationMs_, events_[index].timeMs + events_[index].durationMs);
    }
}

bool Chart::load(fs::FS &filesystem, const char *path, char *error, size_t errorSize) {
    clear();
    File file = filesystem.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        copyText(error, errorSize, "Could not open chart");
        if (file) file.close();
        return false;
    }

    char line[144];
    bool signatureSeen = false;
    int32_t offset = 0;
    uint16_t lineNumber = 0;
    while (file.available()) {
        const size_t length = file.readBytesUntil('\n', line, sizeof(line) - 1);
        line[length] = '\0';
        ++lineNumber;
        char *value = trim(line);
        if (!*value || *value == '#') continue;
        if (!signatureSeen) {
            if (strcmp(value, "PTC1") != 0) {
                copyText(error, errorSize, "Not a PTC1 chart");
                file.close();
                return false;
            }
            signatureSeen = true;
            continue;
        }

        if (!strncmp(value, "title=", 6)) copyText(title_, sizeof(title_), trim(value + 6));
        else if (!strncmp(value, "artist=", 7)) copyText(artist_, sizeof(artist_), trim(value + 7));
        else if (!strncmp(value, "lanes=", 6)) {
            uint32_t parsed = 0;
            if (!parseUnsigned(trim(value + 6), parsed) || parsed < MIN_LANES || parsed > MAX_LANES) {
                copyText(error, errorSize, "lanes must be 4 to 6");
                file.close();
                return false;
            }
            lanes_ = parsed;
        } else if (!strncmp(value, "bpm=", 4)) {
            uint32_t parsed = 0;
            if (parseUnsigned(trim(value + 4), parsed)) bpm_ = max<uint32_t>(30, min<uint32_t>(300, parsed));
        } else if (!strncmp(value, "offset=", 7)) {
            offset = strtol(trim(value + 7), nullptr, 10);
            offset = max<int32_t>(-10000, min<int32_t>(10000, offset));
        } else if (!strncmp(value, "N,", 2)) {
            unsigned long time = 0;
            unsigned int lane = 0;
            unsigned int note = 0;
            unsigned long duration = 0;
            if (sscanf(value + 2, "%lu,%u,%u,%lu", &time, &lane, &note, &duration) != 4 ||
                lane >= lanes_ || note > 127) {
                snprintf(error, errorSize, "Bad note on line %u", lineNumber);
                file.close();
                return false;
            }
            const int64_t shifted = static_cast<int64_t>(time) + offset;
            if (!append(static_cast<uint32_t>(max<int64_t>(0, shifted)), lane, note, duration)) {
                copyText(error, errorSize, count_ >= MAX_EVENTS ? "Chart has over 1024 notes" : "Invalid note");
                file.close();
                return false;
            }
        }
    }
    file.close();
    if (!signatureSeen || !count_) {
        copyText(error, errorSize, "Chart has no notes");
        return false;
    }
    finish();
    if (error && errorSize) error[0] = '\0';
    return true;
}

bool Chart::generate(BuiltIn song, uint8_t lanes, uint8_t difficulty) {
    clear();
    lanes_ = constrain(lanes, MIN_LANES, MAX_LANES);
    difficulty = min<uint8_t>(difficulty, 2);

    if (song == ClassicRush) {
        copyText(title_, sizeof(title_), "Classic Rush");
        copyText(artist_, sizeof(artist_), "Falling Keys");
        bpm_ = difficulty == 0 ? 112 : difficulty == 1 ? 138 : 168;
        const uint16_t step = 60000UL / bpm_ / 2;
        uint32_t random = 0x71A5B39DUL ^ (lanes << 8) ^ difficulty;
        uint8_t previous = 0xFF;
        uint32_t time = 1000;
        const uint16_t groups = difficulty == 0 ? 132 : difficulty == 1 ? 176 : 224;
        for (uint16_t group = 0; group < groups; ++group) {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            uint8_t lane = random % lanes_;
            if (lane == previous && (group & 3U)) lane = (lane + 1 + (random >> 8) % (lanes_ - 1)) % lanes_;
            const uint8_t scale[] = {0, 2, 4, 7, 9, 12};
            const uint8_t note = 60 + scale[lane];
            const bool hold = group > 8 && group % (difficulty == 0 ? 23 : 17) == 0;
            append(time, lane, note, hold ? step * 3 : 0);

            if (difficulty == 2 && group > 12 && group % 13 == 0) {
                uint8_t second = (lane + 2 + ((random >> 16) & 1U)) % lanes_;
                if (second == lane) second = (second + 1) % lanes_;
                append(time, second, 60 + scale[second], 0);
            }
            previous = lane;
            time += step;
            if (difficulty == 0 && group % 12 == 11) time += step;
        }
        finish();
        return true;
    }

    const MelodyNote *melody = song == OdeToJoy ? ODE : TWINKLE;
    const size_t length = song == OdeToJoy ? sizeof(ODE) / sizeof(ODE[0]) : sizeof(TWINKLE) / sizeof(TWINKLE[0]);
    copyText(title_, sizeof(title_), song == OdeToJoy ? "Ode to Joy" : "Twinkle Star");
    copyText(artist_, sizeof(artist_), "Traditional");
    bpm_ = difficulty == 0 ? 96 : difficulty == 1 ? 118 : 144;
    const uint16_t unitMs = 60000UL / bpm_ / 2;
    uint32_t time = 1000;
    for (size_t index = 0; index < length; ++index) {
        const MelodyNote &source = melody[index];
        const uint8_t lane = melodyLane(source.pitch, index, lanes_);
        const uint32_t lengthMs = static_cast<uint32_t>(source.units) * unitMs;
        append(time, lane, source.pitch, source.units >= 4 ? lengthMs - 90 : 0);
        if (difficulty == 2 && index > 4 && index % 12 == 7) {
            const uint8_t harmonyLane = (lane + lanes_ - 2) % lanes_;
            append(time, harmonyLane, max<uint8_t>(36, source.pitch - 12), 0);
        }
        time += lengthMs;
    }
    finish();
    return true;
}

bool Chart::probe(fs::FS &filesystem, const char *path, char *title,
                  size_t titleSize, uint8_t &lanes) {
    File file = filesystem.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) file.close();
        return false;
    }
    char line[112];
    bool signatureSeen = false;
    bool titleSeen = false;
    lanes = 4;
    while (file.available() && file.position() < 1024) {
        const size_t length = file.readBytesUntil('\n', line, sizeof(line) - 1);
        line[length] = '\0';
        char *value = trim(line);
        if (!*value || *value == '#') continue;
        if (!signatureSeen) {
            signatureSeen = strcmp(value, "PTC1") == 0;
            if (!signatureSeen) break;
            continue;
        }
        if (!strncmp(value, "title=", 6)) {
            copyText(title, titleSize, trim(value + 6));
            titleSeen = title && title[0];
        } else if (!strncmp(value, "lanes=", 6)) {
            const uint8_t parsed = atoi(trim(value + 6));
            if (parsed >= MIN_LANES && parsed <= MAX_LANES) lanes = parsed;
        } else if (!strncmp(value, "N,", 2)) break;
    }
    file.close();
    if (!titleSeen && title && titleSize) copyText(title, titleSize, "SD chart");
    return signatureSeen;
}
