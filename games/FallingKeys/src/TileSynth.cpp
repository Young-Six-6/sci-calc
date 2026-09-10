#include "TileSynth.h"

#include <math.h>

namespace {
constexpr uint8_t VOLUME_TABLE[11] = {0, 8, 12, 18, 26, 38, 54, 76, 108, 150, 205};
}

bool TileSynth::begin() {
    if (installed_) return true;
    for (uint16_t index = 0; index < 256; ++index) {
        sine_[index] = static_cast<int16_t>(sinf(index * TWO_PI / 256.0f) * 32767.0f);
    }
    for (uint16_t note = 0; note < 128; ++note) {
        const float frequency = 440.0f * powf(2.0f, (static_cast<int16_t>(note) - 69) / 12.0f);
        increments_[note] = static_cast<uint32_t>(frequency * 4294967296.0 / SAMPLE_RATE);
    }

    commandQueue_ = xQueueCreate(COMMAND_QUEUE_SIZE, sizeof(Command));
    if (!commandQueue_) return false;

    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = SAMPLE_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = static_cast<i2s_comm_format_t>(I2S_COMM_FORMAT_STAND_I2S);
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 8;
    config.dma_buf_len = BLOCK_FRAMES;
    config.use_apll = false;
    config.tx_desc_auto_clear = true;
    config.fixed_mclk = 0;
    if (i2s_driver_install(I2S_PORT, &config, 0, nullptr) != ESP_OK) {
        vQueueDelete(commandQueue_);
        commandQueue_ = nullptr;
        return false;
    }

    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = I2S_BCLK;
    pins.ws_io_num = I2S_LRCK;
    pins.data_out_num = I2S_DOUT;
    pins.data_in_num = I2S_PIN_NO_CHANGE;
    if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) {
        i2s_driver_uninstall(I2S_PORT);
        vQueueDelete(commandQueue_);
        commandQueue_ = nullptr;
        return false;
    }

    for (Voice &voice : voices_) voice = Voice{};
    i2s_zero_dma_buffer(I2S_PORT);
    running_ = true;
    installed_ = true;
    if (xTaskCreatePinnedToCore(taskEntry, "falling-audio", 4096, this, 3, &audioTask_, 0) != pdPASS) {
        running_ = false;
        installed_ = false;
        i2s_driver_uninstall(I2S_PORT);
        vQueueDelete(commandQueue_);
        commandQueue_ = nullptr;
        audioTask_ = nullptr;
        return false;
    }
    return true;
}

void TileSynth::end() {
    if (!installed_) return;
    running_ = false;
    const uint32_t started = millis();
    while (audioTask_ && millis() - started < 500) delay(1);
    if (audioTask_) {
        vTaskDelete(audioTask_);
        audioTask_ = nullptr;
    }
    i2s_zero_dma_buffer(I2S_PORT);
    i2s_driver_uninstall(I2S_PORT);
    if (commandQueue_) {
        vQueueDelete(commandQueue_);
        commandQueue_ = nullptr;
    }
    installed_ = false;
}

bool TileSynth::send(CommandType type, uint8_t note, uint8_t value, uint16_t durationMs) {
    if (!ready() || !commandQueue_) return false;
    Command command;
    command.type = type;
    command.note = note;
    command.value = value;
    command.durationMs = durationMs;
    return xQueueSend(commandQueue_, &command, pdMS_TO_TICKS(5)) == pdTRUE;
}

void TileSynth::play(uint8_t note, uint8_t velocity, uint16_t durationMs) {
    if (note < 128 && velocity) send(CommandType::Play, note, velocity, max<uint16_t>(60, durationMs));
}

void TileSynth::stop(uint8_t note) { send(CommandType::Stop, note); }
void TileSynth::allOff(bool immediate) { send(CommandType::AllOff, 0, immediate ? 1 : 0); }
void TileSynth::setTone(uint8_t tone) { send(CommandType::Tone, 0, min<uint8_t>(tone, TONE_COUNT - 1)); }
void TileSynth::setVolume(uint8_t volume) { send(CommandType::Volume, 0, min<uint8_t>(volume, 10)); }
void TileSynth::miss() { send(CommandType::Miss); }

const char *TileSynth::toneName(uint8_t tone) {
    return tone == Arcade ? "Arcade" : "Piano";
}

void TileSynth::taskEntry(void *context) { static_cast<TileSynth *>(context)->audioLoop(); }

void TileSynth::audioLoop() {
    uint32_t frames[BLOCK_FRAMES];
    while (running_) {
        processCommands();
        const uint32_t started = micros();
        for (size_t index = 0; index < BLOCK_FRAMES; ++index) {
            const uint16_t sample = static_cast<uint16_t>(renderSample());
            frames[index] = (static_cast<uint32_t>(sample) << 16) | sample;
        }
        const uint32_t renderMicros = micros() - started;
        const uint32_t load = min<uint32_t>(1000,
            static_cast<uint32_t>(static_cast<uint64_t>(renderMicros) * SAMPLE_RATE * 1000ULL /
                                  (BLOCK_FRAMES * 1000000ULL)));
        loadPermille_ = static_cast<uint16_t>((loadPermille_ * 7UL + load) / 8UL);
        size_t written = 0;
        const esp_err_t result = i2s_write(I2S_PORT, frames, sizeof(frames), &written, pdMS_TO_TICKS(30));
        if (result != ESP_OK || written != sizeof(frames)) ++writeFaults_;
    }
    i2s_zero_dma_buffer(I2S_PORT);
    audioTask_ = nullptr;
    vTaskDelete(nullptr);
}

void TileSynth::processCommands() {
    Command command;
    while (xQueueReceive(commandQueue_, &command, 0) == pdTRUE) process(command);
}

void TileSynth::process(const Command &command) {
    switch (command.type) {
        case CommandType::Play: startVoice(command.note, command.value, command.durationMs); break;
        case CommandType::Stop:
            for (Voice &voice : voices_) {
                if (voice.active && voice.note == command.note) release(voice, 90);
            }
            break;
        case CommandType::AllOff:
            for (Voice &voice : voices_) {
                if (command.value) voice.active = false;
                else if (voice.active) release(voice, 80);
            }
            if (command.value) thudSamples_ = 0;
            break;
        case CommandType::Tone: tone_ = min<uint8_t>(command.value, TONE_COUNT - 1); break;
        case CommandType::Volume: volume_ = min<uint8_t>(command.value, 10); break;
        case CommandType::Miss:
            thudLength_ = SAMPLE_RATE / 28;
            thudSamples_ = thudLength_;
            break;
    }
}

TileSynth::Voice *TileSynth::allocateVoice() {
    for (Voice &voice : voices_) if (!voice.active) return &voice;
    Voice *candidate = &voices_[0];
    for (Voice &voice : voices_) {
        if (voice.envelope == Envelope::Release && candidate->envelope != Envelope::Release) candidate = &voice;
        else if (voice.envelope == candidate->envelope && voice.age < candidate->age) candidate = &voice;
    }
    return candidate;
}

void TileSynth::startVoice(uint8_t note, uint8_t velocity, uint16_t durationMs) {
    Voice *voice = allocateVoice();
    *voice = Voice{};
    voice->active = true;
    voice->note = note;
    voice->velocity = velocity;
    voice->tone = tone_;
    voice->envelope = Envelope::Attack;
    voice->increment = increments_[note];
    voice->gateSamples = static_cast<uint32_t>(durationMs) * SAMPLE_RATE / 1000UL;
    voice->age = ++age_;
    voice->levelStep = max<int32_t>(1, 32767 / (tone_ == Piano ? 88 : 176));
    voice->brightness = tone_ == Piano ? min<uint32_t>(30000, 9000UL + velocity * 150UL) : 0;
    voice->brightnessStep = max<uint16_t>(1, voice->brightness / 7200);
}

void TileSynth::release(Voice &voice, uint16_t releaseMs) {
    if (!voice.active || voice.envelope == Envelope::Release) return;
    voice.envelope = Envelope::Release;
    const uint32_t samples = max<uint32_t>(1, static_cast<uint32_t>(releaseMs) * SAMPLE_RATE / 1000UL);
    voice.levelStep = max<int32_t>(1, voice.level / static_cast<int32_t>(samples));
    voice.gateSamples = 0;
}

int16_t TileSynth::oscillator(Voice &voice) {
    const uint8_t index = voice.phase >> 24;
    const int32_t fundamental = sine_[index];
    int32_t sample = fundamental;
    if (voice.tone == Piano) {
        const int32_t upper = sine_[static_cast<uint8_t>(index * 2)] * 2L +
                              sine_[static_cast<uint8_t>(index * 3)];
        sample = (fundamental * 7L + ((upper * voice.brightness) >> 15)) / 9;
        if (voice.brightness) {
            voice.brightness = voice.brightness > voice.brightnessStep
                ? voice.brightness - voice.brightnessStep : 0;
        }
    } else {
        const int32_t square = (voice.phase & 0x80000000UL) ? 22000 : -22000;
        sample = (fundamental * 3L + square) / 4;
    }
    voice.phase += voice.increment;
    return clamp(sample);
}

int16_t TileSynth::renderSample() {
    int32_t mixed = 0;
    uint8_t active = 0;
    for (Voice &voice : voices_) {
        if (!voice.active) continue;
        ++active;
        if (voice.envelope == Envelope::Attack) {
            voice.level += voice.levelStep;
            if (voice.level >= 32767) {
                voice.level = 32767;
                voice.envelope = Envelope::Sustain;
            }
        } else if (voice.envelope == Envelope::Release) {
            voice.level -= voice.levelStep;
            if (voice.level <= 0) {
                voice.active = false;
                continue;
            }
        }
        if (voice.gateSamples && --voice.gateSamples == 0) {
            release(voice, voice.tone == Piano ? 280 : 90);
        }
        int32_t sample = oscillator(voice);
        sample = sample * voice.level >> 15;
        mixed += sample * voice.velocity >> 7;
    }
    activeVoices_ = active;

    if (thudSamples_) {
        noise_ ^= noise_ << 13;
        noise_ ^= noise_ >> 17;
        noise_ ^= noise_ << 5;
        const int32_t noise = static_cast<int16_t>(noise_ >> 16);
        mixed += noise * thudSamples_ / max<uint16_t>(1, thudLength_) / 5;
        --thudSamples_;
    }

    mixed = mixed * VOLUME_TABLE[volume_] >> 8;
    constexpr uint32_t LIMIT = 28500;
    const uint32_t absolute = abs(mixed);
    uint16_t target = 32767;
    if (absolute > LIMIT) target = static_cast<uint16_t>(static_cast<uint64_t>(LIMIT) * 32767 / absolute);
    if (target < limiterGain_) limiterGain_ = target;
    else if (limiterGain_ < 32767) limiterGain_ += max<uint16_t>(1, (32767 - limiterGain_) >> 10);
    mixed = static_cast<int32_t>(static_cast<int64_t>(mixed) * limiterGain_ >> 15);
    return clamp(mixed);
}

int16_t TileSynth::clamp(int32_t value) {
    if (value > 32767) return 32767;
    if (value < -32768) return -32768;
    return static_cast<int16_t>(value);
}
