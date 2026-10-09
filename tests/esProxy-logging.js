/* ESPROXY logging regression tests. No Elasticsearch cluster required.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const assert = require('node:assert/strict');
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const { test } = require('node:test');
const { gzipSync } = require('node:zlib');

const password = 'esproxy-test-password-not-for-logs';
const payload = 'esproxy-test-payload-not-for-logs';
const sensors = `sensor=pass:${password}\niponly=ip:127.0.0.1`;
const authorization = `Basic ${Buffer.from(`sensor:${password}`).toString('base64')}`;

function launchProxy (t, upstream, entries, debug = false) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'arkime-esproxy-logging-'));
  const config = path.join(dir, 'config.ini');
  fs.writeFileSync(config, `[default]\nelasticsearch=${upstream}\nprefix=tests\nesProxyHost=127.0.0.1\nesProxyPort=0\n\n[esproxy-sensors]\n${entries}\n`);
  const env = Object.fromEntries(Object.entries(process.env).filter(([key]) => !/^ARKIME/i.test(key) && key !== 'NODE_OPTIONS'));
  const child = spawn(process.execPath, [path.resolve(__dirname, '../viewer/esProxy.js'), '-c', config, '-n', 'esproxy', ...(debug ? ['--debug'] : [])], { env });
  let output = '';
  child.stdout.on('data', chunk => { output += chunk; });
  child.stderr.on('data', chunk => { output += chunk; });
  const completion = once(child, 'close');
  let stopped = false;
  async function stop () {
    if (!stopped) {
      stopped = true;
      child.kill('SIGTERM');
      const timer = setTimeout(() => child.kill('SIGKILL'), 5000);
      try { await completion; } finally { clearTimeout(timer); }
    }
  }
  t.after(async () => {
    await stop();
    fs.rmSync(dir, { recursive: true, force: true });
  });
  const ready = new Promise((resolve, reject) => {
    const timer = setTimeout(() => finish(new Error('ESPROXY startup timed out')), 20000);
    function finish (err, port) {
      clearTimeout(timer);
      child.stdout.off('data', check);
      child.off('close', exited);
      child.off('error', finish);
      if (err) { reject(err); } else { resolve(`http://127.0.0.1:${port}`); }
    }
    function check () {
      const match = output.match(/listening on host 127\.0\.0\.1 port (\d+)/);
      if (match) { finish(null, match[1]); }
    }
    function exited () { finish(new Error('ESPROXY exited before listening')); }
    child.stdout.on('data', check);
    child.once('close', exited);
    child.once('error', finish);
  });
  return { ready, completion, stop, logs: () => output };
}

async function request (base, url, body, auth = authorization, encoding) {
  const headers = { 'content-type': 'application/json' };
  if (auth !== null) { headers.authorization = auth; }
  if (encoding) { headers['content-encoding'] = encoding; }
  const response = await fetch(base + url, { method: body === undefined ? 'GET' : 'POST', headers, body, signal: AbortSignal.timeout(5000) });
  await response.text();
  return response.status;
}

for (const debug of [false, true]) {
  test(`ESPROXY omits credentials and request bodies (debug=${debug})`, { timeout: 30000 }, async t => {
    const forwarded = [];
    const upstream = http.createServer(async (req, res) => {
      const chunks = [];
      for await (const chunk of req) { chunks.push(chunk); }
      forwarded.push({ url: req.url, body: Buffer.concat(chunks).toString() });
      res.setHeader('content-type', 'application/json');
      res.end('{"ok":true}');
    });
    upstream.listen(0, '127.0.0.1');
    await once(upstream, 'listening');
    t.after(async () => {
      upstream.closeAllConnections();
      await new Promise(resolve => upstream.close(resolve));
    });
    const proxy = launchProxy(t, `http://127.0.0.1:${upstream.address().port}`, sensors, debug);
    const base = await proxy.ready;

    assert.equal(await request(base, '/', undefined, null), 401, 'missing credentials');
    assert.equal(await request(base, '/', undefined, `Basic ${Buffer.from('sensor:wrong').toString('base64')}`), 401, 'wrong password');
    assert.equal(await request(base, '/', undefined, `Basic ${Buffer.from('unknown:wrong').toString('base64')}`), 401, 'unknown sensor');
    assert.equal(await request(base, '/', undefined, `Basic ${Buffer.from('__proto__:wrong').toString('base64')}`), 401, 'unconfigured prototype property');
    assert.equal(await request(base, '/'), 200, 'correct password');
    assert.equal(await request(base, '/', undefined, `Basic ${Buffer.from('iponly:unused').toString('base64')}`), 200, 'IP-only sensor');

    const bulk = `{"index":{"_index":"tests_sessions3-261009"}}\n${JSON.stringify({ message: payload })}\n`;
    assert.equal(await request(base, '/_bulk', bulk), 200, 'valid bulk is forwarded');
    assert.equal(forwarded.at(-1).body, bulk, 'bulk body is unchanged');
    const beforeRejected = forwarded.length;
    for (const invalid of [
      JSON.stringify({ unsupported: payload }),
      JSON.stringify({ index: { _index: payload } }),
      `{"${payload}"`,
      JSON.stringify({ index: {}, extra: payload }),
      JSON.stringify({ index: null, message: payload })
    ]) {
      assert.equal(await request(base, '/_bulk', invalid), 400, 'invalid bulk is rejected');
    }
    assert.equal(await request(base, '/_bulk', gzipSync(`{"${payload}"`), authorization, 'gzip'), 400, 'invalid compressed bulk is rejected');
    assert.equal(await request(base, '/not-authorized', payload), 400, 'unapproved POST is rejected');
    assert.equal(await request(base, '/not-authorized', ''), 400, 'empty POST is rejected');
    assert.equal(await request(base, '/tests_sessions3-261009/_update/example', JSON.stringify({ doc: { message: payload } })), 400, 'invalid session update is rejected');
    assert.equal(forwarded.length, beforeRejected, 'rejected bodies never reach upstream');

    const update = JSON.stringify({ script: { source: 'ctx._source.test = params.test', params: { test: payload } } });
    assert.equal(await request(base, '/tests_sessions3-261009/_update/example', update), 200, 'valid update is forwarded');
    assert.equal(forwarded.at(-1).body, update, 'update body is unchanged');
    await proxy.stop();
    const logs = proxy.logs();
    assert.ok(!logs.includes(password), 'sensor password is absent from logs');
    assert.ok(!logs.includes(payload), 'request payload is absent from logs');
    assert.match(logs, /ESPROXY sensors configured: 2/, 'startup reports sensor count');
    assert.match(logs, /Bulk validation failed at line 1/, 'bulk failure has a safe diagnostic');
    assert.match(logs, /POST failed .* body bytes: \d+/, 'POST failure reports body size');
    if (debug) { assert.match(logs, /UPDATE body bytes: \d+/, 'debug update reports body size'); }
  });
}

test('empty allowlist still starts and denies access', { timeout: 30000 }, async t => {
  const proxy = launchProxy(t, 'http://127.0.0.1:1', '');
  assert.equal(await request(await proxy.ready, '/'), 401);
  await proxy.stop();
  assert.match(proxy.logs(), /ESPROXY sensors configured: 0/);
});

test('invalid sensor config exits without logging other sensor passwords', { timeout: 30000 }, async t => {
  const proxy = launchProxy(t, 'http://127.0.0.1:1', `${sensors}\nbad=pass:`);
  await assert.rejects(proxy.ready, /exited before listening/);
  const [code] = await proxy.completion;
  assert.equal(code, 1);
  assert.match(proxy.logs(), /ERROR - esproxy-sensors 'bad'/);
  assert.ok(!proxy.logs().includes(password));
});
