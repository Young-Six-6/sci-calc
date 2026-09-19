const test = require('node:test');
const assert = require('node:assert/strict');
const { buildChordVoicing, normalizeChordType } = require('../renderer/chord-engine.js');

test('builds a calculator-friendly chord with optional bass', () => {
  assert.deepEqual(buildChordVoicing(60, 'triad', true), [48, 60, 64, 67]);
  assert.deepEqual(buildChordVoicing(60, 'seventh', false), [60, 64, 67, 70]);
  assert.equal(buildChordVoicing(20, 'minor', false)[0], 24);
});

test('falls back to a stable major triad for unknown types', () => {
  assert.equal(normalizeChordType('nope'), 'triad');
  assert.deepEqual(buildChordVoicing(65, 'nope', false), [65, 69, 72]);
});
