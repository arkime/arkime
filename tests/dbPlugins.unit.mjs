/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for dbPlugins, which add db url schemes to every app. Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const commonDir = path.join(path.dirname(fileURLToPath(import.meta.url)), '../common');

// A tiny in memory backend, a real one implements the whole ArkimeDb.Collection api
const PLUGIN = `
exports.registerDb = (api) => {
  class MemCollection extends api.ArkimeDb.Collection {
    #docs = new Map();
    async get (id) { return this.#docs.get(id) ?? null; }
    async put (id, doc) { id ??= this.newId(); this.#docs.set(id, doc); return id; }
  }
  class MemConnection {
    #colls = new Map();
    constructor (url, options) { this.url = url; this.prefix = options.prefix; }
    collection (name, options) {
      if (!this.#colls.has(name)) { this.#colls.set(name, new MemCollection(name, options)); }
      return this.#colls.get(name);
    }
    async close () {}
    static tlsHost (url) { return new URL(url.replace(/^memdb/, 'http')).hostname; }
  }
  api.registerDbScheme('memdb', MemConnection);
  api.registerDbScheme('memcluster', MemConnection, { cluster: true });
};
`;

// ArkimeConfig loads once per process, so each case gets its own node
function run (lines, body) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'dbPlugins-'));
  fs.writeFileSync(path.join(dir, 'memdb.js'), PLUGIN);
  const file = path.join(dir, 'config.ini');
  fs.writeFileSync(file, `[default]\npluginsDir=${dir}\n${lines.join('\n')}\n`);
  const script = `
    const ArkimeConfig = require(${JSON.stringify(path.join(commonDir, 'arkimeConfig.js'))});
    const ArkimeUtil = require(${JSON.stringify(path.join(commonDir, 'arkimeUtil.js'))});
    (async () => {
      await ArkimeConfig.initialize({ defaultConfigFile: ${JSON.stringify(file)}, defaultSections: 'default' });
      const out = await (async () => { ${body} })();
      console.log(JSON.stringify(out));
      process.exit(0);
    })().catch(err => { console.log(JSON.stringify({ thrown: err.message })); process.exit(0); });`;
  try {
    const out = execFileSync(process.execPath, ['-e', script, '--', '-c', file], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] });
    return JSON.parse(out.trim().split('\n').pop());
  } catch (err) {
    return { error: (err.stdout ?? '').trim().split('\n').pop() };
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
}

describe('dbPlugins', () => {
  test('a plugin scheme works for plain collections', () => {
    const r = run(['dbPlugins=memdb.js'], `
      const conn = ArkimeUtil.dbConnect('memdb://somewhere', { prefix: 'p_' });
      const coll = conn.collection('users');
      await coll.put('alice', { userId: 'alice' });
      return { doc: await coll.get('alice'), prefix: conn.prefix };`);
    assert.deepEqual(r, { doc: { userId: 'alice' }, prefix: 'p_' });
  });

  test('a plugin scheme is not ES and only holds the cluster db when it says so', () => {
    const r = run(['dbPlugins=memdb.js'], `
      return {
        isES: ArkimeUtil.dbIsES('memdb://x'), isEsUrl: ArkimeConfig.isEsUrl('memdb://x'),
        cluster: ArkimeUtil.dbHoldsCluster('memdb://x'), clusterFlag: ArkimeUtil.dbHoldsCluster('memcluster://x')
      };`);
    assert.deepEqual(r, { isES: false, isEsUrl: false, cluster: false, clusterFlag: true });
  });

  test('a plugin scheme answers for its TLS host', () => {
    const r = run(['dbPlugins=memdb.js'], `
      return [ArkimeConfig.isInsecure(['memdb://localhost/x']), ArkimeConfig.isInsecure(['memdb://remote/x'])];`);
    assert.deepEqual(r, [true, false]);
  });

  test('the built in schemes', () => {
    const r = run([], `
      const urls = ['http://es:9200', 'es:9200', 'elasticsearchs://es', 'opensearch://es', 'postgres://pg/a', 'sqlite://a.db', 'clickhouse://ch'];
      return urls.map(u => [ArkimeUtil.dbIsES(u), ArkimeUtil.dbHoldsCluster(u)]);`);
    assert.deepEqual(r, [[true, true], [true, true], [true, true], [true, true], [false, true], [false, false], [false, false]]);
  });

  test('an unknown scheme says to check dbPlugins', () => {
    const r = run([], 'ArkimeUtil.dbConnect(\'memdb://x\', {});');
    assert.match(r.thrown, /Unknown db url scheme 'memdb', is its dbPlugins module loaded/);
  });

  test('a missing plugin is a startup error', () => {
    const r = run(['dbPlugins=nothere.js'], 'return ArkimeUtil.dbIsES(\'http://x\');');
    assert.match(r.error, /Couldn't find dbPlugins 'nothere.js' in pluginsDir/);
  });
});
