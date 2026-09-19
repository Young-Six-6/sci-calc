(function attachMusicChallenge(root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.MusicChallenge = factory();
}(typeof self !== 'undefined' ? self : this, function createMusicChallenge() {
  const DIFFICULTIES = Object.freeze({
    easy: Object.freeze({ baseLength: 3, maxLength: 5, previewMs: 520, windowBeats: 2.4, points: 100 }),
    normal: Object.freeze({ baseLength: 4, maxLength: 7, previewMs: 390, windowBeats: 1.65, points: 150 }),
    hard: Object.freeze({ baseLength: 5, maxLength: 9, previewMs: 285, windowBeats: 1.05, points: 220 }),
  });

  function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  function normalizeDifficulty(value) {
    return Object.prototype.hasOwnProperty.call(DIFFICULTIES, value) ? value : 'normal';
  }

  function difficultyConfig(value, round = 1) {
    const difficulty = normalizeDifficulty(value);
    const source = DIFFICULTIES[difficulty];
    const safeRound = clamp(Math.round(Number(round) || 1), 1, 99);
    const phraseLength = clamp(source.baseLength + Math.floor((safeRound - 1) / 2), source.baseLength, source.maxLength);
    return { difficulty, ...source, phraseLength, round: safeRound };
  }

  function copyStep(step, index) {
    return {
      ...step,
      challengeIndex: index,
      key_ids: [...new Set((step.key_ids || []).map(Number).filter(Number.isInteger))],
      notes: (step.notes || []).map((note) => ({ ...note })),
    };
  }

  function buildChallengePhrase(data, difficulty = 'normal', round = 1, random = Math.random) {
    const steps = (data?.steps || []).filter((step) => Array.isArray(step.key_ids) && step.key_ids.length);
    if (!steps.length) return [];
    const config = difficultyConfig(difficulty, round);
    const length = Math.min(config.phraseLength, steps.length);
    const room = Math.max(0, steps.length - length);
    const roll = Number(random());
    const normalized = Number.isFinite(roll) ? clamp(roll, 0, .999999) : 0;
    const start = Math.floor(normalized * (room + 1));
    return steps.slice(start, start + length).map((step, index) => copyStep(step, index));
  }

  function timingWindowMs(step, bpm, difficulty = 'normal', round = 1) {
    const config = difficultyConfig(difficulty, round);
    const safeBpm = clamp(Number(bpm) || 120, 40, 240);
    const beats = Math.max(.25, Number(step?.duration_beats) || .5);
    return clamp(60000 / safeBpm * beats * config.windowBeats, 280, 3600);
  }

  function previewDurationMs(step, difficulty = 'normal') {
    const config = difficultyConfig(difficulty, 1);
    const beats = clamp(Number(step?.duration_beats) || .5, .25, 2);
    return Math.max(180, Math.round(config.previewMs * Math.min(1.4, .75 + beats * .35)));
  }

  function targetKeyIds(step) {
    return [...new Set((step?.key_ids || []).map(Number).filter(Number.isInteger))];
  }

  function scoreForStep(difficulty = 'normal', round = 1, combo = 0) {
    const config = difficultyConfig(difficulty, round);
    return config.points + Math.min(200, Math.max(0, Number(combo) || 0) * 12);
  }

  return {
    DIFFICULTIES,
    normalizeDifficulty,
    difficultyConfig,
    buildChallengePhrase,
    timingWindowMs,
    previewDurationMs,
    targetKeyIds,
    scoreForStep,
  };
}));
