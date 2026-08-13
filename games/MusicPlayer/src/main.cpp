#include <Arduino.h>
#include <Audio.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <U8g2lib.h>

#include <algorithm>

#include "BluetoothAudio.h"
#include "SafeKeyboard.h"

namespace {
constexpr uint8_t OLED_CS = 5;
constexpr uint8_t OLED_DC = 16;
constexpr uint8_t OLED_RST = 17;
constexpr uint8_t SD_CS = 4;
constexpr uint8_t I2S_DOUT = 15;
constexpr uint8_t I2S_LRC = 22;
constexpr uint8_t I2S_BCLK = 21;
constexpr uint8_t MAX_TRACKS = 96;
constexpr uint8_t MAX_SCAN_DEPTH = 3;
constexpr uint8_t MAX_VOLUME = 21;
constexpr uint32_t EXIT_HOLD_MS = 1000;
constexpr uint32_t DISPLAY_INTERVAL_MS = 160;

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI display(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
SafeKeyboard keyboard;
Audio audio;

String tracks[MAX_TRACKS];
uint8_t trackCount = 0;
uint8_t currentTrack = 0;
uint8_t browserTrack = 0;
uint8_t volume = 12;
uint8_t volumeBeforeMute = 12;

enum class Page : uint8_t { NowPlaying, Browser, Output };
enum class RepeatMode : uint8_t { Off, All, One };

Page page = Page::NowPlaying;
RepeatMode repeatMode = RepeatMode::All;
bool shuffleEnabled = false;
bool muted = false;
bool paused = false;
bool playing = false;
bool exitTriggered = false;
volatile bool endOfTrack = false;
uint32_t lastDraw = 0;
uint32_t marqueeOffset = 0;
uint32_t lastMarqueeStep = 0;
String notice;
uint32_t noticeUntil = 0;
uint8_t outputCursor = 0;
uint8_t btDeviceCursor = 0;
bool devicePicker = false;

bool isAudioFile(const String &path) {
    const int dot = path.lastIndexOf('.');
    if (dot < 0) return false;
    String extension = path.substring(dot + 1);
    extension.toLowerCase();
    return extension == "mp3" || extension == "wav" ||
           extension == "aac" || extension == "m4a";
}

void addTrack(const String &path) {
    if (trackCount >= MAX_TRACKS || !isAudioFile(path)) return;
    tracks[trackCount++] = path;
}

void scanDirectory(const String &path, uint8_t depth) {
    File directory = SD.open(path);
    if (!directory || !directory.isDirectory()) {
        if (directory) directory.close();
        return;
    }

    File entry = directory.openNextFile();
    while (entry && trackCount < MAX_TRACKS) {
        String entryPath = entry.path();
        const bool isDirectory = entry.isDirectory();
        entry.close();
        if (isDirectory && depth < MAX_SCAN_DEPTH) scanDirectory(entryPath, depth + 1);
        else if (!isDirectory) addTrack(entryPath);
        entry = directory.openNextFile();
    }
    if (entry) entry.close();
    directory.close();
}

String lowerCopy(String value) {
    value.toLowerCase();
    return value;
}

void sortTracks() {
    std::sort(tracks, tracks + trackCount, [](const String &left, const String &right) {
        return lowerCopy(left).compareTo(lowerCopy(right)) < 0;
    });
}

String baseName(const String &path) {
    int slash = path.lastIndexOf('/');
    String name = slash >= 0 ? path.substring(slash + 1) : path;
    const int dot = name.lastIndexOf('.');
    if (dot > 0) name = name.substring(0, dot);
    return name;
}

String printableName(const String &path) {
    String name = baseName(path);
    String result;
    result.reserve(name.length());
    for (size_t i = 0; i < name.length(); ++i) {
        const uint8_t c = static_cast<uint8_t>(name[i]);
        result += (c >= 32 && c <= 126) ? static_cast<char>(c) : '?';
    }
    return result.length() ? result : String("Untitled");
}

String extensionFor(const String &path) {
    const int dot = path.lastIndexOf('.');
    if (dot < 0) return "AUDIO";
    String extension = path.substring(dot + 1);
    extension.toUpperCase();
    return extension;
}

String timeText(uint32_t seconds) {
    char buffer[12];
    const uint32_t minutes = seconds / 60;
    snprintf(buffer, sizeof(buffer), "%lu:%02lu",
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds % 60));
    return String(buffer);
}

void setNotice(const String &text, uint32_t duration = 1600) {
    notice = text;
    noticeUntil = millis() + duration;
}

bool noticeActive() {
    return notice.length() && static_cast<int32_t>(noticeUntil - millis()) > 0;
}

uint8_t randomTrack() {
    if (trackCount <= 1) return currentTrack;
    uint8_t selected = currentTrack;
    while (selected == currentTrack) selected = random(trackCount);
    return selected;
}

bool startTrack(uint8_t index) {
    if (trackCount == 0) return false;
    currentTrack = index % trackCount;
    browserTrack = currentTrack;
    marqueeOffset = 0;
    paused = false;
    endOfTrack = false;
    bluetoothAudio.clearAudio();
    audio.stopSong();
    playing = audio.connecttoFS(SD, tracks[currentTrack].c_str());
    if (!playing) setNotice("Cannot open this track", 2200);
    return playing;
}

void selectNext(bool forward) {
    if (trackCount == 0) return;
    uint8_t next = currentTrack;
    if (shuffleEnabled) next = randomTrack();
    else if (forward) next = (currentTrack + 1) % trackCount;
    else next = (currentTrack + trackCount - 1) % trackCount;
    startTrack(next);
}

void handleTrackEnd() {
    endOfTrack = false;
    if (!playing || trackCount == 0) return;
    if (repeatMode == RepeatMode::One) {
        startTrack(currentTrack);
        return;
    }
    if (repeatMode == RepeatMode::Off && !shuffleEnabled && currentTrack + 1 >= trackCount) {
        playing = false;
        paused = false;
        setNotice("Playlist finished");
        return;
    }
    selectNext(true);
}

void togglePause() {
    if (!playing) {
        startTrack(currentTrack);
        return;
    }
    if (audio.pauseResume()) paused = !paused;
}

void changeVolume(int delta) {
    const int changed = constrain(static_cast<int>(muted ? volumeBeforeMute : volume) + delta, 0, MAX_VOLUME);
    volume = changed;
    volumeBeforeMute = changed;
    muted = false;
    audio.setVolume(volume);
}

void toggleMute() {
    if (muted) {
        volume = volumeBeforeMute;
        muted = false;
    } else {
        volumeBeforeMute = volume;
        volume = 0;
        muted = true;
    }
    audio.setVolume(volume);
}

void seekBy(int seconds) {
    if (!playing) return;
    const int32_t duration = audio.getAudioFileDuration();
    const int32_t current = audio.getAudioCurrentTime();
    int32_t target = max<int32_t>(0, current + seconds);
    if (duration > 0) target = min<int32_t>(duration - 1, target);
    audio.setAudioPlayPosition(target);
}

void cycleRepeat() {
    if (repeatMode == RepeatMode::Off) repeatMode = RepeatMode::All;
    else if (repeatMode == RepeatMode::All) repeatMode = RepeatMode::One;
    else repeatMode = RepeatMode::Off;
}

String repeatText() {
    if (repeatMode == RepeatMode::One) return "R1";
    if (repeatMode == RepeatMode::All) return "RA";
    return "R-";
}

void drawMarquee(const String &text, int x, int y, int width) {
    const int textWidth = display.getStrWidth(text.c_str());
    display.setClipWindow(x, y - 8, x + width, y + 2);
    if (textWidth <= width) {
        display.drawStr(x, y, text.c_str());
    } else {
        const String looped = text + "   " + text;
        const uint16_t cycleWidth = textWidth + display.getStrWidth("   ");
        display.drawStr(x - (marqueeOffset % cycleWidth), y, looped.c_str());
    }
    display.setMaxClipWindow();
}

void drawNowPlaying() {
    display.clearBuffer();
    display.setFont(u8g2_font_5x7_tf);
    String header = "MUSIC ";
    if (trackCount) header += String(currentTrack + 1) + "/" + String(trackCount) + " " + extensionFor(tracks[currentTrack]);
    else header += "NO TRACKS";
    display.drawStr(2, 7, header.c_str());

    String mode = repeatText();
    if (shuffleEnabled) mode += " SHF";
    if (bluetoothAudio.bluetoothRequested()) mode = "BT " + mode;
    display.drawStr(254 - display.getStrWidth(mode.c_str()), 7, mode.c_str());

    display.setFont(u8g2_font_6x10_tf);
    const String title = trackCount ? printableName(tracks[currentTrack]) : String("Put music in /music");
    drawMarquee(title, 2, 24, 252);

    const uint32_t current = playing ? audio.getAudioCurrentTime() : 0;
    const uint32_t duration = playing ? audio.getAudioFileDuration() : 0;
    display.drawFrame(2, 31, 252, 7);
    if (duration > 0) {
        const uint16_t progress = min<uint32_t>(250, (current * 250ULL) / duration);
        if (progress) display.drawBox(3, 32, progress, 5);
    }

    display.setFont(u8g2_font_5x7_tf);
    String timing = timeText(current) + "/" + (duration ? timeText(duration) : String("--:--"));
    display.drawStr(2, 48, timing.c_str());
    String state;
    if (bluetoothAudio.bluetoothRequested() && !bluetoothAudio.bluetoothConnected()) state = "WAIT BT";
    else state = !playing ? "STOP" : (paused ? "PAUSE" : "PLAY");
    display.drawStr(128 - display.getStrWidth(state.c_str()) / 2, 48, state.c_str());
    String volumeText = muted ? "MUTE" : "VOL " + String(volume);
    display.drawStr(254 - display.getStrWidth(volumeText.c_str()), 48, volumeText.c_str());

    String footer = noticeActive() ? notice : String("ENTER play  TAB list  SHIFT+TAB output");
    if (display.getStrWidth(footer.c_str()) > 252) {
        while (footer.length() && display.getStrWidth(footer.c_str()) > 244) footer.remove(footer.length() - 1);
        footer += "...";
    }
    display.drawStr(2, 62, footer.c_str());
    display.sendBuffer();
}

void drawBrowser() {
    display.clearBuffer();
    display.setFont(u8g2_font_5x7_tf);
    String header = "PLAYLIST ";
    header += trackCount ? String(browserTrack + 1) + "/" + String(trackCount) : String("EMPTY");
    display.drawStr(2, 7, header.c_str());
    display.drawStr(172, 7, "ENTER play TAB back");

    if (trackCount == 0) {
        display.setFont(u8g2_font_6x10_tf);
        display.drawStr(56, 36, "No supported audio files");
        display.sendBuffer();
        return;
    }

    constexpr uint8_t visibleRows = 6;
    uint8_t first = browserTrack >= visibleRows ? browserTrack - visibleRows + 1 : 0;
    if (trackCount > visibleRows && first + visibleRows > trackCount) first = trackCount - visibleRows;
    for (uint8_t row = 0; row < visibleRows && first + row < trackCount; ++row) {
        const uint8_t index = first + row;
        const int baseline = 16 + row * 8;
        if (index == browserTrack) {
            display.drawBox(0, baseline - 7, 256, 8);
            display.setDrawColor(0);
        }
        String line = String(index == currentTrack && playing ? ">" : " ") + String(index + 1) + " " + printableName(tracks[index]);
        while (line.length() && display.getStrWidth(line.c_str()) > 252) line.remove(line.length() - 1);
        display.drawStr(2, baseline, line.c_str());
        display.setDrawColor(1);
    }
    display.sendBuffer();
}

void drawOutputChoice() {
    display.clearBuffer();
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(2, 7, "AUDIO OUTPUT");
    display.drawStr(195, 7, "TAB back");

    const char *dacStatus = bluetoothAudio.bluetoothRequested() ? "READY" : "ACTIVE";
    String dacLine = String("PCM5102A   ") + dacStatus + "  21/22/15";
    if (outputCursor == 0) {
        display.drawBox(0, 12, 256, 11);
        display.setDrawColor(0);
    }
    display.drawStr(3, 20, dacLine.c_str());
    display.setDrawColor(1);

    String btLine = "Bluetooth   ";
    btLine += bluetoothAudio.statusText();
    if (bluetoothAudio.bluetoothConnected()) {
        btLine += "  Q" + String(bluetoothAudio.bufferPercent()) + "%";
    }
    if (outputCursor == 1) {
        display.drawBox(0, 25, 256, 11);
        display.setDrawColor(0);
    }
    display.drawStr(3, 33, btLine.c_str());
    display.setDrawColor(1);

    String device = "Device: ";
    device += bluetoothAudio.targetName();
    while (device.length() && display.getStrWidth(device.c_str()) > 252) device.remove(device.length() - 1);
    display.drawStr(2, 47, device.c_str());

    String footer;
    if (outputCursor == 0) footer = "ENTER use DAC   8/2 select";
    else if (!bluetoothAudio.bluetoothStarted()) footer = "ENTER start BT scan   8/2 select";
    else footer = "ENTER use BT  0 change  POWER off";
    display.drawStr(2, 62, footer.c_str());
    display.sendBuffer();
}

void drawBluetoothPicker() {
    display.clearBuffer();
    display.setFont(u8g2_font_5x7_tf);
    String header = "BT DEVICES  ";
    header += bluetoothAudio.statusText();
    display.drawStr(2, 7, header.c_str());
    display.drawStr(210, 7, "TAB back");

    const uint8_t count = bluetoothAudio.deviceCount();
    if (!count) {
        display.setFont(u8g2_font_6x10_tf);
        display.drawStr(42, 29, "Make headphones discoverable");
        display.setFont(u8g2_font_5x7_tf);
        display.drawStr(71, 44, "Scanning for audio devices...");
    } else {
        constexpr uint8_t visibleRows = 5;
        if (btDeviceCursor >= count) btDeviceCursor = count - 1;
        uint8_t first = btDeviceCursor >= visibleRows ? btDeviceCursor - visibleRows + 1 : 0;
        if (count > visibleRows && first + visibleRows > count) first = count - visibleRows;
        for (uint8_t row = 0; row < visibleRows && first + row < count; ++row) {
            const uint8_t index = first + row;
            BluetoothDeviceInfo device;
            if (!bluetoothAudio.getDevice(index, device)) continue;
            const int baseline = 16 + row * 8;
            if (index == btDeviceCursor) {
                display.drawBox(0, baseline - 7, 256, 8);
                display.setDrawColor(0);
            }
            String line = String(index + 1) + " " + device.name + "  " + String(device.rssi);
            while (line.length() && display.getStrWidth(line.c_str()) > 252) line.remove(line.length() - 1);
            display.drawStr(2, baseline, line.c_str());
            display.setDrawColor(1);
        }
    }
    display.drawStr(2, 62, count ? "8/2 select  ENTER connect  0 rescan" : "0 restart scan  TAB cancel");
    display.sendBuffer();
}

void drawOutput() {
    if (devicePicker) drawBluetoothPicker();
    else drawOutputChoice();
}

void drawPage() {
    if (page == Page::NowPlaying) drawNowPlaying();
    else if (page == Page::Browser) drawBrowser();
    else drawOutput();
}

bool installMainFirmware() {
    audio.stopSong();
    playing = false;
    bluetoothAudio.shutdown();
    delay(40);
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(52, 14, "Returning to main firmware");
    display.drawFrame(4, 28, 248, 9);
    display.sendBuffer();

    File firmware = SD.open("/main.bin", FILE_READ);
    if (!firmware || firmware.isDirectory() || firmware.size() == 0) {
        if (firmware) firmware.close();
        setNotice("Missing /main.bin", 2500);
        return false;
    }

    const size_t size = firmware.size();
    if (!Update.begin(size, U_FLASH)) {
        firmware.close();
        setNotice("Update begin failed", 2500);
        return false;
    }

    uint8_t buffer[1024];
    size_t written = 0;
    uint8_t previousPercent = 255;
    while (firmware.available()) {
        const size_t count = firmware.read(buffer, sizeof(buffer));
        if (!count || Update.write(buffer, count) != count) break;
        written += count;
        const uint8_t percent = (written * 100ULL) / size;
        if (percent != previousPercent) {
            previousPercent = percent;
            display.setDrawColor(0);
            display.drawBox(5, 29, 246, 7);
            display.setDrawColor(1);
            display.drawBox(5, 29, percent * 246 / 100, 7);
            display.sendBuffer();
        }
    }
    firmware.close();

    if (written != size || !Update.end(true) || !Update.isFinished()) {
        Update.abort();
        setNotice("Update failed", 2500);
        return false;
    }
    display.clearBuffer();
    display.drawStr(70, 35, "Main firmware restored");
    display.sendBuffer();
    delay(250);
    ESP.restart();
    return true;
}

void handleNowPlayingKey(const PlayerKeyEvent &event) {
    const bool repeat = event.action == PlayerKeyAction::Repeat;
    const bool shift = keyboard.isDown(PlayerKey::Shift);
    if (event.action == PlayerKeyAction::Release) return;
    if (repeat && event.key != PlayerKey::K4 && event.key != PlayerKey::K6 &&
        event.key != PlayerKey::K7 && event.key != PlayerKey::K9 &&
        event.key != PlayerKey::Minus && event.key != PlayerKey::Plus) return;

    switch (event.key) {
        case PlayerKey::K5:
        case PlayerKey::Enter:
            if (!repeat) togglePause();
            break;
        case PlayerKey::K8: if (!repeat) selectNext(false); break;
        case PlayerKey::K2: if (!repeat) selectNext(true); break;
        case PlayerKey::K4: seekBy(shift ? -60 : -10); break;
        case PlayerKey::K6: seekBy(shift ? 60 : 10); break;
        case PlayerKey::K7: changeVolume(-1); break;
        case PlayerKey::K9: changeVolume(1); break;
        case PlayerKey::Minus: changeVolume(-1); break;
        case PlayerKey::Plus: changeVolume(1); break;
        case PlayerKey::K1: if (!repeat) cycleRepeat(); break;
        case PlayerKey::Extra: if (!repeat) cycleRepeat(); break;
        case PlayerKey::K3: if (!repeat) shuffleEnabled = !shuffleEnabled; break;
        case PlayerKey::Angle: if (!repeat) shuffleEnabled = !shuffleEnabled; break;
        case PlayerKey::Multiply: if (!repeat) toggleMute(); break;
        case PlayerKey::Tab:
            if (!repeat && shift) {
                outputCursor = bluetoothAudio.bluetoothRequested() ? 1 : 0;
                devicePicker = false;
                page = Page::Output;
            } else if (!repeat) {
                browserTrack = currentTrack;
                page = Page::Browser;
            }
            break;
        case PlayerKey::Dot:
        case PlayerKey::K0:
            if (!repeat) { browserTrack = currentTrack; page = Page::Browser; }
            break;
        default: break;
    }
}

void handleBrowserKey(const PlayerKeyEvent &event) {
    const bool shift = keyboard.isDown(PlayerKey::Shift);
    if (event.action == PlayerKeyAction::Press && event.key == PlayerKey::Tab && shift) {
        outputCursor = bluetoothAudio.bluetoothRequested() ? 1 : 0;
        devicePicker = false;
        page = Page::Output;
        return;
    }
    if (event.action == PlayerKeyAction::Release || trackCount == 0) {
        if (event.action == PlayerKeyAction::Press &&
            (event.key == PlayerKey::Dot || event.key == PlayerKey::Divide ||
             event.key == PlayerKey::Tab || event.key == PlayerKey::Backspace)) page = Page::NowPlaying;
        return;
    }
    const bool repeat = event.action == PlayerKeyAction::Repeat;
    switch (event.key) {
        case PlayerKey::K8: browserTrack = (browserTrack + trackCount - 1) % trackCount; break;
        case PlayerKey::K2: browserTrack = (browserTrack + 1) % trackCount; break;
        case PlayerKey::K7: browserTrack = browserTrack >= 6 ? browserTrack - 6 : 0; break;
        case PlayerKey::K9: browserTrack = min<int>(trackCount - 1, browserTrack + 6); break;
        case PlayerKey::K5:
        case PlayerKey::Multiply:
        case PlayerKey::Enter:
            if (!repeat) { startTrack(browserTrack); page = Page::NowPlaying; }
            break;
        case PlayerKey::Dot:
        case PlayerKey::Divide:
        case PlayerKey::Tab:
        case PlayerKey::Backspace:
            if (!repeat) page = Page::NowPlaying;
            break;
        default: break;
    }
}

void leaveOutputPage() {
    devicePicker = false;
    page = Page::NowPlaying;
}

void handleOutputKey(const PlayerKeyEvent &event) {
    if (event.action == PlayerKeyAction::Release) return;
    const bool repeat = event.action == PlayerKeyAction::Repeat;

    if (devicePicker) {
        const uint8_t count = bluetoothAudio.deviceCount();
        switch (event.key) {
            case PlayerKey::K8:
                if (count) btDeviceCursor = (btDeviceCursor + count - 1) % count;
                break;
            case PlayerKey::K2:
                if (count) btDeviceCursor = (btDeviceCursor + 1) % count;
                break;
            case PlayerKey::Enter:
            case PlayerKey::K5:
                if (!repeat && count && bluetoothAudio.connectDevice(btDeviceCursor)) {
                    devicePicker = false;
                    outputCursor = 1;
                    setNotice("Connecting selected headphones", 2500);
                }
                break;
            case PlayerKey::K0:
                if (!repeat) {
                    btDeviceCursor = 0;
                    bluetoothAudio.restartDiscovery();
                }
                break;
            case PlayerKey::Tab:
            case PlayerKey::Backspace:
            case PlayerKey::Dot:
                if (!repeat) {
                    bluetoothAudio.selectDac();
                    outputCursor = 0;
                    devicePicker = false;
                }
                break;
            default: break;
        }
        return;
    }

    switch (event.key) {
        case PlayerKey::K8:
        case PlayerKey::K2:
            outputCursor = 1 - outputCursor;
            break;
        case PlayerKey::Enter:
        case PlayerKey::K5:
            if (repeat) break;
            if (outputCursor == 0) {
                bluetoothAudio.selectDac();
                setNotice("PCM5102A output active", 1800);
            } else {
                bluetoothAudio.selectBluetooth();
                if (!bluetoothAudio.bluetoothConnected() &&
                    strcmp(bluetoothAudio.targetName(), "No device selected") == 0) {
                    btDeviceCursor = 0;
                    devicePicker = true;
                } else {
                    setNotice("Bluetooth output selected", 1800);
                }
            }
            break;
        case PlayerKey::K0:
            if (!repeat && outputCursor == 1) {
                btDeviceCursor = 0;
                bluetoothAudio.restartDiscovery();
                devicePicker = true;
            }
            break;
        case PlayerKey::Power:
            if (!repeat) {
                bluetoothAudio.powerOffBluetooth();
                outputCursor = 0;
                setNotice("Bluetooth radio off", 1800);
            }
            break;
        case PlayerKey::Tab:
        case PlayerKey::Backspace:
        case PlayerKey::Dot:
            if (!repeat) leaveOutputPage();
            break;
        default: break;
    }
}

void handleBluetoothRemote() {
    switch (bluetoothAudio.takeRemoteCommand()) {
        case BluetoothRemoteCommand::Play:
            if (!playing || paused) togglePause();
            break;
        case BluetoothRemoteCommand::Pause:
            if (playing && !paused) togglePause();
            break;
        case BluetoothRemoteCommand::Stop:
            audio.stopSong();
            bluetoothAudio.clearAudio();
            playing = false;
            paused = false;
            break;
        case BluetoothRemoteCommand::Previous: selectNext(false); break;
        case BluetoothRemoteCommand::Next: selectNext(true); break;
        case BluetoothRemoteCommand::None: break;
    }
}
}

void audio_eof_mp3(const char *info) {
    (void)info;
    endOfTrack = true;
}

void audio_info(const char *info) {
    Serial.print("audio: ");
    Serial.println(info);
}

void audio_process_i2s(uint32_t *sample, bool *continueI2S) {
    *continueI2S = bluetoothAudio.processI2sSample(*sample);
}

void setup() {
    Serial.begin(115200);
    pinMode(OLED_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(OLED_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    SPI.begin(18, 19, 23);
    display.begin();
    display.setContrast(220);
    keyboard.begin();
    bluetoothAudio.begin();

    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(85, 27, "MUSIC PLAYER");
    display.drawStr(74, 43, "Scanning SD card...");
    display.sendBuffer();

    if (!SD.begin(SD_CS, SPI)) {
        setNotice("SD card unavailable", 5000);
    } else {
        File musicDirectory = SD.open("/music");
        const bool hasMusicDirectory = musicDirectory && musicDirectory.isDirectory();
        if (musicDirectory) musicDirectory.close();
        scanDirectory(hasMusicDirectory ? "/music" : "/", 0);
        sortTracks();
    }

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(volume);
    randomSeed(esp_random());
    if (trackCount) {
        startTrack(0);
        setNotice("ENTER play  TAB playlist", 3500);
    } else {
        setNotice("No audio in /music", 5000);
    }
    drawPage();
}

void loop() {
    bluetoothAudio.setInputSampleRate(audio.getSampleRate());
    if (!bluetoothAudio.bluetoothRequested() || bluetoothAudio.bluetoothConnected()) audio.loop();
    keyboard.update();
    handleBluetoothRemote();

    if (keyboard.isDown(PlayerKey::Escape) && keyboard.heldMs(PlayerKey::Escape) >= EXIT_HOLD_MS) {
        if (!exitTriggered) {
            exitTriggered = true;
            installMainFirmware();
        }
    } else if (!keyboard.isDown(PlayerKey::Escape)) {
        exitTriggered = false;
    }

    PlayerKeyEvent event;
    while (keyboard.nextEvent(event)) {
        if (event.key == PlayerKey::Escape || event.key == PlayerKey::Shift) continue;
        if (page == Page::NowPlaying) handleNowPlayingKey(event);
        else if (page == Page::Browser) handleBrowserKey(event);
        else handleOutputKey(event);
    }

    if (endOfTrack) handleTrackEnd();
    if (notice.length() && !noticeActive()) notice = "";

    const uint32_t now = millis();
    if (page == Page::NowPlaying && now - lastMarqueeStep >= 250) {
        lastMarqueeStep = now;
        ++marqueeOffset;
    }
    if (now - lastDraw >= DISPLAY_INTERVAL_MS) {
        lastDraw = now;
        drawPage();
    }
    delay(1);
}
