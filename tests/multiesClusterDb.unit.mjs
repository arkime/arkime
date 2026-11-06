/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for multies answering a multi viewer's cluster db requests from a
cluster's Postgres dbUrl, SQLite stands in since answer only uses the
collection api. Run with `npm run test:unit`.
*/
import { test, describe, before } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const cjsRequire = createRequire(import.meta.url);
const ArkimeDb = cjsRequire('../common/arkimeDb.js');
const MultiesClusterDb = cjsRequire('../viewer/multiesClusterDb.js');

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'multies-clusterdb-'));
const conn = ArkimeDb.connect(`sqlite://${tmp}/cluster.db`, { prefix: 'tests_' });
// Only builds the request bodies the multi viewer would send, never connects
const es = ArkimeDb.connect('http://127.0.0.1:1', { prefix: 'MULTIPREFIX_' });

const answer = (coll, op, id, query, body) =>
  MultiesClusterDb.answer(conn.collection(coll), op, id, query, body === undefined ? undefined : JSON.stringify(body));

describe('multies cluster db', () => {
  before(async () => {
    const files = conn.collection('files');
    await files.put('test-1', { node: 'test', num: 1, name: '/a.pcap', first: 100, locked: 1 });
    await files.put('test-2', { node: 'test', num: 2, name: '/b.pcap', first: 50 });
    await files.put('other-3', { node: 'other', num: 3, name: '/c.pcap', first: 200 });
  });

  test('collectionName', () => {
    assert.equal(MultiesClusterDb.collectionName('/MULTIPREFIX_files/_search'), 'files');
    assert.equal(MultiesClusterDb.collectionName('/MULTIPREFIX_files_v30/_doc/x'), 'files');
    assert.equal(MultiesClusterDb.collectionName('/MULTIPREFIX_history_v1-*/_search'), 'history');
    assert.equal(MultiesClusterDb.collectionName('/MULTIPREFIX_history_v1-25w44/_doc'), 'history');
    // only history has dated indices
    assert.equal(MultiesClusterDb.collectionName('/MULTIPREFIX_queries-history/_doc'), undefined);
    assert.equal(MultiesClusterDb.collectionName('/MULTIPREFIX_hunts-x/_search'), undefined);
    assert.equal(MultiesClusterDb.collectionName('/MULTIPREFIX_stats*/_count'), 'stats');
    assert.equal(MultiesClusterDb.collectionName('/MULTIPREFIX_sessions3-*/_search'), undefined);
    assert.equal(MultiesClusterDb.collectionName('/MULTIPREFIX_*/_refresh'), undefined);
    assert.equal(MultiesClusterDb.collectionName('/MULTIPREFIX_lookups/_search'), undefined);
    assert.equal(MultiesClusterDb.collectionName('/MULTIPREFIX_hunts/_search'), 'hunts');
  });

  test('search', async () => {
    const body = es.collection('files').searchBody({ where: { node: 'test', locked: { $ne: 1 } }, sort: { first: 'asc' } });
    const result = await answer('files', 'search', undefined, { rest_total_hits_as_int: 'true' }, body);
    assert.equal(result.hits.total, 1);
    assert.deepEqual(result.hits.hits.map(h => [h._index, h._id, h._source.name]), [['MULTIPREFIX_files_v30', 'test-2', '/b.pcap']]);

    const all = await answer('files', 'search', undefined, {}, es.collection('files').searchBody({ sort: [['first', 'desc'], ['id', 'asc']], size: 2 }));
    assert.deepEqual(all.hits.total, { value: 3, relation: 'eq' });
    assert.deepEqual(all.hits.hits.map(h => h.sort), [[200, 'other-3'], [100, 'test-1']]);

    const scrolled = await answer('files', 'search', undefined, { scroll: '1m' }, { query: { match_all: {} }, size: 1 });
    assert.equal(scrolled.hits.hits.length, 3);

    const get = await answer('files', 'search', undefined, {});
    assert.equal(get.hits.hits.length, 3);
  });

  test('aggregate', async () => {
    const body = {
      query: { match_all: {} },
      size: 0,
      aggregations: { groups: { terms: { field: 'node', size: 10 }, aggregations: { first: { min: { field: 'first' } } } } }
    };
    const result = await answer('files', 'search', undefined, { rest_total_hits_as_int: 'true' }, body);
    assert.equal(result.hits.total, 3);
    assert.deepEqual(result.aggregations.groups.buckets, [
      { key: 'test', doc_count: 2, first: { value: 50 } },
      { key: 'other', doc_count: 1, first: { value: 200 } }
    ]);
  });

  test('count and get', async () => {
    assert.deepEqual(await answer('files', 'count', undefined, {}, { query: { term: { node: 'test' } } }), { count: 2 });
    assert.deepEqual(await answer('files', 'count', undefined, {}), { count: 3 });
    const found = await answer('files', 'get', 'test-1', {});
    assert.equal(found.found, true);
    assert.equal(found._source.name, '/a.pcap');
    assert.equal((await answer('files', 'get', 'nope', {})).found, false);
  });

  test('writes', async () => {
    assert.equal((await answer('hunts', 'index', 'h1', {}, { name: 'one' })).result, 'created');
    const created = await answer('hunts', 'index', undefined, {}, { name: 'two' });
    assert.ok(created._id);
    assert.equal((await answer('hunts', 'update', 'h1', {}, { doc: { status: 'done' } })).result, 'updated');
    assert.deepEqual(await conn.collection('hunts').get('h1'), { name: 'one', status: 'done' });
    assert.equal((await answer('hunts', 'update', 'nope', {}, { doc: { a: 1 } })).status, 404);
    await assert.rejects(answer('hunts', 'update', 'h1', {}, { script: { source: 'x' } }));
    assert.equal((await answer('hunts', 'delete', 'h1', {})).result, 'deleted');
    assert.equal((await answer('hunts', 'delete', 'h1', {})).result, 'not_found');
    assert.deepEqual(await answer('hunts', 'refresh', undefined, {}), {});
    assert.equal((await answer('history', 'history', undefined, {}, { userId: 'alice' })).result, 'created');
  });

  test('sequence', async () => {
    const first = await answer('sequence', 'index', 'fn-test', {}, {});
    const second = await answer('sequence', 'index', 'fn-test', {}, {});
    assert.equal(second._version, first._version + 1);
  });

  test('unsupported', async () => {
    await assert.rejects(answer('files', undefined, undefined, {}, {}));
    await assert.rejects(answer('files', 'delete', undefined, {}));
    await assert.rejects(answer('files', 'search', undefined, {}, { query: { match: { name: 'x' } } }));
  });

  test('ops come from the route, never the path or id', async () => {
    const hunts = conn.collection('hunts');
    await hunts.put('keep', { name: 'keep' });
    const count = await hunts.count({});
    // a get or HEAD of an id that names an ES endpoint stays a read
    for (const id of ['_delete_by_query', '_update', '_search']) {
      assert.equal((await answer('hunts', 'get', id, {})).found, false);
    }
    await assert.rejects(answer('hunts', '_delete_by_query', undefined, {}, { query: { match_all: {} } }));
    await assert.rejects(answer('hunts', 'update', undefined, {}, { doc: { a: 1 } }));
    // the history route only writes history
    await assert.rejects(answer('hunts', 'history', undefined, {}, { name: 'forged' }));
    await assert.rejects(answer('queries', 'history', undefined, {}, { creator: 'admin' }));
    assert.equal(await hunts.count({}), count);
    assert.equal(await conn.collection('queries').count({}), 0);
  });
});
