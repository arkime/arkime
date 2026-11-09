/* mongodb.sessions.js  -- MongoDB sessions db for the viewer and multies, registered by mongodb.js
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const cryptoLib = require('crypto');
const { AsyncLocalStorage } = require('async_hooks');

// capture/plugins/mongodb/mongodb.c writes each session into a collection named after the ES index it would
// be in, <prefix>sessions3-<period>, with the session's fields at the top level and _ip holding its ips.
// The viewer's ES queries are parsed by viewer/esQuery.js and compiled here into MongoDB filters and pipelines.

const NON_NULL = ['double', 'string', 'object', 'binData', 'objectId', 'bool', 'date', 'int', 'long', 'decimal', 'timestamp', 'regex'];
const FALSE = { _id: { $exists: false } };
const DEFAULT_FIELDS = [
  'node', 'firstPacket', 'lastPacket', 'ipProtocol', 'source.ip', 'source.port', 'source.bytes', 'source.packets',
  'destination.ip', 'destination.port', 'destination.bytes', 'destination.packets', 'network.bytes', 'network.packets',
  'totDataBytes', 'protocol', 'tags', 'fileId', 'packetPos', 'packetLen', 'rootId'
];
const DATE_FIELDS = new Set(['timestamp', '@timestamp', 'firstPacket', 'lastPacket']);
const NUMBER_TYPES = new Set(['integer', 'seconds', 'float', 'date']);
const COLLECTIONS_CACHE_MS = 1000;
// The indexes capture/plugins/mongodb/mongodb.c makes on each sessions collection
const SESSION_INDEXES = [
  { key: { '$**': 1 }, name: 'all', wildcardProjection: { _id: 0 } },
  { key: { lastPacket: 1, _id: 1 }, name: 'lastPacketId' },
  { key: { firstPacket: 1, _id: 1 }, name: 'firstPacketId' }
];
const BUCKET_CONCURRENCY = 8;
// Like ES's search.max_open_scroll_context
const MAX_SCROLLS = 500;
const SCROLL_ID_PREFIX = 'arkime:mongodb:';
const MAX_BUCKETS = 65536;
// The mongodbPatternTimeout of the patternRun a query runs in, every query it makes uses it
const patternMs = new AsyncLocalStorage();
const BAD_SEGMENTS = new Set(['', '__proto__', 'constructor', 'prototype']);

function defineSessions ({ ArkimeDb, ArkimeConfig, MongoConnection, helpers }) {
  const { escapeRegex, wildcardRegex, regexPattern, ipBytes, cidrRange, ipsOf } = helpers;

  function unsupported (feature) {
    const err = new Error(`MongoDB sessions: ${feature} isn't supported`);
    err.statusCode = 400;
    return err;
  }

  function badRequest (msg) {
    return Object.assign(new Error(`MongoDB sessions: ${msg}`), { statusCode: 400 });
  }

  // An ES keep alive like 5m or 300s, how long an unread scroll stays open
  function scrollMs (keepAlive) {
    const m = /^(\d+)(ms|s|m|h)?$/.exec(String(keepAlive ?? ''));
    if (!m) { return 5 * 60 * 1000; }
    return Number(m[1]) * { ms: 1, s: 1000, m: 60000, h: 3600000 }[m[2] ?? 'ms'];
  }

  // A plain dotted path, never an operator or a prototype key
  function checkField (field) {
    if (typeof field !== 'string' || field.split('.').some(p => BAD_SEGMENTS.has(p) || p[0] === '$')) {
      throw badRequest(`invalid field '${field}'`);
    }
    return field;
  }

  // A value to match, never an object MongoDB would take as operators
  function checkValue (v) {
    if (v !== null && typeof v === 'object' && (Array.isArray(v) || (v._bsontype === undefined && !Buffer.isBuffer(v) && !(v instanceof Date)))) {
      throw badRequest('a query value must be a string, number or boolean');
    }
    return v;
  }

  // Each value of a dotted path, arrays at each level flattened like ES indexes them
  // Any regexp in the query or its aggs, which PCRE can run long
  function hasRegexp (v) {
    if (Array.isArray(v)) { return v.some(hasRegexp); }
    if (!v || typeof v !== 'object') { return false; }
    return Object.entries(v).some(([k, sub]) => k === 'regexp' || hasRegexp(sub));
  }

  function flatValues (field) {
    let expr = '$$ROOT';
    for (const part of field.split('.')) {
      const next = { $map: { input: wrap(expr), as: 'x', in: { $getField: { field: part, input: '$$x' } } } };
      expr = { $reduce: { input: next, initialValue: [], in: { $concatArrays: ['$$value', wrap('$$this')] } } };
    }
    return { $filter: { input: expr, as: 'x', cond: { $and: [{ $ne: ['$$x', null] }, { $ne: [{ $type: '$$x' }, 'missing'] }] } } };

    function wrap (e) { return { $cond: [{ $isArray: e }, e, { $cond: [{ $in: [{ $type: e }, ['missing', 'null']] }, [], [e]] }] }; }
  }

  // A path's values as an array, MongoDB's path already gathers a field from an array of objects. Much cheaper
  // than flatValues, a bucket key is unwound twice to also flatten an array inside an array of objects.
  function pathValues (field) {
    const e = `$${field}`;
    return { $cond: [{ $isArray: e }, e, { $cond: [{ $in: [{ $type: e }, ['missing', 'null']] }, [], [e]] }] };
  }

  // 16 ip bytes, an ipv4 as ::ffff:a.b.c.d, as the text ES gives an ip
  function ipString (bin) {
    const b = Buffer.from(bin.buffer ?? bin);
    if (b.subarray(0, 10).every(x => x === 0) && b[10] === 255 && b[11] === 255) { return [...b.subarray(12)].join('.'); }
    const groups = [];
    for (let i = 0; i < 16; i += 2) { groups.push(b.readUInt16BE(i)); }
    let best = [-1, 0];
    for (let i = 0; i < 8;) {
      let j = i;
      while (j < 8 && groups[j] === 0) { j++; }
      if (j - i > best[1]) { best = [i, j - i]; }
      i = j === i ? i + 1 : j;
    }
    const hex = groups.map(g => g.toString(16));
    if (best[1] < 2) { return hex.join(':'); }
    return `${hex.slice(0, best[0]).join(':')}::${hex.slice(best[0] + best[1]).join(':')}`;
  }

  // ES ip semantics: a cidr, a partial ip like "192.168" (a /16) and any textual form of an ipv6 match
  function ipRange (v) {
    if (typeof v !== 'string') { return null; }
    if (v.includes('/')) {
      try { return cidrRange(v); } catch (e) { return null; }
    }
    if (ipBytes(v)) { return cidrRange(v); }
    const parts = v.replace(/\.$/, '').split('.');
    if (parts.length > 0 && parts.length < 4 && parts.every(p => /^\d{1,3}$/.test(p) && +p < 256)) {
      return cidrRange(`${[...parts, ...Array(4 - parts.length).fill('0')].join('.')}/${parts.length * 8}`);
    }
    return null;
  }

  class MongoSessions {
    #conn;
    #prefix;
    #fieldsMap;
    #collections;
    #collectionsAt = 0;
    #lookups = new Map();
    #lastShortcuts;
    #indexed = new Set();
    #EsQuery;
    #scrolls = new Map();

    static tlsHost (url) { return MongoConnection.tlsHost(url); }

    constructor (info) {
      this.#prefix = info.prefix ?? '';
      // Shares the client when the cluster db is the same url
      this.#conn = ArkimeDb.connect(info.sessionsDbUrl, { insecure: info.insecure, caTrustFile: info.caTrustFile });
      this.#fieldsMap = info.fieldsMap ?? (() => ({}));
      if (!info.EsQuery) { throw new Error('MongoDB sessions need info.EsQuery, viewer/esQuery.js'); }
      this.#EsQuery = info.EsQuery;
    }

    get #db () { return this.#conn.db; }

    #type (field) { return this.#fieldsMap()?.[field]?.type; }
    #isDate (field) { return DATE_FIELDS.has(field) || this.#type(field) === 'date'; }
    #isNumber (field) { return NUMBER_TYPES.has(this.#type(field)) || DATE_FIELDS.has(field); }
    #isIp (field) { return this.#type(field) === 'ip' || field === 'ip' || /\.ip$/.test(field) || /Ip$/.test(field); }
    #isTokens (field) { return /textfield$/.test(this.#type(field) ?? '') || field.endsWith('Tokens'); }
    #path (field) { return checkField(field) === 'id' || field === '_id' ? '_id' : field; }

    // A query value the way capture stored it, ES matches "80" to the number 80
    #value (field, v) {
      if (this.#isDate(field) && typeof v === 'string') {
        const ms = Date.parse(v);
        return Number.isNaN(ms) ? (Number.isFinite(Number(v)) ? Number(v) : v) : ms;
      }
      if (this.#isNumber(field) && typeof v === 'string' && v.trim() !== '' && Number.isFinite(Number(v))) { return Number(v); }
      return v;
    }

    // --------------------------------------------------------------------------
    // The collections an index pattern names, newest first
    async #resolve (index) {
      if (!this.#collections || Date.now() - this.#collectionsAt > COLLECTIONS_CACHE_MS) {
        const names = await this.#db.listCollections({ name: { $regex: `^${escapeRegex(this.#prefix)}sessions3-` } }, { nameOnly: true }).toArray();
        this.#collections = names.map(c => c.name).sort().reverse();
        this.#collectionsAt = Date.now();
      }
      // The viewer leaves the prefix off like the ES client adds it
      const items = (Array.isArray(index) ? index : String(index ?? `${this.#prefix}sessions3-*`).split(','))
        .map(s => s.trim().replace(/^partial-/, '').replace(/^sessions3-/, `${this.#prefix}sessions3-`)).filter(Boolean);
      const out = new Set();
      for (const item of items) {
        if (item.includes('*')) {
          const re = new RegExp(`^${item.replace(/\*+/g, '*').split('*').map(escapeRegex).join('.*')}$`);
          for (const coll of this.#collections) { if (re.test(coll)) { out.add(coll); } }
        } else if (this.#collections.includes(item)) {
          out.add(item);
        }
      }
      return [...out];
    }

    // One pipeline over several collections, each branch filtered (and maybe sorted and cut) on its own
    // indexes. tag sets _coll to the collection a row came from.
    #union (colls, branch, rest, tag) {
      const each = (coll) => tag ? [...branch, { $set: { _coll: { $literal: coll } } }] : branch;
      const [first, ...others] = colls;
      const pipeline = [...each(first)];
      for (const coll of others) { pipeline.push({ $unionWith: { coll, pipeline: each(coll) } }); }
      return this.#db.collection(first).aggregate([...pipeline, ...rest], { allowDiskUse: true, maxTimeMS: this.#timeout() });
    }

    #timeout () {
      const ms = Math.max(1, parseInt(ArkimeConfig.get('mongodbSessionsTimeout', 300), 10) || 300) * 1000;
      const pattern = patternMs.getStore();
      return pattern ? Math.min(ms, pattern) : ms;
    }

    #patternRun (fn) {
      return this.#conn.patternRun(({ maxTimeMS }) => patternMs.run(maxTimeMS, fn));
    }

    // --------------------------------------------------------------------------
    compile (node) {
      if (node.all) { return {}; }
      if (node.none) { return FALSE; }
      if (node.and) { return node.and.length === 1 ? this.compile(node.and[0]) : { $and: node.and.map(n => this.compile(n)) }; }
      if (node.or) { return node.or.length === 1 ? this.compile(node.or[0]) : { $or: node.or.map(n => this.compile(n)) }; }
      if (node.not) { return { $nor: [this.compile(node.not)] }; }

      const { field, op } = node;
      const path = this.#path(field);
      let value = node.value;
      switch (op) {
      case 'eq': return this.#eq(field, value);
      case 'in': return value.length ? { $or: value.map(v => this.#eq(field, v)) } : FALSE;
      case 'lookup': {
        const lookup = this.#lookups.get(String(value.id));
        // Fails closed, an empty set would let a negation match everything
        if (!lookup) { throw new Error(`MongoDB sessions: shortcut ${value.id} isn't in ${this.#prefix}lookups`); }
        const kind = ['ip', 'string', 'number'].includes(value.path) ? value.path : 'string';
        const vals = lookup[kind] ?? [];
        return vals.length ? { $or: vals.map(v => this.#eq(field, v)) } : FALSE;
      }
      case 'range': {
        if (this.#isIp(field)) {
          const ipBounds = {};
          for (const [o, v] of value) {
            const r = ipRange(v);
            if (!r) { return FALSE; }
            // lte and gt take in the whole block below or above, like ES
            ipBounds[`$${o}`] = o === 'lte' || o === 'gt' ? r.end : r.start;
          }
          return { _ip: { $elemMatch: { f: field, a: ipBounds } } };
        }
        const bounds = {};
        for (const [o, v] of value) { checkValue(v); bounds[`$${o}`] = this.#isDate(field) ? this.#EsQuery.dateBound(o, v) : this.#value(field, v); }
        if (value.length === 1) { return { [path]: bounds }; }
        // One value has to meet every bound, the plain bounds narrow it down off an index first
        const same = Object.entries(bounds).map(([o, v]) => ({ [o]: ['$$x', { $literal: v }] }));
        same.unshift(Object.values(bounds).every(v => typeof v === 'number') ? { $isNumber: '$$x' } : { $eq: [{ $type: '$$x' }, 'string'] });
        return { $and: [{ [path]: bounds }, { $expr: { $anyElementTrue: [{ $map: { input: flatValues(field), as: 'x', in: { $and: same } } }] } }] };
      }
      case 'exists': return field === 'id' ? {} : { [path]: { $type: NON_NULL } };
      case 'prefix': return { [path]: { $regex: `^${escapeRegex(value)}` } };
      case 'wildcard': return { [path]: { $regex: wildcardRegex(value), $options: 's' } };
      case 'regexp':
        try {
          return { [path]: { $regex: regexPattern(value), $options: 's' } };
        } catch (err) {
          throw Object.assign(err, { statusCode: 400 });
        }
      case 'match': return { [path]: { $regex: escapeRegex(value), $options: 'i' } };
      case 'phrase':
        if (this.#isTokens(field)) { return this.#tokens(field, value); }
        value = String(value);
        return { [path]: { $regex: escapeRegex(value) } };
      }
      throw unsupported(`query ${op}`);
    }

    #eq (field, v) {
      const path = this.#path(field);
      if (v === null || v === undefined) { return FALSE; }
      checkValue(v);
      if (this.#isIp(field) && typeof v === 'string') {
        const r = ipRange(v);
        if (r) {
          return { _ip: { $elemMatch: { f: field, a: r.start.equals(r.end) ? r.start : { $gte: r.start, $lte: r.end } } } };
        }
      }
      if (this.#isTokens(field)) { return this.#tokens(field, v); }
      return { [path]: this.#value(field, v) };
    }

    // ES analyzes a *Tokens query like the field (split on \W+, lowercase), capture wrote the tokens, so every one must be there
    #tokens (field, v) {
      const tokens = String(v).toLowerCase().split(/[^a-z0-9_]+/).filter(Boolean);
      if (tokens.length === 0) { return FALSE; }
      if (tokens.length === 1) { return { [field]: tokens[0] }; }
      // A phrase like ES match_phrase: the tokens in order within one value of the source field
      const all = { [field]: { $all: tokens } };
      const source = field.endsWith('Tokens') ? field.slice(0, -6) : undefined;
      if (!this.#fieldsMap()?.[source]) { return all; }
      return { $and: [all, { [source]: { $regex: `(^|[^A-Za-z0-9_])${tokens.join('[^A-Za-z0-9_]+')}([^A-Za-z0-9_]|$)`, $options: 'i' } }] };
    }

    // Shortcut lookups read <prefix>lookups first, which the viewer keeps in sync, so multies sees them too
    async #filter (query) {
      return this.#where(query?.query ?? { match_all: {} });
    }

    async #where (clause) {
      const tree = this.#EsQuery.parse(clause, { unsupported });
      const ids = new Set();
      const walk = (node) => {
        if (node.op === 'lookup') { ids.add(String(node.value.id)); }
        for (const child of [...(node.and ?? []), ...(node.or ?? []), ...(node.not ? [node.not] : [])]) { walk(child); }
      };
      walk(tree);
      if (ids.size) {
        for (const row of await this.#db.collection(`${this.#prefix}lookups`).find({ _id: { $in: [...ids] } }).toArray()) {
          this.#lookups.set(row._id, { ip: row.ip ?? [], string: row.string ?? [], number: row.number ?? [] });
        }
      }
      return this.compile(tree);
    }

    #sort (sort) {
      const out = {};
      for (const item of (Array.isArray(sort) ? sort : sort ? [sort] : [])) {
        if (item === '_doc') { continue; }
        const field = typeof item === 'string' ? item : Object.keys(item)[0];
        if (field === '_doc' || field === '_score') { continue; }
        const dir = typeof item === 'string' ? 'asc' : String(item[field]?.order ?? item[field] ?? 'asc').toLowerCase();
        out[this.#path(field)] = dir === 'desc' ? -1 : 1;
      }
      if (Object.keys(out).length === 0) { out.lastPacket = -1; }
      if (!('_id' in out)) { out._id = Object.values(out)[0]; }
      return out;
    }

    // Keyset paging needs a scalar every session has, so a page follows firstPacket or lastPacket (which
    // have indexes with _id) in the first sort key's direction, any other sort field follows lastPacket
    #keysetSort (sort) {
      const [[field, dir]] = Object.entries(sort);
      const key = field === 'firstPacket' ? field : 'lastPacket';
      return { [key]: dir, _id: sort._id ?? dir };
    }

    // The sessions after the last one read, in a keyset sort
    #after (sort, last) {
      const [[key, dir], [, idDir]] = Object.entries(sort);
      const op = (d) => d === -1 ? '$lt' : '$gt';
      return { $or: [{ [key]: { [op(dir)]: last[key] } }, { [key]: last[key], _id: { [op(idDir)]: last._id } }] };
    }

    // --------------------------------------------------------------------------
    // Every leaf value by its dotted path, arrays of objects merged like ES fields
    #flatten (v, path, out) {
      if (v === null || v === undefined) { return; }
      if (Array.isArray(v)) {
        for (const item of v) { this.#flatten(item, path, out); }
      } else if (typeof v === 'object' && !(v instanceof Date) && !Buffer.isBuffer(v) && v._bsontype === undefined) {
        for (const key of Object.keys(v)) {
          if (!path && key === '_ip') { continue; }
          this.#flatten(v[key], path ? `${path}.${key}` : key, out);
        }
      } else {
        (out[path] ??= []).push(v);
      }
    }

    // A hit like ES gives one: fields are arrays by dotted name with dates as ISO strings, _source is the session
    #hit (row, sort) {
      const { _id, _coll, ...source } = row;
      delete source._ip;
      const fields = Object.create(null);
      this.#flatten(source, '', fields);
      for (const [f, vals] of Object.entries(fields)) {
        if (this.#isDate(f)) { fields[f] = vals.map(v => typeof v === 'number' ? new Date(v).toISOString() : v); }
      }
      const hit = { _id, _index: _coll, fields, _source: source };
      if (sort) {
        hit.sort = Object.keys(sort).map(p => {
          const v = p === '_id' ? _id : fields[p]?.[0];
          return this.#isDate(p) && typeof v === 'string' ? Date.parse(v) : v ?? null;
        });
      }
      return hit;
    }

    // --------------------------------------------------------------------------
    // Aggregations, each one pipeline over the matching sessions
    async #aggs (aggs, colls, filter, budget) {
      if (!aggs) { return undefined; }
      const out = {};
      await Promise.all(Object.entries(aggs).map(async ([aggName, agg]) => { out[aggName] = await this.#agg(agg, colls, filter, budget); }));
      return out;
    }

    // ES's search.max_buckets, every bucket of every agg in one search counts
    #spend (budget, n) {
      budget.left -= n;
      if (budget.left < 0) { throw badRequest(`Trying to create too many buckets. Must be less than or equal to: [${MAX_BUCKETS}]`); }
    }

    #metricOp (agg) {
      return ['sum', 'min', 'max', 'avg', 'cardinality', 'value_count', 'percentiles'].find(o => agg[o]);
    }

    // A $group accumulator for a metric over every value of the field like ES, and for the ones that
    // collect the values the expression that finishes them after the $group, so a bucket can sort on it
    #accumulate (agg) {
      const op = this.#metricOp(agg);
      const path = `$${checkField(agg[op].field)}`;
      const vals = pathValues(agg[op].field);
      const flat = (ref) => ({ $reduce: { input: ref, initialValue: [], in: { $concatArrays: ['$$value', '$$this'] } } });
      switch (op) {
      case 'sum': return [{ $sum: { $sum: path } }];
      case 'min': return [{ $min: { $min: path } }];
      case 'max': return [{ $max: { $max: path } }];
      case 'avg': return [{ $push: vals }, (ref) => ({ $avg: flat(ref) })];
      case 'cardinality': return [{ $addToSet: vals }, (ref) => ({ $size: { $setUnion: [flat(ref)] } })];
      case 'value_count': return [{ $sum: { $size: vals } }];
      case 'percentiles': {
        const percents = agg.percentiles.percents;
        if (!Array.isArray(percents) || percents.length !== 1 || !(percents[0] >= 0 && percents[0] <= 100)) { throw unsupported('percentiles of more than one percent'); }
        return [{ $push: vals }, (ref) => ({
          $let: {
            vars: { s: { $sortArray: { input: { $filter: { input: flat(ref), cond: { $isNumber: '$$this' } } }, sortBy: 1 } } },
            in: { $cond: [{ $eq: [{ $size: '$$s' }, 0] }, 0, { $arrayElemAt: ['$$s', { $max: [0, { $subtract: [{ $ceil: { $multiply: [percents[0] / 100, { $size: '$$s' }] } }, 1] }] }] }] }
          }
        })];
      }
      }
    }

    #isMetric (agg) { return !!this.#metricOp(agg); }

    async #agg (agg, colls, filter, budget) {
      if (colls.length === 0) {
        if (agg.terms || agg.histogram || agg.date_histogram) { return { buckets: [], sum_other_doc_count: 0, doc_count_error_upper_bound: 0 }; }
        if (agg.filter) { return { doc_count: 0 }; }
        return { value: this.#metricOp(agg) === 'sum' || this.#metricOp(agg) === 'value_count' ? 0 : null };
      }
      const subs = agg.aggregations ?? agg.aggs;
      if (this.#isMetric(agg)) {
        const [acc, fin] = this.#accumulate(agg);
        const rest = [{ $group: { _id: null, v: acc } }];
        if (fin) { rest.push({ $set: { v: fin('$v') } }); }
        const [row] = await this.#union(colls, [{ $match: filter }], rest).toArray();
        const v = row ? row.v : null;
        return { value: v ?? (['sum', 'value_count', 'cardinality', 'percentiles'].includes(this.#metricOp(agg)) ? 0 : null) };
      }
      if (agg.filter) {
        const sub = { $and: [filter, await this.#where(agg.filter)] };
        const [count, subAggs] = await Promise.all([this.#count(colls, sub), this.#aggs(subs, colls, sub, budget)]);
        return { doc_count: count, ...subAggs };
      }
      if (agg.terms) { return this.#terms(agg.terms, subs, colls, filter, budget); }
      if (agg.histogram || agg.date_histogram) { return this.#histogram(agg.histogram ?? agg.date_histogram, !!agg.date_histogram, subs, colls, filter, budget); }
      throw unsupported(`aggregation ${Object.keys(agg).join(',')}`);
    }

    // Metric sub aggs ride along in the bucket's $group as m<n>, names maps those back since an agg name
    // like source.packets can't be a $group field. The others run once per bucket.
    #groupSubs (subs) {
      const group = {};
      const finish = {};
      const nested = {};
      const names = {};
      for (const [subName, sub] of Object.entries(subs ?? {})) {
        if (this.#isMetric(sub)) {
          const [acc, fin] = this.#accumulate(sub);
          const key = `m${Object.keys(group).length}`;
          group[key] = acc;
          names[key] = subName;
          if (fin) { finish[key] = fin(`$${key}`); }
        } else {
          nested[subName] = sub;
        }
      }
      // The stages right after the $group
      const after = Object.keys(finish).length ? [{ $set: finish }] : [];
      return { group, after, nested, names };
    }

    async #bucketSubs (buckets, keyFilter, nested, colls, filter, budget) {
      if (!Object.keys(nested).length) { return; }
      for (let i = 0; i < buckets.length; i += BUCKET_CONCURRENCY) {
        await Promise.all(buckets.slice(i, i + BUCKET_CONCURRENCY).map(async (b) => {
          Object.assign(b, await this.#aggs(nested, colls, { $and: [filter, keyFilter(b)] }, budget));
        }));
      }
    }

    #fillMetrics (bucket, row, { group, names }) {
      for (const key of Object.keys(group)) { bucket[names[key]] = { value: row[key] ?? 0 }; }
    }

    // The values a terms agg buckets on and the filter for one bucket, a field or one of the painless scripts
    // the viewer sends: the ip summary's source.ip and destination.ip, and ip.dst:port
    #termsKey (terms) {
      if (!terms.script) {
        checkField(terms.field);
        if (['textfield', 'lotextfield', 'uptextfield'].includes(this.#type(terms.field))) {
          throw Object.assign(new Error(`Fielddata is disabled on text field [${terms.field}]`), { statusCode: 400 });
        }
        // An ip buckets on its bytes in _ip, which order like ES orders ips
        if (this.#isIp(terms.field)) {
          const ips = { $filter: { input: { $ifNull: ['$_ip', []] }, cond: { $eq: ['$$this.f', terms.field] } } };
          return [{ $map: { input: ips, in: '$$this.a' } }, (key) => this.#eq(terms.field, key), ipString];
        }
        return [`$${terms.field}`, (key) => this.#eq(terms.field, key)];
      }
      const src = String(terms.script.source ?? terms.script).replace(/"/g, "'");
      if (src.includes("doc['source.ip']") && src.includes("doc['destination.ip']") && !src.includes('_')) {
        return [{ $concatArrays: [pathValues('source.ip'), pathValues('destination.ip')] }, (key) => ({ $or: [this.#eq('source.ip', key), this.#eq('destination.ip', key)] })];
      }
      if (src.includes("doc['destination.ip']") && src.includes("doc['destination.port']")) {
        const under = src.includes("'_'");
        const port = { $toString: { $first: pathValues('destination.port') } };
        const sep = (ip) => under ? '_' : { $cond: [{ $gt: [{ $indexOfCP: [ip, '.'] }, 0] }, ':', '.'] };
        const values = { $map: { input: pathValues('destination.ip'), as: 'ip', in: { $concat: ['$$ip', sep('$$ip'), port] } } };
        return [{ $cond: [{ $eq: [port, null] }, [], values] }, (key) => {
          const str = String(key);
          const idx = under ? str.lastIndexOf('_') : Math.max(str.lastIndexOf(':'), str.lastIndexOf('.'));
          if (idx < 1) { return FALSE; }
          return { $and: [this.#eq('destination.ip', str.slice(0, idx)), { 'destination.port': Number(str.slice(idx + 1)) }] };
        }];
      }
      throw unsupported('script terms aggregation');
    }

    async #terms (terms, subs, colls, filter, budget) {
      const [values, keyFilter, toKey = (k) => k] = this.#termsKey(terms);
      const size = Math.min(MAX_BUCKETS, Math.max(1, Number(terms.size ?? 10)));
      const groupSubs = this.#groupSubs(subs);
      const { group, nested, names } = groupSubs;
      const [orderKey, orderDir] = this.#order(terms.order) ?? ['_count', 'desc'];
      const dir = String(orderDir).toLowerCase() === 'asc' ? 1 : -1;
      const sortBy = orderKey === '_key' || orderKey === '_term' ? { _id: dir } : orderKey === '_count' ? { doc_count: dir, _id: 1 } : { [Object.keys(names).find(k => names[k] === orderKey.replace(/\[.*\]$/, '')) ?? 'doc_count']: dir, _id: 1 };
      const { branch, merge } = this.#buckets(filter, values, { _k: { $ne: null } }, '$_k', group);
      const rest = [
        ...merge,
        ...groupSubs.after,
        { $facet: { top: [{ $sort: sortBy }, { $limit: size }], all: [{ $group: { _id: null, n: { $sum: '$doc_count' } } }] } }
      ];
      if (terms.min_doc_count > 1) { rest.splice(rest.length - 1, 0, { $match: { doc_count: { $gte: Number(terms.min_doc_count) } } }); }
      const [row] = await this.#union(colls, branch, rest).toArray();
      const topRows = row?.top ?? [];
      const buckets = topRows.map(r => {
        const bucket = { key: toKey(r._id), doc_count: r.doc_count };
        this.#fillMetrics(bucket, r, groupSubs);
        return bucket;
      });
      this.#spend(budget, buckets.length);
      await this.#bucketSubs(buckets, (b) => keyFilter(b.key), nested, colls, filter, budget);
      const other = (row?.all?.[0]?.n ?? 0) - buckets.reduce((s, b) => s + b.doc_count, 0);
      return { doc_count_error_upper_bound: 0, sum_other_doc_count: Math.max(0, other), buckets };
    }

    // The stages that bucket the sessions by each of values: every period's branch of the $unionWith
    // groups its own sessions when the metrics can be added up after, else the sessions go to one $group
    #buckets (filter, values, keep, key, group) {
      const pre = [{ $match: filter }, { $set: { _k: values } }, { $unwind: '$_k' }, { $unwind: '$_k' }, { $match: keep }];
      const ops = Object.fromEntries(Object.entries(group).map(([k, acc]) => [k, Object.keys(acc)[0]]));
      const bucket = { $group: { _id: key, doc_count: { $sum: 1 }, ...group } };
      if (!Object.values(ops).every(op => ['$sum', '$min', '$max'].includes(op))) { return { branch: pre, merge: [bucket] }; }
      const merge = { $group: { _id: '$_id', doc_count: { $sum: '$doc_count' }, ...Object.fromEntries(Object.entries(ops).map(([k, op]) => [k, { [op]: `$${k}` }])) } };
      return { branch: [...pre, bucket], merge: [merge] };
    }

    #interval (histo, isDate) {
      const iv = histo.fixed_interval ?? histo.calendar_interval ?? histo.interval;
      if (typeof iv === 'number') {
        if (!(Number.isFinite(iv) && iv > 0)) { throw badRequest(`histogram interval ${iv}`); }
        return iv;
      }
      const m = String(iv).match(/^(\d+)?(ms|s|m|h|d|w)$/) ?? String(iv).match(/^()(second|minute|hour|day|week)$/);
      if (!m) {
        if (!isDate && Number.isFinite(Number(iv)) && Number(iv) > 0) { return Number(iv); }
        throw unsupported(`histogram interval ${iv}`);
      }
      const unit = { ms: 1, s: 1000, second: 1000, m: 60000, minute: 60000, h: 3600000, hour: 3600000, d: 86400000, day: 86400000, w: 604800000, week: 604800000 }[m[2]];
      return (m[1] ? Number(m[1]) : 1) * unit;
    }

    async #histogram (histo, isDate, subs, colls, filter, budget) {
      const field = checkField(histo.field);
      const interval = this.#interval(histo, isDate);
      const offset = Number(histo.offset ?? 0) || 0;
      const groupSubs = this.#groupSubs(subs);
      const { group, nested } = groupSubs;
      const slot = (e) => ({ $floor: { $divide: [{ $subtract: [e, offset] }, interval] } });
      const key = { $add: [{ $multiply: [slot('$_k'), interval] }, offset] };
      let values = `$${field}`;
      let keyFilter = (b) => ({ [this.#path(field)]: { $gte: b.key, $lt: b.key + interval } });
      if (field === 'packetRange') {
        // ES's packetRange is a date range, a histogram on it counts the session in every interval it spans
        const fp = { $ifNull: ['$packetRange.gte', '$firstPacket'] };
        const lp = { $ifNull: ['$packetRange.lte', '$lastPacket'] };
        const end = { $toLong: slot(lp) };
        const start = { $max: [{ $min: [{ $toLong: slot({ $ifNull: [fp, lp] }) }, end] }, { $subtract: [end, 2000000] }] };
        values = { $cond: [{ $isNumber: lp }, { $map: { input: { $range: [start, { $add: [end, 1] }] }, as: 's', in: { $add: [{ $multiply: ['$$s', interval] }, offset] } } }, []] };
        keyFilter = (b) => ({ $and: [{ lastPacket: { $gte: b.key } }, { firstPacket: { $lt: b.key + interval } }] });
      }
      const { branch, merge } = this.#buckets(filter, values, { _k: { $type: 'number' } }, key, group);
      // One past the budget is enough to know it's too many
      const rest = [...merge, ...groupSubs.after, { $sort: { _id: 1 } }, { $limit: Math.max(0, budget.left) + 1 }];
      const rows = await this.#union(colls, branch, rest).toArray();
      if (rows.length > budget.left) { this.#spend(budget, rows.length); }
      let buckets = rows.map(r => {
        const bucket = { key: r._id, doc_count: r.doc_count };
        if (isDate) { bucket.key_as_string = new Date(r._id).toISOString(); }
        this.#fillMetrics(bucket, r, groupSubs);
        return bucket;
      });
      if (Number(histo.min_doc_count ?? 1) === 0) { buckets = this.#fill(buckets, histo, interval, offset, isDate, groupSubs.names); }
      const order = this.#order(histo.order);
      if (order) {
        const [orderKey, orderDir] = order;
        const dir = String(orderDir).toLowerCase() === 'asc' ? 1 : -1;
        const metric = orderKey.replace(/\[.*\]$/, '');
        const val = (b) => orderKey === '_key' ? b.key : orderKey === '_count' ? b.doc_count : (b[metric]?.value ?? 0);
        buckets.sort((a, b) => (val(a) - val(b)) * dir || a.key - b.key);
      }
      this.#spend(budget, buckets.length);
      await this.#bucketSubs(buckets, keyFilter, nested, colls, filter, budget);
      return { buckets };
    }

    // An ES order, { key: dir } or a list of them where the first wins, as [key, dir]
    #order (order) {
      const first = Array.isArray(order) ? order[0] : order;
      return first && typeof first === 'object' ? Object.entries(first)[0] : undefined;
    }

    // The empty buckets ES adds with min_doc_count 0, between the data or extended_bounds
    #fill (buckets, histo, interval, offset, isDate, names) {
      const bounds = histo.extended_bounds ?? histo.hard_bounds;
      const floor = (v) => Math.floor((v - offset) / interval) * interval + offset;
      const toNum = (v) => typeof v === 'string' ? Date.parse(v) : Number(v);
      let lo = buckets[0]?.key;
      let hi = buckets.at(-1)?.key;
      if (bounds?.min !== undefined) { lo = Math.min(lo ?? Infinity, floor(toNum(bounds.min))); }
      if (bounds?.max !== undefined) { hi = Math.max(hi ?? -Infinity, floor(toNum(bounds.max))); }
      if (lo === undefined || hi === undefined || !Number.isFinite(lo) || !Number.isFinite(hi) || (hi - lo) / interval > 100000 || lo + interval <= lo) { return buckets; }
      const have = new Map(buckets.map(b => [b.key, b]));
      const out = [];
      for (let k = lo; k <= hi && out.length <= 100000; k += interval) {
        let b = have.get(k);
        if (!b) {
          b = { key: k, doc_count: 0 };
          if (isDate) { b.key_as_string = new Date(k).toISOString(); }
          for (const n of Object.values(names)) { b[n] = { value: 0 }; }
        }
        out.push(b);
      }
      return out;
    }

    async #count (colls, filter) {
      const counts = await Promise.all(colls.map(c => this.#db.collection(c).countDocuments(filter, { maxTimeMS: this.#timeout() })));
      return counts.reduce((a, b) => a + b, 0);
    }

    // --------------------------------------------------------------------------
    async searchSessions (index, query = {}, options = {}) {
      return hasRegexp(query) ? this.#patternRun(() => this.#searchSessions(index, query, options)) : this.#searchSessions(index, query, options);
    }

    async #searchSessions (index, query, options) {
      const colls = await this.#resolve(index);
      const filter = await this.#filter(query);
      const size = Math.max(0, Number(options.size ?? query.size ?? 10) || 0);
      const from = Math.max(0, Number(options.from ?? query.from ?? 0) || 0);
      const sort = this.#sort(query.sort);
      const [hits, total, aggregations] = await Promise.all([
        size && colls.length ? this.#union(colls, [{ $match: filter }, { $sort: sort }, { $limit: from + size }], [{ $sort: sort }, { $skip: from }, { $limit: size }], true).toArray() : [],
        colls.length ? this.#count(colls, filter) : 0,
        this.#aggs(query.aggregations ?? query.aggs, colls, filter, { left: MAX_BUCKETS })
      ]);
      return { hits: { total, hits: hits.map(r => this.#hit(r, Array.isArray(query.sort) ? sort : undefined)) }, aggregations };
    }

    async getSession (id, options = {}) {
      const result = await this.searchSessions(null, { query: { ids: { values: [id] } }, size: 1 }, options);
      return result.hits.hits[0];
    }

    async msearchSessions (index, queries, options = {}) {
      const responses = await Promise.all(queries.map(q => this.searchSessions(index, typeof q === 'string' ? JSON.parse(q) : q, options)));
      return { body: { responses } };
    }

    // An ES style scroll. MongoDB kills an idle cursor after cursorTimeoutMillis whatever the keep alive asks,
    // so a scroll keeps its filter, sort and the last (sort key, _id) it returned and each read is a fresh
    // query for the page after that
    async searchScroll (index, query, options, cb) {
      try {
        const result = options?.scroll ? await this.#startScroll(index, query, options.scroll) : await this.searchSessions(index, query, options);
        return cb ? cb(null, result) : result;
      } catch (err) {
        if (cb) { return cb(err); }
        throw err;
      }
    }

    ownsScroll (scrollId) {
      return typeof scrollId === 'string' && scrollId.startsWith(SCROLL_ID_PREFIX);
    }

    async scroll (scrollId) {
      return this.#readScroll(scrollId);
    }

    async clearScroll (scrollId) {
      const state = this.#scrolls.get(scrollId);
      if (!state) { return; }
      this.#scrolls.delete(scrollId);
      clearTimeout(state.timer);
    }

    // The next n sessions of filter in a keyset sort, the ones after `after` when it is set
    #page (colls, filter, sort, after, n, patterns) {
      const match = after ? { $and: [filter, this.#after(sort, after)] } : filter;
      const run = () => this.#union(colls, [{ $match: match }, { $sort: sort }, { $limit: n }], [{ $sort: sort }, { $limit: n }], true).toArray();
      return patterns ? this.#patternRun(run) : run();
    }

    // The total is counted once, with the first page
    async #startScroll (index, query, keepAlive) {
      if (this.#scrolls.size >= MAX_SCROLLS) { throw badRequest(`more than ${MAX_SCROLLS} open scrolls`); }
      const colls = await this.#resolve(index);
      if (!colls.length) { return { hits: { total: 0, hits: [] }, _scroll_id: this.#EsQuery.SCROLL_DONE }; }
      const filter = await this.#filter(query);
      const size = Math.min(Math.max(1, Number(query.size ?? 10) || 10), 10000);
      const sort = this.#keysetSort(this.#sort(query.sort));
      const patterns = hasRegexp(query);
      const count = () => this.#count(colls, filter);
      const total = await (patterns ? this.#patternRun(count) : count());
      const id = SCROLL_ID_PREFIX + cryptoLib.randomUUID();
      this.#scrolls.set(id, {
        colls,
        filter,
        sort,
        size,
        total,
        patterns,
        hitSort: Array.isArray(query.sort) ? sort : undefined,
        keepAliveMs: scrollMs(keepAlive)
      });
      return this.#readScroll(id);
    }

    // A short page is the last one, its id is SCROLL_DONE so the caller's next scroll is empty
    async #readScroll (scrollId) {
      const state = this.#scrolls.get(scrollId);
      if (!state) {
        const err = new Error(`MongoDB sessions: scroll ${scrollId} is gone`);
        err.statusCode = 404;
        throw err;
      }
      clearTimeout(state.timer);
      let rows;
      try {
        rows = await this.#page(state.colls, state.filter, state.sort, state.after, state.size, state.patterns);
      } catch (err) {
        await this.clearScroll(scrollId);
        throw err;
      }
      let id = scrollId;
      if (rows.length < state.size) {
        await this.clearScroll(scrollId);
        id = this.#EsQuery.SCROLL_DONE;
      } else {
        const [key] = Object.keys(state.sort);
        const last = rows.at(-1);
        state.after = { [key]: last[key], _id: last._id };
        state.timer = setTimeout(() => this.clearScroll(scrollId), state.keepAliveMs);
        state.timer.unref?.();
      }
      return { _scroll_id: id, hits: { total: state.total, hits: rows.map(r => this.#hit(r, state.hitSort)) } };
    }

    // Pages by lastPacket and _id, the order every caller consumes it in
    async * searchScrollIterator (index, query, options = {}) {
      const colls = await this.#resolve(index);
      if (!colls.length) { return; }
      const filter = await this.#filter(query);
      const patterns = hasRegexp(query);
      let remaining = Number(query.size ?? 10000);
      const pageSize = Math.min(remaining || 2000, 2000);
      const sort = { lastPacket: -1, _id: -1 };
      let after;
      while (remaining > 0) {
        const n = Math.min(pageSize, remaining);
        const rows = await this.#page(colls, filter, sort, after, n, patterns);
        if (!rows.length) { break; }
        remaining -= rows.length;
        yield { hits: { total: rows.length, hits: rows.map(r => this.#hit(r)) } };
        after = rows.at(-1);
        if (rows.length < n) { break; }
      }
    }

    // --------------------------------------------------------------------------
    async #update (index, id, pipeline, nodes) {
      const filter = nodes ? { _id: id, node: { $in: nodes } } : { _id: id };
      for (const c of await this.#resolve(index)) {
        const { matchedCount } = await this.#db.collection(c).updateOne(filter, pipeline);
        if (matchedCount) { return { result: 'updated' }; }
      }
      const err = new Error(`Session ${id} not found`);
      err.statusCode = 404;
      throw err;
    }

    #tagsCnt () { return { $set: { tagsCnt: { $size: '$tags' } } }; }

    async addTagsToSession (index, id, tags) {
      const have = { $ifNull: ['$tags', []] };
      return this.#update(index, id, [
        { $set: { tags: { $concatArrays: [have, { $filter: { input: { $literal: [...new Set(tags)] }, as: 't', cond: { $not: { $in: ['$$t', have] } } } }] } } },
        this.#tagsCnt()
      ]);
    }

    async removeTagsFromSession (index, id, tags) {
      return this.#update(index, id, [
        { $set: { tags: { $filter: { input: { $ifNull: ['$tags', []] }, as: 't', cond: { $not: { $in: ['$$t', { $literal: tags }] } } } } } },
        this.#tagsCnt()
      ]);
    }

    // nodes, when set, limits it to sessions from those nodes
    async addHuntToSession (index, id, huntId, huntName, nodes) {
      return this.#update(index, id, [{
        $set: {
          huntId: { $concatArrays: [{ $ifNull: ['$huntId', []] }, [{ $literal: huntId }]] },
          huntName: { $concatArrays: [{ $ifNull: ['$huntName', []] }, [{ $literal: huntName }]] }
        }
      }], nodes);
    }

    // huntId and huntName are parallel, both drop the entry at the id's position
    async removeHuntFromSession (index, id, huntId, huntName) {
      const ids = { $ifNull: ['$huntId', []] };
      const at = { $indexOfArray: [ids, { $literal: huntId }] };
      const without = (arr) => ({
        $cond: [{ $lt: [at, 0] }, arr, { $concatArrays: [{ $slice: [arr, at] }, { $slice: [arr, { $add: [at, 1] }, { $max: [1, { $size: arr }] }] }] }]
      });
      return this.#update(index, id, [{ $set: { huntId: without(ids), huntName: without({ $ifNull: ['$huntName', []] }) } }]);
    }

    // An ES update body, a partial doc or the painless scripts the viewer sends
    async updateSession (index, id, doc) {
      if (doc.doc) { return this.#update(index, id, [{ $set: Object.fromEntries(Object.entries(doc.doc).map(([k, v]) => [k, { $literal: v }])) }]); }
      const source = doc.script?.source ?? '';
      const params = doc.script?.params ?? {};
      if (params.tags && source.includes('ctx._source.tags.add')) { return this.addTagsToSession(index, id, params.tags); }
      if (params.tags && source.includes('ctx._source.tags.remove')) { return this.removeTagsFromSession(index, id, params.tags); }
      if (params.huntId && source.includes('huntId.add')) { return this.addHuntToSession(index, id, params.huntId, params.huntName); }
      if (params.huntId && source.includes('huntId.remove')) { return this.removeHuntFromSession(index, id, params.huntId, params.huntName); }
      throw unsupported('update script');
    }

    async deleteDocument (index, id) {
      for (const c of await this.#resolve(index)) {
        if ((await this.#db.collection(c).deleteOne({ _id: id })).deletedCount) { return { result: 'deleted' }; }
      }
      return { result: 'not_found' };
    }

    async numberOfDocuments (index) {
      const colls = await this.#resolve(index ?? `${this.#prefix}sessions3-*`);
      const counts = await Promise.all(colls.map(c => this.#db.collection(c).estimatedDocumentCount()));
      return { count: counts.reduce((a, b) => a + b, 0) };
    }

    async getMinValue (index, field) {
      return { body: await this.searchSessions(index, { size: 0, aggs: { min: { min: { field } } } }) };
    }

    // Collections are made as capture writes, so a miss may be one this hasn't listed yet
    async refreshForNotFoundRetry () {
      const before = this.#collections?.length;
      this.#collections = undefined;
      await this.#resolve('*');
      return this.#collections.length !== before;
    }

    async listIndices (index) {
      return this.#resolve(index);
    }

    getSessionIndices () {
      return [`${this.#prefix}sessions3-*`];
    }

    // The collections whose period overlaps the time range, like the ES indices
    async getIndices (startTime, stopTime, bounding, rotateIndex) {
      const colls = await this.#resolve(`${this.#prefix}sessions3-*`);
      const out = [];
      let hourLen = 12 * 3600;
      if (rotateIndex === 'hourly') { hourLen = 3600; } else if (rotateIndex?.startsWith('hourly')) { hourLen = +rotateIndex.substring(6) * 3600; }
      for (const coll of colls) {
        const s = coll.substring(`${this.#prefix}sessions3-`.length);
        const year = (+s[0] >= 6 ? 1900 : 2000) + (+s.substring(0, 2));
        let start; let len;
        if (s[2] === 'w') {
          start = Date.UTC(year, 0, (+s.substring(3, 5)) * 7) / 1000; len = 7 * 86400;
        } else if (s[2] === 'm') {
          start = Date.UTC(year, +s.substring(3, 5) - 1, 1) / 1000; len = 31 * 86400;
        } else if (s.length === 6) {
          start = Date.UTC(year, +s.substring(2, 4) - 1, +s.substring(4, 6)) / 1000; len = 86400;
        } else if (/^\d{6}h\d{2}$/.test(s)) {
          start = Date.UTC(year, +s.substring(2, 4) - 1, +s.substring(4, 6), +s.substring(7, 9)) / 1000; len = hourLen;
        } else {
          out.push(coll);
          continue;
        }
        const slack = bounding === 'last' || !bounding ? 0 : len;
        if (start + len > startTime - slack && start <= stopTime + slack) { out.push(coll); }
      }
      return out.length ? out.join(',') : `${this.#prefix}sessions3-*`;
    }

    // Shortcut values for the terms lookups queries use. A deleted shortcut's doc stays, the expression
    // parser refuses a name it doesn't know.
    // Session docs with index, _id and the *Tokens fields, stored the way capture's MongoDB writer does
    async writeSessions (docs) {
      const byColl = new Map();
      for (const { index, ...doc } of docs) {
        let ips;
        try {
          ips = ipsOf(doc);
        } catch (err) {
          console.log(`MongoDB sessions: dropping session ${JSON.stringify(String(doc._id).slice(0, 100))}, ${err.message}`);
          continue;
        }
        if (!byColl.has(index)) { byColl.set(index, []); }
        byColl.get(index).push({ replaceOne: { filter: { _id: doc._id }, replacement: { ...doc, _ip: ips }, upsert: true } });
      }
      await Promise.all([...byColl].map(async ([coll, ops]) => {
        if (!this.#indexed.has(coll)) {
          await this.#db.collection(coll).createIndexes(SESSION_INDEXES);
          this.#indexed.add(coll);
        }
        await this.#db.collection(coll).bulkWrite(ops, { ordered: false });
      }));
    }

    async checkWriteSessions () {
      await this.#db.command({ ping: 1 });
    }

    async syncShortcuts (shortcuts) {
      if (!shortcuts?.length) { return; }
      const arr = (v) => v === undefined || v === null ? [] : Array.isArray(v) ? v : [v];
      const docs = shortcuts.map(s => ({ _id: String(s._id), ip: arr(s._source.ip).map(String), string: arr(s._source.string).map(String), number: arr(s._source.number).map(Number) }));
      const key = JSON.stringify(docs);
      if (key === this.#lastShortcuts) { return; }
      await this.#db.collection(`${this.#prefix}lookups`).bulkWrite(docs.map(d => ({ replaceOne: { filter: { _id: d._id }, replacement: d, upsert: true } })), { ordered: false });
      for (const d of docs) { this.#lookups.set(d._id, d); }
      this.#lastShortcuts = key;
    }

    async close () {
      await Promise.all([...this.#scrolls.keys()].map(id => this.clearScroll(id)));
    }
  }

  return MongoSessions;
}

module.exports = { defineSessions, DEFAULT_FIELDS };
