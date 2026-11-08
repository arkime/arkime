/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

MongoDB only tests for the mongodb plugin, the shared ArkimeDb conformance tests run it with
  ARKIME_TEST_DB_PLUGIN=capture/plugins/mongodb/mongodb.js ARKIME_TEST_DB_URL=mongodb://localhost/arkime npm run test:unit
These need a MongoDB too, otherwise only the TLS and pattern tests run:
  ARKIME_TEST_DB_URL=mongodb://localhost/arkime node --test capture/plugins/mongodb/mongodb.unit.mjs
*/
import { test, describe, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import path from 'node:path';

const cjsRequire = createRequire(import.meta.url);
const ArkimeDb = cjsRequire('../../../common/arkimeDb.js');
ArkimeDb.loadDbPlugin(path.resolve(import.meta.dirname, 'mongodb.js'));
const { MongoConnection, sessionDoc } = cjsRequire('./mongodb.js');
const EsQuery = cjsRequire('../../../viewer/esQuery.js');

const url = process.env.ARKIME_TEST_DB_URL?.startsWith('mongodb') ? process.env.ARKIME_TEST_DB_URL : undefined;
const run = Date.now().toString(36);
const prefix = `mongodb_unit_${run}_`;
const ids = (result) => result.data.map(h => h.id).sort().join('');
const order = (result) => result.data.map(h => h.id).join('');

describe('mongodb tls', () => {
  test('remote servers are verified like OpenSearch/Elasticsearch', async () => {
    const tls = async (u, options = {}) => {
      const c = new MongoConnection(u, options);
      const o = c.client.options;
      await c.close();
      return { tls: o.tls, insecure: o.rejectUnauthorized === false };
    };
    assert.deepEqual(await tls('mongodb://db.example.com/x'), { tls: true, insecure: false });
    assert.deepEqual(await tls('mongodb://127.0.0.1,db.example.com/x'), { tls: true, insecure: false }, 'any remote host');
    assert.deepEqual(await tls('mongodb+srv://cluster.example.com/x'), { tls: true, insecure: false });
    // --insecure still requires TLS
    assert.deepEqual(await tls('mongodb://db.example.com/x', { insecure: true }), { tls: true, insecure: true });
    // tls settings in the url win, loopback and unix sockets are left alone
    for (const u of ['mongodb://db.example.com/x?tls=false', 'mongodb://127.0.0.1/x', 'mongodb://localhost:27017/x', 'mongodb://[::1]/x', 'mongodb://%2Ftmp%2Fmongodb-27017.sock/x']) {
      assert.ok(!(await tls(u)).tls, u);
    }
    assert.throws(() => new MongoConnection('mongodb://db.example.com/', {}), /needs a database/);
    assert.equal(MongoConnection.tlsHost('mongodb://127.0.0.1,db.example.com:27018/x'), 'db.example.com');
  });
});

describe('mongodb patterns', () => {
  test('a wildcard is atomic pieces', async () => {
    const conn = new MongoConnection('mongodb://127.0.0.1/x', {});
    const q = {};
    const filter = conn.collection('w').fieldOp('name', '$wildcard', 'x*a?*.b', q);
    await conn.close();
    assert.ok(q.patterns, 'patterns run with the timeout');
    assert.deepEqual(filter, { 'd.name': { $regex: '^x(?>.*?a.).*\\.b$', $options: 's' } });
  });

  test('regex syntax that PCRE and RE2 read differently is refused', () => {
    const conn = new MongoConnection('mongodb://127.0.0.1/x', {});
    const coll = conn.collection('r');
    assert.deepEqual(coll.fieldOp('name', '$regex', 'a(?:b|c)', {}), { 'd.name': { $regex: '^(?:a(?:b|c))\\z', $options: 's' } });
    assert.throws(() => coll.fieldOp('name', '$regex', '(?i)abc', {}), /\(\?flags\) and named groups/);
    assert.throws(() => coll.fieldOp('name', '$regex', '(?<n>a)', {}), /\(\?flags\) and named groups/);
    assert.throws(() => coll.fieldOp('name', '$regex', '(a)\\1', {}), /escape/, 'RE2 has no backreferences');
    assert.doesNotThrow(() => coll.fieldOp('name', '$regex', '\\(?i\\)', {}), 'escaped parens are literal');
    return conn.close();
  });
});

describe('mongodb', { skip: !url && 'set ARKIME_TEST_DB_URL to a mongodb:// url' }, () => {
  let conn;

  before(() => {
    conn = new MongoConnection(url, { prefix });
  });

  after(async () => {
    await ArkimeDb.closeAll();
    for (const { name: collName } of await conn.db.listCollections({ name: { $regex: '^mongodb_unit_' } }, { nameOnly: true }).toArray()) {
      await conn.db.collection(collName).drop();
    }
    await conn.close();
  });

  test('typed collections keep each field as its type', async () => {
    const coll = conn.collection('typed', { typed: true, fields: { nodeName: { type: 'keyword', indexed: true }, interval: { type: 'short', indexed: true }, currentTime: { type: 'date', indexed: 'sort' }, cpu: 'long', ver: 'text', up: 'boolean' } });
    await coll.put('a-5', { nodeName: 'a', interval: '5', currentTime: 100, cpu: 7, ver: 6, up: 1 });
    await coll.put('b-5', { nodeName: 'b', interval: 5, currentTime: 200, cpu: 3 });
    await coll.put('a-60', { nodeName: 'a', interval: 60, currentTime: 300, cpu: 9, up: false });
    assert.deepEqual(await coll.get('a-5'), { nodeName: 'a', interval: 5, currentTime: 100, cpu: 7, ver: '6', up: true }, 'cast to the field types');
    const raw = await conn.db.collection(`${prefix}typed`).findOne({ _id: 'a-5' });
    assert.equal(typeof raw.d.interval, 'number');

    assert.equal(ids(await coll.find({ where: { interval: '5' } })), 'a-5b-5', 'a numeric string matches a number field');
    assert.equal((await coll.find({ where: { interval: 'five' } })).total, 0);
    assert.equal(ids(await coll.find({ where: { nope: { $exists: false } } })), 'a-5a-60b-5', 'a field it doesn\'t have is never there');
    assert.equal(order(await coll.find({ sort: { currentTime: 'asc' } })), 'a-5b-5a-60');
    await assert.rejects(coll.put('c', { nodeName: 'c', bogus: 1 }), /no column for 'bogus'/);
    assert.equal(await coll.update('b-5', { cpu: null, ver: 7 }), true);
    assert.deepEqual(await coll.get('b-5'), { nodeName: 'b', interval: 5, currentTime: 200, ver: '7' }, 'a null update removes the field');
    await assert.rejects(coll.nextSequence('x'), /isn't supported/);
    assert.throws(() => conn.collection('typednested', { typed: true, fields: { 'a.b': 'long' } }), /can't be nested/);
  });

  test('indexed fields, ips and sorts use an index', async () => {
    const coll = conn.collection('indexed', { fields: { creator: { type: 'keyword', indexed: true }, n: { type: 'long', indexed: 'sort' }, ip: { type: 'ip', indexed: true } } });
    await coll.init();
    const names = (await conn.db.collection(`${prefix}indexed`).indexes()).map(i => i.name).sort();
    assert.deepEqual(names, ['_id_', 'f_creator', 'f_ip', 'f_n', 'f_n_nl', 'ips']);

    for (let i = 1; i <= 600; i++) {
      await coll.put(`r${i}`, i % 100 === 0 ? { creator: `c${i}` } : { creator: `c${i}`, n: i, ip: `10.9.${Math.floor(i / 256)}.${i % 256}` });
    }
    assert.equal(ids(await coll.find({ where: { creator: 'c7' } })), 'r7');
    assert.equal((await coll.find({ where: { ip: { $cidr: '10.9.1.0/24' } } })).total, 253);
    assert.equal((await coll.find({ where: { ip: { $cidr: '::ffff:10.9.1.0/120' } } })).total, 253, 'an ipv4 is ::ffff:a.b.c.d');
    assert.equal((await coll.find({ where: { ip: { $cidr: 'fe80::/10' } } })).total, 0);
    assert.equal(order(await coll.find({ sort: { n: 'desc' }, size: 2 })), 'r599r598');
    assert.equal(order(await coll.find({ sort: { n: 'asc' }, size: 2 })), 'r1r2');
    assert.equal(order(await coll.find({ sort: { n: 'asc' }, from: 594 })), 'r100r200r300r400r500r600', 'missing values last');

    const plan = async (filter, sort) => JSON.stringify((await conn.db.collection(`${prefix}indexed`).find(filter).sort(sort ?? {}).limit(2).explain()).queryPlanner.winningPlan);
    assert.match(await plan({ 'd.creator': 'c7' }), /"indexName":"f_creator"/);
    assert.match(await plan({ _ip: { $elemMatch: { f: 'ip', a: { $gte: Buffer.from([...Array(10).fill(0), 255, 255, 10, 9, 1, 0]), $lte: Buffer.from([...Array(10).fill(0), 255, 255, 10, 9, 1, 255]) } } } }), /"indexName":"ips"/);
    assert.match(await plan({}, { 'd.n': -1 }), /"indexName":"f_n"/);
    assert.match(await plan({}, { '_n.n': 1, 'd.n': 1 }), /"indexName":"f_n_nl"/);
  });

  test('updates keep the ips and missing sort fields current', async () => {
    const coll = conn.collection('reindex', { fields: { n: { type: 'long', indexed: 'sort' }, ip: { type: 'ip' } } });
    await coll.put('a', { ip: '1.2.3.4' });
    await coll.put('b', { n: 1, ip: '::1' });
    assert.equal(order(await coll.find({ sort: { n: 'asc' } })), 'ba');
    await coll.update('a', { n: 0, ip: '5.6.7.8' });
    assert.equal(order(await coll.find({ sort: { n: 'asc' } })), 'ab');
    assert.equal(ids(await coll.find({ where: { ip: { $cidr: '5.6.0.0/16' } } })), 'a');
    assert.equal((await coll.find({ where: { ip: { $cidr: '1.2.3.4' } } })).total, 0);
    assert.equal(ids(await coll.find({ where: { ip: { $cidr: '::1/128' } } })), 'b');
  });

  test('a wildcard runs in linear time', async () => {
    const coll = conn.collection('wild');
    await coll.put('long', { name: 'a'.repeat(20000) });
    await coll.put('hit', { name: `x${'a'.repeat(8)}b` });
    const start = Date.now();
    assert.equal(ids(await coll.find({ where: { name: { $wildcard: '*a*a*a*a*a*a*a*a*b' } } })), 'hit');
    assert.ok(Date.now() - start < 2000, `took ${Date.now() - start}ms`);
  });

  test('docs keep $ and . keys and $ strings as they are', async () => {
    const coll = conn.collection('dollars');
    const doc = { 'a.b': 1, $x: '$y', nested: { $literal: '$$ROOT' } };
    await coll.put('d', doc);
    assert.deepEqual(await coll.get('d'), doc);
    await coll.update('d', { 'c.d': '$d', $x: 2 });
    assert.deepEqual(await coll.get('d'), { ...doc, 'c.d': '$d', $x: 2 });
  });

  test('collections can be created by several processes at once', async () => {
    const conns = Array.from({ length: 6 }, () => new MongoConnection(url, { prefix }));
    await Promise.all(conns.map(c => c.collection('race', { fields: { a: { type: 'keyword', indexed: 'sort' } } }).init()));
    await Promise.all(conns.map(c => c.collection('race').put('same', { a: 'x' })));
    assert.equal(await conn.collection('race').count(), 1);
    await Promise.all(conns.map(c => c.close()));
  });

  test('meta is per prefix', async () => {
    const other = conn.withPrefix(`${prefix}other_`);
    await other.collection('items').setMeta({ version: 99 });
    await conn.collection('items').setMeta({ version: 4 });
    assert.equal(await other.collection('items').getMeta('version'), 99);
    assert.equal(await conn.collection('items').getMeta('version'), 4);
  });

  test('servers older than the minimum are refused', async () => {
    const want = MongoConnection.MIN_VERSION;
    const c = new MongoConnection(url, { prefix });
    try {
      MongoConnection.MIN_VERSION = [999, 0];
      await assert.rejects(c.collection('old').init(), /is too old, mongodb:\/\/ needs 999.0 or newer/);
    } finally {
      MongoConnection.MIN_VERSION = want;
      await c.close();
    }
  });

  describe('sessions', () => {
    let sessions;
    const fields = { 'source.ip': { type: 'ip' }, 'destination.port': { type: 'integer' }, 'http.uri': { type: 'termfield' }, 'http.uriTokens': { type: 'lotextfield' }, tags: { type: 'termfield' } };
    const day = Date.UTC(2026, 0, 1);
    const hits = (r) => r.hits.hits.map(h => h._id).sort().join(',');

    before(async () => {
      const Sessions = ArkimeDb.sessionsImpl(url);
      sessions = new Sessions({ sessionsDbUrl: url, prefix, fieldsMap: () => fields, EsQuery });
      const coll = conn.db.collection(`${prefix}sessions3-260101`);
      const docs = [
        ['s1', { firstPacket: day, lastPacket: day + 1000, source: { ip: '10.0.0.1' }, destination: { ip: '2001:db8::1', port: 80 }, http: { uri: ['/a/b.html'], uriTokens: ['a', 'b', 'html'] }, tags: ['x'], node: 'n1', network: { bytes: 100 } }],
        ['s2', { firstPacket: day + 3600000, lastPacket: day + 3601000, source: { ip: '10.0.1.2' }, destination: { ip: '192.168.1.1', port: 443 }, tags: ['x', 'y'], node: 'n1', network: { bytes: 50 }, cert: [{ notAfter: 100 }, { notAfter: 300 }] }],
        ['s3', { firstPacket: day + 7200000, lastPacket: day + 7201000, source: { ip: '172.16.0.1' }, destination: { ip: '192.168.1.2', port: 80 }, node: 'n2', network: { bytes: 25 } }]
      ];
      await coll.insertMany(docs.map(([id, doc]) => sessionDoc(id, doc)));
      await conn.db.createCollection(`${prefix}sessions3-251231`);
    });

    test('es queries compile to the same matches', async () => {
      const q = async (query) => hits(await sessions.searchSessions(`${prefix}sessions3-*`, { query, size: 10 }));
      assert.equal(await q({ term: { 'source.ip': '10.0.0.0/16' } }), 's1,s2', 'a cidr');
      assert.equal(await q({ term: { 'source.ip': '10.0' } }), 's1,s2', 'a partial ip');
      assert.equal(await q({ term: { 'destination.ip': '2001:0db8:0:0::1' } }), 's1', 'another ipv6 form');
      assert.equal(await q({ range: { 'destination.ip': { gte: '192.168.1.2' } } }), 's1,s3', 'ipv4 sorts below ipv6 like ES');
      assert.equal(await q({ range: { 'destination.ip': { gte: '::ffff:192.168.1.2', lt: '2001:db8::' } } }), 's3');
      assert.equal(await q({ term: { 'destination.port': '80' } }), 's1,s3', 'a number as a string');
      assert.equal(await q({ range: { lastPacket: { gte: new Date(day + 3600000).toISOString() } } }), 's2,s3', 'an iso date');
      assert.equal(await q({ query_string: { query: 'http.uri:*b.htm*' } }), 's1');
      assert.equal(await q({ match_phrase: { 'http.uriTokens': 'A/B' } }), 's1', 'tokens are all there');
      assert.equal(await q({ bool: { must_not: { exists: { field: 'tags' } } } }), 's3');
      assert.equal(await q({ bool: { should: [{ term: { node: 'n2' } }, { terms: { tags: ['y'] } }] } }), 's2,s3');
      assert.equal(await q({ ids: { values: ['s2'] } }), 's2');
      assert.equal(await q({ range: { 'cert.notAfter': { gte: 150, lte: 250 } } }), '', 'one value meets every bound');
      assert.equal(await q({ range: { 'cert.notAfter': { gte: 250, lte: 350 } } }), 's2');
      assert.equal(await q({ regexp: { 'http.uri': '/a/[a-z]\\.html' } }), 's1');
      assert.equal(await q({ regexp: { 'http.uri': '/a' } }), '', 'regexps are anchored');
      await assert.rejects(q({ regexp: { 'http.uri': '(a)\\1' } }), (err) => err.statusCode === 400);
    });

    test('hits look like ES ones', async () => {
      const r = await sessions.searchSessions(`${prefix}sessions3-*`, { query: { term: { node: 'n1' } }, sort: [{ lastPacket: { order: 'desc' } }], size: 1 });
      assert.equal(r.hits.total, 2);
      const [hit] = r.hits.hits;
      assert.equal(hit._id, 's2');
      assert.equal(hit._index, `${prefix}sessions3-260101`);
      assert.deepEqual(hit.fields.lastPacket, [new Date(day + 3601000).toISOString()]);
      assert.deepEqual(hit.fields['source.ip'], ['10.0.1.2']);
      assert.equal(hit._source._ip, undefined);
      assert.deepEqual(hit.sort, [day + 3601000, 's2']);
    });

    test('aggregations', async () => {
      const r = await sessions.searchSessions(`${prefix}sessions3-*`, {
        size: 0,
        aggregations: {
          node: { terms: { field: 'node', size: 1 }, aggregations: { 'network.bytes': { sum: { field: 'network.bytes' } } } },
          dst: { terms: { script: { source: 'if (doc["destination.ip"].value.indexOf(".") > 0) {return doc["destination.ip"].value + ":" + doc["destination.port"].value} else {return doc["destination.ip"].value + "." + doc["destination.port"].value}' } } },
          hours: { date_histogram: { field: 'lastPacket', fixed_interval: '3600s', min_doc_count: 0 } },
          tags: { terms: { field: 'tags' } },
          dstIp: { terms: { field: 'destination.ip' } },
          byP: { terms: { field: 'node', order: { 'p[95.0]': 'asc' } }, aggregations: { p: { percentiles: { field: 'network.bytes', percents: [95] } }, a: { avg: { field: 'network.bytes' } } } },
          ips: { cardinality: { field: 'source.ip' } }
        }
      });
      assert.equal(r.hits.total, 3);
      assert.deepEqual(r.aggregations.node.buckets.map(b => [b.key, b.doc_count, b['network.bytes'].value]), [['n1', 2, 150]], 'an agg name with a .');
      assert.deepEqual(r.aggregations.dst.buckets.map(b => b.key).sort(), ['192.168.1.1:443', '192.168.1.2:80', '2001:db8::1.80']);
      assert.equal(r.aggregations.node.sum_other_doc_count, 1);
      assert.deepEqual(r.aggregations.hours.buckets.map(b => b.doc_count), [1, 1, 1]);
      assert.deepEqual(r.aggregations.tags.buckets.map(b => [b.key, b.doc_count]), [['x', 2], ['y', 1]]);
      assert.equal(r.aggregations.ips.value, 3);
      assert.deepEqual(r.aggregations.dstIp.buckets.map(b => b.key), ['192.168.1.1', '192.168.1.2', '2001:db8::1'], 'ip ties order by their bytes like ES');
      assert.deepEqual(r.aggregations.byP.buckets.map(b => [b.key, b.p.value, b.a.value]), [['n2', 25, 25], ['n1', 100, 75]], 'sorted by a percentile');
    });

    test('tags, scroll and indices', async () => {
      const index = `${prefix}sessions3-260101`;
      await sessions.addTagsToSession(index, 's3', ['z']);
      assert.equal(hits(await sessions.searchSessions(index, { query: { term: { tags: 'z' } } })), 's3');
      await sessions.removeTagsFromSession(index, 's3', ['z']);
      assert.equal(hits(await sessions.searchSessions(index, { query: { term: { tags: 'z' } } })), '');
      await assert.rejects(sessions.addTagsToSession(index, 'nope', ['z']), /not found/);

      const pages = [];
      for await (const page of sessions.searchScrollIterator(`${prefix}sessions3-*`, { query: { match_all: {} }, size: 3 })) { pages.push(...page.hits.hits.map(h => h._id)); }
      assert.deepEqual(pages, ['s3', 's2', 's1']);
      const scrolled = await sessions.searchScroll(`${prefix}sessions3-*`, { query: { match_all: {} } }, { scroll: '1m' });
      assert.equal(scrolled._scroll_id, EsQuery.SCROLL_DONE);
      assert.equal(scrolled.hits.hits.length, 3);

      // Pages of query.size in the query's sort, like a hunt reads it
      const scrollQuery = { query: { match_all: {} }, size: 1, sort: [{ lastPacket: { order: 'asc' } }] };
      let chunk = await sessions.searchScroll(`${prefix}sessions3-*`, scrollQuery, { scroll: '1m' });
      const read = [];
      while (chunk.hits.hits.length) {
        assert.equal(chunk.hits.total, 3);
        assert.ok(chunk.hits.hits.length <= 1);
        read.push(...chunk.hits.hits.map(h => h._id));
        if (chunk._scroll_id === EsQuery.SCROLL_DONE) { break; }
        assert.ok(sessions.ownsScroll(chunk._scroll_id));
        chunk = await sessions.scroll(chunk._scroll_id);
      }
      assert.deepEqual(read, ['s1', 's2', 's3']);
      assert.equal(chunk._scroll_id, EsQuery.SCROLL_DONE);

      const cleared = await sessions.searchScroll(`${prefix}sessions3-*`, scrollQuery, { scroll: '1m' });
      await sessions.clearScroll(cleared._scroll_id);
      await assert.rejects(sessions.scroll(cleared._scroll_id), (err) => err.statusCode === 404);
      assert.equal(sessions.ownsScroll(EsQuery.SCROLL_DONE), false);

      assert.equal(await sessions.getIndices(day / 1000, day / 1000 + 3600), index);
      assert.equal(await sessions.getIndices(day / 1000 - 1000, day / 1000 + 3600, 'both'), `${index},${prefix}sessions3-251231`);
    });
  });
});
