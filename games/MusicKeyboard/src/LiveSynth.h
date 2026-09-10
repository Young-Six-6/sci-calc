#pragma once

#include <Arduino.h>
#include <driver/i2s.h>

class LiveSynth {
public:
    static constexpr uint32_t SAMPLE_RATE = 44100;
    static constexpr uint8_t MAX_VOICES = 16;
    static constexpr uint8_t SCOPE_SAMPLES = 128;
    static constexpr uint8_t TONE_COUNT = 6;

    enum Tone : uint8_t {
        Piano = 0,
        ElectricPiano,
        Organ,
        WarmPad,
        Lead,
        Bell
    };

    bool begin();
    void end();
    bool ready() const;

    void noteOn(uint8_t note, uint8_t velocity);
    void noteOff(uint8_t note);
    void allNotesOff(bool immediate = false);
    void setSustain(bool enabled);
    void setTone(uint8_t tone);
    void setVolume(uint8_t volume);
    void click(bool accent);

    uint8_t activeVoices() const;
    uint16_t peakLevel() const;
    uint16_t loadPermille() const;
    uint32_t writeFaults() const;
    void copyScope(int8_t *destination, size_t count) const;

    static const char *toneName(uint8_t tone);
    static uint8_t gmProgram(uint8_t tone);

private:
    enum class CommandType : uint8_t {
        NoteOn,
        NoteOff,
        AllOff,
        Sustain,
        Tone,
        Volume,
        Click
    };

    struct Command {
        CommandType type = CommandType::AllOff;
        uint8_t data1 = 0;
        uint8_t data2 = 0;
    };

    enum class Waveform : uint8_t {
        Sine,
        Piano,
        ElectricPiano,
        Organ,
        Warm,
        Lead,
        Bell
    };

    enum class EnvelopeStage : uint8_t { Attack, Decay, Sustain, Release };

    struct Patch {
        Waveform waveform;
        uint16_t attackMs;
        uint16_t decayMs;
        uint16_t releaseMs;
        uint16_t sustainLevel;
        bool oneShot;
    };

    struct Voice {
        bool active = false;
        bool held = false;
        bool sustained = false;
        bool oneShot = false;
        uint8_t note = 0;
        uint8_t velocity = 0;
        uint8_t overlapCount = 0;
        uint8_t envelopeClock = 0;
        uint8_t tone = Piano;
        Waveform waveform = Waveform::Sine;
        Waveform renderWaveform = Waveform::Sine;
        EnvelopeStage stage = EnvelopeStage::Release;
        uint16_t sustainLevel = 0;
        uint16_t decayMs = 0;
        uint16_t releaseMs = 0;
        uint16_t brightnessEnvelope = 0;
        uint16_t brightnessStep = 0;
        uint16_t hammerEnvelope = 0;
        uint16_t hammerStep = 0;
        uint32_t phase = 0;
        uint32_t phaseIncrement = 0;
        uint32_t age = 0;
        uint32_t noise = 1;
        int32_t envelope = 0;
        int32_t envelopeStep = 1;
        int32_t lastSample = 0;
    };

    static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
    static constexpr uint8_t I2S_DOUT = 15;
    static constexpr uint8_t I2S_LRCK = 22;
    static constexpr uint8_t I2S_BCLK = 21;
    static constexpr size_t AUDIO_BLOCK_FRAMES = 128;
    static constexpr uint8_t COMMAND_QUEUE_SIZE = 48;

    QueueHandle_t commandQueue_ = nullptr;
    TaskHandle_t audioTask_ = nullptr;
    volatile bool running_ = false;
    bool installed_ = false;

    Voice voices_[MAX_VOICES];
    int16_t sineTable_[256] = {};
    uint32_t incrementTable_[128] = {};
    uint8_t currentTone_ = Piano;
    uint8_t currentVolume_ = 7;
    bool sustain_ = false;
    uint32_t voiceAge_ = 0;
    int32_t stealTail_ = 0;
    uint16_t limiterGainQ15_ = 32767;

    uint32_t clickPhase_ = 0;
    uint32_t clickIncrement_ = 0;
    uint16_t clickRemaining_ = 0;
    uint16_t clickLength_ = 0;
    uint32_t clickNoise_ = 0xA341316CUL;

    volatile uint8_t activeVoices_ = 0;
    volatile uint16_t peakLevel_ = 0;
    volatile uint16_t loadPermille_ = 0;
    volatile uint32_t writeFaults_ = 0;
    volatile uint8_t scopeWrite_ = 0;
    volatile int8_t scope_[SCOPE_SAMPLES] = {};
    uint8_t scopeDivider_ = 0;

    bool send(CommandType type, uint8_t data1 = 0, uint8_t data2 = 0);
    static void audioTaskEntry(void *context);
    void audioLoop();
    void processCommands();
    void processCommand(const Command &command);
    void renderBlock(uint32_t *frames, size_t frameCount);
    int16_t renderFrame();
    int16_t oscillator(Voice &voice);
    void advanceEnvelope(Voice &voice);
    void beginRelease(Voice &voice);
    Voice *allocateVoice();
    void startVoice(uint8_t note, uint8_t velocity);
    void stopVoice(uint8_t note);
    void silenceVoices(bool immediate);
    void configurePianoTimbre(Voice &voice);
    Patch patchFor(uint8_t tone, uint8_t note) const;
    static int16_t clampSample(int32_t sample);
};
