#pragma once

#include <Arduino.h>
#include <BluetoothA2DPSource.h>
#include <Preferences.h>

enum class BluetoothRemoteCommand : uint8_t {
    None,
    Play,
    Pause,
    Stop,
    Previous,
    Next
};

struct BluetoothDeviceInfo {
    char name[37] = {};
    int8_t rssi = -127;
};

class BluetoothAudio {
public:
    void begin();
    void selectDac();
    void selectBluetooth();
    void powerOffBluetooth();
    void restartDiscovery();
    void shutdown();

    void setInputSampleRate(uint32_t sampleRate);
    bool processI2sSample(uint32_t packedSample);
    void clearAudio();

    bool bluetoothRequested() const;
    bool bluetoothStarted() const;
    bool bluetoothConnected() const;
    bool discoveryActive() const;
    const char *statusText() const;
    const char *targetName() const;
    uint8_t deviceCount() const;
    bool getDevice(uint8_t index, BluetoothDeviceInfo &device) const;
    bool connectDevice(uint8_t index);
    uint8_t bufferPercent() const;
    uint32_t takeUnderruns();
    BluetoothRemoteCommand takeRemoteCommand();

private:
    static constexpr uint32_t BLUETOOTH_SAMPLE_RATE = 44100;
    static constexpr uint16_t PCM_BUFFER_SIZE = 2048;
    static constexpr uint16_t PCM_BUFFER_MASK = PCM_BUFFER_SIZE - 1;
    static constexpr uint8_t MAX_DEVICES = 8;

    enum class State : uint8_t { Off, Scanning, Connecting, Connected, Lost };

    struct StereoFrame {
        int16_t left;
        int16_t right;
    };

    struct DeviceRecord {
        char name[37];
        uint8_t address[ESP_BD_ADDR_LEN];
        int8_t rssi;
    };

    BluetoothA2DPSource a2dp_;
    Preferences preferences_;
    StereoFrame pcm_[PCM_BUFFER_SIZE] = {};
    volatile uint32_t pcmHead_ = 0;
    volatile uint32_t pcmTail_ = 0;
    volatile bool wantsBluetooth_ = false;
    volatile bool started_ = false;
    volatile bool connected_ = false;
    volatile bool discoveryActive_ = false;
    volatile State state_ = State::Off;
    volatile uint32_t underruns_ = 0;
    volatile uint8_t remoteCommand_ = 0;

    mutable portMUX_TYPE deviceMux_ = portMUX_INITIALIZER_UNLOCKED;
    mutable portMUX_TYPE pcmMux_ = portMUX_INITIALIZER_UNLOCKED;
    DeviceRecord devices_[MAX_DEVICES] = {};
    uint8_t deviceCount_ = 0;
    char targetName_[37] = {};
    uint8_t targetAddress_[ESP_BD_ADDR_LEN] = {};
    bool targetAddressValid_ = false;
    bool connectRequested_ = false;
    bool preferencesOpen_ = false;
    bool bleMemoryReleased_ = false;

    uint32_t inputSampleRate_ = BLUETOOTH_SAMPLE_RATE;
    uint32_t untilNextOutput_ = BLUETOOTH_SAMPLE_RATE;
    StereoFrame previousFrame_ = {};
    bool havePreviousFrame_ = false;

    void startBluetooth();
    bool pushFrame(const StereoFrame &frame);
    bool popFrame(StereoFrame &frame);
    int32_t provideFrames(Frame *frames, int32_t frameCount);
    bool onDeviceFound(const char *name, esp_bd_addr_t address, int rssi);
    void onConnectionState(esp_a2d_connection_state_t state);
    void onDiscoveryState(esp_bt_gap_discovery_state_t state);
    void onRemoteCommand(uint8_t command, bool released);
    void resetResampler();
    void clearDevices();

    static int32_t dataCallback(Frame *frames, int32_t frameCount);
    static bool ssidCallback(const char *name, esp_bd_addr_t address, int rssi);
    static void connectionCallback(esp_a2d_connection_state_t state, void *object);
    static void discoveryCallback(esp_bt_gap_discovery_state_t state);
    static void remoteCallback(uint8_t command, bool released);
};

extern BluetoothAudio bluetoothAudio;
