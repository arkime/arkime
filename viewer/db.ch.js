/******************************************************************************/
/* db.ch.js -- ClickHouse sessions implementation of the viewer DB backend
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

const fs = require('fs');
const http = require('http');
const https = require('https');
const NodeURL = require('url').URL;
const ArkimeUtil = require('../common/arkimeUtil');

let Config;

// Materialized typed columns (vs. JSON path subcolumns of `fields`).
// Map field name -> { col: <SQL identifier>, type: 'String'|'DateTime64' }.
const MATERIALIZED = {
  _id: { col: '`_id`', type: 'String' },
  id: { col: '`_id`', type: 'String' },
  node: { col: 'node', type: 'String' },
  lastPacket: { col: 'lastPacket', type: 'DateTime64' },
  firstPacket: { col: 'firstPacket', type: 'DateTime64' },
  timestamp: { col: 'timestamp', type: 'DateTime64' },
  '@timestamp': { col: 'timestamp', type: 'DateTime64' },
  'source.ip': { col: '`source.ip`', type: 'String' },
  'destination.ip': { col: '`destination.ip`', type: 'String' }
};

const ARRAY_FIELDS = new Set(['protocol', 'tags', 'fileId', 'packetPos', 'packetLen', 'huntId', 'huntName']);
const DATE_FIELDS = new Set(['timestamp', '@timestamp', 'firstPacket', 'lastPacket', 'cert.notBefore', 'cert.notAfter']);
const NUMERIC_HINTS = new Set([
  'length', 'ipProtocol', 'source.port', 'source.bytes', 'source.packets', 'srcDataBytes',
  'destination.port', 'destination.bytes', 'destination.packets', 'dstDataBytes', 'totDataBytes',
  'network.bytes', 'network.packets', 'segmentCnt', 'protocolCnt', 'tagsCnt', 'partial',
  'client.bytes', 'server.bytes'
]);
const DEFAULT_FIELDS = [
  'id', 'node', 'firstPacket', 'lastPacket', 'ipProtocol', 'source.ip', 'source.port', 'source.bytes',
  'source.packets', 'destination.ip', 'destination.port', 'destination.bytes', 'destination.packets',
  'network.bytes', 'network.packets', 'totDataBytes', 'protocol', 'tags', 'fileId', 'packetPos',
  'packetLen', 'rootId'
];

// Object-array parents in JSON column: `fields.<parent>` is Array(JSON), where
// each element is an object containing the sub-field. Sub-paths like
// `cert.notAfter` cannot be addressed directly via `fields.cert.notAfter` and
// must be unrolled via arrayMap/arrayExists over CAST(... AS Array(JSON)).
// Populated dynamically from the JSON schema; seeded with built-ins so cold
// queries don't break before the first refresh completes.
const NESTED_OBJECT_ARRAY_PARENTS = new Set(['cert', 'dns']);

// Array-of-objects children inside a nested parent, e.g. each element of
// fields.dns[] carries an answers[] array of objects (dnsOutputAnswers)
const NESTED_OBJECT_SUB_ARRAYS = new Set(['dns.answers']);

// Paths whose CH type is Array(...) — capture/db.c writes nearly every
// non-counter field as a JSON array. Populated from the live schema by
// refreshSchema(); this lets isArrayField default to "true" for any field we
// have seen, which matches ES multi-value semantics.
const ARRAY_PATH_FIELDS = new Set();

function nestedArrayParent (field) {
  const dot = field.indexOf('.');
  if (dot < 0) { return null; }
  const parentPath = field.substring(0, dot);
  if (!NESTED_OBJECT_ARRAY_PARENTS.has(parentPath)) { return null; }
  return { parent: parentPath, sub: field.substring(dot + 1) };
}

function jsonPath (field) {
  // Build `fields.seg1.seg2...` quoting segments that aren't bare identifiers.
  const segs = field.split('.').map(p => /^[A-Za-z_][A-Za-z0-9_]*$/.test(p) ? p : '`' + p.replace(/`/g, '``') + '`');
  return 'fields.' + segs.join('.');
}

// Sub-path inside a nested object element (no `fields.` prefix).
function subPath (sub) {
  const segs = sub.split('.').map(p => /^[A-Za-z_][A-Za-z0-9_]*$/.test(p) ? p : '`' + p.replace(/`/g, '``') + '`');
  return segs.join('.');
}

function chError (feature) {
  return new Error(`CH backend: ${feature} not yet implemented`);
}

function quoteIdent (ident) {
  return '`' + String(ident).replace(/`/g, '``') + '`';
}

function escapeLike (value) {
  return String(value).replace(/([%_\\])/g, '\\$1');
}

function sqlLiteral (value, field) {
  if (value === null || value === undefined) { return 'NULL'; }
  if (Array.isArray(value)) {
    return '[' + value.map(v => sqlLiteral(v, field)).join(', ') + ']';
  }
  if (DATE_FIELDS.has(field)) {
    if (typeof value === 'string') {
      const parsed = Date.parse(value);
      if (!Number.isNaN(parsed)) {
        return `fromUnixTimestamp64Milli(${parsed})`;
      }
    }
    const nvalue = Number(value);
    if (!Number.isNaN(nvalue)) {
      return `fromUnixTimestamp64Milli(${Math.trunc(nvalue)})`;
    }
  }
  if (typeof value === 'number') {
    if (!Number.isFinite(value)) { throw chError('non-finite numeric literal'); }
    return String(value);
  }
  if (typeof value === 'boolean') { return value ? '1' : '0'; }
  return '\'' + String(value).replace(/\\/g, '\\\\').replace(/'/g, "\\'") + '\'';
}

function wildcardToLike (value) {
  return escapeLike(value).replace(/\\\*/g, '*').replace(/\\\?/g, '?').replace(/\*/g, '%').replace(/\?/g, '_');
}

/* eslint-disable no-bitwise -- IP address math below is inherently bitwise */
const IP6_MAX = (1n << 128n) - 1n;

// Parse an IPv6 address (optionally with an embedded trailing IPv4) into a
// 128-bit BigInt; null if malformed.
function parseIp6 (addr) {
  let head = addr; let tail = '';
  const dc = addr.indexOf('::');
  if (dc !== -1) {
    if (addr.indexOf('::', dc + 1) !== -1) { return null; }
    head = addr.substring(0, dc); tail = addr.substring(dc + 2);
  }
  const expand = (part) => part === '' ? [] : part.split(':');
  const headGroups = expand(head); const tailGroups = expand(tail);
  // Embedded IPv4 in the last group -> two hex groups
  const fixV4 = (gs) => {
    if (gs.length && gs[gs.length - 1].includes('.')) {
      const octets = gs.pop().split('.').map(Number);
      if (octets.length !== 4 || octets.some(o => !(o >= 0 && o <= 255))) { return null; }
      gs.push(((octets[0] << 8) | octets[1]).toString(16), ((octets[2] << 8) | octets[3]).toString(16));
    }
    return gs;
  };
  if (fixV4(headGroups) === null || fixV4(tailGroups) === null) { return null; }
  const missing = 8 - headGroups.length - tailGroups.length;
  if (dc === -1 ? missing !== 0 : missing < 0) { return null; }
  const groups = [...headGroups, ...Array(dc === -1 ? 0 : missing).fill('0'), ...tailGroups];
  let big = 0n;
  for (const g of groups) {
    if (!/^[0-9a-fA-F]{1,4}$/.test(g)) { return null; }
    big = (big << 16n) | BigInt(parseInt(g, 16));
  }
  return big;
}

function formatIp6 (big) {
  const groups = [];
  for (let i = 7; i >= 0; i--) { groups.push(((big >> BigInt(i * 16)) & 0xffffn).toString(16)); }
  return groups.join(':');
}

// Parse an ip query value — full, partial ("192.30.252" = /24), CIDR, or
// IPv6 in any textual form — into a normalized {lo, hi, exact} range.
// Returns null when the value isn't a usable ip expression.
function ipRange (value) {
  const str = String(value).trim();
  const slash = str.indexOf('/');
  const addr = slash === -1 ? str : str.substring(0, slash);
  let prefix = slash === -1 ? null : Number(str.substring(slash + 1));
  if (slash !== -1 && (!/^\d{1,3}$/.test(str.substring(slash + 1)))) { return null; }

  if (addr.includes(':')) {
    const big = parseIp6(addr);
    if (big === null) { return null; }
    prefix ??= 128;
    if (prefix < 0 || prefix > 128) { return null; }
    const mask6 = prefix === 0 ? 0n : (IP6_MAX << BigInt(128 - prefix)) & IP6_MAX;
    const lo6 = big & mask6;
    const hi6 = lo6 | (IP6_MAX & ~mask6);
    return { lo: formatIp6(lo6), hi: formatIp6(hi6), exact: prefix === 128 };
  }

  const parts = addr.split('.').filter(p => p !== '');
  if (parts.length === 0 || parts.length > 4 || parts.some(p => !/^\d{1,3}$/.test(p) || Number(p) > 255)) { return null; }
  prefix ??= parts.length * 8;
  if (prefix < 0 || prefix > 32) { return null; }
  while (parts.length < 4) { parts.push('0'); }
  let n = 0;
  for (const p of parts) { n = n * 256 + Number(p); }
  const mask = prefix === 0 ? 0 : (0xFFFFFFFF << (32 - prefix)) >>> 0;
  const lo = (n & mask) >>> 0;
  const hi = (lo | (~mask >>> 0)) >>> 0;
  const dotted = (x) => [x >>> 24, (x >>> 16) & 255, (x >>> 8) & 255, x & 255].join('.');
  return { lo: dotted(lo), hi: dotted(hi), exact: prefix === 32 && slash === -1 };
}
/* eslint-enable no-bitwise */

// Index names carry a time suffix from capture's config.rotate
// ("sessions3-060220", "-060220h04", "-99w03", "-99m03"). Map it to a
// [startMs, endMs) lastPacket range so id lookups and updates prune to a few
// partitions instead of scanning every day. Returns null when the index has
// no parseable suffix (e.g. bare "sessions3" or wildcard searches).
function timeRangeFromIndexName (index) {
  const str = String(index ?? '');
  // Comma lists and wildcards span an unknown range — never constrain those.
  if (str.includes(',') || str.includes('*')) { return null; }
  const yearOf = (yy) => (yy >= 70 ? 1900 + yy : 2000 + yy);

  let m = str.match(/-(\d{2})(\d{2})(\d{2})(?:h\d{2})?$/); // daily/hourly
  if (m) {
    const start = Date.UTC(yearOf(+m[1]), +m[2] - 1, +m[3]);
    return [start, start + 86400000];
  }
  m = str.match(/-(\d{2})w(\d{2})$/); // weekly: week = yday/7, 0-based
  if (m) {
    const start = Date.UTC(yearOf(+m[1]), 0, 1) + (+m[2]) * 7 * 86400000;
    return [start, start + 7 * 86400000];
  }
  m = str.match(/-(\d{2})m(\d{2})$/); // monthly
  if (m) {
    const year = yearOf(+m[1]); const mon = +m[2];
    return [Date.UTC(year, mon - 1, 1), Date.UTC(year + (mon === 12 ? 1 : 0), mon === 12 ? 0 : mon, 1)];
  }
  return null;
}

// [startMs, endMs) envelope covering all given index names; null when any
// index has no parseable time suffix.
function timeEnvelopeFromIndex (index) {
  const indexes = Array.isArray(index) ? index : [index];
  let start = Infinity; let end = -Infinity;
  for (const item of indexes) {
    const range = timeRangeFromIndexName(item);
    if (!range) { return null; } // one unparseable index -> no pruning
    start = Math.min(start, range[0]);
    end = Math.max(end, range[1]);
  }
  if (!Number.isFinite(start) || !Number.isFinite(end)) { return null; }
  return [start, end];
}

// Parse the typed paths out of a JSON column type string from system.columns,
// e.g. "JSON(max_dynamic_paths = 4096, `source.port` Nullable(Int64), tags Array(String))"
// -> Map { 'source.port' => 'Nullable(Int64)', 'tags' => 'Array(String)' }.
// Typed paths are real typed subcolumns, so they can be read directly without
// Dynamic-type dispatch.
function parseTypedPaths (typeStr) {
  const map = new Map();
  if (typeof typeStr !== 'string' || !typeStr.startsWith('JSON(') || !typeStr.endsWith(')')) { return map; }
  const inner = typeStr.slice(5, -1);

  // Split on top-level commas, respecting parens and backticks
  const parts = [];
  let depth = 0; let inTick = false; let cur = '';
  for (const ch of inner) {
    if (ch === '`') { inTick = !inTick; }
    if (!inTick) {
      if (ch === '(') { depth++; }
      if (ch === ')') { depth--; }
      if (ch === ',' && depth === 0) { parts.push(cur); cur = ''; continue; }
    }
    cur += ch;
  }
  if (cur.trim()) { parts.push(cur); }

  for (let part of parts) {
    part = part.trim();
    if (/^(max_dynamic_paths|max_dynamic_types)\s*=/.test(part)) { continue; }
    if (/^SKIP\b/.test(part)) { continue; }
    let path, type;
    if (part.startsWith('`')) {
      const end = part.indexOf('`', 1);
      if (end < 0) { continue; }
      path = part.slice(1, end);
      type = part.slice(end + 1).trim();
    } else {
      const sp = part.search(/\s/);
      if (sp < 0) { continue; }
      path = part.slice(0, sp);
      type = part.slice(sp + 1).trim();
    }
    if (path && type) { map.set(path, type); }
  }
  return map;
}

function dbFieldsMap () {
  try {
    Config ??= require('./config.js');
    return Config.getDBFieldsMap?.() || {};
  } catch (err) {
    return {};
  }
}

class DbCHImpl {
  constructor (info) {
    const dbUrl = info.sessionsDbUrl;
    if (!dbUrl) { throw new Error('CH backend: sessionsDbUrl is required'); }

    let normalized = dbUrl;
    if (normalized.startsWith('clickhouse://') || normalized.startsWith('chttp://')) {
      normalized = normalized.replace(/^[^:]+:\/\//, 'http://');
    } else if (normalized.startsWith('chttps://')) {
      normalized = normalized.replace(/^chttps:\/\//, 'https://');
    }
    const url = new NodeURL(normalized);

    this.url = url;
    this.prefix = ArkimeUtil.formatPrefix(info.prefix);
    this.database = info.clickhouseDatabase || url.pathname.substring(1) || 'arkime';
    this.table = info.clickhouseSessionsTable || 'sessions3';
    this.index = `${this.prefix}sessions3`;
    this.requestTimeout = (parseInt(info.requestTimeout, 10) + 30) * 1000 || 330000;

    const agentOpts = { keepAlive: true, rejectUnauthorized: !info.insecure && !info.clickhouseInsecure };
    const caFile = info.clickhouseCABundle || info.caTrustFile;
    if (caFile) { agentOpts.ca = fs.readFileSync(caFile); }
    this.httpModule = url.protocol === 'https:' ? https : http;
    this.agent = url.protocol === 'https:' ? new https.Agent(agentOpts) : new http.Agent({ keepAlive: true });

    const username = info.clickhouseUser || decodeURIComponent(url.username || '');
    const password = info.clickhousePassword || decodeURIComponent(url.password || '');
    this.headers = {};
    if (username || password) {
      this.headers.Authorization = 'Basic ' + Buffer.from(`${username}:${password}`).toString('base64');
    }
    url.username = '';
    url.password = '';

    // Typed paths declared in the table DDL (populated by refreshSchema).
    this.typedPaths = new Map();

    // Auto-discover Array(JSON) parents in the `fields` column. Refresh
    // periodically so newly-seen JSON paths (e.g. from new parsers) become
    // addressable as nested object arrays without code changes.
    this.refreshSchema().catch((err) => {
      if (Config?.debug >= 1) { console.log('CH backend: initial schema refresh failed:', err.message); }
    });
    this.schemaRefreshTimer = setInterval(() => {
      this.refreshSchema().catch(() => {});
    }, 5 * 60 * 1000);
    this.schemaRefreshTimer.unref?.();
  }

  async refreshSchema () {
    // Typed paths from the DDL are real typed subcolumns; learn them so
    // columnForField can read them directly instead of via Dynamic dispatch.
    try {
      const colRes = await this.query(`SELECT type FROM system.columns WHERE database = ${sqlLiteral(this.database)} AND table = ${sqlLiteral(this.prefix + this.table)} AND name = 'fields'`);
      const typed = parseTypedPaths(colRes?.data?.[0]?.type);
      if (typed.size) { this.typedPaths = typed; }
    } catch (err) {
      if (Config?.debug >= 1) { console.log('CH backend: typed path discovery failed:', err.message); }
    }

    const sql = `SELECT distinctJSONPathsAndTypes(fields) AS p FROM ${quoteIdent(this.index)}`;
    const res = await this.query(sql);
    const map = res?.data?.[0]?.p;
    if (!map || typeof map !== 'object') { return; }
    for (const [path, types] of Object.entries(map)) {
      if (!Array.isArray(types)) { continue; }
      const isArr = types.some(t => typeof t === 'string' && t.startsWith('Array('));
      if (!isArr) { continue; }
      ARRAY_PATH_FIELDS.add(path);
      if (!path.includes('.') && types.some(t => typeof t === 'string' && t.startsWith('Array(JSON'))) {
        NESTED_OBJECT_ARRAY_PARENTS.add(path);
      }
    }
  }

  async query (sql, opts = {}) {
    if (Config?.debug >= 1) { console.log('CH SQL:', sql); }
    // Opt-in per-query timing: ARKIME_CH_TIMING=<file> appends "<ms>\t<sql prefix>"
    if (process.env.ARKIME_CH_TIMING) {
      const t0 = process.hrtime.bigint();
      try {
        return await this.queryInternal(sql, opts);
      } finally {
        const ms = Number(process.hrtime.bigint() - t0) / 1e6;
        const flat = sql.replace(/\s+/g, ' ');
        const wi = flat.indexOf('WHERE');
        const stack = new Error().stack.split('\n').slice(3, 8).map(s => s.trim().replace(/^at /, '')).join(' < ');
        const logSql = flat.substring(0, 110) + ' ||| ' + (wi >= 0 ? flat.substring(wi, wi + 200) : '') + ' ||| ' + stack;
        fs.appendFile(process.env.ARKIME_CH_TIMING, `${process.pid}\t${ms.toFixed(2)}\t${logSql}\n`, () => {});
      }
    }
    return this.queryInternal(sql, opts);
  }

  async queryInternal (sql, opts = {}) {
    const reqUrl = new NodeURL(this.url.toString());
    reqUrl.pathname = '/';
    reqUrl.search = '';
    reqUrl.searchParams.set('database', this.database);
    reqUrl.searchParams.set('default_format', opts.format || 'JSON');
    if (opts.mutationsSync) { reqUrl.searchParams.set('mutations_sync', '1'); }

    const body = opts.raw || opts.text ? sql : (opts.format === 'TabSeparated' ? sql : `${sql}\nFORMAT JSON`);

    return new Promise((resolve, reject) => {
      const req = this.httpModule.request(reqUrl, {
        method: 'POST',
        agent: this.agent,
        timeout: this.requestTimeout,
        headers: {
          ...this.headers,
          'Content-Type': 'text/plain; charset=UTF-8',
          'Content-Length': Buffer.byteLength(body)
        }
      }, (res) => {
        const chunks = [];
        res.on('data', (chunk) => chunks.push(chunk));
        res.on('end', () => {
          const text = Buffer.concat(chunks).toString();
          if (res.statusCode < 200 || res.statusCode >= 300) {
            return reject(new Error(`CH backend: query failed (${res.statusCode}): ${text}`));
          }
          if (opts.text) { return resolve(text); }
          try {
            resolve(text.length ? JSON.parse(text) : {});
          } catch (err) {
            err.message = `CH backend: failed to parse JSON response: ${err.message}; body=${text}`;
            reject(err);
          }
        });
      });
      req.on('timeout', () => req.destroy(new Error('CH backend: query timed out')));
      req.on('error', reject);
      req.end(body);
    });
  }

  isArrayField (field) {
    if (MATERIALIZED[field]) { return false; }
    // Typed paths declare their type in the DDL — most authoritative.
    const typed = this.typedPaths.get(field);
    if (typed) { return typed.startsWith('Array'); }
    // Authoritative: trust the live JSON schema. capture/db.c writes nearly
    // every non-counter field as an array, so any path discovered with an
    // Array(...) type is multi-valued.
    if (ARRAY_PATH_FIELDS.has(field)) { return true; }
    if (ARRAY_FIELDS.has(field)) { return true; }
    if (nestedArrayParent(field)) { return true; }
    if (NUMERIC_HINTS.has(field) || DATE_FIELDS.has(field)) { return false; }
    const info = dbFieldsMap()[field];
    if (!info) { return false; }
    // textfield types are the *Tokens fields, which capture writes as arrays
    if (['termfield', 'lotermfield', 'uptermfield', 'textfield', 'lotextfield', 'uptextfield', 'ip'].includes(info.type)) { return true; }
    return false;
  }

  // True if this field lives in the JSON column AND holds an array.
  // Materialized columns are scalar regardless of ES type metadata.
  isJsonArrayField (field) {
    if (MATERIALIZED[field]) { return false; }
    return this.isArrayField(field);
  }

  // Typed expression for use in WHERE / ORDER BY / aggregations.
  // Returns SQL that yields a real (non-Dynamic) typed value.
  columnForField (field, forSort = false) {
    const mat = MATERIALIZED[field];
    if (mat) {
      // Qualify with table to avoid alias-shadowing when SELECT exposes
      // a formatted projection under the same name (e.g. lastPacket).
      if (mat.col.startsWith('`') || mat.col.includes('.')) { return mat.col; }
      return `${quoteIdent(this.prefix + this.table)}.${mat.col}`;
    }
    const nested = nestedArrayParent(field);
    if (nested) {
      // Unroll Array(JSON) parent and project the sub-path. Filter out
      // elements where sub is null so callers see a clean array of values.
      const subType = this.scalarTypeForField(field);
      const subDot = nested.sub.indexOf('.');
      if (subDot > 0 && NESTED_OBJECT_SUB_ARRAYS.has(`${nested.parent}.${nested.sub.substring(0, subDot)}`)) {
        // Two-level unroll: fields.parent[].mid[].leaf
        const mid = subPath(nested.sub.substring(0, subDot));
        const leaf = subPath(nested.sub.substring(subDot + 1));
        const inner = `arrayFlatten(arrayMap(a -> if(dynamicType(a.${leaf}) LIKE 'Array%', CAST(a.${leaf} AS Array(${subType})), [CAST(a.${leaf} AS ${subType})]), arrayFilter(a -> isNotNull(a.${leaf}), CAST(c.${mid} AS Array(JSON)))))`;
        return `arrayFlatten(arrayMap(c -> ${inner}, arrayFilter(c -> isNotNull(c.${mid}), CAST(${jsonPath(nested.parent)} AS Array(JSON)))))`;
      }
      const sub = subPath(nested.sub);
      const arr = `CAST(${jsonPath(nested.parent)} AS Array(JSON))`;
      const info = dbFieldsMap()[field];
      if (info?.type === 'date' || DATE_FIELDS.has(field)) {
        // ES returns date sub-fields as ISO strings; emit ISO strings so
        // viewer/db.js fixSessionFields' Date.parse() works.
        return `arrayMap(c -> formatDateTime(fromUnixTimestamp64Milli(CAST(c.${sub} AS Int64), 'UTC'), '%Y-%m-%dT%H:%i:%S.%fZ', 'UTC'), arrayFilter(c -> isNotNull(c.${sub}), ${arr}))`;
      }
      // Sub-values vary in shape per field ({"dns":[{"ASN":["AS4713 ..."],
      // "opcode":"QUERY"}]}), so adapt per element. dynamicType here is cheap:
      // it runs on values already materialized by the Array(JSON) cast, not on
      // a per-part Dynamic column.
      return `arrayFlatten(arrayMap(c -> if(dynamicType(c.${sub}) LIKE 'Array%', CAST(c.${sub} AS Array(${subType})), [CAST(c.${sub} AS ${subType})]), arrayFilter(c -> isNotNull(c.${sub}), ${arr})))`;
    }
    // Typed JSON path: a real typed subcolumn, read it directly — no
    // dynamicType() branching, no `.:` casts.
    if (this.typedPaths.has(field)) {
      return jsonPath(field);
    }
    if (this.isArrayField(field)) {
      let elemType = this.scalarTypeForField(field);
      if (field === 'fileId' || field === 'packetPos' || field === 'packetLen') { elemType = 'Int64'; }
      const path = jsonPath(field);
      // Build an Array(elemType) from a Dynamic path holding Array(T), scalar
      // T, or null — using ONLY `.:` typed-subcolumn reads. dynamicType() and
      // raw Dynamic reads deserialize the whole Dynamic structure in every
      // part and are ~10x slower per part.
      return `arrayMap(x -> assumeNotNull(x), arrayFilter(x -> isNotNull(x), arrayConcat(ifNull(${path}.:\`Array(Nullable(${elemType}))\`, CAST([] AS Array(Nullable(${elemType})))), [${path}.:\`${elemType}\`])))`;
    }
    const t = this.scalarTypeForField(field);
    if (t === 'String') {
      // Use toString() to coerce any underlying scalar (incl. ints) to text.
      return `toString(${jsonPath(field)})`;
    }
    return `${jsonPath(field)}.:\`${t}\``;
  }

  // Expression to use in SELECT projection. Returns native JSON-friendly value.
  selectExpr (field) {
    if (field === 'id') { return `\`_id\` AS \`id\``; }
    if (field === '_id') { return `\`_id\` AS \`_id\``; }
    if (field === '@timestamp') {
      return `formatDateTime(timestamp, '%Y-%m-%dT%H:%i:%S.%fZ', 'UTC') AS \`@timestamp\``;
    }
    const mat = MATERIALIZED[field];
    if (mat && mat.type === 'DateTime64') {
      return `formatDateTime(${mat.col}, '%Y-%m-%dT%H:%i:%S.%fZ', 'UTC') AS \`${field}\``;
    }
    if (mat) { return `${mat.col} AS \`${field}\``; }
    if (nestedArrayParent(field)) {
      return `${this.columnForField(field)} AS \`${field}\``;
    }
    const info = dbFieldsMap()[field];
    if (info?.type === 'date') {
      return `formatDateTime(fromUnixTimestamp64Milli(toInt64OrZero(toString(${jsonPath(field)})), 'UTC'), '%Y-%m-%dT%H:%i:%S.%fZ', 'UTC') AS \`${field}\``;
    }
    return `${jsonPath(field)} AS \`${field}\``;
  }

  scalarTypeForField (field) {
    const info = dbFieldsMap()[field];
    if (info?.type === 'integer') { return 'Int64'; }
    if (info?.type === 'seconds' || info?.type === 'float') { return 'Float64'; }
    if (info?.type === 'date') { return 'Int64'; }
    if (DATE_FIELDS.has(field)) { return 'Int64'; }
    if (NUMERIC_HINTS.has(field) || /(?:Cnt|Bytes|Packets|-cnt|-bytes|-packets)$/.test(field)) { return 'Int64'; }
    return 'String';
  }

  jsonTypeForField (field) {
    return this.scalarTypeForField(field);
  }

  // Legacy name kept for compatibility w/ older callers.
  jsonExtract (field) {
    return this.columnForField(field);
  }

  predicateForFieldValue (field, op, value) {
    if (op === 'IN' && value.length === 0) { return '0'; }
    // ES rounds second-resolution date strings up for lte/gt (lte "10:27:57"
    // means through 10:27:57.999); mirror that so sub-second timestamps match.
    if ((op === '<=' || op === '>') && DATE_FIELDS.has(field) && typeof value === 'string') {
      const parsed = Date.parse(value);
      if (!Number.isNaN(parsed) && parsed % 1000 === 0) { value = parsed + 999; }
    }
    return this.nestedDatePredicate(field, op, value) ??
      this.tokensPredicate(field, op, value) ??
      this.ipPredicate(field, op, value) ??
      this.basicPredicate(field, op, value);
  }

  // ES analyzes both sides of a *Tokens query (wordSplit analyzer: split on
  // \W+, lowercase); capture stores these fields pre-tokenized, so tokenize
  // the query value the same way and require every token to be present.
  // Returns null for non-tokens fields.
  tokensPredicate (field, op, value) {
    const info = dbFieldsMap()[field];
    if (!info?.type?.endsWith('textfield')) { return null; }
    if (op !== '=' && op !== 'IN') { return null; }
    const col = this.columnForField(field);
    const tokensOf = (v) => String(v).toLowerCase().split(/[^a-z0-9_]+/).filter(Boolean);
    const one = (v) => {
      const toks = tokensOf(v);
      if (toks.length === 0) { return '0'; }
      if (toks.length === 1) { return `has(${col}, ${sqlLiteral(toks[0], field)})`; }
      return `hasAll(${col}, [${toks.map(t => sqlLiteral(t, field)).join(', ')}])`;
    };
    const parts = (op === 'IN' ? value : [value]).map(one);
    return parts.length === 1 ? parts[0] : `(${parts.join(' OR ')})`;
  }

  // A range with multiple bounds must be satisfied by a SINGLE value (ES
  // point-range semantics) — separate per-bound arrayExists would let
  // different elements of a multi-valued field satisfy different bounds.
  rangePredicate (field, conds) {
    conds = conds.map(([op, value]) => {
      // Same ES second-rounding as predicateForFieldValue
      if ((op === '<=' || op === '>') && DATE_FIELDS.has(field) && typeof value === 'string') {
        const parsed = Date.parse(value);
        if (!Number.isNaN(parsed) && parsed % 1000 === 0) { return [op, parsed + 999]; }
      }
      return [op, value];
    });
    const nested = nestedArrayParent(field);
    const info = dbFieldsMap()[field];
    if (nested && (info?.type === 'date' || DATE_FIELDS.has(field))) {
      const msConds = conds.map(([op, v]) => [op, typeof v === 'number' ? v : Date.parse(v)]);
      if (!msConds.some(([, ms]) => Number.isNaN(ms))) {
        const sub = subPath(nested.sub);
        const arr = `CAST(${jsonPath(nested.parent)} AS Array(JSON))`;
        const body = msConds.map(([op, ms]) => `CAST(c.${sub} AS Int64) ${op} ${Math.trunc(ms)}`).join(' AND ');
        return `arrayExists(c -> ${body}, arrayFilter(c -> isNotNull(c.${sub}), ${arr}))`;
      }
    }
    if (!nested && this.isJsonArrayField(field) && !this.isIpField(field)) {
      const col = this.columnForField(field);
      const body = conds.map(([op, v]) => `x ${op} ${sqlLiteral(v, field)}`).join(' AND ');
      return `arrayExists(x -> ${body}, ${col})`;
    }
    return conds.map(([op, v]) => this.predicateForFieldValue(field, op, v)).join(' AND ');
  }

  lookupsTableName () {
    return `${quoteIdent(this.database)}.${quoteIdent(this.prefix + 'lookups')}`;
  }

  // ClickHouse equivalent of an ES terms-lookup query: shortcut values live in
  // the synced lookups table, matched via IN (subquery). FINAL collapses
  // ReplacingMergeTree updates so edits are visible immediately.
  termsLookupPredicate (field, lookup) {
    const path = ['ip', 'string', 'number'].includes(lookup.path) ? lookup.path : 'string';
    const sub = (expr) => `(SELECT ${expr}(arrayJoin(${quoteIdent(path)})) FROM ${this.lookupsTableName()} FINAL WHERE id = ${sqlLiteral(String(lookup.id))})`;
    const col = this.columnForField(field);
    const isArr = this.isJsonArrayField(field);
    if (path === 'ip') {
      // Normalize both sides so IPv6 textual variants compare equal
      const ipSet = sub('toIPv6OrNull');
      if (isArr) { return `arrayExists(x -> toIPv6OrNull(toString(x)) IN ${ipSet}, ${col})`; }
      return `toIPv6OrNull(toString(${col})) IN ${ipSet}`;
    }
    if (path === 'number') {
      const numSet = sub('toFloat64');
      if (isArr) { return `arrayExists(x -> toFloat64(x) IN ${numSet}, ${col})`; }
      return `toFloat64(${col}) IN ${numSet}`;
    }
    const strSet = sub('toString');
    if (isArr) { return `arrayExists(x -> toString(x) IN ${strSet}, ${col})`; }
    return `toString(${col}) IN ${strSet}`;
  }

  // Upsert the given shortcut docs (ES lookups hits) into the lookups table.
  // Called from Db.getShortcutsCache whenever the viewer refreshes its
  // shortcuts, so translated queries always see current values. Deleted
  // shortcuts leave stale rows behind, which is harmless: the expression
  // parser rejects unknown shortcut names before translation.
  async syncShortcuts (shortcuts) {
    if (!shortcuts?.length) { return; }
    const asArray = (v) => v === undefined || v === null ? [] : (Array.isArray(v) ? v : [v]);
    const rows = shortcuts.map((s) => JSON.stringify({
      id: String(s._id),
      ip: asArray(s._source.ip).map(String),
      string: asArray(s._source.string).map(String),
      number: asArray(s._source.number).map(Number)
    })).join('\n');
    if (rows === this.lastShortcutsSync) { return; }
    await this.query(`INSERT INTO ${this.lookupsTableName()} (id, ip, string, number) FORMAT JSONEachRow\n${rows}`, { raw: true, text: true });
    this.lastShortcutsSync = rows;
  }

  // Date sub-fields inside object arrays (cert.notBefore/notAfter) are stored
  // as ms integers, but columnForField projects them as ISO strings for
  // display — compare the raw Int64 instead. Returns null when not applicable.
  nestedDatePredicate (field, op, value) {
    const nested = nestedArrayParent(field);
    if (!nested) { return null; }
    const info = dbFieldsMap()[field];
    if (info?.type !== 'date' && !DATE_FIELDS.has(field)) { return null; }
    const ms = typeof value === 'number' ? value : Date.parse(value);
    if (Number.isNaN(ms)) { return null; }
    const sub = subPath(nested.sub);
    const arr = `CAST(${jsonPath(nested.parent)} AS Array(JSON))`;
    return `arrayExists(c -> CAST(c.${sub} AS Int64) ${op === 'IN' ? '=' : op} ${Math.trunc(ms)}, arrayFilter(c -> isNotNull(c.${sub}), ${arr}))`;
  }

  basicPredicate (field, op, value) {
    const col = this.columnForField(field);
    if (this.isJsonArrayField(field)) {
      if (op === '=') { return `has(${col}, ${sqlLiteral(value, field)})`; }
      if (op === 'IN') { return `hasAny(${col}, [${value.map(v => sqlLiteral(v, field)).join(', ')}])`; }
      if (op === '<' || op === '<=' || op === '>' || op === '>=') {
        return `arrayExists(x -> x ${op} ${sqlLiteral(value, field)}, ${col})`;
      }
    }
    if (op === 'IN') { return `${col} IN (${value.map(v => sqlLiteral(v, field)).join(', ')})`; }
    return `${col} ${op} ${sqlLiteral(value, field)}`;
  }

  isIpField (field) {
    if (dbFieldsMap()[field]?.type === 'ip') { return true; }
    return field === 'ip' || /\.ip$/.test(field) || /Ip$/.test(field);
  }

  // ES ip-type semantics on string-stored addresses: CIDR ("10.0.0.0/24"),
  // partial ("192.30.252" = /24), and any textual IPv6 form all match by
  // normalizing through toIPv6OrNull and comparing binary ranges. Exact IPv4
  // stays on the plain string-equality fast path (usable by bloom indexes).
  // Returns null when the field/value isn't an ip expression.
  ipPredicate (field, op, value) {
    if (!this.isIpField(field)) { return null; }
    const col = this.columnForField(field);
    const isArr = this.isJsonArrayField(field);
    const norm = (x) => `toIPv6OrNull(toString(${x}))`;
    const wrap = (cmpFn) => isArr ? `arrayExists(x -> ${cmpFn('x')}, ${col})` : cmpFn(col);
    const between = (r) => (x) => `(${norm(x)} >= toIPv6OrNull(${sqlLiteral(r.lo)}) AND ${norm(x)} <= toIPv6OrNull(${sqlLiteral(r.hi)}))`;

    if (op === '<' || op === '<=' || op === '>' || op === '>=') {
      if (typeof value !== 'string') { return null; }
      const r = ipRange(value);
      if (!r) { return null; }
      return wrap((x) => `${norm(x)} ${op} toIPv6OrNull(${sqlLiteral(r.lo)})`);
    }
    if (op !== '=' && op !== 'IN') { return null; }

    const values = op === 'IN' ? value : [value];
    const parts = []; const plain = [];
    for (const v of values) {
      const r = typeof v === 'string' ? ipRange(v) : null;
      if (r && !r.exact) {
        parts.push(wrap(between(r)));
      } else if (r && r.exact && String(v).includes(':')) {
        // Exact IPv6: normalize both sides so textual variants compare equal
        parts.push(wrap((x) => `${norm(x)} = toIPv6OrNull(${sqlLiteral(r.lo)})`));
      } else {
        plain.push(v);
      }
    }
    if (parts.length === 0) { return null; }
    if (plain.length === 1) { parts.push(this.basicPredicate(field, '=', plain[0])); }
    if (plain.length > 1) { parts.push(this.basicPredicate(field, 'IN', plain)); }
    return parts.length === 1 ? parts[0] : `(${parts.join(' OR ')})`;
  }

  translateClause (clause) {
    if (!clause || Object.keys(clause).length === 0 || clause.match_all) { return '1'; }
    if (clause.bool) { return this.translateBool(clause.bool); }
    if (clause.term) {
      const field = Object.keys(clause.term)[0];
      const value = clause.term[field]?.value ?? clause.term[field];
      return this.predicateForFieldValue(field, '=', value);
    }
    if (clause.terms) {
      const field = Object.keys(clause.terms)[0];
      const value = clause.terms[field];
      // ES terms-lookup ({index, id, path}) = shortcut reference; resolved
      // against the synced lookups table
      if (value && !Array.isArray(value) && typeof value === 'object' && value.id !== undefined && value.path) {
        return this.termsLookupPredicate(field, value);
      }
      return this.predicateForFieldValue(field, 'IN', value);
    }
    if (clause.range) {
      const field = Object.keys(clause.range)[0];
      const range = clause.range[field];
      const conds = [];
      if (range.gt !== undefined) { conds.push(['>', range.gt]); }
      if (range.gte !== undefined) { conds.push(['>=', range.gte]); }
      if (range.lt !== undefined) { conds.push(['<', range.lt]); }
      if (range.lte !== undefined) { conds.push(['<=', range.lte]); }
      if (conds.length === 0) { return '1'; }
      if (conds.length === 1) { return this.predicateForFieldValue(field, conds[0][0], conds[0][1]); }
      return this.rangePredicate(field, conds);
    }
    if (clause.exists) {
      const field = clause.exists.field;
      const mat = MATERIALIZED[field];
      if (mat && mat.type === 'String') { return `${mat.col} != ''`; }
      if (mat) { return `isNotNull(${mat.col})`; }
      const nested = nestedArrayParent(field);
      if (nested) {
        // No isNotNull(parent) guard: SQL evaluates both sides columnar anyway,
        // and a raw Dynamic read is ~10x the cost; CAST yields [] for nulls.
        const sub = subPath(nested.sub);
        const arr = `CAST(${jsonPath(nested.parent)} AS Array(JSON))`;
        return `arrayExists(c -> isNotNull(c.${sub}), ${arr})`;
      }
      if (this.isJsonArrayField(field)) {
        // columnForField already yields [] for missing/null paths; a raw
        // isNotNull(path) read of the Dynamic column would be ~10x slower.
        return `length(${this.columnForField(field)}) > 0`;
      }
      if (this.typedPaths.has(field)) {
        return `isNotNull(${jsonPath(field)})`;
      }
      // Dynamic scalar: check the typed subcolumns instead of reading the raw
      // Dynamic value (slow). Covers the scalar and array representations of
      // the type this backend assumes for the field everywhere else.
      const t = this.scalarTypeForField(field);
      return `(isNotNull(${jsonPath(field)}.:\`${t}\`) OR notEmpty(ifNull(${jsonPath(field)}.:\`Array(Nullable(${t}))\`, CAST([] AS Array(Nullable(${t}))))))`;
    }
    if (clause.prefix) {
      const field = Object.keys(clause.prefix)[0];
      const value = clause.prefix[field]?.value ?? clause.prefix[field];
      if (this.isJsonArrayField(field)) {
        return `arrayExists(x -> startsWith(toString(x), ${sqlLiteral(value)}), ${this.columnForField(field)})`;
      }
      return `startsWith(toString(${this.columnForField(field)}), ${sqlLiteral(value)})`;
    }
    if (clause.wildcard) {
      const field = Object.keys(clause.wildcard)[0];
      const value = clause.wildcard[field]?.value ?? clause.wildcard[field];
      const pattern = sqlLiteral(wildcardToLike(value));
      if (this.isJsonArrayField(field)) {
        return `arrayExists(x -> toString(x) LIKE ${pattern}, ${this.columnForField(field)})`;
      }
      return `toString(${this.columnForField(field)}) LIKE ${pattern}`;
    }
    if (clause.match) {
      const field = Object.keys(clause.match)[0];
      const value = clause.match[field]?.query ?? clause.match[field];
      if (this.isJsonArrayField(field)) {
        return `arrayExists(x -> positionCaseInsensitive(toString(x), ${sqlLiteral(value)}) > 0, ${this.columnForField(field)})`;
      }
      return `positionCaseInsensitive(toString(${this.columnForField(field)}), ${sqlLiteral(value)}) > 0`;
    }
    if (clause.match_phrase) {
      const field = Object.keys(clause.match_phrase)[0];
      const value = clause.match_phrase[field]?.query ?? clause.match_phrase[field];
      // *Tokens fields are stored pre-tokenized; phrase ≈ all tokens present
      const tokens = this.tokensPredicate(field, '=', value);
      if (tokens) { return tokens; }
      if (this.isJsonArrayField(field)) {
        return `arrayExists(x -> position(toString(x), ${sqlLiteral(value)}) > 0, ${this.columnForField(field)})`;
      }
      return `position(toString(${this.columnForField(field)}), ${sqlLiteral(value)}) > 0`;
    }
    if (clause.ids) {
      return this.predicateForFieldValue('id', 'IN', clause.ids.values || []);
    }
    if (clause.query_string) {
      return this.translateQueryString(clause.query_string.query || clause.query_string);
    }
    if (clause.regexp) {
      const field = Object.keys(clause.regexp)[0];
      const value = clause.regexp[field]?.value ?? clause.regexp[field];
      // ES regexp queries anchor to the whole value; RE2 match() is substring.
      const pattern = sqlLiteral(`^(?:${value})$`);
      if (this.isJsonArrayField(field)) {
        return `arrayExists(x -> match(toString(x), ${pattern}), ${this.columnForField(field)})`;
      }
      return `match(toString(${this.columnForField(field)}), ${pattern})`;
    }
    if (clause.nested) { throw chError('nested queries'); }
    if (clause.script) { throw chError('script queries'); }
    throw chError(`query clause ${Object.keys(clause).join(',')}`);
  }

  translateBool (bool) {
    const parts = [];
    const addAll = (items, joiner) => {
      if (!items) { return; }
      if (!Array.isArray(items)) { items = [items]; }
      if (items.length === 0) { return; }
      const translated = items.map(item => `(${this.translateClause(item)})`);
      parts.push(translated.length === 1 ? translated[0] : `(${translated.join(` ${joiner} `)})`);
    };
    addAll(bool.must, 'AND');
    addAll(bool.filter, 'AND');
    if (bool.must_not) {
      // ifNull: comparisons on missing Nullable fields yield NULL, and ES's
      // must_not matches docs that lack the field — NULL must negate to true.
      const mustNot = Array.isArray(bool.must_not) ? bool.must_not : [bool.must_not];
      parts.push(...mustNot.map(item => `NOT ifNull((${this.translateClause(item)}), 0)`));
    }
    if (bool.should) {
      const should = Array.isArray(bool.should) ? bool.should : [bool.should];
      const min = Number(bool.minimum_should_match ?? (parts.length ? 0 : 1));
      if (min > 1) { throw chError('bool minimum_should_match > 1'); }
      if (min === 1) {
        parts.push(`(${should.map(item => `(${this.translateClause(item)})`).join(' OR ')})`);
      }
    }
    return parts.length ? parts.join(' AND ') : '1';
  }

  translateQueryString (query) {
    if (typeof query !== 'string') { throw chError('query_string object syntax'); }
    if (/[[\]{}~^]|\bTO\b|[<>]=?/.test(query)) { throw chError('query_string advanced syntax'); }
    const tokens = query.match(/\(|\)|\bAND\b|\bOR\b|\bNOT\b|[^\s()]+/gi) || [];
    const out = [];
    for (const token of tokens) {
      const upper = token.toUpperCase();
      if (token === '(' || token === ')' || upper === 'AND' || upper === 'OR') {
        out.push(upper === 'AND' || upper === 'OR' ? upper : token);
      } else if (upper === 'NOT') {
        out.push('NOT');
      } else {
        const match = token.match(/^([^:]+):(.+)$/);
        if (!match) { throw chError('query_string free text'); }
        const field = match[1];
        let value = match[2].replace(/^"|"$/g, '');
        if (value.includes('*') || value.includes('?')) {
          out.push(`toString(${this.columnForField(field)}) LIKE ${sqlLiteral(wildcardToLike(value))}`);
        } else {
          value = value.replace(/\\([:\\])/g, '$1');
          out.push(this.predicateForFieldValue(field, '=', value));
        }
      }
    }
    return out.length ? out.join(' ') : '1';
  }

  projectionFromQuery (query) {
    const wanted = new Set(['id']);
    let wantAll = false;
    if (query._source !== false) {
      for (const field of DEFAULT_FIELDS) { wanted.add(field); }
    }
    if (query.fields === '*') { wantAll = true; }
    const fields = Array.isArray(query.fields) ? query.fields : (query.fields === '*' ? [] : query.fields);
    if (Array.isArray(fields)) {
      for (const item of fields) {
        if (typeof item === 'string') {
          if (item === '*') { wantAll = true; } else { wanted.add(item); }
        } else if (item?.field) {
          if (item.field === '*') { wantAll = true; } else { wanted.add(item.field); }
        }
      }
    }
    if (query._source === true) { wantAll = true; }
    if (Array.isArray(query._source)) {
      for (const field of query._source) {
        if (field === '*') { wantAll = true; } else { wanted.add(field); }
      }
    }
    if (Array.isArray(query.docvalue_fields)) {
      for (const item of query.docvalue_fields) {
        if (typeof item === 'string') { wanted.add(item); } else if (item?.field) { wanted.add(item.field); }
      }
    }
    const projection = [...wanted].filter(field => field !== '_source' && field !== '*');
    projection.wantAll = wantAll;
    return projection;
  }

  translateSort (sort) {
    const order = [];
    if (sort) {
      const sorts = Array.isArray(sort) ? sort : [sort];
      for (const item of sorts) {
        if (item === '_doc') { continue; }
        const field = Object.keys(item)[0];
        if (field === '_doc') { continue; }
        const direction = String(item[field]?.order || item[field] || 'asc').toUpperCase() === 'DESC' ? 'DESC' : 'ASC';
        order.push(`${this.columnForField(field, true)} ${direction}`);
      }
    }
    if (order.length === 0) { order.push(`${this.lastPacketCol()} DESC`); }
    order.push(`${quoteIdent(this.prefix + this.table)}.${quoteIdent('_id')} ${order[0].includes(' DESC') ? 'DESC' : 'ASC'}`);
    return order.join(', ');
  }

  translateQuery (esQuery, opts = {}) {
    const query = esQuery || {};
    const projection = this.projectionFromQuery(query);
    const select = projection.map(field => this.selectExpr(field));
    if (!projection.includes('id')) { select.unshift(`\`_id\` AS \`id\``); }

    const where = this.translateClause(query.query || { match_all: {} }) + (opts.timeClause || '');
    const order = this.translateSort(query.sort);
    const size = Math.max(0, Number(opts.size ?? query.size ?? 10));
    const from = Math.max(0, Number(opts.from ?? query.from ?? 0));
    if (from > 10000 && !opts.keyset) { throw chError('from pagination beyond 10000'); }
    let keyset = '';
    if (opts.searchAfter) {
      const idCol = `${quoteIdent(this.prefix + this.table)}.${quoteIdent('_id')}`;
      keyset = ` AND (${this.lastPacketCol()}, ${idCol}) < (${sqlLiteral(opts.searchAfter.lastPacket, 'lastPacket')}, ${sqlLiteral(opts.searchAfter.id)})`;
    }
    if (projection.wantAll) {
      select.push(`toJSONString(fields) AS \`__fields_json__\``);
    }
    // exact_rows_before_limit: the pre-LIMIT match count comes back in the
    // same response (rows_before_limit_at_least), saving a count() query.
    // Useless with LIMIT 0, where ClickHouse short-circuits the scan.
    const settings = size > 0 ? ' SETTINGS exact_rows_before_limit = 1' : '';
    const sql = `SELECT ${select.join(', ')}, formatDateTime(${quoteIdent(this.prefix + this.table)}.lastPacket, '%y%m%d') AS \`_dayIndex\` FROM ${this.tableName()} WHERE (${where})${keyset} ORDER BY ${order} LIMIT ${size}${from ? ` OFFSET ${from}` : ''}${settings}`;
    return { sql, projection, where, size };
  }

  metricExpression (agg) {
    const wrapDate = (field, expr) => DATE_FIELDS.has(field) ? `toUnixTimestamp64Milli(${expr})` : expr;
    if (agg.sum) { return `sum(${this.columnForField(agg.sum.field)})`; }
    if (agg.min) { return wrapDate(agg.min.field, `min(${this.columnForField(agg.min.field)})`); }
    if (agg.max) { return wrapDate(agg.max.field, `max(${this.columnForField(agg.max.field)})`); }
    if (agg.avg) { return wrapDate(agg.avg.field, `avg(${this.columnForField(agg.avg.field)})`); }
    if (agg.cardinality) { return `uniqExact(${this.columnForField(agg.cardinality.field)})`; }
    throw chError(`aggregation ${Object.keys(agg).join(',')}`);
  }

  isMetricAgg (agg) {
    return !!(agg.sum || agg.min || agg.max || agg.avg || agg.cardinality);
  }

  async translateAggs (aggs, where) {
    if (!aggs) { return undefined; }
    const aggregations = {};
    const entries = Object.entries(aggs);

    // ES computes all aggs in one pass; batch all top-level pure-metric aggs
    // into a single scan instead of one query each.
    const metricEntries = entries.filter(([, agg]) => this.isMetricAgg(agg));
    const otherEntries = entries.filter(([, agg]) => !this.isMetricAgg(agg));

    const work = otherEntries.map(async ([aggName, agg]) => {
      aggregations[aggName] = await this.runAgg(agg, where);
    });
    if (metricEntries.length) {
      const select = metricEntries.map(([aggName, agg]) => `${this.metricExpression(agg)} AS ${quoteIdent(aggName)}`).join(', ');
      work.push(this.query(`SELECT ${select} FROM ${this.tableName()} WHERE ${where}`).then((result) => {
        const row = result.data?.[0] || {};
        for (const [aggName] of metricEntries) {
          aggregations[aggName] = { value: row[aggName] ?? 0 };
        }
      }));
    }
    await Promise.all(work);
    return aggregations;
  }

  async runAgg (agg, where) {
    if (agg.terms) { return this.runTermsAgg(agg, where); }
    if (agg.histogram || agg.date_histogram) { return this.runHistogramAgg(agg, where); }
    if (agg.sum || agg.min || agg.max || agg.avg || agg.cardinality) {
      const expr = this.metricExpression(agg);
      const result = await this.query(`SELECT ${expr} AS value FROM ${this.tableName()} WHERE ${where}`);
      return { value: result.data?.[0]?.value ?? 0 };
    }
    throw chError(`aggregation ${Object.keys(agg).join(',')}`);
  }

  async runTermsAgg (agg, where) {
    const terms = agg.terms;
    let keyExpr;
    let field = terms.field;
    let bucketWhereFn = null;
    if (terms.script) {
      // Painless quotes vary by caller; normalize before matching
      const src = (terms.script.source || '').replace(/"/g, "'");
      // Painless script for 'ip' summary: union of source.ip and destination.ip
      if (src.includes("doc['source.ip']") && src.includes("doc['destination.ip']") && !src.includes('_')) {
        const sCol = this.columnForField('source.ip');
        const dCol = this.columnForField('destination.ip');
        keyExpr = `arrayJoin([${sCol}, ${dCol}])`;
        bucketWhereFn = (key) => {
          const lit = sqlLiteral(key, 'source.ip');
          return `(${sCol} = ${lit} OR ${dCol} = ${lit})`;
        };
      // Painless script for 'ip.dst:port'. Two variants exist: sessionsList
      // joins with '_'; spigraphhierarchy joins IPv4 with ':' and IPv6 with
      // '.' (an IPv4 address contains '.', an IPv6 address doesn't).
      } else if (src.includes("doc['destination.ip']") && src.includes("doc['destination.port']")) {
        const ipCol = this.columnForField('destination.ip');
        const portCol = this.columnForField('destination.port');
        const sepExpr = src.includes("'_'") ? "'_'" : `if(position(${ipCol}, '.') > 0, ':', '.')`;
        keyExpr = `concat(${ipCol}, ${sepExpr}, toString(${portCol}))`;
        bucketWhereFn = (key) => {
          const str = String(key);
          // Split at the final separator: '_' when present; otherwise ':'
          // beats '.' only when it comes later (IPv4 'a.b.c.d:port' vs IPv6
          // 'a::b.port', where the address itself contains the other char).
          const idx = str.includes('_') ? str.lastIndexOf('_') : Math.max(str.lastIndexOf(':'), str.lastIndexOf('.'));
          if (idx < 1) { return '0'; }
          const ip = str.slice(0, idx);
          const port = str.slice(idx + 1);
          return `(${ipCol} = ${sqlLiteral(ip, 'destination.ip')} AND ${portCol} = ${sqlLiteral(Number(port), 'destination.port')})`;
        };
      } else {
        throw chError('script terms aggregation');
      }
    } else {
      const baseCol = this.columnForField(field);
      // arrayDistinct matches ES semantics: a doc counts once per bucket even
      // if the value repeats within its array.
      keyExpr = this.isJsonArrayField(field) ? `arrayJoin(arrayDistinct(${baseCol}))` : baseCol;
    }

    // Split sub-aggs: metrics fold into the bucket GROUP BY, histograms run as
    // one grouped query for ALL buckets; anything else falls back to per-bucket.
    const subAggs = agg.aggregations || agg.aggs || {};
    const metricSubs = []; const histoSubs = []; const otherSubs = [];
    for (const [subName, subAgg] of Object.entries(subAggs)) {
      if (this.isMetricAgg(subAgg)) { metricSubs.push([subName, subAgg]); } else if (subAgg.histogram || subAgg.date_histogram) { histoSubs.push([subName, subAgg]); } else { otherSubs.push([subName, subAgg]); }
    }
    const metricSelect = metricSubs.map(([subName, subAgg]) => `${this.metricExpression(subAgg)} AS ${quoteIdent(subName)}`);

    const size = Number(terms.size || 10);
    const orderDir = String(terms.order?._count || 'desc').toUpperCase() === 'ASC' ? 'ASC' : 'DESC';
    // ES tie-breaks equal counts by key; for ip-typed fields the key order is
    // the IP's binary value, not its string form
    const tieExpr = (!terms.script && field && this.isIpField(field)) ? 'toIPv6OrNull(key)' : 'key';
    const select = [`${keyExpr} AS key`, 'count() AS doc_count', ...metricSelect].join(', ');
    const result = await this.query(`SELECT ${select} FROM ${this.tableName()} WHERE ${where} GROUP BY key WITH TOTALS ORDER BY doc_count ${orderDir}, ${tieExpr} ASC LIMIT ${size}`);
    const rows = result.data || [];
    const totalDocCount = Number(result.totals?.doc_count ?? 0);
    const buckets = rows
      .filter(row => row.key !== null && row.key !== undefined && row.key !== '')
      .map(row => {
        const bucket = { key: row.key, doc_count: Number(row.doc_count) };
        for (const [subName] of metricSubs) { bucket[subName] = { value: row[subName] ?? 0 }; }
        return bucket;
      });
    const returnedSum = buckets.reduce((s, b) => s + b.doc_count, 0);
    const sumOther = Math.max(0, totalDocCount - returnedSum);

    await Promise.all([
      ...histoSubs.map(([subName, subAgg]) =>
        this.runTermsHistogramSub(buckets, keyExpr, bucketWhereFn, field, subName, subAgg, where)),
      ...buckets.map(async (bucket) => {
        if (otherSubs.length === 0) { return; }
        const filter = bucketWhereFn ? bucketWhereFn(bucket.key) : this.predicateForFieldValue(field, '=', bucket.key);
        const bucketWhere = `(${where}) AND (${filter})`;
        for (const [subName, subAgg] of otherSubs) {
          bucket[subName] = await this.runAgg(subAgg, bucketWhere);
        }
      })
    ]);

    return { doc_count_error_upper_bound: 0, sum_other_doc_count: sumOther, buckets };
  }

  // Histogram sub-agg under a terms agg (e.g. SPIGraph): one grouped query for
  // all buckets instead of one query per bucket.
  async runTermsHistogramSub (buckets, keyExpr, bucketWhereFn, field, subName, subAgg, where) {
    for (const bucket of buckets) { bucket[subName] = { buckets: [] }; }
    if (buckets.length === 0) { return; }

    const histo = subAgg.histogram || subAgg.date_histogram;
    const intervalMs = histo.interval || this.intervalToMs(histo.fixed_interval || histo.calendar_interval || histo.interval);
    const intervalSeconds = Math.max(1, Math.floor(intervalMs / 1000));
    const hfield = histo.field === '@timestamp' ? 'timestamp' : histo.field;
    const slotExpr = `toUnixTimestamp(toStartOfInterval(${this.columnForField(hfield)}, INTERVAL ${intervalSeconds} second)) * 1000`;

    const keys = buckets.map(b => b.key);
    const keyFilter = bucketWhereFn
      ? '(' + keys.map(key => bucketWhereFn(key)).join(' OR ') + ')'
      : this.predicateForFieldValue(field, 'IN', keys);

    const subSubs = subAgg.aggregations || subAgg.aggs || {};
    const metricSelect = Object.entries(subSubs).map(([subSubName, a]) => `${this.metricExpression(a)} AS ${quoteIdent(subSubName)}`);
    const select = [`${keyExpr} AS key`, `${slotExpr} AS slot`, 'count() AS doc_count', ...metricSelect].join(', ');
    const rows = (await this.query(`SELECT ${select} FROM ${this.tableName()} WHERE (${where}) AND ${keyFilter} GROUP BY key, slot ORDER BY slot ASC`)).data || [];

    const byKey = new Map(buckets.map(b => [String(b.key), b]));
    for (const row of rows) {
      const bucket = byKey.get(String(row.key));
      if (!bucket) { continue; }
      const hb = { key: Number(row.slot), doc_count: Number(row.doc_count) };
      for (const subSubName of Object.keys(subSubs)) { hb[subSubName] = { value: row[subSubName] ?? 0 }; }
      bucket[subName].buckets.push(hb);
    }
  }

  async runHistogramAgg (agg, where) {
    const histo = agg.histogram || agg.date_histogram;
    const intervalMs = histo.interval || this.intervalToMs(histo.fixed_interval || histo.calendar_interval || histo.interval);
    const intervalSeconds = Math.max(1, Math.floor(intervalMs / 1000));
    const field = histo.field === '@timestamp' ? 'timestamp' : histo.field;
    if (histo.field === 'packetRange') {
      // ES packetRange is a date_range field (firstPacket..lastPacket); a
      // histogram over it counts the session in EVERY interval it spans.
      // Emit one bucket index per spanned interval via arrayJoin(range()).
      // Clamp the start so a missing firstPacket can't explode the range.
      const fp = `toUnixTimestamp64Milli(${quoteIdent(this.prefix + this.table)}.firstPacket)`;
      const lp = `toUnixTimestamp64Milli(${this.lastPacketCol()})`;
      const endSlot = `intDiv(${lp}, ${intervalMs})`;
      const startSlot = `greatest(least(intDiv(${fp}, ${intervalMs}), ${endSlot}), ${endSlot} - 2000000)`;
      const spanKeyExpr = `arrayJoin(range(toUInt64(${startSlot}), toUInt64(${endSlot} + 1))) * ${intervalMs}`;
      const subAggsSpan = agg.aggregations || agg.aggs || {};
      const metricSelectSpan = Object.entries(subAggsSpan).map(([subName, subAgg]) => `${this.metricExpression(subAgg)} AS ${quoteIdent(subName)}`);
      const spanSelect = [`${spanKeyExpr} AS key`, 'count() AS doc_count', ...metricSelectSpan].join(', ');
      const spanRows = (await this.query(`SELECT ${spanSelect} FROM ${this.tableName()} WHERE ${where} GROUP BY key ORDER BY key ASC`)).data || [];
      return {
        buckets: spanRows.map(row => {
          const bucket = { key: Number(row.key), doc_count: Number(row.doc_count) };
          for (const subName of Object.keys(subAggsSpan)) { bucket[subName] = { value: row[subName] ?? 0 }; }
          return bucket;
        })
      };
    }
    const keyExpr = `toStartOfInterval(${this.columnForField(field)}, INTERVAL ${intervalSeconds} second)`;
    const subAggs = agg.aggregations || agg.aggs || {};
    const metricSelect = [];
    for (const [subName, subAgg] of Object.entries(subAggs)) {
      metricSelect.push(`${this.metricExpression(subAgg)} AS ${quoteIdent(subName)}`);
    }
    const select = [`toUnixTimestamp(${keyExpr}) * 1000 AS key`, 'count() AS doc_count', ...metricSelect].join(', ');
    const rows = (await this.query(`SELECT ${select} FROM ${this.tableName()} WHERE ${where} GROUP BY key ORDER BY key ASC`)).data || [];
    return {
      buckets: rows.map(row => {
        const bucket = { key: Number(row.key), doc_count: Number(row.doc_count) };
        for (const subName of Object.keys(subAggs)) { bucket[subName] = { value: row[subName] ?? 0 }; }
        return bucket;
      })
    };
  }

  intervalToMs (interval) {
    if (typeof interval === 'number') { return interval; }
    const match = String(interval).match(/^(\d+)(ms|s|m|h|d|w)?$/);
    if (!match) { throw chError(`histogram interval ${interval}`); }
    const value = Number(match[1]);
    const unit = match[2] || 'ms';
    return value * ({ ms: 1, s: 1000, m: 60000, h: 3600000, d: 86400000, w: 604800000 }[unit]);
  }

  tableName () {
    return `${quoteIdent(this.database)}.${quoteIdent(this.prefix + this.table)}`;
  }

  // Table-qualified lastPacket: SELECT projections alias `lastPacket` to a
  // formatted string, which would otherwise shadow the raw column in WHERE
  // and ORDER BY of the same query.
  lastPacketCol () {
    return `${quoteIdent(this.prefix + this.table)}.${quoteIdent('lastPacket')}`;
  }

  // SQL clause (starting with " AND ...") constraining lastPacket to the
  // index's time range; '' when the range can't be determined. Mutations
  // (UPDATE/DELETE) have no projections, so they use the bare column name.
  timeClauseFromIndex (index, { qualify = true } = {}) {
    const env = timeEnvelopeFromIndex(index);
    if (!env) { return ''; }
    const col = qualify ? this.lastPacketCol() : quoteIdent('lastPacket');
    return ` AND ${col} >= fromUnixTimestamp64Milli(${env[0]}) AND ${col} < fromUnixTimestamp64Milli(${env[1]})`;
  }

  async getSession (id, options = {}) {
    const query = { query: { ids: { values: [id] } }, _source: options._source, fields: options.fields || ['*'], size: 1 };
    const result = await this.searchSessions(null, query, options);
    return result.hits.hits[0];
  }

  async searchSessions (index, query, options = {}) {
    const translated = this.translateQuery(query, { ...options, timeClause: this.timeClauseFromIndex(index) });
    const [rowsResult, countResult, aggregations] = await Promise.all([
      this.query(translated.sql),
      translated.size === 0 ? this.query(`SELECT count() AS count FROM ${this.tableName()} WHERE ${translated.where}`) : null,
      this.translateAggs(query.aggregations || query.aggs, translated.where)
    ]);
    const total = countResult
      ? Number(countResult.data?.[0]?.count ?? 0)
      : Number(rowsResult.rows_before_limit_at_least ?? rowsResult.data?.length ?? 0);
    const hits = this.rowsToHits(rowsResult.data || [], translated.projection);
    this.attachSort(hits, query.sort);
    return { hits: { total, hits }, aggregations };
  }

  // Populate `sort` field on each hit so callers (e.g. multies merging
  // results from multiple clusters) can re-sort. Mirrors the ES response
  // shape where each hit carries an array of sort key values matching the
  // request's `sort` spec.
  attachSort (hits, sort) {
    if (!Array.isArray(sort) || sort.length === 0) { return; }
    const fields = sort.map((s) => Object.keys(s)[0]);
    for (const hit of hits) {
      const arr = [];
      for (const f of fields) {
        let v;
        if (hit.fields && hit.fields[f] !== undefined) {
          v = Array.isArray(hit.fields[f]) ? hit.fields[f][0] : hit.fields[f];
        } else if (f === '_id') {
          v = hit._id;
        } else {
          // Walk dotted path against _source as last resort.
          let cur = hit._source;
          for (const p of f.split('.')) {
            if (cur == null) { cur = undefined; break; }
            cur = cur[p];
          }
          v = Array.isArray(cur) ? cur[0] : cur;
        }
        if (v instanceof Date) { v = v.getTime(); }
        arr.push(v ?? null);
      }
      hit.sort = arr;
    }
  }

  async * searchSessionsIterator (index, query, options = {}) {
    let remaining = Number(query.size ?? 10000);
    let searchAfter;
    const pageSize = Math.min(remaining || 2000, 2000);
    while (remaining > 0) {
      const q = { ...query, size: Math.min(pageSize, remaining), from: 0, sort: query.sort || [{ lastPacket: { order: 'desc' } }] };
      const translated = this.translateQuery(q, { ...options, keyset: true, searchAfter, timeClause: this.timeClauseFromIndex(index) });
      const rowsResult = await this.query(translated.sql);
      const rows = rowsResult.data || [];
      if (rows.length === 0) { break; }
      remaining -= rows.length;
      const total = searchAfter ? remaining + rows.length : Number(rowsResult.rows_before_limit_at_least ?? rows.length);
      yield { hits: { total, hits: this.rowsToHits(rows, translated.projection) } };
      const last = rows[rows.length - 1];
      searchAfter = { lastPacket: last.lastPacket, id: last.id };
      if (rows.length < pageSize) { break; }
    }
  }

  async msearchSessions (index, queries, options = {}) {
    const responses = await Promise.all(queries.map(query => {
      const q = typeof query === 'string' ? JSON.parse(query) : query;
      return this.searchSessions(index, q, options);
    }));
    return { body: { responses } };
  }

  tagsAddAssignment (tags) {
    const lit = `[${tags.map(tag => sqlLiteral(tag)).join(', ')}]`;
    const newExpr = `arrayDistinct(arrayConcat(CAST(fields.tags AS Array(String)), ${lit}))`;
    return `fields = CAST(JSONMergePatch(toJSONString(fields), concat('{"tags":', toJSONString(${newExpr}), ',"tagsCnt":', toString(length(${newExpr})), '}')) AS JSON)`;
  }

  tagsRemoveAssignment (tags) {
    const lit = `[${tags.map(tag => sqlLiteral(tag)).join(', ')}]`;
    const newExpr = `arrayFilter(x -> NOT has(${lit}, x), CAST(fields.tags AS Array(String)))`;
    return `fields = CAST(JSONMergePatch(toJSONString(fields), concat('{"tags":', toJSONString(${newExpr}), ',"tagsCnt":', toString(length(${newExpr})), '}')) AS JSON)`;
  }

  huntAddAssignment (huntId, huntName) {
    const newIds = `arrayConcat(CAST(fields.huntId AS Array(String)), [${sqlLiteral(huntId)}])`;
    const newNames = `arrayConcat(CAST(fields.huntName AS Array(String)), [${sqlLiteral(huntName)}])`;
    return `fields = CAST(JSONMergePatch(toJSONString(fields), concat('{"huntId":', toJSONString(${newIds}), ',"huntName":', toJSONString(${newNames}), '}')) AS JSON)`;
  }

  huntRemoveAssignment (huntId, huntName) {
    const newIds = `arrayFilter(x -> x != ${sqlLiteral(huntId)}, CAST(fields.huntId AS Array(String)))`;
    const newNames = `arrayFilter(x -> x != ${sqlLiteral(huntName)}, CAST(fields.huntName AS Array(String)))`;
    return `fields = CAST(JSONMergePatch(toJSONString(fields), concat('{"huntId":', toJSONString(${newIds}), ',"huntName":', toJSONString(${newNames}), '}')) AS JSON)`;
  }

  // Fallback for callers that hand us a raw ES update body. Prefer the named
  // methods below — viewer/db.js dispatches to them directly.
  updateAssignmentsFromDoc (doc) {
    if (doc.doc) {
      // Always merge into the JSON `fields` column.
      return [`fields = CAST(JSONMergePatch(toJSONString(fields), ${sqlLiteral(JSON.stringify(doc.doc))}) AS JSON)`];
    }
    if (!doc.script) { throw chError('updateSession body without doc or script'); }
    const source = doc.script.source || '';
    const params = doc.script.params || {};
    if (params.tags && source.includes('ctx._source.tags.add')) {
      return [this.tagsAddAssignment(params.tags)];
    }
    if (params.tags && source.includes('ctx._source.tags.remove')) {
      return [this.tagsRemoveAssignment(params.tags)];
    }
    if (params.huntId && source.includes('huntId.add')) {
      return [this.huntAddAssignment(params.huntId, params.huntName)];
    }
    if (params.huntId && source.includes('huntId.remove')) {
      return [this.huntRemoveAssignment(params.huntId, params.huntName)];
    }
    throw chError('painless update script');
  }

  async applyAssignments (id, assignments, index) {
    // Constrain by the index's time range so the update only visits a few
    // partitions — a bare `_id = x` predicate would touch every part.
    const where = `\`_id\` = ${sqlLiteral(id)}${this.timeClauseFromIndex(index, { qualify: false })}`;

    // Prefer a lightweight UPDATE (patch parts, CH 25.7+): no part rewrite,
    // synchronous, immediately visible to the re-search the viewer does after
    // tagging. Fall back to a heavyweight mutation on servers/tables without
    // patch part support and remember the answer.
    if (this.lightweightUpdates !== false) {
      try {
        await this.query(`UPDATE ${this.tableName()} SET ${assignments.join(', ')} WHERE ${where} SETTINGS enable_lightweight_update = 1`, { text: true });
        this.lightweightUpdates = true;
        return { result: 'updated' };
      } catch (err) {
        this.lightweightUpdates = false;
        console.log('CH backend: lightweight UPDATE unavailable, using ALTER TABLE UPDATE mutations:', err.message.split('\n')[0]);
      }
    }

    // mutations_sync so the change is visible to the immediate re-search the
    // viewer does after tagging (matches ES update-then-refresh semantics).
    await this.query(`ALTER TABLE ${this.tableName()} UPDATE ${assignments.join(', ')} WHERE ${where}`, { text: true, mutationsSync: true });
    return { result: 'updated' };
  }

  async updateSession (index, id, doc) {
    return this.applyAssignments(id, this.updateAssignmentsFromDoc(doc), index);
  }

  async addTagsToSession (index, id, tags) {
    return this.applyAssignments(id, [this.tagsAddAssignment(tags)], index);
  }

  async removeTagsFromSession (index, id, tags) {
    return this.applyAssignments(id, [this.tagsRemoveAssignment(tags)], index);
  }

  async addHuntToSession (index, id, huntId, huntName) {
    return this.applyAssignments(id, [this.huntAddAssignment(huntId, huntName)], index);
  }

  async removeHuntFromSession (index, id, huntId, huntName) {
    return this.applyAssignments(id, [this.huntRemoveAssignment(huntId, huntName)], index);
  }

  async deleteDocument (index, id, options) {
    const where = `\`_id\` = ${sqlLiteral(id)}${this.timeClauseFromIndex(index, { qualify: false })}`;
    try {
      // Lightweight delete: row mask instead of part rewrite.
      await this.query(`DELETE FROM ${this.tableName()} WHERE ${where}`, { text: true });
    } catch (err) {
      await this.query(`ALTER TABLE ${this.tableName()} DELETE WHERE ${where}`, { text: true, mutationsSync: true });
    }
    return { result: 'deleted' };
  }

  // Seam adapter: ClickHouse has no ES-style scroll. When scroll is
  // requested, fetch up to a large cap in one shot and return a sentinel
  // _scroll_id that Db.scroll/Db.clearScroll resolve to empty.
  async searchScroll (index, query, options, cb) {
    try {
      const chQuery = options?.scroll ? { ...query, size: 1000000 } : query;
      const result = await this.searchSessions(index, chQuery, options);
      if (options?.scroll) { result._scroll_id = 'ch:done'; }
      if (cb) { return cb(null, result); }
      return result;
    } catch (err) {
      if (cb) { return cb(err); }
      throw err;
    }
  }

  async * searchScrollIterator (index, query, options) {
    yield * this.searchSessionsIterator(index, query, options);
  }

  // ES refreshes the index and retries a not-found get; CH inserts are
  // visible immediately, so a retry never helps.
  async refreshForNotFoundRetry () {
    return false;
  }

  getSessionIndices () {
    return [`${this.prefix}${this.table}`];
  }

  // Index names are only time hints for CH; the bare table name carries no
  // date suffix so searchSessions applies no index-based time pruning.
  async getIndices () {
    return `${this.prefix}${this.table}`;
  }

  async getMinValue (index, field) {
    const result = await this.searchSessions(index, { size: 0, aggs: { min: { min: { field } } } });
    return { body: result };
  }

  async numberOfDocuments () {
    const result = await this.query(`SELECT count() AS count FROM ${this.tableName()}`);
    return { count: Number(result.data?.[0]?.count ?? 0) };
  }

  close () {
    this.agent.destroy();
  }

  decodeRawValue (value) {
    if (value === undefined || value === null || value === '') { return undefined; }
    try { return JSON.parse(value); } catch (e) { return value; }
  }

  flattenJson (obj, prefix, out) {
    for (const key in obj) {
      const v = obj[key];
      if (v === null || v === undefined) { continue; }
      const path = prefix ? prefix + '.' + key : key;
      if (Array.isArray(v)) {
        if (v.length === 0) { continue; }
        if (typeof v[0] === 'object' && v[0] !== null && !Array.isArray(v[0])) {
          // Array of objects: keep as-is (rare)
          out[path] = v;
        } else {
          out[path] = v;
        }
      } else if (typeof v === 'object') {
        this.flattenJson(v, path, out);
      } else {
        out[path] = [v];
      }
    }
  }

  rowsToHits (rows, projection) {
    return rows.map((row) => {
      const fields = {};
      let source;
      if (row.__fields_json__) {
        try {
          source = typeof row.__fields_json__ === 'string'
            ? JSON.parse(row.__fields_json__)
            : row.__fields_json__;
          this.flattenJson(source, '', fields);
          delete fields._id;
        } catch (e) { /* ignore */ }
      }
      if (!source) { source = {}; }
      for (const field of projection) {
        if (field === 'id') { continue; }
        let value = row[field];
        if (value === undefined || value === null) { continue; }
        fields[field] = Array.isArray(value) ? value : [value];
        const parts = field.split('.');
        let obj = source;
        for (let i = 0; i < parts.length - 1; i++) {
          if (obj[parts[i]] === undefined || obj[parts[i]] === null || typeof obj[parts[i]] !== 'object') {
            obj[parts[i]] = {};
          }
          obj = obj[parts[i]];
        }
        if (obj[parts[parts.length - 1]] === undefined) {
          obj[parts[parts.length - 1]] = value;
        }
      }
      const day = row._dayIndex || '000000';
      const hit = { _id: row.id, _index: `${this.index}-${day}`, fields, _source: source };
      return hit;
    });
  }
}

module.exports = DbCHImpl;
