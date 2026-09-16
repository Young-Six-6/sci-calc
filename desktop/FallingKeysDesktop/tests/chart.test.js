const assert = require('node:assert/strict');
const test = require('node:test');
const { parsePTC, mapLanes } = require('../renderer/chart.js');

test('parses PTC1 metadata, offset, sorting, and duration', () => {
  const chart = parsePTC('PTC1\ntitle=Demo\nlanes=4\noffset=25\nN,time_ms,lane,note,duration_ms\nN,500,1,64,100\nN,100,0,60,0\n');
  assert.equal(chart.title, 'Demo');
  assert.equal(chart.events[0].time, 125);
  assert.equal(chart.events[1].time, 525);
  assert.equal(chart.duration, 625);
});

test('maps flexible demo charts to the selected lane count', () => {
  const chart = parsePTC('PTC1\nlanes=6\nN,time_ms,lane,note,duration_ms\nN,0,5,72,0\n', { flexibleLanes: true });
  assert.equal(mapLanes(chart, 4).events[0].lane, 1);
});
