#pragma once

#include <Arduino.h>
#include <driver/i2s.h>

class TileSynth {
public:
    static constexpr uint8_t MAX_VOICES = 8;
    enum Tone : uint8_t { Piano, Arcade, TONE_COUNT };

    bool begin();
    void end();
    bool ready() const { return installed_ && running_; }
    void play(uint8_t note, uint8_t velocity, uint16_t durationMs);
    void stop(uint8_t note);
    void allOff(bool immediate = false);
    void setTone(uint8_t tone);
    void setVolume(uint8_t volume);
    void miss();

    static const char *toneName(uint8_t tone);
    uint8_t activeVoices() const { return activeVoices_; }
    uint16_t loadPermille() const { return loadPermille_; }
    uint32_t writeFaults() const { return writeFaults_; }

private:
    static constexpr uint32_t SAMPLE_RATE = 44100;
    static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
    static constexpr uint8_t I2S_DOUT = 15;
    static constexpr uint8_t I2S_LRCK = 22;
    static constexpr uint8_t I2S_BCLK = 21;
    static constexpr size_t BLOCK_FRAMES = 128;
    static constexpr uint8_t COMMAND_QUEUE_SIZE = 32;

    enum class CommandType : uint8_t { Play, Stop, AllOff, Tone, Volume, Miss };
    struct Command {
        CommandType type = CommandType::AllOff;
        uint8_t note = 0;
        uint8_t value = 0;
        uint16_t durationMs = 0;
    };

    enum class Envelope : uint8_t { Attack, Sustain, Release };
    struct Voice {
        bool active = false;
        uint8_t note = 0;
        uint8_t velocity = 0;
        uint8_t tone = Piano;
        Envelope envelope = Envelope::Release;
        uint32_t phase = 0;
        uint32_t increment = 0;
        uint32_t gateSamples = 0;
        uint32_t age = 0;
        int32_t level = 0;
        int32_t levelStep = 1;
        uint16_t brightness = 0;
        uint16_t brightnessStep = 1;
    };

    QueueHandle_t commandQueue_ = nullptr;
    TaskHandle_t audioTask_ = nullptr;
    volatile bool running_ = false;
    bool installed_ = false;
    Voice voices_[MAX_VOICES];
    int16_t sine_[256] = {};
    uint32_t increments_[128] = {};
    uint8_t tone_ = Piano;
    uint8_t volume_ = 7;
    uint32_t age_ = 0;
    uint32_t noise_ = 0xC001D00DUL;
    uint16_t thudSamples_ = 0;
    uint16_t thudLength_ = 0;
    uint16_t limiterGain_ = 32767;

    volatile uint8_t activeVoices_ = 0;
    volatile uint16_t loadPermille_ = 0;
    volatile uint32_t writeFaults_ = 0;

    bool send(CommandType type, uint8_t note = 0, uint8_t value = 0, uint16_t durationMs = 0);
    static void taskEntry(void *context);
    void audioLoop();
    void processCommands();
    void process(const Command &command);
    Voice *allocateVoice();
    void startVoice(uint8_t note, uint8_t velocity, uint16_t durationMs);
    void release(Voice &voice, uint16_t releaseMs = 180);
    int16_t renderSample();
    int16_t oscillator(Voice &voice);
    static int16_t clamp(int32_t value);
};
