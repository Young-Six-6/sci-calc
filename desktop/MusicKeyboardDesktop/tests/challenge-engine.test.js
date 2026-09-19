const test = require('node:test');
const assert = require('node:assert/strict');
const {
  buildChallengePhrase,
  difficultyConfig,
  normalizeDifficulty,
  previewDurationMs,
  scoreForStep,
  targetKeyIds,
  timingWindowMs,
} = require('../renderer/challenge-engine.js');

function sampleLesson() {
  return {
    steps: Array.from({ length: 12 }, (_, index) => ({
      tick: index * 120,
      duration_beats: index % 2 ? .5 : 1,
      key_ids: [18 + (index % 3)],
      notes: [{ pitch: 60 + index, name: `N${index}`, key_id: 18 + (index % 3) }],
    })),
  };
}

test('builds a bounded contiguous phrase and copies step arrays', () => {
  const phrase = buildChallengePhrase(sampleLesson(), 'normal', 3, () => .4);
  assert.equal(phrase.length, 5);
  assert.equal(phrase[0].challengeIndex, 0);
  assert.deepEqual(phrase.map((step) => step.tick), [360, 480, 600, 720, 840]);
  phrase[0].key_ids.push(99);
  assert.equal(sampleLesson().steps[3].key_ids.includes(99), false);
});

test('difficulty grows phrases only up to its configured cap', () => {
  assert.equal(difficultyConfig('easy', 99).phraseLength, 5);
  assert.equal(difficultyConfig('hard', 99).phraseLength, 9);
  assert.equal(normalizeDifficulty('unknown'), 'normal');
});

test('timing and scoring stay within playable bounds', () => {
  const step = { duration_beats: .5, key_ids: [21, 18, 19] };
  assert.equal(targetKeyIds(step).length, 3);
  assert.ok(timingWindowMs(step, 120, 'hard', 4) >= 280);
  assert.ok(previewDurationMs(step, 'easy') >= 180);
  assert.ok(scoreForStep('hard', 4, 3) > scoreForStep('easy', 1, 0));
});
