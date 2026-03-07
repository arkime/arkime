/******************************************************************************/
/* Cache implementations
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const { LRUCache } = require('lru-cache');
const Bson = require('bson');
const BSON = new Bson();
const ArkimeUtil = require('./arkimeUtil');

/******************************************************************************/
// Base Cache
/******************************************************************************/
class ArkimeCache {
  #lru;
  constructor (options) {
    this.cacheSize = parseInt(options.cacheSize ?? 100000);
    this.cacheTimeout = ArkimeUtil.parseTimeStr(options.cacheTimeout ?? 24 * 60 * 60);
    this.#lru = new LRUCache({ max: this.cacheSize });
  }

  // ----------------------------------------------------------------------------
  has (key) {
    return this.#lru.has(key);
  }

  // ----------------------------------------------------------------------------
  async get (key) {
    return this.#lru.get(key);
  }

  // ----------------------------------------------------------------------------
  set (key, result) {
    this.#lru.set(key, result);
  }

  // ----------------------------------------------------------------------------
  /**
   * Create a new cache based on type and options
   * @param {string} options.type The type of cache: memory, redis, memcached, lmdb
   */
  static createCache (options) {
    switch (options.type) {
    case 'memory':
      return new ArkimeCache(options);
    case 'redis':
      return new ArkimeRedisCache(options);
    case 'memcached':
      return new ArkimeMemcachedCache(options);
    case 'lmdb':
      return new ArkimeLMDBCache(options);
    case 'sqlite':
    case 'sqlite3':
      return new ArkimeSQLiteCache(options);
    default:
      console.log('ERROR - Unknown cache type', options.type);
      process.exit(1);
    }
  }
}

module.exports = ArkimeCache;

/******************************************************************************/
// Redis Cache
/******************************************************************************/
class ArkimeRedisCache extends ArkimeCache {
  constructor (options) {
    super(options);

    this.client = ArkimeUtil.createRedisClient(options.getConfig('redisURL'), 'cache');
  }

  // ----------------------------------------------------------------------------
  async get (key) {
    if (super.has(key)) {
      return super.get(key);
    }

    return new Promise((resolve, reject) => {
      this.client.getBuffer(key, (err, reply) => {
        if (err || reply === null) {
          return resolve(undefined);
        }
        const bsonResult = BSON.deserialize(reply, { promoteBuffers: true });
        super.set(key, bsonResult);
        return resolve(bsonResult);
      });
    });
  }

  // ----------------------------------------------------------------------------
  set (key, result) {
    super.set(key, result);

    const data = BSON.serialize(result, false, true, false);
    this.client.setex(key, this.cacheTimeout, data);
  }
}

/******************************************************************************/
// Memcached Cache
/******************************************************************************/
class ArkimeMemcachedCache extends ArkimeCache {
  constructor (options) {
    super(options);

    this.client = ArkimeUtil.createMemcachedClient(options.getConfig('memcachedURL'), 'cache');
  }

  // ----------------------------------------------------------------------------
  async get (key) {
    if (super.has(key)) {
      return super.get(key);
    }

    return new Promise((resolve, reject) => {
      this.client.get(key, (err, reply) => {
        if (err || reply === null) {
          return resolve(undefined);
        }
        const bsonResult = BSON.deserialize(reply, { promoteBuffers: true });
        super.set(key, bsonResult); // Set memory cache
        return resolve(bsonResult);
      });
    });
  }

  // ----------------------------------------------------------------------------
  set (key, result) {
    super.set(key, result);

    const data = BSON.serialize(result, false, true, false);
    this.client.set(key, data, { expires: this.cacheTimeout }, () => {});
  }
}

/******************************************************************************/
// LMDB Cache
/******************************************************************************/
class ArkimeLMDBCache extends ArkimeCache {
  constructor (options) {
    super(options);

    // eslint-disable-next-line no-shadow
    const { open } = require('lmdb');

    const path = options.getConfig('lmdbDir');

    if (typeof (path) !== 'string') {
      console.log('ERROR - lmdbDir must be set');
      process.exit(1);
    }

    try {
      this.store = open({
        path,
        compression: true
      });
    } catch (err) {
      console.log('ERROR -', err);
      process.exit(1);
    }
  }

  // ----------------------------------------------------------------------------
  async get (key) {
    return this.store.get(key);
  }

  // ----------------------------------------------------------------------------
  set (key, result) {
    this.store.put(key, result);
  }
}

/******************************************************************************/
// SQLite Cache
/******************************************************************************/
class ArkimeSQLiteCache extends ArkimeCache {
  constructor (options) {
    super(options);

    const Database = require('better-sqlite3');
    let dbPath = options.getConfig('sqliteFile');

    if (typeof (dbPath) !== 'string') {
      console.log('ERROR - sqliteFile must be set for sqlite cache');
      process.exit(1);
    }

    try {
      this.db = new Database(dbPath);
      this.db.pragma('journal_mode = WAL');
      this.db.pragma('synchronous = NORMAL');
      this.db.pragma('busy_timeout = 5000');

      this.db.exec(`
        CREATE TABLE IF NOT EXISTS cache (
          key TEXT PRIMARY KEY,
          value BLOB NOT NULL,
          expires INTEGER NOT NULL
        )
      `);
      this.db.exec('CREATE INDEX IF NOT EXISTS cache_expires ON cache(expires)');
    } catch (err) {
      console.log('ERROR -', err);
      process.exit(1);
    }
  }

  // ----------------------------------------------------------------------------
  async get (key) {
    if (super.has(key)) {
      return super.get(key);
    }

    const row = this.db.prepare('SELECT value FROM cache WHERE key = ? AND expires > ?').get(key, Date.now() / 1000);
    if (!row) {
      return undefined;
    }

    const result = BSON.deserialize(row.value, { promoteBuffers: true });
    super.set(key, result);
    return result;
  }

  // ----------------------------------------------------------------------------
  set (key, result) {
    super.set(key, result);

    const data = BSON.serialize(result, false, true, false);
    const expires = Math.floor(Date.now() / 1000) + this.cacheTimeout;
    this.db.prepare('INSERT OR REPLACE INTO cache (key, value, expires) VALUES (?, ?, ?)').run(key, data, expires);
  }
}
