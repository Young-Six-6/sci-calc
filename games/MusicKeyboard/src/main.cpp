#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <U8g2lib.h>
#include <esp_heap_caps.h>

#include "LiveSynth.h"
#include "MidiRecorder.h"
#include "SafeKeyboard.h"

namespace {
constexpr uint8_t OLED_CS = 5;
constexpr uint8_t OLED_DC = 16;
constexpr uint8_t OLED_RST = 17;
constexpr uint8_t SD_CS = 4;
constexpr uint32_t EXIT_HOLD_MS = 1000;
constexpr uint8_t KEY_SLOT_COUNT = 31;
constexpr uint8_t MAX_NOTES_PER_KEY = 3;

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI display(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
SafeKeyboard keyboard;
LiveSynth synth;
MidiRecorder recorder;

enum class Page : uint8_t { Keyboard, Scope, Recorder, Settings };
enum class Layout : uint8_t { Calculator, Chords, Chromatic };

struct NoteBinding {
    bool active = false;
    uint8_t count = 0;
    uint8_t notes[MAX_NOTES_PER_KEY] = {};
};

Page page = Page::Keyboard;
Layout layout = Layout::Calculator;
NoteBinding bindings[KEY_SLOT_COUNT];
uint8_t visibleNotes[128] = {};

uint8_t selectedTone = LiveSynth::Piano;
uint8_t octave = 4;
int8_t transpose = 0;
uint8_t root = 0;
uint8_t velocity = 100;
uint8_t volume = 7;
uint16_t bpm = 120;
bool sustain = false;
bool metronome = false;
bool sdReady = false;
bool exitTriggered = false;
bool recordingOverflow = false;

bool playback = false;
uint16_t playbackIndex = 0;
uint32_t playbackStartedAt = 0;

uint32_t nextBeatAt = 0;
uint8_t beatIndex = 0;
uint32_t beatFlashUntil = 0;
uint32_t lastDraw = 0;
char notice[42] = {};
uint32_t noticeUntil = 0;

const char *NOTE_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

void setNotice(const char *text, uint32_t duration = 1800) {
    snprintf(notice, sizeof(notice), "%s", text ? text : "");
    noticeUntil = millis() + duration;
}

bool noticeActive() {
    return notice[0] && static_cast<int32_t>(noticeUntil - millis()) > 0;
}

void formatNote(uint8_t note, char *buffer, size_t size) {
    snprintf(buffer, size, "%s%d", NOTE_NAMES[note % 12], static_cast<int>(note / 12) - 1);
}

uint8_t keyboardBaseNote() {
    const int16_t note = static_cast<int16_t>((octave + 1) * 12) + root + transpose;
    return static_cast<uint8_t>(max<int16_t>(24, min<int16_t>(96, note)));
}

void clearVisibleNotes() {
    memset(visibleNotes, 0, sizeof(visibleNotes));
}

void markRecordFailure() {
    if (recorder.isRecording()) recordingOverflow = true;
}

void soundingNoteOn(uint8_t note, uint8_t noteVelocity, bool recordEvent) {
    synth.noteOn(note, noteVelocity);
    if (visibleNotes[note] < UINT8_MAX) ++visibleNotes[note];
    if (recordEvent && recorder.isRecording() &&
        !recorder.addNoteOn(note, noteVelocity, millis())) markRecordFailure();
}

void soundingNoteOff(uint8_t note, bool recordEvent) {
    synth.noteOff(note);
    if (visibleNotes[note]) --visibleNotes[note];
    if (recordEvent && recorder.isRecording() &&
        !recorder.addNoteOff(note, millis())) markRecordFailure();
}

NoteBinding *bindingFor(KeyboardKey key) {
    const uint8_t index = static_cast<uint8_t>(key);
    return index < KEY_SLOT_COUNT ? &bindings[index] : nullptr;
}

void releaseBinding(KeyboardKey key, bool recordEvent = true) {
    NoteBinding *binding = bindingFor(key);
    if (!binding || !binding->active) return;
    for (uint8_t index = 0; index < binding->count; ++index) {
        soundingNoteOff(binding->notes[index], recordEvent);
    }
    *binding = NoteBinding{};
}

void releaseAllBindings(bool recordEvent = true) {
    for (uint8_t index = 0; index < KEY_SLOT_COUNT; ++index) {
        if (!bindings[index].active) continue;
        for (uint8_t note = 0; note < bindings[index].count; ++note) {
            soundingNoteOff(bindings[index].notes[note], recordEvent);
        }
        bindings[index] = NoteBinding{};
    }
}

void panic(bool recordEvent = true) {
    releaseAllBindings(recordEvent);
    synth.allNotesOff(true);
    clearVisibleNotes();
    setNotice("All notes off", 900);
}

uint8_t appendMappedNote(NoteBinding &binding, int16_t note) {
    if (binding.count >= MAX_NOTES_PER_KEY) return binding.count;
    note = max<int16_t>(24, min<int16_t>(108, note));
    binding.notes[binding.count++] = static_cast<uint8_t>(note);
    return binding.count;
}

bool mapKeyToNotes(KeyboardKey key, NoteBinding &binding, int8_t octaveLift = 0) {
    binding = NoteBinding{};
    const int16_t base = keyboardBaseNote() + octaveLift;

    if (layout != Layout::Calculator) {
        switch (key) {
            case KeyboardKey::Multiply:
                appendMappedNote(binding, base);
                appendMappedNote(binding, base + 4);
                appendMappedNote(binding, base + 7);
                break;
            case KeyboardKey::Divide:
                appendMappedNote(binding, base + 2);
                appendMappedNote(binding, base + 5);
                appendMappedNote(binding, base + 9);
                break;
            case KeyboardKey::Minus:
                appendMappedNote(binding, base + 5);
                appendMappedNote(binding, base + 9);
                appendMappedNote(binding, base + 12);
                break;
            case KeyboardKey::Plus:
                appendMappedNote(binding, base + 7);
                appendMappedNote(binding, base + 11);
                appendMappedNote(binding, base + 14);
                break;
            case KeyboardKey::Power:
                appendMappedNote(binding, base + 9);
                appendMappedNote(binding, base + 12);
                appendMappedNote(binding, base + 16);
                break;
            default:
                break;
        }
    }
    if (binding.count) return true;

    int8_t offset = INT8_MIN;
    if (layout == Layout::Calculator || layout == Layout::Chords) {
        switch (key) {
            case KeyboardKey::Divide:
                if (layout == Layout::Calculator) offset = 16;
                break;
            case KeyboardKey::Multiply:
                if (layout == Layout::Calculator) offset = 17;
                break;
            case KeyboardKey::Minus:
                if (layout == Layout::Calculator) offset = 19;
                break;
            case KeyboardKey::Plus:
                if (layout == Layout::Calculator) offset = 21;
                break;
            case KeyboardKey::Power:
                if (layout == Layout::Calculator) offset = 23;
                break;
            case KeyboardKey::Dot: offset = -3; break;
            case KeyboardKey::K0: offset = -1; break;
            case KeyboardKey::K1: offset = 0; break;
            case KeyboardKey::K2: offset = 2; break;
            case KeyboardKey::K3: offset = 4; break;
            case KeyboardKey::K4: offset = 5; break;
            case KeyboardKey::K5: offset = 7; break;
            case KeyboardKey::K6: offset = 9; break;
            case KeyboardKey::K7: offset = 11; break;
            case KeyboardKey::K8: offset = 12; break;
            case KeyboardKey::K9: offset = 14; break;
            case KeyboardKey::Enter:
                if (layout == Layout::Chords) offset = 12;
                break;
            case KeyboardKey::Sin: offset = 1; break;
            case KeyboardKey::Sec: offset = 3; break;
            case KeyboardKey::Cos: offset = 6; break;
            case KeyboardKey::Csc: offset = 8; break;
            case KeyboardKey::Tan: offset = 10; break;
            case KeyboardKey::Cot: offset = 13; break;
            case KeyboardKey::Exp: offset = 15; break;
            case KeyboardKey::Extra: offset = 18; break;
            default: break;
        }
    } else {
        switch (key) {
            case KeyboardKey::K1: offset = 0; break;
            case KeyboardKey::K2: offset = 1; break;
            case KeyboardKey::K3: offset = 2; break;
            case KeyboardKey::K4: offset = 3; break;
            case KeyboardKey::K5: offset = 4; break;
            case KeyboardKey::K6: offset = 5; break;
            case KeyboardKey::K7: offset = 6; break;
            case KeyboardKey::K8: offset = 7; break;
            case KeyboardKey::K9: offset = 8; break;
            case KeyboardKey::K0: offset = 9; break;
            case KeyboardKey::Dot: offset = 10; break;
            case KeyboardKey::Enter: offset = 11; break;
            case KeyboardKey::Sin: offset = 12; break;
            case KeyboardKey::Sec: offset = 13; break;
            case KeyboardKey::Cos: offset = 14; break;
            case KeyboardKey::Csc: offset = 15; break;
            case KeyboardKey::Tan: offset = 16; break;
            case KeyboardKey::Cot: offset = 17; break;
            case KeyboardKey::Exp: offset = 18; break;
            case KeyboardKey::Extra: offset = 19; break;
            default: break;
        }
    }
    if (offset == INT8_MIN) return false;
    appendMappedNote(binding, base + offset);
    return true;
}

void stopPlayback() {
    if (!playback) return;
    playback = false;
    playbackIndex = 0;
    synth.allNotesOff(true);
    clearVisibleNotes();
    synth.setTone(selectedTone);
    synth.setVolume(volume);
    synth.setSustain(sustain);
}

void startBinding(KeyboardKey key, int8_t octaveLift = 0) {
    NoteBinding *slot = bindingFor(key);
    if (!slot || slot->active) return;
    if (playback) stopPlayback();
    NoteBinding mapped;
    if (!mapKeyToNotes(key, mapped, octaveLift)) return;
    mapped.active = true;
    *slot = mapped;
    for (uint8_t index = 0; index < slot->count; ++index) {
        soundingNoteOn(slot->notes[index], velocity, true);
    }
}

void stopRecording(bool overflowed = false) {
    if (!recorder.isRecording()) return;
    releaseAllBindings(true);
    if (sustain && !recorder.addSustain(false, millis())) recordingOverflow = true;
    recorder.stop(millis());
    synth.setSustain(false);
    synth.allNotesOff(false);
    synth.setSustain(sustain);
    clearVisibleNotes();

    const bool hasTake = recorder.hasTake();
    const bool saved = hasTake && sdReady && recorder.saveToSd();
    char message[42];
    if (!hasTake) {
        snprintf(message, sizeof(message), "%s", "Empty take");
    } else if (saved) {
        const char *name = strrchr(recorder.savedPath(), '/');
        snprintf(message, sizeof(message), "%s%s", overflowed ? "Full - saved " : "Saved ",
                 name ? name + 1 : recorder.savedPath());
    } else if (sdReady) {
        snprintf(message, sizeof(message), "%s", "Save failed; take in RAM");
    } else {
        snprintf(message, sizeof(message), "%s", "No SD; take in RAM");
    }
    recordingOverflow = false;
    setNotice(message, 2800);
}

void startRecording() {
    stopPlayback();
    releaseAllBindings(false);
    synth.allNotesOff(true);
    clearVisibleNotes();
    recorder.start(millis(), bpm, selectedTone);
    if (sustain && !recorder.addSustain(true, millis())) markRecordFailure();
    recordingOverflow = false;
    page = Page::Recorder;
    setNotice("Recording started", 1200);
}

void toggleRecording() {
    if (recorder.isRecording()) stopRecording();
    else startRecording();
}

void clearTake() {
    if (recorder.isRecording()) {
        releaseAllBindings(false);
        synth.allNotesOff(true);
        clearVisibleNotes();
    }
    stopPlayback();
    recorder.clear();
    recordingOverflow = false;
    setNotice("Take cleared", 1200);
}

void startPlayback() {
    if (recorder.isRecording()) stopRecording();
    if (!recorder.hasTake()) {
        setNotice("No recorded take", 1400);
        return;
    }
    releaseAllBindings(false);
    synth.allNotesOff(true);
    clearVisibleNotes();
    synth.setSustain(false);
    synth.setTone(recorder.initialTone());
    playback = true;
    playbackIndex = 0;
    playbackStartedAt = millis();
    page = Page::Recorder;
    setNotice("Playback", 900);
}

void togglePlayback() {
    if (playback) {
        stopPlayback();
        setNotice("Playback stopped", 1000);
    } else {
        startPlayback();
    }
}

void servicePlayback() {
    if (!playback) return;
    const uint32_t elapsed = millis() - playbackStartedAt;
    while (playbackIndex < recorder.eventCount() &&
           recorder.event(playbackIndex).timeMs <= elapsed) {
        const MidiRecorder::Event &entry = recorder.event(playbackIndex++);
        switch (static_cast<MidiRecorder::EventType>(entry.type)) {
            case MidiRecorder::EventType::NoteOn:
                soundingNoteOn(entry.data, entry.value, false);
                break;
            case MidiRecorder::EventType::NoteOff:
                soundingNoteOff(entry.data, false);
                break;
            case MidiRecorder::EventType::Program:
                synth.setTone(entry.data);
                break;
            case MidiRecorder::EventType::Sustain:
                synth.setSustain(entry.data != 0);
                break;
            case MidiRecorder::EventType::Tempo:
                break;
        }
    }
    if (playbackIndex >= recorder.eventCount() && elapsed >= recorder.durationMs() + 900) {
        stopPlayback();
        setNotice("Playback complete", 1200);
    }
}

void changeTone(int8_t direction) {
    selectedTone = static_cast<uint8_t>(
        (selectedTone + LiveSynth::TONE_COUNT + direction) % LiveSynth::TONE_COUNT);
    synth.setTone(selectedTone);
    if (recorder.isRecording() && !recorder.addProgram(selectedTone, millis())) markRecordFailure();
    char text[30];
    snprintf(text, sizeof(text), "Tone: %s", LiveSynth::toneName(selectedTone));
    setNotice(text, 1000);
}

void changeOctave(int8_t direction) {
    octave = static_cast<uint8_t>(max<int16_t>(2, min<int16_t>(6, octave + direction)));
    char text[22];
    snprintf(text, sizeof(text), "Octave: %u", octave);
    setNotice(text, 800);
}

void changeTranspose(int8_t direction) {
    transpose = static_cast<int8_t>(max<int16_t>(-12, min<int16_t>(12, transpose + direction)));
    char text[24];
    snprintf(text, sizeof(text), "Transpose: %+d", transpose);
    setNotice(text, 800);
}

void changeRoot(int8_t direction) {
    root = static_cast<uint8_t>((root + 12 + direction) % 12);
    char text[20];
    snprintf(text, sizeof(text), "Root: %s", NOTE_NAMES[root]);
    setNotice(text, 800);
}

void changeVolume(int8_t direction) {
    volume = static_cast<uint8_t>(max<int16_t>(0, min<int16_t>(10, volume + direction)));
    synth.setVolume(volume);
    char text[20];
    snprintf(text, sizeof(text), "Volume: %u", volume);
    setNotice(text, 800);
}

void changeBpm(int8_t direction) {
    bpm = static_cast<uint16_t>(max<int32_t>(40, min<int32_t>(240, bpm + direction * 5)));
    nextBeatAt = millis();
    if (recorder.isRecording() && !recorder.addTempo(bpm, millis())) markRecordFailure();
    char text[18];
    snprintf(text, sizeof(text), "Tempo: %u", bpm);
    setNotice(text, 800);
}

void changeVelocity(int8_t direction) {
    velocity = static_cast<uint8_t>(max<int16_t>(32, min<int16_t>(127, velocity + direction * 8)));
    char text[20];
    snprintf(text, sizeof(text), "Velocity: %u", velocity);
    setNotice(text, 800);
}

void toggleSustain() {
    sustain = !sustain;
    synth.setSustain(sustain);
    if (recorder.isRecording() && !recorder.addSustain(sustain, millis())) markRecordFailure();
    setNotice(sustain ? "Sustain on" : "Sustain off", 900);
}

void toggleMetronome() {
    metronome = !metronome;
    beatIndex = 0;
    nextBeatAt = millis();
    setNotice(metronome ? "Metronome on" : "Metronome off", 900);
}

bool repeatableShiftControl(KeyboardKey key) {
    switch (key) {
        case KeyboardKey::K2:
        case KeyboardKey::K4:
        case KeyboardKey::K6:
        case KeyboardKey::K8:
        case KeyboardKey::K7:
        case KeyboardKey::K9:
        case KeyboardKey::Divide:
        case KeyboardKey::Multiply:
        case KeyboardKey::Minus:
        case KeyboardKey::Plus:
        case KeyboardKey::Power:
        case KeyboardKey::Exp:
            return true;
        default:
            return false;
    }
}

void handleShiftControl(KeyboardKey key, bool repeat) {
    if (repeat && !repeatableShiftControl(key)) return;
    switch (key) {
        case KeyboardKey::K1: if (!repeat) changeTone(-1); break;
        case KeyboardKey::K3: if (!repeat) changeTone(1); break;
        case KeyboardKey::K4: changeOctave(-1); break;
        case KeyboardKey::K6: changeOctave(1); break;
        case KeyboardKey::K2: changeTranspose(-1); break;
        case KeyboardKey::K8: changeTranspose(1); break;
        case KeyboardKey::Divide: changeRoot(-1); break;
        case KeyboardKey::Multiply: changeRoot(1); break;
        case KeyboardKey::Minus: changeVolume(-1); break;
        case KeyboardKey::Plus: changeVolume(1); break;
        case KeyboardKey::K7: changeBpm(-1); break;
        case KeyboardKey::K9: changeBpm(1); break;
        case KeyboardKey::Power: changeVelocity(-1); break;
        case KeyboardKey::Exp: changeVelocity(1); break;
        case KeyboardKey::K5: if (!repeat) toggleSustain(); break;
        case KeyboardKey::Dot: if (!repeat) toggleMetronome(); break;
        case KeyboardKey::K0: if (!repeat) toggleRecording(); break;
        case KeyboardKey::Enter: if (!repeat) togglePlayback(); break;
        case KeyboardKey::Backspace: if (!repeat) clearTake(); break;
        case KeyboardKey::Tab:
            if (!repeat) {
                layout = static_cast<Layout>((static_cast<uint8_t>(layout) + 1) % 3);
                if (layout == Layout::Calculator) setNotice("Calculator layout", 1100);
                else if (layout == Layout::Chords) setNotice("Chord layout", 1100);
                else setNotice("Chromatic layout", 1100);
            }
            break;
        case KeyboardKey::Angle:
            if (!repeat) page = Page::Settings;
            break;
        case KeyboardKey::Extra:
            if (!repeat) panic();
            break;
        default:
            break;
    }
}

void handleKeyboardEvent(const KeyboardEvent &event) {
    if (event.action == KeyboardAction::Release) {
        releaseBinding(event.key, true);
        return;
    }
    if (event.key == KeyboardKey::Shift) return;
    if (event.key == KeyboardKey::Escape) {
        if (event.action == KeyboardAction::Press) panic();
        return;
    }

    const bool shift = keyboard.isDown(KeyboardKey::Shift);
    const bool repeat = event.action == KeyboardAction::Repeat;
    if (shift) {
        handleShiftControl(event.key, repeat);
        return;
    }
    if (repeat) return;

    switch (event.key) {
        case KeyboardKey::Tab:
            page = static_cast<Page>((static_cast<uint8_t>(page) + 1) % 4);
            break;
        case KeyboardKey::Angle:
            toggleSustain();
            break;
        case KeyboardKey::Backspace:
            panic();
            break;
        default: {
            const int8_t octaveLift = layout == Layout::Calculator &&
                                      event.key != KeyboardKey::Enter &&
                                      keyboard.isDown(KeyboardKey::Enter) ? 12 : 0;
            startBinding(event.key, octaveLift);
            break;
        }
    }
}

void serviceMetronome() {
    if (!metronome) return;
    const uint32_t now = millis();
    if (static_cast<int32_t>(now - nextBeatAt) < 0) return;
    synth.click(beatIndex == 0);
    beatFlashUntil = now + 80;
    beatIndex = (beatIndex + 1) % 4;
    const uint32_t interval = 60000UL / bpm;
    nextBeatAt += interval;
    if (static_cast<int32_t>(now - nextBeatAt) >= 0) nextBeatAt = now + interval;
}

void drawHeader(const char *pageName) {
    display.setFont(u8g2_font_5x7_tf);
    char baseName[8];
    formatNote(keyboardBaseNote(), baseName, sizeof(baseName));
    char left[56];
    snprintf(left, sizeof(left), "%s  %s  T%+d  V%u", LiveSynth::toneName(selectedTone),
             baseName, transpose, volume);
    display.drawStr(1, 7, left);
    const int width = display.getStrWidth(pageName);
    display.drawStr(max(0, 255 - width), 7, pageName);
    display.drawHLine(0, 9, 256);

    if (recorder.isRecording()) display.drawBox(202, 1, 4, 4);
    else if (playback) display.drawFrame(202, 1, 4, 4);
    if (metronome && static_cast<int32_t>(beatFlashUntil - millis()) > 0) {
        display.drawBox(211, 1, 4, 4);
    }
}

void activeNoteText(char *buffer, size_t size) {
    buffer[0] = '\0';
    uint8_t shown = 0;
    uint8_t total = 0;
    for (uint8_t note = 0; note < 128; ++note) {
        if (!visibleNotes[note]) continue;
        ++total;
        if (shown >= 6) continue;
        char name[8];
        formatNote(note, name, sizeof(name));
        if (shown) strncat(buffer, " ", size - strlen(buffer) - 1);
        strncat(buffer, name, size - strlen(buffer) - 1);
        ++shown;
    }
    if (!total) snprintf(buffer, size, "--");
    else if (total > shown && strlen(buffer) + 2 < size) strncat(buffer, " +", size - strlen(buffer) - 1);
}

void drawPiano() {
    constexpr uint8_t WHITE_OFFSETS[7] = {0, 2, 4, 5, 7, 9, 11};
    constexpr uint8_t BLACK_AFTER[5] = {0, 1, 3, 4, 5};
    constexpr uint8_t BLACK_OFFSETS[5] = {1, 3, 6, 8, 10};
    constexpr uint8_t keyWidth = 18;
    constexpr uint8_t top = 34;
    constexpr uint8_t height = 30;
    const uint8_t firstC = keyboardBaseNote() / 12 * 12;

    for (uint8_t white = 0; white < 14; ++white) {
        const uint8_t note = firstC + (white / 7) * 12 + WHITE_OFFSETS[white % 7];
        const uint8_t x = 2 + white * keyWidth;
        if (note < 128 && visibleNotes[note]) display.drawBox(x + 1, top + 1, keyWidth - 2, height - 2);
        else display.drawFrame(x, top, keyWidth + 1, height);
    }
    for (uint8_t octaveIndex = 0; octaveIndex < 2; ++octaveIndex) {
        for (uint8_t black = 0; black < 5; ++black) {
            const uint8_t note = firstC + octaveIndex * 12 + BLACK_OFFSETS[black];
            const uint8_t x = 2 + (octaveIndex * 7 + BLACK_AFTER[black] + 1) * keyWidth - 5;
            if (note < 128 && visibleNotes[note]) display.drawBox(x, top, 10, 18);
            else display.drawFrame(x, top, 10, 18);
        }
    }
}

void drawKeyboardPage() {
    drawHeader("KEYS");
    char notes[64];
    activeNoteText(notes, sizeof(notes));
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(3, 23, notes);
    display.setFont(u8g2_font_5x7_tf);
    char status[42];
    const char *layoutName = layout == Layout::Calculator ? "CALC" :
                             layout == Layout::Chords ? "CHORD" : "CHROM";
    const char *upper = layout == Layout::Calculator && keyboard.isDown(KeyboardKey::Enter)
        ? " UP" : "";
    snprintf(status, sizeof(status), "%s%s  %u BPM  %s", layoutName, upper,
             bpm, sustain ? "SUS" : "---");
    display.drawStr(3, 31, status);
    drawPiano();
}

void drawScopePage() {
    drawHeader("SCOPE");
    int8_t samples[LiveSynth::SCOPE_SAMPLES];
    synth.copyScope(samples, sizeof(samples));
    display.drawHLine(0, 35, 256);
    int previousY = 35 - samples[0] * 20 / 128;
    for (uint8_t index = 1; index < LiveSynth::SCOPE_SAMPLES; ++index) {
        const int y = 35 - samples[index] * 20 / 128;
        display.drawLine((index - 1) * 2, previousY, index * 2, y);
        previousY = y;
    }

    display.setFont(u8g2_font_5x7_tf);
    char status[56];
    const uint8_t peak = min<uint32_t>(100, synth.peakLevel() * 100UL / 32767UL);
    snprintf(status, sizeof(status), "VOICES %u/%u   PEAK %u%%   CPU %u.%u%%",
             synth.activeVoices(), LiveSynth::MAX_VOICES, peak,
             synth.loadPermille() / 10, synth.loadPermille() % 10);
    display.drawStr(2, 17, status);
    display.drawFrame(1, 56, 125, 7);
    display.drawBox(2, 57, peak * 123 / 100, 5);
    display.drawFrame(130, 56, 125, 7);
    display.drawBox(131, 57, min<uint16_t>(1000, synth.loadPermille()) * 123 / 1000, 5);
}

void formatTime(uint32_t milliseconds, char *buffer, size_t size) {
    const uint32_t minutes = milliseconds / 60000UL;
    const uint32_t seconds = milliseconds / 1000UL % 60;
    const uint32_t tenths = milliseconds / 100UL % 10;
    snprintf(buffer, size, "%02lu:%02lu.%lu",
             static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds),
             static_cast<unsigned long>(tenths));
}

void drawRecorderPage() {
    drawHeader("REC");
    const uint32_t shownTime = recorder.isRecording()
        ? recorder.elapsedMs(millis())
        : playback ? min<uint32_t>(millis() - playbackStartedAt, recorder.durationMs())
                   : recorder.durationMs();
    char timeText[16];
    formatTime(shownTime, timeText, sizeof(timeText));

    display.setFont(u8g2_font_6x10_tf);
    const char *state = recorder.isRecording() ? "RECORDING" : playback ? "PLAYBACK" :
                        recorder.hasTake() ? "TAKE READY" : "EMPTY";
    display.drawStr(3, 22, state);
    display.setFont(u8g2_font_10x20_tf);
    display.drawStr(3, 43, timeText);

    display.setFont(u8g2_font_5x7_tf);
    char events[32];
    snprintf(events, sizeof(events), "%u / %u EVENTS", recorder.eventCount(),
             MidiRecorder::MAX_RECORDED_EVENTS);
    display.drawStr(132, 21, events);
    display.drawFrame(132, 27, 122, 7);
    display.drawBox(133, 28,
        static_cast<uint32_t>(recorder.eventCount()) * 120 / MidiRecorder::MAX_RECORDED_EVENTS, 5);
    const char *path = recorder.savedPath();
    const char *name = path[0] ? strrchr(path, '/') : nullptr;
    display.drawStr(132, 43, name ? name + 1 : (sdReady ? "RAM TAKE" : "NO SD / RAM"));
    char detail[34];
    snprintf(detail, sizeof(detail), "%u BPM  %s", recorder.initialBpm(),
             LiveSynth::toneName(recorder.initialTone()));
    display.drawStr(3, 54, detail);
    display.drawStr(3, 63, recorder.isFull() ? "EVENT BUFFER FULL" : "MIDI FORMAT 0");
}

void drawSettingsPage() {
    drawHeader("SET");
    display.setFont(u8g2_font_5x7_tf);
    char line[34];
    snprintf(line, sizeof(line), "TONE   %s", LiveSynth::toneName(selectedTone));
    display.drawStr(3, 19, line);
    snprintf(line, sizeof(line), "OCTAVE %u", octave);
    display.drawStr(3, 29, line);
    snprintf(line, sizeof(line), "ROOT   %s", NOTE_NAMES[root]);
    display.drawStr(3, 39, line);
    snprintf(line, sizeof(line), "TRANS  %+d", transpose);
    display.drawStr(3, 49, line);
    const char *layoutName = layout == Layout::Calculator ? "CALCULATOR" :
                             layout == Layout::Chords ? "CHORDS" : "CHROMATIC";
    snprintf(line, sizeof(line), "LAYOUT %s", layoutName);
    display.drawStr(3, 59, line);

    snprintf(line, sizeof(line), "VELOCITY %u", velocity);
    display.drawStr(134, 19, line);
    snprintf(line, sizeof(line), "VOLUME   %u / 10", volume);
    display.drawStr(134, 29, line);
    snprintf(line, sizeof(line), "TEMPO    %u BPM", bpm);
    display.drawStr(134, 39, line);
    snprintf(line, sizeof(line), "SUSTAIN  %s", sustain ? "ON" : "OFF");
    display.drawStr(134, 49, line);
    snprintf(line, sizeof(line), "METRO    %s", metronome ? "ON" : "OFF");
    display.drawStr(134, 59, line);
}

void drawPage() {
    display.clearBuffer();
    switch (page) {
        case Page::Keyboard: drawKeyboardPage(); break;
        case Page::Scope: drawScopePage(); break;
        case Page::Recorder: drawRecorderPage(); break;
        case Page::Settings: drawSettingsPage(); break;
    }
    if (noticeActive()) {
        display.setDrawColor(1);
        display.drawBox(0, 52, 256, 12);
        display.setDrawColor(0);
        display.setFont(u8g2_font_6x10_tf);
        const int width = display.getStrWidth(notice);
        display.drawStr(max(2, (256 - width) / 2), 62, notice);
        display.setDrawColor(1);
    }
    display.sendBuffer();
}

void resumeSynthAfterUpdateFailure() {
    if (synth.begin()) {
        synth.setTone(selectedTone);
        synth.setVolume(volume);
        synth.setSustain(sustain);
    }
}

bool installMainFirmware() {
    if (recorder.isRecording()) stopRecording();
    stopPlayback();
    panic(false);

    if (!sdReady) sdReady = SD.begin(SD_CS, SPI);
    File firmware = sdReady ? SD.open("/main.bin", FILE_READ) : File();
    if (!firmware || firmware.isDirectory() || firmware.size() == 0) {
        if (firmware) firmware.close();
        setNotice("Missing /main.bin", 2600);
        return false;
    }

    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(58, 18, "RETURNING TO MAIN");
    display.drawFrame(4, 31, 248, 9);
    display.sendBuffer();
    synth.end();

    const size_t size = firmware.size();
    if (!Update.begin(size, U_FLASH)) {
        firmware.close();
        resumeSynthAfterUpdateFailure();
        setNotice("Update begin failed", 2600);
        return false;
    }

    uint8_t buffer[1024];
    size_t written = 0;
    uint8_t previousPercent = 255;
    while (firmware.available()) {
        const size_t count = firmware.read(buffer, sizeof(buffer));
        if (!count || Update.write(buffer, count) != count) break;
        written += count;
        const uint8_t percent = written * 100ULL / size;
        if (percent == previousPercent) continue;
        previousPercent = percent;
        display.setDrawColor(0);
        display.drawBox(5, 32, 246, 7);
        display.setDrawColor(1);
        display.drawBox(5, 32, percent * 246 / 100, 7);
        display.sendBuffer();
    }
    firmware.close();

    if (written != size || !Update.end(true) || !Update.isFinished()) {
        Update.abort();
        resumeSynthAfterUpdateFailure();
        setNotice("Update failed", 2600);
        return false;
    }

    display.clearBuffer();
    display.drawStr(65, 35, "MAIN RESTORED");
    display.sendBuffer();
    delay(250);
    ESP.restart();
    return true;
}
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

    display.clearBuffer();
    display.setFont(u8g2_font_10x20_tf);
    display.drawStr(41, 29, "MUSIC KEYBOARD");
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(79, 46, "Starting audio...");
    display.sendBuffer();

    sdReady = SD.begin(SD_CS, SPI);
    const bool audioReady = synth.begin();
    if (audioReady) {
        synth.setTone(selectedTone);
        synth.setVolume(volume);
        synth.setSustain(sustain);
    }
    Serial.printf("MusicKeyboard audio=%s sd=%s heap=%u internal=%u psram=%u voices=%u\n",
                  audioReady ? "ok" : "failed", sdReady ? "ok" : "failed",
                  ESP.getFreeHeap(), heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  ESP.getPsramSize(), LiveSynth::MAX_VOICES);
    if (!audioReady) setNotice("I2S audio init failed", 5000);
    else if (!sdReady) setNotice("No SD; recording stays in RAM", 3500);
    else setNotice("Music Keyboard ready", 1400);
    drawPage();
}

void loop() {
    keyboard.update();

    if (keyboard.isDown(KeyboardKey::Escape) &&
        keyboard.heldMs(KeyboardKey::Escape) >= EXIT_HOLD_MS) {
        if (!exitTriggered) {
            exitTriggered = true;
            installMainFirmware();
        }
    } else if (!keyboard.isDown(KeyboardKey::Escape)) {
        exitTriggered = false;
    }

    KeyboardEvent event;
    while (keyboard.nextEvent(event)) handleKeyboardEvent(event);
    if (recordingOverflow && recorder.isRecording()) stopRecording(true);

    servicePlayback();
    serviceMetronome();

    const uint32_t now = millis();
    const uint32_t interval = page == Page::Scope ? 40 : 70;
    if (now - lastDraw >= interval) {
        lastDraw = now;
        drawPage();
    }
    delay(1);
}
