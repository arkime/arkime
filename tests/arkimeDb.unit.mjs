/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Conformance tests for ArkimeDb collections. Run with `npm run test:unit`.

SQLite always runs. Set these to also run the other backends:
  ARKIME_TEST_ES_URL=http://localhost:9200
  ARKIME_TEST_PG_URL=postgres://user:pass@localhost:5432/arkime
*/
import { test, describe, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const cjsRequire = createRequire(import.meta.url);
const ArkimeUtil = cjsRequire('../common/arkimeUtil.js');
const ArkimeDb = cjsRequire('../common/arkimeDb.js');
const ESConnection = cjsRequire('../common/arkimeDb.es.js');

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'arkimedb-unit-'));
const run = Date.now().toString(36);

const backends = [
  { name: 'sqlite', url: `sqlite://${tmp}/unit.db` }
];
if (process.env.ARKIME_TEST_ES_URL) { backends.push({ name: 'es', url: process.env.ARKIME_TEST_ES_URL, prefix: `arkimedb_unit_${run}_` }); }
if (process.env.ARKIME_TEST_PG_URL) { backends.push({ name: 'pg', url: process.env.ARKIME_TEST_PG_URL, prefix: `arkimedb_unit_${run}_` }); }

const docs = {
  a: { name: 'alpha', creator: 'bob', roles: ['r1', 'r2'], n: 5, tags: ['Foo', 'bar'], issuedAt: 100, flag: true, ip: '10.1.2.3', hosts: [{ name: 'h1', port: 1 }, { name: 'h2' }] },
  b: { name: 'Beta', creator: 'sue', roles: ['r2'], n: 10, tags: [], issuedAt: 200, ip: '10.2.0.1' },
  c: { name: 'gamma', creator: 'bob', roles: 'r3', n: 1, issuedAt: 300, nested: { x: 'y' }, ip: '2001:db8::1', hosts: { name: 'h3' } },
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
  [{ n: { $ne: 5 } }, 'bcd'],
  [{ n: { $exists: false } }, 'd'],
  [{ creator: { $nin: ['bob'] } }, 'bd'],
  [{ name: { $gt: 'b' } }, 'cd'],
  [{ 'nested.x': { $ne: 'y' } }, 'abd'],
  [{ name: { $contains: 'eta' } }, 'b'],
  [{ name: { $icontains: 'BETA' } }, 'b'],
  [{ tags: { $icontains: 'foo' } }, 'a'],
  [{ tags: { $contains: 'foo' } }, ''],
  [{ name: { $prefix: 'ga' } }, 'c'],
  [{ creator: { $wildcard: 'b*' } }, 'ac'],
  [{ creator: { $wildcard: '?u?' } }, 'b'],
  [{ name: { $wildcard: '*eta' } }, 'b'],
  [{ name: { $wildcard: '*ETA' } }, ''],
  [{ tags: { $wildcard: 'F*' } }, 'a'],
  [{ name: { $wildcard: 'al%' } }, ''],
  [{ name: { $wildcard: '[B]eta' } }, ''],
  [{ name: { $regex: 'g.*a' } }, 'c'],
  [{ creator: { $regex: 'b.b' } }, 'ac'],
  [{ name: { $regex: 'eta' } }, ''],
  [{ tags: { $regex: '[Ff]oo' } }, 'a'],
  [{ id: 'a' }, 'a'],
  [{ id: { $in: ['a', 'd'] } }, 'ad'],
  [{ id: { $ne: 'a' }, creator: 'bob' }, 'c'],
  [{ 'nested.x': 'y' }, 'c'],
  [{ $not: { creator: 'bob' } }, 'bd'],
  [{ flag: true }, 'a'],
  [{ $and: [{ creator: 'bob' }, { n: { $lt: 3 } }] }, 'c'],
  [{ 'hosts.name': 'h2' }, 'a'],
  [{ 'hosts.name': { $prefix: 'h' } }, 'ac'],
  [{ 'hosts.name': { $ne: 'h1' } }, 'bcd'],
  [{ 'hosts.port': { $exists: true } }, 'a'],
  [{ ip: { $cidr: '10.1.0.0/16' } }, 'a'],
  [{ ip: { $cidr: '10.0.0.0/8' } }, 'ab'],
  [{ ip: { $cidr: '10.2.0.1' } }, 'b'],
  [{ ip: { $cidr: '2001:db8::/32' } }, 'c'],
  [{ ip: { $cidr: '192.168.0.0/16' } }, ''],
  [null, 'abcd']
];

// Indexed fields hold one value, the where, sort and aggregate tests must match the plain collection
const indexedFields = {
  name: { type: 'keyword', indexed: 'wildcard' },
  creator: { type: 'keyword', indexed: true },
  n: { type: 'long', indexed: 'sort' },
  issuedAt: { type: 'date', indexed: true },
  flag: { type: 'boolean', indexed: true },
  'nested.x': { type: 'keyword', indexed: true },
  roles: 'keyword',
  editRoles: 'keyword',
  tags: 'keyword'
};

const ids = (result) => result.data.map(h => h.id).sort().join('');
const order = (result) => result.data.map(h => h.id).join('');

for (const backend of backends) {
  describe(`ArkimeDb ${backend.name}`, () => {
    let conn, coll, vcoll, icoll;

    before(async () => {
      conn = ArkimeUtil.dbConnect(backend.url, { prefix: backend.prefix });
      const keyword = { dynamic_templates: [{ string_template: { match_mapping_type: 'string', mapping: { type: 'keyword' } } }], properties: { ip: { type: 'ip' } } };
      coll = conn.collection(`items${run}`, { es: { create: { mappings: keyword } } });
      vcoll = conn.collection(`versioned${run}`, { version: true, es: { create: { mappings: keyword } } });
      icoll = conn.collection(`indexed${run}`, { fields: indexedFields, es: { create: { mappings: keyword } } });
      await coll.init();
      await vcoll.init();
      await icoll.init();
      for (const [id, doc] of Object.entries(docs)) {
        await coll.put(id, doc);
        await icoll.put(id, doc);
      }
    });

    after(async () => {
      if (backend.name === 'es') {
        await conn.client.indices.delete({ index: `${backend.prefix}items${run},${backend.prefix}versioned${run},${backend.prefix}indexed${run}` });
      } else if (backend.name === 'pg') {
        await conn.pool.query(`DROP TABLE IF EXISTS ${backend.prefix}items${run}, ${backend.prefix}versioned${run}, ${backend.prefix}indexed${run}, ${backend.prefix}typedre${run}, ${backend.prefix}arkime_meta`);
        await conn.pool.query(`DROP FUNCTION IF EXISTS ${backend.prefix}arkime_inet(text)`);
      } else {
        await coll.deleteAll();
        await vcoll.deleteAll();
        await icoll.deleteAll();
      }
    });

    test('get, missing get and delete', async () => {
      assert.deepEqual(await coll.get('c'), docs.c);
      assert.equal(await coll.get('nope'), null);
      assert.equal(await coll.delete('nope'), false);
    });

    for (const [label, c] of [['', () => coll], ['indexed ', () => icoll]]) {
      for (const [where, expected] of whereCases) {
        test(`${label}where ${JSON.stringify(where)}`, async () => {
          assert.equal(ids(await c().find({ where })), expected);
          assert.equal(await c().count(where), expected.length);
        });
        // multies answers the viewer's ES queries from these backends, an ES term can't say it was a $cidr
        if (backend.name !== 'es' && !JSON.stringify(where ?? {}).includes('$cidr')) {
          test(`${label}where from es ${JSON.stringify(where)}`, async () => {
            assert.equal(ids(await c().find({ where: ESConnection.decompile(ESConnection.compile(where)) })), expected);
          });
        }
      }

      test(`${label}sort and page`, async () => {
        assert.equal(order(await c().find({ sort: { issuedAt: 'desc' } })), 'cbad');
        assert.equal(order(await c().find({ sort: { name: 'asc' } })), 'badc');
        assert.equal(order(await c().find({ sort: { n: 'asc' } })), 'cabd');
        assert.equal(order(await c().find({ sort: { n: 'desc' } })), 'bacd');
        assert.equal(order(await c().find({ sort: [['creator', 'asc'], ['issuedAt', 'desc']] })), 'dcab');

        const page = await c().find({ sort: { issuedAt: 'asc' }, from: 1, size: 2 });
        assert.equal(page.total, 4);
        assert.equal(order(page), 'ab');
        assert.deepEqual(page.data[0].doc, docs.a);
        assert.equal(order(await c().find({ sort: { issuedAt: 'asc' }, from: 1 })), 'abc');
      });

      test(`${label}aggregate`, async () => {
        assert.deepEqual(await c().aggregate({ groupBy: 'creator', metrics: { first: { min: 'issuedAt' }, last: { max: 'issuedAt' }, total: { sum: 'n' } } }), [
          { key: 'bob', count: 2, first: 100, last: 300, total: 6 },
          { key: 'ann', count: 1, first: 50, last: 50, total: 0 },
          { key: 'sue', count: 1, first: 200, last: 200, total: 10 }
        ]);
        assert.deepEqual((await c().aggregate({ groupBy: 'roles' })).map(b => `${b.key}:${b.count}`), ['r2:2', 'r1:1', 'r3:1']);
        assert.deepEqual((await c().aggregate({ where: { issuedAt: { $gte: 100 } }, groupBy: 'creator', size: 1 })).map(b => `${b.key}:${b.count}`), ['bob:2']);
        assert.deepEqual((await c().aggregate({ groupBy: 'id', size: 2 })).map(b => `${b.key}:${b.count}`), ['a:1', 'b:1']);
        assert.deepEqual((await c().aggregate({ where: { creator: 'bob' }, groupBy: 'id', metrics: { total: { sum: 'n' } } })), [
          { key: 'a', count: 1, total: 5 },
          { key: 'c', count: 1, total: 1 }
        ]);
        await assert.rejects(c().aggregate({ groupBy: 'creator', metrics: { x: { avg: 'n' } } }), /Unknown db metric/);
      });
    }

    if (backend.name === 'es') {
      test('fields map the index unless es.dbpl', async () => {
        const mapped = conn.collection(`mapped${run}`, { fields: { creator: { type: 'keyword', indexed: true }, n: 'long' } });
        const dbpl = conn.collection(`dbpl${run}`, { fields: { creator: 'keyword' }, es: { dbpl: true } });
        await mapped.init();
        await dbpl.init();
        const { body } = await conn.client.indices.getMapping({ index: mapped.index });
        assert.deepEqual(Object.values(body)[0].mappings.properties, { creator: { type: 'keyword' }, n: { type: 'long' } });
        assert.equal((await conn.client.indices.exists({ index: dbpl.index })).body, false);
        await conn.client.indices.delete({ index: mapped.index });
      });

      test('find scrolls when from + size is past 10000', async () => {
        const big = conn.collection(`big${run}`, { fields: { i: 'long' } });
        await big.init();
        const body = [];
        for (let i = 0; i < 10050; i++) { body.push({ index: { _index: big.index, _id: `b${i}` } }, { i }); }
        await conn.client.bulk({ body, refresh: true });
        try {
          const page = await big.find({ sort: { i: 'asc' }, from: 9990, size: 30 });
          assert.equal(page.total, 10050);
          assert.deepEqual(page.data.map(h => h.doc.i), Array.from({ length: 30 }, (_, k) => 9990 + k));
          assert.equal((await big.find({ size: 100000 })).data.length, 10050);
          assert.equal((await big.find({ sort: { i: 'desc' }, size: 10001 })).data.at(-1).doc.i, 49);
        } finally {
          await conn.client.indices.delete({ index: big.index });
        }
      });
    }

    if (backend.name === 'pg') {
      test('typed collections keep each field in its own column', async () => {
        const t = `${backend.prefix}typed${run}`;
        const v1 = { nodeName: { type: 'keyword', indexed: true }, interval: { type: 'short', indexed: true }, currentTime: { type: 'date', indexed: 'sort' }, cpu: 'long', memoryP: 'float', ver: 'text' };
        const old = conn.collection(`typed${run}`, { typed: true, fields: v1 });
        try {
          await old.put('a-5', { nodeName: 'a', interval: 5, currentTime: 100, cpu: 7, memoryP: 1.5, ver: '6.0' });
          await old.put('b-5', { nodeName: 'b', interval: 5, currentTime: 200, cpu: 3 });
          await old.put('a-60', { nodeName: 'a', interval: 60, currentTime: 300, cpu: 9 });
          assert.deepEqual(await old.get('a-5'), { nodeName: 'a', interval: 5, currentTime: 100, cpu: 7, memoryP: 1.5, ver: '6.0' });
          assert.deepEqual(await old.get('b-5'), { nodeName: 'b', interval: 5, currentTime: 200, cpu: 3 }, 'null columns are left out');

          const { rows: cols } = await conn.pool.query(`SELECT column_name, data_type FROM information_schema.columns
            WHERE table_name = $1 ORDER BY column_name`, [t]);
          assert.deepEqual(Object.fromEntries(cols.map(r => [r.column_name, r.data_type])), {
            cpu: 'bigint', currentTime: 'bigint', id: 'text', interval: 'smallint', memoryP: 'real', nodeName: 'text', ver: 'text', version: 'integer'
          });

          assert.deepEqual(ids(await old.find({ where: { interval: 5, currentTime: { $gte: 150 } } })), 'b-5');
          assert.deepEqual(ids(await old.find({ where: { nodeName: { $in: ['a'] }, cpu: { $ne: 7 } } })), 'a-60');
          assert.deepEqual(ids(await old.find({ where: { memoryP: { $exists: false } } })), 'a-60b-5');
          assert.deepEqual(ids(await old.find({ where: { nope: { $exists: false } } })), 'a-5a-60b-5', 'a field with no column is never there');
          assert.equal((await old.find({ where: { nope: 1 } })).total, 0);
          assert.equal((await old.find({ where: { interval: '5' } })).total, 2, 'a numeric string matches a number column');
          assert.equal(order(await old.find({ sort: { currentTime: 'desc' } })), 'a-60b-5a-5');
          assert.deepEqual((await old.find({ where: { nodeName: 'a' }, fields: ['cpu'], sort: { cpu: 'asc' } })).data.map(h => h.doc), [{ cpu: 7 }, { cpu: 9 }]);
          assert.deepEqual(await old.aggregate({ groupBy: 'interval', metrics: { cpu: { sum: 'cpu' }, hi: { max: 'currentTime' } } }),
            [{ key: 5, count: 2, cpu: 10, hi: 200 }, { key: 60, count: 1, cpu: 9, hi: 300 }]);

          assert.equal(await old.update('b-5', { cpu: 4, ver: '6.1' }), true);
          assert.deepEqual(await old.get('b-5'), { nodeName: 'b', interval: 5, currentTime: 200, cpu: 4, ver: '6.1' });
          await old.put('b-5', { nodeName: 'b', interval: 5 });
          assert.deepEqual(await old.get('b-5'), { nodeName: 'b', interval: 5 }, 'a put replaces the whole doc');
          await assert.rejects(old.put('c', { nodeName: 'c', bogus: 1 }), /no column for 'bogus'/);
          await assert.rejects(old.put('a-5', { nodeName: 'a' }, { createOnly: true }), ArkimeDb.ConflictError);

          const read = await old.getVersioned('a-5');
          await old.putIfVersion('a-5', { ...read.doc, cpu: 8 }, read.version);
          await assert.rejects(old.putIfVersion('a-5', read.doc, read.version), ArkimeDb.ConflictError);
          assert.equal((await old.get('a-5')).cpu, 8);

          // A newer version adds a field, the older rows have no value for it and older code still reads it
          const PGConnection = cjsRequire('../common/arkimeDb.pg.js');
          const newer = new PGConnection(backend.url, { prefix: backend.prefix });
          try {
            const upgraded = newer.collection(`typed${run}`, { typed: true, fields: { ...v1, totalK: 'long' } });
            await upgraded.put('d-5', { nodeName: 'd', interval: 5, totalK: 42 });
            assert.deepEqual(await upgraded.get('a-60'), { nodeName: 'a', interval: 60, currentTime: 300, cpu: 9 });
            const again = new PGConnection(backend.url, { prefix: backend.prefix });
            assert.deepEqual(await again.collection(`typed${run}`, { typed: true, fields: v1 }).get('d-5'), { nodeName: 'd', interval: 5, totalK: 42 });
            await again.close();
          } finally {
            await newer.close();
          }

          assert.equal(await old.deleteWhere({ nodeName: 'a' }), 2);
          assert.equal(await old.count(), 2);
          await assert.rejects(old.nextSequence('x'), /isn't supported/);
        } finally {
          await conn.pool.query(`DROP TABLE IF EXISTS ${t}`);
        }
      });

      test('indexed fields are generated columns that queries use the index for', async () => {
        const t = `${backend.prefix}indexed${run}`;
        const { rows: cols } = await conn.pool.query(`SELECT column_name FROM information_schema.columns
          WHERE table_name = $1 AND is_generated = 'ALWAYS' ORDER BY column_name`, [t]);
        assert.deepEqual(cols.map(r => r.column_name), ['f_creator', 'f_flag', 'f_issuedat', 'f_n', 'f_name', 'f_nested__x']);

        const query = conn.pool.query;
        const seen = [];
        conn.pool.query = function (q, p) { seen.push([q, p]); return query.call(this, q, p); };
        try {
          await icoll.find({ where: { creator: 'bob' } });
        } finally {
          conn.pool.query = query;
        }
        const [sql, params] = seen.find(([q]) => q.includes('LIMIT'));
        const client = await conn.pool.connect();
        try {
          await client.query('BEGIN');
          await client.query('SET LOCAL enable_seqscan = off');
          const { rows } = await client.query(`EXPLAIN ${sql}`, params);
          assert.match(rows.map(r => r['QUERY PLAN']).join('\n'), /Index.*f_creator/);
          const { rows: desc } = await client.query(`EXPLAIN SELECT id FROM ${t} ORDER BY f_n DESC NULLS LAST LIMIT 2`);
          assert.match(desc.map(r => r['QUERY PLAN']).join('\n'), /Index.*f_n_desc/);
          const { rows: trgm } = await client.query(`EXPLAIN SELECT id FROM ${t} WHERE (f_name #>> '{}') LIKE '%eta%'`);
          assert.match(trgm.map(r => r['QUERY PLAN']).join('\n'), /Bitmap Index Scan on .*f_name_trgm/);
        } finally {
          await client.query('ROLLBACK');
          client.release();
        }
      });
    }

    test('rejects bad field names and operators', async () => {
      await assert.rejects(coll.find({ where: { "x') OR 1=1 --": 1 } }), /Invalid db field name/);
      await assert.rejects(coll.find({ where: { name: { $like: '%' } } }), /Unknown db where operator/);
      await assert.rejects(coll.find({ where: { roles: new Set(['r1']) } }), /operator object/);
      await assert.rejects(coll.find({ where: { roles: {} } }), /operator object/);
      await assert.rejects(coll.count({ roles: new Map() }), /operator object/);
      await assert.rejects(coll.find({ sort: { 'bad name': 'asc' } }), /Invalid db field name/);
    });

    if (backend.name !== 'es') {
      test('rejects regexes RE2 can not run', async () => {
        const where = { $regex: '(.*)(.*)\\2\\1' };
        await assert.rejects(coll.find({ where: { name: where } }), /invalid escape sequence/);
        await assert.rejects(coll.find({ where: { id: where } }), /invalid escape sequence/);
        await assert.rejects(icoll.find({ where: { creator: where } }), /invalid escape sequence/);
        if (backend.name === 'pg') {
          await assert.rejects(conn.collection(`typedre${run}`, { typed: true, fields: { nodeName: 'keyword' } }).find({ where: { nodeName: where } }), /invalid escape sequence/);
        }
      });
    }

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

      assert.equal(await vcoll.update('co', { lastUsed: null, tags: { a: 1 } }), true);
      assert.deepEqual(await vcoll.get('co'), { name: 'first', lastUsed: null, tags: { a: 1 } });

      assert.equal(await vcoll.update('missing', { lastUsed: 5 }), false);
      assert.equal(await vcoll.get('missing'), null);
    });

    test('a hostile $wildcard runs in linear time', async () => {
      await vcoll.put('redos', { u: 'a'.repeat(200) }, { refresh: false });
      await vcoll.refresh();
      const start = Date.now();
      const { total } = await vcoll.find({ where: { u: { $wildcard: '*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*b' } } });
      assert.equal(total, 0);
      assert.ok(Date.now() - start < 1000, `took ${Date.now() - start} ms`);
      await vcoll.delete('redos');
    });

    test('nextSequence', async () => {
      assert.equal(await vcoll.nextSequence('seqA'), 1);
      assert.equal(await vcoll.nextSequence('seqA'), 2);
      assert.equal(await vcoll.nextSequence('seqB'), 1);
      assert.equal(await vcoll.nextSequence('seqA'), 3);
      await vcoll.delete('seqA');
      await vcoll.delete('seqB');
    });

    test('lease', async () => {
      assert.equal(await vcoll.lease('pv', 'a', 60000), true);
      assert.equal(await vcoll.lease('pv', 'a', 60000), true, 'owner renews');
      assert.equal(await vcoll.lease('pv', 'b', 60000), false, 'fresh lease is held');
      assert.equal((await vcoll.get('pv')).name, 'a');
      assert.equal(await vcoll.lease('pv', 'b', 0), true, 'stale lease is taken');
      await vcoll.refresh();
      assert.equal((await vcoll.get('pv')).name, 'b');
      assert.equal(await vcoll.delete('pv'), true);
    });

    test('putIfVersion only replaces the doc as it was read', async () => {
      for (const c of [coll, vcoll]) {
        assert.equal(await c.getVersioned('cas'), null);
        await c.put('cas', { n: 1 });
        const first = await c.getVersioned('cas');
        assert.deepEqual(first.doc, { n: 1 });
        await c.putIfVersion('cas', { n: 2 }, first.version);
        assert.deepEqual(await c.get('cas'), { n: 2 });
        await assert.rejects(c.putIfVersion('cas', { n: 3 }, first.version), err => err instanceof ArkimeDb.ConflictError && err.statusCode === 409);
        assert.deepEqual(await c.get('cas'), { n: 2 }, 'a stale version does not write');
        const second = await c.getVersioned('cas');
        await c.delete('cas');
        await assert.rejects(c.putIfVersion('cas', { n: 4 }, second.version), ArkimeDb.ConflictError);
        assert.equal(await c.get('cas'), null, 'a deleted doc is not recreated');

        await c.put('race', { n: 0 });
        const read = await c.getVersioned('race');
        const results = await Promise.allSettled([1, 2, 3, 4].map(n => c.putIfVersion('race', { n }, read.version)));
        assert.equal(results.filter(r => r.status === 'fulfilled').length, 1, 'only one racing write wins');
        const now = await c.getVersioned('race');
        await Promise.allSettled([c.delete('race'), c.putIfVersion('race', { n: 9 }, now.version)]);
        assert.equal(await c.get('race'), null, 'a racing delete is not undone');
      }
    });

    if (backend.name === 'pg') {
      test('pg tables can be created by several processes at once', async () => {
        const PGConnection = cjsRequire('../common/arkimeDb.pg.js');
        const conns = Array.from({ length: 6 }, () => new PGConnection(backend.url, { prefix: backend.prefix }));
        await Promise.all(conns.map(c => c.collection(`race${run}`).init()));
        await conns[0].pool.query(`DROP TABLE ${backend.prefix}race${run}`);
        await Promise.all(conns.map(c => c.close()));
      });

      test('pg survives an idle client losing its connection', async () => {
        const PGConnection = cjsRequire('../common/arkimeDb.pg.js');
        const idle = new PGConnection(backend.url, { prefix: backend.prefix });
        try {
          const { rows } = await idle.pool.query('SELECT pg_backend_pid() AS pid');
          await conn.pool.query('SELECT pg_terminate_backend($1)', [rows[0].pid]);
          await new Promise(resolve => setTimeout(resolve, 200));
          assert.equal((await idle.pool.query('SELECT 1 AS one')).rows[0].one, 1);
        } finally {
          await idle.close();
        }
      });

      test('pg meta table and inet helper are per prefix', async () => {
        const PGConnection = cjsRequire('../common/arkimeDb.pg.js');
        const other = new PGConnection(backend.url, { prefix: `${backend.prefix}other_` });
        try {
          const c = other.collection(`items${run}`);
          await c.setMeta({ version: 99 });
          assert.equal(await c.getMeta('version'), 99);
          assert.equal(await vcoll.getMeta('version'), 4);
          const names = [`${backend.prefix}arkime_meta`, `${backend.prefix}other_arkime_meta`, `${backend.prefix}arkime_inet`, `${backend.prefix}other_arkime_inet`];
          const { rows } = await conn.pool.query('SELECT relname AS name FROM pg_class WHERE relname = ANY($1) UNION ALL SELECT proname FROM pg_proc WHERE proname = ANY($1)', [names]);
          assert.deepEqual(rows.map(r => r.name).sort(), names.sort());
        } finally {
          await other.pool.query(`DROP TABLE IF EXISTS ${backend.prefix}other_items${run}, ${backend.prefix}other_arkime_meta`);
          await other.pool.query(`DROP FUNCTION IF EXISTS ${backend.prefix}other_arkime_inet(text)`);
          await other.close();
        }
      });
    }

    if (backend.name === 'es') {
      test('es writeIndex reads a pattern and writes one index', async () => {
        const hist = conn.collection(`hist${run}`, { es: { index: `${backend.prefix}hist${run}_*`, writeIndex: () => `${backend.prefix}hist${run}_a` } });
        const id = await hist.create({ api: '/x' });
        await hist.put('fixed', { api: '/y' });
        await hist.refresh();
        assert.deepEqual(await hist.get(id), { api: '/x' });
        const { data } = await hist.find();
        assert.deepEqual(data.map(h => h.index), [`${backend.prefix}hist${run}_a`, `${backend.prefix}hist${run}_a`]);
        assert.equal(await hist.delete(id), true);
        assert.equal(await hist.get(id), null);
        await conn.client.indices.delete({ index: `${backend.prefix}hist${run}_a` });
      });
    }

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

describe('viewer cluster db', () => {
  test('capture creates the same typed stats columns as the viewer', () => {
    const DbClusterImpl = cjsRequire('../viewer/db.cluster.js');
    const pgType = { keyword: 'text', short: 'smallint', date: 'bigint', long: 'bigint', float: 'real' };
    const src = fs.readFileSync(new URL('../capture/db.pg.c', import.meta.url), 'utf8');
    const block = src.slice(src.indexOf('pgStatsColumns[] = {'), src.indexOf('{NULL, NULL}', src.indexOf('pgStatsColumns[] = {')));
    const capture = Object.fromEntries([...block.matchAll(/\{"(\w+)", "(\w+)"\}/g)].map(m => [m[1], m[2]]));
    for (const collName of ['stats', 'dstats']) {
      const fields = DbClusterImpl.collectionOptions(collName).fields;
      const viewer = Object.fromEntries(Object.entries(fields).map(([f, spec]) => [f, pgType[spec.type ?? spec]]));
      assert.deepEqual(viewer, capture, collName);
    }
  });
});

describe('viewer users db', () => {
  test('views are only looked up by a string id or name', async () => {
    const DbUsersImpl = cjsRequire('../viewer/db.users.js');
    const users = new DbUsersImpl(ArkimeUtil.dbConnect(`sqlite://${tmp}/viewerusers.db`));
    await users.init();
    const id = await users.createView({ name: 'mine', user: 'bob', expression: 'ip == 1.2.3.4' });
    assert.equal((await users.getViewByIdOrName(id, 'bob', [])).name, 'mine');
    assert.equal((await users.getViewByIdOrName('mine', 'bob', [])).name, 'mine');
    assert.equal(await users.getViewByIdOrName({ $wildcard: '*' }, 'bob', []), null);
    assert.equal(await users.getViewByIdOrName(['mine'], 'bob', []), null);
  });
});

describe('cont3xt db', () => {
  test('sqlite uses cont3xt_ names and moves the older layout once', async () => {
    const Db = cjsRequire('../cont3xt/db.js');
    const url = `sqlite://${tmp}/cont3xt.db`;
    const conn = ArkimeUtil.dbConnect(url);

    // the older layout, with a viewer view sharing the old views table
    const oldViews = conn.collection('old_views', { sqlite: { table: 'views' } });
    const oldLinks = conn.collection('old_links', { sqlite: { table: 'link_groups' } });
    await oldViews.put('cv1', { name: 'cont3xt view', creator: 'bob', integrations: [] });
    await oldViews.put('vv1', { name: 'viewer view', user: 'sue', expression: 'ip == 1.2.3.4' });
    await oldLinks.put('l1', { name: 'links', creator: 'bob', links: [] });

    await Db.initialize({ url });
    assert.deepEqual((await Db.getMatchingViews(undefined, undefined, true)).map(v => v._id), ['cv1']);
    assert.deepEqual((await Db.getMatchingLinkGroups(undefined, undefined, true)).map(l => l._id), ['l1']);
    assert.deepEqual((await oldViews.find()).data.map(h => h.id), ['vv1'], 'viewer views stay where they were');
    assert.equal(await oldLinks.count(), 0);

    // cont3xt can't see or change a viewer view through its own views
    assert.equal(await Db.getView('vv1'), null);
    assert.equal(await Db.deleteView('vv1'), null);

    const tables = conn.db.prepare("SELECT name FROM sqlite_master WHERE type = 'table' AND name LIKE 'cont3xt_%' ORDER BY name").all().map(r => r.name);
    assert.deepEqual(tables, ['cont3xt_history', 'cont3xt_links', 'cont3xt_overviews', 'cont3xt_views']);

    // a second start copies nothing
    await Db.initialize({ url });
    assert.equal((await Db.getMatchingViews(undefined, undefined, true)).length, 1);
  });
});

describe('ArkimeDb', () => {
  test('connections are shared per url and connection options', () => {
    const url = `sqlite://${tmp}/shared.db`;
    assert.equal(ArkimeUtil.dbConnect(url), ArkimeUtil.dbConnect(url));
    assert.equal(ArkimeUtil.dbConnect(url, { section: 'a' }), ArkimeUtil.dbConnect(url, { section: 'b' }));
    assert.notEqual(ArkimeUtil.dbConnect(url), ArkimeUtil.dbConnect(url, { prefix: 'x_' }));
    assert.notEqual(ArkimeUtil.dbConnect(url, { basicAuth: 'a:b' }), ArkimeUtil.dbConnect(url, { basicAuth: 'a:c' }));
  });

  test('table names every sql backend unless overridden', async () => {
    const conn = ArkimeUtil.dbConnect(`sqlite://${tmp}/tables.db`);
    await conn.collection('shared', { table: 'shared_table' }).put('x', { a: 1 });
    await conn.collection('own', { table: 'shared_table2', sqlite: { table: 'own_table' } }).put('y', { b: 2 });
    const tables = conn.db.prepare("SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name").all().map(r => r.name);
    assert.deepEqual(tables, ['meta', 'own_table', 'shared_table']);
  });

  test('sqlite aggregate groups by the id and indexed fields', async () => {
    const conn = ArkimeUtil.dbConnect(`sqlite://${tmp}/aggcols.db`);
    const audits = conn.collection('audits', { fields: { issuedAt: { type: 'date', indexed: true } } });
    await audits.put('x', { issuedAt: 1, n: 2 });
    await audits.put('y', { issuedAt: 1, n: 3 });
    await audits.put('z', { issuedAt: 2 });
    assert.deepEqual(await audits.aggregate({ groupBy: 'issuedAt', metrics: { total: { sum: 'n' } } }), [
      { key: 1, count: 2, total: 5 },
      { key: 2, count: 1, total: 0 }
    ]);
    assert.deepEqual((await audits.aggregate({ groupBy: 'id', size: 2 })).map(b => b.key), ['x', 'y']);
    assert.deepEqual((await audits.aggregate({ where: { issuedAt: 2 }, groupBy: 'id' })).map(b => b.key), ['z']);
  });

  test('fields build the es mapping, indexed only marks the sql backends', () => {
    assert.deepEqual(ArkimeDb.esProperties({
      creator: { type: 'keyword', indexed: true },
      integrations: { type: 'keyword', index: false },
      'fields.from': 'keyword',
      'fields.type': 'keyword'
    }), {
      creator: { type: 'keyword' },
      integrations: { type: 'keyword', index: false },
      fields: { properties: { from: { type: 'keyword' }, type: { type: 'keyword' } } }
    });
    assert.deepEqual(ArkimeDb.indexedFields({ fields: { a: 'keyword', b: { type: 'long', indexed: true } } }), ['b']);
    assert.deepEqual(ArkimeDb.sortIndexedFields({ fields: { a: { type: 'keyword', indexed: 'sort' }, b: { type: 'long', indexed: true }, c: { type: 'long', indexed: 'sort' } } }), ['c']);
    assert.deepEqual(ArkimeDb.wildcardIndexedFields({ fields: { a: { type: 'keyword', indexed: 'wildcard' }, b: { type: 'keyword', indexed: true } } }), ['a']);
    assert.deepEqual(ArkimeDb.esProperties({ c: { type: 'long', indexed: 'sort' }, d: { type: 'keyword', indexed: 'wildcard' } }), { c: { type: 'long' }, d: { type: 'keyword' } });
    assert.throws(() => ArkimeDb.indexedFields({ fields: { id: { type: 'keyword', indexed: true } } }), /always indexed/);
    assert.throws(() => ArkimeDb.indexedFields({ fields: { 'bad name': { type: 'keyword', indexed: true } } }), /Invalid db field name/);
  });

  test('sqlite indexed fields are generated columns that queries use the index for', async () => {
    const conn = ArkimeUtil.dbConnect(`sqlite://${tmp}/indexed.db`);
    conn.db.exec('CREATE TABLE history (id TEXT PRIMARY KEY, json TEXT NOT NULL)');
    conn.db.prepare('INSERT INTO history (id, json) VALUES (?, ?)').run('old', JSON.stringify({ userId: 'bob', issuedAt: 5 }));

    const hcoll = conn.collection('history', { fields: { userId: { type: 'keyword', indexed: true }, issuedAt: { type: 'date', indexed: true }, tags: 'keyword' } });
    const cols = conn.db.prepare('PRAGMA table_xinfo(history)').all().map(c => c.name);
    assert.deepEqual(cols, ['id', 'json', 'f_userId', 'f_issuedAt']);
    await hcoll.put('new', { userId: 'sue', issuedAt: 10 });
    assert.equal(ids(await hcoll.find({ where: { userId: 'bob' } })), 'old');
    assert.equal(order(await hcoll.find({ sort: { issuedAt: 'desc' } })), 'newold');

    // the query find runs must search the index
    const prepare = conn.db.prepare;
    const seen = [];
    conn.db.prepare = function (sql) { seen.push(sql); return prepare.call(this, sql); };
    try {
      await hcoll.find({ where: { userId: 'bob', issuedAt: { $gte: 1 } } });
    } finally {
      conn.db.prepare = prepare;
    }
    const select = seen.find(sql => sql.includes('LIMIT'));
    const plan = conn.db.prepare(`EXPLAIN QUERY PLAN ${select}`).all('bob', 1, -1, 0).map(r => r.detail).join('\n');
    assert.match(plan, /USING INDEX history_f_(userId|issuedAt)/);

    // a second open of the same table adds nothing
    const again = ArkimeUtil.dbConnect(`sqlite://${tmp}/indexed.db`, { prefix: 'again' });
    again.collection('history', { fields: { userId: { type: 'keyword', indexed: true }, issuedAt: { type: 'date', indexed: true } } });
    assert.equal(again.db.prepare('PRAGMA table_xinfo(history)').all().length, 4);
  });

  test('sqlite legacyMeta moves older meta keys under the table', async () => {
    const conn = ArkimeUtil.dbConnect(`sqlite://${tmp}/legacymeta.db`);
    conn.db.exec('CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL)');
    conn.db.prepare('INSERT INTO meta (key, value) VALUES (?, ?)').run('shortcutsVersion', '7');
    const shortcuts = conn.collection('shortcuts', { version: true, sqlite: { legacyMeta: { version: 'shortcutsVersion' } } });
    assert.equal(await shortcuts.getMeta('version'), 7);
    assert.equal(conn.db.prepare("SELECT COUNT(*) AS n FROM meta WHERE key = 'shortcutsVersion'").get().n, 0);
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

  test('ipInCidr matches like an ES ip term and bad cidrs throw', () => {
    assert.equal(ArkimeDb.ipInCidr('10.1.2.3', '10.0.0.0/8'), true);
    assert.equal(ArkimeDb.ipInCidr('10.1.2.3', '10.1.2.3'), true);
    assert.equal(ArkimeDb.ipInCidr('11.0.0.1', '10.0.0.0/8'), false);
    assert.equal(ArkimeDb.ipInCidr('2001:db8::5', '2001:db8::/32'), true);
    assert.equal(ArkimeDb.ipInCidr('not an ip', '10.0.0.0/8'), false);
    for (const bad of ['10.0.0.0/33', 'nope', '10.0.0.0/8/1', '::/129']) {
      assert.throws(() => ArkimeDb.condOps({ $cidr: bad }), /Invalid db where \$cidr/, bad);
    }
    assert.deepEqual(ArkimeDb.pathValues({ a: [{ b: 1 }, { b: [2, null] }, { c: 3 }], d: null }, 'a.b'), [1, 2]);
  });

  test('es find takes from and size as query strings', async () => {
    const calls = [];
    const client = { search: async (p) => { calls.push(p); return { body: { hits: { total: 300, hits: [] } } }; } };
    const pages = ArkimeDb.connect('http://127.0.0.1:1', { client, prefix: 'pages_' }).collection('items');
    await pages.find({ from: '100', size: '50' });
    await pages.find({ from: { x: 1 }, size: '10' });
    assert.deepEqual(calls.map(p => [p.scroll, p.body.from, p.body.size]), [[undefined, 100, 50], [undefined, 0, 10]]);
  });

  test('postgres verifies remote servers like OpenSearch/Elasticsearch', async () => {
    const PGConnection = cjsRequire('../common/arkimeDb.pg.js');
    const ssl = async (url, options = {}) => {
      const c = new PGConnection(url, options);
      await c.close();
      return c.pool.options.ssl;
    };
    assert.deepEqual(await ssl('postgres://u@db.example.com/x'), { rejectUnauthorized: true });
    assert.deepEqual(await ssl('postgresql://u@10.1.2.3/x'), { rejectUnauthorized: true });
    // --insecure still requires TLS
    assert.deepEqual(await ssl('postgres://u@db.example.com/x', { insecure: true }), { rejectUnauthorized: false });
    // ssl settings in the url win, loopback and unix sockets are left alone
    for (const url of ['postgres://u@db.example.com/x?sslmode=disable', 'postgres://u@127.0.0.1/x', 'postgres://u@localhost:5432/x', 'postgres://u@[::1]/x', 'postgres://u@/x?host=/tmp']) {
      assert.equal(await ssl(url), undefined, url);
    }
  });

  test('unknown schemes throw', () => {
    assert.throws(() => ArkimeUtil.dbConnect('nope://x'), /Unknown db url scheme 'nope'/);
    assert.throws(() => ArkimeUtil.dbConnect('lmdb:///tmp/x'), /no longer supported/);
    assert.throws(() => ArkimeUtil.dbConnect('redis://localhost:6379/0'), /no longer supported/);
  });

  after(async () => {
    await ArkimeDb.closeAll();
    fs.rmSync(tmp, { recursive: true, force: true });
  });
});
