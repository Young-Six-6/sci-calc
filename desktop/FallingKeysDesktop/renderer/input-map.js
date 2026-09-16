(function exposeInputMap(root, factory) {
  const api = factory();
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  if (root) root.FallingKeysInput = api;
})(typeof globalThis === 'undefined' ? this : globalThis, function inputFactory() {
  function laneForKey(keyId, laneCount) {
    const id = Number(keyId);
    const lanes = Math.max(4, Math.min(6, Number(laneCount) || 4));
    if (!Number.isInteger(id) || id < 0 || id >= 30 || id === 0 || id === 24) return -1;
    const lane = id % 6;
    return lane < lanes ? lane : -1;
  }

  function maskHas(mask, keyId) {
    return (Number(mask) >>> 0 & (2 ** Number(keyId))) !== 0;
  }

  return { laneForKey, maskHas };
});
