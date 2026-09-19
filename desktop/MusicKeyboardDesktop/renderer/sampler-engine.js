(function attachMusicSampler(root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.MusicSampler = factory();
}(typeof self !== 'undefined' ? self : this, function createMusicSampler() {
  const DEFAULT_KEY_IDS = Object.freeze([18, 19, 20, 12, 13, 14, 6, 7, 8, 25, 26, 4]);

  function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  function buildSlices(duration, count = 6, start = 0, end = 1) {
    const safeDuration = Math.max(0, Number(duration) || 0);
    const safeCount = clamp(Math.round(Number(count) || 6), 2, 12);
    const from = clamp(Number(start) || 0, 0, .98);
    const to = clamp(Number(end) || 1, from + .02, 1);
    const span = to - from;
    return Array.from({ length: safeCount }, (_, index) => ({
      index,
      start: safeDuration * (from + span * index / safeCount),
      end: safeDuration * (from + span * (index + 1) / safeCount),
    }));
  }

  function sliceForKey(keyId, keyIds = DEFAULT_KEY_IDS) {
    const index = keyIds.indexOf(Number(keyId));
    return index >= 0 ? index : -1;
  }

  return { DEFAULT_KEY_IDS, buildSlices, sliceForKey };
}));
