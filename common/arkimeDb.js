/* arkimeDb.js  -- Database connection helpers, use through ArkimeUtil
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const cryptoLib = require('crypto');
const net = require('net');

class ArkimeDb {
  // ----------------------------------------------------------------------------
  /**
   * Create a redis client from the provided url
   * @param {string} url - The redis url to connect to.
   * @param {string} section - The section this redis client is being created for
   */
  static createRedisClient (url, section) {
    url = ArkimeUtil.sanitizeStr(url);
    // redis://[:pass]@host:port/db
    if (url.startsWith('redis://') || url.startsWith('rediss://')) {
      const match = url.match(/(rediss?):\/\/(:[^@]+@)?([^:/]+)(:[0-9]+)?\/([0-9]+)/);
      if (!match) {
        console.log(`${section} - ERROR - can't parse redis url '%s' should be of form //[:pass@]redishost[:redisport]/redisDbNum`, url);
        process.exit(1);
      }

      if (ArkimeConfig.debug > 1) {
        console.log('REDIS:', url);
      }
      const Redis = require('ioredis');
      return new Redis(url);
    }

    // redis-sentinel://sentinelPassword:redisPassword@host:port[,hostN:portN]/name/db
    if (url.startsWith('redis-sentinel://')) {
      const match = url.match(/(redis-sentinel):\/\/(([^:]+)?:([^@]+)?@)?([^/]+)\/([^/]+)\/([0-9]+)(\/.+)?/);
      if (!match) {
        console.log(`${section} - ERROR - can't parse redis-sentinel url '%s' should be of form //[sentinelPassword:redisPassword@]sentinelHost[:sentinelPort][,sentinelHostN[:sentinelPortN]]/redisName/redisDbNum`, url);
        process.exit(1);
      }

      const options = { sentinels: [], name: match[6], db: parseInt(match[7]) };
      for (const hp of match[5].split(',')) {
        const hostport = hp.split(':');
        options.sentinels.push({ host: hostport[0], port: hostport[1] || 26379 });
      }

      if (match[3]) {
        options.sentinelPassword = match[3];
      }
      if (match[4]) {
        options.password = match[4];
      }

      if (ArkimeConfig.debug > 1) {
        console.log('REDIS-SENTINEL:', options);
      }
      const Redis = require('ioredis');
      return new Redis(options);
    }

    // redis-cluster://[:pass]@host:port/db
    if (url.startsWith('redis-cluster://')) {
      const match = url.match(/(redis-cluster):\/\/(:([^@]+)@)?([^/]+)\/([0-9]+)(\/.+)?/);
      if (!match) {
        console.log(`${section} - ERROR - can't parse redis-cluster url '%s' should be of form //[:redisPassword@]redisHost[:redisPort][,redisHostN[:redisPortN]]/redisDbNum`, url);
        process.exit(1);
      }

      const hosts = [];
      for (const hp of match[4].split(',')) {
        const hostport = hp.split(':');
        hosts.push({ host: hostport[0], port: hostport[1] || 6379 });
      }

      const options = { db: parseInt(match[5]) };
      if (match[3]) {
        options.password = match[3];
      }

      if (ArkimeConfig.debug > 1) {
        console.log('REDIS-CLUSTER: hosts', hosts, 'options', { redisOptions: options });
      }
      const Redis = require('ioredis');
      return new Redis.Cluster(hosts, { redisOptions: options });
    }

    console.log(`${section} - Unknown redis url '%s'`, url);
    process.exit(1);
  }

  // ----------------------------------------------------------------------------
  /**
   * Parse the elasticsearch url and return the url and possible auth object
   */
  static createElasticsearchInfo (url) {
    url = url.split(',')[0];
    url = ArkimeConfig.esUrl(url);
    let auth;
    if (url.includes('://usersElasticsearch')) {
      let es, basicAuth;

      const usersEs = ArkimeConfig.usersDbUrls()?.filter(u => ArkimeDb.isES(u));
      if (usersEs?.length) {
        es = usersEs.join(',');
        basicAuth = ArkimeConfig.get('usersElasticsearchBasicAuth');
      }

      if (!es) {
        console.log(`ERROR - No OpenSearch/Elasticsearch usersDbUrl defined but used in ${url}`);
        process.exit(1);
      }

      url = url.replace(/^.*:\/\/usersElasticsearch/, es);

      if (basicAuth) {
        if (!basicAuth.includes(':')) {
          basicAuth = Buffer.from(basicAuth, 'base64').toString();
        }
        basicAuth = ArkimeUtil.splitRemain(basicAuth, ':', 1);

        auth = {
          username: basicAuth[0],
          password: basicAuth[1]
        };
      }
    }

    return { url, auth };
  }

  // ----------------------------------------------------------------------------
  /**
   * Create a memcached client from the provided url
   * @param {string} url - The memcached url to connect to.
   * @param {string} section - The section this memcached client is being created for
   */
  static createMemcachedClient (url, section) {
    url = ArkimeUtil.sanitizeStr(url);
    // memcached://[user:pass@]server1[:11211],[user:pass@]server2[:11211],...
    if (url.startsWith('memcached://')) {
      if (ArkimeConfig.debug > 0) {
        console.log('MEMCACHED:', section, url);
      }
      const memjs = require('memjs');
      return memjs.Client.create(url.substring(12));
    }

    console.log(`${section} - Unknown memcached url '%s'`, url);
    process.exit(1);
  }

  // ----------------------------------------------------------------------------
  static createSQLiteDB (url) {
    const Database = require('better-sqlite3');
    let dbPath;
    if (url.startsWith('sqlite3://')) {
      dbPath = url.slice(10);
    } else if (url.startsWith('sqlite://')) {
      dbPath = url.slice(9);
    } else {
      dbPath = url;
    }

    const db = new Database(dbPath, { timeout: 5000 });
    // Processes opening a new db together can deadlock switching it to WAL, sqlite says busy without waiting
    for (let i = 0; ; i++) {
      try {
        db.pragma('journal_mode = WAL');
        break;
      } catch (err) {
        if (err.code !== 'SQLITE_BUSY' || i >= 50) { throw err; }
        Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 20 + Math.random() * 80);
      }
    }
    db.pragma('synchronous = NORMAL');
    return db;
  }

  // ----------------------------------------------------------------------------
  // Connections and collections
  // ----------------------------------------------------------------------------
  static #backends = new Map([
    ['http', './arkimeDb.es'],
    ['https', './arkimeDb.es'],
    ['elasticsearch', './arkimeDb.es'],
    ['opensearch', './arkimeDb.es'],
    ['sqlite', './arkimeDb.sqlite'],
    ['sqlite3', './arkimeDb.sqlite'],
    ['postgres', './arkimeDb.pg'],
    ['postgresql', './arkimeDb.pg']
  ]);

  static #connections = new Map();
  // The options that change what a connection talks to, so callers only share one when these match
  static #connOptions = ['prefix', 'node', 'apiKey', 'basicAuth', 'insecure', 'caTrustFile', 'clientKey', 'clientCert', 'clientKeyPass', 'requestTimeout'];

  /**
   * Register a backend for a url scheme
   * @param {string} scheme - The url scheme, without the ://
   * @param {string|function} backend - A module path or a Connection class
   */
  static register (scheme, backend) {
    ArkimeDb.#backends.set(scheme, backend);
  }

  /**
   * Get a connection for a url, connections are shared per url and connection options.
   * @param {string} url - The db url, the scheme picks the backend
   * @param {object} options - Backend options, prefix is used by all of them.
   *   For ES, client reuses an existing @elastic/elasticsearch client.
   */
  static connect (url, options = {}) {
    if (options.client) {
      return new (ArkimeDb.#backend('http'))(url, options);
    }

    const key = `${url}\t${JSON.stringify(ArkimeDb.#connOptions.map(o => options[o] ?? null))}`;
    let conn = ArkimeDb.#connections.get(key);
    if (!conn) {
      const scheme = url?.match(/^([a-z0-9+.-]+):/i)?.[1].toLowerCase();
      conn = new (ArkimeDb.#backend(scheme))(url, options);
      ArkimeDb.#connections.set(key, conn);
    }
    return conn;
  }

  /**
   * Is the url (or a missing url) an Elasticsearch/OpenSearch db. Apps default to ES
   * for anything they don't recognize, so only the other backends are checked.
   */
  static isES (url) {
    return !/^(lmdb|redis[a-z-]*|sqlite3?|postgres(ql)?):/i.test(url ?? '');
  }

  static #backend (scheme) {
    const backend = ArkimeDb.#backends.get(scheme);
    if (!backend) {
      if (/^(lmdb|redis)/.test(scheme)) { throw new Error(`${scheme}:// is no longer supported as a db, use sqlite:// or postgres://`); }
      throw new Error(`Unknown db url scheme '${scheme}'`);
    }
    return typeof backend === 'string' ? require(backend) : backend;
  }

  /**
   * Close and forget every cached connection
   */
  static async closeAll () {
    const conns = [...ArkimeDb.#connections.values()];
    ArkimeDb.#connections.clear();
    await Promise.all(conns.map(c => c.close?.()));
  }

  // ----------------------------------------------------------------------------
  // where/sort helpers shared by the backends
  // ----------------------------------------------------------------------------
  static #FIELD_RE = /^[A-Za-z_][A-Za-z0-9_]*(\.[A-Za-z_][A-Za-z0-9_]*)*$/;
  static #OPS = new Set(['$in', '$nin', '$ne', '$exists', '$gt', '$gte', '$lt', '$lte', '$contains', '$icontains', '$prefix', '$wildcard', '$regex', '$cidr']);
  static #regexCache = new Map();

  /**
   * A whole value match regex like ES regexp, RE2 so user patterns can't backtrack forever
   */
  static regex (pattern) {
    const key = `r\t${pattern}`;
    let re = ArkimeDb.#regexCache.get(key);
    if (!re) {
      const RE2 = require('re2');
      re = new RE2(`^(?:${pattern})$`, 's');
      if (ArkimeDb.#regexCache.size > 1000) { ArkimeDb.#regexCache.clear(); }
      ArkimeDb.#regexCache.set(key, re);
    }
    return re;
  }

  /**
   * Is the value an ip in cidr, or that exact ip when it has no /, like an ES term on an ip field
   */
  static ipInCidr (value, cidr) {
    const family = net.isIP(String(value));
    return family !== 0 && ArkimeDb.#cidrList(cidr).check(String(value), family === 6 ? 'ipv6' : 'ipv4');
  }

  static #cidrList (cidr) {
    const key = `c\t${cidr}`;
    let list = ArkimeDb.#regexCache.get(key);
    if (!list) {
      const [addr, bits, extra] = String(cidr).split('/');
      const family = net.isIP(addr);
      const max = family === 6 ? 128 : 32;
      if (!family || extra !== undefined || (bits !== undefined && !(/^\d+$/.test(bits) && +bits <= max))) {
        throw new Error(`Invalid db where $cidr '${cidr}'`);
      }
      list = new net.BlockList();
      if (bits === undefined) {
        list.addAddress(addr, family === 6 ? 'ipv6' : 'ipv4');
      } else {
        list.addSubnet(addr, +bits, family === 6 ? 'ipv6' : 'ipv4');
      }
      if (ArkimeDb.#regexCache.size > 1000) { ArkimeDb.#regexCache.clear(); }
      ArkimeDb.#regexCache.set(key, list);
    }
    return list;
  }

  /**
   * The values at a dotted path like ES indexes them, arrays at any level are flattened and nulls dropped
   */
  static pathValues (doc, field) {
    let vals = [doc];
    for (const part of field.split('.')) {
      vals = vals.flatMap(v => (v !== null && typeof v === 'object' && !Array.isArray(v)) ? [v[part]] : []).flat(Infinity);
    }
    return vals.filter(v => v !== null && v !== undefined);
  }

  static checkField (field) {
    if (typeof field !== 'string' || !ArkimeDb.#FIELD_RE.test(field)) {
      throw new Error(`Invalid db field name '${field}'`);
    }
    return field;
  }

  // ----------------------------------------------------------------------------
  // fields schema shared by the backends
  // ----------------------------------------------------------------------------
  static #fieldSpec (spec) {
    return typeof spec === 'string' ? { type: spec } : spec;
  }

  static #NUMERIC = new Set(['long', 'integer', 'short', 'byte', 'double', 'float', 'half_float', 'scaled_float', 'unsigned_long']);

  /**
   * The indexed fields with a numeric type, which sort the same by their column
   */
  static numericIndexedFields (options) {
    return ArkimeDb.indexedFields(options).filter(f => ArkimeDb.#NUMERIC.has(ArkimeDb.#fieldSpec(options.fields[f]).type));
  }

  /**
   * The numeric indexed fields marked indexed: 'sort', Postgres gives them a descending index too
   */
  static sortIndexedFields (options) {
    return ArkimeDb.numericIndexedFields(options).filter(f => ArkimeDb.#fieldSpec(options.fields[f]).indexed === 'sort');
  }

  /**
   * The indexed fields marked indexed: 'wildcard', Postgres gives them a trigram index too
   */
  static wildcardIndexedFields (options) {
    return ArkimeDb.indexedFields(options).filter(f => ArkimeDb.#fieldSpec(options.fields[f]).indexed === 'wildcard');
  }

  /**
   * The fields marked indexed, which the SQL backends give a column and index
   */
  static indexedFields (options) {
    return Object.entries(options.fields ?? {})
      .filter(([, spec]) => ArkimeDb.#fieldSpec(spec).indexed)
      .map(([field]) => {
        if (field === 'id') { throw new Error('id is always indexed'); }
        return ArkimeDb.checkField(field);
      });
  }

  /**
   * [field, type] for every field, typed collections give each its own column
   */
  static fieldTypes (options) {
    return Object.entries(options.fields ?? {}).map(([field, spec]) => [ArkimeDb.checkField(field), ArkimeDb.#fieldSpec(spec).type]);
  }

  static fieldColumn (field) {
    return `f_${field.replace(/\./g, '__')}`;
  }

  /**
   * fields as ES mapping properties, dotted names nest
   */
  static esProperties (fields = {}) {
    const properties = {};
    for (const [field, spec] of Object.entries(fields)) {
      const mapping = { ...ArkimeDb.#fieldSpec(spec) };
      delete mapping.indexed;
      const parts = ArkimeDb.checkField(field).split('.');
      let props = properties;
      for (const part of parts.slice(0, -1)) {
        props[part] ??= {};
        props[part].properties ??= {};
        props = props[part].properties;
      }
      const last = parts.at(-1);
      props[last] = { ...props[last], ...mapping };
    }
    return properties;
  }

  /**
   * Split a where condition into [op, value] pairs, plain values are $eq
   */
  static condOps (cond) {
    if (cond === null || typeof cond !== 'object' || Array.isArray(cond)) {
      return [['$eq', cond]];
    }
    // A Set, Map or {} would compile to no clause at all, which on an access filter means everything
    const proto = Object.getPrototypeOf(cond);
    const ops = (proto === Object.prototype || proto === null) ? Object.entries(cond) : [];
    if (ops.length === 0) {
      throw new Error('db where value must be a scalar, an array or an operator object');
    }
    for (const [op, value] of ops) {
      if (!ArkimeDb.#OPS.has(op)) {
        throw new Error(`Unknown db where operator '${op}'`);
      }
      if ((op === '$in' || op === '$nin') && !Array.isArray(value)) {
        throw new Error(`db where operator ${op} needs an array`);
      }
      if (op === '$cidr') { ArkimeDb.#cidrList(value); }
    }
    return ops;
  }

  /**
   * Normalize sort into [[field, 'asc'|'desc'], ...]
   */
  static sortList (sort) {
    if (!sort) { return []; }
    const list = Array.isArray(sort) ? sort : Object.entries(sort);
    return list.map(([field, order]) => [ArkimeDb.checkField(field), order === 'desc' ? 'desc' : 'asc']);
  }

  static checkMetrics (metrics = {}) {
    for (const metric of Object.values(metrics)) {
      const [op, field] = Object.entries(metric)[0] ?? [];
      if (!['min', 'max', 'sum'].includes(op)) { throw new Error(`Unknown db metric '${op}'`); }
      ArkimeDb.checkField(field);
    }
    return metrics;
  }

  static newId (idType) {
    return idType === 'hex' ? cryptoLib.randomBytes(16).toString('hex') : cryptoLib.randomUUID();
  }
}

/**
 * Thrown by put with createOnly when the id already exists
 */
class ArkimeDbConflictError extends Error {
  constructor (id, message = `Document '${id}' already exists`) {
    super(message);
    this.id = id;
    this.statusCode = 409;
  }
}

/**
 * A collection of json docs keyed by id. Every backend implements:
 *   init(), get(id), put(id, doc, { createOnly, refresh }), create(doc), update(id, fields, { refresh }),
 *   delete(id), deleteWhere(where), deleteAll(), find({ where, sort, from, size, version, fields }),
 *   count(where), getMeta(key), setMeta(obj), refresh(), lease(id, owner, ttlMs), nextSequence(id),
 *   aggregate({ where, groupBy, metrics: { name: { min|max|sum: field } }, size }),
 *   getVersioned(id), putIfVersion(id, doc, version)
 * aggregate returns [{ key, count, <metric names> }] like an ES terms aggregation, biggest first.
 * find returns { total, data: [{ id, doc, version? }] }, with no size it returns every match.
 * fields is a hint (ES only fetches those), callers still get whatever the backend returns.
 * update sets top level fields of an existing doc and returns false when there is no doc.
 * getVersioned returns { doc, version } or null, version is opaque. putIfVersion only writes
 * when the doc still has that version, else it throws a ConflictError, so it never recreates a deleted doc.
 * refresh: false skips the ES refresh after a write, other backends ignore it.
 *
 * Collection options:
 *   idType: 'uuid' (default) or 'hex' for generated ids
 *   version: true to keep a per doc version that every put bumps
 *   table: the table name for every SQL backend, defaults to the collection name
 *   fields: { name: 'keyword' | { type, indexed, ...es mapping } } - dotted names nest. ES builds the
 *     index mapping from them, SQLite and Postgres give indexed fields a column and index.
 *     Fields can be arrays like in ES, except indexed ones which must hold one value.
 *   typed: true for fixed shape docs, Postgres keeps each top level field in its own column instead
 *     of a json doc. Missing columns are added at init and a field with no column can't be written.
 *   es: { index, dbpl, create: { settings, mappings } } - the index is created from fields and create,
 *     unless dbpl is set, which means db.pl owns it
 *   sqlite: { table, idColumn, legacyMeta }, pg: { table } - per backend names/layout, only needed for older layouts
 */
class ArkimeDbCollection {
  name;
  options;

  constructor (collName, options = {}) {
    this.name = collName;
    this.options = options;
  }

  async init () {}
  async refresh () {}

  async create (doc) {
    return this.put(null, doc);
  }

  async deleteAll () {
    return this.deleteWhere(null);
  }

  async count (where) {
    return (await this.find({ where, size: 0 })).total;
  }

  newId () {
    return ArkimeDb.newId(this.options.idType);
  }
}

ArkimeDb.ConflictError = ArkimeDbConflictError;
ArkimeDb.Collection = ArkimeDbCollection;

module.exports = ArkimeDb;

// At end because of circular require
const ArkimeUtil = require('./arkimeUtil');
const ArkimeConfig = require('./arkimeConfig');
