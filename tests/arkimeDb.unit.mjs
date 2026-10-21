/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Conformance tests for ArkimeDb collections. Run with `npm run test:unit`.

SQLite and LMDB always run. Set these to also run the other backends:
  ARKIME_TEST_ES_URL=http://localhost:9200
  ARKIME_TEST_REDIS_URL=redis://localhost:6379/5
  ARKIME_TEST_PG_URL=postgres://user:pass@localhost:5432/arkime
*/
import { test, describe, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const require = createRequire(import.meta.url);
const ArkimeUtil = require('../common/arkimeUtil.js');
const ArkimeDb = require('../common/arkimeDb.js');

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'arkimedb-unit-'));
const run = Date.now().toString(36);

const backends = [
  { name: 'sqlite', url: `sqlite://${tmp}/unit.db` },
  { name: 'lmdb', url: `lmdb://${tmp}/unit.lmdb` }
];
if (process.env.ARKIME_TEST_ES_URL) { backends.push({ name: 'es', url: process.env.ARKIME_TEST_ES_URL, prefix: `arkimedb_unit_${run}_` }); }
if (process.env.ARKIME_TEST_REDIS_URL) { backends.push({ name: 'redis', url: process.env.ARKIME_TEST_REDIS_URL }); }
if (process.env.ARKIME_TEST_PG_URL) { backends.push({ name: 'pg', url: process.env.ARKIME_TEST_PG_URL, prefix: `arkimedb_unit_${run}_` }); }

const docs = {
  a: { name: 'alpha', creator: 'bob', roles: ['r1', 'r2'], n: 5, tags: ['Foo', 'bar'], issuedAt: 100, flag: true },
  b: { name: 'Beta', creator: 'sue', roles: ['r2'], n: 10, tags: [], issuedAt: 200 },
  c: { name: 'gamma', creator: 'bob', roles: 'r3', n: 1, issuedAt: 300, nested: { x: 'y' } },
  d: { name: 'delta', creator: 'ann', editRoles: ['r1'], issuedAt: 50 }
};

const whereCases = [
  [{ creator: 'bob' }, 'ac'],
  [{ roles: 'r2' }, 'ab'],
  [{ roles: { $in: ['r3', 'r1'] } }, 'ac'],
  [{ roles: { $in: [] } }, ''],
  [{ $or: [{ creator: 'ann' }, { roles: 'r3' }] }, 'cd'],
  [{ $or: [] }, ''],
  [{ roles: { $ne: 'r2' } }, 'cd'],
  [{ roles: { $nin: ['r2', 'r3'] } }, 'd'],
  [{ roles: { $exists: true } }, 'abc'],
  [{ editRoles: { $exists: false } }, 'abc'],
  [{ tags: { $exists: true } }, 'a'],
  [{ n: { $gte: 5 } }, 'ab'],
  [{ n: { $gt: 5, $lte: 10 } }, 'b'],
  [{ issuedAt: { $lt: 100 } }, 'd'],
  [{ name: { $contains: 'eta' } }, 'b'],
  [{ name: { $icontains: 'BETA' } }, 'b'],
  [{ tags: { $icontains: 'foo' } }, 'a'],
  [{ tags: { $contains: 'foo' } }, ''],
  [{ name: { $prefix: 'ga' } }, 'c'],
  [{ id: 'a' }, 'a'],
  [{ id: { $in: ['a', 'd'] } }, 'ad'],
  [{ id: { $ne: 'a' }, creator: 'bob' }, 'c'],
  [{ 'nested.x': 'y' }, 'c'],
  [{ $not: { creator: 'bob' } }, 'bd'],
  [{ flag: true }, 'a'],
  [{ $and: [{ creator: 'bob' }, { n: { $lt: 3 } }] }, 'c'],
  [null, 'abcd']
];

const ids = (result) => result.data.map(h => h.id).sort().join('');
const order = (result) => result.data.map(h => h.id).join('');

for (const backend of backends) {
  describe(`ArkimeDb ${backend.name}`, () => {
    let conn, coll, vcoll;

    before(async () => {
      conn = ArkimeUtil.dbConnect(backend.url, { prefix: backend.prefix });
      const keyword = { dynamic_templates: [{ string_template: { match_mapping_type: 'string', mapping: { type: 'keyword' } } }] };
      coll = conn.collection(`items${run}`, { es: { create: { mappings: keyword } } });
      vcoll = conn.collection(`versioned${run}`, { version: true, es: { create: { mappings: keyword } } });
      await coll.init();
      await vcoll.init();
      for (const [id, doc] of Object.entries(docs)) {
        await coll.put(id, doc);
      }
    });

    after(async () => {
      if (backend.name === 'es') {
        await conn.client.indices.delete({ index: `${backend.prefix}items${run},${backend.prefix}versioned${run}` });
      } else if (backend.name === 'pg') {
        await conn.pool.query(`DROP TABLE IF EXISTS ${backend.prefix}items${run}, ${backend.prefix}versioned${run}`);
        await conn.pool.query('DELETE FROM arkime_meta WHERE key LIKE $1', [`${backend.prefix}%`]);
      } else {
        await coll.deleteAll();
        await vcoll.deleteAll();
      }
    });

    test('get, missing get and delete', async () => {
      assert.deepEqual(await coll.get('c'), docs.c);
      assert.equal(await coll.get('nope'), null);
      assert.equal(await coll.delete('nope'), false);
    });

    for (const [where, expected] of whereCases) {
      test(`where ${JSON.stringify(where)}`, async () => {
        assert.equal(ids(await coll.find({ where })), expected);
        assert.equal(await coll.count(where), expected.length);
      });
    }

    test('sort and page', async () => {
      assert.equal(order(await coll.find({ sort: { issuedAt: 'desc' } })), 'cbad');
      assert.equal(order(await coll.find({ sort: { name: 'asc' } })), 'badc');
      assert.equal(order(await coll.find({ sort: { n: 'asc' } })), 'cabd');
      assert.equal(order(await coll.find({ sort: { n: 'desc' } })), 'bacd');
      assert.equal(order(await coll.find({ sort: [['creator', 'asc'], ['issuedAt', 'desc']] })), 'dcab');

      const page = await coll.find({ sort: { issuedAt: 'asc' }, from: 1, size: 2 });
      assert.equal(page.total, 4);
      assert.equal(order(page), 'ab');
      assert.deepEqual(page.data[0].doc, docs.a);
    });

    test('rejects bad field names and operators', async () => {
      await assert.rejects(coll.find({ where: { "x') OR 1=1 --": 1 } }), /Invalid db field name/);
      await assert.rejects(coll.find({ where: { name: { $regex: '.*' } } }), /Unknown db where operator/);
      await assert.rejects(coll.find({ sort: { 'bad name': 'asc' } }), /Invalid db field name/);
    });

    test('create, versions and meta', async () => {
      const id = await vcoll.create({ name: 'new' });
      assert.equal(typeof id, 'string');
      assert.deepEqual(await vcoll.get(id), { name: 'new' });

      await vcoll.put('v', { name: 'one' });
      await vcoll.put('v', { name: 'two' });
      const { data } = await vcoll.find({ where: { id: 'v' }, version: true });
      assert.equal(data[0].version, 2);
      assert.deepEqual(data[0].doc, { name: 'two' });

      assert.equal(await vcoll.getMeta('version'), undefined);
      await vcoll.setMeta({ version: 3, initSync: true });
      await vcoll.setMeta({ version: 4 });
      assert.equal(await vcoll.getMeta('version'), 4);
      assert.equal(await vcoll.getMeta('initSync'), true);
    });

    test('createOnly conflicts and update', async () => {
      await vcoll.put('co', { name: 'first' }, { createOnly: true });
      await assert.rejects(vcoll.put('co', { name: 'second' }, { createOnly: true }), ArkimeDb.ConflictError);
      assert.deepEqual(await vcoll.get('co'), { name: 'first' });

      assert.equal(await vcoll.update('co', { lastUsed: 5 }), true);
      assert.deepEqual(await vcoll.get('co'), { name: 'first', lastUsed: 5 });
      const { data } = await vcoll.find({ where: { id: 'co' }, version: true });
      assert.equal(data[0].version, 2);

      assert.equal(await vcoll.update('missing', { lastUsed: 5 }), false);
      assert.equal(await vcoll.get('missing'), null);
    });

    test('find with no size returns every match', async () => {
      for (let i = 0; i < 1200; i++) {
        await vcoll.put(`many${i}`, { many: true, i }, { refresh: false });
      }
      await vcoll.refresh();
      const { total, data } = await vcoll.find({ where: { many: true } });
      assert.equal(total, 1200);
      assert.equal(data.length, 1200);
      assert.equal(new Set(data.map(h => h.doc.i)).size, 1200);
      assert.equal((await vcoll.find({ where: { many: true }, sort: { i: 'desc' }, size: 3 })).data.map(h => h.doc.i).join(), '1199,1198,1197');
    });

    test('deleteWhere and replace', async () => {
      assert.equal(await coll.deleteWhere({ issuedAt: { $lt: 150 } }), 2);
      assert.equal(ids(await coll.find()), 'bc');
      await coll.put('b', { name: 'replaced' });
      assert.deepEqual(await coll.get('b'), { name: 'replaced' });
      assert.equal(await coll.delete('b'), true);
      assert.equal(ids(await coll.find()), 'c');
    });
  });
}

describe('ArkimeDb', () => {
  test('connections are shared per url and prefix', () => {
    const url = `sqlite://${tmp}/shared.db`;
    assert.equal(ArkimeUtil.dbConnect(url), ArkimeUtil.dbConnect(url));
    assert.notEqual(ArkimeUtil.dbConnect(url), ArkimeUtil.dbConnect(url, { prefix: 'x_' }));
  });

  test('table names every sql backend unless overridden', async () => {
    const conn = ArkimeUtil.dbConnect(`sqlite://${tmp}/tables.db`);
    await conn.collection('shared', { table: 'shared_table' }).put('x', { a: 1 });
    await conn.collection('own', { table: 'shared_table2', sqlite: { table: 'own_table' } }).put('y', { b: 2 });
    const tables = conn.db.prepare("SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name").all().map(r => r.name);
    assert.deepEqual(tables, ['meta', 'own_table', 'shared_table']);
  });

  test('sqlite idColumn keeps older tables keyed by another column', async () => {
    const conn = ArkimeUtil.dbConnect(`sqlite://${tmp}/idcol.db`);
    conn.db.exec('CREATE TABLE users (userId TEXT PRIMARY KEY, json TEXT NOT NULL)');
    conn.db.prepare('INSERT INTO users (userId, json) VALUES (?, ?)').run('bob', JSON.stringify({ userId: 'bob' }));
    const users = conn.collection('users', { sqlite: { idColumn: 'userId' } });
    assert.deepEqual(await users.get('bob'), { userId: 'bob' });
    await users.put('sue', { userId: 'sue' });
    assert.equal((await users.find({ where: { id: { $in: ['bob', 'sue'] } }, sort: { id: 'desc' } })).data.map(h => h.id).join(), 'sue,bob');
    await assert.rejects(users.put('bob', {}, { createOnly: true }), ArkimeDb.ConflictError);
    assert.equal(await users.update('bob', { lastUsed: 1 }), true);
    assert.equal(await users.delete('sue'), true);
  });

  test('unknown schemes throw', () => {
    assert.throws(() => ArkimeUtil.dbConnect('nope://x'), /Unknown db url scheme 'nope'/);
  });

  after(async () => {
    await ArkimeDb.closeAll();
    fs.rmSync(tmp, { recursive: true, force: true });
  });
});
