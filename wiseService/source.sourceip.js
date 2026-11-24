/******************************************************************************/
/* SourceIP.io source for WISE
 *
 * Adds a "SourceIP.io Provider" field (sourceip.provider) to sessions whose IP is public, so it can be
 * shown as a column next to Dst ASN. Install into /opt/arkime/wiseService/ (see README.md).
 * Configured by the [sourceip] section of wiseService.ini.
 *
 * With SOURCEIP_API_KEY + SOURCEIP_ENDPOINT (https://api.sourceip.io) lookups are batched: IPs that
 * arrive within batchWaitMs are sent in one POST /v1/lookup/batch (max 100). Without a key it falls back to
 * one GET /api/v1/lookup per IP on the anonymous quota (100/day, shared with Cont3xt).
 * Guards: public IPs only, 1 day cache, daily budget, pause on 429 (Retry-After / quota reset).
 * Each request is a single attempt with a hard deadline (timeoutMs, default 4000), so batchWaitMs + timeoutMs stays under
 * WISE's own 5 s response timeout. (A retry never helped: when api.sourceip.io stalls, the second attempt stalls too.) A call that still fails yields "no result"
 * for its IPs instead of failing capture's whole /get batch.
 */
'use strict';

const https = require('https');
const net = require('net');
const WISESource = require('./wiseSource.js');

const ANON_URL = 'https://sourceip.io/api/v1/lookup';
const MAX_BATCH = 100;

// Non-public ranges: these would cost quota and carry no operator info worth showing.
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

class SourceIPSource extends WISESource {
  constructor (api, section) {
    super(api, section, { cacheTimeout: 60 * 60 * +api.getConfig(section, 'cacheAgeMin', '1440') });

    this.field = api.getConfig(section, 'field', 'sourceip.provider');
    this.key = api.getConfig(section, 'key', process.env.SOURCEIP_API_KEY);
    this.endpoint = String(api.getConfig(section, 'endpoint', process.env.SOURCEIP_ENDPOINT) ?? '').replace(/\/+$/, '');
    this.keyed = !!(this.key && this.endpoint);
    // addresses per UTC day this source may spend; keep headroom for Cont3xt (shares the key's quota)
    this.budget = +api.getConfig(section, 'dailyBudget', this.keyed ? '800' : '60');
    this.reserve = +api.getConfig(section, 'reserve', this.keyed ? '100' : '0'); // stop when the server says <= this left
    this.batchWaitMs = +api.getConfig(section, 'batchWaitMs', '500');
    this.trace = String(api.getConfig(section, 'trace', 'false')) === 'true'; // log every public-IP lookup outcome
    this.timeoutMs = +api.getConfig(section, 'timeoutMs', '4000'); // batchWaitMs + this must stay under WISE's 5 s

    this.used = 0;
    this.day = new Date().toISOString().slice(0, 10);
    this.blockedUntil = 0;
    this.pending = new Map(); // ip -> [cb, ...]
    this.timer = undefined;

    this.theField = this.api.addField(
      `field:${this.field};db:${this.field};kind:termfield;count:true;friendly:SourceIP.io Provider;` +
      'help:Operator(s) of the public IP according to sourceip.io');
    this.api.addSource(section, this, ['ip']);
    console.log(this.section, this.keyed
      ? `- keyed batch ${this.endpoint}/v1/lookup/batch, budget ${this.budget}/day, reserve ${this.reserve}`
      : `- anonymous ${ANON_URL}, budget ${this.budget}/day`);
  }

  // log why lookups were skipped, at most once a minute per reason
  logSkip (reason, ...extra) {
    this.skipLogged ??= {};
    if (Date.now() - (this.skipLogged[reason] ?? 0) < 60 * 1000) { return; }
    this.skipLogged[reason] = Date.now();
    console.log(this.section, 'no result:', reason, ...extra, `(used ${this.used}/${this.budget})`);
  }

  // why lookups can't happen right now, or undefined if they can
  blockReason (n) {
    const today = new Date().toISOString().slice(0, 10);
    if (today !== this.day) { this.day = today; this.used = 0; }
    if (Date.now() < this.blockedUntil) { return `paused until ${new Date(this.blockedUntil).toISOString()}`; }
    if (this.used + n > this.budget) { return 'daily budget exhausted'; }
    return undefined;
  }

  // true if n more addresses may be looked up now; counts them against the budget
  allow (n) {
    const why = this.blockReason(n);
    if (why) { this.logSkip(why); return false; }
    this.used += n;
    return true;
  }

  // WISE result for a provider response entry
  toResult (providers) {
    const names = [...new Set((providers ?? []).map((p) => p.name).filter(Boolean))];
    if (names.length === 0) { return WISESource.emptyResult; }
    return WISESource.encodeResult.apply(null, names.flatMap((n) => [this.theField, n]));
  }

  // Look at rate limit headers: pause on 429, and stop early once the server's remaining count hits the reserve
  checkLimits (res) {
    const reset = +res.headers['x-ratelimit-reset']; // epoch seconds, next UTC midnight
    const remaining = res.headers['x-ratelimit-remaining'];
    if (res.statusCode === 429) {
      const retry = +res.headers['retry-after'];
      const quotaGone = remaining !== undefined && +remaining <= 0;
      this.blockedUntil = (quotaGone && reset > 0) ? reset * 1000 : Date.now() + (retry > 0 ? retry * 1000 : 60 * 1000);
      console.log(this.section, 'rate limited, pausing until', new Date(this.blockedUntil).toISOString());
    } else if (remaining !== undefined && +remaining <= this.reserve && reset > 0) {
      this.blockedUntil = reset * 1000;
      console.log(this.section, `only ${remaining} lookups left, pausing until`, new Date(this.blockedUntil).toISOString());
    }
  }

  // one attempt; done(err) on timeout/network error, done(null, status, data) once a response is complete
  attempt (method, url, body, done) {
    const headers = { 'User-Agent': 'arkime-wise-sourceip' };
    if (this.key) { headers['x-api-key'] = this.key; }
    if (body) { headers['content-type'] = 'application/json'; }
    let finished = false;
    const finish = (...args) => {
      if (finished) { return; }
      finished = true;
      clearTimeout(deadline);
      done(...args);
    };
    // overall deadline (not just socket idle), so a slow or stalled response can't outlast WISE's 5 s timeout
    const deadline = setTimeout(() => req.destroy(new Error(`timeout after ${this.timeoutMs} ms`)), this.timeoutMs);
    const req = https.request(url, { method, headers }, (res) => {
      let data = '';
      res.on('data', (c) => { data += c; });
      res.on('end', () => { this.checkLimits(res); finish(null, res.statusCode, data); });
      res.on('error', (err) => finish(err));
      res.on('aborted', () => finish(new Error('response aborted')));
    });
    req.on('error', (err) => finish(err));
    req.end(body);
  }

  // single attempt (no retry: a stalled API stalls the retry too, and the time is better spent waiting on the first)
  request (method, url, body, done) {
    this.attempt(method, url, body, done);
  }

  getIp (ip, cb) {
    // undefined = "not handled", which WISE does not cache, so these IPs are retried later
    if (!isPublic(ip)) { return cb(null, undefined); }
    if (this.trace) {
      const t0 = Date.now();
      const orig = cb;
      cb = (err, out) => {
        const what = out === undefined ? 'UNDEFINED' : (out === WISESource.emptyResult ? 'empty' : 'providers');
        console.log(this.section, 'trace', ip, what, `${Date.now() - t0}ms`);
        orig(err, out);
      };
    }
    if (!this.keyed) { return this.getSingle(ip, cb); }

    const waiting = this.pending.get(ip);
    if (waiting) { return waiting.push(cb); } // duplicates are charged, so collapse them
    const why = this.blockReason(1);
    if (why) { this.logSkip(why); return cb(null, undefined); }
    this.pending.set(ip, [cb]);
    if (this.pending.size >= MAX_BATCH) {
      this.flush();
    } else if (!this.timer) {
      this.timer = setTimeout(() => this.flush(), this.batchWaitMs);
    }
  }

  flush () {
    clearTimeout(this.timer);
    this.timer = undefined;
    const ips = [...this.pending.keys()].slice(0, MAX_BATCH);
    const batch = ips.map((ip) => [ip, this.pending.get(ip)]);
    ips.forEach((ip) => this.pending.delete(ip));
    if (this.pending.size > 0) { this.timer = setTimeout(() => this.flush(), 0); }
    if (batch.length === 0) { return; }

    const fail = () => batch.forEach(([, cbs]) => cbs.forEach((cb) => cb(null, undefined)));
    if (!this.allow(ips.length)) { return fail(); }

    this.request('POST', `${this.endpoint}/v1/lookup/batch`, JSON.stringify({ ips }), (err, status, data) => {
      if (err) { console.log(this.section, 'batch', err.message); return fail(); }
      if (status !== 200) {
        if (status !== 429) { console.log(this.section, 'batch HTTP', status); }
        return fail();
      }
      let results;
      try { results = JSON.parse(data).results; } catch (e) { console.log(this.section, 'batch', e.message); return fail(); }
      batch.forEach(([ip, cbs], i) => {
        const r = (results?.[i]?.ip === ip) ? results[i] : results?.find((x) => x.ip === ip);
        if (r === undefined) { this.logSkip('IP missing from batch response', ip); }
        const out = r === undefined ? undefined : (r.error ? WISESource.emptyResult : this.toResult(r.providers));
        cbs.forEach((cb) => cb(null, out));
      });
    });
  }

  getSingle (ip, cb) {
    if (!this.allow(1)) { return cb(null, undefined); }
    const url = this.keyed ? `${this.endpoint}/v1/lookup` : ANON_URL;
    this.request('GET', `${url}?ip=${encodeURIComponent(ip)}`, undefined, (err, status, data) => {
      if (err) { console.log(this.section, ip, err.message); return cb(null, undefined); }
      if (status === 400 || status === 404) { return cb(null, WISESource.emptyResult); }
      if (status !== 200) {
        if (status !== 429) { console.log(this.section, ip, 'HTTP', status); }
        return cb(null, undefined);
      }
      try {
        cb(null, this.toResult(JSON.parse(data).providers));
      } catch (e) {
        console.log(this.section, ip, e.message);
        cb(null, undefined);
      }
    });
  }
}

exports.initSource = function (api) {
  if (String(api.getConfig('sourceip', 'enabled', 'false')) !== 'true') { return; }
  return new SourceIPSource(api, 'sourceip');
};
