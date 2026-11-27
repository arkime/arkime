/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

The ClickHouse sessions backend's scroll, sessions iterator and field map, with query() answered from rows in memory

Set ARKIME_TEST_CH_URL=http://localhost:8123 to also run the shortcut lookup and batched msearch SQL against a real ClickHouse
*/
import { test, describe, before as beforeAll, after as afterAll } from 'node:test';
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
        ch.requests.push({ queryId: url.searchParams.get('query_id'), sql, timeout: opts.timeout });
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
    assert.equal(ch.requests.find(r => r.sql.startsWith('OPTIMIZE')).timeout, 0, 'an optimize is never timed out');
    assert.ok(ch.requests.find(r => r.sql.startsWith('ALTER')).timeout > 0);
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

describe('ClickHouse shortcut ip lookups', { skip: !process.env.ARKIME_TEST_CH_URL }, () => {
  const url = process.env.ARKIME_TEST_CH_URL;
  const db = `arkime_unit_lookups_${process.pid}`;
  const q = async (sql) => {
    const res = await fetch(url, { method: 'POST', body: sql });
    const text = await res.text();
    if (!res.ok) { throw new Error(text); }
    return text.trim();
  };
  // Only the SQL is built here, it runs against ARKIME_TEST_CH_URL
  const ch = new StubCH({ sessionsDbUrl: `clickhouse://127.0.0.1:1/${db}`, prefix: 't_' });
  ch.columnForField = () => 'col';
  // [== $id, != $id] for a session whose ip is value, != negated the way compileNode does
  const match = (id, value) => {
    ch.isJsonArrayField = () => Array.isArray(value);
    const pred = ch.termsLookupPredicate('source.ip', { id, path: 'ip' });
    const col = Array.isArray(value) ? `[${value.map(v => `'${v}'`).join(', ')}]` : `'${value}'`;
    return q(`SELECT ${pred}, NOT ifNull((${pred}), 0) FROM (SELECT ${col} AS col) FORMAT TSV`);
  };

  beforeAll(async () => {
    await q(`CREATE DATABASE ${db}`);
    await q(`CREATE TABLE ${db}.t_lookups (id String, updated DateTime DEFAULT now(), ip Array(String), string Array(String), number Array(Float64)) ENGINE = ReplacingMergeTree(updated) ORDER BY (id)`);
    await q(`INSERT INTO ${db}.t_lookups (id, ip) VALUES ('nets', ['10.1.0.0/16', '192.168.1.5', '2001:db8::/32']), ('notip', ['10.1.0.0/16', 'nonsense']), ('badbits', ['10.1.0.0/40'])`);
  });
  afterAll(() => q(`DROP DATABASE IF EXISTS ${db}`));

  test('a CIDR matches its whole range, so != excludes it', async () => {
    assert.equal(await match('nets', '10.1.2.3'), '1\t0');
    assert.equal(await match('nets', '::ffff:10.1.9.9'), '1\t0');
    assert.equal(await match('nets', '2001:db8:1::1'), '1\t0');
    assert.equal(await match('nets', '10.2.0.1'), '0\t1');
    assert.equal(await match('nets', '2001:db9::1'), '0\t1');
  });

  test('an exact address only matches itself', async () => {
    assert.equal(await match('nets', '192.168.1.5'), '1\t0');
    assert.equal(await match('nets', '192.168.1.6'), '0\t1');
  });

  test('an array field matches when any element does', async () => {
    assert.equal(await match('nets', ['10.9.9.9', '10.1.0.7']), '1\t0');
    assert.equal(await match('nets', ['10.9.9.9']), '0\t1');
  });

  test('a value that is not an address or CIDR, or a missing shortcut, throws', async () => {
    await assert.rejects(match('notip', '10.1.2.3'), /shortcut ip is not an address or CIDR/);
    await assert.rejects(match('badbits', '10.1.2.3'), /shortcut ip is not an address or CIDR/);
    await assert.rejects(match('missing', '10.1.2.3'), /shortcut not found in lookups table/);
  });
});

// Answers the batched spigraph SQL and the per-value searches from canned rows, recording each statement
class MsearchCH extends DbCHImpl {
  sqls = [];
  fail = 0;
  async query (sql) {
    (this.sqls ??= []).push(sql);
    await new Promise(resolve => setImmediate(resolve));
    if (this.fail > 0 && sql.startsWith('SELECT count() AS count FROM `arkime`.`tests_sessions3`')) { this.fail--; throw new Error('down'); }
    if (sql.includes('AS __v') && sql.includes('GROUP BY __v, key')) {
      return { data: [{ __v: 0, key: 60000, doc_count: 2, 'source.bytes': 10 }, { __v: 1, key: 0, doc_count: 1, 'source.bytes': 5 }, { __v: 1, key: 60000, doc_count: 3, 'source.bytes': 7 }] };
    }
    if (sql.includes('AS __v')) { return { data: [{ __v: 0, __count: 2, p95: 100 }, { __v: 1, __count: 4, p95: 50 }] }; }
    if (sql.startsWith('SELECT count() AS count')) { return { data: [{ count: 7 }] }; }
    return { data: [] };
  }
}

describe('ClickHouse msearch', () => {
  const newCHs = async () => {
    const ch = new MsearchCH({ sessionsDbUrl: 'clickhouse://127.0.0.1:1/arkime', prefix: 'tests_' });
    await ch.schemaReady;
    ch.sqls = [];
    return ch;
  };
  // spigraph's per-value searches: the same query with one more term filter
  const search = (extra = {}) => ({
    size: 0,
    query: { bool: { filter: [{ range: { lastPacket: { gte: 0 } } }] } },
    aggregations: {
      dbHisto: { histogram: { field: 'lastPacket', interval: 60000, min_doc_count: 1 }, aggregations: { 'source.bytes': { sum: { field: 'source.bytes' } } } },
      p95: { percentiles: { field: 'network.bytes', percents: [95] } },
      low: { min: { field: 'network.bytes' } }
    },
    ...extra
  });
  const perValue = (field, values, extra) => values.map((value) => {
    const query = search(extra);
    query.query.bool.filter.push({ term: { [field]: value } });
    return JSON.stringify(query);
  });

  test('one search per value of a field runs as one count and one histogram scan', async () => {
    const ch = await newCHs();
    const { body: { responses } } = await ch.msearchSessions('tests_sessions3-*', perValue('destination.ip', ['10.0.0.1', '10.0.0.2', '10.0.0.3']));
    assert.equal(ch.sqls.length, 2);
    assert.ok(ch.sqls.every(sql => sql.includes("indexOf([toIPv6('10.0.0.1'), toIPv6('10.0.0.2'), toIPv6('10.0.0.3')], toIPv6OrNull(toString(`destination.ip`))) - 1 AS __v")));
    assert.ok(ch.sqls.some(sql => sql.includes(' LIMIT 65537 BY __v')));
    assert.deepEqual(responses[0], {
      hits: { total: 2, hits: [] },
      aggregations: { p95: { value: 100 }, low: { value: null }, dbHisto: { buckets: [{ key: 60000, doc_count: 2, 'source.bytes': { value: 10 } }] } }
    });
    assert.deepEqual(responses[1].aggregations.dbHisto.buckets.map(b => [b.key, b.doc_count, b['source.bytes'].value]), [[0, 1, 5], [60000, 3, 7]]);
    assert.equal(responses[1].hits.total, 4);
    // a value no row matches answers like its own empty search
    assert.deepEqual(responses[2], { hits: { total: 0, hits: [] }, aggregations: { p95: { value: 0 }, low: { value: null }, dbHisto: { buckets: [] } } });
  });

  test('values that aren\'t a plain equality are each compared on their own in the one scan', async () => {
    for (const [field, values] of [['destination.ip', ['10.0.0', '10.0.1.0/24']], ['destination.port', ['x', 80]], ['node', ['a', 'a']], ['http.uriTokens', ['a', 'b']]]) {
      const ch = await newCHs();
      ch.fieldsMap = () => ({ 'http.uriTokens': { type: 'textfield' } });
      await ch.msearchSessions('tests_sessions3-*', perValue(field, values));
      assert.equal(ch.sqls.length, 2, field);
      assert.ok(ch.sqls.every(sql => sql.includes('arrayJoin(arrayFilter(i -> i >= 0, [if(') && !sql.includes('indexOf(')), field);
    }
  });

  test('anything a GROUP BY on the value might answer differently runs one search per value', async () => {
    const twoTerms = perValue('destination.port', [80, 443]).map((query, i) => {
      const parsed = JSON.parse(query);
      parsed.query.bool.filter.splice(1, 0, { term: { 'destination.ip': `10.0.0.${i}` } });
      return JSON.stringify(parsed);
    });
    const withTerms = perValue('destination.ip', ['10.0.0.1', '10.0.0.2'], { aggregations: { mapG1: { terms: { field: 'source.geo.country_iso_code', size: 10 } } } });
    const withHits = perValue('destination.ip', ['10.0.0.1', '10.0.0.2'], { size: 5 });
    const packetRange = perValue('destination.ip', ['10.0.0.1', '10.0.0.2'], { aggregations: { dbHisto: { histogram: { field: 'packetRange', interval: 60000 } } } });
    for (const [what, queries] of [['two terms differ', twoTerms], ['terms agg', withTerms], ['hits', withHits], ['packetRange', packetRange], ['one value', perValue('destination.ip', ['10.0.0.1'])]]) {
      const ch = await newCHs();
      const { body: { responses } } = await ch.msearchSessions('tests_sessions3-*', queries);
      assert.equal(responses.length, queries.length, what);
      assert.ok(!ch.sqls.some(sql => sql.includes('__v')), what);
      assert.equal(ch.sqls.filter(sql => sql.startsWith('SELECT count() AS count FROM `arkime`.`tests_sessions3` WHERE')).length, queries.length, what);
    }
  });

  test('searches that can\'t be batched run a few at a time', async () => {
    const ch = await newCHs();
    let inFlight = 0;
    let most = 0;
    const searchSessions = ch.searchSessions.bind(ch);
    ch.searchSessions = async (...args) => {
      most = Math.max(most, ++inFlight);
      try { return await searchSessions(...args); } finally { inFlight--; }
    };
    const values = Array.from({ length: 30 }, (_, i) => `10.0.0.${i}`);
    const { body: { responses } } = await ch.msearchSessions('tests_sessions3-*', perValue('destination.ip', values, { size: 5 }));
    assert.equal(responses.length, 30);
    assert.ok(most > 1 && most <= 8, `most in flight ${most}`);
  });

  test('numberOfDocuments shares one count between callers for a few seconds and retries after an error', async () => {
    const ch = await newCHs();
    const total = 'SELECT count() AS count FROM `arkime`.`tests_sessions3`';
    const [a, b] = await Promise.all([ch.numberOfDocuments(), ch.numberOfDocuments()]);
    assert.deepEqual([a, b], [{ count: 7 }, { count: 7 }]);
    assert.deepEqual(await ch.numberOfDocuments(), { count: 7 });
    assert.equal(ch.sqls.filter(sql => sql === total).length, 1);

    const failing = await newCHs();
    failing.fail = 1;
    await assert.rejects(failing.numberOfDocuments(), /down/);
    assert.deepEqual(await failing.numberOfDocuments(), { count: 7 });
  });
});

// Each value's answer from the one batched scan matches that value's own search, both on ARKIME_TEST_CH_URL
describe('ClickHouse msearch on a real ClickHouse', { skip: !process.env.ARKIME_TEST_CH_URL }, () => {
  const url = process.env.ARKIME_TEST_CH_URL;
  const db = `arkime_unit_msearch_${process.pid}`;
  const q = async (sql) => {
    const res = await fetch(url, { method: 'POST', body: sql });
    const text = await res.text();
    if (!res.ok) { throw new Error(text); }
    return text.trim();
  };
  const ips = ['10.0.0.1', '10.0.0.2', 'fe80::1', '2001:db8::1', '10.0.1.7', undefined];
  const protocols = [['tcp'], ['udp', 'dns'], ['tcp', 'http'], ['udp'], []];
  const rows = Array.from({ length: 60 }, (_, i) => ({
    _id: `id${i}`,
    lastPacket: 1700000000000 + i * 17000,
    node: ['n1', 'n2', 'n3'][i % 3],
    'source.bytes': i * 10,
    protocol: protocols[i % 5],
    'source.ip': ips[i % 6],
    ...(i % 7 ? { 'destination.port': [80, 443, 53][i % 3] } : {}),
    ...(i % 4 ? { 'http.method': ['GET', 'POST'][i % 2], 'http.statuscode': [[200], [404, 200]][i % 2] } : {}),
    ...(i % 3 ? { tags: ['a', 'b', 'c'].slice(i % 2, 3) } : {}),
    ...(i % 5 ? { 'email.ip': [['10.0.0.1'], ['::1', '10.0.0.1'], ['0:0:0:0:0:0:0:1']][i % 3] } : {})
  }));
  let ch;
  let sqls;

  beforeAll(async () => {
    await q(`CREATE DATABASE ${db}`);
    await q(`CREATE TABLE ${db}.t_sessions3 (
      fields JSON(\`_id\` String, lastPacket Nullable(Int64), \`destination.port\` Nullable(Int64), \`source.bytes\` Nullable(Int64), protocol Array(String), \`http.statuscode\` Array(Int64)),
      \`_id\` String DEFAULT fields.\`_id\`,
      node LowCardinality(String) MATERIALIZED ifNull(fields.node, ''),
      lastPacket DateTime64(3, 'UTC') DEFAULT fromUnixTimestamp64Milli(ifNull(fields.lastPacket, 0)),
      \`source.ip\` String MATERIALIZED ifNull(fields.\`source.ip\`, ''),
      \`destination.ip\` String MATERIALIZED ifNull(fields.\`destination.ip\`, '')
    ) ENGINE = MergeTree ORDER BY lastPacket`);
    await q(`INSERT INTO ${db}.t_sessions3 (fields) FORMAT JSONEachRow\n${rows.map(fields => JSON.stringify({ fields })).join('\n')}`);
    ch = new DbCHImpl({ sessionsDbUrl: `${url}/${db}`, prefix: 't_', fieldsMap: () => ({ 'email.ip': { type: 'ip' } }) });
    await ch.schemaReady;
    const query = ch.query.bind(ch);
    ch.query = (sql, ...args) => { sqls.push(sql); return query(sql, ...args); };
  });
  afterAll(async () => {
    ch?.close();
    await q(`DROP DATABASE IF EXISTS ${db}`);
  });

  const perValue = (field, values) => values.map(value => ({
    size: 0,
    query: { bool: { filter: [{ range: { lastPacket: { gte: 1700000100000 } } }, { term: { [field]: value } }] } },
    aggregations: {
      dbHisto: { histogram: { field: 'lastPacket', interval: 60000, min_doc_count: 1 }, aggregations: { 'source.bytes': { sum: { field: 'source.bytes' } } } },
      bytes: { sum: { field: 'source.bytes' } },
      low: { min: { field: 'destination.port' } }
    }
  }));

  for (const [field, values, lookup] of [
    ['node', ['n1', 'n3', 'none'], true],
    ['destination.port', [80, 443, 53, 22], true],
    ['destination.port', ['80', '443'], true],
    ['protocol', ['tcp', 'udp', 'dns', 'none'], true],
    ['tags', ['a', 'b', 'c'], true],
    ['http.method', ['GET', 'POST', 'none'], true],
    ['http.statuscode', [200, 404, 500], true],
    ['source.ip', ['10.0.0.1', 'fe80:0:0:0:0:0:0:1', '2001:db8::1', '10.0.0.9'], true],
    ['email.ip', ['10.0.0.1', '::1'], true],
    ['source.ip', ['10.0.0', '10.0.1.0/24'], false]
  ]) {
    test(`${field} ${JSON.stringify(values)}`, async () => {
      const queries = perValue(field, values);
      sqls = [];
      const { body: { responses } } = await ch.msearchSessions('t_sessions3-*', queries.map(query => JSON.stringify(query)));
      assert.equal(sqls.length, 2);
      assert.equal(sqls.every(sql => sql.includes('indexOf(')), lookup);
      const own = await Promise.all(queries.map(query => ch.searchSessions('t_sessions3-*', query)));
      assert.ok(own.some(response => response.hits.total > 0));
      assert.deepEqual(responses, own.map(response => ({ hits: response.hits, aggregations: response.aggregations })));
    });
  }

  test('several percentiles in one aggregation answer as each one alone, ranked by the one asked for', async () => {
    const pctAgg = (percents) => ({ percentiles: { field: 'source.bytes', percents, hdr: { number_of_significant_value_digits: 2 } } });
    const search = (aggs, order) => ({ size: 0, query: { match_all: {} }, aggregations: { t: { terms: { field: 'node', size: 3, ...(order ? { order } : {}) }, aggregations: aggs } } });
    const all = await ch.searchSessions('t_sessions3-*', search({ m0: pctAgg([95, 25, 50, 75]) }, { 'm0[95.0]': 'asc' }));
    const buckets = all.aggregations.t.buckets;
    assert.equal(buckets.length, 3);
    const p95s = buckets.map(b => b.m0.values['95.0']);
    assert.deepEqual(p95s, [...p95s].sort((a, b) => a - b), 'ranked by the p95 ascending');
    for (const p of [25, 50, 75, 95]) {
      const alone = await ch.searchSessions('t_sessions3-*', search({ m0: pctAgg([p]) }));
      const byNode = Object.fromEntries(alone.aggregations.t.buckets.map(b => [b.key, b.m0.value]));
      for (const b of buckets) { assert.equal(b.m0.values[p.toFixed(1)], byNode[b.key], `p${p} of ${b.key}`); }
    }
  });
});

// a box plot asks for its quartiles beside the percentile it ranks by, in one
// percentiles aggregation; the translator answers one percent at a time
class PercentileCH extends DbCHImpl {
  sqls = [];
  async query (sql) {
    this.sqls.push(sql);
    if (sql.startsWith('SELECT count() AS count')) { return { data: [{ count: 3 }] }; }
    if (sql.includes('GROUP BY key')) {
      return { data: [{ key: 'tcp', doc_count: 2, m0__pct0: 900, m0__pct1: 100, m0__pct2: 300, m0__pct3: 700, m0Mn: 50, m0Mx: 1000 }], totals: { doc_count: 2 } };
    }
    if (sql.includes('quantileTDigest')) { return { data: [{ all__pct0: 10, all__pct1: 20 }] }; }
    return { data: [] };
  }
}

describe('ClickHouse several percentiles in one aggregation', () => {
  const newCHp = async () => {
    const ch = new PercentileCH({ sessionsDbUrl: 'clickhouse://127.0.0.1:1/arkime', prefix: 'tests_' });
    await ch.schemaReady;
    ch.sqls = [];
    return ch;
  };
  const pct = (percents) => ({ percentiles: { field: 'network.bytes', percents, hdr: { number_of_significant_value_digits: 2 } } });

  test('each percent is its own quantile, ordered by the one asked for, answered as ES values', async () => {
    const ch = await newCHp();
    const query = {
      size: 0,
      query: { match_all: {} },
      aggregations: {
        t: {
          terms: { field: 'protocol', size: 5, order: { 'm0[95.0]': 'desc' } },
          aggregations: { m0: pct([95, 25, 50, 75]), m0Mn: { min: { field: 'network.bytes' } }, m0Mx: { max: { field: 'network.bytes' } } }
        }
      }
    };
    const before = JSON.stringify(query);
    const res = await ch.searchSessions('tests_sessions3-*', query);
    assert.equal(JSON.stringify(query), before, 'the caller\'s query is left as it was');
    const sql = ch.sqls.find(s => s.includes('GROUP BY key'));
    for (const [i, level] of [[0, 0.95], [1, 0.25], [2, 0.5], [3, 0.75]]) {
      assert.ok(sql.includes(`quantileTDigest(${level})`) && sql.includes(`AS \`m0__pct${i}\``), `percent ${level}`);
    }
    assert.ok(sql.includes('ORDER BY `m0__pct0` DESC'));
    assert.deepEqual(res.aggregations.t.buckets[0], {
      key: 'tcp',
      doc_count: 2,
      m0: { values: { '95.0': 900, '25.0': 100, '50.0': 300, '75.0': 700 } },
      m0Mn: { value: 50 },
      m0Mx: { value: 1000 }
    });
  });

  test('a top level one and a single percent', async () => {
    const ch = await newCHp();
    const res = await ch.searchSessions('tests_sessions3-*', { size: 0, query: { match_all: {} }, aggregations: { all: pct([50, 99.9]) } });
    assert.deepEqual(res.aggregations.all, { values: { '50.0': 10, '99.9': 20 } });
    ch.sqls = [];
    await ch.searchSessions('tests_sessions3-*', { size: 0, query: { match_all: {} }, aggregations: { one: pct([95]) } });
    assert.ok(ch.sqls.some(s => s.includes('AS `one`')), 'a single percent is not split');
  });
});

// a terms aggregation's include list (the summary's Top IPs counting just its
// candidates) limits the buckets to those values
describe('ClickHouse terms include', () => {
  const newCHi = async () => {
    const ch = new PercentileCH({ sessionsDbUrl: 'clickhouse://127.0.0.1:1/arkime', prefix: 'tests_' });
    await ch.schemaReady;
    ch.sqls = [];
    return ch;
  };
  const termsSql = (ch) => ch.sqls.find(s => s.includes('GROUP BY key'));

  test("a field's values: its own IN predicate, an address normalized like its ip queries", async () => {
    const ch = await newCHi();
    await ch.searchSessions('tests_sessions3-*', { size: 0, query: { match_all: {} }, aggregations: { t: { terms: { field: 'source.ip', size: 2, include: ['10.0.0.1', '2001:db8::1'] } } } });
    const sql = termsSql(ch);
    const where = sql.slice(sql.indexOf(' WHERE '), sql.indexOf(' GROUP BY '));
    assert.ok(where.includes("'10.0.0.1'"), where);
    assert.ok(where.includes("toIPv6OrNull('2001:db8:0:0:0:0:0:1')"), 'an IPv6 address compares normalized');
    assert.ok(!sql.includes('key IN'), 'not as a key filter');
  });

  test("a script's keys: filtered once made", async () => {
    const ch = await newCHi();
    const script = "if (doc['source.ip'].size() == 0) { return []; } return [doc['source.ip'].value, doc['destination.ip'].value];";
    await ch.searchSessions('tests_sessions3-*', { size: 0, query: { match_all: {} }, aggregations: { t: { terms: { script: { source: script, lang: 'painless' }, size: 2, include: ['10.0.0.1'] } } } });
    assert.ok(termsSql(ch).includes("AND key IN ('10.0.0.1')"));
    assert.ok(/arrayJoin\(\[`source\.ip`, if\(`destination\.ip` = `source\.ip`, '', `destination\.ip`\)\]\)/.test(termsSql(ch)), 'a session from an address to itself counts once');
  });

  test('an empty list matches nothing, a pattern is refused', async () => {
    const ch = await newCHi();
    await ch.searchSessions('tests_sessions3-*', { size: 0, query: { match_all: {} }, aggregations: { t: { terms: { field: 'tags', size: 2, include: [] } } } });
    assert.ok(termsSql(ch).includes('AND 0'));
    await assert.rejects(ch.searchSessions('tests_sessions3-*', { size: 0, query: { match_all: {} }, aggregations: { t: { terms: { field: 'protocol', size: 2, include: 'tc.*' } } } }), /include pattern/);
  });
});

// a terms sub-aggregation of listed values (the summary's Top IPs counting
// each source's sessions by destination) is one grouped query for every
// bucket, not a query per bucket
class PairCH extends DbCHImpl {
  sqls = [];
  async query (sql) {
    this.sqls.push(sql);
    if (sql.startsWith('SELECT count() AS count')) { return { data: [{ count: 9 }] }; }
    if (sql.includes('GROUP BY key, subkey')) {
      return {
        data: [
          { key: '10.0.0.1', subkey: '10.0.0.2', doc_count: 3, m0: 30 },
          { key: '10.0.0.1', subkey: '10.0.0.1', doc_count: 1, m0: 10 },
          { key: '10.0.0.2', subkey: '10.0.0.1', doc_count: 2, m0: 20 },
          { key: '10.0.0.9', subkey: '10.0.0.1', doc_count: 5, m0: 50 }
        ]
      };
    }
    if (sql.includes('GROUP BY key')) {
      return { data: [{ key: '10.0.0.1', doc_count: 4, m0: 40 }, { key: '10.0.0.2', doc_count: 2, m0: 20 }, { key: '10.0.0.3', doc_count: 1, m0: 5 }], totals: { doc_count: 7 } };
    }
    return { data: [] };
  }
}

describe('ClickHouse terms of listed values under terms', () => {
  const ips = ['10.0.0.1', '10.0.0.2', '10.0.0.3'];
  const bytes = { m0: { sum: { field: 'network.bytes' } } };
  const search = async (pair, fieldsMap) => {
    const ch = new PairCH({ sessionsDbUrl: 'clickhouse://127.0.0.1:1/arkime', prefix: 'tests_', ...(fieldsMap ? { fieldsMap: () => fieldsMap } : {}) });
    await ch.schemaReady;
    ch.sqls = [];
    const res = await ch.searchSessions('tests_sessions3-*', {
      size: 0,
      query: { match_all: {} },
      aggregations: { side0: { terms: { field: 'source.ip', size: 3, include: ips }, aggs: { ...bytes, pair } } }
    });
    return { ch, buckets: res.aggregations.side0.buckets, grouped: ch.sqls.filter(s => s.includes('GROUP BY key')) };
  };

  test('one query for every bucket, its rows handed to their buckets in order', async () => {
    const { buckets, grouped } = await search({ terms: { field: 'destination.ip', size: 3, include: ips }, aggs: bytes });
    assert.equal(grouped.length, 2, 'the terms and its pairs');
    const sql = grouped.find(s => s.includes('GROUP BY key, subkey'));
    assert.ok(sql.includes('`source.ip` AS key, `destination.ip` AS subkey, count() AS doc_count, '), sql);
    const where = sql.slice(sql.indexOf(' WHERE '), sql.indexOf(' GROUP BY '));
    for (const col of ['`source.ip`', '`destination.ip`']) {
      assert.ok(ips.every(ip => new RegExp(`${col}[^\`]*'${ip}'`).test(where)), `${col} is limited to the listed values`);
    }
    assert.ok(sql.includes('ORDER BY doc_count DESC, toIPv6OrNull(subkey) ASC'), 'ordered as a terms aggregation of its own');
    assert.deepEqual(buckets.map(b => b.pair.buckets), [
      [{ key: '10.0.0.2', doc_count: 3, m0: { value: 30 } }, { key: '10.0.0.1', doc_count: 1, m0: { value: 10 } }],
      [{ key: '10.0.0.1', doc_count: 2, m0: { value: 20 } }],
      []
    ]);
    assert.ok(buckets.every(b => b.pair.sum_other_doc_count === 0 && b.m0.value > 0));
  });

  test('ordered by a metric or by key when asked', async () => {
    const byMetric = await search({ terms: { field: 'destination.ip', size: 3, include: ips, order: { m0: 'asc' } }, aggs: bytes });
    assert.ok(byMetric.grouped[1].includes('ORDER BY `m0` ASC, toIPv6OrNull(subkey) ASC'));
    const byKey = await search({ terms: { field: 'destination.port', size: 2, include: [80, 443], order: { _key: 'desc' } } });
    assert.ok(byKey.grouped[1].includes('ORDER BY subkey DESC, subkey ASC'));
  });

  test('an empty list has no buckets and no query', async () => {
    const { buckets, grouped } = await search({ terms: { field: 'destination.ip', size: 3, include: [] }, aggs: bytes });
    assert.equal(grouped.length, 1);
    assert.ok(buckets.every(b => b.pair.buckets.length === 0));
  });

  test('anything else is still a query per bucket', async () => {
    const cases = {
      'no list': { terms: { field: 'destination.ip', size: 3 }, aggs: bytes },
      'no room for the list': { terms: { field: 'destination.ip', size: 2, include: ips }, aggs: bytes },
      'a field with several values a session': { terms: { field: 'tags', size: 3, include: ['a', 'b'] }, aggs: bytes },
      'a sub-aggregation that is not a metric': { terms: { field: 'destination.ip', size: 3, include: ips }, aggs: { t: { terms: { field: 'node', size: 2 } } } }
    };
    for (const [what, pair] of Object.entries(cases)) {
      const { grouped } = await search(pair);
      assert.ok(!grouped.some(s => s.includes('GROUP BY key, subkey')), what);
      assert.ok(grouped.length >= 4, `${what}: the terms and a query for each of its 3 buckets`);
    }
  });
});

// The one grouped query's pairs are what each bucket's own search counts, on ARKIME_TEST_CH_URL
describe('ClickHouse terms of listed values under terms on a real ClickHouse', { skip: !process.env.ARKIME_TEST_CH_URL }, () => {
  const url = process.env.ARKIME_TEST_CH_URL;
  const db = `arkime_unit_pairs_${process.pid}`;
  const q = async (sql) => {
    const res = await fetch(url, { method: 'POST', body: sql });
    const text = await res.text();
    if (!res.ok) { throw new Error(text); }
    return text.trim();
  };
  const ips = ['10.0.0.1', '10.0.0.2', 'fe80::1', '2001:db8::1', '10.0.1.7'];
  const rows = Array.from({ length: 90 }, (_, i) => ({
    _id: `id${i}`,
    lastPacket: 1700000000000 + i * 17000,
    'network.bytes': 10 + (i * 7) % 50,
    'source.ip': ips[i % 5],
    ...(i % 11 ? { 'destination.ip': ips[(i * 3 + (i % 4)) % 5] } : {})
  }));
  const listed = ['10.0.0.1', '10.0.0.2', 'fe80::1', '2001:db8::1'];
  const metrics = { m0: { sum: { field: 'network.bytes' } }, m0Mn: { min: { field: 'network.bytes' } } };
  let ch;
  let sqls;

  beforeAll(async () => {
    await q(`CREATE DATABASE ${db}`);
    await q(`CREATE TABLE ${db}.t_sessions3 (
      fields JSON(\`_id\` String, lastPacket Nullable(Int64), \`network.bytes\` Nullable(Int64)),
      \`_id\` String DEFAULT fields.\`_id\`,
      lastPacket DateTime64(3, 'UTC') DEFAULT fromUnixTimestamp64Milli(ifNull(fields.lastPacket, 0)),
      \`source.ip\` String MATERIALIZED ifNull(fields.\`source.ip\`, ''),
      \`destination.ip\` String MATERIALIZED ifNull(fields.\`destination.ip\`, '')
    ) ENGINE = MergeTree ORDER BY lastPacket`);
    await q(`INSERT INTO ${db}.t_sessions3 (fields) FORMAT JSONEachRow\n${rows.map(fields => JSON.stringify({ fields })).join('\n')}`);
    ch = new DbCHImpl({ sessionsDbUrl: `${url}/${db}`, prefix: 't_' });
    await ch.schemaReady;
    const query = ch.query.bind(ch);
    ch.query = (sql, ...args) => { sqls.push(sql); return query(sql, ...args); };
  });
  afterAll(async () => {
    ch?.close();
    await q(`DROP DATABASE IF EXISTS ${db}`);
  });

  for (const order of [undefined, { m0: 'desc' }, { _key: 'asc' }]) {
    test(`each source's pairs match its own search, order ${JSON.stringify(order) ?? 'by sessions'}`, async () => {
      sqls = [];
      const pairTerms = { field: 'destination.ip', size: listed.length, include: listed, ...(order ? { order } : {}) };
      const res = await ch.searchSessions('t_sessions3-*', {
        size: 0,
        query: { match_all: {} },
        aggregations: { side0: { terms: { field: 'source.ip', size: listed.length, include: listed }, aggs: { ...metrics, pair: { terms: pairTerms, aggs: metrics } } } }
      });
      assert.equal(sqls.filter(s => s.includes('GROUP BY key')).length, 2, 'the terms and its pairs');
      const buckets = res.aggregations.side0.buckets;
      assert.equal(buckets.length, listed.length);
      let pairs = 0;
      for (const bucket of buckets) {
        const own = await ch.searchSessions('t_sessions3-*', {
          size: 0,
          query: { term: { 'source.ip': bucket.key } },
          aggregations: { pair: { terms: pairTerms, aggs: metrics } }
        });
        assert.deepEqual(bucket.pair.buckets, own.aggregations.pair.buckets, `source ${bucket.key}`);
        pairs += bucket.pair.buckets.length;
        // and they are the rows' own counts
        for (const p of bucket.pair.buckets) {
          const mine = rows.filter(r => r['source.ip'] === bucket.key && r['destination.ip'] === p.key);
          assert.equal(p.doc_count, mine.length);
          assert.equal(p.m0.value, mine.reduce((sum, r) => sum + r['network.bytes'], 0));
        }
      }
      assert.ok(pairs > listed.length, 'several pairs a source');
    });
  }
});
