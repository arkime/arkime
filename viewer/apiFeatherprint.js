/******************************************************************************/
/* apiFeatherprint.js -- api calls + engine for the viewer-resident
 *                       featherprint passive IP fingerprint subsystem.
 *
 * Architecture: poll Arkime sessions ES on a schedule (monitor mode) or on
 * demand (lookup mode), extract device records via the in-module classifier,
 * persist to featherprint.db.js and fire notifiers on changes. No capture
 * push, no spool, no wise dependency.
 *
 * Also home for [featherprint-defaults] + [featherprint-subnet] config
 * parsing, the longest-prefix-match PolicyTree used to filter sessions, and
 * the per-IP signal extraction / device-classification logic (rule data
 * loaded from featherprint.rules.yaml).
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const fs = require('fs');
const net = require('net');
const path = require('path');
const yaml = require('js-yaml');
const Db = require('./db.js');
const Config = require('./config.js');
const Notifier = require('../common/notifier');
const ArkimeConfig = require('../common/arkimeConfig');
const ArkimeUtil = require('../common/arkimeUtil');
const iptrie = require('arkime-iptrie');
const arkimeparser = require('./arkimeparser.js');
const BuildQuery = require('./buildQuery');
const internals = require('./internals');

const FeatherprintDb = require('./featherprint.db.js');

// ============================================================================
// Config / policy
// ============================================================================

// ----------------------------------------------------------------------------
// PolicyTree: longest-prefix lookup with per-flag merge semantics, backed
// by arkime-iptrie.
//
//  * Defaults are applied first.
//  * Each matching subnet from shortest to longest prefix overrides the
//    flags it explicitly sets. We pre-compute the merged "resolved" flags
//    at construction time and store them as the iptrie value, so each
//    lookup() is a single longest-prefix-match probe.
//  * The pseudo-flag `ignore` is auto-cleared if a longer-prefix subnet
//    matches without itself setting `ignore` -- so e.g.
//      10/8    = ignore
//      10.1/16 = +alertOnNewIp
//    means 10.1.0.0/16 is watched, 10.0.0.0/8 minus 10.1.0.0/16 is not.
class PolicyTree {
  constructor (defaults, subnets) {
    this.defaults = defaults;
    // Sort shortest-prefix first so each add() can find its parent already
    // present in the trie and inherit resolved flags.
    this.subnets = [...subnets].sort((a, b) => a.cidr.bits - b.cidr.bits);
    this.trie = new iptrie.IPTrie();

    for (const s of this.subnets) {
      const par = this.trie.find(s.cidr.ip);
      s.parent = par ? par.subnet : null;
      const explicitIgnore = Object.prototype.hasOwnProperty.call(s.flags, 'ignore');
      const resolved = { ...defaults, ...(s.parent?.resolvedFlags ?? {}), ...s.flags };
      // Auto-unignore: longer prefix without explicit ignore clears an
      // inherited ignore.
      if (!explicitIgnore && s.parent?.resolvedFlags.ignore) resolved.ignore = false;
      s.resolvedFlags = resolved;
      this.trie.add(s.cidr.ip, s.cidr.bits, { cidr: s.cidr.text, flags: resolved, subnet: s });
    }
  }

  lookup (ip) {
    if (typeof ip !== 'string') return { flags: { ...this.defaults }, matched: [] };
    const hit = this.trie.find(ip);
    if (!hit) return { flags: { ...this.defaults }, matched: [] };
    return { flags: { ...hit.flags }, matched: [hit.cidr] };
  }

  // Build an ES bool filter that matches sessions touching any in-scope
  // subnet. This is only a coarse fetch prefilter; per-IP ignore filtering
  // happens in #relevantIpsForSession. We deliberately do NOT must_not the
  // ignored ranges here -- a session can carry both an ignored IP and an
  // in-scope one (e.g. a DHCP OFFER relayed through an ignored-range server
  // to an in-scope client), and excluding at the session level would drop
  // the in-scope signals too.
  //
  // Returns { bool: {...} } or null if no scope.
  compileEsFilter (ipFields) {
    ipFields = ipFields || FeatherprintAPIs.IP_FIELDS;
    const includeCidrs = this.subnets
      .filter(s => !s.resolvedFlags.ignore)
      .map(s => s.cidr.text);

    if (includeCidrs.length === 0) return null;

    const should = [];
    for (const cidr of includeCidrs) {
      for (const field of ipFields) {
        should.push({ term: { [field]: cidr } });
      }
    }
    return { bool: { should, minimum_should_match: 1 } };
  }
}

// ============================================================================
// Classifier -- per-IP signal extraction + device classification.
//
// Rule data (MAC source ranking, device classification heuristics) lives in
// featherprint.rules.yaml, loaded once at startup. Override the file via the
// arkime ini `featherprintRulesFile` setting, or drop a replacement at
// /opt/arkime/etc/featherprint.rules.yaml or ./featherprint.rules.yaml.
// ============================================================================

function findRulesFile () {
  const explicit = Config.get('featherprintRulesFile');
  const candidates = [
    explicit,
    '/opt/arkime/etc/featherprint.rules.yaml',
    './featherprint.rules.yaml',
    path.join(__dirname, 'featherprint.rules.yaml')
  ].filter(Boolean);
  for (const c of candidates) {
    try {
      if (fs.statSync(c).isFile()) return c;
    } catch (_) { /* not found */ }
  }
  return null;
}

function loadRules () {
  const file = findRulesFile();
  if (!file) {
    console.log('FEATHERPRINT - no rules file found; classifier disabled');
    return { macSourceRank: {}, classify: [] };
  }
  try {
    const doc = yaml.load(fs.readFileSync(file, 'utf8')) || {};
    console.log(`FEATHERPRINT - loaded rules from ${file}`);
    return {
      macSourceRank: doc.macSourceRank || {},
      classify: Array.isArray(doc.classify) ? doc.classify : []
    };
  } catch (e) {
    console.log(`FEATHERPRINT - failed to load rules from ${file}: ${e.message}`);
    return { macSourceRank: {}, classify: [] };
  }
}

// Loaded in FeatherprintAPIs.initialize(), not at require time: Config isn't
// initialized until premain(), so findRulesFile() can't read the
// featherprintRulesFile ini setting any earlier.
let RULES = { macSourceRank: {}, classify: [] };

// Clause keys ending in "Matches" hold user-supplied regexes, precompiled
// once. The values entry names the ctx array the regex is tested against.
const REGEX_CLAUSES = {
  nameMatches: 'nameStrs',
  dhcpVendorClassMatches: 'vendorClasses',
  ssdpServerMatches: 'ssdpServers',
  ssdpUsnMatches: 'ssdpUsns'
};

function compileClause (clause) {
  clause._res = {};
  for (const key of Object.keys(REGEX_CLAUSES)) {
    if (typeof clause[key] !== 'string') continue;
    try {
      clause._res[key] = new RegExp(clause[key], 'i');
    } catch (e) {
      console.log(`WARNING - featherprint: bad ${key} regex "${clause[key]}": ${e.message}`);
    }
  }
}

function initRules () {
  RULES = loadRules();
  for (const r of RULES.classify) {
    compileClause(r);
    if (Array.isArray(r.anyOf)) r.anyOf.forEach(compileClause);
  }
}

function clauseMatches (clause, ctx) {
  if (clause.hasService !== undefined && !ctx.serviceTypes.has(clause.hasService)) return false;
  if (clause.nameContains !== undefined && !ctx.nameStrs.some(n => n.includes(clause.nameContains))) return false;
  for (const [key, ctxKey] of Object.entries(REGEX_CLAUSES)) {
    if (clause[key] === undefined) continue;
    const re = clause._res[key];
    if (!re) return false; // regex failed to compile -- never match
    if (!ctx[ctxKey].some(v => v && re.test(v))) return false;
  }
  if (clause.macPrefixIn) {
    const mac = (ctx.macValue || '').toLowerCase();
    if (!clause.macPrefixIn.some(p => mac.startsWith(String(p).toLowerCase()))) return false;
  }
  return true;
}

// Normalise a value to an array: null/undefined -> [], scalar -> [v],
// array -> v. ES session fields may be single- or multi-valued depending
// on session content.
function toArray (v) {
  if (v === undefined || v === null) return [];
  return Array.isArray(v) ? v : [v];
}

function firstOf (v) {
  return toArray(v)[0];
}

// "_airplay._tcp.local" -> "_airplay._tcp" (lowercase, .local stripped).
function normalizeServiceType (svcName) {
  return String(svcName).toLowerCase().replace(/\.local\.?$/, '');
}

// "Living Room._airplay._tcp.local" -> "_airplay._tcp": the service type is
// the labels from the first underscore-label onward.
function serviceTypeFromInstance (instanceName) {
  const labels = String(instanceName).split('.');
  const i = labels.findIndex(l => l.startsWith('_'));
  if (i === -1) return null;
  return normalizeServiceType(labels.slice(i).join('.'));
}

function serviceProto (type) {
  if (type.endsWith('._udp')) return 'udp';
  if (type.endsWith('._tcp')) return 'tcp';
  return 'tcp';
}

class FeatherprintClassify {
  // --------------------------------------------------------------------------
  static #pickBetterMac (current, candidate) {
    if (!current) return candidate;
    if (!candidate) return current;
    const cr = RULES.macSourceRank[current.source] ?? 0;
    const nr = RULES.macSourceRank[candidate.source] ?? 0;
    if (nr > cr) return candidate;
    if (nr < cr) return current;
    // Same source: prefer the most recent observation.
    return candidate.ts > current.ts ? candidate : current;
  }

  // --------------------------------------------------------------------------
  // Rule-driven classifier. Walks the configured classify list, first match
  // wins. Each rule's clauses must all hold (AND), except clauses listed in
  // `anyOf` where any may match (OR).
  static classifyDevice (record) {
    const ctx = {
      serviceTypes: new Set((record.services ?? []).map(s => s.type).filter(Boolean)),
      nameStrs: (record.names ?? []).map(n => (n.name || '').toLowerCase()),
      vendorClasses: record.dhcp?.vendorClass ? [record.dhcp.vendorClass] : [],
      ssdpServers: record.ssdp?.server ?? [],
      ssdpUsns: record.ssdp?.usn ?? [],
      macValue: record.mac?.value
    };
    for (const rule of RULES.classify) {
      if (Array.isArray(rule.anyOf)) {
        if (!rule.anyOf.some(sub => clauseMatches(sub, ctx))) continue;
      }
      // Top-level clauses (besides anyOf and class) must all hold.
      if (!clauseMatches(rule, ctx)) continue;
      return rule.class;
    }
    return 'unknown';
  }

  // --------------------------------------------------------------------------
  // Walk a session _source extracting per-protocol signals for one IP.
  // `flags` is the resolved policy flag bag for this IP.
  static #extractFromSession (ip, src, signals, flags) {
    const ts = src.lastPacket || src.firstPacket;
    const protocols = toArray(src.protocol);

    // ARP: source.ip == arp.ip (we populated both for replies in capture/parsers/arp.c).
    // A session about a single IP may still carry several sender MACs (re-announcements
    // with a changed MAC merge into one session) -- every one is an observation.
    // Multi-IP arp sessions (merged request+reply) can't be attributed, skip those.
    const arpIps = toArray(src?.arp?.ip);
    const arpMacs = toArray(src?.arp?.mac);
    if (arpIps.length === 1 && arpIps[0] === ip) {
      for (const m of arpMacs) {
        signals.macs.push({ value: m, source: 'arp', ts });
      }
    }

    // DHCP: broadcast DISCOVER/REQUESTs come from 0.0.0.0 and OFFER/ACKs from
    // the server, so bind by the server-assigned yiaddr or the requested IP;
    // source.ip covers unicast renewals from the leased address. The `dhcp`
    // policy flag (default true) can turn this off per subnet.
    if (src?.dhcp && flags?.dhcp !== false) {
      const dhcpBound =
        toArray(src.dhcp.yiaddrIp).includes(ip) ||
        toArray(src.dhcp.requestIp).includes(ip) ||
        src?.source?.ip === ip;
      if (dhcpBound) {
        for (const h of toArray(src.dhcp.host)) {
          signals.names.push({ name: h, source: 'dhcp', ts });
        }
        for (const m of toArray(src.dhcp.mac)) {
          signals.macs.push({ value: m, source: 'dhcp', ts });
        }
        if (src.dhcp.classId) {
          signals.dhcp.vendorClass = firstOf(src.dhcp.classId);
        }
        if (src.dhcp.paramReqList) {
          signals.dhcp.paramReqList = firstOf(src.dhcp.paramReqList);
        }
      }
    }

    // The session dns field is an array of per-query objects, each carrying
    // its own answers array.
    const answers = [];
    for (const d of toArray(src?.dns)) {
      for (const a of toArray(d?.answers)) answers.push(a);
    }

    // DNS A/AAAA answers (mDNS rides the same parser): bind answer.name to
    // answer.ip regardless of who asked.
    for (const a of answers) {
      if (a?.ip === ip && a?.name) {
        signals.names.push({ name: a.name, source: 'dns', ts, ttl: a.ttl });
      }
    }

    // mDNS/DNS-SD service discovery: announcements come from the device
    // itself. PTR answers on "_x._tcp" names and SRV answers on instance
    // names describe the announcer's services.
    if (src?.source?.ip === ip && protocols.includes('mdns')) {
      for (const a of answers) {
        if (typeof a?.name !== 'string') continue;
        if (a.type === 'PTR' && a.name[0] === '_') {
          // For "_services._dns-sd._udp" enumeration answers the rdata is the
          // advertised service type; otherwise the rrname is the type itself.
          const raw = (a.name.startsWith('_services._dns-sd._udp') && a.ptr) ? a.ptr : a.name;
          const type = normalizeServiceType(raw);
          signals.services.push({ type, proto: serviceProto(type), ts });
        } else if (a.type === 'SRV') {
          const type = serviceTypeFromInstance(a.name);
          if (type) {
            signals.services.push({ type, proto: serviceProto(type), port: a.port, ts });
          }
          if (a.srv) { // SRV target is the device's hostname
            signals.names.push({ name: a.srv, source: 'mdns', ts, ttl: a.ttl });
          }
        }
      }
    }

    // NBNS + SSDP are announced by source.ip.
    if (src?.source?.ip === ip) {
      for (const n of toArray(src?.nbns?.host)) {
        signals.names.push({ name: n, source: 'nbns', ts });
      }
      for (const u of toArray(src?.ssdp?.usn)) {
        signals.names.push({ name: u, source: 'ssdp', ts });
        signals.ssdp.usn.add(u);
      }
      for (const s of toArray(src?.ssdp?.server)) {
        signals.names.push({ name: s, source: 'ssdp-server', ts });
        signals.ssdp.server.add(s);
      }
      for (const n of toArray(src?.ssdp?.nt)) {
        signals.ssdp.nt.add(n);
        if (n.startsWith('urn:')) { // UPnP device/service type URNs
          signals.services.push({ type: n, proto: 'ssdp', ts });
        }
      }
      for (const s of toArray(src?.ssdp?.st)) {
        signals.ssdp.st.add(s);
      }
      for (const l of toArray(src?.ssdp?.location)) {
        signals.ssdp.location.add(l);
      }

      // Fallback MAC source: source.mac on a session originated by this IP.
      for (const m of toArray(src?.source?.mac)) {
        signals.macs.push({ value: m, source: 'srcMac', ts });
      }
    }
  }

  // --------------------------------------------------------------------------
  static #dedupNames (names) {
    // Collapse on (name, source). Keep earliest firstSeen, latest lastSeen.
    // Entries carried over from a previous record already have firstSeen/
    // lastSeen; freshly-observed signals only have ts -- fall back to it so a
    // reprocessing window never ratchets a stored firstSeen forward.
    const seen = new Map();
    for (const n of names) {
      if (!n.name) continue;
      const k = `${n.name}|${n.source}`;
      const first = n.firstSeen ?? n.ts;
      const last = n.lastSeen ?? n.ts;
      const cur = seen.get(k);
      if (!cur) {
        seen.set(k, { name: n.name, source: n.source, firstSeen: first, lastSeen: last, ttl: n.ttl });
      } else {
        if (first < cur.firstSeen) cur.firstSeen = first;
        if (last > cur.lastSeen) cur.lastSeen = last;
        if (n.ttl !== undefined) cur.ttl = n.ttl;
      }
    }
    return Array.from(seen.values()).sort((a, b) => b.lastSeen - a.lastSeen);
  }

  static #dedupServices (services) {
    const seen = new Map();
    for (const s of services) {
      const k = `${s.type ?? ''}|${s.proto}|${s.port ?? ''}`;
      const first = s.firstSeen ?? s.ts;
      const last = s.lastSeen ?? s.ts;
      const cur = seen.get(k);
      if (!cur) {
        seen.set(k, { ...s, firstSeen: first, lastSeen: last });
      } else {
        if (first < cur.firstSeen) cur.firstSeen = first;
        if (last > cur.lastSeen) cur.lastSeen = last;
      }
    }
    const out = Array.from(seen.values());
    // A PTR sighting records the type without a port; drop it once an SRV
    // sighting supplies the ported entry for the same type+proto.
    return out.filter(s => s.port !== undefined ||
      !out.some(o => o !== s && o.type === s.type && o.proto === s.proto && o.port !== undefined));
  }

  // Merge previous + newly observed SSDP attributes; Sets in, arrays out.
  static #mergeSsdp (previous, sets) {
    const out = {};
    for (const key of ['server', 'usn', 'nt', 'st', 'location']) {
      const merged = new Set([...(previous?.[key] ?? []), ...(sets?.[key] ?? [])]);
      if (merged.size) out[key] = Array.from(merged).slice(-20);
    }
    return Object.keys(out).length ? out : undefined;
  }

  // --------------------------------------------------------------------------
  static classifyIp (ip, sessions, previous, flags) {
    const signals = {
      macs: [],
      names: [],
      services: [],
      dhcp: {},
      ssdp: { server: new Set(), usn: new Set(), nt: new Set(), st: new Set(), location: new Set() }
    };

    let firstSeen = previous?.firstSeen ?? Number.MAX_SAFE_INTEGER;
    let lastSeen = previous?.lastSeen ?? 0;

    for (const sess of sessions) {
      const src = sess._source ?? sess;
      if (src.firstPacket && src.firstPacket < firstSeen) firstSeen = src.firstPacket;
      if (src.lastPacket && src.lastPacket > lastSeen) lastSeen = src.lastPacket;
      FeatherprintClassify.#extractFromSession(ip, src, signals, flags);
    }

    if (firstSeen === Number.MAX_SAFE_INTEGER) firstSeen = lastSeen || Date.now();
    if (lastSeen === 0) lastSeen = firstSeen;

    // Best MAC by source rank, then recency.
    let bestMac = null;
    for (const m of signals.macs) {
      if (!m.value) continue;
      const candidate = { value: m.value.toLowerCase(), source: m.source, ts: m.ts };
      bestMac = FeatherprintClassify.#pickBetterMac(bestMac, candidate);
    }
    // Cross-window guard: a lower-ranked source (e.g. srcMac picking up the
    // gateway MAC on routed traffic) never displaces a higher-ranked stored
    // MAC -- without this a window with only routed sessions would fire a
    // bogus changeMac.
    if (bestMac && previous?.mac?.value && bestMac.value !== previous.mac.value &&
        (RULES.macSourceRank[bestMac.source] ?? 0) < (RULES.macSourceRank[previous.mac.source] ?? 0)) {
      bestMac = null;
    }
    const macHistory = previous?.mac?.history ?? [];
    if (bestMac && !macHistory.some(h => h.mac === bestMac.value)) {
      macHistory.push({ mac: bestMac.value, ts: bestMac.ts, source: bestMac.source });
    }

    const prevFirst = previous?.firstSeen;
    const record = {
      ip,
      firstSeen: prevFirst ? Math.min(prevFirst, firstSeen) : firstSeen,
      lastSeen,
      mac: bestMac
        ? {
          value: bestMac.value,
          source: bestMac.source,
          firstSeen: macHistory[0]?.ts ?? bestMac.ts,
          lastSeen: bestMac.ts,
          history: macHistory
        }
        : (previous?.mac ?? null),
      names: FeatherprintClassify.#dedupNames([
        ...(previous?.names ?? []),
        ...signals.names
      ]),
      services: FeatherprintClassify.#dedupServices([
        ...(previous?.services ?? []),
        ...signals.services
      ]),
      dhcp: Object.keys(signals.dhcp).length
        ? { ...previous?.dhcp, ...signals.dhcp }
        : previous?.dhcp,
      ssdp: FeatherprintClassify.#mergeSsdp(previous?.ssdp, signals.ssdp)
    };

    record.classification = FeatherprintClassify.classifyDevice(record);
    return record;
  }
}

// ============================================================================
// Engine
// ============================================================================

class FeatherprintAPIs {
  static #internals = null;

  // --------------------------------------------------------------------------
  // Constants
  // --------------------------------------------------------------------------

  // Recognised flag names. Anything else is rejected with a warning.
  // `dhcp` (default true) gates DHCP-derived signal collection per subnet;
  // `ignore` drops the subnet from monitoring entirely.
  static KNOWN_FLAGS = new Set([
    'alertOnNewIp',
    'alertOnNewMac',
    'alertOnChangeMac',
    'alertOnChangeIp',
    'alertOnNewName',
    'alertOnChangeName',
    'alertOnNewService',
    'alertOnChangeDevice',
    'dhcp',
    'ignore'
  ]);

  // alertAll is shorthand for every alert* flag.
  static ALERT_FLAGS = [
    'alertOnNewIp',
    'alertOnNewMac',
    'alertOnChangeMac',
    'alertOnChangeIp',
    'alertOnNewName',
    'alertOnChangeName',
    'alertOnNewService',
    'alertOnChangeDevice'
  ];

  // Event kind -> alert flag name.
  static EVENT_KIND_TO_FLAG = Object.freeze({
    newIp: 'alertOnNewIp',
    newMac: 'alertOnNewMac',
    changeMac: 'alertOnChangeMac',
    changeIp: 'alertOnChangeIp',
    newName: 'alertOnNewName',
    changeName: 'alertOnChangeName',
    newService: 'alertOnNewService',
    changeDevice: 'alertOnChangeDevice'
  });

  static IP_FIELDS = ['source.ip', 'destination.ip', 'arp.ip', 'dns.ip', 'dhcp.yiaddrIp', 'dhcp.requestIp'];

  // --------------------------------------------------------------------------
  // Config parsing
  // --------------------------------------------------------------------------

  static parseFlagList (str) {
    const result = {};
    if (!str || typeof str !== 'string') return result;
    for (let tok of str.split(',')) {
      tok = tok.trim();
      if (!tok) continue;
      let val = true;
      if (tok[0] === '+') {
        tok = tok.slice(1);
      } else if (tok[0] === '-') {
        val = false;
        tok = tok.slice(1);
      }
      if (tok === 'alertAll') {
        for (const f of FeatherprintAPIs.ALERT_FLAGS) result[f] = val;
        continue;
      }
      if (!FeatherprintAPIs.KNOWN_FLAGS.has(tok)) {
        console.log(`WARNING - featherprint: unknown flag "${tok}" ignored`);
        continue;
      }
      result[tok] = val;
    }
    return result;
  }

  // Minimal CIDR parser. iptrie validates the IP itself; we just split on /
  // and capture (text, ip, bits, family).
  static parseCidr (s) {
    s = String(s).trim();
    const slash = s.indexOf('/');
    const ip = slash >= 0 ? s.slice(0, slash) : s;
    const family = ip.includes(':') ? 6 : 4;
    const maxBits = family === 6 ? 128 : 32;
    let bits;
    if (slash >= 0) {
      bits = parseInt(s.slice(slash + 1), 10);
      if (isNaN(bits) || bits < 0 || bits > maxBits) return null;
    } else {
      bits = maxBits;
    }
    return { text: s, ip, bits, family };
  }

  // Parse a positive-integer config value, falling back to dflt on a
  // missing / non-numeric / out-of-range value (min defaults to 1). Guards
  // the tick loop against a typo'd chunkSec=0/negative (spins forever) or a
  // non-numeric interval (setInterval(NaN) busy-fires).
  static #posInt (key, dflt, min = 1) {
    const n = parseInt(Config.get(key, dflt), 10);
    if (!Number.isFinite(n) || n < min) {
      if (Config.get(key, undefined) !== undefined) {
        console.log(`WARNING - featherprint: ${key} must be an integer >= ${min}; using ${dflt}`);
      }
      return dflt;
    }
    return n;
  }

  static loadConfig () {
    const intervalSec = FeatherprintAPIs.#posInt('featherprintInterval', 300, 30);
    const settings = {
      monitorEnabled: Config.get('featherprintMonitorEnabled', true),
      intervalSec,
      notifier: Config.get('featherprintNotifier', undefined),
      historyLimit: FeatherprintAPIs.#posInt('featherprintHistoryLimit', 50),
      dedupWindowSec: FeatherprintAPIs.#posInt('featherprintDedupWindow', 60),
      lookupLimit: FeatherprintAPIs.#posInt('featherprintLookupLimit', 10000),
      // Safety lag: how far in the past to treat as "indexed" to avoid racing
      // ES refresh + capture's dbFlushTimeout. Mirrors cron's formula.
      safetyLagSec: FeatherprintAPIs.#posInt('featherprintSafetyLag',
        (+Config.get('dbFlushTimeout', 5)) + 60 + 20, 0),
      // Cold-start lookback in days. -1 means "all data": query min(@timestamp)
      // and start there. Combined with the empty-chunk fast-forward (chunks
      // double up to 24h on 0-session results), -1 is cheap even across years
      // of mostly-empty range.
      initialLookbackDays: FeatherprintAPIs.#posInt('featherprintInitialLookbackDays', 7, -1),
      chunkSec: FeatherprintAPIs.#posInt('featherprintChunkSec', intervalSec),
      maxChunksPerTick: FeatherprintAPIs.#posInt('featherprintMaxChunksPerTick', 12)
    };

    const defaultsSection = ArkimeConfig.getSection('featherprint-defaults') ?? {};
    const defaults = {};
    for (const k of Object.keys(defaultsSection)) {
      if (!FeatherprintAPIs.KNOWN_FLAGS.has(k)) {
        console.log(`WARNING - featherprint: unknown default flag "${k}" ignored`);
        continue;
      }
      const raw = defaultsSection[k];
      defaults[k] = raw === true || raw === 'true' || raw === '1';
    }
    for (const f of [...FeatherprintAPIs.ALERT_FLAGS, 'ignore']) {
      if (defaults[f] === undefined) defaults[f] = false;
    }
    // DHCP signal collection is on unless explicitly disabled.
    if (defaults.dhcp === undefined) defaults.dhcp = true;

    const subnetsSection = ArkimeConfig.getSection('featherprint-subnet') ?? {};
    const subnets = [];
    for (const cidrText of Object.keys(subnetsSection)) {
      const cidr = FeatherprintAPIs.parseCidr(cidrText);
      if (!cidr) {
        console.log(`WARNING - featherprint: invalid CIDR "${cidrText}" in [featherprint-subnet]`);
        continue;
      }
      const flags = FeatherprintAPIs.parseFlagList(subnetsSection[cidrText]);
      subnets.push({ cidr, flags });
    }

    const policy = new PolicyTree(defaults, subnets);
    return { settings, defaults, subnets, policy };
  }

  // --------------------------------------------------------------------------
  static initialize (options = {}) {
    initRules(); // Config is available now; load rules so featherprintRulesFile applies
    const cfg = FeatherprintAPIs.loadConfig();
    FeatherprintAPIs.#internals = {
      ...cfg,
      isPrimaryViewer: options.isPrimaryViewer ?? (() => false),
      timer: null,
      running: false,
      state: null,
      wasPrimary: null
    };
    const i = FeatherprintAPIs.#internals;

    if (!i.settings.monitorEnabled) {
      console.log('featherprint: monitor disabled (featherprintMonitorEnabled=false); lookup-only mode');
      return;
    }
    if (i.subnets.length === 0) {
      console.log('featherprint: no [featherprint-subnet] entries configured; monitor will idle');
    }

    const intervalMs = Math.max(30, i.settings.intervalSec) * 1000;
    i.timer = setInterval(FeatherprintAPIs.tick, intervalMs);
    console.log(`featherprint: monitor scheduled every ${intervalMs / 1000}s ` +
      `(safetyLag=${i.settings.safetyLagSec}s, chunk=${i.settings.chunkSec}s, ` +
      `maxChunks/tick=${i.settings.maxChunksPerTick}, ` +
      `initialLookbackDays=${i.settings.initialLookbackDays}); this node ` +
      `${i.isPrimaryViewer() ? 'IS' : 'is NOT'} currently primary`);
    // Kick off an immediate first tick after a short delay so cron-leader
    // election and ES connections have a moment to settle.
    setTimeout(FeatherprintAPIs.tick, 5000);
  }

  // --------------------------------------------------------------------------
  // Load persisted monitor state. On cold start, seed lpValue at
  // (now - initialLookbackSec) and persist it so future restarts resume here.
  static async #ensureState () {
    const i = FeatherprintAPIs.#internals;
    if (i.state) return i.state;
    const nowSec = Math.floor(Date.now() / 1000);
    // getState() returns null only when the state doc is genuinely absent
    // (404); any thrown error is a transport failure. Never treat that as a
    // cold start -- reseeding would overwrite the persisted cursor and rewind
    // the monitor. Let it propagate so tick() skips this run and retries.
    let s = await FeatherprintDb.getState();
    if (s) {
      console.log(`featherprint: resumed at lpValue=${new Date(s.lpValue * 1000).toISOString()}`);
      i.state = s;
      return s;
    }

    let lp;
    let reason;
    if (i.settings.initialLookbackDays === -1) {
      // Query the oldest @timestamp across all session indices so the very
      // first tick starts at the actual beginning of the data, then the
      // chunk loop (with adaptive growth on empty chunks) walks forward.
      try {
        const r = await Db.search(Db.getSessionIndices(true), {
          size: 0,
          aggs: { minTs: { min: { field: '@timestamp' } } }
        });
        const minMs = r?.aggregations?.minTs?.value;
        if (minMs && Number.isFinite(minMs)) {
          lp = Math.floor(minMs / 1000) - 60;
          reason = `initialLookbackDays=-1, min(@timestamp)=${new Date(minMs).toISOString()}`;
        } else {
          reason = 'initialLookbackDays=-1 but min(@timestamp) returned no data';
        }
      } catch (e) {
        console.log('featherprint: min(@timestamp) probe failed:', e?.message || e);
      }
    }
    if (lp === undefined) {
      const days = Math.max(0, i.settings.initialLookbackDays);
      lp = nowSec - days * 24 * 60 * 60;
      reason = reason || (days === 0
        ? 'initialLookbackDays=0 (only data from now forward)'
        : `initialLookbackDays=${days}`);
    }
    s = {
      lpValue: lp,
      firstProcessedTs: lp * 1000,
      lastProcessedTs: lp * 1000,
      lastTickAt: 0,
      lastChunkSessions: 0,
      lastChunkAlerts: 0
    };
    try {
      await FeatherprintDb.saveState(s);
      console.log(`featherprint: cold start: seeding lpValue=${new Date(lp * 1000).toISOString()} (${reason})`);
    } catch (e) {
      console.log('featherprint: saveState failed on cold start:', e?.message || e);
    }
    i.state = s;
    return s;
  }

  // --------------------------------------------------------------------------
  // One scheduled poll. Idempotent and safe to re-run; only the cron-leader
  // actually queries ES. Walks in chunks (chunkSec) up to maxChunksPerTick so
  // long backfills are bounded and progress is persisted after each chunk.
  static async tick () {
    const i = FeatherprintAPIs.#internals;
    if (!i) return;
    const isP = !!i.isPrimaryViewer();
    if (isP !== i.wasPrimary) {
      if (isP) {
        console.log('featherprint: this node is now PRIMARY; resuming monitor');
        i.state = null; // force re-read of persisted lpValue
      } else {
        console.log('featherprint: this node is no longer PRIMARY; monitor idle');
      }
      i.wasPrimary = isP;
    }
    if (!isP || i.running) return;
    i.running = true;
    try {
      const state = await FeatherprintAPIs.#ensureState();
      const settings = i.settings;
      const nowSec = Math.floor(Date.now() / 1000);
      // Apply safety lag so ES has time to refresh + capture flush.
      const endSec = nowSec - settings.safetyLagSec;
      if (endSec <= state.lpValue) return;

      let remaining = settings.maxChunksPerTick;
      let totalDevices = 0;
      let totalAlerts = 0;
      let totalSessions = 0;
      let currentChunkSec = settings.chunkSec;
      const maxChunkSec = 24 * 60 * 60; // cap empty fast-forward at 24h

      while (remaining > 0 && state.lpValue < endSec) {
        const chunkEnd = Math.min(state.lpValue + currentChunkSec, endSec);
        const startMs = state.lpValue * 1000;
        const endMs = chunkEnd * 1000;
        let result;
        try {
          result = await FeatherprintAPIs.aggregate({
            timeRange: { start: startMs, end: endMs },
            mode: 'persist',
            notify: true
          });
        } catch (e) {
          console.log('featherprint: tick chunk error:', e?.message || e);
          break;
        }
        totalDevices += result.devicesTouched;
        totalAlerts += result.alerts;
        totalSessions += result.sessionsScanned;

        state.lpValue = chunkEnd;
        state.lastProcessedTs = endMs;
        state.lastTickAt = Date.now();
        state.lastChunkSessions = result.sessionsScanned;
        state.lastChunkAlerts = result.alerts;
        try {
          await FeatherprintDb.saveState(state);
        } catch (e) {
          console.log('featherprint: saveState failed:', e?.message || e);
        }

        if (result.sessionsScanned === 0) {
          // Empty chunk: keep going (don't burn a tick budget slot), and grow
          // the next chunk to fast-forward through sparse / cold-start backfill.
          currentChunkSec = Math.min(currentChunkSec * 2, maxChunkSec);
        } else {
          remaining--;
          currentChunkSec = settings.chunkSec;
        }
      }
      console.log(`featherprint: tick totals: ${totalDevices} devices, ` +
        `${totalAlerts} alerts, ${totalSessions} sessions`);
    } catch (e) {
      console.log('featherprint: tick error:', e?.message || e);
    } finally {
      i.running = false;
    }
  }

  // --------------------------------------------------------------------------
  // Core engine -- shared by monitor (persist+notify), lookup (transient),
  // and backfill (persist no notify).
  //
  //   aggregate({timeRange:{start,end}, ipFilter?, policy?, mode, notify, user}) ->
  //     { devices:[record...], devicesTouched, alerts, sessionsScanned, elapsedMs }
  //
  //   mode: 'persist' | 'transient'
  //   user: when set (on-demand lookup), the caller's forced expression is
  //         ANDed into the session query so a user restricted by an admin
  //         `user.expression` cannot read session data outside their scope.
  //         The background monitor passes no user (it is scoped by the admin's
  //         [featherprint-subnet] config instead).
  static async aggregate ({ timeRange, ipFilter, policy, mode = 'transient', notify = false, timeField = '@timestamp', user } = {}) {
    const i = FeatherprintAPIs.#internals;
    if (!i) throw new Error('featherprint not initialized');
    const t0 = Date.now();
    const effectivePolicy = policy ?? i.policy;

    const query = FeatherprintAPIs.#buildEsQuery({ timeRange, ipFilter, policy: effectivePolicy, timeField });
    if (!query) {
      return { devices: [], devicesTouched: 0, alerts: 0, sessionsScanned: 0, elapsedMs: Date.now() - t0 };
    }
    await FeatherprintAPIs.#applyUserExpression(query.query.bool.must, user);

    const sessions = await FeatherprintAPIs.#scrollSessions(query, i.settings.lookupLimit);

    // Bucket sessions by IP. Each session may contribute to multiple IPs
    // (source.ip, destination.ip, arp.ip, dns.answers[*].ip) -- fan out only
    // for in-scope IPs.
    const byIp = new Map();
    for (const sess of sessions) {
      const src = sess._source ?? sess;
      for (const ip of FeatherprintAPIs.#relevantIpsForSession(src, effectivePolicy, ipFilter)) {
        if (!byIp.has(ip)) byIp.set(ip, []);
        byIp.get(ip).push(sess);
      }
    }

    const out = [];
    let alerts = 0;
    for (const [ip, ipSessions] of byIp.entries()) {
      const previous = mode === 'persist' ? await FeatherprintDb.getIp(ip) : null;
      const { flags } = effectivePolicy.lookup(ip);
      const record = FeatherprintClassify.classifyIp(ip, ipSessions, previous, flags);

      if (mode === 'persist') {
        const events = FeatherprintAPIs.#diffEvents(previous, record);
        if (events.length === 0 && previous) {
          record.lastSeen = Math.max(previous.lastSeen ?? 0, record.lastSeen);
        }
        await FeatherprintDb.upsertDevice(record);
        const macEvents = await FeatherprintAPIs.#trackMacForIp(ip, record, record.lastSeen);
        for (const ev of macEvents) events.push(ev);
        for (const ev of events) {
          await FeatherprintDb.appendHistory({
            ip, kind: ev.kind, ts: record.lastSeen, before: ev.before, after: ev.after
          });
        }
        if (notify) {
          alerts += await FeatherprintAPIs.#fireNotifiers(ip, events, effectivePolicy);
        }
      }
      out.push(record);
    }

    return {
      devices: out,
      devicesTouched: out.length,
      alerts,
      sessionsScanned: sessions.length,
      elapsedMs: Date.now() - t0
    };
  }

  // --------------------------------------------------------------------------
  // Direct-IP lookup: convenience wrapper around aggregate. Used by
  // GET /api/featherprint/lookup. `user` (the requesting user) scopes the
  // underlying session query to the user's forced expression + time limit.
  // Throws an Error whose .timeLimit is set when the window is too wide.
  static async lookup (ip, { start, stop, user } = {}) {
    const i = FeatherprintAPIs.#internals;
    const now = Date.now();
    const range = { start: start ?? (now - 24 * 60 * 60 * 1000), end: stop ?? now };
    const tlErr = FeatherprintAPIs.#timeLimitError(user, range.start, range.end);
    if (tlErr) {
      const e = new Error(tlErr);
      e.timeLimit = true;
      throw e;
    }
    // Lookup overrides subnet policy -- operator typed the IP explicitly.
    // parseCidr with no /bits applies the full host mask (v4 and v6).
    const lookupPolicy = new PolicyTree(i.defaults, [
      { cidr: FeatherprintAPIs.parseCidr(ip), flags: {} }
    ]);
    const r = await FeatherprintAPIs.aggregate({
      timeRange: range,
      ipFilter: [ip],
      policy: lookupPolicy,
      mode: 'transient',
      timeField: 'lastPacket',
      user
    });
    return r.devices[0] || { ip, notFound: true };
  }

  // --------------------------------------------------------------------------
  // Admin: resolved policy snapshot for UI inspection.
  static getResolvedConfig () {
    const i = FeatherprintAPIs.#internals;
    if (!i) return null;
    return {
      settings: i.settings,
      defaults: i.defaults,
      subnets: i.subnets.map(s => ({ cidr: s.cidr.text, flags: s.flags }))
    };
  }

  // --------------------------------------------------------------------------
  // Admin: force an immediate tick. Re-entrancy guard in tick() prevents a
  // duplicate scan if one is already in flight.
  static async runTickNow () {
    const i = FeatherprintAPIs.#internals;
    if (!i) throw new Error('featherprint not initialized');
    if (!i.settings.monitorEnabled) return { triggered: false, monitorDisabled: true };
    if (!i.isPrimaryViewer()) return { triggered: false, notPrimary: true };
    if (i.running) return { triggered: false, alreadyRunning: true };
    await FeatherprintAPIs.tick();
    return { triggered: true };
  }

  // --------------------------------------------------------------------------
  // Regression tests: drain everything synchronously. Bypasses primary/lag
  // checks, runs ticks back-to-back with a large per-tick budget and no
  // safety lag until lpValue catches up to now. Returns totals.
  static async processAll () {
    const i = FeatherprintAPIs.#internals;
    if (!i) throw new Error('featherprint not initialized');
    while (i.running) {
      await new Promise(r => setTimeout(r, 50));
    }
    const saved = {
      safetyLagSec: i.settings.safetyLagSec,
      maxChunksPerTick: i.settings.maxChunksPerTick,
      monitorEnabled: i.settings.monitorEnabled,
      isPrimaryViewer: i.isPrimaryViewer
    };
    i.settings.safetyLagSec = 0;
    i.settings.maxChunksPerTick = 1000000;
    i.settings.monitorEnabled = true;
    i.isPrimaryViewer = () => true;
    let iterations = 0;
    try {
      while (iterations < 20) {
        iterations++;
        const before = i.state ? i.state.lpValue : 0;
        await FeatherprintAPIs.tick();
        const after = i.state ? i.state.lpValue : 0;
        if (after <= before) break;
      }
    } finally {
      i.settings.safetyLagSec = saved.safetyLagSec;
      i.settings.maxChunksPerTick = saved.maxChunksPerTick;
      i.settings.monitorEnabled = saved.monitorEnabled;
      i.isPrimaryViewer = saved.isPrimaryViewer;
    }
    return { iterations, lpValue: i.state?.lpValue };
  }

  // ==========================================================================
  // Internals
  // ==========================================================================

  static #buildEsQuery ({ timeRange, ipFilter, policy, timeField = '@timestamp' }) {
    const must = [{
      range: { [timeField]: { gte: timeRange.start, lt: timeRange.end } }
    }];

    if (Array.isArray(ipFilter) && ipFilter.length) {
      const ipShould = [];
      for (const ip of ipFilter) {
        for (const f of FeatherprintAPIs.IP_FIELDS) ipShould.push({ term: { [f]: ip } });
      }
      must.push({ bool: { should: ipShould, minimum_should_match: 1 } });
    } else {
      const subnetFilter = policy.compileEsFilter(FeatherprintAPIs.IP_FIELDS);
      if (!subnetFilter) return null;
      must.push(subnetFilter);
    }

    // Limit to sessions that carry featherprint-relevant signals. The mdns
    // protocol term matters: PTR/SRV-only service announcements have no
    // dns.ip, so an exists clause alone would skip them.
    must.push({
      bool: {
        should: [
          { exists: { field: 'arp.mac' } },
          { exists: { field: 'dhcp.host' } },
          { exists: { field: 'dhcp.mac' } },
          { exists: { field: 'dns.ip' } },
          { term: { protocol: 'mdns' } },
          { exists: { field: 'nbns.host' } },
          { exists: { field: 'ssdp.usn' } },
          { exists: { field: 'ssdp.server' } }
        ],
        minimum_should_match: 1
      }
    });

    return {
      query: { bool: { must } },
      _source: [
        'source.ip', 'source.mac', 'destination.ip',
        'firstPacket', 'lastPacket', 'protocol',
        'arp.ip', 'arp.mac',
        'dhcp.host', 'dhcp.mac', 'dhcp.classId', 'dhcp.yiaddrIp', 'dhcp.requestIp', 'dhcp.paramReqList',
        'dns.ip', 'dns.answers',
        'nbns.host',
        'ssdp.usn', 'ssdp.server', 'ssdp.nt', 'ssdp.st', 'ssdp.location'
      ]
    };
  }

  // AND the caller's admin-forced expression into the session query's `must`
  // array (mirrors buildQuery.js and apiCrons.js). A no-op when there is no
  // user or no forced expression. Resolves any $shortcut references the
  // expression uses.
  static async #applyUserExpression (must, user) {
    if (!user || typeof user.getExpression !== 'function' || !user.getExpression()) return;
    let shortcuts;
    try {
      shortcuts = await Db.getShortcutsCache(user);
    } catch (e) {
      console.log('featherprint: shortcuts cache fetch failed:', e?.message || e);
    }
    arkimeparser.parser.yy = {
      fieldsMap: Config.getFieldsMap(),
      dbFieldsMap: Config.getDBFieldsMap(),
      prefix: internals.prefix,
      emailSearch: true, // expression was set by an admin
      shortcuts: shortcuts || {},
      shortcutTypeMap: internals.shortcutTypeMap
    };
    must.push(arkimeparser.parse(user.getExpression()));
    await BuildQuery.lookupQueryItems(must);
  }

  // Reject a lookup window wider than the caller's admin-set time limit
  // (hours). Returns an error string or null. Mirrors buildQuery.js.
  static #timeLimitError (user, startMs, stopMs) {
    const limit = user?.timeLimit;
    if (!limit) return null;
    if ((stopMs - startMs) / 3600000 > limit) {
      return `User time limit (${limit} hours) exceeded`;
    }
    return null;
  }

  static async #scrollSessions (query, limit) {
    // Db.searchScroll (under searchSessions) does a plain search up to the ES
    // 10k window and scrolls beyond it, so pass the real limit through.
    query.size = limit;
    // @timestamp is session start, but sessions rotate on lastPacket, so a
    // session falling in our window may live in a later/earlier index. Match
    // cron: search across all session indices each time.
    const r = await Db.searchSessions(Db.getSessionIndices(true), query, {});
    const hits = r?.hits?.hits ?? [];
    if (hits.length >= limit) {
      console.log(`WARNING - featherprint: window scan hit featherprintLookupLimit (${limit}); some sessions were not examined`);
    }
    return hits;
  }

  // Addresses that never identify a device: unspecified, broadcast,
  // multicast (224/4, ff00::/8).
  static #isTrackableIp (ip) {
    if (typeof ip !== 'string' || !ip) return false;
    if (ip === '0.0.0.0' || ip === '255.255.255.255' || ip === '::') return false;
    if (ip.includes(':')) {
      return !(ip.length > 1 && (ip[0] === 'f' || ip[0] === 'F') && (ip[1] === 'f' || ip[1] === 'F'));
    }
    return parseInt(ip, 10) < 224;
  }

  static #relevantIpsForSession (src, policy, ipFilter) {
    const out = new Set();
    const add = (ip) => {
      if (!FeatherprintAPIs.#isTrackableIp(ip)) return;
      if (Array.isArray(ipFilter)) {
        if (!ipFilter.includes(ip)) return;
      } else {
        // Only track IPs that fall inside a configured subnet. Without the
        // matched-length gate, an in-scope client's DNS answers (external
        // resolver IPs, CDN ranges) and out-of-scope destination IPs would
        // be persisted as devices with newIp history just for co-occurring
        // in a fetched session.
        const { flags, matched } = policy.lookup(ip);
        if (matched.length === 0 || flags.ignore) return;
      }
      out.add(ip);
    };

    toArray(src?.source?.ip).forEach(add);
    toArray(src?.destination?.ip).forEach(add);
    toArray(src?.arp?.ip).forEach(add);
    toArray(src?.dhcp?.yiaddrIp).forEach(add);
    toArray(src?.dhcp?.requestIp).forEach(add);
    for (const d of toArray(src?.dns)) {
      for (const a of toArray(d?.answers)) {
        if (a?.ip) add(a.ip);
      }
    }
    return out;
  }

  // --------------------------------------------------------------------------
  // Diff old vs new device record into an event list.
  static #diffEvents (prev, curr) {
    const events = [];
    if (!prev) {
      events.push({
        kind: 'newIp', before: null,
        after: { ip: curr.ip, classification: curr.classification }
      });
      if (curr.mac) events.push({ kind: 'newMac', before: null, after: curr.mac });
      return events;
    }
    if (curr.mac && prev.mac && curr.mac.value !== prev.mac.value) {
      events.push({ kind: 'changeMac', before: prev.mac, after: curr.mac });
    } else if (curr.mac && !prev.mac) {
      events.push({ kind: 'newMac', before: null, after: curr.mac });
    }
    const prevNames = new Set((prev.names ?? []).map(n => n.name));
    for (const n of curr.names ?? []) {
      if (!prevNames.has(n.name)) {
        events.push({ kind: 'newName', before: null, after: { name: n.name, source: n.source } });
      }
    }
    const prevSvcs = new Set((prev.services ?? []).map(s => `${s.type ?? ''}|${s.proto}`));
    for (const s of curr.services ?? []) {
      const k = `${s.type ?? ''}|${s.proto}`;
      if (!prevSvcs.has(k)) {
        events.push({ kind: 'newService', before: null, after: s });
      }
    }
    // changeName: the newest self-announced name (dhcp/nbns) was replaced by
    // a different one from the same source.
    for (const source of ['dhcp', 'nbns']) {
      const newest = (names) => (names ?? [])
        .filter(n => n.source === source)
        .reduce((a, b) => (!a || b.lastSeen > a.lastSeen ? b : a), null);
      const prevTop = newest(prev.names);
      const currTop = newest(curr.names);
      if (prevTop && currTop && prevTop.name !== currTop.name) {
        events.push({
          kind: 'changeName',
          before: { name: prevTop.name, source },
          after: { name: currTop.name, source }
        });
      }
    }
    // changeDevice: classification moved between two known classes.
    if (prev.classification && prev.classification !== 'unknown' &&
        curr.classification !== prev.classification) {
      events.push({
        kind: 'changeDevice',
        before: { classification: prev.classification },
        after: { classification: curr.classification }
      });
    }
    return events;
  }

  // --------------------------------------------------------------------------
  // MAC-perspective tracking: emits changeIp (this MAC was previously seen on
  // a different IP). Only meaningful for MACs that actually identify one
  // device: a low-ranked srcMac observation is typically the last-hop router
  // MAC shared by every IP on a routed segment, so tracking it would emit a
  // bogus changeIp each time a different IP behind that gateway is processed.
  static async #trackMacForIp (ip, curr, ts) {
    if (!curr.mac?.value) return [];
    const srcMacRank = RULES.macSourceRank.srcMac ?? 0;
    if ((RULES.macSourceRank[curr.mac.source] ?? 0) <= srcMacRank) return [];
    const macVal = curr.mac.value;
    let macDoc;
    try {
      macDoc = await FeatherprintDb.getMac(macVal);
    } catch (e) {
      console.log('featherprint: getMac failed:', e?.message);
      return [];
    }
    const events = [];
    const nowTs = ts ?? Date.now();
    if (!macDoc) {
      macDoc = {
        mac: macVal,
        currentIp: ip,
        firstSeen: nowTs,
        lastSeen: nowTs,
        ipHistory: [{ ip, ts: nowTs, source: curr.mac.source }]
      };
    } else if (macDoc.currentIp && macDoc.currentIp !== ip) {
      events.push({
        kind: 'changeIp',
        before: { ip: macDoc.currentIp, mac: macVal },
        after: { ip, mac: macVal }
      });
      macDoc.currentIp = ip;
      macDoc.lastSeen = nowTs;
      macDoc.ipHistory = macDoc.ipHistory ?? [];
      macDoc.ipHistory.push({ ip, ts: nowTs, source: curr.mac.source });
      if (macDoc.ipHistory.length > 50) macDoc.ipHistory = macDoc.ipHistory.slice(-50);
    } else {
      macDoc.lastSeen = nowTs;
      if (!macDoc.currentIp) macDoc.currentIp = ip;
    }
    try {
      await FeatherprintDb.upsertMac(macDoc);
    } catch (e) {
      console.log('featherprint: upsertMac failed:', e?.message);
    }
    return events;
  }

  // --------------------------------------------------------------------------
  static async #fireNotifiers (ip, events, policy) {
    const i = FeatherprintAPIs.#internals;
    let fired = 0;
    const { flags } = policy.lookup(ip);
    const notifierId = i.settings.notifier;
    for (const ev of events) {
      const flagName = FeatherprintAPIs.EVENT_KIND_TO_FLAG[ev.kind];
      if (!flagName || !flags[flagName]) continue;
      const alertDoc = {
        ip,
        kind: ev.kind,
        ts: Date.now(),
        before: ev.before,
        after: ev.after,
        acked: false,
        message: `featherprint: ${ip} ${ev.kind}`
      };
      try {
        await FeatherprintDb.insertAlert(alertDoc);
        fired++;
      } catch (e) {
        console.log('featherprint: alert insert failed:', e?.message);
      }
      if (notifierId) {
        try {
          Notifier.issueAlert(notifierId,
            `featherprint: ${ip} ${ev.kind}: ${JSON.stringify(ev.after)}`,
            () => {});
        } catch (e) {
          console.log('featherprint: issueAlert failed:', e?.message);
        }
      }
    }
    return fired;
  }

  // ==========================================================================
  // HTTP handlers
  // ==========================================================================

  // --------------------------------------------------------------------------
  /**
   * GET - /api/featherprint/ip/:ip
   *
   * Fetch the persisted featherprint device record for a single IP.
   * @name /featherprint/ip/:ip
   * @param {string} :ip - The IPv4 / IPv6 address whose device record to fetch.
   * @returns {boolean} success - True on hit, false on miss.
   * @returns {object} device - The persisted device record (mac, names, services, classification, ...).
   */
  // Exact IPv4/IPv6 address, or a CIDR over one.
  static #isIpOrCidr (s) {
    if (typeof s !== 'string') return false;
    const slash = s.indexOf('/');
    const ip = slash === -1 ? s : s.slice(0, slash);
    if (net.isIP(ip) === 0) return false;
    if (slash === -1) return true;
    const bits = +s.slice(slash + 1);
    return Number.isInteger(bits) && bits >= 0 && bits <= (net.isIP(ip) === 6 ? 128 : 32);
  }

  static #limitParam (v, dflt, max) {
    const n = parseInt(v ?? dflt, 10);
    if (isNaN(n) || n < 1) return dflt;
    return Math.min(n, max);
  }

  static async apiGetIp (req, res) {
    try {
      if (net.isIP(req.params.ip) === 0) {
        return res.serverError(400, 'Invalid ip');
      }
      const doc = await FeatherprintDb.getIp(req.params.ip);
      if (!doc) return res.status(404).send({ success: false, text: 'Not found' });
      return res.send({ success: true, device: doc });
    } catch (e) {
      return res.serverError(500, `featherprint getIp: ${e.message}`);
    }
  }

  // --------------------------------------------------------------------------
  /**
   * GET - /api/featherprint/search
   *
   * Search persisted featherprint device records by IP / MAC / name. All
   * filters are optional and combine as AND. MAC and name are substring
   * matches; ip must be an exact IP or CIDR (ES ip fields don't support
   * wildcards).
   * @name /featherprint/search
   * @param {string} ip - Exact IP or CIDR.
   * @param {string} mac - Substring of a MAC address (case insensitive).
   * @param {string} name - Substring of a discovered name (DNS/NBNS/DHCP/SSDP).
   * @param {number} limit=200 - Max records to return.
   * @returns {boolean} success - Whether the search succeeded.
   * @returns {array} devices - Matching device records, sorted by lastSeen desc.
   */
  static async apiSearch (req, res) {
    try {
      if (req.query.ip !== undefined && !FeatherprintAPIs.#isIpOrCidr(req.query.ip)) {
        return res.serverError(400, 'ip must be an exact IP or CIDR');
      }
      const out = await FeatherprintDb.searchDevices({
        ip: req.query.ip,
        mac: ArkimeUtil.isString(req.query.mac) ? req.query.mac : undefined,
        name: ArkimeUtil.isString(req.query.name) ? req.query.name : undefined,
        limit: FeatherprintAPIs.#limitParam(req.query.limit, 200, 10000)
      });
      return res.send({ success: true, devices: out });
    } catch (e) {
      return res.serverError(500, `featherprint search: ${e.message}`);
    }
  }

  // --------------------------------------------------------------------------
  /**
   * GET - /api/featherprint/history/:ip
   *
   * Return the recent change history for one IP (newIp, newMac, changeMac,
   * changeIp, newName, newService, ...). Sorted by ts desc.
   * @name /featherprint/history/:ip
   * @param {string} :ip - The IP whose history to fetch.
   * @param {number} limit=50 - Max history entries to return.
   * @returns {boolean} success - Whether the lookup succeeded.
   * @returns {array} history - Event log entries.
   */
  static async apiGetHistory (req, res) {
    try {
      if (net.isIP(req.params.ip) === 0) {
        return res.serverError(400, 'Invalid ip');
      }
      const out = await FeatherprintDb.listHistory(
        req.params.ip,
        FeatherprintAPIs.#limitParam(req.query.limit, 50, 1000)
      );
      return res.send({ success: true, history: out });
    } catch (e) {
      return res.serverError(500, `featherprint history: ${e.message}`);
    }
  }

  // --------------------------------------------------------------------------
  /**
   * GET - /api/featherprint/alerts
   *
   * Return featherprint alerts (one per event that matched an `alertOn*`
   * policy flag), sorted by ts desc. Includes both open and acked alerts;
   * callers filter client-side on `acked`.
   * @name /featherprint/alerts
   * @param {number} limit=500 - Max alerts to return.
   * @param {string} acked - Optional server-side filter: "true" or "false".
   * @returns {boolean} success - Whether the lookup succeeded.
   * @returns {array} alerts - Alert documents, newest first.
   */
  static async apiGetAlerts (req, res) {
    try {
      let acked;
      if (req.query.acked === 'true') acked = true;
      else if (req.query.acked === 'false') acked = false;
      const out = await FeatherprintDb.listAlerts({
        acked,
        limit: FeatherprintAPIs.#limitParam(req.query.limit, 500, 10000)
      });
      return res.send({ success: true, alerts: out });
    } catch (e) {
      return res.serverError(500, `featherprint alerts: ${e.message}`);
    }
  }

  // --------------------------------------------------------------------------
  /**
   * POST - /api/featherprint/ack/:id
   *
   * Acknowledge a single featherprint alert. The alert stays in the index
   * but is marked acked with the user and timestamp.
   * @name /featherprint/ack/:id
   * @param {string} :id - The alert document id to ack.
   * @returns {boolean} success - Whether the ack persisted.
   */
  static async apiAckAlert (req, res) {
    try {
      if (!ArkimeUtil.isString(req.params.id)) {
        return res.serverError(400, 'Invalid id');
      }
      const user = req.user?.userId || 'unknown';
      const ackedAt = Date.now();
      await FeatherprintDb.ackAlert(req.params.id, user, ackedAt);
      return res.send({ success: true, ackedBy: user, ackedAt });
    } catch (e) {
      return res.serverError(500, `featherprint ack: ${e.message}`);
    }
  }

  // --------------------------------------------------------------------------
  /**
   * GET - /api/featherprint/lookup
   *
   * Transient on-demand fingerprint for an IP. Reads sessions ES directly,
   * never persists, never fires notifiers, and works on any viewer node
   * (no cron-leader requirement). Subnet policy is bypassed -- the operator
   * typed the IP explicitly.
   * @name /featherprint/lookup
   * @param {string} ip - The IP to fingerprint (also accepted as :ip route param).
   * @param {number} start - Window start (seconds since epoch). Default: now - 24h.
   * @param {number} stop - Window end (seconds since epoch). Default: now.
   * @returns {boolean} success - Whether the lookup succeeded.
   * @returns {object} device - The transient device record (or {ip, notFound:true}).
   */
  static async apiLookup (req, res) {
    try {
      const ip = req.query.ip || req.params.ip;
      if (!ip) return res.status(400).send({ success: false, text: 'ip required' });
      if (net.isIP(ip) === 0) return res.serverError(400, 'Invalid ip');
      const start = req.query.start ? parseInt(req.query.start, 10) * 1000 : undefined;
      const stop = req.query.stop ? parseInt(req.query.stop, 10) * 1000 : undefined;
      const device = await FeatherprintAPIs.lookup(ip, {
        start: isNaN(start) ? undefined : start,
        stop: isNaN(stop) ? undefined : stop,
        user: req.user
      });
      return res.send({ success: true, device });
    } catch (e) {
      if (e.timeLimit) return res.serverError(403, e.message);
      return res.serverError(500, `featherprint lookup: ${e.message}`);
    }
  }

  // --------------------------------------------------------------------------
  /**
   * GET - /api/featherprint/state
   *
   * Return the persisted monitor cursor (lpValue, lastProcessedTs, ...).
   * Used by the UI status row to show how far the monitor has caught up.
   * @name /featherprint/state
   * @returns {boolean} success - Whether the state lookup succeeded.
   * @returns {object} state - The persisted monitor state (or null if uninitialized).
   */
  static async apiGetMonitorState (req, res) {
    try {
      const state = await FeatherprintDb.getState();
      return res.send({ success: true, state: state || null });
    } catch (e) {
      return res.serverError(500, `featherprint state: ${e.message}`);
    }
  }

  // --------------------------------------------------------------------------
  /**
   * GET - /api/featherprint/config
   *
   * Admin: snapshot of the resolved policy (settings + defaults + per-subnet
   * flag bags). Reflects [featherprint-defaults] and [featherprint-subnet]
   * as parsed at viewer start.
   * @name /featherprint/config
   * @returns {boolean} success - Always true.
   * @returns {object} config - {settings, defaults, subnets:[{cidr, flags}]}.
   */
  static async apiGetConfig (req, res) {
    try {
      return res.send({ success: true, config: FeatherprintAPIs.getResolvedConfig() });
    } catch (e) {
      return res.serverError(500, `featherprint config: ${e.message}`);
    }
  }

  // --------------------------------------------------------------------------
  /**
   * POST - /api/featherprint/tick
   *
   * Admin: force an immediate monitor tick. If a tick is already running, the
   * existing run owns the work; the re-entrancy guard in tick() prevents a
   * duplicate scan. Returns status flags so the UI can show why the tick
   * was skipped (notPrimary, alreadyRunning, monitorDisabled).
   * @name /featherprint/tick
   * @returns {boolean} success - Whether the request was processed.
   * @returns {boolean} triggered - Whether a new tick actually started.
   * @returns {boolean} alreadyRunning - True if a tick was already in flight.
   * @returns {boolean} notPrimary - True if this node is not the cron leader.
   * @returns {boolean} monitorDisabled - True if featherprintMonitorEnabled=false.
   */
  static async apiRunTickNow (req, res) {
    try {
      const result = await FeatherprintAPIs.runTickNow();
      return res.send({ success: true, ...result });
    } catch (e) {
      return res.serverError(500, `featherprint tick: ${e.message}`);
    }
  }
}

module.exports = FeatherprintAPIs;
