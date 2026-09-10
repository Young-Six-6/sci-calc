#pragma once

#include <Arduino.h>

class MidiRecorder {
public:
    static constexpr uint16_t MAX_RECORDED_EVENTS = 2048;
    static constexpr uint16_t PPQN = 480;

    enum class EventType : uint8_t { NoteOn, NoteOff, Program, Sustain, Tempo };

    struct Event {
        uint32_t timeMs = 0;
        uint16_t data = 0;
        uint8_t type = static_cast<uint8_t>(EventType::NoteOff);
        uint8_t value = 0;
    };

    void start(uint32_t now, uint16_t bpm, uint8_t tone);
    void stop(uint32_t now);
    void clear();

    bool addNoteOn(uint8_t note, uint8_t velocity, uint32_t now);
    bool addNoteOff(uint8_t note, uint32_t now);
    bool addProgram(uint8_t tone, uint32_t now);
    bool addSustain(bool enabled, uint32_t now);
    bool addTempo(uint16_t bpm, uint32_t now);

    bool saveToSd();
    bool isRecording() const;
    bool hasTake() const;
    bool isFull() const;
    uint16_t eventCount() const;
    uint32_t durationMs() const;
    uint32_t elapsedMs(uint32_t now) const;
    uint16_t initialBpm() const;
    uint8_t initialTone() const;
    const Event &event(uint16_t index) const;
    const char *savedPath() const;

private:
    Event events_[MAX_RECORDED_EVENTS];
    uint16_t eventCount_ = 0;
    uint32_t startedAt_ = 0;
    uint32_t durationMs_ = 0;
    uint16_t initialBpm_ = 120;
    uint8_t initialTone_ = 0;
    bool recording_ = false;
    bool full_ = false;
    bool hasNotes_ = false;
    char savedPath_[32] = {};

    bool append(EventType type, uint16_t data, uint8_t value, uint32_t now);
};
