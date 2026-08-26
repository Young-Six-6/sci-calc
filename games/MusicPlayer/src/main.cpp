#include <Arduino.h>
#include <Audio.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <U8g2lib.h>
#include <driver/i2s.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include <algorithm>

#include "MidiPlayer.h"
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
constexpr uint32_t MIDI_BUSY_DISPLAY_INTERVAL_MS = 320;

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI display(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
SafeKeyboard keyboard;
Audio audio;
MidiPlayer midi;

String tracks[MAX_TRACKS];
uint8_t trackCount = 0;
uint8_t currentTrack = 0;
uint8_t browserTrack = 0;
uint8_t volume = 12;
uint8_t volumeBeforeMute = 12;

enum class Page : uint8_t { NowPlaying, Browser };
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
uint32_t lastMidiDiagnostics = 0;
uint64_t midiRenderMicros = 0;
uint32_t midiRenderedFrames = 0;
uint32_t midiRenderPeakMicros = 0;
uint32_t midiWriteFaults = 0;
bool midiPolyphonyWarningShown = false;
String notice;
uint32_t noticeUntil = 0;

void setNotice(const String &text, uint32_t duration);

const char *resetReasonText(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT WDT";
        case ESP_RST_TASK_WDT: return "TASK WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        default: return nullptr;
    }
}

bool isAudioFile(const String &path) {
    const int dot = path.lastIndexOf('.');
    if (dot < 0) return false;
    String extension = path.substring(dot + 1);
    extension.toLowerCase();
    return extension == "mp3" || extension == "wav" ||
           extension == "aac" || extension == "m4a" ||
           extension == "mid" || extension == "midi";
}

bool isMidiFile(const String &path) {
    const size_t length = path.length();
    if (length < 4) return false;
    auto lower = [](char value) {
        return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
    };
    if (length >= 4 && path[length - 4] == '.' &&
        lower(path[length - 3]) == 'm' && lower(path[length - 2]) == 'i' &&
        lower(path[length - 1]) == 'd') return true;
    return length >= 5 && path[length - 5] == '.' &&
           lower(path[length - 4]) == 'm' && lower(path[length - 3]) == 'i' &&
           lower(path[length - 2]) == 'd' && lower(path[length - 1]) == 'i';
}

bool currentIsMidi() {
    return trackCount && isMidiFile(tracks[currentTrack]);
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
    midiRenderMicros = 0;
    midiRenderedFrames = 0;
    midiRenderPeakMicros = 0;
    midiWriteFaults = 0;
    lastMidiDiagnostics = millis();
    midiPolyphonyWarningShown = false;
    paused = false;
    endOfTrack = false;
    midi.stop();
    const bool hadAudioDecoder = audio.getCodec() != 0;
    audio.stopSong();
    if (currentIsMidi()) {
        if (hadAudioDecoder) audio.releaseCodecBuffers();
        playing = midi.open(SD, tracks[currentTrack].c_str());
        if (playing) {
            midi.setVolume(volume);
            midi.setVoiceLimit(MidiPlayer::MAX_VOICES);
            i2s_set_sample_rates(static_cast<i2s_port_t>(audio.getI2sPort()), MidiPlayer::SAMPLE_RATE);
            Serial.printf("MIDI start, duration=%us cache=%s, free heap: %u bytes\n",
                          midi.totalDuration(), midi.durationCacheHit() ? "hit" : "scan",
                          ESP.getFreeHeap());
        } else {
            setNotice(midi.errorText(), 2600);
        }
    } else {
        const uint32_t heapBefore = ESP.getFreeHeap();
        const uint32_t largestBefore = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        playing = audio.connecttoFS(SD, tracks[currentTrack].c_str());
        if (!playing) {
            const bool exists = SD.exists(tracks[currentTrack].c_str());
            setNotice(exists ? "Audio decoder init failed" : "Cannot open this track", 2600);
            Serial.printf("Audio open failed: %s heap=%u largest=%u before=%u/%u\n",
                          tracks[currentTrack].c_str(), ESP.getFreeHeap(),
                          heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                          heapBefore, largestBefore);
        }
    }
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
    if (currentIsMidi()) {
        paused = !paused;
        midi.setPaused(paused);
        i2s_zero_dma_buffer(static_cast<i2s_port_t>(audio.getI2sPort()));
    } else if (audio.pauseResume()) {
        paused = !paused;
    }
}

void changeVolume(int delta) {
    const int changed = constrain(static_cast<int>(muted ? volumeBeforeMute : volume) + delta, 0, MAX_VOLUME);
    volume = changed;
    volumeBeforeMute = changed;
    muted = false;
    audio.setVolume(volume);
    midi.setVolume(volume);
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
    midi.setVolume(volume);
}

void seekBy(int seconds) {
    if (!playing) return;
    if (currentIsMidi()) {
        setNotice("MIDI seek unavailable", 1500);
        return;
    }
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

    const bool midiTrack = currentIsMidi();
    String mode = repeatText();
    if (shuffleEnabled) mode += " SHF";
    if (midiTrack) mode = String(midi.toneProfileName()) + " " + mode;
    display.drawStr(254 - display.getStrWidth(mode.c_str()), 7, mode.c_str());

    display.setFont(u8g2_font_6x10_tf);
    const String title = trackCount ? printableName(tracks[currentTrack]) : String("Put music in /music");
    drawMarquee(title, 2, 24, 252);

    const uint32_t current = playing ? (midiTrack ? midi.currentTime() : audio.getAudioCurrentTime()) : 0;
    const uint32_t duration = playing
        ? (midiTrack ? midi.totalDuration() : audio.getAudioFileDuration()) : 0;
    display.drawFrame(2, 31, 252, 7);
    if (duration > 0) {
        const uint16_t progress = min<uint32_t>(250, (current * 250ULL) / duration);
        if (progress) display.drawBox(3, 32, progress, 5);
    }

    display.setFont(u8g2_font_5x7_tf);
    String timing = timeText(current) + "/" + (duration ? timeText(duration) : String("--:--"));
    display.drawStr(2, 48, timing.c_str());
    String state;
    if (!playing) state = "STOP";
    else if (paused) state = "PAUSE";
    else if (midiTrack) state = String(midi.tempoBpm()) + "B " + String(midi.activeVoices()) + "V";
    else state = "PLAY";
    display.drawStr(128 - display.getStrWidth(state.c_str()) / 2, 48, state.c_str());
    String volumeText = muted ? "MUTE" : "VOL " + String(volume);
    display.drawStr(254 - display.getStrWidth(volumeText.c_str()), 48, volumeText.c_str());

    String footer = noticeActive() ? notice : String("ENTER play  TAB playlist");
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
        display.drawStr(51, 36, "No supported audio/MIDI files");
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

void drawPage() {
    if (page == Page::NowPlaying) drawNowPlaying();
    else drawBrowser();
}

bool installMainFirmware() {
    midi.stop();
    audio.stopSong();
    playing = false;
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
        case PlayerKey::K4:
            if (currentIsMidi() && shift) {
                midi.changeTranspose(-1);
                String text = "MIDI transpose ";
                if (midi.transpose() >= 0) text += "+";
                setNotice(text + String(midi.transpose()), 1200);
            } else seekBy(shift ? -60 : -10);
            break;
        case PlayerKey::K6:
            if (currentIsMidi() && shift) {
                midi.changeTranspose(1);
                String text = "MIDI transpose ";
                if (midi.transpose() >= 0) text += "+";
                setNotice(text + String(midi.transpose()), 1200);
            } else seekBy(shift ? 60 : 10);
            break;
        case PlayerKey::K7: changeVolume(-1); break;
        case PlayerKey::K9: changeVolume(1); break;
        case PlayerKey::Minus: changeVolume(-1); break;
        case PlayerKey::Plus: changeVolume(1); break;
        case PlayerKey::K1:
            if (!repeat && shift && currentIsMidi()) {
                midi.cycleToneProfile();
                setNotice(String("MIDI tone ") + midi.toneProfileName(), 1600);
            } else if (!repeat && !shift) {
                cycleRepeat();
            }
            break;
        case PlayerKey::Extra: if (!repeat) cycleRepeat(); break;
        case PlayerKey::K3: if (!repeat) shuffleEnabled = !shuffleEnabled; break;
        case PlayerKey::Angle: if (!repeat) shuffleEnabled = !shuffleEnabled; break;
        case PlayerKey::Multiply: if (!repeat) toggleMute(); break;
        case PlayerKey::Tab:
            if (!repeat && shift) {
                setNotice("PCM5102A output only", 1600);
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
        setNotice("PCM5102A output only", 1600);
        page = Page::NowPlaying;
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

}

void audio_eof_mp3(const char *info) {
    (void)info;
    endOfTrack = true;
}

void audio_info(const char *info) {
    Serial.print("audio: ");
    Serial.println(info);
}

void pumpMidiAudio() {
    if (!playing || paused || !currentIsMidi()) return;
    midi.setVoiceLimit(MidiPlayer::MAX_VOICES);

    constexpr size_t MIDI_BATCH_FRAMES = 256;
    uint32_t frames[MIDI_BATCH_FRAMES];
    const uint32_t renderStarted = micros();
    const size_t frameCount = midi.render(frames, MIDI_BATCH_FRAMES);
    const uint32_t renderElapsed = micros() - renderStarted;
    midiRenderMicros += renderElapsed;
    midiRenderedFrames += frameCount;
    midiRenderPeakMicros = max(midiRenderPeakMicros, renderElapsed);
    if (frameCount) {
        size_t bytesWritten = 0;
        const size_t bytesRequested = frameCount * sizeof(uint32_t);
        const esp_err_t result = i2s_write(static_cast<i2s_port_t>(audio.getI2sPort()), frames,
                                           bytesRequested, &bytesWritten, pdMS_TO_TICKS(20));
        if (result != ESP_OK || bytesWritten != bytesRequested) ++midiWriteFaults;
    }

    if (midi.hasError()) {
        playing = false;
        paused = false;
        setNotice(midi.errorText(), 2600);
    } else if (midi.isFinished()) {
        endOfTrack = true;
    }
}

void setup() {
    Serial.begin(115200);
    const esp_reset_reason_t resetReason = esp_reset_reason();
    Serial.printf("Boot reset=%u heap=%u internal=%u largest=%u\n",
                  static_cast<unsigned>(resetReason), ESP.getFreeHeap(),
                  heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    pinMode(OLED_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(OLED_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    SPI.begin(18, 19, 23);
    display.begin();
    display.setContrast(220);
    keyboard.begin();

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
    if (const char *resetText = resetReasonText(resetReason)) {
        setNotice(String("Last reset: ") + resetText, 6000);
    }
    drawPage();
}

void loop() {
    if (currentIsMidi()) {
        pumpMidiAudio();
    } else {
        audio.loop();
    }
    keyboard.update();

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
        else handleBrowserKey(event);
    }

    if (endOfTrack) handleTrackEnd();
    if (notice.length() && !noticeActive()) notice = "";

    const uint32_t now = millis();
    if (playing && currentIsMidi() && now - lastMidiDiagnostics >= 5000) {
        lastMidiDiagnostics = now;
        const uint32_t limiterHits = midi.takeLimiterHits();
        const uint32_t voiceSteals = midi.takeVoiceSteals();
        const uint32_t mergedNoteOns = midi.takeMergedNoteOns();
        const uint32_t droppedNoteOns = midi.takeDroppedNoteOns();
        const uint32_t sustainVoiceSteals = midi.takeSustainVoiceSteals();
        const uint32_t heldVoiceSteals = midi.takeHeldVoiceSteals();
        if ((heldVoiceSteals || droppedNoteOns) && !midiPolyphonyWarningShown) {
            midiPolyphonyWarningShown = true;
            setNotice("MIDI polyphony limit", 2400);
        }
        const uint32_t loadPermille = midiRenderedFrames
            ? static_cast<uint32_t>((midiRenderMicros * MidiPlayer::SAMPLE_RATE * 1000ULL) /
                                    (static_cast<uint64_t>(midiRenderedFrames) * 1000000ULL))
            : 0;
        Serial.printf("MIDI perf load=%u.%u%% peak=%uus limiter=%u merged=%u drop=%u reuse=%u sustain=%u held=%u voices=%u i2s=%u heap=%u\n",
                      loadPermille / 10, loadPermille % 10, midiRenderPeakMicros,
                      limiterHits, mergedNoteOns, droppedNoteOns, voiceSteals,
                      sustainVoiceSteals, heldVoiceSteals,
                      midi.activeVoices(), midiWriteFaults,
                      ESP.getFreeHeap());
        midiRenderMicros = 0;
        midiRenderedFrames = 0;
        midiRenderPeakMicros = 0;
        midiWriteFaults = 0;
    }
    if (page == Page::NowPlaying && now - lastMarqueeStep >= 250) {
        lastMarqueeStep = now;
        ++marqueeOffset;
    }
    const uint32_t displayInterval = currentIsMidi() && midi.activeVoices() >= MidiPlayer::MAX_VOICES
        ? MIDI_BUSY_DISPLAY_INTERVAL_MS : DISPLAY_INTERVAL_MS;
    if (now - lastDraw >= displayInterval) {
        lastDraw = now;
        drawPage();
    }
    delay(1);
}
