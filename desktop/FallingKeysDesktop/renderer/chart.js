(function exposeChartParser(root, factory) {
  const api = factory();
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  if (root) root.FallingKeysChart = api;
})(typeof globalThis === 'undefined' ? this : globalThis, function chartFactory() {
  const MAX_EVENTS = 250000;

  function numberOr(value, fallback) {
    const number = Number(value);
    return Number.isFinite(number) ? number : fallback;
  }

  function parsePTC(text, options = {}) {
    if (typeof text !== 'string') throw new Error('Chart data must be text');
    const lines = text.replace(/^\uFEFF/, '').split(/\r?\n/);
    const first = lines.find((line) => line.trim().length > 0);
    if (!first || first.trim() !== 'PTC1') throw new Error('Only PTC1 charts are supported');

    const meta = { title: 'Untitled chart', artist: '', lanes: 4, bpm: 120, offset: 0 };
    const events = [];
    for (const rawLine of lines) {
      const line = rawLine.trim();
      if (!line || line.startsWith('#')) continue;
      if (/^N\s*,/i.test(line)) {
        const fields = line.slice(line.indexOf(',') + 1).split(',').map((field) => field.trim());
        if (fields[0].toLowerCase() === 'time_ms') continue;
        if (fields.length < 3) continue;
        const time = numberOr(fields[0], NaN);
        const lane = Math.trunc(numberOr(fields[1], NaN));
        const note = Math.trunc(numberOr(fields[2], 60));
        const duration = Math.max(0, Math.min(120000, numberOr(fields[3], 0)));
        if (!Number.isFinite(time) || !Number.isFinite(lane) || lane < 0 || lane >= 6) continue;
        events.push({
          id: events.length,
          time: Math.max(0, time + numberOr(meta.offset, 0)),
          lane,
          note: Math.max(0, Math.min(127, note)),
          duration,
        });
        if (events.length > (options.maxEvents || MAX_EVENTS)) throw new Error('Chart has too many notes');
        continue;
      }
      const separator = line.indexOf('=');
      if (separator <= 0) continue;
      const key = line.slice(0, separator).trim().toLowerCase();
      const value = line.slice(separator + 1).trim();
      if (key === 'title' || key === 'artist') meta[key] = value.slice(0, 160);
      if (key === 'lanes') meta.lanes = Math.max(4, Math.min(6, Math.trunc(numberOr(value, 4))));
      if (key === 'bpm') meta.bpm = Math.max(20, Math.min(320, numberOr(value, 120)));
      if (key === 'offset') meta.offset = Math.max(-10000, Math.min(10000, numberOr(value, 0)));
    }
    events.sort((left, right) => left.time - right.time || left.lane - right.lane || left.id - right.id);
    if (!events.length) throw new Error('Chart has no notes');
    const duration = events.reduce((latest, event) => Math.max(latest, event.time + event.duration), 0);
    return {
      ...meta,
      events,
      duration,
      id: options.id || `${meta.title}:${events.length}:${duration}`,
      source: options.source || 'file',
      flexibleLanes: Boolean(options.flexibleLanes),
    };
  }

  function mapLanes(chart, laneCount) {
    const lanes = Math.max(4, Math.min(6, Math.trunc(laneCount || chart.lanes)));
    const events = chart.events.map((event, index) => ({
      ...event,
      id: index,
      lane: event.lane % lanes,
    }));
    return { ...chart, lanes, events, id: `${chart.id}|lanes:${lanes}` };
  }

  return { parsePTC, mapLanes, MAX_EVENTS };
});
