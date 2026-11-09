/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

The ES query DSL as the tree sessions db backends compile
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const EsQuery = createRequire(import.meta.url)('../viewer/esQuery.js');

describe('EsQuery', () => {
  test('leaf clauses', () => {
    assert.deepEqual(EsQuery.parse({ term: { node: 'a' } }), { field: 'node', op: 'eq', value: 'a' });
    assert.deepEqual(EsQuery.parse({ term: { node: { value: 'a' } } }), { field: 'node', op: 'eq', value: 'a' });
    assert.deepEqual(EsQuery.parse({ terms: { tags: ['a', 'b'] } }), { field: 'tags', op: 'in', value: ['a', 'b'] });
    assert.deepEqual(EsQuery.parse({ terms: { 'source.ip': { index: 'lookups', id: 'x', path: 'ip' } } }),
      { field: 'source.ip', op: 'lookup', value: { index: 'lookups', id: 'x', path: 'ip' } });
    assert.deepEqual(EsQuery.parse({ range: { lastPacket: { gte: 1, lt: 5 } } }), { field: 'lastPacket', op: 'range', value: [['gte', 1], ['lt', 5]] });
    assert.deepEqual(EsQuery.parse({ range: { lastPacket: {} } }), { all: true });
    assert.deepEqual(EsQuery.parse({ exists: { field: 'tags' } }), { field: 'tags', op: 'exists' });
    assert.deepEqual(EsQuery.parse({ wildcard: { 'host.http': { value: 'a*' } } }), { field: 'host.http', op: 'wildcard', value: 'a*' });
    assert.deepEqual(EsQuery.parse({ match_phrase: { 'http.uriTokens': { query: 'a b' } } }), { field: 'http.uriTokens', op: 'phrase', value: 'a b' });
    assert.deepEqual(EsQuery.parse({ ids: { values: ['x'] } }), { field: 'id', op: 'in', value: ['x'] });
    assert.deepEqual(EsQuery.parse(undefined), { all: true });
    assert.deepEqual(EsQuery.parse({ match_all: {} }), { all: true });
  });

  test('bool', () => {
    const a = { term: { a: 1 } };
    const b = { term: { b: 2 } };
    const A = { field: 'a', op: 'eq', value: 1 };
    const B = { field: 'b', op: 'eq', value: 2 };
    assert.deepEqual(EsQuery.parse({ bool: { filter: [a, b] } }), { and: [A, B] });
    assert.deepEqual(EsQuery.parse({ bool: { must: a, must_not: b } }), { and: [A, { not: B }] });
    assert.deepEqual(EsQuery.parse({ bool: { should: [a, b] } }), { or: [A, B] });
    // with a filter, should only scores unless minimum_should_match says otherwise
    assert.deepEqual(EsQuery.parse({ bool: { filter: a, should: [b] } }), A);
    assert.deepEqual(EsQuery.parse({ bool: { filter: a, should: [b], minimum_should_match: 1 } }), { and: [A, { or: [B] }] });
    assert.deepEqual(EsQuery.parse({ bool: {} }), { all: true });
    assert.throws(() => EsQuery.parse({ bool: { should: [a, b], minimum_should_match: 2 } }), /minimum_should_match/);
    assert.throws(() => EsQuery.parse({ nested: {} }, { unsupported: (f) => new Error(`no ${f}`) }), /no nested queries/);
  });

  test('query_string', () => {
    const q = (s) => EsQuery.queryString(s);
    assert.deepEqual(q('a:1'), { field: 'a', op: 'eq', value: '1' });
    assert.deepEqual(q('a:1 b:2 AND c:3'), { or: [{ field: 'a', op: 'eq', value: '1' }, { and: [{ field: 'b', op: 'eq', value: '2' }, { field: 'c', op: 'eq', value: '3' }] }] });
    assert.deepEqual(q('NOT a:1 AND (b:x* OR c:"d\\:e")'), {
      and: [{ not: { field: 'a', op: 'eq', value: '1' } }, { or: [{ field: 'b', op: 'wildcard', value: 'x*' }, { field: 'c', op: 'eq', value: 'd:e' }] }]
    });
    assert.deepEqual(q(''), { all: true });
    assert.throws(() => q('free'), /free text/);
    assert.throws(() => q('a:[1 TO 2]'), /advanced syntax/);
    assert.throws(() => q('(a:1'), /syntax/);
  });

  test('dateBound rounds a whole second up for lte and gt like ES', () => {
    assert.equal(EsQuery.dateBound('lte', '2026-01-01T00:00:01Z'), Date.parse('2026-01-01T00:00:01Z') + 999);
    assert.equal(EsQuery.dateBound('gte', '2026-01-01T00:00:01Z'), Date.parse('2026-01-01T00:00:01Z'));
    assert.equal(EsQuery.dateBound('lte', 5), 5);
  });
});
