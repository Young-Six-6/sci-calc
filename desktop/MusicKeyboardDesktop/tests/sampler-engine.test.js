const test = require('node:test');
const assert = require('node:assert/strict');
const { DEFAULT_KEY_IDS, buildSlices, sliceForKey } = require('../renderer/sampler-engine.js');

test('splits a sample into ordered bounded slices', () => {
  const slices = buildSlices(10, 4, .1, .9);
  assert.equal(slices.length, 4);
  assert.equal(slices[0].index, 0);
  assert.equal(slices[0].start, 1);
  assert.ok(Math.abs(slices[0].end - 3) < 1e-9);
  assert.ok(Math.abs(slices.at(-1).end - 9) < 1e-9);
});

test('maps calculator performance keys to slice indexes', () => {
  assert.equal(sliceForKey(18), 0);
  assert.equal(sliceForKey(14), 5);
  assert.equal(sliceForKey(21), -1);
  assert.equal(DEFAULT_KEY_IDS.length, 12);
});
