(function attachMidiWriter(root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.MusicMidiWriter = factory();
}(typeof self !== 'undefined' ? self : this, function createMidiWriter() {
  const PPQN = 480;

  function pushBe16(target, value) {
    target.push((value >>> 8) & 0xff, value & 0xff);
  }

  function pushBe32(target, value) {
    target.push((value >>> 24) & 0xff, (value >>> 16) & 0xff, (value >>> 8) & 0xff, value & 0xff);
  }

  function vlq(value) {
    let buffer = value & 0x7f;
    const bytes = [];
    while ((value >>>= 7)) {
      buffer <<= 8;
      buffer |= ((value & 0x7f) | 0x80);
    }
    while (true) {
      bytes.push(buffer & 0xff);
      if (buffer & 0x80) buffer >>>= 8;
      else break;
    }
    return bytes;
  }

  function tempoBytes(bpm) {
    const safeBpm = Math.max(1, Number(bpm) || 120);
    const mpqn = Math.round(60000000 / safeBpm);
    return [(mpqn >>> 16) & 0xff, (mpqn >>> 8) & 0xff, mpqn & 0xff];
  }

  function encodeTrack(take) {
    const bytes = [];
    const initialBpm = Number(take.initialBpm) || 120;
    const initialProgram = Number(take.initialProgram) || 0;
    bytes.push(0x00, 0xff, 0x51, 0x03, ...tempoBytes(initialBpm));
    bytes.push(0x00, 0xc0, initialProgram & 0x7f);

    const events = [...(take.events || [])]
      .map((event, index) => ({ ...event, _index: index }))
      .sort((a, b) => (a.timeMs - b.timeMs) || (a._index - b._index));
    let previousMs = 0;
    let remainder = 0;
    let currentBpm = initialBpm;
    let durationTicks = 0;
    for (const event of events) {
      const ms = Math.max(0, Number(event.timeMs) || 0);
      const deltaMs = Math.max(0, ms - previousMs);
      const scaled = remainder + deltaMs * currentBpm * PPQN;
      const ticks = Math.floor(scaled / 60000);
      remainder = scaled % 60000;
      durationTicks += ticks;
      bytes.push(...vlq(ticks));
      if (event.kind === 'noteOn') {
        bytes.push(0x90, Number(event.note) & 0x7f, Math.max(1, Math.min(127, Number(event.velocity) || 100)));
      } else if (event.kind === 'noteOff') {
        bytes.push(0x80, Number(event.note) & 0x7f, 0x00);
      } else if (event.kind === 'program') {
        bytes.push(0xc0, Number(event.program) & 0x7f);
      } else if (event.kind === 'sustain') {
        bytes.push(0xb0, 64, event.enabled ? 127 : 0);
      } else if (event.kind === 'tempo') {
        bytes.push(0xff, 0x51, 0x03, ...tempoBytes(event.bpm));
        currentBpm = Math.max(1, Number(event.bpm) || currentBpm);
        remainder = 0;
      }
      previousMs = ms;
    }

    const endMs = Math.max(previousMs, Number(take.durationMs) || 0);
    const endScaled = remainder + Math.max(0, endMs - previousMs) * currentBpm * PPQN;
    const endTicks = Math.floor(endScaled / 60000);
    bytes.push(...vlq(endTicks), 0xb0, 64, 0x00, 0x00, 0xb0, 123, 0x00, 0x00, 0xff, 0x2f, 0x00);
    durationTicks += endTicks;
    return { bytes, durationTicks };
  }

  function encodeTake(take) {
    const track = encodeTrack(take);
    const bytes = [0x4d, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06];
    pushBe16(bytes, 0);
    pushBe16(bytes, 1);
    pushBe16(bytes, PPQN);
    bytes.push(0x4d, 0x54, 0x72, 0x6b);
    pushBe32(bytes, track.bytes.length);
    bytes.push(...track.bytes);
    return Uint8Array.from(bytes);
  }

  return { PPQN, vlq, encodeTake };
}));
