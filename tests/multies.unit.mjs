/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Unit tests for the multi viewer's request handling. Run with `npm run test:unit`.

These run the real viewer/multies.js against two fake clusters on 127.0.0.1,
so they need no Elasticsearch. tests.pl can't cover a failed merge: its multi
viewer clusters always answer in a shape the merge accepts.
*/
import { test, describe, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const VIEWER = path.join(path.dirname(fileURLToPath(import.meta.url)), '..', 'viewer');

// what every fake cluster answers to a search's aggregations
let answerAggs = () => ({});

function startFakeCluster () {
  const server = http.createServer((req, res) => {
    let body = '';
    req.on('data', (c) => { body += c; });
    req.on('end', () => {
      res.setHeader('content-type', 'application/json');
      if (req.url === '/' || req.url.startsWith('/?')) {
        return res.end(JSON.stringify({ version: { distribution: 'opensearch', number: '2.19.0' } }));
      }
      if (/_search/.test(req.url)) {
        const q = JSON.parse(body || '{}');
        return res.end(JSON.stringify({ took: 1, timed_out: false, hits: { total: 1, hits: [] }, aggregations: answerAggs(q) }));
      }
      return res.end('{}');
    });
  });
  return new Promise((resolve) => server.listen(0, '127.0.0.1', () => resolve(server)));
}

function freePort () {
  const server = http.createServer();
  return new Promise((resolve) => server.listen(0, '127.0.0.1', () => {
    const { port } = server.address();
    server.close(() => resolve(port));
  }));
}

// Starts multies in front of two fake clusters, extra holds more [multi] settings.
// Everything is on the returned object as it is made, so stopMulties can clean up a failed start
async function startMulties (m, extra = []) {
  m.out = ''; // multies' stdout + stderr
  m.clusters = await Promise.all([startFakeCluster(), startFakeCluster()]);
  const [a, b] = m.clusters.map(s => s.address().port);
  const port = await freePort();
  m.url = `http://127.0.0.1:${port}`;
  m.dir = fs.mkdtempSync(path.join(os.tmpdir(), 'multies-unit-'));
  const ini = path.join(m.dir, 'config.ini');
  fs.writeFileSync(ini, [
    '[default]',
    `elasticsearch=http://127.0.0.1:${a}`,
    '[multi]',
    'multiES=true',
    'multiESHost=127.0.0.1',
    `multiESPort=${port}`,
    `multiESNodes=127.0.0.1:${a},prefix:a_,name:siteA;127.0.0.1:${b},prefix:b_,name:siteB`,
    ...extra,
    ''
  ].join('\n'));

  m.multies = spawn(process.execPath, [path.join(VIEWER, 'multies.js'), '-c', ini, '-n', 'multi'], { cwd: VIEWER, stdio: ['ignore', 'pipe', 'pipe'] });
  m.multies.stdout.on('data', (d) => { m.out += d; });
  m.multies.stderr.on('data', (d) => { m.out += d; });
  const started = Date.now();
  while (!m.out.includes('listening on')) {
    if (m.multies.exitCode !== null || Date.now() - started > 15000) { throw new Error(`multies did not start:\n${m.out}`); }
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
}

async function stopMulties (m) {
  if (m.multies?.exitCode === null) {
    await new Promise((resolve) => { m.multies.once('exit', resolve); m.multies.kill('SIGKILL'); });
  }
  await Promise.all((m.clusters || []).map(s => new Promise((resolve) => { s.closeAllConnections(); s.close(resolve); })));
  if (m.dir) { fs.rmSync(m.dir, { recursive: true, force: true }); }
}

// one over the largest request body multies reads
const TOO_LARGE = Buffer.alloc(32 * 1024 * 1024 + 1, 0x20);

describe('multies', () => {
  const m = {};

  before(() => startMulties(m));
  after(() => stopMulties(m));

  const post = (urlPath, body) => fetch(`${m.url}${urlPath}`, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body,
    signal: AbortSignal.timeout(5000)
  });
  const search = (qs = '') => post(`/MULTIPREFIX_sessions3-*/_search${qs}`, JSON.stringify({ size: 0, aggregations: { bytes: { sum: { field: 'network.bytes' } } } }));
  const msearch = (qs, ...queries) => post(`/_msearch${qs}`, queries.map(q => `{}\n${JSON.stringify(q)}\n`).join(''));

  test('merges the clusters\' aggregations', async () => {
    answerAggs = () => ({ bytes: { value: 10 } });
    const res = await search();
    assert.equal(res.status, 200);
    assert.equal((await res.json()).aggregations.bytes.value, 20);
  });

  test('a merge that throws answers 500 instead of leaving the request open', async () => {
    // an aggregation the search never asked for has nothing to merge into
    answerAggs = () => ({ unexpected: { value: 1 } });
    const res = await search();
    assert.equal(res.status, 500);
    assert.match((await res.json()).error, /merge/);
  });

  test('the merge error is logged even when the url has format specifiers', async () => {
    answerAggs = () => ({ unexpected: { value: 1 } });
    const res = await search('?x=%c');
    assert.equal(res.status, 500);
    const started = Date.now();
    while (!/x=%c merging cluster results [\s\S]*TypeError/.test(m.out) && Date.now() - started < 2000) {
      await new Promise((resolve) => setTimeout(resolve, 20));
    }
    assert.match(m.out, /x=%c merging cluster results [\s\S]*TypeError/);
  });

  test('an msearch no cluster answers gets a 500', async () => {
    const res = await msearch('?cluster=nosuchcluster', { size: 0 });
    assert.equal(res.status, 500);
    assert.match((await res.json()).error, /No cluster answered/);
  });

  test('an msearch no cluster answers gets a 500 after an async files lookup too', async () => {
    // a fileand term makes the query rewrite look up files first, so the
    // merge runs later, outside the express handler
    const res = await msearch('?cluster=nosuchcluster', { size: 0, query: { fileand: 'test.pcap' } });
    assert.equal(res.status, 500);
    assert.match((await res.json()).error, /No cluster answered/);
  });

  test('a request body over 32MB is refused', async () => {
    const res = await fetch(`${m.url}/MULTIPREFIX_sessions3-*/_search`, { method: 'POST', body: TOO_LARGE, signal: AbortSignal.timeout(20000) });
    assert.equal(res.status, 413);
    // and multies still answers
    answerAggs = () => ({ bytes: { value: 10 } });
    assert.equal((await search()).status, 200);
  });
});

describe('multies with multiESBasicAuth', () => {
  const m = {};
  const authorization = 'Basic ' + Buffer.from('user:pass').toString('base64');

  before(() => startMulties(m, ['multiESBasicAuth=user:pass']));
  after(() => stopMulties(m));

  const post = (headers, body) => fetch(`${m.url}/MULTIPREFIX_sessions3-*/_search`, {
    method: 'POST',
    headers: { 'content-type': 'application/json', ...headers },
    body,
    signal: AbortSignal.timeout(20000)
  });

  test('the right credentials are let through', async () => {
    const res = await post({ authorization }, '{"size":0}');
    assert.equal(res.status, 200);
  });

  test('auth is checked before the body, so a too large one gets a 401 not a 413', async () => {
    assert.equal((await post({}, TOO_LARGE)).status, 401);
    assert.equal((await post({ authorization: 'Basic ' + Buffer.from('user:wrong').toString('base64') }, TOO_LARGE)).status, 401);
    assert.equal((await post({ authorization }, TOO_LARGE)).status, 413);
  });
});
