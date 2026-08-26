#include "MidiPlayer.h"
#include "MidiDurationCache.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr uint32_t MIDI_HEADER = 0x4D546864UL;
constexpr uint32_t MIDI_TRACK = 0x4D54726BUL;
constexpr uint64_t TIMING_DENOMINATOR = 1000000ULL;
constexpr uint8_t VOLUME_TABLE[22] = {
    0, 1, 2, 3, 4, 6, 8, 10, 12, 14, 17,
    20, 23, 27, 30, 34, 38, 43, 48, 52, 58, 64
};

uint16_t readBigEndian16(fs::File &file) {
    const int high = file.read();
    const int low = file.read();
    if (high < 0 || low < 0) return 0;
    return static_cast<uint16_t>((high << 8) | low);
}

uint32_t readBigEndian32(fs::File &file) {
    uint32_t value = 0;
    for (uint8_t index = 0; index < 4; ++index) {
        const int byte = file.read();
        if (byte < 0) return 0;
        value = (value << 8) | static_cast<uint8_t>(byte);
    }
    return value;
}

int16_t clampSample(int32_t value) {
    return static_cast<int16_t>(constrain(value, -32768, 32767));
}
}

bool MidiPlayer::open(fs::FS &filesystem, const char *path) {
    stop();
    file_ = filesystem.open(path, FILE_READ);
    if (!file_ || file_.isDirectory()) {
        fail("Cannot open MIDI file");
        return false;
    }

    for (uint16_t index = 0; index < 256; ++index) {
        const float angle = static_cast<float>(index) * 6.28318530718f / 256.0f;
        sineTable_[index] = static_cast<int16_t>(sinf(angle) * 32767.0f);
    }
    resetChannels();
    if (!readHeader()) {
        file_.close();
        return false;
    }

    totalDuration_ = MidiDurationCache::getOrScan(filesystem, path, durationCacheHit_);

    tempoUsPerQuarter_ = 500000;
    currentTick_ = 0;
    renderedFrames_ = 0;
    sampleRemainder_ = 0;
    voiceAge_ = 0;
    stealTailLeft_ = 0;
    stealTailRight_ = 0;
    limiterGainQ15_ = 32767;
    limiterHits_ = 0;
    voiceSteals_ = 0;
    mergedNoteOns_ = 0;
    droppedNoteOns_ = 0;
    sustainVoiceSteals_ = 0;
    heldVoiceSteals_ = 0;
    paused_ = false;
    finished_ = false;
    failed_ = false;
    error_ = "";
    memset(voices_, 0, sizeof(voices_));

    tracksRemain_ = false;
    for (uint8_t index = 0; index < trackCount_; ++index) {
        if (!primeTrack(tracks_[index])) {
            fail("Invalid MIDI track data");
            file_.close();
            return false;
        }
        tracksRemain_ |= !tracks_[index].ended;
    }
    running_ = tracksRemain_;
    if (!running_) {
        fail("MIDI file has no events");
        file_.close();
        return false;
    }
    if (!scheduleNextEvent()) {
        fail("Invalid MIDI event order");
        file_.close();
        return false;
    }
    return true;
}

void MidiPlayer::stop() {
    if (file_) file_.close();
    memset(voices_, 0, sizeof(voices_));
    running_ = false;
    paused_ = false;
    finished_ = false;
    failed_ = false;
    tracksRemain_ = false;
    trackCount_ = 0;
    totalDuration_ = 0;
    durationCacheHit_ = false;
    error_ = "";
}

bool MidiPlayer::readHeader() {
    if (file_.size() < 14 || readBigEndian32(file_) != MIDI_HEADER) {
        fail("Missing MIDI header");
        return false;
    }
    const uint32_t headerLength = readBigEndian32(file_);
    if (headerLength < 6 || file_.position() + headerLength > file_.size()) {
        fail("Invalid MIDI header length");
        return false;
    }
    const uint16_t format = readBigEndian16(file_);
    const uint16_t declaredTracks = readBigEndian16(file_);
    division_ = readBigEndian16(file_);
    if (format > 1 || declaredTracks == 0) {
        fail("Only MIDI format 0/1 is supported");
        return false;
    }
    if (declaredTracks > MAX_TRACKS) {
        fail("MIDI has too many tracks");
        return false;
    }
    if (division_ == 0 || (division_ & 0x8000)) {
        fail("SMPTE MIDI timing is unsupported");
        return false;
    }

    if (!file_.seek(8 + headerLength)) {
        fail("Cannot seek MIDI header");
        return false;
    }
    trackCount_ = 0;
    while (trackCount_ < declaredTracks && file_.position() + 8 <= file_.size()) {
        const uint32_t chunkId = readBigEndian32(file_);
        const uint32_t chunkLength = readBigEndian32(file_);
        const uint32_t chunkStart = file_.position();
        const uint64_t chunkEnd = static_cast<uint64_t>(chunkStart) + chunkLength;
        if (chunkEnd > file_.size()) {
            fail("Truncated MIDI track");
            return false;
        }
        if (chunkId == MIDI_TRACK) {
            TrackState &track = tracks_[trackCount_++];
            track.start = chunkStart;
            track.end = static_cast<uint32_t>(chunkEnd);
        }
        if (!file_.seek(static_cast<uint32_t>(chunkEnd))) {
            fail("Cannot seek MIDI track");
            return false;
        }
    }
    if (trackCount_ != declaredTracks) {
        fail("Missing MIDI track chunk");
        return false;
    }
    return true;
}

bool MidiPlayer::primeTrack(TrackState &track) {
    track.cursor = track.start;
    track.eventPosition = track.start;
    track.nextTick = 0;
    track.runningStatus = 0;
    track.bufferPosition = 0;
    track.bufferLength = 0;
    track.ended = track.start >= track.end;
    if (track.ended) return true;
    uint32_t delta = 0;
    if (!readVariableLength(track, delta)) return false;
    track.nextTick = delta;
    track.eventPosition = track.cursor;
    return track.eventPosition < track.end;
}

bool MidiPlayer::readVariableLength(TrackState &track, uint32_t &value) {
    value = 0;
    for (uint8_t count = 0; count < 4; ++count) {
        const int byte = readTrackByte(track);
        if (byte < 0) return false;
        value = (value << 7) | (byte & 0x7F);
        if (!(byte & 0x80)) return true;
    }
    return false;
}

int MidiPlayer::readTrackByte(TrackState &track) {
    if (track.cursor >= track.end) return -1;
    if (track.bufferPosition >= track.bufferLength) {
        if (!file_.seek(track.cursor)) return -1;
        const uint32_t remaining = track.end - track.cursor;
        const size_t requested = min<uint32_t>(TRACK_BUFFER_SIZE, remaining);
        const int received = file_.read(track.buffer, requested);
        if (received <= 0) return -1;
        track.bufferPosition = 0;
        track.bufferLength = static_cast<uint16_t>(received);
    }
    ++track.cursor;
    return track.buffer[track.bufferPosition++];
}

bool MidiPlayer::skipBytes(TrackState &track, uint32_t count) {
    const uint64_t target = static_cast<uint64_t>(track.cursor) + count;
    if (target > track.end) return false;
    const uint16_t buffered = track.bufferLength - track.bufferPosition;
    if (count <= buffered) {
        track.bufferPosition += count;
    } else {
        track.bufferPosition = 0;
        track.bufferLength = 0;
    }
    track.cursor = static_cast<uint32_t>(target);
    return true;
}

bool MidiPlayer::advanceTrack(TrackState &track) {
    if (track.cursor >= track.end) {
        track.ended = true;
        return true;
    }
    uint32_t delta = 0;
    if (!readVariableLength(track, delta)) return false;
    if (UINT32_MAX - track.nextTick < delta) return false;
    track.nextTick += delta;
    track.eventPosition = track.cursor;
    if (track.eventPosition >= track.end) track.ended = true;
    return true;
}

bool MidiPlayer::dispatchEvent(TrackState &track) {
    if (track.cursor != track.eventPosition) {
        track.cursor = track.eventPosition;
        track.bufferPosition = 0;
        track.bufferLength = 0;
    }
    int first = readTrackByte(track);
    if (first < 0) return false;

    uint8_t status = static_cast<uint8_t>(first);
    int firstData = -1;
    if (status < 0x80) {
        if (track.runningStatus < 0x80 || track.runningStatus >= 0xF0) return false;
        status = track.runningStatus;
        firstData = first;
    } else if (status < 0xF0) {
        track.runningStatus = status;
    } else {
        track.runningStatus = 0;
    }

    if (status < 0xF0) {
        const uint8_t command = status & 0xF0;
        const uint8_t channel = status & 0x0F;
        int data1 = firstData >= 0 ? firstData : readTrackByte(track);
        int data2 = 0;
        if (data1 < 0 || data1 >= 0x80) return false;
        if (command != 0xC0 && command != 0xD0) {
            data2 = readTrackByte(track);
            if (data2 < 0 || data2 >= 0x80) return false;
        }

        switch (command) {
            case 0x80:
                noteOff(channel, data1);
                break;
            case 0x90:
                if (data2) noteOn(channel, data1, data2);
                else noteOff(channel, data1);
                break;
            case 0xB0:
                switch (data1) {
                    case 7:
                        channels_[channel].volume = data2;
                        updateChannelGain(channel);
                        break;
                    case 10:
                        channels_[channel].pan = data2;
                        updateChannelGain(channel);
                        break;
                    case 11:
                        channels_[channel].expression = data2;
                        updateChannelGain(channel);
                        break;
                    case 64: setSustain(channel, data2 >= 64); break;
                    case 120: allNotesOff(channel, true); break;
                    case 121:
                        channels_[channel].volume = 100;
                        channels_[channel].expression = 127;
                        channels_[channel].pan = 64;
                        updateChannelGain(channel);
                        setSustain(channel, false);
                        setPitchBend(channel, 8192);
                        break;
                    case 123: allNotesOff(channel); break;
                    default: break;
                }
                break;
            case 0xC0:
                channels_[channel].program = data1;
                break;
            case 0xE0:
                setPitchBend(channel, static_cast<uint16_t>(data1 | (data2 << 7)));
                break;
            default:
                break;
        }
        return advanceTrack(track);
    }

    if (status == 0xFF) {
        const int type = readTrackByte(track);
        uint32_t length = 0;
        if (type < 0 || !readVariableLength(track, length)) return false;
        if (type == 0x2F) {
            if (!skipBytes(track, length)) return false;
            track.ended = true;
            return true;
        }
        if (type == 0x51 && length == 3) {
            const int a = readTrackByte(track);
            const int b = readTrackByte(track);
            const int c = readTrackByte(track);
            if (a < 0 || b < 0 || c < 0) return false;
            const uint32_t tempo = (static_cast<uint32_t>(a) << 16) |
                                   (static_cast<uint32_t>(b) << 8) |
                                   static_cast<uint32_t>(c);
            if (tempo >= 1000 && tempo <= 60000000) tempoUsPerQuarter_ = tempo;
        } else if (!skipBytes(track, length)) {
            return false;
        }
        return advanceTrack(track);
    }

    if (status == 0xF0 || status == 0xF7) {
        uint32_t length = 0;
        if (!readVariableLength(track, length) || !skipBytes(track, length)) return false;
        return advanceTrack(track);
    }

    uint8_t systemLength = 0;
    if (status == 0xF1 || status == 0xF3) systemLength = 1;
    else if (status == 0xF2) systemLength = 2;
    if (!skipBytes(track, systemLength)) return false;
    return advanceTrack(track);
}

bool MidiPlayer::scheduleNextEvent() {
    tracksRemain_ = false;
    uint32_t soonest = UINT32_MAX;
    for (uint8_t index = 0; index < trackCount_; ++index) {
        if (tracks_[index].ended) continue;
        tracksRemain_ = true;
        soonest = min(soonest, tracks_[index].nextTick);
    }
    if (!tracksRemain_) {
        for (uint8_t channel = 0; channel < CHANNEL_COUNT; ++channel) allNotesOff(channel);
        framesUntilEvent_ = UINT64_MAX;
        return true;
    }
    if (soonest < currentTick_) return false;
    nextEventTick_ = soonest;
    const uint64_t deltaTicks = static_cast<uint64_t>(soonest - currentTick_);
    const uint64_t denominator = static_cast<uint64_t>(division_) * TIMING_DENOMINATOR;
    const uint64_t numerator = deltaTicks * tempoUsPerQuarter_ * SAMPLE_RATE + sampleRemainder_;
    framesUntilEvent_ = numerator / denominator;
    sampleRemainder_ = numerator % denominator;
    return true;
}

bool MidiPlayer::processDueEvents() {
    uint16_t guard = 0;
    while (tracksRemain_ && framesUntilEvent_ == 0) {
        currentTick_ = nextEventTick_;
        for (uint8_t index = 0; index < trackCount_; ++index) {
            TrackState &track = tracks_[index];
            while (!track.ended && track.nextTick == currentTick_) {
                if (!dispatchEvent(track)) {
                    fail("Corrupt MIDI event");
                    return false;
                }
                if (++guard > 1024) {
                    fail("Too many simultaneous MIDI events");
                    return false;
                }
            }
        }
        if (!scheduleNextEvent()) {
            fail("Invalid MIDI event order");
            return false;
        }
    }
    return true;
}

size_t MidiPlayer::render(uint32_t *frames, size_t frameCount) {
    if (!running_ || paused_ || !frames || !frameCount) return 0;
    size_t written = 0;
    while (written < frameCount && running_) {
        if (!processDueEvents()) break;
        if (!tracksRemain_ && activeVoices() == 0) {
            running_ = false;
            finished_ = !failed_;
            break;
        }
        frames[written++] = renderFrame();
        ++renderedFrames_;
        if (tracksRemain_ && framesUntilEvent_ != UINT64_MAX && framesUntilEvent_ > 0) {
            --framesUntilEvent_;
        }
    }
    return written;
}

void MidiPlayer::resetChannels() {
    for (uint8_t index = 0; index < CHANNEL_COUNT; ++index) {
        channels_[index] = ChannelState{};
        updateChannelGain(index);
    }
}

void MidiPlayer::updateChannelGain(uint8_t channel) {
    ChannelState &state = channels_[channel];
    const uint8_t leftPan = min<uint16_t>(127, static_cast<uint16_t>(127 - state.pan) * 2);
    const uint8_t rightPan = min<uint16_t>(127, static_cast<uint16_t>(state.pan) * 2);
    constexpr uint32_t MIDI_GAIN_DENOMINATOR = 127UL * 127UL * 127UL;
    const uint32_t channelGain = static_cast<uint32_t>(state.volume) * state.expression;
    state.leftGainQ15 = static_cast<uint16_t>(
        (static_cast<uint64_t>(channelGain) * leftPan * 32767 + MIDI_GAIN_DENOMINATOR / 2) /
        MIDI_GAIN_DENOMINATOR);
    state.rightGainQ15 = static_cast<uint16_t>(
        (static_cast<uint64_t>(channelGain) * rightPan * 32767 + MIDI_GAIN_DENOMINATOR / 2) /
        MIDI_GAIN_DENOMINATOR);
    for (Voice &voice : voices_) {
        if (!voice.active || voice.channel != channel) continue;
        voice.baseLeftGainQ15 = static_cast<uint16_t>(
            static_cast<uint32_t>(state.leftGainQ15) * voice.velocity >> 7);
        voice.baseRightGainQ15 = static_cast<uint16_t>(
            static_cast<uint32_t>(state.rightGainQ15) * voice.velocity >> 7);
        updateVoiceGain(voice);
    }
}

MidiPlayer::Patch MidiPlayer::patchFor(uint8_t channel, uint8_t note) const {
    if (channel == 9) {
        if (note == 35 || note == 36) return {Waveform::Sine, 1, 180, 40, 0, true};
        if (note == 38 || note == 39 || note == 40) return {Waveform::Noise, 1, 140, 35, 0, true};
        if (note >= 42 && note <= 59) {
            const uint16_t decay = note < 47 ? 55 : 180;
            return {Waveform::Noise, 1, decay, 25, 0, true};
        }
        return {Waveform::Triangle, 1, 220, 40, 0, true};
    }

    const uint16_t pianoDecay = static_cast<uint16_t>(constrain(1900 - note * 11, 650, 1550));
    const uint16_t pianoRelease = static_cast<uint16_t>(constrain(760 - note * 4, 300, 620));
    switch (toneProfile_) {
        case ToneProfile::Piano:
            return {Waveform::Piano, 1, pianoDecay, pianoRelease, 2200, false};
        case ToneProfile::Mellow:
            return {Waveform::Sine, 2, 1050, 520, 4200, false};
        case ToneProfile::Synth:
            return {Waveform::Mixed, 5, 480, 300, 12000, false};
        case ToneProfile::GeneralMidi:
            break;
    }

    switch (channels_[channel].program >> 3) {
        case 0: return {Waveform::Piano, 1, pianoDecay, pianoRelease, 2200, false};
        case 1: return {Waveform::Sine, 2, 650, 180, 3000, false};
        case 2: return {Waveform::Organ, 8, 180, 160, 28000, false};
        case 3: return {Waveform::Triangle, 2, 520, 280, 4500, false};
        case 4: return {Waveform::Mixed, 3, 420, 180, 12500, false};
        case 5: return {Waveform::Saw, 90, 500, 650, 23500, false};
        case 6: return {Waveform::Triangle, 180, 700, 800, 25000, false};
        case 7: return {Waveform::Mixed, 25, 380, 260, 22000, false};
        case 8: return {Waveform::Square, 18, 360, 220, 20500, false};
        case 9: return {Waveform::Sine, 35, 450, 300, 24500, false};
        case 10: return {Waveform::Saw, 4, 260, 160, 21000, false};
        case 11: return {Waveform::Triangle, 420, 900, 1200, 25500, false};
        case 12: return {Waveform::Sine, 12, 850, 700, 13000, false};
        case 13: return {Waveform::Triangle, 3, 700, 260, 4000, false};
        case 14: return {Waveform::Noise, 2, 300, 120, 2000, false};
        default: return {Waveform::Mixed, 5, 480, 300, 12000, false};
    }
}

MidiPlayer::Voice *MidiPlayer::allocateVoice(uint8_t channel, uint8_t velocity) {
    for (uint8_t index = 0; index < voiceLimit_; ++index) {
        Voice &voice = voices_[index];
        if (!voice.active) return &voice;
        if (voice.stage == EnvelopeStage::Release && voice.envelope <= 128) {
            voice.active = false;
            return &voice;
        }
    }
    Voice *candidate = nullptr;
    uint32_t candidateLevel = UINT32_MAX;
    uint8_t candidateClass = UINT8_MAX;
    for (uint8_t index = 0; index < voiceLimit_; ++index) {
        Voice &voice = voices_[index];
        uint8_t voiceClass = 3;
        if (voice.stage == EnvelopeStage::Release) voiceClass = 0;
        else if (!voice.held && !voice.sustained) voiceClass = 1;
        else if (!voice.held) voiceClass = 2;
        const uint32_t level = voiceClass == 3
            ? (static_cast<uint32_t>(voice.velocity) + 1) << 15
            : static_cast<uint32_t>(max<int32_t>(0, voice.envelope)) *
              (static_cast<uint16_t>(voice.velocity) + 1);
        if (!candidate || voiceClass < candidateClass ||
            (voiceClass == candidateClass && (level < candidateLevel ||
             (level == candidateLevel && voice.age < candidate->age)))) {
            candidate = &voice;
            candidateLevel = level;
            candidateClass = voiceClass;
        }
    }
    if (candidate) {
        if (candidateClass == 3) {
            constexpr uint8_t HELD_STEAL_MARGIN = 12;
            const bool percussionWouldStealMelody = channel == 9;
            const bool incomingClearlyStronger =
                static_cast<uint16_t>(velocity) >= static_cast<uint16_t>(candidate->velocity) + HELD_STEAL_MARGIN;
            if (percussionWouldStealMelody || !incomingClearlyStronger) {
                ++droppedNoteOns_;
                return nullptr;
            }
        }
        stealTailLeft_ = constrain(stealTailLeft_ + candidate->lastLeft, -131072, 131072);
        stealTailRight_ = constrain(stealTailRight_ + candidate->lastRight, -131072, 131072);
        ++voiceSteals_;
        if (candidate->held) ++heldVoiceSteals_;
        else if (candidate->sustained) ++sustainVoiceSteals_;
    }
    return candidate;
}

void MidiPlayer::noteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (channel >= CHANNEL_COUNT || note >= 128 || velocity == 0) return;
    for (Voice &existing : voices_) {
        if (!existing.active || !existing.held || existing.oneShot ||
            existing.channel != channel || existing.note != note) continue;
        if (existing.overlapCount < UINT8_MAX) ++existing.overlapCount;
        existing.velocity = max(existing.velocity, velocity);
        existing.age = ++voiceAge_;
        const ChannelState &state = channels_[channel];
        existing.baseLeftGainQ15 = static_cast<uint16_t>(
            static_cast<uint32_t>(state.leftGainQ15) * existing.velocity >> 7);
        existing.baseRightGainQ15 = static_cast<uint16_t>(
            static_cast<uint32_t>(state.rightGainQ15) * existing.velocity >> 7);
        updateVoiceGain(existing);
        ++mergedNoteOns_;
        return;
    }
    Voice *voice = allocateVoice(channel, velocity);
    if (!voice) return;
    const Patch patch = patchFor(channel, note);
    *voice = Voice{};
    voice->active = true;
    voice->held = !patch.oneShot;
    voice->oneShot = patch.oneShot;
    voice->channel = channel;
    voice->note = note;
    voice->velocity = velocity;
    voice->overlapCount = 1;
    voice->waveform = patch.waveform;
    voice->sustainLevel = patch.sustainLevel;
    voice->decayMs = patch.decayMs;
    voice->releaseMs = patch.releaseMs;
    voice->age = ++voiceAge_;
    voice->noise = 0x9E3779B9UL ^ (static_cast<uint32_t>(note) << 16) ^ voiceAge_;
    updateVoicePitch(*voice);
    configurePianoTimbre(*voice, true);

    const uint32_t attackSteps = max<uint32_t>(1, patch.attackMs * SAMPLE_RATE / 8000);
    voice->envelope = patch.attackMs <= 1 ? 32767 : 0;
    voice->envelopeStep = max<int32_t>(1, 32767 / static_cast<int32_t>(attackSteps));
    voice->stage = patch.attackMs <= 1 ? EnvelopeStage::Decay : EnvelopeStage::Attack;
    if (voice->stage == EnvelopeStage::Decay) {
        const uint32_t decaySteps = max<uint32_t>(1, patch.decayMs * SAMPLE_RATE / 8000);
        voice->envelopeStep = max<int32_t>(1, (32767 - patch.sustainLevel) / static_cast<int32_t>(decaySteps));
    }
    const ChannelState &state = channels_[channel];
    voice->baseLeftGainQ15 = static_cast<uint16_t>(
        static_cast<uint32_t>(state.leftGainQ15) * velocity >> 7);
    voice->baseRightGainQ15 = static_cast<uint16_t>(
        static_cast<uint32_t>(state.rightGainQ15) * velocity >> 7);
    updateVoiceGain(*voice);
}

void MidiPlayer::noteOff(uint8_t channel, uint8_t note) {
    Voice *oldestHeld = nullptr;
    for (Voice &voice : voices_) {
        if (!voice.active || !voice.held || voice.channel != channel ||
            voice.note != note || voice.oneShot) continue;
        if (!oldestHeld || voice.age < oldestHeld->age) oldestHeld = &voice;
    }
    if (!oldestHeld) return;
    if (oldestHeld->overlapCount > 1) {
        --oldestHeld->overlapCount;
        return;
    }
    oldestHeld->held = false;
    if (channels_[channel].sustain) oldestHeld->sustained = true;
    else beginRelease(*oldestHeld);
}

void MidiPlayer::allNotesOff(uint8_t channel, bool immediate) {
    for (Voice &voice : voices_) {
        if (!voice.active || voice.channel != channel) continue;
        voice.held = false;
        voice.sustained = false;
        if (immediate) voice.active = false;
        else beginRelease(voice);
    }
}

void MidiPlayer::setSustain(uint8_t channel, bool enabled) {
    if (channel >= CHANNEL_COUNT) return;
    channels_[channel].sustain = enabled;
    if (enabled) return;
    for (Voice &voice : voices_) {
        if (voice.active && voice.channel == channel && voice.sustained && !voice.held) {
            voice.sustained = false;
            beginRelease(voice);
        }
    }
}

void MidiPlayer::setPitchBend(uint8_t channel, uint16_t bend) {
    if (channel >= CHANNEL_COUNT) return;
    channels_[channel].pitchBend = min<uint16_t>(bend, 16383);
    for (Voice &voice : voices_) {
        if (voice.active && voice.channel == channel) updateVoicePitch(voice);
    }
}

void MidiPlayer::updateVoicePitch(Voice &voice) {
    int16_t note = static_cast<int16_t>(voice.note) + transpose_;
    if (voice.channel == 9) {
        if (voice.note == 35 || voice.note == 36) note = 43;
        else if (voice.waveform == Waveform::Noise) note = 84;
    }
    const float bend = (static_cast<int32_t>(channels_[voice.channel].pitchBend) - 8192) / 4096.0f;
    const float frequency = 440.0f * powf(2.0f, (static_cast<float>(note - 69) + bend) / 12.0f);
    const double increment = frequency * 4294967296.0 / SAMPLE_RATE;
    voice.phaseIncrement = static_cast<uint32_t>(constrain(increment, 1.0, 4294967295.0));
    voice.renderWaveform = voice.waveform;
    if (voice.phaseIncrement > 600000000UL &&
        (voice.renderWaveform == Waveform::Saw || voice.renderWaveform == Waveform::Square ||
         voice.renderWaveform == Waveform::Mixed || voice.renderWaveform == Waveform::Organ ||
         voice.renderWaveform == Waveform::Piano)) {
        voice.renderWaveform = Waveform::Sine;
    } else if (voice.phaseIncrement > 300000000UL &&
               (voice.renderWaveform == Waveform::Saw || voice.renderWaveform == Waveform::Square ||
                voice.renderWaveform == Waveform::Mixed)) {
        voice.renderWaveform = Waveform::Triangle;
    }
}

void MidiPlayer::configurePianoTimbre(Voice &voice, bool strike) {
    if (voice.waveform != Waveform::Piano) {
        voice.brightnessEnvelope = 0;
        voice.brightnessStep = 0;
        voice.hammerEnvelope = 0;
        voice.hammerStep = 0;
        return;
    }

    voice.brightnessEnvelope = static_cast<uint16_t>(
        min<uint32_t>(32767, 11000UL + static_cast<uint32_t>(voice.velocity) * 150));
    const uint16_t brightnessMs = static_cast<uint16_t>(constrain(300 - voice.note * 2, 90, 230));
    const uint32_t brightnessSteps = max<uint32_t>(1, brightnessMs * SAMPLE_RATE / 8000);
    voice.brightnessStep = static_cast<uint16_t>(
        max<uint32_t>(1, voice.brightnessEnvelope / brightnessSteps));

    if (!strike) {
        voice.hammerEnvelope = 0;
        voice.hammerStep = 0;
        return;
    }
    voice.hammerEnvelope = static_cast<uint16_t>(
        min<uint32_t>(24576, 6500UL + static_cast<uint32_t>(voice.velocity) * 120));
    const uint32_t hammerSamples = max<uint32_t>(1, (4 + voice.velocity / 18) * SAMPLE_RATE / 1000);
    voice.hammerStep = static_cast<uint16_t>(
        max<uint32_t>(1, voice.hammerEnvelope / hammerSamples));
}

void MidiPlayer::updateVoiceGain(Voice &voice) {
    const uint16_t envelope = static_cast<uint16_t>(max<int32_t>(0, voice.envelope));
    voice.leftGainQ15 = static_cast<uint16_t>(
        static_cast<uint32_t>(voice.baseLeftGainQ15) * envelope >> 15);
    voice.rightGainQ15 = static_cast<uint16_t>(
        static_cast<uint32_t>(voice.baseRightGainQ15) * envelope >> 15);
}

void MidiPlayer::beginRelease(Voice &voice) {
    if (!voice.active || voice.stage == EnvelopeStage::Release) return;
    voice.stage = EnvelopeStage::Release;
    const uint32_t releaseSteps = max<uint32_t>(1, voice.releaseMs * SAMPLE_RATE / 8000);
    voice.envelopeStep = max<int32_t>(1, voice.envelope / static_cast<int32_t>(releaseSteps));
}

int16_t MidiPlayer::oscillator(Voice &voice) {
    const uint8_t index = voice.phase >> 24;
    int32_t sample = 0;
    switch (voice.renderWaveform) {
        case Waveform::Sine:
            sample = sineTable_[index];
            break;
        case Waveform::Triangle: {
            const uint16_t phase = voice.phase >> 16;
            sample = phase < 32768 ? -32768 + static_cast<int32_t>(phase) * 2
                                   : 98303 - static_cast<int32_t>(phase) * 2;
            break;
        }
        case Waveform::Saw:
            sample = static_cast<int16_t>(voice.phase >> 16);
            break;
        case Waveform::Square:
            sample = (voice.phase & 0x80000000UL) ? 27000 : -27000;
            break;
        case Waveform::Organ:
            sample = ((static_cast<int32_t>(sineTable_[index]) * 4 +
                       static_cast<int32_t>(sineTable_[static_cast<uint8_t>(index * 2)]) * 2 +
                       sineTable_[static_cast<uint8_t>(index * 3)]) * 9362) >> 16;
            break;
        case Waveform::Piano:
            sample = static_cast<int32_t>(sineTable_[index]) * 5;
            if (voice.brightnessEnvelope) {
                const int32_t upper =
                    static_cast<int32_t>(sineTable_[static_cast<uint8_t>(index * 2)]) * 2 +
                    sineTable_[static_cast<uint8_t>(index * 3)];
                if (voice.brightnessEnvelope > 24576) sample = (sample + upper) >> 3;
                else if (voice.brightnessEnvelope > 16384) sample = (sample * 2 + upper) >> 4;
                else if (voice.brightnessEnvelope > 8192) sample = (sample * 4 + upper) >> 5;
                else sample = (sample * 8 + upper) >> 6;
            } else {
                sample >>= 3;
            }
            break;
        case Waveform::Mixed:
            sample = (static_cast<int32_t>(static_cast<int16_t>(voice.phase >> 16)) +
                      ((voice.phase & 0x80000000UL) ? 22000 : -22000)) / 2;
            break;
        case Waveform::Noise:
            voice.noise ^= voice.noise << 13;
            voice.noise ^= voice.noise >> 17;
            voice.noise ^= voice.noise << 5;
            sample = static_cast<int16_t>(voice.noise >> 16);
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

void MidiPlayer::advanceEnvelope(Voice &voice) {
    if (!voice.active) return;
    if (++voice.envelopeClock < 8) return;
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
                const uint32_t steps = max<uint32_t>(1, voice.decayMs * SAMPLE_RATE / 8000);
                voice.envelopeStep = max<int32_t>(1, (32767 - voice.sustainLevel) / static_cast<int32_t>(steps));
            }
            break;
        case EnvelopeStage::Decay:
            voice.envelope -= voice.envelopeStep;
            if (voice.envelope <= voice.sustainLevel) {
                voice.envelope = voice.sustainLevel;
                if (voice.oneShot || voice.sustainLevel == 0) beginRelease(voice);
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
    if (voice.active) updateVoiceGain(voice);
}

uint32_t MidiPlayer::renderFrame() {
    int32_t left = 0;
    int32_t right = 0;
    for (Voice &voice : voices_) {
        if (!voice.active) continue;
        int32_t sample = oscillator(voice);
        voice.lastLeft = sample * static_cast<int32_t>(voice.leftGainQ15) >> 15;
        voice.lastRight = sample * static_cast<int32_t>(voice.rightGainQ15) >> 15;
        left += voice.lastLeft;
        right += voice.lastRight;
        advanceEnvelope(voice);
    }

    left += stealTailLeft_;
    right += stealTailRight_;
    stealTailLeft_ = stealTailLeft_ * 63 / 64;
    stealTailRight_ = stealTailRight_ * 63 / 64;
    if (abs(stealTailLeft_) < 4) stealTailLeft_ = 0;
    if (abs(stealTailRight_) < 4) stealTailRight_ = 0;

    const uint8_t gain = VOLUME_TABLE[min<uint8_t>(masterVolume_, 21)];
    left = left * gain >> 8;
    right = right * gain >> 8;

    constexpr int32_t LIMIT_LEVEL = 28000;
    const uint32_t peak = max<uint32_t>(abs(left), abs(right));
    uint16_t targetGain = 32767;
    if (peak > LIMIT_LEVEL) {
        targetGain = static_cast<uint16_t>((static_cast<uint64_t>(LIMIT_LEVEL) * 32767) / peak);
        ++limiterHits_;
    }
    if (targetGain < limiterGainQ15_) {
        limiterGainQ15_ = targetGain;
    } else if (limiterGainQ15_ < 32767) {
        limiterGainQ15_ += max<uint16_t>(1, (32767 - limiterGainQ15_) >> 12);
    }
    if (limiterGainQ15_ < 32767) {
        left = static_cast<int32_t>((static_cast<int64_t>(left) * limiterGainQ15_) >> 15);
        right = static_cast<int32_t>((static_cast<int64_t>(right) * limiterGainQ15_) >> 15);
    }
    const uint16_t packedLeft = static_cast<uint16_t>(clampSample(left));
    const uint16_t packedRight = static_cast<uint16_t>(clampSample(right));
    return (static_cast<uint32_t>(packedLeft) << 16) | packedRight;
}

void MidiPlayer::setPaused(bool paused) {
    paused_ = paused && running_;
}

void MidiPlayer::setVolume(uint8_t volume) {
    masterVolume_ = min<uint8_t>(volume, 21);
}

void MidiPlayer::setVoiceLimit(uint8_t limit) {
    limit = constrain(limit, static_cast<uint8_t>(1), MAX_VOICES);
    if (limit < voiceLimit_) {
        for (uint8_t index = limit; index < MAX_VOICES; ++index) voices_[index].active = false;
    }
    voiceLimit_ = limit;
}

void MidiPlayer::changeTranspose(int8_t delta) {
    const int16_t changed = static_cast<int16_t>(transpose_) + delta;
    transpose_ = static_cast<int8_t>(max<int16_t>(-24, min<int16_t>(24, changed)));
    for (Voice &voice : voices_) if (voice.active) updateVoicePitch(voice);
}

void MidiPlayer::cycleToneProfile() {
    const uint8_t next = (static_cast<uint8_t>(toneProfile_) + 1) % 4;
    toneProfile_ = static_cast<ToneProfile>(next);
    for (Voice &voice : voices_) {
        if (!voice.active || voice.channel == 9) continue;
        const Patch patch = patchFor(voice.channel, voice.note);
        voice.waveform = patch.waveform;
        voice.sustainLevel = patch.sustainLevel;
        voice.decayMs = patch.decayMs;
        voice.releaseMs = patch.releaseMs;
        updateVoicePitch(voice);
        configurePianoTimbre(voice, false);
    }
}

void MidiPlayer::fail(const char *message) {
    error_ = message;
    failed_ = true;
    running_ = false;
    finished_ = false;
}

bool MidiPlayer::isRunning() const { return running_; }
bool MidiPlayer::isPaused() const { return paused_; }
bool MidiPlayer::isFinished() const { return finished_; }
bool MidiPlayer::hasError() const { return failed_; }
const char *MidiPlayer::errorText() const { return error_; }
uint32_t MidiPlayer::currentTime() const { return renderedFrames_ / SAMPLE_RATE; }
uint32_t MidiPlayer::totalDuration() const { return totalDuration_; }
bool MidiPlayer::durationCacheHit() const { return durationCacheHit_; }
uint16_t MidiPlayer::tempoBpm() const { return tempoUsPerQuarter_ ? 60000000UL / tempoUsPerQuarter_ : 0; }
int8_t MidiPlayer::transpose() const { return transpose_; }

const char *MidiPlayer::toneProfileName() const {
    switch (toneProfile_) {
        case ToneProfile::GeneralMidi: return "GM";
        case ToneProfile::Piano: return "PIANO";
        case ToneProfile::Mellow: return "MELLOW";
        case ToneProfile::Synth: return "SYNTH";
    }
    return "GM";
}

uint32_t MidiPlayer::takeLimiterHits() {
    const uint32_t result = limiterHits_;
    limiterHits_ = 0;
    return result;
}

uint32_t MidiPlayer::takeVoiceSteals() {
    const uint32_t result = voiceSteals_;
    voiceSteals_ = 0;
    return result;
}

uint32_t MidiPlayer::takeMergedNoteOns() {
    const uint32_t result = mergedNoteOns_;
    mergedNoteOns_ = 0;
    return result;
}

uint32_t MidiPlayer::takeDroppedNoteOns() {
    const uint32_t result = droppedNoteOns_;
    droppedNoteOns_ = 0;
    return result;
}

uint32_t MidiPlayer::takeSustainVoiceSteals() {
    const uint32_t result = sustainVoiceSteals_;
    sustainVoiceSteals_ = 0;
    return result;
}

uint32_t MidiPlayer::takeHeldVoiceSteals() {
    const uint32_t result = heldVoiceSteals_;
    heldVoiceSteals_ = 0;
    return result;
}

uint8_t MidiPlayer::activeVoices() const {
    uint8_t count = 0;
    for (const Voice &voice : voices_) count += voice.active;
    return count;
}
