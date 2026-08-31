/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

/* UpdateCheckService.js -- client side "newer release available" check
 *
 * Fetches a static per-major JSON file (releases-v5.json) from an origin the
 * admin controls via the updateCheckUrl config. Deliberately uses plain fetch
 * instead of fetchWrapper: fetchWrapper attaches Arkime session cookies, which
 * must never be sent off-origin. Nothing leaves the browser until the user has
 * consented once, and `off` mode is additionally enforced by CSP connect-src.
 */
import { reactive } from 'vue';

const CONSENT_KEY = 'arkimeUpdateCheckConsent';
const CACHE_KEY = 'arkimeUpdateCheckCache';
const DISMISS_KEY = 'arkimeUpdateCheckDismissed';

const CACHE_MS = 24 * 60 * 60 * 1000;
const FETCH_TIMEOUT_MS = 10000;

const state = reactive({
  mode: 'off', // off | manual | auto
  baseUrl: '',
  version: '', // the running version
  major: undefined,
  status: 'idle', // idle | checking | done | error
  latest: undefined, // newest version we should move to, if newer than ours
  latestUrl: undefined,
  security: false, // a security release sits between ours and latest
  eol: false, // our major is no longer supported
  consent: undefined, // undefined = never asked, true/false = answered
  dismissed: '' // version the user dismissed
});

export function updateCheckState () { return state; }

function readStorage (key) {
  try { return localStorage.getItem(key); } catch { return null; }
}

function writeStorage (key, value) {
  try { localStorage.setItem(key, value); } catch { /* private mode */ }
}

/** Turns 5.8.1 / v5.8.1 / 5.8.1-GIT into [5, 8, 1], or undefined */
export function parseVersion (version) {
  const match = /^v?(\d+)\.(\d+)\.(\d+)/.exec(String(version ?? '').trim());
  return match ? [+match[1], +match[2], +match[3]] : undefined;
}

/** Numeric compare so 5.10.0 sorts after 5.9.0 */
export function compareVersions (a, b) {
  const pa = parseVersion(a);
  const pb = parseVersion(b);
  if (!pa || !pb) { return 0; }
  for (let i = 0; i < 3; i++) {
    if (pa[i] !== pb[i]) { return pa[i] < pb[i] ? -1 : 1; }
  }
  return 0;
}

/** Dev builds shouldn't nag -- they're always "behind" a release */
export function isDevBuild (version) {
  return String(version ?? '').includes('-GIT');
}

/** `${base}/releases-v${major}.json`, tolerating a trailing slash on base */
export function releasesUrl (baseUrl, major) {
  return `${String(baseUrl).replace(/\/+$/, '')}/releases-v${major}.json`;
}

// the displayed result always reflects the most recent completed check,
// so a 404 or a failure can't leave a stale "update available" notice up
function clearResult () {
  state.latest = undefined;
  state.latestUrl = undefined;
  state.security = false;
  state.eol = false;
}

function applyPayload (payload) {
  const releases = Array.isArray(payload?.releases) ? payload.releases : [];

  let newest;
  let newestUrl;
  let security = false;

  for (const release of releases) {
    if (!parseVersion(release?.version)) { continue; }
    if (compareVersions(release.version, state.version) <= 0) { continue; }
    if (release.security) { security = true; }
    if (!newest || compareVersions(release.version, newest) > 0) {
      newest = release.version;
      newestUrl = release.url;
    }
  }

  // fall back to the precomputed `latest` when no releases[] is published
  if (!newest && compareVersions(payload?.latest, state.version) > 0) {
    newest = payload.latest;
    newestUrl = payload.url;
  }

  state.latest = newest;
  state.latestUrl = newestUrl;
  state.security = security;
  state.eol = !!payload?.eol;
  state.status = 'done';
}

function readCache () {
  try {
    const cached = JSON.parse(readStorage(CACHE_KEY));
    if (cached?.major !== state.major) { return undefined; }
    if (!cached.ts || Date.now() - cached.ts > CACHE_MS) { return undefined; }
    return cached.payload;
  } catch { return undefined; }
}

/**
 * Fetches the release file for our major version.
 * @param {object} [options]
 * @param {boolean} [options.force] skip the 24h cache (an explicit user click)
 */
export async function checkForUpdates (options = {}) {
  if (state.mode === 'off' || !state.consent || state.major === undefined) { return; }

  if (!options.force) {
    const cached = readCache();
    if (cached) { applyPayload(cached); return; }
  }

  clearResult();
  state.status = 'checking';

  try {
    const res = await fetch(releasesUrl(state.baseUrl, state.major), {
      method: 'GET',
      mode: 'cors',
      cache: 'no-store',
      credentials: 'omit', // never send Arkime cookies off-origin
      referrerPolicy: 'no-referrer',
      signal: AbortSignal.timeout(FETCH_TIMEOUT_MS)
    });

    // a major we haven't published a file for yet is a quiet no-op
    if (res.status === 404) { state.status = 'done'; return; }
    if (!res.ok) { throw new Error(`status ${res.status}`); }

    const payload = await res.json();
    writeStorage(CACHE_KEY, JSON.stringify({ ts: Date.now(), major: state.major, payload }));
    applyPayload(payload);
  } catch {
    state.status = 'error';
  }
}

export function grantConsent () {
  state.consent = true;
  writeStorage(CONSENT_KEY, 'true');
  return checkForUpdates({ force: true });
}

export function denyConsent () {
  state.consent = false;
  writeStorage(CONSENT_KEY, 'false');
}

export function dismissUpdate () {
  if (!state.latest) { return; }
  state.dismissed = state.latest;
  writeStorage(DISMISS_KEY, state.latest);
}

/** True when there's something new the user hasn't already waved off */
export function hasUndismissedUpdate () {
  return !!state.latest && state.dismissed !== state.latest;
}

/**
 * Wires the service up from the app's injected constants. Safe to call when
 * the feature is off -- it just leaves the service inert.
 * @param {object} constants the app's $constants object
 */
export function initUpdateCheck (constants) {
  const mode = constants?.CHECK_FOR_UPDATES;
  state.mode = ['manual', 'auto'].includes(mode) ? mode : 'off';
  state.baseUrl = constants?.UPDATE_CHECK_URL || '';
  state.version = constants?.VERSION || '';
  state.dismissed = readStorage(DISMISS_KEY) || '';
  state.status = 'idle';
  clearResult();

  const consent = readStorage(CONSENT_KEY);
  state.consent = consent === null ? undefined : consent === 'true';

  const parsed = parseVersion(state.version);
  state.major = parsed?.[0];

  if (state.mode === 'off' || !state.baseUrl || !parsed || isDevBuild(state.version)) {
    state.mode = 'off';
    return;
  }

  if (state.mode === 'auto' && state.consent) {
    checkForUpdates().catch(() => { /* surfaced via state.status */ });
  }
}
