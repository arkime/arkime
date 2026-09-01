/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Unit tests for the update check. Run with `npm run test:unit`.

These are not part of tests.pl: the client half never runs there (tests.pl
forces NODE_ENV=development, and configure.ac pins dev builds to X.Y.Z-GIT,
which isDevBuild() turns the feature off for).
*/
import { test, describe, beforeEach } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const ArkimeUtil = require('../common/arkimeUtil.js');

const {
  initUpdateCheck, checkForUpdates, updateCheckState, dismissUpdate,
  hasUndismissedUpdate, safeUrl, compareVersions, parseVersion,
  isDevBuild, releasesUrl
} = await import('../common/vueapp/UpdateCheckService.js');

// ---------------------------------------------------------------- server half

describe('ArkimeUtil.updateCheckConfig', () => {
  // it warns on bad input, which is the point, but not in test output
  const quiet = (fn) => {
    const log = console.log;
    console.log = () => {};
    try { return fn(); } finally { console.log = log; }
  };
  const cfg = (o) => quiet(() => ArkimeUtil.updateCheckConfig((k, d) => (k in o ? o[k] : d)));
  const OFF = { mode: 'off', url: '', origin: undefined };

  test('defaults to manual against versions.arkime.com', () => {
    assert.deepEqual(cfg({}), {
      mode: 'manual',
      url: 'https://versions.arkime.com',
      origin: 'https://versions.arkime.com'
    });
  });

  test('honors auto', () => assert.equal(cfg({ checkForUpdates: 'auto' }).mode, 'auto'));

  test('off in its various spellings', () => {
    assert.deepEqual(cfg({ checkForUpdates: 'off' }), OFF);
    assert.deepEqual(cfg({ checkForUpdates: false }), OFF);
    assert.deepEqual(cfg({ checkForUpdates: 'false' }), OFF);
  });

  test('fails closed on a typo rather than guessing', () => {
    assert.deepEqual(cfg({ checkForUpdates: 'manaul' }), OFF);
  });

  test('fails closed on an unusable url', () => {
    assert.deepEqual(cfg({ updateCheckUrl: 'not a url' }), OFF);
    assert.deepEqual(cfg({ updateCheckUrl: 'file:///etc/passwd' }), OFF);
  });

  test('reduces a mirror to its origin for CSP', () => {
    assert.equal(cfg({ updateCheckUrl: 'https://mirror.corp/arkime' }).origin, 'https://mirror.corp');
    assert.equal(cfg({ updateCheckUrl: 'http://mirror.corp:8080' }).origin, 'http://mirror.corp:8080');
  });
});

// ---------------------------------------------------------------- client half

describe('UpdateCheckService', () => {
  const BASE = {
    VERSION: '7.0.0',
    CHECK_FOR_UPDATES: 'manual',
    UPDATE_CHECK_URL: 'https://versions.arkime.com'
  };

  let store, calls, lastUrl, lastOpts;
  const state = updateCheckState();

  const serve = (body, status = 200) => {
    globalThis.fetch = async (url, opts) => {
      calls++; lastUrl = url; lastOpts = opts;
      return { ok: status >= 200 && status < 300, status, json: async () => body };
    };
  };
  const boot = (o = {}) => initUpdateCheck({ ...BASE, ...o }); // a page load
  const fresh = (o = {}) => { store.clear(); boot(o); };       // a new browser

  beforeEach(() => {
    store = new Map();
    calls = 0;
    globalThis.localStorage = {
      getItem: (k) => (store.has(k) ? store.get(k) : null),
      setItem: (k, v) => store.set(k, String(v))
    };
    serve({});
  });

  describe('version handling', () => {
    test('parses the forms we ship', () => {
      assert.deepEqual(parseVersion('7.0.0'), [7, 0, 0]);
      assert.deepEqual(parseVersion('v7.0.0'), [7, 0, 0]);
      assert.deepEqual(parseVersion('7.0.0-GIT'), [7, 0, 0]);
      assert.equal(parseVersion('nope'), undefined);
    });

    test('compares numerically, not as strings', () => {
      assert.equal(compareVersions('7.10.0', '7.9.0'), 1);
      assert.equal(compareVersions('8.0.0', '7.99.99'), 1);
      assert.equal(compareVersions('7.0.0', '7.0.0'), 0);
    });

    test('requests the file for its own major', () => {
      assert.equal(releasesUrl('https://versions.arkime.com', 7), 'https://versions.arkime.com/releases-v7.json');
      assert.equal(releasesUrl('https://m.corp/arkime//', 8), 'https://m.corp/arkime/releases-v8.json');
    });

    test('dev builds never nag', () => {
      assert.equal(isDevBuild('7.0.0-GIT'), true);
      fresh({ VERSION: '7.0.0-GIT' });
      assert.equal(state.mode, 'off');
    });
  });

  describe('disabled configurations stay inert', () => {
    for (const [label, o] of [
      ['off', { CHECK_FOR_UPDATES: 'off' }],
      ['unknown mode', { CHECK_FOR_UPDATES: 'bogus' }],
      ['no url', { UPDATE_CHECK_URL: '' }],
      ['unparseable version', { VERSION: 'nope' }]
    ]) {
      test(label, () => { fresh(o); assert.equal(state.mode, 'off'); });
    }
  });

  describe('what triggers a check', () => {
    test('manual sends nothing on load, the user has to ask', () => {
      fresh();
      assert.equal(calls, 0);
    });

    test('manual fetches on an explicit check', async () => {
      fresh();
      await checkForUpdates({ force: true });
      assert.equal(calls, 1);
      assert.equal(lastUrl, 'https://versions.arkime.com/releases-v7.json');
    });

    test('auto checks on load, the admin opted the deployment in', async () => {
      fresh({ CHECK_FOR_UPDATES: 'auto' });
      await new Promise((r) => setTimeout(r, 10));
      assert.equal(calls, 1);
    });

    test('off never fetches, even when asked directly', async () => {
      fresh({ CHECK_FOR_UPDATES: 'off' });
      await checkForUpdates({ force: true });
      assert.equal(calls, 0);
    });

    test('the request carries no cookies and no referrer', async () => {
      fresh();
      await checkForUpdates({ force: true });
      assert.equal(lastOpts.credentials, 'omit');
      assert.equal(lastOpts.referrerPolicy, 'no-referrer');
    });
  });

  describe('reading a feed', () => {
    test('offers the numerically newest release', async () => {
      fresh();
      serve({ releases: [
        { version: '7.0.9', url: 'https://x/709' },
        { version: '7.10.0', url: 'https://x/7100' },
        { version: '7.0.0', url: 'https://x/700' }
      ] });
      await checkForUpdates({ force: true });
      assert.equal(state.latest, '7.10.0');
      assert.equal(state.latestUrl, 'https://x/7100');
    });

    test('says nothing when we are current', async () => {
      fresh();
      serve({ releases: [{ version: '7.0.0', url: 'https://x/700' }] });
      await checkForUpdates({ force: true });
      assert.equal(state.latest, undefined);
      assert.equal(state.status, 'done');
    });

    test('announces the next major and its eol', async () => {
      fresh();
      serve({ eol: true, releases: [{ version: '8.0.0', url: 'https://x/800' }] });
      await checkForUpdates({ force: true });
      assert.equal(state.latest, '8.0.0');
      assert.equal(state.eol, true);
    });

    test('only flags a security fix newer than ours', async () => {
      fresh();
      serve({ releases: [{ version: '6.9.0', security: true }, { version: '7.1.0', url: 'https://x/710' }] });
      await checkForUpdates({ force: true });
      assert.equal(state.security, false);

      serve({ releases: [{ version: '7.1.0', url: 'https://x/710', security: true }] });
      await checkForUpdates({ force: true });
      assert.equal(state.security, true);
    });

    test('the precomputed latest fallback keeps its security flag', async () => {
      fresh();
      serve({ latest: '7.0.2', url: 'https://x/702', security: true });
      await checkForUpdates({ force: true });
      assert.equal(state.latest, '7.0.2');
      assert.equal(state.latestUrl, 'https://x/702');
      assert.equal(state.security, true);
    });

    test('never follows a non http(s) release link', async () => {
      assert.equal(safeUrl('https://x/y'), 'https://x/y');
      assert.equal(safeUrl('javascript:alert(1)'), undefined);
      assert.equal(safeUrl('data:text/html,x'), undefined);

      fresh();
      serve({ releases: [{ version: '7.1.0', url: 'javascript:alert(document.cookie)' }] });
      await checkForUpdates({ force: true });
      assert.equal(state.latestUrl, undefined, 'link dropped');
      assert.equal(state.latest, '7.1.0', 'version still reported');
    });
  });

  describe('failures', () => {
    test('an unpublished major is quiet, not an error', async () => {
      fresh();
      serve({}, 404);
      await checkForUpdates({ force: true });
      assert.equal(state.status, 'done');
      assert.equal(state.latest, undefined);
    });

    test('a failure clears the previous result rather than leaving it stale', async () => {
      fresh();
      serve({ releases: [{ version: '7.1.0', url: 'https://x/710' }] });
      await checkForUpdates({ force: true });
      assert.equal(state.latest, '7.1.0');

      serve({}, 500);
      await checkForUpdates({ force: true });
      assert.equal(state.status, 'error');
      assert.equal(state.latest, undefined);
    });

    test('a thrown fetch is an error, not a crash', async () => {
      fresh();
      globalThis.fetch = async () => { throw new Error('offline'); };
      await checkForUpdates({ force: true });
      assert.equal(state.status, 'error');
    });
  });

  describe('caching', () => {
    test('manual keeps its result across a reload without fetching', async () => {
      fresh();
      serve({ releases: [{ version: '7.1.0', url: 'https://x/710' }] });
      await checkForUpdates({ force: true });
      calls = 0;
      boot();
      assert.equal(calls, 0, 'no request on reload');
      assert.equal(hasUndismissedUpdate(), true, 'the dot survives');
    });

    test('a 404 is not retried on every load', async () => {
      fresh();
      serve({}, 404);
      await checkForUpdates({ force: true });
      calls = 0;
      boot({ CHECK_FOR_UPDATES: 'auto' });
      await new Promise((r) => setTimeout(r, 10));
      assert.equal(calls, 0);
    });

    test('an unreachable host is not retried on every load', async () => {
      fresh();
      globalThis.fetch = async () => { calls++; throw new Error('down'); };
      await checkForUpdates({ force: true });
      calls = 0;
      boot({ CHECK_FOR_UPDATES: 'auto' });
      await new Promise((r) => setTimeout(r, 10));
      assert.equal(calls, 0);
    });

    test('changing updateCheckUrl does not reuse the old host result', async () => {
      fresh();
      serve({ releases: [{ version: '7.1.0', url: 'https://x/710' }] });
      await checkForUpdates({ force: true });

      boot({ UPDATE_CHECK_URL: 'https://mirror.internal' });
      assert.equal(state.latest, undefined, 'stale result dropped');
    });
  });

  describe('dismissal', () => {
    test('is remembered per version', async () => {
      fresh();
      serve({ releases: [{ version: '7.1.0', url: 'https://x/710' }] });
      await checkForUpdates({ force: true });
      assert.equal(hasUndismissedUpdate(), true);

      dismissUpdate();
      assert.equal(hasUndismissedUpdate(), false);

      serve({ releases: [{ version: '7.2.0', url: 'https://x/720' }] });
      await checkForUpdates({ force: true });
      assert.equal(hasUndismissedUpdate(), true, 'a newer release reappears');
    });
  });
});
