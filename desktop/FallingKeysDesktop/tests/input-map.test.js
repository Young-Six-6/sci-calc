const assert = require('node:assert/strict');
const test = require('node:test');
const { laneForKey, maskHas } = require('../renderer/input-map.js');

test('preserves physical matrix columns as lanes', () => {
  assert.equal(laneForKey(1, 4), 1);
  assert.equal(laneForKey(5, 4), -1);
  assert.equal(laneForKey(5, 6), 5);
  assert.equal(laneForKey(0, 6), -1);
  assert.equal(laneForKey(24, 6), -1);
});

test('reads the 30-key state mask', () => {
  const mask = 2 ** 29;
  assert.equal(maskHas(mask, 29), true);
  assert.equal(maskHas(mask, 28), false);
});
