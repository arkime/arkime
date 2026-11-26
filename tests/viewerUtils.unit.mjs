/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for viewer/viewerUtils.js. Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const cjsRequire = createRequire(import.meta.url);
const ViewerUtils = cjsRequire('../viewer/viewerUtils.js');

describe('ViewerUtils.isSafeSegment', () => {
  test('allows the session ids Db.session2Sid makes', () => {
    for (const sid of [
      '3@191021-abcdefghijklmnopqrstuvwxyz_-AB',
      '3@191021:191021-abcd',
      'x@other_index-2024.01.02:AbC_d-123'
    ]) {
      assert.equal(ViewerUtils.isSafeSegment(sid), true, sid);
    }
  });

  test('allows node names', () => {
    for (const node of ['test', 'sensor-01', 'sensor_01.example.com', 'a']) {
      assert.equal(ViewerUtils.isSafeSegment(node), true, node);
    }
  });

  test('refuses anything that would change the s2s path or ids body', () => {
    for (const bad of [
      '', '.', '..', 'a/b', '../a', 'a\\b', 'a?b=1', 'a#b', 'a&ids=b', 'a,b', 'a%2Fb', 'a b', 'a\nb', 'a\0b', 'é'
    ]) {
      assert.equal(ViewerUtils.isSafeSegment(bad), false, JSON.stringify(bad));
    }
  });

  test('refuses non strings', () => {
    for (const bad of [undefined, null, 1, true, ['a'], { toString: () => 'a' }]) {
      assert.equal(ViewerUtils.isSafeSegment(bad), false);
    }
  });
});

describe('ViewerUtils percentiles', () => {
  test('percentileAgg asks for one HDR digest of every percent, once each', () => {
    assert.deepEqual(ViewerUtils.percentileAgg('length', 95), {
      percentiles: { field: 'length', percents: [95], hdr: { number_of_significant_value_digits: ViewerUtils.PERCENTILE_HDR_DIGITS } }
    });
    assert.deepEqual(ViewerUtils.percentileAgg('length', [50, 25, 50, 75]).percentiles.percents, [50, 25, 75]);
  });

  test('percentileAggs adds the field min and max', () => {
    assert.deepEqual(Object.keys(ViewerUtils.percentileAggs('m0', 'length', 95)), ['m0', 'm0Mn', 'm0Mx']);
    assert.deepEqual(ViewerUtils.percentileAggs('m0', 'length', 95).m0Mx, { max: { field: 'length' } });
  });

  test('percentileValue picks the percent asked for, else the first', () => {
    const result = { values: { '25.0': 10, '50.0': 20, '95.0': 90 } };
    assert.equal(ViewerUtils.percentileValue(result, 50), 20);
    assert.equal(ViewerUtils.percentileValue(result, 95), 90);
    assert.equal(ViewerUtils.percentileValue(result), 10);
    assert.equal(ViewerUtils.percentileValue(result, 99), 10, 'a percent not asked for is the first');
    assert.equal(ViewerUtils.percentileValue({ value: 7 }), 7, 'ClickHouse answers one percent as a value');
    assert.equal(ViewerUtils.percentileValue({ values: { '95.0': null } }, 95), 0, 'a bucket without values');
    assert.equal(ViewerUtils.percentileValue(undefined), 0);
  });

  test('an HDR value outside the min and max is clamped to them', () => {
    // one session of 123969 bytes: HDR at 2 digits says 123904
    assert.equal(ViewerUtils.percentileValue({ values: { '95.0': 123904 } }, 95, 123969, 123969), 123969);
    assert.equal(ViewerUtils.percentileValue({ values: { '95.0': 1200 } }, 95, 0, 1100), 1100);
    assert.equal(ViewerUtils.percentileValue({ values: { '95.0': 500 } }, 95, 100, 1000), 500);
    assert.equal(ViewerUtils.percentileValue({ values: { '95.0': 500 } }, 95, null, undefined), 500, 'no bounds, no clamp');
  });

  test('percentileIn reads an aggregation and its bounds from a bucket', () => {
    const bucket = { m0: { values: { '95.0': 123904, '25.0': 123904 } }, m0Mn: { value: 123969 }, m0Mx: { value: 123969 } };
    assert.equal(ViewerUtils.percentileIn(bucket, 'm0', 95), 123969);
    assert.equal(ViewerUtils.percentileIn(bucket, 'm0', 25), 123969);
    assert.equal(ViewerUtils.percentileIn({}, 'm0', 95), 0);
  });
});

describe('ViewerUtils.timelineFilters', () => {
  const req = (query, timelineDataFilters) => ({ query, user: { settings: timelineDataFilters ? { timelineDataFilters } : {} } });

  test("the user's filters, old names made new, and the metric field", () => {
    assert.deepEqual(ViewerUtils.timelineFilters(req({}, ['totPackets', 'totBytes', 'totDataBytes'])), ['network.packets', 'network.bytes', 'totDataBytes']);
    assert.deepEqual(ViewerUtils.timelineFilters(req({ metricField: 'tcpflags.syn' }, ['network.packets'])), ['network.packets', 'tcpflags.syn']);
    assert.deepEqual(ViewerUtils.timelineFilters(req({ metricField: 'network.packets' }, ['network.packets'])), ['network.packets']);
  });

  test('the defaults when the user has none', () => {
    assert.deepEqual(ViewerUtils.timelineFilters(req({})), ['network.packets', 'network.bytes', 'totDataBytes']);
  });

  test('metricOnly: just the metric field, or nothing besides sessions', () => {
    for (const metricOnly of [true, 'true']) {
      assert.deepEqual(ViewerUtils.timelineFilters(req({ metricOnly, metricField: 'network.bytes' }, ['network.packets'])), ['network.bytes']);
      assert.deepEqual(ViewerUtils.timelineFilters(req({ metricOnly })), []);
    }
    assert.deepEqual(ViewerUtils.timelineFilters(req({ metricOnly: 'false' })), ['network.packets', 'network.bytes', 'totDataBytes']);
  });

  test('the query it runs, when not the request\'s own', () => {
    const r = req({}, ['network.packets']);
    assert.deepEqual(ViewerUtils.timelineFilters(r, { metricOnly: true, metricField: 'tcpflags.syn' }), ['tcpflags.syn']);
  });
});
