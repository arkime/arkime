/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

MySQL only tests for the mysqldb plugin, the shared ArkimeDb conformance tests run it with
  ARKIME_TEST_DB_PLUGIN=capture/plugins/mysqldb/mysqldb.js ARKIME_TEST_DB_URL=mysql://user:pass@localhost/arkime npm run test:unit
These need a MySQL too, otherwise only the TLS option tests run:
  ARKIME_TEST_DB_URL=mysql://user:pass@localhost/arkime node --test capture/plugins/mysqldb/mysqldb.unit.mjs
*/
import { test, describe, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import path from 'node:path';

const cjsRequire = createRequire(import.meta.url);
const ArkimeDb = cjsRequire('../../../common/arkimeDb.js');
ArkimeDb.loadDbPlugin(path.resolve(import.meta.dirname, 'mysqldb.js'));
const { MySQLConnection } = cjsRequire('./mysqldb.js');

const url = process.env.ARKIME_TEST_DB_URL?.startsWith('mysql:') ? process.env.ARKIME_TEST_DB_URL : undefined;
const run = Date.now().toString(36);
const prefix = `mysqldb_unit_${run}_`;
const ids = (result) => result.data.map(h => h.id).sort().join('');
const order = (result) => result.data.map(h => h.id).join('');

describe('mysqldb tls', () => {
  test('remote servers are verified like OpenSearch/Elasticsearch', async () => {
    const ssl = async (u, options = {}) => {
      const c = new MySQLConnection(u, options);
      await c.close();
      return c.pool.pool.config.connectionConfig.ssl;
    };
    assert.deepEqual(await ssl('mysql://u@db.example.com/x'), { rejectUnauthorized: true });
    assert.deepEqual(await ssl('mysql://u@10.1.2.3/x'), { rejectUnauthorized: true });
    // --insecure still requires TLS
    assert.deepEqual(await ssl('mysql://u@db.example.com/x', { insecure: true }), { rejectUnauthorized: false });
    assert.deepEqual(await ssl('mysql://u@db.example.com/x?ssl-mode=REQUIRED'), { rejectUnauthorized: false });
    assert.equal(typeof (await ssl('mysql://u@db.example.com/x?ssl-mode=VERIFY_CA')).checkServerIdentity, 'function');
    // ssl-mode in the url wins, loopback and unix sockets are left alone
    for (const u of ['mysql://u@db.example.com/x?ssl-mode=DISABLED', 'mysql://u@127.0.0.1/x', 'mysql://u@localhost:3306/x', 'mysql://u@[::1]/x', 'mysql://u@db.example.com/x?socket=/tmp/mysql.sock']) {
      assert.ok(!await ssl(u), u);
    }
    assert.throws(() => new MySQLConnection('mysql://u@db.example.com/x?ssl-mode=PREFERRED', {}), /isn't one of/);
    assert.throws(() => new MySQLConnection('mysql://u@db.example.com/', {}), /needs a database/);
  });
});

describe('mysqldb', { skip: !url && 'set ARKIME_TEST_DB_URL to a mysql:// url' }, () => {
  let conn;

  before(() => {
    conn = new MySQLConnection(url, { prefix });
  });

  after(async () => {
    const [rows] = await conn.pool.query('SELECT TABLE_NAME AS t FROM information_schema.TABLES WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME LIKE ?', ['mysqldb\\_unit\\_%']);
    for (const { t } of rows) { await conn.pool.query(`DROP TABLE \`${t}\``); }
    await conn.close();
  });

  test('typed collections keep each field in its own column', async () => {
    const v1 = { nodeName: { type: 'keyword', indexed: true }, interval: { type: 'short', indexed: true }, currentTime: { type: 'date', indexed: 'sort' }, cpu: 'long', memoryP: 'float', ver: 'text', up: 'boolean' };
    const old = conn.collection('typed', { typed: true, fields: v1 });
    await old.put('a-5', { nodeName: 'a', interval: 5, currentTime: 100, cpu: 7, memoryP: 1.5, ver: '6.0', up: true });
    await old.put('b-5', { nodeName: 'b', interval: 5, currentTime: 200, cpu: 3 });
    await old.put('a-60', { nodeName: 'a', interval: 60, currentTime: 300, cpu: 9, up: false });
    assert.deepEqual(await old.get('a-5'), { nodeName: 'a', interval: 5, currentTime: 100, cpu: 7, memoryP: 1.5, ver: '6.0', up: true });
    assert.deepEqual(await old.get('b-5'), { nodeName: 'b', interval: 5, currentTime: 200, cpu: 3 }, 'null columns are left out');

    const [cols] = await conn.pool.query('SELECT COLUMN_NAME AS c, COLUMN_TYPE AS t FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ? ORDER BY COLUMN_NAME', [`${prefix}typed`]);
    assert.deepEqual(Object.fromEntries(cols.map(r => [r.c, r.t])), {
      cpu: 'bigint', currentTime: 'bigint', id: 'varbinary(3072)', interval: 'smallint', memoryP: 'double', nodeName: 'varchar(768)', up: 'tinyint(1)', ver: 'varchar(768)', version: 'int'
    });

    assert.equal(ids(await old.find({ where: { interval: 5, currentTime: { $gte: 150 } } })), 'b-5');
    assert.equal(ids(await old.find({ where: { nodeName: { $in: ['a'] }, cpu: { $ne: 7 } } })), 'a-60');
    assert.equal(ids(await old.find({ where: { memoryP: { $exists: false } } })), 'a-60b-5');
    assert.equal(ids(await old.find({ where: { nope: { $exists: false } } })), 'a-5a-60b-5', 'a field with no column is never there');
    assert.equal(ids(await old.find({ where: { up: false } })), 'a-60');
    assert.equal((await old.find({ where: { nope: 1 } })).total, 0);
    assert.equal((await old.find({ where: { interval: '5' } })).total, 2, 'a numeric string matches a number column');
    assert.equal(order(await old.find({ sort: { currentTime: 'desc' } })), 'a-60b-5a-5');
    assert.equal(order(await old.find({ sort: { memoryP: 'asc' } })).slice(0, 3), 'a-5', 'nulls sort last');
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
    const newer = new MySQLConnection(url, { prefix });
    try {
      const upgraded = newer.collection('typed', { typed: true, fields: { ...v1, totalK: 'long' } });
      await upgraded.put('d-5', { nodeName: 'd', interval: 5, totalK: 42 });
      assert.deepEqual(await upgraded.get('a-60'), { nodeName: 'a', interval: 60, currentTime: 300, cpu: 9, up: false });
      const again = new MySQLConnection(url, { prefix });
      assert.deepEqual(await again.collection('typed', { typed: true, fields: v1 }).get('d-5'), { nodeName: 'd', interval: 5, totalK: 42 });
      await again.close();
    } finally {
      await newer.close();
    }

    assert.equal(await old.deleteWhere({ nodeName: 'a' }), 2);
    assert.equal(await old.count(), 2);
    await assert.rejects(old.nextSequence('x'), /isn't supported/);
  });

  test('indexed fields are generated columns that queries use the index for', async () => {
    const t = `${prefix}indexed`;
    const coll = conn.collection('indexed', { fields: { creator: { type: 'keyword', indexed: true }, n: { type: 'long', indexed: 'sort' }, ip: { type: 'ip', indexed: true }, up: { type: 'boolean', indexed: true } } });
    await coll.init();
    const [cols] = await conn.pool.query("SELECT COLUMN_NAME AS c FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ? AND EXTRA LIKE '%GENERATED%' ORDER BY COLUMN_NAME", [t]);
    assert.deepEqual(cols.map(r => r.c), ['f_creator', 'f_ip', 'f_n', 'f_up']);

    await conn.pool.query(`INSERT /*+ SET_VAR(cte_max_recursion_depth = 10000) */ INTO \`${t}\` (id, doc) WITH RECURSIVE seq (i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM seq WHERE i < 3000)
      SELECT CONCAT('r', i), JSON_OBJECT('creator', CONCAT('c', i), 'n', i, 'ip', CONCAT('10.9.', i DIV 256, '.', i MOD 256), 'up', i % 2 = 0) FROM seq`);
    await conn.pool.query(`ANALYZE TABLE \`${t}\``);

    const execute = conn.pool.execute;
    const seen = [];
    conn.pool.execute = function (q, p) { seen.push([q, p]); return execute.call(this, q, p); };
    try {
      assert.equal(ids(await coll.find({ where: { creator: 'c7' } })), 'r7');
      assert.equal((await coll.find({ where: { ip: { $cidr: '10.9.1.0/24' } } })).total, 256);
      assert.equal(order(await coll.find({ sort: { n: 'desc' }, size: 2 })), 'r3000r2999');
      assert.equal(order(await coll.find({ sort: { n: 'asc' }, size: 2 })), 'r1r2');
    } finally {
      conn.pool.execute = execute;
    }
    const plan = async (match) => {
      const [sql, params] = seen.find(([q, p]) => q.includes('LIMIT') && match(q, p));
      const [rows] = await conn.pool.execute(`EXPLAIN FORMAT=TREE ${sql}`, params);
      return Object.values(rows[0])[0];
    };
    assert.match(await plan(q => q.includes('f_creator =')), /Index lookup on .* using f_creator/);
    assert.match(await plan(q => q.includes('f_ip BETWEEN')), /Index range scan on .* using f_ip/);
    assert.match(await plan(q => q.includes('f_n DESC')), /Index scan on .* using f_n \(reverse\)/);
    assert.match(await plan(q => q.includes('f_n IS NULL')), /Index scan on .* using f_n_nl/);
  });

  test('indexed columns are exact past what they hold', async () => {
    const coll = conn.collection('exact', { fields: { name: { type: 'keyword', indexed: true }, n: { type: 'long', indexed: 'sort' } } });
    const long = 'x'.repeat(800);
    await coll.put('a', { name: `${long}a`, n: 2 ** 53 });
    await coll.put('b', { name: `${long}b`, n: 2 ** 53 + 2 });
    await coll.put('c', { name: 5, n: 'five' });
    assert.equal(ids(await coll.find({ where: { name: `${long}b` } })), 'b', 'a keyword longer than its column');
    assert.equal(ids(await coll.find({ where: { name: { $in: [`${long}a`, 5] } } })), 'ac');
    assert.equal(ids(await coll.find({ where: { n: 2 ** 53 + 2 } })), 'b', 'a number past a double');
    assert.equal(ids(await coll.find({ where: { name: 5 } })), 'c', 'a value of another type');
    assert.equal(ids(await coll.find({ where: { n: 'five' } })), 'c');
    assert.equal(ids(await coll.find({ where: { name: { $gt: `${long}a` } } })), 'b');
  });

  test('tables can be created by several processes at once', async () => {
    const conns = Array.from({ length: 6 }, () => new MySQLConnection(url, { prefix }));
    await Promise.all(conns.map(c => c.collection('race', { fields: { a: { type: 'keyword', indexed: 'sort' } } }).init()));
    await Promise.all(conns.map(c => c.close()));
  });

  test('a lost connection is replaced', async () => {
    const idle = new MySQLConnection(url, { prefix });
    try {
      const [[{ id }]] = await idle.pool.query('SELECT CONNECTION_ID() AS id');
      await conn.pool.query(`KILL ${Number(id)}`);
      await new Promise(resolve => setTimeout(resolve, 200));
      assert.equal((await idle.pool.query('SELECT 1 AS one'))[0][0].one, 1);
    } finally {
      await idle.close();
    }
  });

  test('meta is per prefix', async () => {
    const other = conn.withPrefix(`${prefix}other_`);
    await other.collection('items').setMeta({ version: 99 });
    await conn.collection('items').setMeta({ version: 4 });
    assert.equal(await other.collection('items').getMeta('version'), 99);
    assert.equal(await conn.collection('items').getMeta('version'), 4);
  });

  test('a lone surrogate is stored as U+FFFD and a NUL is kept', async () => {
    const coll = conn.collection('items');
    await coll.put('nul', { name: 'a\u0000b', s: '\ud800', ok: '\\ud800 😀' });
    assert.deepEqual(await coll.get('nul'), { name: 'a\u0000b', s: '�', ok: '\\ud800 😀' });
    assert.equal((await coll.find({ where: { name: 'a\u0000b' } })).total, 1);
  });

  test('ids sort and compare by their bytes', async () => {
    const coll = conn.collection('bytes');
    for (const id of ['a', 'B', 'a ', 'é', 'z']) { await coll.put(id, { id }); }
    assert.deepEqual((await coll.find({ sort: { id: 'asc' } })).data.map(h => h.id), ['B', 'a', 'a ', 'z', 'é']);
    assert.equal((await coll.find({ where: { id: 'a' } })).total, 1, 'no trailing space padding');
    assert.equal((await coll.find({ where: { id: { $wildcard: '?' } } })).total, 4, '? is one character');
  });

  test('servers older than the minimum are refused', async () => {
    const want = MySQLConnection.MIN_VERSION;
    const c = new MySQLConnection(url, { prefix });
    try {
      MySQLConnection.MIN_VERSION = [999, 0];
      await assert.rejects(c.collection('old').init(), /is too old, mysql:\/\/ needs 999.0 or newer/);
    } finally {
      MySQLConnection.MIN_VERSION = want;
      await c.close();
    }
  });
});
