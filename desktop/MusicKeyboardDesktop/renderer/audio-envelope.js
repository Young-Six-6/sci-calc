(function attachMusicAudioEnvelope(root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.MusicAudioEnvelope = factory();
}(typeof self !== 'undefined' ? self : this, function createMusicAudioEnvelope() {
  const DEFAULT_FLOOR = 0.0001;

  function positive(value, fallback = 0) {
    const number = Number(value);
    return Number.isFinite(number) ? Math.max(0, number) : fallback;
  }

  function exponentialBetween(start, end, progress) {
    const safeStart = Math.max(DEFAULT_FLOOR, start);
    const safeEnd = Math.max(DEFAULT_FLOOR, end);
    return safeStart * Math.pow(safeEnd / safeStart, Math.max(0, Math.min(1, progress)));
  }

  function envelopeValueAtTime(envelope, time) {
    const floor = Math.max(DEFAULT_FLOOR, positive(envelope?.floor, DEFAULT_FLOOR));
    const peak = Math.max(floor, positive(envelope?.peak, floor));
    const sustain = Math.max(floor, positive(envelope?.sustainGain, floor));
    const attack = positive(envelope?.attack);
    const decay = positive(envelope?.decay);
    const elapsed = Math.max(0, positive(time) - positive(envelope?.startTime));

    if (attack > 0 && elapsed < attack) {
      return floor + (peak - floor) * (elapsed / attack);
    }
    if (decay > 0 && elapsed < attack + decay) {
      return exponentialBetween(peak, sustain, (elapsed - attack) / decay);
    }

    const tail = positive(envelope?.oneShotTail);
    if (tail > 0 && elapsed < attack + decay + tail) {
      return exponentialBetween(sustain, floor, (elapsed - attack - decay) / tail);
    }
    return tail > 0 ? floor : sustain;
  }

  function holdEnvelopeAtTime(audioParam, envelope, time) {
    const heldValue = envelopeValueAtTime(envelope, time);
    if (typeof audioParam?.cancelAndHoldAtTime === 'function') {
      audioParam.cancelAndHoldAtTime(time);
      return heldValue;
    }

    audioParam.cancelScheduledValues(time);
    audioParam.setValueAtTime(heldValue, time);
    return heldValue;
  }

  return { DEFAULT_FLOOR, envelopeValueAtTime, holdEnvelopeAtTime };
}));
