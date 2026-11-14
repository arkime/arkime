/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

The OpenSearch/Elasticsearch sessions backend's query rewrites
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const cjsRequire = createRequire(import.meta.url);
const DbESImpl = cjsRequire('../viewer/db.es.js');

describe('rankEmptyMetricsLast', () => {
  const rank = (aggs) => { DbESImpl.rankEmptyMetricsLast(aggs); return aggs; };
  const has = (field) => ({ max: { script: { source: 'doc.containsKey(params.f) && doc[params.f].size() > 0 ? 1 : 0', params: { f: field } } } });

  test('orders first by whether an avg, min, max or percentiles field has a value', () => {
    for (const op of ['avg', 'min', 'max']) {
      const got = rank({ a: { terms: { field: 'source.ip', order: { m: 'desc' } }, aggs: { m: { [op]: { field: 'network.bytes' } } } } });
      assert.deepEqual(got.a.terms.order, [{ m_has: 'desc' }, { m: 'desc' }], op);
      assert.deepEqual(got.a.aggs.m_has, has('network.bytes'), op);
    }

    const aggs = rank({ a: { histogram: { field: 'source.port', interval: 1024, order: { 'm[95.0]': 'asc' } }, aggregations: { m: { percentiles: { field: 'network.bytes', percents: [95] } } } } });
    assert.deepEqual(aggs.a.histogram.order, [{ m_has: 'desc' }, { 'm[95.0]': 'asc' }], 'empty buckets last when ascending too');
    assert.deepEqual(aggs.a.aggregations.m_has, has('network.bytes'));
  });

  test('goes just before the metric in an order list', () => {
    const aggs = rank({ a: { date_histogram: { field: 'firstPacket', fixed_interval: '1h', order: [{ _count: 'desc' }, { m: 'asc' }] }, aggs: { m: { min: { field: 'network.bytes' } } } } });
    assert.deepEqual(aggs.a.date_histogram.order, [{ _count: 'desc' }, { m_has: 'desc' }, { m: 'asc' }]);
  });

  test('rewrites nested bucket aggregations and only once', () => {
    const aggs = { a: { terms: { field: 'source.ip', order: { _count: 'desc' } }, aggs: { b: { terms: { field: 'protocol', order: { m: 'desc' } }, aggs: { m: { avg: { field: 'network.bytes' } } } } } } };
    const once = structuredClone(rank(aggs));
    assert.deepEqual(once.a.terms.order, { _count: 'desc' });
    assert.deepEqual(once.a.aggs.b.terms.order, [{ m_has: 'desc' }, { m: 'desc' }]);
    assert.deepEqual(rank(aggs), once, 'a second pass changes nothing');
  });

  test('leaves orders that have no empty case alone', () => {
    const cases = {
      count: { a: { terms: { field: 'source.ip', order: { _count: 'desc' } }, aggs: { m: { avg: { field: 'network.bytes' } } } } },
      key: { a: { terms: { field: 'source.ip', order: { _key: 'asc' } }, aggs: { m: { max: { field: 'network.bytes' } } } } },
      sum: { a: { terms: { field: 'source.ip', order: { m: 'desc' } }, aggs: { m: { sum: { field: 'network.bytes' } } } } },
      cardinality: { a: { terms: { field: 'source.ip', order: { m: 'desc' } }, aggs: { m: { cardinality: { field: 'destination.ip' } } } } },
      noOrder: { a: { terms: { field: 'source.ip' }, aggs: { m: { avg: { field: 'network.bytes' } } } } },
      noSubAggs: { a: { terms: { field: 'source.ip', order: { _count: 'desc' } } } }
    };
    for (const [label, aggs] of Object.entries(cases)) {
      assert.deepEqual(rank(structuredClone(aggs)), aggs, label);
    }
    for (const odd of [undefined, null, 'x', {}]) {
      assert.doesNotThrow(() => DbESImpl.rankEmptyMetricsLast(odd));
    }
  });
});
