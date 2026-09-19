const test = require('node:test');
const assert = require('node:assert/strict');
const { envelopeValueAtTime, holdEnvelopeAtTime } = require('../renderer/audio-envelope.js');

const envelope = {
  startTime: 10,
  floor: 0.0001,
  peak: 0.2,
  sustainGain: 0.02,
  attack: 0.1,
  decay: 1,
  oneShotTail: 0,
};

test('calculates the current gain while a note is still attacking or decaying', () => {
  const duringAttack = envelopeValueAtTime(envelope, 10.05);
  const duringDecay = envelopeValueAtTime(envelope, 10.6);
  assert.ok(duringAttack > envelope.floor && duringAttack < envelope.peak);
  assert.ok(duringDecay > envelope.sustainGain && duringDecay < envelope.peak);
  assert.equal(envelopeValueAtTime(envelope, 12), envelope.sustainGain);
});

test('uses cancelAndHoldAtTime so release starts at the audible gain without a jump', () => {
  const calls = [];
  const audioParam = {
    cancelAndHoldAtTime: (time) => calls.push(['hold', time]),
    cancelScheduledValues: (time) => calls.push(['cancel', time]),
    setValueAtTime: (value, time) => calls.push(['set', value, time]),
  };
  holdEnvelopeAtTime(audioParam, envelope, 10.6);
  assert.deepEqual(calls, [['hold', 10.6]]);
});

test('falls back to the computed ADSR value when cancelAndHoldAtTime is unavailable', () => {
  const calls = [];
  const audioParam = {
    cancelScheduledValues: (time) => calls.push(['cancel', time]),
    setValueAtTime: (value, time) => calls.push(['set', value, time]),
  };
  const held = holdEnvelopeAtTime(audioParam, envelope, 10.6);
  assert.deepEqual(calls[0], ['cancel', 10.6]);
  assert.deepEqual(calls[1], ['set', held, 10.6]);
  assert.ok(held > envelope.sustainGain && held < envelope.peak);
});

test('tracks the natural tail of one-shot instruments', () => {
  const bell = { ...envelope, oneShotTail: 2 };
  const startOfTail = envelopeValueAtTime(bell, 11.1);
  const middleOfTail = envelopeValueAtTime(bell, 12.1);
  const endOfTail = envelopeValueAtTime(bell, 13.1);
  assert.ok(startOfTail > middleOfTail);
  assert.ok(middleOfTail > endOfTail);
  assert.ok(Math.abs(endOfTail - bell.floor) < 1e-12);
});
