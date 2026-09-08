#include "AudioBeeper.h"

#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
constexpr int I2S_DOUT = 15;
constexpr int I2S_LRC = 22;
constexpr int I2S_BCLK = 21;
constexpr uint32_t SAMPLE_RATE = 16000;
constexpr uint32_t TONE_HZ = 440;
constexpr int16_t AMPLITUDE = 2800;
constexpr size_t FRAMES_PER_BLOCK = 128;
}

bool AudioBeeper::begin() {
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = SAMPLE_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 4;
    config.dma_buf_len = FRAMES_PER_BLOCK;
    config.use_apll = false;
    config.tx_desc_auto_clear = true;
    config.fixed_mclk = I2S_PIN_NO_CHANGE;

    i2s_pin_config_t pins = {};
    pins.bck_io_num = I2S_BCLK;
    pins.ws_io_num = I2S_LRC;
    pins.data_out_num = I2S_DOUT;
    pins.data_in_num = I2S_PIN_NO_CHANGE;

    if (i2s_driver_install(I2S_PORT, &config, 0, nullptr) != ESP_OK) return false;
    if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) {
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }
    i2s_zero_dma_buffer(I2S_PORT);
    if (xTaskCreatePinnedToCore(taskEntry, "chip8-beep", 3072, this, 1, nullptr, 0) != pdPASS) {
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }
    ready_ = true;
    return true;
}

void AudioBeeper::taskEntry(void *context) {
    static_cast<AudioBeeper *>(context)->run();
}

void AudioBeeper::run() {
    int16_t samples[FRAMES_PER_BLOCK * 2];
    uint32_t phase = 0;
    while (true) {
        for (size_t frame = 0; frame < FRAMES_PER_BLOCK; ++frame) {
            int16_t sample = 0;
            if (toneEnabled_) {
                sample = phase < SAMPLE_RATE / 2 ? AMPLITUDE : -AMPLITUDE;
                phase += TONE_HZ;
                if (phase >= SAMPLE_RATE) phase -= SAMPLE_RATE;
            }
            samples[frame * 2] = sample;
            samples[frame * 2 + 1] = sample;
        }
        size_t written = 0;
        i2s_write(I2S_PORT, samples, sizeof(samples), &written, portMAX_DELAY);
    }
}
