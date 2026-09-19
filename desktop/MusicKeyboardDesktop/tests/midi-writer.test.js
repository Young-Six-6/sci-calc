const test = require('node:test');
const assert = require('node:assert/strict');
const writer = require('../renderer/midi-writer.js');

test('writes a format 0 MIDI file with tempo, program, notes, and end marker', () => {
  const midi = writer.encodeTake({
    initialBpm: 120,
    initialProgram: 0,
    durationMs: 500,
    events: [
      { kind: 'noteOn', timeMs: 0, note: 60, velocity: 100 },
      { kind: 'noteOff', timeMs: 500, note: 60 },
    ],
  });
  assert.equal(Buffer.from(midi.subarray(0, 14)).toString('ascii', 0, 4), 'MThd');
  assert.deepEqual(Array.from(midi.subarray(8, 14)), [0, 0, 0, 1, 1, 224]);
  assert.equal(Buffer.from(midi).toString('hex').includes('ff510307a120'), true);
  assert.equal(Buffer.from(midi).toString('hex').includes('903c64'), true);
  assert.equal(Buffer.from(midi).toString('hex').endsWith('00ff2f00'), true);
});

test('encodes variable length quantities', () => {
  assert.deepEqual(writer.vlq(0), [0]);
  assert.deepEqual(writer.vlq(127), [127]);
  assert.deepEqual(writer.vlq(128), [129, 0]);
  assert.deepEqual(writer.vlq(480), [131, 96]);
});

test('keeps a tempo change in the exported event stream', () => {
  const midi = Buffer.from(writer.encodeTake({
    initialBpm: 120,
    initialProgram: 0,
    durationMs: 1000,
    events: [{ kind: 'tempo', timeMs: 500, bpm: 90 }],
  }));
  assert.equal(midi.toString('hex').includes('ff51030a2c2b'), true);
});
