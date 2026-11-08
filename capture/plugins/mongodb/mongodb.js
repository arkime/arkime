/* mongodb.js  -- MongoDB collections for ArkimeDb, a dbPlugins module for mongodb:// and mongodb+srv:// urls
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const net = require('net');

let ArkimeDb, ArkimeConfig, ArkimeUtil;

// Each doc is { _id, d: the doc, v: its version }, with _ip holding the ips in it for $cidr
const NON_NULL = ['double', 'string', 'object', 'binData', 'objectId', 'bool', 'date', 'int', 'long', 'decimal', 'timestamp', 'regex'];
const NUMERIC = new Set(['long', 'date', 'integer', 'short', 'byte', 'double', 'float', 'half_float', 'scaled_float', 'unsigned_long']);
const FALSE = { _id: { $exists: false } };
const DUPLICATE_KEY = 11000;
const PATTERN_SLOTS = 2;
const PATTERN_QUEUE = 100;

function escapeRegex (s) {
  return String(s).replace(/[\\^$.*+?()[\]{}|/-]/g, '\\$&');
}

// A * and ? pattern to a regex that runs in linear time: each literal piece after a * is found at its
// first match inside an atomic group, which is always enough for a glob, so nothing backtracks
function wildcardRegex (c) {
  const piece = (s) => [...s].map(ch => ch === '?' ? '.' : escapeRegex(ch)).join('');
  const parts = String(c).split('*');
  if (parts.length === 1) { return `^${piece(parts[0])}$`; }
  const last = parts.pop();
  let re = `^${piece(parts.shift())}`;
  for (const p of parts) { if (p) { re += `(?>.*?${piece(p)})`; } }
  return `${re}.*${piece(last)}$`;
}

// RE2 checks the syntax, which refuses backreferences and lookarounds. PCRE then runs it, so
// (?flags) and named groups, which the two read differently, are refused too.
function regexPattern (c) {
  const pattern = String(c);
  ArkimeDb.regex(pattern);
  for (let i = 0; i < pattern.length; i++) {
    if (pattern[i] === '\\') { i++; continue; }
    if (pattern[i] === '(' && pattern[i + 1] === '?' && pattern[i + 2] !== ':') {
      throw new Error(`Invalid db where $regex '${pattern}', (?flags) and named groups aren't supported`);
    }
  }
  return `^(?:${pattern})\\z`;
}

// Always 16 bytes, an ipv4 as ::ffff:a.b.c.d, so ipv4 and ipv6 compare in the order ES uses
function ipBytes (ip) {
  if (typeof ip !== 'string') { return null; }
  ip = ip.replace(/%.*$/, '');
  const family = net.isIP(ip);
  if (family === 4) { return Buffer.from([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, ...ip.split('.').map(Number)]); }
  if (family !== 6) { return null; }
  const bytes = Buffer.alloc(16);
  const groups = (s) => {
    const out = [];
    for (const g of s ? s.split(':') : []) {
      if (g.includes('.')) {
        const v4 = ipBytes(g);
        out.push(v4.readUInt16BE(12), v4.readUInt16BE(14));
      } else {
        out.push(parseInt(g, 16));
      }
    }
    return out;
  };
  const [head, tail] = ip.split('::');
  const h = groups(head);
  const t = tail === undefined ? [] : groups(tail);
  [...h, ...Array(8 - h.length - t.length).fill(0), ...t].forEach((g, i) => bytes.writeUInt16BE(g, i * 2));
  return bytes;
}

function cidrRange (cidr) {
  const [ip, bits] = String(cidr).split('/');
  const start = ipBytes(ip);
  const end = Buffer.from(start);
  const keep = bits === undefined ? 128 : Number(bits) + (net.isIPv4(ip) ? 96 : 0);
  for (let i = 0; i < start.length; i++) {
    const block = 2 ** (8 - Math.max(0, Math.min(8, keep - i * 8)));
    start[i] -= start[i] % block;
    end[i] = start[i] + block - 1;
  }
  return { start, end };
}

// Every ip string in a doc and its dotted path, array positions left out like ES flattens them
function ipsOf (doc, path = '', out = []) {
  if (Array.isArray(doc)) {
    for (const v of doc) { ipsOf(v, path, out); }
  } else if (doc !== null && typeof doc === 'object') {
    for (const [k, v] of Object.entries(doc)) { ipsOf(v, path ? `${path}.${k}` : k, out); }
  } else {
    const b = ipBytes(doc);
    if (b) { out.push({ f: path, a: b }); }
  }
  return out;
}

// The stored form, a lone surrogate isn't valid UTF-8 so BSON would write it as U+FFFD anyway
function stored (id, doc, v) {
  const row = { _id: id, d: doc, v };
  const ips = ipsOf(doc);
  if (ipBytes(id)) { ips.push({ f: 'id', a: ipBytes(id) }); }
  if (ips.length) { row._ip = ips; }
  return row;
}

function kindOf (type) {
  if (NUMERIC.has(type)) { return 'number'; }
  if (type === 'boolean') { return 'boolean'; }
  return 'string';
}

function count (v) {
  const n = Math.trunc(Number(v));
  return Number.isFinite(n) && n > 0 ? n : 0;
}

// One field condition as a MongoDB filter. Dotted paths and arrays match any value like ES, so
// MongoDB's own matching does almost all of it.
function fieldFilter (field, op, c, q) {
  const path = field === 'id' ? '_id' : `d.${field}`;
  switch (op) {
  case '$eq': return c === null ? FALSE : { [path]: c };
  case '$ne': return c === null ? {} : { [path]: { $ne: c } };
  case '$in': { const vals = c.filter(v => v !== null); return vals.length ? { [path]: { $in: vals } } : FALSE; }
  case '$nin': return { [path]: { $nin: c.filter(v => v !== null) } };
  case '$exists':
    if (field === 'id') { return c ? {} : FALSE; }
    return c ? { [path]: { $type: NON_NULL } } : { [path]: { $not: { $type: NON_NULL } } };
  case '$gt': case '$gte': case '$lt': case '$lte':
    // MongoDB only compares a number with numbers and a string with strings, by their bytes
    return { [path]: { [op]: typeof c === 'number' ? c : String(c) } };
  case '$contains': return { [path]: { $regex: escapeRegex(c) } };
  case '$icontains': return { [path]: { $regex: escapeRegex(c), $options: 'i' } };
  case '$prefix': return { [path]: { $regex: `^${escapeRegex(c)}` } };
  case '$wildcard': q.patterns = true; return { [path]: { $regex: wildcardRegex(c), $options: 's' } };
  case '$regex': q.patterns = true; return { [path]: { $regex: regexPattern(c), $options: 's' } };
  case '$cidr': {
    const { start, end } = cidrRange(c);
    return { _ip: { $elemMatch: { f: field, a: { $gte: start, $lte: end } } } };
  }
  }
}

function compile (where, q, op) {
  if (!where) { return {}; }
  const parts = [];
  for (const [key, cond] of Object.entries(where)) {
    if (key === '$or') {
      parts.push(cond.length ? { $or: cond.map(w => compile(w, q, op)) } : FALSE);
    } else if (key === '$and') {
      if (cond.length) { parts.push({ $and: cond.map(w => compile(w, q, op)) }); }
    } else if (key === '$not') {
      parts.push({ $nor: [compile(cond, q, op)] });
    } else {
      ArkimeDb.checkField(key);
      for (const [o, c] of ArkimeDb.condOps(cond)) { parts.push(op(key, o, c, q)); }
    }
  }
  return parts.length === 0 ? {} : parts.length === 1 ? parts[0] : { $and: parts };
}

// A value flattened one array level at each step of a dotted path, the values ES would index
function flatValues (field) {
  let expr = '$d';
  for (const part of field.split('.')) {
    expr = { $getField: { field: part, input: expr } };
    expr = flatten({ $map: { input: arrayOf(expr), as: 'x', in: '$$x' } });
  }
  return { $filter: { input: expr, as: 'x', cond: { $ne: ['$$x', null] } } };

  function arrayOf (e) { return { $cond: [{ $isArray: e }, e, { $cond: [{ $eq: [{ $type: e }, 'missing'] }, [], [e]] }] }; }
  function flatten (e) {
    return { $reduce: { input: e, initialValue: [], in: { $concatArrays: ['$$value', { $cond: [{ $isArray: '$$this' }, '$$this', ['$$this']] }] } } };
  }
}

function defineCollections () {
  class MongoCollection extends ArkimeDb.Collection {
    conn;
    table;
    #ready;
    op = (field, o, c, q) => this.fieldOp(field, o, c, q);

    constructor (conn, collName, options) {
      super(collName, options);
      this.conn = conn;
      this.table = ArkimeDb.checkField(options.mongodb?.collection ?? `${conn.prefix}${options.table ?? collName}`);
    }

    get coll () { return this.conn.db.collection(this.table); }
    get meta () { return this.conn.db.collection(`${this.conn.prefix}arkime_meta`); }

    fieldOp (field, o, c, q) { return fieldFilter(field, o, c, q); }

    init () {
      this.#ready ??= (async () => {
        await this.conn.checkVersion();
        const sorted = new Set(ArkimeDb.sortIndexedFields(this.options));
        const indexes = [];
        for (const field of ArkimeDb.indexedFields(this.options)) {
          indexes.push({ key: { [`d.${field}`]: 1 }, name: ArkimeDb.fieldColumn(field) });
          if (sorted.has(field)) { indexes.push({ key: { [`_n.${field}`]: 1, [`d.${field}`]: 1 }, name: `${ArkimeDb.fieldColumn(field)}_nl` }); }
        }
        if (ArkimeDb.fieldTypes(this.options).some(([, t]) => t === 'ip')) { indexes.push({ key: { '_ip.f': 1, '_ip.a': 1 }, name: 'ips' }); }
        if (indexes.length) { await this.coll.createIndexes(indexes); }
      })().catch((err) => {
        this.#ready = undefined;
        throw err;
      });
      return this.#ready;
    }

    async #run (q, fn) {
      await this.init();
      return q.patterns ? this.conn.patternRun(fn) : fn({});
    }

    // Writes keep _n.<field> set for the sorted fields that are missing, so an ascending sort puts them last off the index
    #row (id, doc, v) {
      const row = stored(id, this.prepare(doc), v);
      const missing = ArkimeDb.sortIndexedFields(this.options).filter(f => !ArkimeDb.pathValues(row.d, f).length);
      if (missing.length) { row._n = Object.fromEntries(missing.map(f => [f, 1])); }
      return row;
    }

    prepare (doc) { return doc; }

    async get (id) {
      await this.init();
      const row = await this.coll.findOne({ _id: id }, { projection: { d: 1 } });
      return row ? row.d : null;
    }

    async getVersioned (id) {
      await this.init();
      const row = await this.coll.findOne({ _id: id }, { projection: { d: 1, v: 1 } });
      return row ? { doc: row.d, version: row.v } : null;
    }

    async put (id, doc, { createOnly } = {}) {
      await this.init();
      id ??= this.newId();
      const row = this.#row(id, doc, 1);
      if (createOnly) {
        try {
          await this.coll.insertOne(row);
        } catch (err) {
          if (err.code === DUPLICATE_KEY) { throw new ArkimeDb.ConflictError(id); }
          throw err;
        }
        return id;
      }
      // $literal, a doc's own $ strings aren't expressions
      const set = { d: { $literal: row.d }, v: { $add: [{ $ifNull: ['$v', 0] }, 1] }, _ip: { $literal: row._ip ?? [] }, _n: { $literal: row._n ?? {} } };
      await this.conn.retry(() => this.coll.updateOne({ _id: id }, [{ $set: set }], { upsert: true }));
      return id;
    }

    async putIfVersion (id, doc, version) {
      await this.init();
      const row = this.#row(id, doc, version + 1);
      const { matchedCount } = await this.coll.updateOne({ _id: id, v: version }, { $set: { d: row.d, v: version + 1, _ip: row._ip ?? [], _n: row._n ?? {} } });
      if (matchedCount !== 1) { throw new ArkimeDb.ConflictError(id, `Document '${id}' was changed or deleted`); }
    }

    // Top level fields replace, $setField takes a field name with a . or $ as it is
    async update (id, fields) {
      await this.init();
      fields = this.prepare(fields, true);
      let d = '$d';
      for (const [k, v] of Object.entries(fields)) { d = { $setField: { field: { $literal: k }, input: d, value: { $literal: v } } }; }
      const { matchedCount } = await this.coll.updateOne({ _id: id }, [{ $set: { d, v: { $add: ['$v', 1] } } }]);
      if (matchedCount === 1) { await this.#reindex(id); }
      return matchedCount === 1;
    }

    // An update can change the ips or the missing sorted fields, so those are worked out again
    async #reindex (id) {
      const row = await this.coll.findOne({ _id: id }, { projection: { d: 1, v: 1 } });
      if (!row) { return; }
      const again = this.#row(id, row.d, row.v);
      await this.coll.updateOne({ _id: id, v: row.v }, { $set: { _ip: again._ip ?? [], _n: again._n ?? {} } });
    }

    async delete (id) {
      await this.init();
      return (await this.coll.deleteOne({ _id: id })).deletedCount > 0;
    }

    async deleteWhere (where) {
      await this.init();
      const q = {};
      const filter = compile(where, q, this.op);
      return (await this.#run(q, (opts) => this.coll.deleteMany(filter, opts))).deletedCount;
    }

    async count (where) {
      await this.init();
      const q = {};
      const filter = compile(where, q, this.op);
      return this.#run(q, (opts) => this.coll.countDocuments(filter, opts));
    }

    async find ({ where, sort, from = 0, size, version } = {}) {
      await this.init();
      const q = {};
      const filter = compile(where, q, this.op);
      const total = await this.#run(q, (opts) => this.coll.countDocuments(filter, opts));
      if (size === 0) { return { total, data: [] }; }
      from = count(from);
      const limit = size === undefined || size === null ? 0 : count(size);

      const keys = ArkimeDb.sortList(sort);
      let rows;
      if (keys.length === 0) {
        rows = await this.#run(q, (opts) => this.coll.find(filter, { ...opts, projection: { d: 1, v: 1 }, skip: from, limit }).toArray());
      } else if (keys.length === 1 && (keys[0][0] === 'id' || ArkimeDb.sortIndexedFields(this.options).includes(keys[0][0]))) {
        // Off an index: missing sorts lowest, so descending reads f_<field> backwards and ascending
        // reads f_<field>_nl, where _n.<field> marks the missing ones to go last
        const [f, dir] = keys[0];
        const s = f === 'id' ? { _id: dir === 'desc' ? -1 : 1 } : dir === 'desc' ? { [`d.${f}`]: -1 } : { [`_n.${f}`]: 1, [`d.${f}`]: 1 };
        rows = await this.#run(q, (opts) => this.coll.find(filter, { ...opts, projection: { d: 1, v: 1 }, sort: s, skip: from, limit }).toArray());
      } else {
        // Missing values last, then numbers by value and strings by their bytes, like the other backends
        const add = {};
        const s = {};
        keys.forEach(([f, dir], i) => {
          if (f === 'id') { s._id = dir === 'desc' ? -1 : 1; return; }
          const v = { $getField: { field: f.split('.').pop(), input: f.includes('.') ? `$d.${f.split('.').slice(0, -1).join('.')}` : '$d' } };
          add[`_m${i}`] = { $cond: [{ $in: [{ $type: v }, ['missing', 'null']] }, 1, 0] };
          add[`_s${i}`] = v;
          s[`_m${i}`] = 1;
          s[`_s${i}`] = dir === 'desc' ? -1 : 1;
        });
        const pipeline = [{ $match: filter }, { $addFields: add }, { $sort: s }, { $skip: from }];
        if (limit) { pipeline.push({ $limit: limit }); }
        pipeline.push({ $project: { d: 1, v: 1 } });
        rows = await this.#run(q, (opts) => this.coll.aggregate(pipeline, opts).toArray());
      }

      return {
        total,
        data: rows.map(r => {
          const hit = { id: r._id, doc: r.d };
          if (version && this.options.version) { hit.version = r.v; }
          return hit;
        })
      };
    }

    async aggregate ({ where, groupBy, metrics = {}, size = 1000 } = {}) {
      ArkimeDb.checkMetrics(metrics);
      ArkimeDb.checkField(groupBy);
      await this.init();
      const q = {};
      const filter = compile(where, q, this.op);
      const pipeline = [{ $match: filter }];
      let key = '$_id';
      if (groupBy !== 'id') {
        pipeline.push({ $addFields: { _k: flatValues(groupBy) } }, { $unwind: '$_k' });
        key = '$_k';
      }
      const group = { _id: key, count: { $sum: 1 } };
      for (const [metricName, metric] of Object.entries(metrics)) {
        const [op, field] = Object.entries(metric)[0];
        const v = `$d.${ArkimeDb.checkField(field)}`;
        group[`m_${ArkimeDb.checkField(metricName)}`] = { [`$${op}`]: { $cond: [{ $isNumber: v }, v, op === 'sum' ? 0 : null] } };
      }
      pipeline.push({ $group: group }, { $sort: { count: -1, _id: 1 } }, { $limit: count(size) || 1 });
      const rows = await this.#run(q, (opts) => this.coll.aggregate(pipeline, opts).toArray());
      return rows.map(r => {
        const bucket = { key: r._id, count: r.count };
        for (const metricName of Object.keys(metrics)) { bucket[metricName] = r[`m_${metricName}`] ?? null; }
        return bucket;
      });
    }

    async nextSequence (id) {
      await this.init();
      const row = await this.conn.retry(() => this.coll.findOneAndUpdate({ _id: id }, { $inc: { 'd.seq': 1, v: 1 } }, { upsert: true, returnDocument: 'after' }));
      return row.d.seq;
    }

    // Taken when it is new, ours or stale, a fresh one held by someone else fails the upsert's insert
    async lease (id, owner, ttlMs) {
      await this.init();
      const now = Date.now();
      try {
        await this.coll.updateOne({ _id: id, $or: [{ 'd.name': owner }, { 'd.lastRun': { $lte: now - ttlMs } }] },
          { $set: { 'd.name': owner, 'd.lastRun': now }, $setOnInsert: { 'd.enabled': false }, $inc: { v: 1 } }, { upsert: true });
        return true;
      } catch (err) {
        if (err.code === DUPLICATE_KEY) { return false; }
        throw err;
      }
    }

    async getMeta (key) {
      await this.init();
      return (await this.meta.findOne({ _id: `${this.table}:${key}` }))?.value;
    }

    async setMeta (obj) {
      await this.init();
      for (const [key, value] of Object.entries(obj)) {
        await this.meta.updateOne({ _id: `${this.table}:${key}` }, { $set: { value } }, { upsert: true });
      }
    }
  }

  // MongoDB has no columns, a typed collection is a json one that only takes its fields, stores each as
  // its type and matches a query value only against a field it could be stored in
  class MongoTypedCollection extends MongoCollection {
    #kinds;

    constructor (conn, collName, options) {
      super(conn, collName, options);
      this.#kinds = new Map(ArkimeDb.fieldTypes(options).map(([field, type]) => {
        if (field.includes('.')) { throw new Error(`typed ${this.table} field '${field}' can't be nested`); }
        return [field, kindOf(type)];
      }));
    }

    #cast (kind, v) {
      if (v === null || v === undefined) { return undefined; }
      if (kind === 'number') { return typeof v === 'number' ? v : Number(v); }
      if (kind === 'boolean') { return !!v; }
      return String(v);
    }

    #fits (kind, v) {
      if (kind === 'number') { return typeof v === 'number' || (typeof v === 'string' && v.trim() !== '' && Number.isFinite(Number(v))); }
      if (kind === 'string') { return typeof v === 'string' || typeof v === 'number'; }
      return typeof v === 'boolean';
    }

    prepare (doc, partial) {
      const out = {};
      for (const [field, v] of Object.entries(doc)) {
        const kind = this.#kinds.get(field);
        if (!kind) { throw new Error(`${this.table} has no column for '${field}'`); }
        const c = this.#cast(kind, v);
        if (c !== undefined) { out[field] = c; } else if (partial) { out[field] = null; }
      }
      return out;
    }

    async get (id) {
      const doc = await super.get(id);
      return doc && Object.fromEntries(Object.entries(doc).filter(([, v]) => v !== null));
    }

    fieldOp (field, op, c, q) {
      if (field === 'id') { return fieldFilter(field, op, c, q); }
      const kind = this.#kinds.get(field);
      if (op === '$exists') { return kind ? fieldFilter(field, op, c, q) : c ? FALSE : {}; }
      if (!kind) { return ['$ne', '$nin'].includes(op) ? {} : FALSE; }
      if (['$eq', '$ne'].includes(op)) {
        if (!this.#fits(kind, c)) { return op === '$eq' ? FALSE : {}; }
        return fieldFilter(field, op, this.#cast(kind, c), q);
      }
      if (['$in', '$nin'].includes(op)) { return fieldFilter(field, op, c.filter(v => this.#fits(kind, v)).map(v => this.#cast(kind, v)), q); }
      if (['$gt', '$gte', '$lt', '$lte'].includes(op)) {
        if ((kind === 'number' && typeof c === 'number') || (kind === 'string' && typeof c === 'string')) { return fieldFilter(field, op, c, q); }
        return FALSE;
      }
      return kind === 'string' ? fieldFilter(field, op, c, q) : FALSE;
    }

    async nextSequence () {
      throw new Error(`nextSequence isn't supported on typed ${this.table}`);
    }

    async lease () {
      throw new Error(`lease isn't supported on typed ${this.table}`);
    }
  }

  return { MongoCollection, MongoTypedCollection };
}

// Like the OpenSearch/Elasticsearch connections, a remote server's certificate is verified with caTrustFile
// or the system CAs, and --insecure still requires TLS. tls settings in the url win, loopback and unix
// socket servers are left alone.
function tlsOptions (url, options) {
  if (/[?&](tls|ssl)[A-Za-z]*=/i.test(url)) { return {}; }
  const hosts = MongoConnection.hosts(url);
  if (!url.startsWith('mongodb+srv:') && hosts.every(h => ArkimeConfig.isLoopbackHost(h))) { return {}; }
  const insecure = options.insecure ?? ArkimeConfig.insecure;
  const caTrustFile = options.caTrustFile ?? ArkimeConfig.get('caTrustFile');
  ArkimeUtil.tlsOptionsOrExit({ insecure, caTrustFile }, 'MongoDB');
  return { tls: true, ...(insecure ? { tlsAllowInvalidCertificates: true, tlsAllowInvalidHostnames: true } : {}), ...(caTrustFile ? { tlsCAFile: caTrustFile } : {}) };
}

class MongoConnection {
  client;
  db;
  prefix;
  #closed = false;
  #collections = new Map();
  #slots = { free: PATTERN_SLOTS, waiting: [] };
  #version;
  static #classes;
  // The oldest MongoDB this is written for
  static MIN_VERSION = [9, 0];

  /**
   * mongodb://user:password@host[:port][,host2...]/database[?options] or mongodb+srv://
   */
  constructor (url, options) {
    this.prefix = options.prefix ?? '';
    if (options.client) {
      this.client = options.client;
      this.db = options.db;
      this.#slots = options.slots;
      return;
    }
    const database = MongoConnection.database(url);
    if (!database) { throw new Error('A mongodb:// url needs a database, mongodb://user:pass@host/database'); }
    const { MongoClient } = require('mongodb');
    this.client = new MongoClient(url, { ...tlsOptions(url, options), retryWrites: true, appName: 'arkime' });
    this.db = this.client.db(database);
  }

  static database (url) {
    const m = url.match(/^mongodb(?:\+srv)?:\/\/[^/]*\/([^?]*)/i);
    return m ? decodeURIComponent(m[1]) : '';
  }

  static hosts (url) {
    const m = url.match(/^mongodb(?:\+srv)?:\/\/(?:[^@/]*@)?([^/?]*)/i);
    return (m?.[1] ?? '').split(',').map(h => {
      if (h.startsWith('[')) { return h.slice(1, h.indexOf(']')); }
      if (h.includes('%2F') || h.endsWith('.sock')) { return '/'; }
      return h.split(':')[0];
    });
  }

  static tlsHost (url) {
    return url.startsWith('mongodb+srv:') ? MongoConnection.hosts(url)[0] : MongoConnection.hosts(url).find(h => !ArkimeConfig.isLoopbackHost(h)) ?? MongoConnection.hosts(url)[0];
  }

  checkVersion () {
    this.#version ??= this.db.command({ buildInfo: 1 }).then(({ version }) => {
      const [major, minor] = version.split('.').map(Number);
      const [wantMajor, wantMinor] = MongoConnection.MIN_VERSION;
      if (major < wantMajor || (major === wantMajor && minor < wantMinor)) {
        throw new Error(`MongoDB ${version} is too old, mongodb:// needs ${wantMajor}.${wantMinor} or newer`);
      }
    }).catch((err) => {
      this.#version = undefined;
      throw err;
    });
    return this.#version;
  }

  // Two upserts of a new id can both try the insert, the loser's duplicate key is retried as an update
  async retry (fn) {
    for (let tries = 0; ; tries++) {
      try {
        return await fn();
      } catch (err) {
        if (err.code !== DUPLICATE_KEY || tries >= 3) { throw err; }
      }
    }
  }

  // A user's pattern can run long, so each runs with mongodbPatternTimeout and only a couple at a time
  async patternRun (fn) {
    const slots = this.#slots;
    if (slots.free > 0) {
      slots.free--;
    } else {
      if (slots.waiting.length >= PATTERN_QUEUE) { throw new Error('Too many $regex/$wildcard searches running, try again'); }
      await new Promise(resolve => slots.waiting.push(resolve));
    }
    try {
      const timeout = Math.max(0, parseInt(ArkimeConfig.get('mongodbPatternTimeout', 60), 10) || 0) * 1000;
      return await fn(timeout ? { maxTimeMS: timeout } : {});
    } finally {
      const next = slots.waiting.shift();
      if (next) { next(); } else { slots.free++; }
    }
  }

  withPrefix (prefix) {
    return prefix === this.prefix ? this : new MongoConnection(undefined, { client: this.client, db: this.db, slots: this.#slots, prefix });
  }

  collection (collName, options = {}) {
    let coll = this.#collections.get(collName);
    if (!coll) {
      const { MongoCollection, MongoTypedCollection } = MongoConnection.#classes ??= defineCollections();
      coll = new (options.typed ? MongoTypedCollection : MongoCollection)(this, collName, options);
      this.#collections.set(collName, coll);
    }
    return coll;
  }

  async close () {
    if (this.#closed) { return; }
    this.#closed = true;
    await this.client.close();
  }
}

exports.registerDb = (api) => {
  ({ ArkimeDb, ArkimeConfig, ArkimeUtil } = api);
  const MongoSessions = require('./mongodb.sessions.js').defineSessions({
    ArkimeDb, ArkimeConfig, MongoConnection, helpers: { escapeRegex, wildcardRegex, regexPattern, ipBytes, cidrRange, ipsOf }
  });
  for (const scheme of ['mongodb', 'mongodb+srv']) {
    api.registerDbScheme(scheme, MongoConnection, { cluster: true });
    api.registerSessionsScheme(scheme, MongoSessions);
  }
};
exports.MongoConnection = MongoConnection;

// A session as capture/plugins/mongodb/mongodb.c stores it, for tools and tests that write one
exports.sessionDoc = (id, doc) => ({ ...doc, _id: id, _ip: ipsOf(doc) });
