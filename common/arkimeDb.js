/* arkimeDb.js  -- Database connection helpers, use through ArkimeUtil
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const cryptoLib = require('crypto');

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
    url = url.replace(/^elasticsearch/, 'http').replace(/^opensearch/, 'http');
    let auth;
    if (url.includes('://usersElasticsearch')) {
      let es, basicAuth;

      if (ArkimeConfig.get('usersElasticsearch')) {
        es = ArkimeConfig.get('usersElasticsearch');
        basicAuth = ArkimeConfig.get('usersElasticsearchBasicAuth');
      }

      if (!es) {
        console.log(`ERROR - No usersElasticsearch defined but used in ${url}`);
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
  /**
   * Create a LMDB store from the provided url
   * @param {string} url - The LMDB url to connect to.
   * @param {string} section - The section this LMDB client is being created for
   */
  static createLMDBStore (url, section) {
    // eslint-disable-next-line no-shadow
    const { open } = require('lmdb');

    try {
      const root = open({
        path: url.slice(7),
        compression: true
      });
      if (section) {
        return root.openDB(section);
      }
      return root;
    } catch (err) {
      console.log('ERROR -', err);
      process.exit(1);
    }
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

    const db = new Database(dbPath);
    db.pragma('journal_mode = WAL');
    db.pragma('synchronous = NORMAL');
    db.pragma('busy_timeout = 5000');
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
    ['lmdb', './arkimeDb.lmdb'],
    ['redis', './arkimeDb.redis'],
    ['rediss', './arkimeDb.redis'],
    ['redis-sentinel', './arkimeDb.redis'],
    ['redis-cluster', './arkimeDb.redis'],
    ['postgres', './arkimeDb.pg'],
    ['postgresql', './arkimeDb.pg']
  ]);

  static #connections = new Map();

  /**
   * Register a backend for a url scheme
   * @param {string} scheme - The url scheme, without the ://
   * @param {string|function} backend - A module path or a Connection class
   */
  static register (scheme, backend) {
    ArkimeDb.#backends.set(scheme, backend);
  }

  /**
   * Get a connection for a url, connections are shared per url and prefix.
   * @param {string} url - The db url, the scheme picks the backend
   * @param {object} options - Backend options, prefix is used by all of them.
   *   For ES, client reuses an existing @elastic/elasticsearch client.
   */
  static connect (url, options = {}) {
    if (options.client) {
      return new (ArkimeDb.#backend('http'))(url, options);
    }

    const key = `${url}\t${options.prefix ?? ''}`;
    let conn = ArkimeDb.#connections.get(key);
    if (!conn) {
      const scheme = url?.match(/^([a-z0-9+.-]+):/i)?.[1].toLowerCase();
      conn = new (ArkimeDb.#backend(scheme))(url, options);
      ArkimeDb.#connections.set(key, conn);
    }
    return conn;
  }

  static #backend (scheme) {
    const backend = ArkimeDb.#backends.get(scheme);
    if (!backend) {
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
  static #OPS = new Set(['$in', '$nin', '$ne', '$exists', '$gt', '$gte', '$lt', '$lte', '$contains', '$icontains', '$prefix']);

  static checkField (field) {
    if (typeof field !== 'string' || !ArkimeDb.#FIELD_RE.test(field)) {
      throw new Error(`Invalid db field name '${field}'`);
    }
    return field;
  }

  /**
   * Split a where condition into [op, value] pairs, plain values are $eq
   */
  static condOps (cond) {
    if (cond === null || typeof cond !== 'object' || Array.isArray(cond)) {
      return [['$eq', cond]];
    }
    const ops = Object.entries(cond);
    for (const [op, value] of ops) {
      if (!ArkimeDb.#OPS.has(op)) {
        throw new Error(`Unknown db where operator '${op}'`);
      }
      if ((op === '$in' || op === '$nin') && !Array.isArray(value)) {
        throw new Error(`db where operator ${op} needs an array`);
      }
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

  static getPath (doc, field) {
    let value = doc;
    for (const part of field.split('.')) {
      if (value === null || typeof value !== 'object') { return undefined; }
      value = value[part];
    }
    return value;
  }

  static #values (value) {
    if (value === undefined || value === null) { return []; }
    return Array.isArray(value) ? value.filter(v => v !== null) : [value];
  }

  static #matchOp (vals, op, c) {
    switch (op) {
    case '$eq': return vals.some(v => v === c);
    case '$ne': return !vals.some(v => v === c);
    case '$in': return vals.some(v => c.includes(v));
    case '$nin': return !vals.some(v => c.includes(v));
    case '$exists': return (vals.length > 0) === !!c;
    case '$gt': return vals.some(v => typeof v === typeof c && v > c);
    case '$gte': return vals.some(v => typeof v === typeof c && v >= c);
    case '$lt': return vals.some(v => typeof v === typeof c && v < c);
    case '$lte': return vals.some(v => typeof v === typeof c && v <= c);
    case '$contains': return vals.some(v => typeof v === 'string' && v.includes(c));
    case '$icontains': return vals.some(v => typeof v === 'string' && v.toLowerCase().includes(String(c).toLowerCase()));
    case '$prefix': return vals.some(v => typeof v === 'string' && v.startsWith(c));
    }
    return false;
  }

  /**
   * Does a doc match a where, used by the backends that scan
   */
  static matches (doc, id, where) {
    if (!where) { return true; }
    for (const [key, cond] of Object.entries(where)) {
      if (key === '$or') {
        if (!cond.some(w => ArkimeDb.matches(doc, id, w))) { return false; }
      } else if (key === '$and') {
        if (!cond.every(w => ArkimeDb.matches(doc, id, w))) { return false; }
      } else if (key === '$not') {
        if (ArkimeDb.matches(doc, id, cond)) { return false; }
      } else {
        const vals = ArkimeDb.#values(key === 'id' ? id : ArkimeDb.getPath(doc, ArkimeDb.checkField(key)));
        for (const [op, c] of ArkimeDb.condOps(cond)) {
          if (!ArkimeDb.#matchOp(vals, op, c)) { return false; }
        }
      }
    }
    return true;
  }

  /**
   * Filter, sort and page [{ id, doc }] in memory, missing values sort last
   */
  static findInMemory (items, { where, sort, from = 0, size } = {}) {
    let hits = items.filter(h => ArkimeDb.matches(h.doc, h.id, where));
    const total = hits.length;

    const sortList = ArkimeDb.sortList(sort);
    if (sortList.length) {
      const key = (h, field) => {
        const v = field === 'id' ? h.id : ArkimeDb.getPath(h.doc, field);
        return Array.isArray(v) ? v[0] : v;
      };
      hits.sort((a, b) => {
        for (const [field, order] of sortList) {
          const va = key(a, field);
          const vb = key(b, field);
          if (va === vb) { continue; }
          if (va === undefined || va === null) { return 1; }
          if (vb === undefined || vb === null) { return -1; }
          const cmp = va < vb ? -1 : (va > vb ? 1 : 0);
          if (cmp) { return order === 'desc' ? -cmp : cmp; }
        }
        return 0;
      });
    }

    hits = hits.slice(from, size === undefined ? undefined : from + size);
    return { total, data: hits };
  }

  static newId (idType) {
    return idType === 'hex' ? cryptoLib.randomBytes(16).toString('hex') : cryptoLib.randomUUID();
  }
}

/**
 * Thrown by put with createOnly when the id already exists
 */
class ArkimeDbConflictError extends Error {
  constructor (id) {
    super(`Document '${id}' already exists`);
    this.id = id;
  }
}

/**
 * A collection of json docs keyed by id. Every backend implements:
 *   init(), get(id), put(id, doc, { createOnly, refresh }), create(doc), update(id, fields, { refresh }),
 *   delete(id), deleteWhere(where), deleteAll(), find({ where, sort, from, size, version, fields }),
 *   count(where), getMeta(key), setMeta(obj), refresh()
 * find returns { total, data: [{ id, doc, version? }] }, with no size it returns every match.
 * fields is a hint (ES only fetches those), callers still get whatever the backend returns.
 * update sets top level fields of an existing doc and returns false when there is no doc.
 * refresh: false skips the ES refresh after a write, other backends ignore it.
 *
 * Collection options:
 *   idType: 'uuid' (default) or 'hex' for generated ids
 *   version: true to keep a per doc version that every put bumps
 *   table: the table name for every SQL backend, defaults to the collection name
 *   es: { index, create: { settings, mappings } }, sqlite: { table, columns }, pg: { table },
 *   lmdb: { db, json }, redis: { prefix } - per backend names/layout, only needed for older layouts
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

/**
 * Base for backends with no query language, subclasses implement
 * scan() -> [{ id, doc, version? }], get, put, delete, getMeta and setMeta
 */
class ArkimeDbScanCollection extends ArkimeDbCollection {
  async find (options = {}) {
    return ArkimeDb.findInMemory(await this.scan(), options);
  }

  async deleteWhere (where) {
    const { data } = ArkimeDb.findInMemory(await this.scan(), { where });
    for (const { id } of data) {
      await this.delete(id);
    }
    return data.length;
  }

  async update (id, fields) {
    const doc = await this.get(id);
    if (!doc) { return false; }
    await this.put(id, Object.assign(doc, fields));
    return true;
  }
}

ArkimeDb.ConflictError = ArkimeDbConflictError;
ArkimeDb.Collection = ArkimeDbCollection;
ArkimeDb.ScanCollection = ArkimeDbScanCollection;

module.exports = ArkimeDb;

// At end because of circular require
const ArkimeUtil = require('./arkimeUtil');
const ArkimeConfig = require('./arkimeConfig');
