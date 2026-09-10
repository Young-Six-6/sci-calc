#include "LiveSynth.h"

#include <math.h>

namespace {
constexpr uint8_t VOLUME_TABLE[11] = {0, 8, 12, 18, 26, 38, 54, 76, 108, 150, 205};

uint16_t envelopeTicks(uint16_t milliseconds) {
    const uint32_t ticks = static_cast<uint32_t>(milliseconds) * LiveSynth::SAMPLE_RATE / 8000UL;
    return static_cast<uint16_t>(max<uint32_t>(1, min<uint32_t>(ticks, 65535)));
}
}

bool LiveSynth::begin() {
    if (installed_) return true;

    for (uint16_t index = 0; index < 256; ++index) {
        sineTable_[index] = static_cast<int16_t>(sinf(index * TWO_PI / 256.0f) * 32767.0f);
    }
    for (uint16_t note = 0; note < 128; ++note) {
        const float frequency = 440.0f * powf(2.0f, (static_cast<int16_t>(note) - 69) / 12.0f);
        const double increment = frequency * 4294967296.0 / SAMPLE_RATE;
        incrementTable_[note] = static_cast<uint32_t>(max<double>(1.0, min<double>(increment, 4294967295.0)));
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
    config.dma_buf_len = AUDIO_BLOCK_FRAMES;
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
    const BaseType_t created = xTaskCreatePinnedToCore(
        audioTaskEntry, "keyboard-audio", 4096, this, 3, &audioTask_, 0);
    if (created != pdPASS) {
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

void LiveSynth::end() {
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
    activeVoices_ = 0;
    peakLevel_ = 0;
}

bool LiveSynth::ready() const {
    return installed_ && running_;
}

bool LiveSynth::send(CommandType type, uint8_t data1, uint8_t data2) {
    if (!ready() || !commandQueue_) return false;
    Command command;
    command.type = type;
    command.data1 = data1;
    command.data2 = data2;
    return xQueueSend(commandQueue_, &command, pdMS_TO_TICKS(8)) == pdTRUE;
}

void LiveSynth::noteOn(uint8_t note, uint8_t velocity) {
    if (note < 128 && velocity) send(CommandType::NoteOn, note, velocity);
}

void LiveSynth::noteOff(uint8_t note) {
    if (note < 128) send(CommandType::NoteOff, note);
}

void LiveSynth::allNotesOff(bool immediate) {
    send(CommandType::AllOff, immediate ? 1 : 0);
}

void LiveSynth::setSustain(bool enabled) {
    send(CommandType::Sustain, enabled ? 1 : 0);
}

void LiveSynth::setTone(uint8_t tone) {
    send(CommandType::Tone, min<uint8_t>(tone, TONE_COUNT - 1));
}

void LiveSynth::setVolume(uint8_t volume) {
    send(CommandType::Volume, min<uint8_t>(volume, 10));
}

void LiveSynth::click(bool accent) {
    send(CommandType::Click, accent ? 1 : 0);
}

uint8_t LiveSynth::activeVoices() const { return activeVoices_; }
uint16_t LiveSynth::peakLevel() const { return peakLevel_; }
uint16_t LiveSynth::loadPermille() const { return loadPermille_; }
uint32_t LiveSynth::writeFaults() const { return writeFaults_; }

void LiveSynth::copyScope(int8_t *destination, size_t count) const {
    if (!destination) return;
    count = min<size_t>(count, SCOPE_SAMPLES);
    const uint8_t first = scopeWrite_;
    for (size_t index = 0; index < count; ++index) {
        destination[index] = scope_[(first + index) & (SCOPE_SAMPLES - 1)];
    }
}

const char *LiveSynth::toneName(uint8_t tone) {
    static const char *NAMES[TONE_COUNT] = {
        "Piano", "E.Piano", "Organ", "Warm Pad", "Lead", "Bell"
    };
    return NAMES[min<uint8_t>(tone, TONE_COUNT - 1)];
}

uint8_t LiveSynth::gmProgram(uint8_t tone) {
    static constexpr uint8_t PROGRAMS[TONE_COUNT] = {0, 4, 16, 89, 80, 14};
    return PROGRAMS[min<uint8_t>(tone, TONE_COUNT - 1)];
}

void LiveSynth::audioTaskEntry(void *context) {
    static_cast<LiveSynth *>(context)->audioLoop();
}

void LiveSynth::audioLoop() {
    uint32_t frames[AUDIO_BLOCK_FRAMES];
    while (running_) {
        processCommands();
        const uint32_t renderStarted = micros();
        renderBlock(frames, AUDIO_BLOCK_FRAMES);
        const uint32_t renderMicros = micros() - renderStarted;
        const uint32_t instantLoad = min<uint32_t>(1000,
            static_cast<uint32_t>(static_cast<uint64_t>(renderMicros) * SAMPLE_RATE * 1000ULL /
                                  (AUDIO_BLOCK_FRAMES * 1000000ULL)));
        loadPermille_ = static_cast<uint16_t>((loadPermille_ * 7UL + instantLoad) / 8UL);

        size_t bytesWritten = 0;
        const size_t requested = sizeof(frames);
        const esp_err_t result = i2s_write(
            I2S_PORT, frames, requested, &bytesWritten, pdMS_TO_TICKS(30));
        if (result != ESP_OK || bytesWritten != requested) ++writeFaults_;
    }
    i2s_zero_dma_buffer(I2S_PORT);
    audioTask_ = nullptr;
    vTaskDelete(nullptr);
}

void LiveSynth::processCommands() {
    Command command;
    while (xQueueReceive(commandQueue_, &command, 0) == pdTRUE) processCommand(command);
}

void LiveSynth::processCommand(const Command &command) {
    switch (command.type) {
        case CommandType::NoteOn: startVoice(command.data1, command.data2); break;
        case CommandType::NoteOff: stopVoice(command.data1); break;
        case CommandType::AllOff: silenceVoices(command.data1 != 0); break;
        case CommandType::Sustain:
            sustain_ = command.data1 != 0;
            if (!sustain_) {
                for (Voice &voice : voices_) {
                    if (voice.active && voice.sustained && !voice.held) {
                        voice.sustained = false;
                        beginRelease(voice);
                    }
                }
            }
            break;
        case CommandType::Tone: currentTone_ = min<uint8_t>(command.data1, TONE_COUNT - 1); break;
        case CommandType::Volume: currentVolume_ = min<uint8_t>(command.data1, 10); break;
        case CommandType::Click:
            clickLength_ = command.data1 ? SAMPLE_RATE / 45 : SAMPLE_RATE / 70;
            clickRemaining_ = clickLength_;
            clickPhase_ = 0;
            clickIncrement_ = static_cast<uint32_t>(
                (command.data1 ? 2600.0 : 1800.0) * 4294967296.0 / SAMPLE_RATE);
            break;
    }
}

LiveSynth::Patch LiveSynth::patchFor(uint8_t tone, uint8_t note) const {
    const int32_t pianoDecay = max<int32_t>(650, min<int32_t>(1450, 1900 - note * 10));
    switch (tone) {
        case ElectricPiano: return {Waveform::ElectricPiano, 3, 850, 420, 6500, false};
        case Organ: return {Waveform::Organ, 8, 120, 180, 28500, false};
        case WarmPad: return {Waveform::Warm, 90, 520, 900, 24500, false};
        case Lead: return {Waveform::Lead, 6, 280, 220, 20500, false};
        case Bell: return {Waveform::Bell, 1, 1350, 500, 0, true};
        case Piano:
        default: return {Waveform::Piano, 1, static_cast<uint16_t>(pianoDecay), 520, 1900, false};
    }
}

LiveSynth::Voice *LiveSynth::allocateVoice() {
    for (Voice &voice : voices_) {
        if (!voice.active) return &voice;
        if (voice.stage == EnvelopeStage::Release && voice.envelope < 256) {
            voice.active = false;
            return &voice;
        }
    }

    Voice *candidate = &voices_[0];
    uint8_t candidateClass = 4;
    uint32_t candidateLevel = UINT32_MAX;
    for (Voice &voice : voices_) {
        uint8_t voiceClass = 3;
        if (voice.stage == EnvelopeStage::Release) voiceClass = 0;
        else if (!voice.held && !voice.sustained) voiceClass = 1;
        else if (!voice.held) voiceClass = 2;
        const uint32_t level = static_cast<uint32_t>(max<int32_t>(0, voice.envelope)) *
                               (static_cast<uint16_t>(voice.velocity) + 1);
        if (voiceClass < candidateClass ||
            (voiceClass == candidateClass && level < candidateLevel)) {
            candidate = &voice;
            candidateClass = voiceClass;
            candidateLevel = level;
        }
    }
    stealTail_ = max<int32_t>(-65536, min<int32_t>(65536, stealTail_ + candidate->lastSample));
    return candidate;
}

void LiveSynth::startVoice(uint8_t note, uint8_t velocity) {
    const Patch patch = patchFor(currentTone_, note);
    if (!patch.oneShot) {
        for (Voice &voice : voices_) {
            if (!voice.active || !voice.held || voice.oneShot || voice.note != note) continue;
            if (voice.overlapCount < UINT8_MAX) ++voice.overlapCount;
            voice.velocity = max<uint8_t>(voice.velocity, velocity);
            voice.age = ++voiceAge_;
            return;
        }
    }

    Voice *voice = allocateVoice();
    *voice = Voice{};
    voice->active = true;
    voice->held = !patch.oneShot;
    voice->oneShot = patch.oneShot;
    voice->note = note;
    voice->velocity = velocity;
    voice->overlapCount = 1;
    voice->tone = currentTone_;
    voice->waveform = patch.waveform;
    voice->renderWaveform = patch.waveform;
    voice->sustainLevel = patch.sustainLevel;
    voice->decayMs = patch.decayMs;
    voice->releaseMs = patch.releaseMs;
    voice->phaseIncrement = incrementTable_[note];
    voice->age = ++voiceAge_;
    voice->noise = 0x9E3779B9UL ^ (static_cast<uint32_t>(note) << 16) ^ voiceAge_;

    if (voice->phaseIncrement > 600000000UL && voice->renderWaveform != Waveform::Sine) {
        voice->renderWaveform = Waveform::Sine;
    } else if (voice->phaseIncrement > 300000000UL &&
               (voice->renderWaveform == Waveform::Lead || voice->renderWaveform == Waveform::Warm)) {
        voice->renderWaveform = Waveform::Sine;
    }
    configurePianoTimbre(*voice);

    voice->envelope = patch.attackMs <= 1 ? 32767 : 0;
    voice->stage = patch.attackMs <= 1 ? EnvelopeStage::Decay : EnvelopeStage::Attack;
    if (voice->stage == EnvelopeStage::Attack) {
        voice->envelopeStep = max<int32_t>(1, 32767 / envelopeTicks(patch.attackMs));
    } else {
        voice->envelopeStep = max<int32_t>(1,
            (32767 - patch.sustainLevel) / envelopeTicks(patch.decayMs));
    }
}

void LiveSynth::stopVoice(uint8_t note) {
    Voice *oldest = nullptr;
    for (Voice &voice : voices_) {
        if (!voice.active || !voice.held || voice.oneShot || voice.note != note) continue;
        if (!oldest || voice.age < oldest->age) oldest = &voice;
    }
    if (!oldest) return;
    if (oldest->overlapCount > 1) {
        --oldest->overlapCount;
        return;
    }
    oldest->held = false;
    if (sustain_) oldest->sustained = true;
    else beginRelease(*oldest);
}

void LiveSynth::silenceVoices(bool immediate) {
    for (Voice &voice : voices_) {
        if (!voice.active) continue;
        voice.held = false;
        voice.sustained = false;
        if (immediate) voice.active = false;
        else beginRelease(voice);
    }
    if (immediate) {
        stealTail_ = 0;
        clickRemaining_ = 0;
    }
}

void LiveSynth::configurePianoTimbre(Voice &voice) {
    if (voice.waveform != Waveform::Piano) return;
    voice.brightnessEnvelope = static_cast<uint16_t>(
        min<uint32_t>(32767, 11000UL + static_cast<uint32_t>(voice.velocity) * 145UL));
    const uint16_t brightnessMs = static_cast<uint16_t>(max<int32_t>(90, min<int32_t>(230, 300 - voice.note * 2)));
    voice.brightnessStep = static_cast<uint16_t>(max<uint32_t>(
        1, voice.brightnessEnvelope / envelopeTicks(brightnessMs)));
    voice.hammerEnvelope = static_cast<uint16_t>(
        min<uint32_t>(23500, 6000UL + static_cast<uint32_t>(voice.velocity) * 115UL));
    const uint32_t hammerSamples = max<uint32_t>(1, (4 + voice.velocity / 20) * SAMPLE_RATE / 1000UL);
    voice.hammerStep = static_cast<uint16_t>(max<uint32_t>(1, voice.hammerEnvelope / hammerSamples));
}

int16_t LiveSynth::oscillator(Voice &voice) {
    const uint8_t index = voice.phase >> 24;
    const int32_t sine = sineTable_[index];
    int32_t sample = sine;
    switch (voice.renderWaveform) {
        case Waveform::Sine:
            break;
        case Waveform::Piano: {
            sample = sine * 5;
            if (voice.brightnessEnvelope) {
                const int32_t upper = sineTable_[static_cast<uint8_t>(index * 2)] * 2L +
                                      sineTable_[static_cast<uint8_t>(index * 3)];
                if (voice.brightnessEnvelope > 24576) sample = (sample + upper) >> 3;
                else if (voice.brightnessEnvelope > 16384) sample = (sample * 2 + upper) >> 4;
                else if (voice.brightnessEnvelope > 8192) sample = (sample * 4 + upper) >> 5;
                else sample = (sample * 8 + upper) >> 6;
            } else {
                sample >>= 3;
            }
            break;
        }
        case Waveform::ElectricPiano:
            sample = (sine * 6L + sineTable_[static_cast<uint8_t>(index * 2)] * 2L +
                      sineTable_[static_cast<uint8_t>(index * 3)]) / 9;
            break;
        case Waveform::Organ:
            sample = (sine * 4L + sineTable_[static_cast<uint8_t>(index * 2)] * 2L +
                      sineTable_[static_cast<uint8_t>(index * 3)]) / 7;
            break;
        case Waveform::Warm: {
            const uint16_t phase16 = voice.phase >> 16;
            const int32_t triangle = phase16 < 32768
                ? -32768 + static_cast<int32_t>(phase16) * 2
                : 98303 - static_cast<int32_t>(phase16) * 2;
            sample = (triangle * 2L + sine * 3L) / 5;
            break;
        }
        case Waveform::Lead: {
            const int32_t saw = static_cast<int16_t>(voice.phase >> 16);
            const int32_t square = (voice.phase & 0x80000000UL) ? 22000 : -22000;
            sample = (saw * 2L + square + sine) / 4;
            break;
        }
        case Waveform::Bell:
            sample = (sine * 5L + sineTable_[static_cast<uint8_t>(index * 2)] * 2L +
                      sineTable_[static_cast<uint8_t>(index * 3)] * 2L +
                      sineTable_[static_cast<uint8_t>(index * 5)]) / 10;
            break;
    }

    if (voice.waveform == Waveform::Piano && voice.hammerEnvelope) {
        voice.noise ^= voice.noise << 13;
        voice.noise ^= voice.noise >> 17;
        voice.noise ^= voice.noise << 5;
        const int32_t hammer = static_cast<int16_t>(voice.noise >> 16);
        sample += (hammer * voice.hammerEnvelope) >> 17;
        voice.hammerEnvelope = voice.hammerEnvelope > voice.hammerStep
            ? voice.hammerEnvelope - voice.hammerStep : 0;
    }
    voice.phase += voice.phaseIncrement;
    return clampSample(sample);
}

void LiveSynth::beginRelease(Voice &voice) {
    if (!voice.active || voice.stage == EnvelopeStage::Release) return;
    voice.stage = EnvelopeStage::Release;
    voice.envelopeStep = max<int32_t>(1, voice.envelope / envelopeTicks(voice.releaseMs));
}

void LiveSynth::advanceEnvelope(Voice &voice) {
    if (!voice.active || ++voice.envelopeClock < 8) return;
    voice.envelopeClock = 0;
    if (voice.brightnessEnvelope) {
        voice.brightnessEnvelope = voice.brightnessEnvelope > voice.brightnessStep
            ? voice.brightnessEnvelope - voice.brightnessStep : 0;
    }
    switch (voice.stage) {
        case EnvelopeStage::Attack:
            voice.envelope += voice.envelopeStep;
            if (voice.envelope >= 32767) {
                voice.envelope = 32767;
                voice.stage = EnvelopeStage::Decay;
                voice.envelopeStep = max<int32_t>(1,
                    (32767 - voice.sustainLevel) / envelopeTicks(voice.decayMs));
            }
            break;
        case EnvelopeStage::Decay:
            voice.envelope -= voice.envelopeStep;
            if (voice.envelope <= voice.sustainLevel) {
                voice.envelope = voice.sustainLevel;
                if (voice.oneShot || voice.sustainLevel == 0) voice.active = false;
                else voice.stage = EnvelopeStage::Sustain;
            }
            break;
        case EnvelopeStage::Sustain:
            if (!voice.held && !voice.sustained) beginRelease(voice);
            break;
        case EnvelopeStage::Release:
            voice.envelope -= voice.envelopeStep;
            if (voice.envelope <= 0) voice.active = false;
            break;
    }
}

int16_t LiveSynth::renderFrame() {
    int32_t mixed = 0;
    for (Voice &voice : voices_) {
        if (!voice.active) continue;
        int32_t sample = oscillator(voice);
        sample = sample * voice.envelope >> 15;
        sample = sample * voice.velocity >> 7;
        voice.lastSample = sample;
        mixed += sample;
        advanceEnvelope(voice);
    }

    if (clickRemaining_) {
        clickNoise_ ^= clickNoise_ << 13;
        clickNoise_ ^= clickNoise_ >> 17;
        clickNoise_ ^= clickNoise_ << 5;
        const int32_t tonal = (clickPhase_ & 0x80000000UL) ? 12000 : -12000;
        const int32_t noise = static_cast<int16_t>(clickNoise_ >> 16) / 5;
        mixed += (tonal + noise) * clickRemaining_ / max<uint16_t>(1, clickLength_);
        clickPhase_ += clickIncrement_;
        --clickRemaining_;
    }

    mixed += stealTail_;
    stealTail_ = stealTail_ * 63 / 64;
    if (abs(stealTail_) < 4) stealTail_ = 0;
    mixed = mixed * VOLUME_TABLE[currentVolume_] >> 8;

    constexpr int32_t LIMIT_LEVEL = 28500;
    const uint32_t absolute = static_cast<uint32_t>(abs(mixed));
    uint16_t targetGain = 32767;
    if (absolute > LIMIT_LEVEL) {
        targetGain = static_cast<uint16_t>(static_cast<uint64_t>(LIMIT_LEVEL) * 32767 / absolute);
    }
    if (targetGain < limiterGainQ15_) limiterGainQ15_ = targetGain;
    else if (limiterGainQ15_ < 32767) {
        limiterGainQ15_ += max<uint16_t>(1, (32767 - limiterGainQ15_) >> 11);
    }
    if (limiterGainQ15_ < 32767) mixed = static_cast<int32_t>(
        static_cast<int64_t>(mixed) * limiterGainQ15_ >> 15);
    return clampSample(mixed);
}

void LiveSynth::renderBlock(uint32_t *frames, size_t frameCount) {
    uint16_t blockPeak = 0;
    for (size_t index = 0; index < frameCount; ++index) {
        const int16_t sample = renderFrame();
        const uint16_t absolute = static_cast<uint16_t>(abs(static_cast<int32_t>(sample)));
        blockPeak = max<uint16_t>(blockPeak, absolute);
        if (++scopeDivider_ >= 8) {
            scopeDivider_ = 0;
            scope_[scopeWrite_] = static_cast<int8_t>(sample >> 8);
            scopeWrite_ = (scopeWrite_ + 1) & (SCOPE_SAMPLES - 1);
        }
        const uint16_t packed = static_cast<uint16_t>(sample);
        frames[index] = (static_cast<uint32_t>(packed) << 16) | packed;
    }
    uint8_t count = 0;
    for (const Voice &voice : voices_) if (voice.active) ++count;
    activeVoices_ = count;
    peakLevel_ = max<uint16_t>(blockPeak, peakLevel_ > 600 ? peakLevel_ - 600 : 0);
}

int16_t LiveSynth::clampSample(int32_t sample) {
    if (sample > 32767) return 32767;
    if (sample < -32768) return -32768;
    return static_cast<int16_t>(sample);
}
