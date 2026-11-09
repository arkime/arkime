/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

The ClickHouse sessions backend's scroll, sessions iterator and field map, with query() answered from rows in memory
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { EventEmitter } from 'node:events';

const cjsRequire = createRequire(import.meta.url);
const DbCHImpl = cjsRequire('../viewer/db.ch.js');
const EsQuery = cjsRequire('../viewer/esQuery.js');

// lastPacket repeats so the _id tiebreak matters, port is a permutation so a port sort differs from both
const makeRows = (n) => Array.from({ length: n }, (_, i) => ({ id: `id${String(i).padStart(5, '0')}`, lastPacketMs: 1000 + Math.floor(i / 3), port: (i * 3) % n }));
const ROWS = makeRows(25);

class StubCH extends DbCHImpl {
  sqls = [];
  rows = ROWS;
  async query (sql) {
    if (sql.startsWith('SELECT count() AS count')) { return { data: [{ count: this.rows.length }] }; }
    if (!sql.includes('_dayIndex')) { return { data: [] }; }
    this.sqls.push(sql);
    const order = / ORDER BY (.*) LIMIT /.exec(sql)[1];
    const asc = !order.split(', ')[0].endsWith(' DESC');
    const byTime = (a, b) => (a.lastPacketMs - b.lastPacketMs) || (a.id < b.id ? -1 : a.id > b.id ? 1 : 0);
    const cmp = order.split(', ')[0].includes('lastPacket') ? byTime : (a, b) => a.port - b.port;
    let rows = [...this.rows].sort((a, b) => asc ? cmp(a, b) : cmp(b, a));
    const after = /\) ([<>]) \(fromUnixTimestamp64Milli\((\d+)\), '([^']*)'\)/.exec(sql);
    if (after) {
      const key = { lastPacketMs: Number(after[2]), id: after[3] };
      rows = rows.filter(r => after[1] === '>' ? byTime(r, key) > 0 : byTime(r, key) < 0);
    }
    const total = rows.length;
    const limit = / LIMIT (\d+)(?: OFFSET (\d+))?/.exec(sql);
    const offset = Number(limit[2] ?? 0);
    rows = rows.slice(offset, offset + Number(limit[1]));
    const data = rows.map(r => ({ id: r.id, lastPacket: new Date(r.lastPacketMs).toISOString() }));
    return sql.includes('exact_rows_before_limit') ? { data, rows_before_limit_at_least: total } : { data };
  }
}

function newCH (info) {
  return new StubCH({ sessionsDbUrl: 'clickhouse://127.0.0.1:1/arkime', prefix: 'tests_', ...info });
}

const byPort = (rows) => [...rows].sort((a, b) => a.port - b.port).map(r => r.id);
const byTimeDesc = (rows) => rows.map(r => r.id).reverse();

async function iterateAll (ch, query) {
  const pages = [];
  for await (const page of ch.searchSessionsIterator('tests_sessions3-*', { _source: ['_id'], query: { match_all: {} }, ...query })) {
    pages.push({ total: page.hits.total, ids: page.hits.hits.map(h => h._id) });
  }
  return pages;
}

async function scrollAll (ch, size, order) {
  const query = { size, _source: ['_id'], sort: [{ lastPacket: { order } }], query: { match_all: {} } };
  let res = await ch.searchScroll('tests_sessions3-*', query, { scroll: '1m' });
  const pages = [];
  while (true) {
    pages.push(res.hits.hits.map(h => h._id));
    assert.equal(res.hits.total, ROWS.length);
    if (res._scroll_id === EsQuery.SCROLL_DONE) { return pages; }
    assert.ok(ch.ownsScroll(res._scroll_id));
    res = await ch.scroll(res._scroll_id);
  }
}

describe('ClickHouse scroll', () => {
  test('pages of the requested size cover every row once, in order', async () => {
    const ch = newCH();
    const asc = await scrollAll(ch, 10, 'asc');
    assert.deepEqual(asc.map(p => p.length), [10, 10, 5]);
    assert.deepEqual(asc.flat(), ROWS.map(r => r.id));
    const desc = await scrollAll(ch, 5, 'desc');
    assert.deepEqual(desc.map(p => p.length), [5, 5, 5, 5, 5, 0]);
    assert.deepEqual(desc.flat(), ROWS.map(r => r.id).reverse());
    assert.equal(ch.scrolls.size, 0);
    // only the first page asks for the exact total
    assert.equal(ch.sqls.filter(s => s.includes('exact_rows_before_limit')).length, 2);
    ch.close();
  });

  test('clearScroll and a missing scroll', async () => {
    const ch = newCH();
    const res = await ch.searchScroll('tests_sessions3-*', { size: 3, query: { match_all: {} } }, { scroll: '1m' });
    assert.equal(ch.scrolls.size, 1);
    await ch.clearScroll(res._scroll_id);
    assert.equal(ch.scrolls.size, 0);
    await assert.rejects(ch.scroll(res._scroll_id), { statusCode: 404 });
    assert.equal(ch.ownsScroll('some-es-scroll-id'), false);
    ch.close();
  });

  test('a page is at most 10000 rows', async () => {
    const ch = newCH();
    await ch.searchScroll('tests_sessions3-*', { size: 2000000, query: { match_all: {} } }, { scroll: '1m' });
    assert.match(ch.sqls[0], / LIMIT 10000 /);
    ch.close();
  });
});

describe('ClickHouse sessions iterator', () => {
  test('from is honored on the default sort', async () => {
    const ch = newCH();
    // a small window is one plain search
    let pages = await iterateAll(ch, { from: 5, size: 10 });
    assert.deepEqual(pages, [{ total: 25, ids: byTimeDesc(ROWS).slice(5, 15) }]);
    assert.match(ch.sqls[0], / LIMIT 10 OFFSET 5$/);
    // a big one pages by keyset, the offset only on the first page
    ch.sqls = [];
    pages = await iterateAll(ch, { from: 5, size: 20000 });
    assert.deepEqual(pages, [{ total: 25, ids: byTimeDesc(ROWS).slice(5) }]);
    assert.match(ch.sqls[0], / LIMIT 2000 OFFSET 5 SETTINGS exact_rows_before_limit = 1$/);
    ch.rows = makeRows(4100);
    ch.sqls = [];
    pages = await iterateAll(ch, { from: 50, size: 20000, sort: [{ lastPacket: { order: 'asc', missing: '_last' } }] });
    assert.deepEqual(pages.map(p => p.ids.length), [2000, 2000, 50]);
    assert.deepEqual(pages.map(p => p.total), [4100, 4100, 4100]);
    assert.deepEqual(pages.flatMap(p => p.ids), ch.rows.map(r => r.id).slice(50));
    assert.match(ch.sqls[0], /ORDER BY `tests_sessions3`.lastPacket ASC.* OFFSET 50 SETTINGS/);
    assert.doesNotMatch(ch.sqls[1], /OFFSET/);
    assert.match(ch.sqls[1], /\) > \(fromUnixTimestamp64Milli/);
    ch.close();
  });

  test('a custom sort is honored', async () => {
    const ch = newCH();
    const sort = [{ 'source.port': { order: 'asc', missing: '_last' } }];
    let pages = await iterateAll(ch, { from: 3, size: 10, sort });
    assert.deepEqual(pages, [{ total: 25, ids: byPort(ROWS).slice(3, 13) }]);
    ch.sqls = [];
    pages = await iterateAll(ch, { from: 3, size: 20000, sort });
    assert.deepEqual(pages, [{ total: 25, ids: byPort(ROWS).slice(3) }]);
    assert.match(ch.sqls[0], /ORDER BY .*source.*port.* ASC, `tests_sessions3`.`_id` ASC LIMIT 2000 OFFSET 3$/);
    ch.close();
  });

  test('beyond 10000 the iterator pages on, a plain search still refuses', async () => {
    const ch = newCH();
    ch.rows = makeRows(12010);
    const sort = [{ 'source.port': { order: 'desc' } }];
    let pages = await iterateAll(ch, { from: 10002, size: 20000, sort });
    assert.deepEqual(pages.map(p => p.ids.length), [2000, 8]);
    assert.deepEqual(pages.map(p => p.total), [12010, 12010]);
    assert.deepEqual(pages.flatMap(p => p.ids), byPort(ch.rows).reverse().slice(10002));
    assert.match(ch.sqls[0], / OFFSET 10002$/);
    assert.match(ch.sqls[1], / OFFSET 12002$/);
    ch.sqls = [];
    pages = await iterateAll(ch, { from: 10002, size: 20000 });
    assert.deepEqual(pages.flatMap(p => p.ids), byTimeDesc(ch.rows).slice(10002));
    assert.throws(() => ch.translateQuery({ from: 10002, size: 10, query: { match_all: {} } }), /from pagination beyond 10000/);
    ch.close();
  });
});

describe('ClickHouse field map', () => {
  const phrase = { query: { match_phrase: { 'http.uriTokens': 'foo bar' } }, size: 1 };
  test('the constructor fieldsMap types the fields', () => {
    const ch = newCH({ fieldsMap: () => ({ 'http.uriTokens': { type: 'textfield' } }) });
    const { sql } = ch.translateQuery(phrase);
    assert.match(sql, /hasAll\(/);
    assert.doesNotMatch(sql, /position\(/);
    ch.close();
  });

  test('without one the field is an untyped array', () => {
    const ch = newCH();
    const { sql } = ch.translateQuery(phrase);
    assert.match(sql, /position\(/);
    assert.doesNotMatch(sql, /hasAll\(/);
    ch.close();
  });
});

describe('ClickHouse rowsToHits', () => {
  test('empty and null projected values are left out like ES', () => {
    const ch = newCH();
    const [hit] = ch.rowsToHits([{ id: 'a', protocol: ['udp'], tags: [], 'dns.host': [], 'http.uri': null }], ['id', 'protocol', 'tags', 'dns.host', 'http.uri']);
    assert.deepEqual(hit.fields, { protocol: ['udp'] });
    assert.deepEqual(hit._source, { protocol: ['udp'] });
    ch.close();
  });
});

// queryInternal over a fake http module: records each request's query_id and sql, answers with reply(sql)
function newRecordingCH (reply = () => ({ data: [] })) {
  const ch = new DbCHImpl({ sessionsDbUrl: 'clickhouse://127.0.0.1:1/arkime', prefix: 'tests_' });
  ch.requests = [];
  ch.schemaReady = Promise.resolve();
  ch.httpModule = {
    request (url, opts, cb) {
      const req = new EventEmitter();
      req.destroy = (err) => req.emit('error', err);
      req.end = (body) => {
        const sql = body.replace(/\nFORMAT JSON$/, '');
        ch.requests.push({ queryId: url.searchParams.get('query_id'), sql });
        const res = new EventEmitter();
        res.statusCode = 200;
        cb(res);
        setImmediate(() => { res.emit('data', Buffer.from(JSON.stringify(reply(sql)))); res.emit('end'); });
      };
      return req;
    }
  };
  return ch;
}

describe('ClickHouse query ids', () => {
  const tagOf = (queryId) => decodeURIComponent(queryId.slice('arkime::'.length, queryId.lastIndexOf('::')));

  test('every query of a search carries its cancelId, others carry none', async () => {
    const ch = newRecordingCH();
    await ch.searchSessions('tests_sessions3-*', { size: 1, query: { match_all: {} }, aggregations: { p: { terms: { field: 'protocol' } } } }, { cancelId: 'bob::c1' });
    await ch.searchSessions('tests_sessions3-*', { size: 1, query: { match_all: {} } });
    const tagged = ch.requests.filter(r => tagOf(r.queryId) === 'bob::c1');
    assert.ok(tagged.length >= 3, `rows, count and the terms agg are tagged, got ${tagged.length}`);
    const untagged = ch.requests.filter(r => tagOf(r.queryId) === '');
    assert.equal(untagged.length, 2);
    assert.equal(new Set(ch.requests.map(r => r.queryId)).size, ch.requests.length, 'every query id is unique');
    ch.close();
  });

  test('scroll pages keep the cancelId', async () => {
    const rows = Array.from({ length: 3 }, (_, i) => ({ id: `id${i}`, lastPacket: new Date(1000 + i).toISOString() }));
    const ch = newRecordingCH((sql) => sql.includes('LIMIT') ? { data: rows, rows_before_limit_at_least: 9 } : { data: [] });
    const first = await ch.searchScroll('tests_sessions3-*', { size: 3, query: { match_all: {} } }, { scroll: '1m', cancelId: 'amy::s1' });
    await ch.scroll(first._scroll_id);
    assert.ok(ch.requests.length >= 2);
    for (const r of ch.requests) { assert.equal(tagOf(r.queryId), 'amy::s1'); }
    ch.close();
  });

  test('cancel kills the search by its query id prefix', async () => {
    const ch = newRecordingCH(() => ({ data: [{ kill_status: 'waiting' }] }));
    assert.equal(await ch.cancelByOpaqueId("o'brien::c2"), true);
    assert.equal(ch.requests[0].sql, `KILL QUERY WHERE startsWith(query_id, '${'arkime::' + encodeURIComponent("o'brien::c2").replace(/'/g, "\\'") + '::'}') ASYNC`);
    ch.close();
  });

  test('cancel of nothing running is false', async () => {
    const ch = newRecordingCH(() => ({ data: [] }));
    assert.equal(await ch.cancelByOpaqueId('bob::gone'), false);
    ch.close();
  });

  test('adminQueries gives the Arkime user of our queries', async () => {
    const ch = newRecordingCH(() => ({
      data: [
        { query_id: 'arkime::' + encodeURIComponent('bob::c1') + '::u1', user: 'arkime', elapsed: 2.5, read_rows: 10, read_bytes: 100, memory_usage: 1000, is_cancelled: 0, query: 'SELECT 1' },
        { query_id: 'arkime::::u2', user: 'arkime', elapsed: 1, read_rows: 0, read_bytes: 0, memory_usage: 0, is_cancelled: 1, query: 'SELECT 2' },
        { query_id: 'other-tool-1', user: 'default', elapsed: 9, read_rows: 1, read_bytes: 1, memory_usage: 1, is_cancelled: 0, query: 'INSERT' }
      ]
    }));
    const queries = await ch.adminQueries();
    assert.deepEqual(queries.map(q => [q.user, q.dbUser, q.cancelled]), [['bob', 'arkime', false], ['', 'arkime', true], ['', 'default', false]]);
    assert.equal(queries[0].elapsed, 2.5);
    assert.match(ch.requests[0].sql, /FROM system\.processes WHERE query_id != queryID\(\)/);
    ch.close();
  });
});

describe('ClickHouse admin tables', () => {
  const part = (over) => ({ database: 'arkime', table: 'tests_sessions3', pid: '202601', partition: '202601', parts: 2, patchParts: 1, rows: 10, bytes: 100, compressed: 50, uncompressed: 200, indexBytes: 7, memory: 720, first: 1767225600, last: 1767312000, modified: 1767312000, ...over });

  test('lists every partition with its patch parts and real times', async () => {
    const ch = newRecordingCH(() => ({ data: [part(), part({ table: 'other', pid: 'all', partition: 'tuple()', first: 0, last: 0 })] }));
    const tables = await ch.adminTables();
    assert.deepEqual(JSON.parse(tables[0].id), ['arkime', 'tests_sessions3', '202601']);
    assert.equal(tables[0].table, 'arkime.tests_sessions3');
    assert.equal(tables[0].patchParts, 1);
    assert.equal(tables[0].indexBytes, 7);
    assert.equal(tables[0].memory, 720);
    assert.equal(tables[0].first, 1767225600000);
    assert.equal(tables[1].partition, '', 'an unpartitioned table has no partition name');
    assert.equal(tables[1].first, null);
    assert.match(ch.requests[0].sql, /startsWith\(partition_id, 'patch-'\)/);
    assert.match(ch.requests[0].sql, /NOT IN \('system', 'information_schema'\)/);
    assert.match(ch.requests[0].sql, /primary_key_bytes_in_memory_allocated \+ index_granularity_bytes_in_memory_allocated/);
    ch.close();
  });

  test('optimize and drop quote the table and use the partition id', async () => {
    const ch = newRecordingCH((sql) => sql.startsWith('SELECT 1') ? { data: [{ 1: 1 }] } : {});
    const id = JSON.stringify(['arkime', 'we`ird', '202601']);
    assert.equal(await ch.adminTableAction(id, 'optimize'), true);
    assert.equal(await ch.adminTableAction(id, 'drop'), true);
    await new Promise(resolve => setImmediate(resolve));
    const sqls = ch.requests.map(r => r.sql);
    assert.ok(sqls.includes("OPTIMIZE TABLE `arkime`.`we``ird` PARTITION ID '202601' FINAL"));
    assert.ok(sqls.includes("ALTER TABLE `arkime`.`we``ird` DROP PARTITION ID '202601'"));
    ch.close();
  });

  test('only a couple of optimizes run at once, and one partition only once', async () => {
    const ch = newRecordingCH((sql) => sql.startsWith('SELECT 1') ? { data: [{ 1: 1 }] } : {});
    let release;
    const gate = new Promise(resolve => { release = resolve; });
    const query = ch.query.bind(ch);
    ch.query = (sql, opts) => sql.startsWith('OPTIMIZE') ? gate.then(() => query(sql, opts)) : query(sql, opts);
    const id = (pid) => JSON.stringify(['arkime', 'tests_sessions3', pid]);
    assert.equal(await ch.adminTableAction(id('202601'), 'optimize'), true);
    await assert.rejects(ch.adminTableAction(id('202601'), 'optimize'), { statusCode: 409 });
    assert.equal(await ch.adminTableAction(id('202602'), 'optimize'), true);
    await assert.rejects(ch.adminTableAction(id('202603'), 'optimize'), { statusCode: 409 });
    release();
    await new Promise(resolve => setTimeout(resolve, 10));
    assert.equal(await ch.adminTableAction(id('202603'), 'optimize'), true, 'a slot is free once one finishes');
    ch.close();
  });

  test('the partition check only treats patch parts as belonging to another partition', async () => {
    const ch = newRecordingCH(() => ({ data: [] }));
    await ch.adminTableAction(JSON.stringify(['arkime', 't', '2']), 'drop');
    assert.match(ch.requests[0].sql, /partition_id = '2' OR \(startsWith\(partition_id, 'patch-'\) AND substring\(partition_id, 40\) = '2'\)/);
    ch.close();
  });

  test('an unknown partition, bad id or action does nothing', async () => {
    const ch = newRecordingCH(() => ({ data: [] }));
    assert.equal(await ch.adminTableAction(JSON.stringify(['arkime', 'nope', '1']), 'drop'), false);
    assert.equal(await ch.adminTableAction('not json', 'drop'), false);
    assert.equal(await ch.adminTableAction(JSON.stringify(['a', 1, 'b']), 'drop'), false);
    assert.ok(!ch.requests.some(r => /DROP|OPTIMIZE/.test(r.sql)));
    ch.close();
  });
});
