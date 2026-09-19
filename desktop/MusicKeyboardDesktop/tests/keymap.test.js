const test = require('node:test');
const assert = require('node:assert/strict');
const keymap = require('../renderer/keymap.js');

const settings = { octave: 4, root: 0, transpose: 0, layout: 'calculator' };

test('calculator layout matches the physical white-key mapping', () => {
  assert.deepEqual(keymap.keyToNotes(18, settings, false), [60]);
  assert.deepEqual(keymap.keyToNotes(19, settings, false), [62]);
  assert.deepEqual(keymap.keyToNotes(20, settings, false), [64]);
  assert.deepEqual(keymap.keyToNotes(25, settings, false), [59]);
  assert.deepEqual(keymap.keyToNotes(26, settings, false), [57]);
});

test('calculator Enter lift raises a note by one octave', () => {
  assert.deepEqual(keymap.keyToNotes(18, settings, true), [72]);
  assert.deepEqual(keymap.keyToNotes(21, settings, true), []);
});

test('chord and chromatic layouts preserve firmware offsets', () => {
  assert.deepEqual(keymap.keyToNotes(2, { ...settings, layout: 'chords' }, false), [60, 64, 67]);
  assert.deepEqual(keymap.keyToNotes(21, { ...settings, layout: 'chords' }, false), [72]);
  assert.deepEqual(keymap.keyToNotes(26, { ...settings, layout: 'chromatic' }, false), [70]);
  assert.deepEqual(keymap.keyToNotes(23, { ...settings, layout: 'chromatic' }, false), [79]);
});
