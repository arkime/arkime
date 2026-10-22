/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Unit tests for merging aggregation results across clusters in the multi
viewer. Run with `npm run test:unit`.

These are not part of tests.pl: its multi viewer clusters don't share
sessions, so no bucket there ever comes back from more than one cluster.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const { newAggregations, aggConvert2Obj, aggAdd, aggConvert2Arr } = require('../viewer/multiesAggs.js');

// merge cluster responses the way multies.js does for one search
const merge = (aggregations, ...responses) => {
  const merged = newAggregations(aggregations);
  for (const response of responses) {
    const copy = structuredClone(response);
    aggConvert2Obj(copy);
    aggAdd(merged, copy);
  }
  aggConvert2Arr(merged);
  return merged;
};

const terms = (buckets) => ({ doc_count_error_upper_bound: 0, sum_other_doc_count: 0, buckets });

describe('top-level metric aggregations', () => {
  const aggregations = {
    bytes: { sum: { field: 'network.bytes' } },
    m0: { max: { field: 'network.bytes' } },
    m1: { min: { field: 'network.bytes' } },
    m2: { cardinality: { field: 'source.ip' } }
  };

  test('combine per their declared operator, whatever their name', () => {
    const merged = merge(aggregations,
      { bytes: { value: 10 }, m0: { value: 7 }, m1: { value: 3 }, m2: { value: 4 } },
      { bytes: { value: 5 }, m0: { value: 9 }, m1: { value: 1 }, m2: { value: 6 } });
    assert.equal(merged.bytes.value, 15);
    assert.equal(merged.m0.value, 9);
    assert.equal(merged.m1.value, 1);
    assert.equal(merged.m2.value, 6, 'a unique count keeps the larger cluster count');
  });

  test('a cluster with no value leaves the other cluster\'s value', () => {
    const merged = merge(aggregations,
      { bytes: { value: 10 }, m0: { value: 7 }, m1: { value: 3 }, m2: { value: 4 } },
      { bytes: { value: null }, m0: { value: null }, m1: { value: null }, m2: { value: null } });
    assert.deepEqual(merged, { bytes: { value: 10 }, m0: { value: 7 }, m1: { value: 3 }, m2: { value: 4 } });
  });
});

describe('terms buckets', () => {
  // a summary widget averaging bytes: sum + count per value, ranked by an avg
  const aggregations = {
    agg0: {
      terms: { field: 'source.ip', size: 5, order: { m0o: 'desc' } },
      aggs: {
        m0: { sum: { field: 'network.bytes' } },
        m0c: { value_count: { field: 'network.bytes' } },
        m0o: { avg: { field: 'network.bytes' } },
        m1: { cardinality: { field: 'destination.ip' } }
      }
    }
  };

  test('merge each metric per its operator and re-rank by the weighted average', () => {
    const merged = merge(aggregations,
      { agg0: terms([
        { key: 'x', doc_count: 1, m0: { value: 10 }, m0c: { value: 1 }, m0o: { value: 10 }, m1: { value: 1 } },
        { key: 'y', doc_count: 2, m0: { value: 10 }, m0c: { value: 2 }, m0o: { value: 5 }, m1: { value: 2 } }
      ]) },
      { agg0: terms([
        { key: 'x', doc_count: 3, m0: { value: 6 }, m0c: { value: 3 }, m0o: { value: 2 }, m1: { value: 3 } }
      ]) });

    const [first, second] = merged.agg0.buckets;
    assert.equal(first.key, 'y', 'y averages 5, x now averages 4');
    assert.equal(second.key, 'x');
    assert.equal(second.doc_count, 4);
    assert.equal(second.m0.value, 16);
    assert.equal(second.m0c.value, 4);
    assert.equal(second.m0o.value, 4, '(10 * 1 + 2 * 3) / 4');
    assert.equal(second.m1.value, 3, 'a unique count keeps the larger cluster count');
    assert.equal(merged.agg0._nested, undefined, 'merge bookkeeping is removed');
  });
});

describe('nested terms', () => {
  // a summary bar of source.ip split by its top 2 protocols
  const aggregations = {
    agg0: {
      terms: { field: 'source.ip', size: 5, order: { _count: 'desc' } },
      aggs: {
        m0: { sum: { field: 'network.bytes' } },
        split: {
          terms: { field: 'protocol', size: 2, order: { _count: 'desc' } },
          aggs: { m0: { sum: { field: 'network.bytes' } } }
        }
      }
    }
  };

  test('a value in several clusters merges its split, re-ranked and cut to size', () => {
    const merged = merge(aggregations,
      { agg0: terms([{
        key: 'ip1',
        doc_count: 6,
        m0: { value: 60 },
        split: terms([{ key: 'tcp', doc_count: 5, m0: { value: 50 } }, { key: 'udp', doc_count: 1, m0: { value: 10 } }])
      }]) },
      { agg0: terms([{
        key: 'ip1',
        doc_count: 8,
        m0: { value: 80 },
        split: terms([{ key: 'udp', doc_count: 6, m0: { value: 60 } }, { key: 'icmp', doc_count: 2, m0: { value: 20 } }])
      }]) });

    const [ip1] = merged.agg0.buckets;
    assert.equal(ip1.doc_count, 14);
    assert.equal(ip1.m0.value, 140);
    assert.deepEqual(ip1.split.buckets.map(b => [b.key, b.doc_count, b.m0.value]), [['udp', 7, 70], ['tcp', 5, 50]]);
  });

  test('a fractional size cuts like ES does, and an unusable one does not cut', () => {
    const withSplitSize = (size) => {
      const aggs = structuredClone(aggregations);
      aggs.agg0.aggs.split.terms.size = size;
      return aggs;
    };
    const clusters = [
      { agg0: terms([{ key: 'ip1', doc_count: 6, split: terms([{ key: 'tcp', doc_count: 5 }, { key: 'udp', doc_count: 1 }]) }]) },
      { agg0: terms([{ key: 'ip1', doc_count: 8, split: terms([{ key: 'udp', doc_count: 6 }, { key: 'icmp', doc_count: 2 }]) }]) }
    ];
    const splitKeys = (size) => merge(withSplitSize(size), ...clusters).agg0.buckets[0].split.buckets.map(b => b.key);

    assert.deepEqual(splitKeys(1.5), ['udp'], 'floored to 1');
    assert.deepEqual(splitKeys('2'), ['udp', 'tcp']);
    for (const size of [-1, 0, 'abc', null]) {
      assert.deepEqual(splitKeys(size), ['udp', 'tcp', 'icmp'], `size ${size} leaves the split uncut`);
    }
  });

  test('a value in one cluster keeps that cluster\'s split', () => {
    const split = terms([{ key: 'tcp', doc_count: 2, m0: { value: 20 } }]);
    const merged = merge(aggregations,
      { agg0: terms([{ key: 'ip1', doc_count: 2, m0: { value: 20 }, split }]) },
      { agg0: terms([{ key: 'ip2', doc_count: 1, m0: { value: 5 }, split: terms([]) }]) });
    assert.deepEqual(merged.agg0.buckets.map(b => b.key), ['ip1', 'ip2']);
    assert.deepEqual(merged.agg0.buckets[0].split, split);
  });

  test('every level of a deeper hierarchy merges (spigraphhierarchy)', () => {
    const level = (field, size, aggs) => ({
      terms: { field, size },
      aggregations: { srcip: { cardinality: { field: 'source.ip' } }, ...aggs }
    });
    const hierarchy = { field: level('protocol', 5, { field: level('destination.port', 5, { field: level('host.http', 2) }) }) };
    const leaf = (key, doc_count, srcip) => ({ key, doc_count, srcip: { value: srcip } });
    const cluster = (hosts) => ({
      field: terms([{
        key: 'tcp',
        doc_count: 10,
        srcip: { value: 4 },
        field: terms([{ key: 80, doc_count: 10, srcip: { value: 4 }, field: terms(hosts) }])
      }])
    });

    const merged = merge(hierarchy,
      cluster([leaf('a.com', 6, 2), leaf('b.com', 4, 2)]),
      cluster([leaf('c.com', 9, 5), leaf('b.com', 1, 1)]));

    const [tcp] = merged.field.buckets;
    assert.equal(tcp.doc_count, 20);
    assert.equal(tcp.srcip.value, 4);
    const [port80] = tcp.field.buckets;
    assert.equal(port80.doc_count, 20);
    assert.deepEqual(port80.field.buckets.map(b => [b.key, b.doc_count]), [['c.com', 9], ['a.com', 6]]);
  });
});
