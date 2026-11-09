/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

The ClickHouse sessions backend's scroll, with query() answered from rows in memory
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const cjsRequire = createRequire(import.meta.url);
const DbCHImpl = cjsRequire('../viewer/db.ch.js');
const EsQuery = cjsRequire('../viewer/esQuery.js');

// 25 rows, lastPacket repeats so the _id tiebreak matters
const ROWS = Array.from({ length: 25 }, (_, i) => ({ id: `id${String(i).padStart(2, '0')}`, lastPacketMs: 1000 + Math.floor(i / 3) }));

class StubCH extends DbCHImpl {
  sqls = [];
  async query (sql) {
    if (!sql.includes('_dayIndex')) { return { data: [] }; }
    this.sqls.push(sql);
    const asc = sql.includes('lastPacket ASC');
    const cmp = (a, b) => (a.lastPacketMs - b.lastPacketMs) || (a.id < b.id ? -1 : a.id > b.id ? 1 : 0);
    let rows = [...ROWS].sort((a, b) => asc ? cmp(a, b) : cmp(b, a));
    const after = /\) ([<>]) \(fromUnixTimestamp64Milli\((\d+)\), '([^']*)'\)/.exec(sql);
    if (after) {
      const key = { lastPacketMs: Number(after[2]), id: after[3] };
      rows = rows.filter(r => after[1] === '>' ? cmp(r, key) > 0 : cmp(r, key) < 0);
    }
    const total = rows.length;
    rows = rows.slice(0, Number(/ LIMIT (\d+)/.exec(sql)[1]));
    const data = rows.map(r => ({ id: r.id, lastPacket: new Date(r.lastPacketMs).toISOString() }));
    return sql.includes('exact_rows_before_limit') ? { data, rows_before_limit_at_least: total } : { data };
  }
}

function newCH () {
  return new StubCH({ sessionsDbUrl: 'clickhouse://127.0.0.1:1/arkime', prefix: 'tests_' });
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
