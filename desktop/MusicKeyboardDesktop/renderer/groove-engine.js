(function attachMusicGroove(root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.MusicGroove = factory();
}(typeof self !== 'undefined' ? self : this, function createMusicGroove() {
  const PATTERN_LENGTH = 32;
  const DRUM_LANES = [
    { id: 'kick', labelKey: 'kick' },
    { id: 'snare', labelKey: 'snare' },
    { id: 'hat', labelKey: 'hat' },
    { id: 'clap', labelKey: 'clap' },
  ];
  const ARP_PATTERNS = ['up', 'down', 'updown', 'random'];
  const ARP_RATES = [4, 8, 16, 32];

  function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  function emptyLane() {
    return Array(PATTERN_LENGTH).fill(false);
  }

  function defaultPattern() {
    const pattern = {
      kick: emptyLane(),
      snare: emptyLane(),
      hat: emptyLane(),
      clap: emptyLane(),
    };
    [0, 8, 16, 24].forEach((step) => { pattern.kick[step] = true; });
    [4, 12, 20, 28].forEach((step) => { pattern.snare[step] = true; });
    for (let step = 0; step < PATTERN_LENGTH; step += 2) pattern.hat[step] = true;
    [12, 28].forEach((step) => { pattern.clap[step] = true; });
    return pattern;
  }

  function sanitizePattern(saved) {
    const defaults = defaultPattern();
    const source = saved && typeof saved === 'object' ? saved : {};
    return DRUM_LANES.reduce((pattern, lane) => {
      const values = Array.isArray(source[lane.id]) ? source[lane.id] : defaults[lane.id];
      pattern[lane.id] = Array.from({ length: PATTERN_LENGTH }, (_, index) => Boolean(values[index]));
      return pattern;
    }, {});
  }

  function sanitizeGrooveSettings(saved) {
    const source = saved && typeof saved === 'object' ? saved : {};
    return {
      steps: Number(source.steps) === 32 ? 32 : 16,
      resolution: Number(source.resolution) === 8 ? 8 : 16,
      pattern: sanitizePattern(source.pattern),
    };
  }

  function stepIntervalMs(bpm, resolution) {
    const safeBpm = clamp(Number(bpm) || 120, 40, 240);
    const division = [4, 8, 16, 32].includes(Number(resolution)) ? Number(resolution) : 16;
    return 60000 / safeBpm * 4 / division;
  }

  function uniqueNotes(notes) {
    return [...new Set((Array.isArray(notes) ? notes : [])
      .map((note) => Number(note))
      .filter((note) => Number.isFinite(note))
      .map((note) => clamp(Math.round(note), 24, 108)))].sort((a, b) => a - b);
  }

  function buildArpeggio(notes, pattern = 'up', octaves = 1) {
    const base = uniqueNotes(notes);
    if (!base.length) return [];
    const octaveCount = clamp(Math.round(Number(octaves) || 1), 1, 3);
    const sequence = [];
    for (let octave = 0; octave < octaveCount; octave += 1) {
      base.forEach((note) => {
        const shifted = note + octave * 12;
        if (shifted <= 108) sequence.push(shifted);
      });
    }
    if (pattern === 'down') return sequence.reverse();
    if (pattern === 'updown' && sequence.length > 1) {
      const returnPath = sequence.length === 2 ? [sequence[0]] : sequence.slice(1, -1).reverse();
      return sequence.concat(returnPath);
    }
    return sequence;
  }

  return {
    ARP_PATTERNS,
    ARP_RATES,
    DRUM_LANES,
    PATTERN_LENGTH,
    buildArpeggio,
    defaultPattern,
    sanitizeGrooveSettings,
    stepIntervalMs,
  };
}));
