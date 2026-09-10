#include "MidiRecorder.h"

#include <SD.h>

#include "LiveSynth.h"

namespace {
bool writeBytes(File &file, const uint8_t *data, size_t length) {
    return file.write(data, length) == length;
}

bool writeByte(File &file, uint8_t value) {
    return file.write(&value, 1) == 1;
}

bool writeBe16(File &file, uint16_t value) {
    const uint8_t bytes[2] = {
        static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)
    };
    return writeBytes(file, bytes, sizeof(bytes));
}

bool writeBe32(File &file, uint32_t value) {
    const uint8_t bytes[4] = {
        static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16),
        static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)
    };
    return writeBytes(file, bytes, sizeof(bytes));
}

bool writeVlq(File &file, uint32_t value) {
    value = min<uint32_t>(value, 0x0FFFFFFFUL);
    uint8_t bytes[4];
    uint8_t count = 0;
    bytes[count++] = value & 0x7F;
    while ((value >>= 7) != 0 && count < sizeof(bytes)) {
        bytes[count++] = static_cast<uint8_t>((value & 0x7F) | 0x80);
    }
    while (count) {
        if (!writeByte(file, bytes[--count])) return false;
    }
    return true;
}

bool writeTempo(File &file, uint16_t bpm) {
    bpm = max<uint16_t>(40, min<uint16_t>(bpm, 240));
    const uint32_t microseconds = 60000000UL / bpm;
    const uint8_t bytes[6] = {
        0xFF, 0x51, 0x03,
        static_cast<uint8_t>(microseconds >> 16),
        static_cast<uint8_t>(microseconds >> 8),
        static_cast<uint8_t>(microseconds)
    };
    return writeBytes(file, bytes, sizeof(bytes));
}

uint32_t millisecondsToTicks(uint32_t milliseconds, uint16_t bpm, uint64_t &remainder) {
    const uint64_t scaled = remainder +
        static_cast<uint64_t>(milliseconds) * bpm * MidiRecorder::PPQN;
    remainder = scaled % 60000ULL;
    return static_cast<uint32_t>(scaled / 60000ULL);
}
}

void MidiRecorder::start(uint32_t now, uint16_t bpm, uint8_t tone) {
    clear();
    startedAt_ = now;
    initialBpm_ = max<uint16_t>(40, min<uint16_t>(bpm, 240));
    initialTone_ = min<uint8_t>(tone, LiveSynth::TONE_COUNT - 1);
    recording_ = true;
}

void MidiRecorder::stop(uint32_t now) {
    if (!recording_) return;
    durationMs_ = hasNotes_ ? now - startedAt_ : 0;
    recording_ = false;
}

void MidiRecorder::clear() {
    eventCount_ = 0;
    startedAt_ = 0;
    durationMs_ = 0;
    recording_ = false;
    full_ = false;
    hasNotes_ = false;
    savedPath_[0] = '\0';
}

bool MidiRecorder::append(EventType type, uint16_t data, uint8_t value, uint32_t now) {
    if (!recording_) return false;
    if (eventCount_ >= MAX_RECORDED_EVENTS) {
        full_ = true;
        return false;
    }
    Event &entry = events_[eventCount_++];
    entry.timeMs = now - startedAt_;
    entry.data = data;
    entry.type = static_cast<uint8_t>(type);
    entry.value = value;
    durationMs_ = entry.timeMs;
    return true;
}

bool MidiRecorder::addNoteOn(uint8_t note, uint8_t velocity, uint32_t now) {
    const bool appended = append(
        EventType::NoteOn, min<uint8_t>(note, 127), min<uint8_t>(velocity, 127), now);
    if (appended) hasNotes_ = true;
    return appended;
}

bool MidiRecorder::addNoteOff(uint8_t note, uint32_t now) {
    return append(EventType::NoteOff, min<uint8_t>(note, 127), 0, now);
}

bool MidiRecorder::addProgram(uint8_t tone, uint32_t now) {
    return append(EventType::Program, min<uint8_t>(tone, LiveSynth::TONE_COUNT - 1), 0, now);
}

bool MidiRecorder::addSustain(bool enabled, uint32_t now) {
    return append(EventType::Sustain, enabled ? 1 : 0, 0, now);
}

bool MidiRecorder::addTempo(uint16_t bpm, uint32_t now) {
    return append(EventType::Tempo, max<uint16_t>(40, min<uint16_t>(bpm, 240)), 0, now);
}

bool MidiRecorder::saveToSd() {
    savedPath_[0] = '\0';
    if (!hasTake()) return false;
    if (!SD.exists("/recordings") && !SD.mkdir("/recordings")) return false;

    for (uint16_t number = 0; number < 1000; ++number) {
        snprintf(savedPath_, sizeof(savedPath_), "/recordings/KEYS%03u.MID", number);
        if (!SD.exists(savedPath_)) break;
        if (number == 999) {
            savedPath_[0] = '\0';
            return false;
        }
    }

    File file = SD.open(savedPath_, FILE_WRITE);
    if (!file) {
        savedPath_[0] = '\0';
        return false;
    }

    bool okay = true;
    const uint8_t header[] = {'M', 'T', 'h', 'd'};
    const uint8_t trackHeader[] = {'M', 'T', 'r', 'k'};
    okay = okay && writeBytes(file, header, sizeof(header));
    okay = okay && writeBe32(file, 6);
    okay = okay && writeBe16(file, 0);
    okay = okay && writeBe16(file, 1);
    okay = okay && writeBe16(file, PPQN);
    okay = okay && writeBytes(file, trackHeader, sizeof(trackHeader));
    const uint32_t lengthPosition = file.position();
    okay = okay && writeBe32(file, 0);
    const uint32_t trackStart = file.position();

    okay = okay && writeVlq(file, 0) && writeTempo(file, initialBpm_);
    okay = okay && writeVlq(file, 0) && writeByte(file, 0xC0) &&
           writeByte(file, LiveSynth::gmProgram(initialTone_));

    uint32_t previousMs = 0;
    uint16_t activeBpm = initialBpm_;
    uint64_t tickRemainder = 0;
    for (uint16_t index = 0; okay && index < eventCount_; ++index) {
        const Event &entry = events_[index];
        const uint32_t deltaMs = entry.timeMs - previousMs;
        const uint32_t deltaTicks = millisecondsToTicks(deltaMs, activeBpm, tickRemainder);
        previousMs = entry.timeMs;
        okay = writeVlq(file, deltaTicks);
        if (!okay) break;

        switch (static_cast<EventType>(entry.type)) {
            case EventType::NoteOn:
                okay = writeByte(file, 0x90) && writeByte(file, entry.data) &&
                       writeByte(file, entry.value);
                break;
            case EventType::NoteOff:
                okay = writeByte(file, 0x80) && writeByte(file, entry.data) && writeByte(file, 0);
                break;
            case EventType::Program:
                okay = writeByte(file, 0xC0) && writeByte(file, LiveSynth::gmProgram(entry.data));
                break;
            case EventType::Sustain:
                okay = writeByte(file, 0xB0) && writeByte(file, 64) &&
                       writeByte(file, entry.data ? 127 : 0);
                break;
            case EventType::Tempo:
                okay = writeTempo(file, entry.data);
                activeBpm = entry.data;
                break;
        }
    }

    const uint32_t tailMs = durationMs_ > previousMs ? durationMs_ - previousMs : 0;
    okay = okay && writeVlq(file, millisecondsToTicks(tailMs, activeBpm, tickRemainder));
    const uint8_t allOff[] = {0xB0, 64, 0};
    okay = okay && writeBytes(file, allOff, sizeof(allOff));
    const uint8_t panic[] = {0, 0xB0, 123, 0};
    okay = okay && writeBytes(file, panic, sizeof(panic));
    const uint8_t endTrack[] = {0, 0xFF, 0x2F, 0};
    okay = okay && writeBytes(file, endTrack, sizeof(endTrack));

    const uint32_t trackEnd = file.position();
    okay = okay && file.seek(lengthPosition);
    okay = okay && writeBe32(file, trackEnd - trackStart);
    file.flush();
    file.close();

    if (!okay) {
        SD.remove(savedPath_);
        savedPath_[0] = '\0';
    }
    return okay;
}

bool MidiRecorder::isRecording() const { return recording_; }
bool MidiRecorder::hasTake() const { return hasNotes_; }
bool MidiRecorder::isFull() const { return full_; }
uint16_t MidiRecorder::eventCount() const { return eventCount_; }
uint32_t MidiRecorder::durationMs() const { return durationMs_; }
uint32_t MidiRecorder::elapsedMs(uint32_t now) const {
    return recording_ ? now - startedAt_ : durationMs_;
}
uint16_t MidiRecorder::initialBpm() const { return initialBpm_; }
uint8_t MidiRecorder::initialTone() const { return initialTone_; }
const MidiRecorder::Event &MidiRecorder::event(uint16_t index) const { return events_[index]; }
const char *MidiRecorder::savedPath() const { return savedPath_; }
