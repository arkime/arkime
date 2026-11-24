/******************************************************************************/
/* SourceIP.io source, contributed by Alan Keister (@AlanKK)
 *
 * Fills sourceip.provider with the operators that publish a public IP.
 * With a key, IPs that arrive within batchWaitMs go in one POST
 * /v1/lookup/batch (max 100), without one each IP is a GET on the anonymous
 * endpoint (100/day, shared with Cont3xt). Only public IPs are sent, and a
 * daily budget plus the server's rate limit headers keep it inside the quota.
 * https://sourceip.io/api
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const axios = require('axios');
const net = require('net');
const WISESource = require('./wiseSource.js');

const ANON_URL = 'https://sourceip.io/api/v1/lookup';
const DEFAULT_ENDPOINT = 'https://api.sourceip.io';
const MAX_BATCH = 100;

// Non-public ranges are never sent, they would only spend quota and leak internal addresses
const privateList = new net.BlockList();
[
  ['0.0.0.0', 8], ['10.0.0.0', 8], ['100.64.0.0', 10], ['127.0.0.0', 8], ['169.254.0.0', 16],
  ['172.16.0.0', 12], ['192.0.0.0', 24], ['192.0.2.0', 24], ['192.168.0.0', 16], ['198.18.0.0', 15],
  ['198.51.100.0', 24], ['203.0.113.0', 24], ['224.0.0.0', 4], ['240.0.0.0', 4]
].forEach(([a, p]) => privateList.addSubnet(a, p, 'ipv4'));
[
  ['::', 128], ['::1', 128], ['64:ff9b:1::', 48], ['100::', 64], ['2001:2::', 48], ['2001:db8::', 32],
  ['3fff::', 20], ['5f00::', 16], ['fc00::', 7], ['fe80::', 10], ['fec0::', 10], ['ff00::', 8]
].forEach(([a, p]) => privateList.addSubnet(a, p, 'ipv6'));

function isPublic (ip) {
  const family = net.isIPv4(ip) ? 'ipv4' : (net.isIPv6(ip) ? 'ipv6' : undefined);
  return family !== undefined && !privateList.check(ip, family);
}

function utcDay () {
  return new Date().toISOString().slice(0, 10);
}

class SourceIPSource extends WISESource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { cacheTimeout: 60 * +api.getConfig(section, 'cacheAgeMin', 1440), tlsSettings: 'endpoint' });
    if (this.tlsOptions === undefined) { return; } // bad TLS settings, already logged

    this.key = api.getConfig(section, 'key');
    this.endpoint = String(api.getConfig(section, 'endpoint', DEFAULT_ENDPOINT)).replace(/\/+$/, '');
    this.field = api.getConfig(section, 'field', 'sourceip.provider');
    // lookups per UTC day, the key's quota is shared with Cont3xt so leave it some
    this.dailyBudget = +api.getConfig(section, 'dailyBudget', this.key ? 800 : 60);
    // pause once the server says this many or fewer lookups are left
    this.reserve = +api.getConfig(section, 'reserve', this.key ? 100 : 0);
    this.batchWaitMs = +api.getConfig(section, 'batchWaitMs', 500);
    this.timeoutMs = +api.getConfig(section, 'timeoutMs', 4000);
    this.trace = api.getConfigBool(section, 'trace', false);

    for (const setting of ['dailyBudget', 'reserve', 'batchWaitMs', 'timeoutMs']) {
      if (!Number.isInteger(this[setting]) || this[setting] < 0) {
        console.log(`${section} - ERROR not loading since ${setting} must be a whole number >= 0`);
        return;
      }
    }
    if (this.batchWaitMs + this.timeoutMs >= 5000) {
      console.log(`${section} - WARNING batchWaitMs + timeoutMs is ${this.batchWaitMs + this.timeoutMs}ms, WISE gives up on a request after 5000ms`);
    }

    this.used = 0;
    this.day = utcDay();
    this.blockedUntil = 0;
    this.skipLogged = new Map();
    this.pending = new Map(); // ip -> [cb, ...]

    this.providerField = api.addField(`field:${this.field};db:${this.field};kind:termfield;count:true;friendly:SourceIP.io Provider;help:Operators that publish this public IP, from sourceip.io`);
    api.addView('sourceip', `require:${this.field.split('.')[0]};title:SourceIP.io;fields:${this.field}`);
    api.addValueAction('sourceip', { name: 'SourceIP.io', url: 'https://sourceip.io/lookup?ip=%TEXT%', category: 'ip' });
    api.addSource(section, this, ['ip']);

    console.log(this.section, this.key
      ? `- batch lookups with a key on ${this.endpoint}, budget ${this.dailyBudget}/day, reserve ${this.reserve}`
      : `- anonymous lookups on ${ANON_URL}, budget ${this.dailyBudget}/day`);
  }

  // ----------------------------------------------------------------------------
  // why lookups can't happen right now, at most one log a minute per reason
  #blocked (n) {
    const today = utcDay();
    if (today !== this.day) { this.day = today; this.used = 0; }

    let why;
    if (Date.now() < this.blockedUntil) {
      why = `paused until ${new Date(this.blockedUntil).toISOString()}`;
    } else if (this.used + n > this.dailyBudget) {
      why = 'daily budget used up';
    } else {
      return false;
    }

    const key = why.split(' ')[0];
    if (Date.now() - (this.skipLogged.get(key) ?? 0) >= 60 * 1000) {
      this.skipLogged.set(key, Date.now());
      console.log(`${this.section} - not looking up, ${why} (used ${this.used}/${this.dailyBudget})`);
    }
    return true;
  }

  // ----------------------------------------------------------------------------
  // true if n more addresses may be looked up now, and counts them
  #allow (n) {
    if (this.#blocked(n)) { return false; }
    this.used += n;
    return true;
  }

  // ----------------------------------------------------------------------------
  #toResult (providers) {
    const names = [...new Set((providers ?? []).map((p) => p.name).filter(Boolean))];
    if (names.length === 0) { return WISESource.emptyResult; }
    return WISESource.encodeResult(...names.flatMap((n) => [this.providerField, n]));
  }

  // ----------------------------------------------------------------------------
  // pause on a 429, and stop early once the server's remaining count reaches the reserve
  #checkLimits (res) {
    const reset = +res.headers['x-ratelimit-reset']; // epoch seconds, next UTC midnight
    const remaining = res.headers['x-ratelimit-remaining'];
    if (res.status === 429) {
      const retry = +res.headers['retry-after'];
      const quotaGone = remaining !== undefined && +remaining <= 0;
      this.blockedUntil = (quotaGone && reset > 0) ? reset * 1000 : Date.now() + (retry > 0 ? retry * 1000 : 60 * 1000);
      console.log(`${this.section} - rate limited, pausing until ${new Date(this.blockedUntil).toISOString()}`);
    } else if (remaining !== undefined && +remaining <= this.reserve && reset > 0) {
      this.blockedUntil = reset * 1000;
      console.log(`${this.section} - only ${remaining} lookups left, pausing until ${new Date(this.blockedUntil).toISOString()}`);
    }
  }

  // ----------------------------------------------------------------------------
  // One attempt with a deadline for the whole request, a retry never helps when
  // the API stalls. Returns the response, or undefined after logging the error
  async #call (method, url, data) {
    const headers = { 'User-Agent': 'arkime-wise-sourceip' };
    if (this.key) { headers['x-api-key'] = this.key; }
    try {
      const res = await axios({
        method,
        url,
        data,
        headers,
        signal: AbortSignal.timeout(this.timeoutMs),
        validateStatus: () => true,
        // the TLS settings are for the endpoint, not the anonymous sourceip.io url
        ...(url.startsWith(`${this.endpoint}/`) ? this.axiosTls : {})
      });
      this.#checkLimits(res);
      return res;
    } catch (err) {
      console.log(`${this.section} - ${method} failed: ${err.message}`);
      return undefined;
    }
  }

  // ----------------------------------------------------------------------------
  getIp (ip, cb) {
    // undefined isn't cached by WISE, so skipped IPs are tried again later
    if (!isPublic(ip)) { return cb(null, undefined); }

    if (this.trace) {
      const start = Date.now();
      const orig = cb;
      cb = (err, result) => {
        const what = result === undefined ? 'no answer' : (result === WISESource.emptyResult ? 'no provider' : 'provider');
        console.log(`${this.section} - trace ${ip} ${what} ${Date.now() - start}ms`);
        orig(err, result);
      };
    }

    if (!this.key) { return this.#lookupOne(ip, cb); }

    // each address in a batch is charged, so send duplicates once
    const waiting = this.pending.get(ip);
    if (waiting) { waiting.push(cb); return; }
    if (this.#blocked(this.pending.size + 1)) { return cb(null, undefined); }

    this.pending.set(ip, [cb]);
    if (this.pending.size >= MAX_BATCH) {
      this.#flush();
    } else {
      this.timer ??= setTimeout(() => this.#flush(), this.batchWaitMs);
    }
  }

  // ----------------------------------------------------------------------------
  async #flush () {
    clearTimeout(this.timer);
    this.timer = undefined;
    const batch = this.pending;
    this.pending = new Map();
    const ips = [...batch.keys()];
    const answer = (ip, result) => batch.get(ip).forEach((cb) => cb(null, result));

    if (!this.#allow(ips.length)) { return ips.forEach((ip) => answer(ip, undefined)); }

    const res = await this.#call('POST', `${this.endpoint}/v1/lookup/batch`, { ips });
    if (res?.status !== 200 || !Array.isArray(res.data?.results)) {
      if (res && res.status !== 429) { console.log(`${this.section} - batch HTTP ${res.status}`); }
      return ips.forEach((ip) => answer(ip, undefined));
    }

    // results are in the order sent, an address that didn't parse has an error instead of providers
    const results = res.data.results;
    ips.forEach((ip, i) => {
      const r = results[i]?.ip === ip ? results[i] : results.find((x) => x?.ip === ip);
      answer(ip, r === undefined ? undefined : (r.error ? WISESource.emptyResult : this.#toResult(r.providers)));
    });
  }

  // ----------------------------------------------------------------------------
  async #lookupOne (ip, cb) {
    if (!this.#allow(1)) { return cb(null, undefined); }

    const res = await this.#call('GET', `${ANON_URL}?ip=${encodeURIComponent(ip)}`);
    if (res?.status === 400 || res?.status === 404) { return cb(null, WISESource.emptyResult); }
    if (res?.status !== 200) {
      if (res && res.status !== 429) { console.log(`${this.section} - ${ip} HTTP ${res.status}`); }
      return cb(null, undefined);
    }
    cb(null, this.#toResult(res.data?.providers));
  }
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('sourceip', {
    singleton: true,
    name: 'sourceip',
    description: 'Fill sourceip.provider with the operator of public IPs from sourceip.io. Every public IP WISE is asked about is sent to sourceip.io',
    link: 'https://arkime.com/wise#sourceip',
    types: ['ip'],
    fields: [
      { name: 'key', password: true, required: false, help: 'SourceIP.io API key, without one lookups are anonymous and limited to 100 a day' },
      { name: 'endpoint', required: false, help: `The endpoint that came with the key (defaults to ${DEFAULT_ENDPOINT})` },
      { name: 'field', required: false, help: 'The field to fill with provider names (defaults to sourceip.provider)' },
      { name: 'dailyBudget', required: false, regex: '^[0-9]+$', help: 'Addresses to look up per UTC day, counted in memory (defaults to 800 with a key, 60 without)' },
      { name: 'reserve', required: false, regex: '^[0-9]+$', help: 'Pause when the server says this many lookups or fewer are left, leaving them for Cont3xt (defaults to 100 with a key, 0 without)' },
      { name: 'batchWaitMs', required: false, regex: '^[0-9]+$', help: 'How long to collect IPs into one batch (defaults to 500)' },
      { name: 'timeoutMs', required: false, regex: '^[0-9]+$', help: 'Deadline for each API call, keep batchWaitMs + timeoutMs under 5000 (defaults to 4000)' },
      { name: 'trace', required: false, regex: '^(true|false)$', help: 'Log every lookup and its outcome' },
      ...WISESource.tlsConfigFields
    ]
  });

  // works without any settings, so load when the section exists
  if (!api.getConfigSections().includes('sourceip')) { return; }
  return new SourceIPSource(api, 'sourceip');
};
// ----------------------------------------------------------------------------
