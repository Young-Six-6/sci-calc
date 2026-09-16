(function exposeMidiParser(root, factory) {
  const api = factory();
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  if (root) root.FallingKeysMidi = api;
})(typeof globalThis === 'undefined' ? this : globalThis, function midiFactory() {
  const textDecoder = new TextDecoder('utf-8');

  function ascii(data, start, length) {
    return String.fromCharCode(...data.subarray(start, start + length));
  }

  function vlq(data, cursor, end) {
    let value = 0;
    let count = 0;
    while (cursor < end && count < 4) {
      const byte = data[cursor++];
      value = (value << 7) | (byte & 0x7f);
      count += 1;
      if (!(byte & 0x80)) return { value, cursor };
    }
    throw new Error('Invalid MIDI variable-length value');
  }

  function decodeText(data) {
    return textDecoder.decode(data).replace(/[\u0000-\u001f]+/g, ' ').trim();
  }

  function parseTrack(data, start, end, trackIndex) {
    let cursor = start;
    let tick = 0;
    let runningStatus = 0;
    let name = '';
    const notes = [];
    const tempos = [];
    const active = new Map();

    while (cursor < end) {
      const delta = vlq(data, cursor, end);
      tick += delta.value;
      cursor = delta.cursor;
      if (cursor >= end) break;
      let status = data[cursor++];
      if (status < 0x80) {
        if (!runningStatus) throw new Error('MIDI data uses running status without a prior event');
        cursor -= 1;
        status = runningStatus;
      } else if (status < 0xf0) {
        runningStatus = status;
      } else {
        runningStatus = 0;
      }

      if (status === 0xff) {
        if (cursor >= end) throw new Error('Truncated MIDI meta event');
        const type = data[cursor++];
        const size = vlq(data, cursor, end);
        cursor = size.cursor;
        if (cursor + size.value > end) throw new Error('Truncated MIDI meta payload');
        if (type === 0x03 && !name) name = decodeText(data.subarray(cursor, cursor + size.value));
        if (type === 0x51 && size.value === 3) {
          tempos.push({ tick, microsecondsPerBeat: (data[cursor] << 16) | (data[cursor + 1] << 8) | data[cursor + 2] });
        }
        cursor += size.value;
        if (type === 0x2f) break;
        continue;
      }
      if (status === 0xf0 || status === 0xf7) {
        const size = vlq(data, cursor, end);
        cursor = size.cursor + size.value;
        if (cursor > end) throw new Error('Truncated MIDI system-exclusive event');
        continue;
      }

      const command = status & 0xf0;
      const channel = status & 0x0f;
      if (cursor >= end) throw new Error('Truncated MIDI channel event');
      const first = data[cursor++];
      const second = command === 0xc0 || command === 0xd0 ? 0 : (cursor < end ? data[cursor++] : 0);
      if (command === 0x90 && second > 0) {
        const key = channel * 128 + first;
        const stack = active.get(key) || [];
        stack.push({ startTick: tick, pitch: first, velocity: second, channel, track: trackIndex });
        active.set(key, stack);
      } else if (command === 0x80 || (command === 0x90 && second === 0)) {
        const key = channel * 128 + first;
        const stack = active.get(key);
        if (stack && stack.length) {
          const note = stack.pop();
          notes.push({ ...note, endTick: Math.max(tick, note.startTick + 1) });
        }
      }
    }
    return { name, notes, tempos };
  }

  function buildTimeConverter(tempos, division) {
    const merged = [{ tick: 0, microsecondsPerBeat: 500000 }];
    for (const tempo of tempos.sort((a, b) => a.tick - b.tick)) {
      const previous = merged[merged.length - 1];
      if (previous.tick === tempo.tick) previous.microsecondsPerBeat = tempo.microsecondsPerBeat;
      else merged.push(tempo);
    }
    const segments = [];
    let seconds = 0;
    for (let index = 0; index < merged.length; index += 1) {
      const current = merged[index];
      if (index > 0) {
        const previous = merged[index - 1];
        seconds += ((current.tick - previous.tick) * previous.microsecondsPerBeat) / division / 1000000;
      }
      segments.push({ tick: current.tick, seconds, microsecondsPerBeat: current.microsecondsPerBeat });
    }
    return {
      segments,
      tickToSeconds(tick) {
        let segment = segments[0];
        for (const candidate of segments) {
          if (candidate.tick > tick) break;
          segment = candidate;
        }
        return segment.seconds + ((tick - segment.tick) * segment.microsecondsPerBeat) / division / 1000000;
      },
    };
  }

  function selectMelody(notes) {
    const groups = new Map();
    for (const note of notes) {
      const group = groups.get(note.startTick) || [];
      group.push(note);
      groups.set(note.startTick, group);
    }
    const melody = [];
    const accompaniment = [];
    for (const group of groups.values()) {
      group.sort((a, b) => b.pitch - a.pitch || a.track - b.track || a.channel - b.channel);
      melody.push(group[0]);
      accompaniment.push(...group.slice(1));
    }
    melody.sort((a, b) => a.startTick - b.startTick || a.pitch - b.pitch);
    accompaniment.sort((a, b) => a.startTick - b.startTick || a.pitch - b.pitch);
    return { melody, accompaniment };
  }

  function laneNotes(notes, converter, laneCount) {
    if (!notes.length) return [];
    const lowest = Math.min(...notes.map((note) => note.pitch));
    const highest = Math.max(...notes.map((note) => note.pitch));
    const range = Math.max(1, highest - lowest);
    return notes.map((note, index) => {
      const start = converter.tickToSeconds(note.startTick) * 1000;
      const end = converter.tickToSeconds(note.endTick) * 1000;
      const pitchLane = Math.round(((note.pitch - lowest) / range) * (laneCount - 1));
      const contour = index > 0 && note.pitch === notes[index - 1].pitch ? (index % 2) : 0;
      return {
        id: index,
        time: Math.max(0, start),
        lane: Math.max(0, Math.min(laneCount - 1, pitchLane + contour)),
        note: note.pitch,
        duration: Math.max(0, end - start),
      };
    });
  }

  function parseMIDI(input, options = {}) {
    const bytes = input instanceof Uint8Array ? input : new Uint8Array(input);
    if (bytes.length < 14 || ascii(bytes, 0, 4) !== 'MThd') throw new Error('Not a standard MIDI file');
    const headerLength = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength).getUint32(4);
    if (headerLength < 6 || 8 + headerLength > bytes.length) throw new Error('Invalid MIDI header');
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    const format = view.getUint16(8); const trackCount = view.getUint16(10); const division = view.getUint16(12);
    if (format > 2 || trackCount < 1) throw new Error('Unsupported MIDI format');
    if (division & 0x8000) throw new Error('SMPTE-timed MIDI is not supported');
    const tracks = []; const allTempos = []; let cursor = 8 + headerLength;
    for (let trackIndex = 0; trackIndex < trackCount; trackIndex += 1) {
      if (cursor + 8 > bytes.length || ascii(bytes, cursor, 4) !== 'MTrk') throw new Error('Invalid MIDI track chunk');
      const length = view.getUint32(cursor + 4); const trackStart = cursor + 8; const trackEnd = trackStart + length;
      if (trackEnd > bytes.length) throw new Error('Truncated MIDI track');
      const track = parseTrack(bytes, trackStart, trackEnd, trackIndex); tracks.push(track); allTempos.push(...track.tempos); cursor = trackEnd;
    }
    const notes = tracks.flatMap((track) => track.notes);
    if (!notes.length) throw new Error('MIDI file has no playable notes');
    const converter = buildTimeConverter(allTempos, division);
    const selected = selectMelody(notes);
    const laneCount = Math.max(4, Math.min(6, Math.trunc(options.lanes || 4)));
    const melody = laneNotes(selected.melody, converter, laneCount);
    const accompaniment = selected.accompaniment.map((note, index) => ({
      id: index,
      time: Math.max(0, converter.tickToSeconds(note.startTick) * 1000),
      note: note.pitch,
      duration: Math.max(0, (converter.tickToSeconds(note.endTick) - converter.tickToSeconds(note.startTick)) * 1000),
      velocity: note.velocity,
    }));
    const duration = notes.reduce((latest, note) => Math.max(latest, converter.tickToSeconds(note.endTick) * 1000), 0);
    const firstTempo = converter.segments[0].microsecondsPerBeat;
    const fallbackTitle = (options.source || 'MIDI chart').replace(/\\/g, '/').split('/').pop().replace(/\.(mid|midi)$/i, '') || 'MIDI chart';
    const namedTrack = tracks.find((track) => track.name);
    return {
      title: options.title || namedTrack?.name || fallbackTitle,
      artist: options.artist || '',
      lanes: laneCount,
      bpm: Math.round(60000000 / firstTempo),
      offset: 0,
      events: melody,
      accompaniment,
      duration,
      id: options.id || `midi:${fallbackTitle}:${bytes.length}:${notes.length}`,
      source: options.source || 'midi',
      format: 'midi',
      flexibleLanes: true,
      stats: { tracks: tracks.length, totalNotes: notes.length, melodyNotes: melody.length, accompanimentNotes: accompaniment.length },
    };
  }

  return { parseMIDI };
});
