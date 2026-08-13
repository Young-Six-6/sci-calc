#include "BluetoothAudio.h"

#include <algorithm>
#include <cstring>

namespace {
BluetoothAudio *activeBluetoothAudio = nullptr;

void copyAsciiName(char *destination, size_t size, const char *source) {
    if (!size) return;
    size_t written = 0;
    if (source) {
        while (source[written] && written + 1 < size) {
            const uint8_t value = static_cast<uint8_t>(source[written]);
            destination[written] = value >= 32 && value <= 126 ? static_cast<char>(value) : '?';
            ++written;
        }
    }
    destination[written] = '\0';
}
}

BluetoothAudio bluetoothAudio;

void BluetoothAudio::begin() {
    activeBluetoothAudio = this;
    preferencesOpen_ = preferences_.begin("music-player", false);
    if (preferencesOpen_) {
        preferences_.getString("bt-device", targetName_, sizeof(targetName_));
        connectRequested_ = targetName_[0] != '\0';
    }
}

void BluetoothAudio::selectDac() {
    wantsBluetooth_ = false;
    clearAudio();
}

void BluetoothAudio::selectBluetooth() {
    wantsBluetooth_ = true;
    clearAudio();
    if (!started_) startBluetooth();
}

void BluetoothAudio::powerOffBluetooth() {
    wantsBluetooth_ = false;
    clearAudio();
    if (started_) {
        if (discoveryActive_) a2dp_.cancel_discovery();
        a2dp_.end(false);
        started_ = false;
    }
    connected_ = false;
    discoveryActive_ = false;
    state_ = State::Off;
}

void BluetoothAudio::startBluetooth() {
    if (started_) return;
    clearDevices();
    state_ = connectRequested_ ? State::Connecting : State::Scanning;
    connected_ = false;
    discoveryActive_ = false;

    a2dp_.set_local_name("SciCalc Music");
    a2dp_.set_ssp_enabled(true);
    a2dp_.set_auto_reconnect(connectRequested_, 2);
    a2dp_.set_data_callback_in_frames(dataCallback);
    a2dp_.set_ssid_callback(ssidCallback);
    a2dp_.set_on_connection_state_changed(connectionCallback, this);
    a2dp_.set_discovery_mode_callback(discoveryCallback);
    a2dp_.set_avrc_passthru_command_callback(remoteCallback);
    if (!bleMemoryReleased_) {
        a2dp_.set_reset_ble(true);
        bleMemoryReleased_ = true;
    } else {
        a2dp_.set_reset_ble(false);
    }
    started_ = true;
    a2dp_.start();
    a2dp_.set_reset_ble(false);
}

void BluetoothAudio::restartDiscovery() {
    wantsBluetooth_ = true;
    clearAudio();
    if (started_) {
        if (discoveryActive_) a2dp_.cancel_discovery();
        a2dp_.end(false);
        started_ = false;
    }
    connected_ = false;
    discoveryActive_ = false;
    state_ = State::Scanning;
    targetName_[0] = '\0';
    targetAddressValid_ = false;
    connectRequested_ = false;
    if (preferencesOpen_) preferences_.remove("bt-device");
    startBluetooth();
}

void BluetoothAudio::shutdown() {
    wantsBluetooth_ = false;
    clearAudio();
    if (started_) {
        if (discoveryActive_) a2dp_.cancel_discovery();
        a2dp_.end(false);
        started_ = false;
    }
    connected_ = false;
    discoveryActive_ = false;
    state_ = State::Off;
    if (preferencesOpen_) {
        preferences_.end();
        preferencesOpen_ = false;
    }
}

void BluetoothAudio::setInputSampleRate(uint32_t sampleRate) {
    if (sampleRate < 4000 || sampleRate > 192000 || sampleRate == inputSampleRate_) return;
    inputSampleRate_ = sampleRate;
    resetResampler();
}

bool BluetoothAudio::processI2sSample(uint32_t packedSample) {
    if (!wantsBluetooth_) return true;
    if (!connected_) return false;

    StereoFrame current = {
        static_cast<int16_t>(packedSample >> 16),
        static_cast<int16_t>(packedSample & 0xFFFF)
    };
    if (!havePreviousFrame_) {
        previousFrame_ = current;
        havePreviousFrame_ = true;
        untilNextOutput_ = inputSampleRate_;
        pushFrame(current);
        return false;
    }

    while (untilNextOutput_ <= BLUETOOTH_SAMPLE_RATE) {
        const int32_t leftDelta = static_cast<int32_t>(current.left) - previousFrame_.left;
        const int32_t rightDelta = static_cast<int32_t>(current.right) - previousFrame_.right;
        StereoFrame interpolated = {
            static_cast<int16_t>(previousFrame_.left +
                (static_cast<int64_t>(leftDelta) * untilNextOutput_) / BLUETOOTH_SAMPLE_RATE),
            static_cast<int16_t>(previousFrame_.right +
                (static_cast<int64_t>(rightDelta) * untilNextOutput_) / BLUETOOTH_SAMPLE_RATE)
        };
        if (!pushFrame(interpolated)) break;
        untilNextOutput_ += inputSampleRate_;
    }
    if (untilNextOutput_ > BLUETOOTH_SAMPLE_RATE) untilNextOutput_ -= BLUETOOTH_SAMPLE_RATE;
    previousFrame_ = current;
    return false;
}

bool BluetoothAudio::pushFrame(const StereoFrame &frame) {
    while (wantsBluetooth_ && connected_) {
        const uint32_t head = pcmHead_;
        const uint32_t next = (head + 1) & PCM_BUFFER_MASK;
        if (next != pcmTail_) {
            pcm_[head] = frame;
            __sync_synchronize();
            pcmHead_ = next;
            return true;
        }
        delay(1);
    }
    return false;
}

bool BluetoothAudio::popFrame(StereoFrame &frame) {
    bool found = false;
    portENTER_CRITICAL(&pcmMux_);
    const uint32_t tail = pcmTail_;
    if (tail != pcmHead_) {
        frame = pcm_[tail];
        pcmTail_ = (tail + 1) & PCM_BUFFER_MASK;
        found = true;
    }
    portEXIT_CRITICAL(&pcmMux_);
    return found;
}

void BluetoothAudio::clearAudio() {
    portENTER_CRITICAL(&pcmMux_);
    pcmTail_ = pcmHead_;
    portEXIT_CRITICAL(&pcmMux_);
    resetResampler();
}

void BluetoothAudio::resetResampler() {
    havePreviousFrame_ = false;
    untilNextOutput_ = inputSampleRate_;
}

int32_t BluetoothAudio::provideFrames(Frame *frames, int32_t frameCount) {
    bool underrun = false;
    for (int32_t index = 0; index < frameCount; ++index) {
        StereoFrame frame = {};
        if (wantsBluetooth_ && connected_ && popFrame(frame)) {
            frames[index].channel1 = frame.left;
            frames[index].channel2 = frame.right;
        } else {
            frames[index].channel1 = 0;
            frames[index].channel2 = 0;
            underrun = wantsBluetooth_ && connected_;
        }
    }
    if (underrun) ++underruns_;
    return frameCount;
}

bool BluetoothAudio::onDeviceFound(const char *name, esp_bd_addr_t address, int rssi) {
    if (!name || !name[0]) return false;
    char cleanName[sizeof(DeviceRecord::name)] = {};
    copyAsciiName(cleanName, sizeof(cleanName), name);

    portENTER_CRITICAL(&deviceMux_);
    uint8_t index = 0;
    for (; index < deviceCount_; ++index) {
        if (memcmp(devices_[index].address, address, ESP_BD_ADDR_LEN) == 0) break;
    }
    if (index == deviceCount_ && deviceCount_ < MAX_DEVICES) {
        ++deviceCount_;
        memcpy(devices_[index].address, address, ESP_BD_ADDR_LEN);
        copyAsciiName(devices_[index].name, sizeof(devices_[index].name), cleanName);
    }
    if (index < deviceCount_) devices_[index].rssi = constrain(rssi, -127, 20);

    const bool addressMatches = targetAddressValid_ &&
        memcmp(targetAddress_, address, ESP_BD_ADDR_LEN) == 0;
    const bool nameMatches = targetName_[0] &&
        strncmp(cleanName, targetName_, sizeof(targetName_) - 1) == 0;
    const bool selected = connectRequested_ && (addressMatches || nameMatches);
    if (selected) {
        memcpy(targetAddress_, address, ESP_BD_ADDR_LEN);
        targetAddressValid_ = true;
        copyAsciiName(targetName_, sizeof(targetName_), cleanName);
        state_ = State::Connecting;
    }
    portEXIT_CRITICAL(&deviceMux_);
    return selected;
}

void BluetoothAudio::onConnectionState(esp_a2d_connection_state_t state) {
    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        connected_ = true;
        discoveryActive_ = false;
        state_ = State::Connected;
        clearAudio();
    } else if (state == ESP_A2D_CONNECTION_STATE_CONNECTING) {
        connected_ = false;
        state_ = State::Connecting;
    } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
        connected_ = false;
        state_ = started_ ? (connectRequested_ ? State::Connecting : State::Scanning) : State::Off;
        clearAudio();
    }
}

void BluetoothAudio::onDiscoveryState(esp_bt_gap_discovery_state_t state) {
    discoveryActive_ = state == ESP_BT_GAP_DISCOVERY_STARTED;
    if (discoveryActive_ && !connectRequested_) state_ = State::Scanning;
}

void BluetoothAudio::onRemoteCommand(uint8_t command, bool released) {
    if (!released) return;
    switch (command) {
        case ESP_AVRC_PT_CMD_PLAY: remoteCommand_ = static_cast<uint8_t>(BluetoothRemoteCommand::Play); break;
        case ESP_AVRC_PT_CMD_PAUSE: remoteCommand_ = static_cast<uint8_t>(BluetoothRemoteCommand::Pause); break;
        case ESP_AVRC_PT_CMD_STOP: remoteCommand_ = static_cast<uint8_t>(BluetoothRemoteCommand::Stop); break;
        case ESP_AVRC_PT_CMD_BACKWARD: remoteCommand_ = static_cast<uint8_t>(BluetoothRemoteCommand::Previous); break;
        case ESP_AVRC_PT_CMD_FORWARD: remoteCommand_ = static_cast<uint8_t>(BluetoothRemoteCommand::Next); break;
        default: break;
    }
}

void BluetoothAudio::clearDevices() {
    portENTER_CRITICAL(&deviceMux_);
    deviceCount_ = 0;
    memset(devices_, 0, sizeof(devices_));
    portEXIT_CRITICAL(&deviceMux_);
}

bool BluetoothAudio::bluetoothRequested() const { return wantsBluetooth_; }
bool BluetoothAudio::bluetoothStarted() const { return started_; }
bool BluetoothAudio::bluetoothConnected() const { return connected_; }
bool BluetoothAudio::discoveryActive() const { return discoveryActive_; }

const char *BluetoothAudio::statusText() const {
    switch (state_) {
        case State::Off: return "OFF";
        case State::Scanning: return "SCANNING";
        case State::Connecting: return "CONNECTING";
        case State::Connected: return "CONNECTED";
        case State::Lost: return "LOST";
    }
    return "OFF";
}

const char *BluetoothAudio::targetName() const {
    return targetName_[0] ? targetName_ : "No device selected";
}

uint8_t BluetoothAudio::deviceCount() const {
    portENTER_CRITICAL(&deviceMux_);
    const uint8_t count = deviceCount_;
    portEXIT_CRITICAL(&deviceMux_);
    return count;
}

bool BluetoothAudio::getDevice(uint8_t index, BluetoothDeviceInfo &device) const {
    bool found = false;
    portENTER_CRITICAL(&deviceMux_);
    if (index < deviceCount_) {
        copyAsciiName(device.name, sizeof(device.name), devices_[index].name);
        device.rssi = devices_[index].rssi;
        found = true;
    }
    portEXIT_CRITICAL(&deviceMux_);
    return found;
}

bool BluetoothAudio::connectDevice(uint8_t index) {
    bool found = false;
    portENTER_CRITICAL(&deviceMux_);
    if (index < deviceCount_) {
        copyAsciiName(targetName_, sizeof(targetName_), devices_[index].name);
        memcpy(targetAddress_, devices_[index].address, ESP_BD_ADDR_LEN);
        targetAddressValid_ = true;
        connectRequested_ = true;
        state_ = State::Connecting;
        found = true;
    }
    portEXIT_CRITICAL(&deviceMux_);
    if (!found) return false;

    if (preferencesOpen_) preferences_.putString("bt-device", targetName_);
    if (discoveryActive_) esp_bt_gap_cancel_discovery();
    return true;
}

uint8_t BluetoothAudio::bufferPercent() const {
    portENTER_CRITICAL(&pcmMux_);
    const uint32_t head = pcmHead_;
    const uint32_t tail = pcmTail_;
    portEXIT_CRITICAL(&pcmMux_);
    const uint32_t used = (head - tail) & PCM_BUFFER_MASK;
    return static_cast<uint8_t>((static_cast<uint32_t>(used) * 100) / PCM_BUFFER_MASK);
}

uint32_t BluetoothAudio::takeUnderruns() {
    const uint32_t result = underruns_;
    underruns_ = 0;
    return result;
}

BluetoothRemoteCommand BluetoothAudio::takeRemoteCommand() {
    const uint8_t command = remoteCommand_;
    remoteCommand_ = 0;
    return static_cast<BluetoothRemoteCommand>(command);
}

int32_t BluetoothAudio::dataCallback(Frame *frames, int32_t frameCount) {
    if (!activeBluetoothAudio) {
        memset(frames, 0, frameCount * sizeof(Frame));
        return frameCount;
    }
    return activeBluetoothAudio->provideFrames(frames, frameCount);
}

bool BluetoothAudio::ssidCallback(const char *name, esp_bd_addr_t address, int rssi) {
    return activeBluetoothAudio && activeBluetoothAudio->onDeviceFound(name, address, rssi);
}

void BluetoothAudio::connectionCallback(esp_a2d_connection_state_t state, void *object) {
    static_cast<BluetoothAudio *>(object)->onConnectionState(state);
}

void BluetoothAudio::discoveryCallback(esp_bt_gap_discovery_state_t state) {
    if (activeBluetoothAudio) activeBluetoothAudio->onDiscoveryState(state);
}

void BluetoothAudio::remoteCallback(uint8_t command, bool released) {
    if (activeBluetoothAudio) activeBluetoothAudio->onRemoteCommand(command, released);
}
