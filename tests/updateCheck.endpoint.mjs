/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Smoke tests a real update check host, end to end through the client code.
Opt in, and NOT part of CI: it needs the network and would turn an outage of
someone else's server into a red build.

  npm run test:endpoint
  UPDATE_CHECK_URL=https://mirror.internal npm run test:endpoint

It asks for major 1, which no Arkime release has, so it reads the
releases-v1.json fixture rather than anything a real client depends on.

The failure this exists to catch is CORS. The service fetches with
mode: 'cors', so a host serving the file perfectly over curl still breaks
every browser unless it sends Access-Control-Allow-Origin, and the UI reports
nothing more useful than "Update check failed".
*/
import { test, describe, before } from 'node:test';
import assert from 'node:assert/strict';

const BASE = process.env.UPDATE_CHECK_URL || 'https://versions.arkime.com';
const URL_V1 = `${BASE.replace(/\/+$/, '')}/releases-v1.json`;

const { initUpdateCheck, checkForUpdates, updateCheckState } =
  await import('../common/vueapp/UpdateCheckService.js');

describe(`update check endpoint ${BASE}`, () => {
  let res, body;

  before(async () => {
    res = await fetch(URL_V1, { headers: { Origin: 'https://demo.arkime.com' } });
    body = res.ok ? await res.text() : '';
  });

  test('serves the fixture', () => {
    assert.equal(res.status, 200, `${URL_V1} returned ${res.status}`);
  });

  test('allows cross origin reads, or no browser can use it', () => {
    const allow = res.headers.get('access-control-allow-origin');
    assert.ok(allow, 'no Access-Control-Allow-Origin header, browsers will block this');
    assert.ok(allow === '*' || allow === 'https://demo.arkime.com', `unexpected allow origin: ${allow}`);
  });

  test('is json', () => {
    assert.match(res.headers.get('content-type') || '', /application\/json/);
  });

  test('parses and has the shape the client expects', () => {
    const json = JSON.parse(body);
    assert.ok(Array.isArray(json.releases), 'releases[] missing');
    for (const r of json.releases) {
      assert.match(r.version, /^\d+\.\d+\.\d+/, `bad version ${r.version}`);
      if (r.url) { assert.match(r.url, /^https?:\/\//, `bad url ${r.url}`); }
    }
  });

  test('a 1.0.0 client resolves an update through the real service', async () => {
    globalThis.localStorage = undefined; // service falls back cleanly
    initUpdateCheck({
      VERSION: '1.0.0',
      CHECK_FOR_UPDATES: 'manual',
      UPDATE_CHECK_URL: BASE
    });
    await checkForUpdates({ force: true });

    const state = updateCheckState();
    assert.equal(state.status, 'done', 'the fetch did not complete');
    assert.equal(state.latest, '1.0.1');
    assert.equal(state.security, true, 'security flag lost');
    assert.equal(state.eol, true, 'eol flag lost');
  });
});
