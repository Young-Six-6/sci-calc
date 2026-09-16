const assert = require('node:assert/strict');
const test = require('node:test');
const { parseMIDI } = require('../renderer/midi.js');

test('parses a standard MIDI note and converts ticks to milliseconds', () => {
  const events = [
    0x00, 0xff, 0x51, 0x03, 0x07, 0xa1, 0x20,
    0x00, 0x90, 0x3c, 0x64,
    0x60, 0x80, 0x3c, 0x40,
    0x00, 0xff, 0x2f, 0x00,
  ];
  const bytes = Uint8Array.from([
    0x4d, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x60,
    0x4d, 0x54, 0x72, 0x6b, 0x00, 0x00, 0x00, events.length, ...events,
  ]);
  const chart = parseMIDI(bytes, { source: 'test.mid', lanes: 4 });
  assert.equal(chart.events.length, 1);
  assert.equal(chart.events[0].time, 0);
  assert.equal(chart.duration, 500);
  assert.equal(chart.bpm, 120);
  assert.equal(chart.stats.accompanimentNotes, 0);
});
