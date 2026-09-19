(function attachMusicKeymap(root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.MusicKeymap = factory();
}(typeof self !== 'undefined' ? self : this, function createMusicKeymap() {
  const KEY_NAMES = [
    'ESC', '/', '*', '-', 'SIN', 'SEC',
    '7', '8', '9', '+', 'COS', 'CSC',
    '4', '5', '6', '^', 'TAN', 'COT',
    '1', '2', '3', 'ENTER', 'EXP', 'RPN/NORM',
    'SHIFT', '0', '.', 'BACKSPACE', 'ANGLE', 'TAB',
  ];
  const TONE_NAMES = ['Piano', 'E.Piano', 'Organ', 'Warm Pad', 'Lead', 'Bell'];
  const GM_PROGRAMS = [0, 4, 16, 89, 80, 14];
  const ROOT_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  const LAYOUT_NAMES = ['Calculator', 'Chords', 'Chromatic'];
  const NOTE_OFFSETS = {
    1: 16, 2: 17, 3: 19, 9: 21, 15: 23,
    26: -3, 25: -1, 18: 0, 19: 2, 20: 4,
    12: 5, 13: 7, 14: 9, 6: 11, 7: 12,
    8: 14, 4: 1, 5: 3, 10: 6, 11: 8,
    16: 10, 17: 13, 22: 15, 23: 18,
  };
  const CHROMATIC_OFFSETS = {
    18: 0, 19: 1, 20: 2, 12: 3, 13: 4, 14: 5,
    6: 6, 7: 7, 8: 8, 25: 9, 26: 10, 21: 11,
    4: 12, 5: 13, 10: 14, 11: 15, 16: 16, 17: 17,
    22: 18, 23: 19,
  };

  function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  function keyboardBaseNote(settings) {
    return clamp((settings.octave + 1) * 12 + settings.root + settings.transpose, 24, 96);
  }

  function note(base, offset) {
    return clamp(base + offset, 24, 108);
  }

  function keyToNotes(keyId, settings, enterHeld) {
    const layout = settings.layout || 'calculator';
    const base = keyboardBaseNote(settings);
    if (layout !== 'calculator') {
      const chord = {
        2: [0, 4, 7],
        1: [2, 5, 9],
        3: [5, 9, 12],
        9: [7, 11, 14],
        15: [9, 12, 16],
      }[keyId];
      if (chord) return chord.map((offset) => note(base, offset));
    }
    if (layout === 'chromatic') {
      if (!(keyId in CHROMATIC_OFFSETS)) return [];
      return [note(base, CHROMATIC_OFFSETS[keyId])];
    }
    if (layout === 'chords' && keyId === 21) return [note(base, 12)];
    if (!(keyId in NOTE_OFFSETS)) return [];
    let offset = NOTE_OFFSETS[keyId];
    if (layout === 'calculator' && enterHeld && keyId !== 21) offset += 12;
    return [note(base, offset)];
  }

  return {
    KEY_NAMES,
    TONE_NAMES,
    GM_PROGRAMS,
    ROOT_NAMES,
    LAYOUT_NAMES,
    keyboardBaseNote,
    keyToNotes,
    clamp,
  };
}));
