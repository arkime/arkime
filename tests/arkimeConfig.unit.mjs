/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for where each db lives and its prefix. Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const configJs = path.join(path.dirname(fileURLToPath(import.meta.url)), '../common/arkimeConfig.js');

// ArkimeConfig loads once per process, so each config gets its own node
function resolve (lines) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'arkimeConfig-'));
  const file = path.join(dir, 'config.ini');
  fs.writeFileSync(file, `[default]\n${lines.join('\n')}\n`);
  const script = `
    const C = require(${JSON.stringify(configJs)});
    (async () => {
      await C.initialize({ defaultConfigFile: ${JSON.stringify(file)}, defaultSections: 'default' });
      console.log(JSON.stringify({
        db: C.dbUrls(), sessions: C.sessionsDbUrls(), sessionsEs: C.sessionsEsUrls(), users: C.usersDbUrls() ?? C.dbUrls(), hasEs: C.hasEs(),
        dbPrefix: C.dbPrefix(), sessionsPrefix: C.sessionsDbPrefix(), usersPrefix: C.usersPrefix(),
        insecure: [['https://localhost:9200'], ['https://es:9200'], ['http://es:9200'], ['postgres://localhost/a'],
          ['postgres://pg.example/a'], ['postgres:///a?host=/tmp'], ['https://127.0.0.1:9200', 'postgres://pg.example/a']].map(u => C.isInsecure(u))
      }));
      process.exit(0);
    })();`;
  try {
    const out = execFileSync(process.execPath, ['-e', script, '--', '-c', file], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] });
    return JSON.parse(out.trim().split('\n').pop());
  } catch (err) {
    return { error: (err.stdout ?? '').trim().split('\n').pop() };
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
}

describe('ArkimeConfig db urls', () => {
  test('nothing set is localhost for everything', () => {
    const r = resolve([]);
    assert.deepEqual(r.db, ['http://localhost:9200']);
    assert.deepEqual(r.sessions, ['http://localhost:9200']);
    assert.deepEqual(r.users, ['http://localhost:9200']);
  });

  test('elasticsearch is everything without dbUrl', () => {
    const r = resolve(['elasticsearch=http://es:9200']);
    assert.deepEqual(r.db, ['http://es:9200']);
    assert.deepEqual(r.sessions, ['http://es:9200']);
    assert.deepEqual(r.users, ['http://es:9200']);
  });

  test('dbUrl beats elasticsearch for everything', () => {
    const r = resolve(['elasticsearch=http://es:9200', 'dbUrl=elasticsearch://db:9200']);
    assert.deepEqual(r.db, ['http://db:9200']);
    assert.deepEqual(r.sessions, ['http://db:9200']);
    assert.deepEqual(r.users, ['http://db:9200']);
  });

  test('sessionsDbUrl only moves the sessions', () => {
    const r = resolve(['dbUrl=postgres://pg/arkime', 'sessionsDbUrl=http://ses:9200']);
    assert.deepEqual(r.db, ['postgres://pg/arkime']);
    assert.deepEqual(r.sessions, ['http://ses:9200']);
    assert.deepEqual(r.sessionsEs, ['http://ses:9200']);
    assert.deepEqual(r.users, ['postgres://pg/arkime']);
  });

  test('usersDbUrl only moves the users', () => {
    const r = resolve(['dbUrl=http://db:9200', 'usersDbUrl=sqlite://users.db']);
    assert.deepEqual(r.sessions, ['http://db:9200']);
    assert.deepEqual(r.users, ['sqlite://users.db']);
  });

  test('ClickHouse sessions keep an ES dbUrl for the rest of the ES work', () => {
    const r = resolve(['dbUrl=http://db:9200', 'sessionsDbUrl=clickhouse://ch:8123']);
    assert.deepEqual(r.sessions, ['clickhouse://ch:8123']);
    assert.deepEqual(r.sessionsEs, ['http://db:9200']);
  });

  test('hasEs follows the sessions and cluster dbs, not the users db', () => {
    assert.equal(resolve([]).hasEs, true);
    assert.equal(resolve(['dbUrl=http://db:9200', 'sessionsDbUrl=clickhouse://ch:8123']).hasEs, true);
    assert.equal(resolve(['dbUrl=postgres://pg/arkime', 'sessionsDbUrl=clickhouse://ch:8123']).hasEs, false);
    // an ES users db alone would point the ES only pages at a default localhost ES
    assert.equal(resolve(['dbUrl=postgres://pg/arkime', 'sessionsDbUrl=clickhouse://ch:8123', 'usersDbUrl=http://users:9200']).hasEs, false);
    assert.equal(resolve(['dbUrl=postgres://pg/arkime', 'sessionsDbUrl=clickhouse://ch:8123', 'elasticsearch=http://es:9200']).hasEs, true);
  });

  test('sessionsDbUrl alone is an error', () => {
    const r = resolve(['sessionsDbUrl=http://ses:9200']);
    assert.match(r.error, /set dbUrl, only the sessions db uses sessionsDbUrl/);
  });
});

describe('ArkimeConfig isInsecure', () => {
  test('only when every url that uses TLS is to this machine', () => {
    assert.deepEqual(resolve([]).insecure, [true, false, false, true, false, true, false]);
  });
});

describe('ArkimeConfig db prefixes', () => {
  test('prefix is everything without the others', () => {
    const r = resolve(['prefix=p']);
    assert.equal(r.dbPrefix, 'p');
    assert.equal(r.sessionsPrefix, 'p');
    assert.equal(r.usersPrefix, 'p');
  });

  test('dbPrefix is for what lives in dbUrl', () => {
    const r = resolve(['prefix=p', 'dbPrefix=d']);
    assert.equal(r.dbPrefix, 'd');
    assert.equal(r.sessionsPrefix, 'd');
    assert.equal(r.usersPrefix, 'd');
  });

  test('a db with its own url skips dbPrefix', () => {
    const r = resolve(['dbUrl=http://db:9200', 'prefix=p', 'dbPrefix=d', 'sessionsDbUrl=http://ses:9200', 'usersDbUrl=http://users:9200']);
    assert.equal(r.dbPrefix, 'd');
    assert.equal(r.sessionsPrefix, 'p');
    assert.equal(r.usersPrefix, 'p');
  });

  test('its own prefix setting wins wherever the db lives', () => {
    for (const urls of [[], ['sessionsDbUrl=http://ses:9200', 'usersDbUrl=http://users:9200']]) {
      const r = resolve(['dbUrl=http://db:9200', 'prefix=p', 'dbPrefix=d', 'sessionsDbPrefix=s', 'usersDbPrefix=u', ...urls]);
      assert.equal(r.sessionsPrefix, 's');
      assert.equal(r.usersPrefix, 'u');
    }
  });

  test('usersPrefix is the older name for usersDbPrefix', () => {
    assert.equal(resolve(['dbPrefix=d', 'usersPrefix=u']).usersPrefix, 'u');
    assert.equal(resolve(['usersDbUrl=http://users:9200', 'usersPrefix=u']).usersPrefix, 'u');
  });
});
