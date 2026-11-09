/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

The ClickHouse sessions backend's scroll, sessions iterator and field map, with query() answered from rows in memory
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

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
