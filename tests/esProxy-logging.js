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
const { gzipSync, gunzipSync } = require('node:zlib');

const password = 'esproxy-test-password-not-for-logs';
const payload = 'esproxy-test-payload-not-for-logs';
const sensors = `sensor=pass:${password}\niponly=ip:127.0.0.1`;
const authorization = `Basic ${Buffer.from(`sensor:${password}`).toString('base64')}`;

function launchProxy (t, upstream, entries, debug = 0) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'arkime-esproxy-logging-'));
  const config = path.join(dir, 'config.ini');
  fs.writeFileSync(config, `[default]\nelasticsearch=${upstream}\nprefix=tests\nesProxyHost=127.0.0.1\nesProxyPort=0\n\n[esproxy-sensors]\n${entries}\n`);
  const env = Object.fromEntries(Object.entries(process.env).filter(([key]) => !/^ARKIME/i.test(key) && key !== 'NODE_OPTIONS'));
  const child = spawn(process.execPath, [path.resolve(__dirname, '../viewer/esProxy.js'), '-c', config, '-n', 'esproxy', ...Array(debug).fill('--debug')], { env });
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

function bulkBody (...lines) {
  return lines.map(line => JSON.stringify(line)).join('\n') + '\n';
}

for (const debug of [0, 1, 3]) {
  test(`ESPROXY logging (debug=${debug})`, { timeout: 30000 }, async t => {
    const forwarded = [];
    const upstream = http.createServer(async (req, res) => {
      const chunks = [];
      for await (const chunk of req) { chunks.push(chunk); }
      const wireBody = Buffer.concat(chunks);
      const encoding = req.headers['content-encoding'];
      const body = encoding === 'gzip' ? gunzipSync(wireBody) : wireBody;
      forwarded.push({ url: req.url, body: body.toString(), encoding, wireBody });
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

    const expectedCommands = [];
    const expectedBodies = [];
    function expectCommand (line, command, body) {
      expectedCommands.push({ line, command });
      if (body !== undefined) { expectedBodies.push({ line: line + 1, body }); }
    }

    const index = 'tests_sessions3-261009';
    const indexCommand = { index: { _index: index, _id: 'session-index' } };
    const indexDoc = { message: `${payload}-index` };
    const bulk = bulkBody(indexCommand, indexDoc);
    assert.equal(await request(base, '/_bulk', bulk), 200, 'index bulk');
    assert.equal(forwarded.at(-1).body, bulk, 'index body unchanged');
    expectCommand(1, indexCommand, JSON.stringify(indexDoc));

    const deleteCommand = { delete: { _index: 'tests_fields', _id: 'field-delete' } };
    const createCommand = { create: { _index: index, _id: 'session-create' } };
    const createDoc = { message: `${payload}-create` };
    const updateCommand = { update: { _index: 'tests_fields', _id: 123 } };
    const updateDoc = { doc: { friendlyName: `${payload}-field-label` } };
    const mixed = bulkBody(deleteCommand, createCommand, createDoc, indexCommand, indexDoc, updateCommand, updateDoc);
    assert.equal(await request(base, '/_bulk', mixed), 200, 'mixed operations');
    assert.equal(forwarded.at(-1).body, mixed, 'mixed body unchanged');
    expectCommand(1, deleteCommand);
    expectCommand(2, createCommand, JSON.stringify(createDoc));
    expectCommand(4, indexCommand, JSON.stringify(indexDoc));
    expectCommand(6, updateCommand, JSON.stringify(updateDoc));

    const gzipCommand = { index: { _index: index, _id: 'session-gzip' } };
    const gzipDoc = { message: `${payload}-gzip` };
    const gzipBulk = bulkBody(gzipCommand, gzipDoc);
    const compressed = gzipSync(gzipBulk);
    assert.equal(await request(base, '/_bulk', compressed, authorization, 'gzip'), 200, 'gzip bulk');
    assert.equal(forwarded.at(-1).body, gzipBulk, 'gzip contents unchanged');
    assert.equal(forwarded.at(-1).encoding, 'gzip', 'gzip encoding retained');
    assert.deepEqual(forwarded.at(-1).wireBody, compressed, 'gzip bytes unchanged');
    expectCommand(1, gzipCommand, JSON.stringify(gzipDoc));

    const fieldsIndex = { index: { _index: 'tests_fields', _id: 'field-index' } };
    const fieldsDoc = { friendlyName: `${payload}-field-create` };
    const fieldsUpdate = { update: { _index: 'tests_fields', _id: 'field-update' } };
    const fieldsUpdateDoc = { doc: { friendlyName: `${payload}-field-kept`, forbidden: `${payload}-field-dropped` } };
    const fieldsDrop = { update: { _index: 'tests_fields', _id: 'field-drop' } };
    const fieldsDropDoc = { doc: { forbidden: `${payload}-field-only-dropped` } };
    const fieldsBulk = bulkBody(fieldsIndex, fieldsDoc, fieldsUpdate, fieldsUpdateDoc, fieldsDrop, fieldsDropDoc);
    assert.equal(await request(base, '/_bulk', gzipSync(fieldsBulk), authorization, 'gzip'), 200, 'field rewrites');
    assert.equal(forwarded.at(-1).body, bulkBody({ create: fieldsIndex.index }, fieldsDoc, fieldsUpdate, { doc: { friendlyName: fieldsUpdateDoc.doc.friendlyName } }), 'field index becomes create and disallowed update keys are dropped');
    assert.equal(forwarded.at(-1).encoding, undefined, 'rewritten body is uncompressed');
    expectCommand(1, fieldsIndex, JSON.stringify(fieldsDoc));
    expectCommand(3, fieldsUpdate, JSON.stringify(fieldsUpdateDoc));
    expectCommand(5, fieldsDrop, JSON.stringify(fieldsDropDoc));

    const beforeDropped = forwarded.length;
    assert.equal(await request(base, '/_bulk', bulkBody(fieldsDrop, fieldsDropDoc)), 200, 'fully dropped update');
    assert.equal(forwarded.length, beforeDropped, 'fully dropped update is not forwarded');
    expectCommand(1, fieldsDrop, JSON.stringify(fieldsDropDoc));

    const extraMetadata = { index: { ...indexCommand.index, extra: { message: `${payload}-metadata-extra` } } };
    const extraBulk = bulkBody(extraMetadata, indexDoc);
    assert.equal(await request(base, '/_bulk', extraBulk), 200, 'additional metadata');
    assert.equal(forwarded.at(-1).body, extraBulk, 'additional metadata unchanged');
    expectCommand(1, indexCommand, JSON.stringify(indexDoc));

    const postPayload = `${payload}-post`;
    const updatePayload = `${payload}-update`;
    const beforeRejected = forwarded.length;
    for (const invalid of [
      JSON.stringify({ unsupported: payload }),
      `{"${payload}-syntax"`,
      JSON.stringify({ index: {}, extra: payload }),
      JSON.stringify({ index: null, message: payload })
    ]) {
      assert.equal(await request(base, '/_bulk', invalid), 400, 'invalid bulk is rejected');
    }

    const badIndex = { index: { _index: 'command-index-denied', _id: 'command-id-denied' } };
    const rejectedDoc = { message: `${payload}-rejected` };
    assert.equal(await request(base, '/_bulk', bulkBody(badIndex, rejectedDoc)), 400, 'unapproved bulk index');
    expectCommand(1, badIndex, JSON.stringify(rejectedDoc));
    const badMetadata = { index: { _index: { message: payload }, _id: { message: payload } } };
    assert.equal(await request(base, '/_bulk', bulkBody(badMetadata, rejectedDoc)), 400, 'non-string index');
    expectCommand(1, { index: {} }, JSON.stringify(rejectedDoc));

    const malformedUpdate = `{"${payload}-field-syntax"`;
    assert.equal(await request(base, '/_bulk', `${JSON.stringify(fieldsUpdate)}\n${malformedUpdate}\n`), 400, 'malformed field-update body');
    expectCommand(1, fieldsUpdate, malformedUpdate);
    assert.equal(await request(base, '/_bulk', gzipSync(`{"${payload}-gzip-syntax"`), authorization, 'gzip'), 400, 'malformed gzip command');
    assert.equal(await request(base, '/not-authorized', postPayload), 400, 'unapproved POST is rejected');
    assert.equal(await request(base, '/not-authorized', ''), 400, 'empty POST is rejected');
    assert.equal(await request(base, '/tests_sessions3-261009/_update/example', JSON.stringify({ doc: { message: payload } })), 400, 'invalid session update is rejected');
    assert.equal(forwarded.length, beforeRejected, 'rejected bodies never reach upstream');

    const update = JSON.stringify({ script: { source: 'ctx._source.test = params.test', params: { test: updatePayload } } });
    assert.equal(await request(base, '/tests_sessions3-261009/_update/example', update), 200, 'valid update is forwarded');
    assert.equal(forwarded.at(-1).body, update, 'update body is unchanged');
    await proxy.stop();
    const logs = proxy.logs();
    assert.equal(logs.includes(password), debug > 0, 'sensor password follows upstream debug behavior');
    if (debug) {
      assert.ok(logs.includes(`${payload}-gzip-syntax`), 'gzip failure details include the decoded line');
      assert.ok(logs.includes(postPayload), 'rejected POST body is logged');
      assert.ok(logs.includes(update), 'accepted session-update body is logged');
      assert.ok(logs.includes('Bulk error'), 'bulk error details are logged');
    } else {
      assert.ok(!logs.includes(payload), 'request payload is absent from logs');
      assert.ok(!logs.includes('Bulk error'), 'bulk error details are absent');
    }
    const commandLogs = logs.split('\n').filter(line => line.startsWith('Bulk command node: ')).map(line => {
      const match = line.match(/^Bulk command node: sensor line: (\d+) (.+)$/);
      assert.ok(match, 'command log format');
      return { line: Number(match[1]), command: JSON.parse(match[2]) };
    });
    assert.deepEqual(commandLogs, expectedCommands, 'each recognized incoming command is logged once');
    const bodyLogs = logs.split('\n').filter(line => line.startsWith('Bulk body node: ')).map(line => {
      const match = line.match(/^Bulk body node: sensor line: (\d+) (.*)$/);
      assert.ok(match, 'body log format');
      return { line: Number(match[1]), body: match[2] };
    });
    assert.deepEqual(bodyLogs, debug ? expectedBodies : [], 'document bodies follow debug and delete operations have no body');
    assert.ok(logs.includes('Bulk validation failed at line 1'), 'bulk failure has a safe diagnostic');
    assert.ok(logs.includes('Bulk dropping field update keys node: sensor'), 'field-update drop event is logged');
    assert.ok(logs.includes('POST failed node: sensor'), 'POST failure is logged');
  });
}

test('empty allowlist still starts and denies access', { timeout: 30000 }, async t => {
  const proxy = launchProxy(t, 'http://127.0.0.1:1', '');
  assert.equal(await request(await proxy.ready, '/'), 401);
  await proxy.stop();
  assert.ok(!proxy.logs().includes(password));
});

for (const debug of [0, 3]) {
  test(`invalid sensor config exits (debug=${debug})`, { timeout: 30000 }, async t => {
    const proxy = launchProxy(t, 'http://127.0.0.1:1', `${sensors}\nbad=pass:`, debug);
    await assert.rejects(proxy.ready, /exited before listening/);
    const [code] = await proxy.completion;
    assert.equal(code, 1);
    assert.ok(proxy.logs().includes("ERROR - esproxy-sensors 'bad'"));
    assert.equal(proxy.logs().includes(password), debug > 0);
  });
}
