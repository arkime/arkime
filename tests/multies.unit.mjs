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

describe('multies', () => {
  let clusters, multies, url, dir;

  before(async () => {
    clusters = await Promise.all([startFakeCluster(), startFakeCluster()]);
    const [a, b] = clusters.map(s => s.address().port);
    const port = await freePort();
    url = `http://127.0.0.1:${port}`;
    dir = fs.mkdtempSync(path.join(os.tmpdir(), 'multies-unit-'));
    const ini = path.join(dir, 'config.ini');
    fs.writeFileSync(ini, [
      '[default]',
      `elasticsearch=http://127.0.0.1:${a}`,
      '[multi]',
      'multiES=true',
      'multiESHost=127.0.0.1',
      `multiESPort=${port}`,
      `multiESNodes=127.0.0.1:${a},prefix:a_,name:siteA;127.0.0.1:${b},prefix:b_,name:siteB`,
      ''
    ].join('\n'));

    multies = spawn(process.execPath, [path.join(VIEWER, 'multies.js'), '-c', ini, '-n', 'multi'], { cwd: VIEWER, stdio: ['ignore', 'pipe', 'pipe'] });
    let out = '';
    multies.stdout.on('data', (d) => { out += d; });
    multies.stderr.on('data', (d) => { out += d; });
    const started = Date.now();
    while (!out.includes('listening on')) {
      if (multies.exitCode !== null || Date.now() - started > 15000) { throw new Error(`multies did not start:\n${out}`); }
      await new Promise((resolve) => setTimeout(resolve, 50));
    }
  });

  after(async () => {
    if (multies?.exitCode === null) {
      await new Promise((resolve) => { multies.once('exit', resolve); multies.kill('SIGKILL'); });
    }
    await Promise.all((clusters || []).map(s => new Promise((resolve) => { s.closeAllConnections(); s.close(resolve); })));
    if (dir) { fs.rmSync(dir, { recursive: true, force: true }); }
  });

  const search = () => fetch(`${url}/MULTIPREFIX_sessions3-*/_search`, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ size: 0, aggregations: { bytes: { sum: { field: 'network.bytes' } } } }),
    signal: AbortSignal.timeout(5000)
  });

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
});
