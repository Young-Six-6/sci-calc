const test = require('node:test');
const assert = require('node:assert/strict');
const groove = require('../renderer/groove-engine.js');

test('builds ordered arpeggios across octaves', () => {
  assert.deepEqual(groove.buildArpeggio([67, 60, 64], 'up', 2), [60, 64, 67, 72, 76, 79]);
  assert.deepEqual(groove.buildArpeggio([60, 64, 67], 'down', 1), [67, 64, 60]);
  assert.deepEqual(groove.buildArpeggio([60, 64, 67], 'updown', 1), [60, 64, 67, 64]);
});

test('keeps groove timing divisions musically consistent', () => {
  assert.equal(groove.stepIntervalMs(120, 4), 500);
  assert.equal(groove.stepIntervalMs(120, 8), 250);
  assert.equal(groove.stepIntervalMs(120, 16), 125);
  assert.equal(groove.stepIntervalMs(120, 32), 62.5);
});

test('sanitizes a saved 16/32-step pattern without changing its shape', () => {
  const saved = groove.sanitizeGrooveSettings({ steps: 32, resolution: 8, pattern: { kick: [true], snare: [], hat: [], clap: [] } });
  assert.equal(saved.steps, 32);
  assert.equal(saved.resolution, 8);
  assert.equal(saved.pattern.kick.length, groove.PATTERN_LENGTH);
  assert.equal(saved.pattern.kick[0], true);
  assert.equal(saved.pattern.kick[1], false);
  assert.equal(saved.pattern.snare.every(Boolean), false);
});
