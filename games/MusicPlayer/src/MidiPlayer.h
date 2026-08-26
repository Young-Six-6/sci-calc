#pragma once

#include <Arduino.h>
#include <FS.h>

class MidiPlayer {
public:
    static constexpr uint32_t SAMPLE_RATE = 44100;
    static constexpr uint8_t MAX_VOICES = 32;

    bool open(fs::FS &filesystem, const char *path);
    void stop();
    size_t render(uint32_t *frames, size_t frameCount);

    void setPaused(bool paused);
    void setVolume(uint8_t volume);
    void setVoiceLimit(uint8_t limit);
    void changeTranspose(int8_t delta);
    void cycleToneProfile();

    bool isRunning() const;
    bool isPaused() const;
    bool isFinished() const;
    bool hasError() const;
    const char *errorText() const;
    uint32_t currentTime() const;
    uint32_t totalDuration() const;
    bool durationCacheHit() const;
    uint16_t tempoBpm() const;
    uint8_t activeVoices() const;
    int8_t transpose() const;
    const char *toneProfileName() const;
    uint32_t takeLimiterHits();
    uint32_t takeVoiceSteals();
    uint32_t takeMergedNoteOns();
    uint32_t takeDroppedNoteOns();
    uint32_t takeSustainVoiceSteals();
    uint32_t takeHeldVoiceSteals();

private:
    static constexpr uint8_t MAX_TRACKS = 24;
    static constexpr uint8_t CHANNEL_COUNT = 16;
    static constexpr uint16_t TRACK_BUFFER_SIZE = 256;

    enum class EnvelopeStage : uint8_t { Attack, Decay, Sustain, Release };
    enum class Waveform : uint8_t { Sine, Triangle, Saw, Square, Organ, Piano, Mixed, Noise };
    enum class ToneProfile : uint8_t { GeneralMidi, Piano, Mellow, Synth };

    struct TrackState {
        uint32_t start = 0;
        uint32_t end = 0;
        uint32_t cursor = 0;
        uint32_t eventPosition = 0;
        uint32_t nextTick = 0;
        uint8_t runningStatus = 0;
        uint16_t bufferPosition = 0;
        uint16_t bufferLength = 0;
        uint8_t buffer[TRACK_BUFFER_SIZE] = {};
        bool ended = true;
    };

    struct ChannelState {
        uint16_t pitchBend = 8192;
        uint8_t program = 0;
        uint8_t volume = 100;
        uint8_t expression = 127;
        uint8_t pan = 64;
        uint16_t leftGainQ15 = 0;
        uint16_t rightGainQ15 = 0;
        bool sustain = false;
    };

    struct Patch {
        Waveform waveform;
        uint16_t attackMs;
        uint16_t decayMs;
        uint16_t releaseMs;
        uint16_t sustainLevel;
        bool oneShot;
    };

    struct Voice {
        uint32_t phase = 0;
        uint32_t phaseIncrement = 0;
        uint32_t age = 0;
        uint32_t noise = 1;
        int32_t envelope = 0;
        int32_t envelopeStep = 0;
        uint16_t sustainLevel = 0;
        uint16_t decayMs = 0;
        uint16_t releaseMs = 0;
        uint16_t brightnessEnvelope = 0;
        uint16_t brightnessStep = 0;
        uint16_t hammerEnvelope = 0;
        uint16_t hammerStep = 0;
        uint8_t channel = 0;
        uint8_t note = 0;
        uint8_t velocity = 0;
        uint8_t overlapCount = 1;
        Waveform waveform = Waveform::Sine;
        Waveform renderWaveform = Waveform::Sine;
        EnvelopeStage stage = EnvelopeStage::Release;
        bool active = false;
        bool held = false;
        bool sustained = false;
        bool oneShot = false;
        uint8_t envelopeClock = 0;
        uint16_t baseLeftGainQ15 = 0;
        uint16_t baseRightGainQ15 = 0;
        uint16_t leftGainQ15 = 0;
        uint16_t rightGainQ15 = 0;
        int32_t lastLeft = 0;
        int32_t lastRight = 0;
    };

    fs::File file_;
    TrackState tracks_[MAX_TRACKS] = {};
    ChannelState channels_[CHANNEL_COUNT] = {};
    Voice voices_[MAX_VOICES] = {};
    int16_t sineTable_[256] = {};

    uint8_t trackCount_ = 0;
    uint8_t voiceLimit_ = MAX_VOICES;
    uint8_t masterVolume_ = 12;
    int8_t transpose_ = 0;
    ToneProfile toneProfile_ = ToneProfile::GeneralMidi;
    uint16_t division_ = 480;
    uint32_t tempoUsPerQuarter_ = 500000;
    uint32_t currentTick_ = 0;
    uint32_t nextEventTick_ = 0;
    uint64_t framesUntilEvent_ = 0;
    uint64_t sampleRemainder_ = 0;
    uint64_t renderedFrames_ = 0;
    uint32_t totalDuration_ = 0;
    uint32_t voiceAge_ = 0;
    int32_t stealTailLeft_ = 0;
    int32_t stealTailRight_ = 0;
    uint16_t limiterGainQ15_ = 32767;
    uint32_t limiterHits_ = 0;
    uint32_t voiceSteals_ = 0;
    uint32_t mergedNoteOns_ = 0;
    uint32_t droppedNoteOns_ = 0;
    uint32_t sustainVoiceSteals_ = 0;
    uint32_t heldVoiceSteals_ = 0;
    bool running_ = false;
    bool paused_ = false;
    bool finished_ = false;
    bool failed_ = false;
    bool tracksRemain_ = false;
    bool durationCacheHit_ = false;
    const char *error_ = "";

    bool readHeader();
    bool primeTrack(TrackState &track);
    bool readVariableLength(TrackState &track, uint32_t &value);
    bool dispatchEvent(TrackState &track);
    bool advanceTrack(TrackState &track);
    bool scheduleNextEvent();
    bool processDueEvents();
    bool skipBytes(TrackState &track, uint32_t count);
    int readTrackByte(TrackState &track);
    void fail(const char *message);
    void resetChannels();
    void updateChannelGain(uint8_t channel);

    Patch patchFor(uint8_t channel, uint8_t note) const;
    Voice *allocateVoice(uint8_t channel, uint8_t velocity);
    void noteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void noteOff(uint8_t channel, uint8_t note);
    void allNotesOff(uint8_t channel, bool immediate = false);
    void setSustain(uint8_t channel, bool enabled);
    void setPitchBend(uint8_t channel, uint16_t bend);
    void updateVoicePitch(Voice &voice);
    void configurePianoTimbre(Voice &voice, bool strike);
    void updateVoiceGain(Voice &voice);
    void beginRelease(Voice &voice);
    int16_t oscillator(Voice &voice);
    void advanceEnvelope(Voice &voice);
    uint32_t renderFrame();
};
