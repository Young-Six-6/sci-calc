const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

const source = fs.readFileSync(path.join(__dirname, '..', 'renderer', 'app.js'), 'utf8');

test('audio timeline contains both lead and accompaniment notes', () => {
  assert.match(source, /this\.audioNotes = \[/);
  assert.match(source, /lead: true/);
  assert.match(source, /lead: false/);
  assert.match(source, /this\.scheduleAudio\(\)/);
});

test('key judgment does not directly trigger note or miss sounds', () => {
  assert.doesNotMatch(source, /playToneAt\(candidate\.note/);
  assert.doesNotMatch(source, /function playMiss\(/);
});

test('latency calibration corrects physical judgment but not the auto pilot', () => {
  assert.match(source, /localStorage\.setItem\('fallingKeys\.latencyMs'/);
  assert.match(source, /nowFromPerf\(timestamp = performance\.now\(\), applyCalibration = true\)/);
  assert.match(source, /this\.pressLane\(event\.lane, this\.startedAtPerf \+ event\.time, false\)/);
  assert.match(source, /this\.releaseLane\(event\.lane, this\.startedAtPerf \+ event\.time \+ event\.duration, false\)/);
});
