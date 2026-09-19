(function attachMusicChord(root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.MusicChord = factory();
}(typeof self !== 'undefined' ? self : this, function createMusicChord() {
  const CHORD_TYPES = Object.freeze({
    triad: Object.freeze([0, 4, 7]),
    minor: Object.freeze([0, 3, 7]),
    seventh: Object.freeze([0, 4, 7, 10]),
    sus4: Object.freeze([0, 5, 7]),
  });

  function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  function normalizeChordType(value) {
    return Object.prototype.hasOwnProperty.call(CHORD_TYPES, value) ? value : 'triad';
  }

  function buildChordVoicing(root, type = 'triad', includeBass = true) {
    const base = clamp(Math.round(Number(root) || 60), 24, 108);
    const intervals = CHORD_TYPES[normalizeChordType(type)];
    const notes = intervals.map((interval) => clamp(base + interval, 24, 108));
    if (includeBass) notes.unshift(clamp(base - 12, 24, 96));
    return [...new Set(notes)];
  }

  return { CHORD_TYPES, normalizeChordType, buildChordVoicing };
}));
