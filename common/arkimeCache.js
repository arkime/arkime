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

    this.prefix = `cache:${options.name ?? 'default'}:`;
    this.client = ArkimeUtil.createRedisClient(options.getConfig('redisURL'), 'cache');
  }

  // ----------------------------------------------------------------------------
  async get (key) {
    if (super.has(key)) {
      return super.get(key);
    }

    return new Promise((resolve, reject) => {
      this.client.getBuffer(this.prefix + key, (err, reply) => {
        if (err || reply === null) {
          return resolve(undefined);
        }
        try {
          const bsonResult = BSON.deserialize(reply, { promoteBuffers: true });
          super.set(key, bsonResult);
          return resolve(bsonResult);
        } catch (e) {
          return resolve(undefined);
        }
      });
    });
  }

  // ----------------------------------------------------------------------------
  set (key, result) {
    super.set(key, result);

    const data = BSON.serialize(result, false, true, false);
    this.client.setex(this.prefix + key, this.cacheTimeout, data);
  }
}

/******************************************************************************/
// Memcached Cache
/******************************************************************************/
class ArkimeMemcachedCache extends ArkimeCache {
  constructor (options) {
    super(options);

    this.prefix = `cache:${options.name ?? 'default'}:`;
    this.client = ArkimeUtil.createMemcachedClient(options.getConfig('memcachedURL'), 'cache');
  }

  // ----------------------------------------------------------------------------
  async get (key) {
    if (super.has(key)) {
      return super.get(key);
    }

    return new Promise((resolve, reject) => {
      this.client.get(this.prefix + key, (err, reply) => {
        if (err || reply === null) {
          return resolve(undefined);
        }
        try {
          const bsonResult = BSON.deserialize(reply, { promoteBuffers: true });
          super.set(key, bsonResult); // Set memory cache
          return resolve(bsonResult);
        } catch (e) {
          return resolve(undefined);
        }
      });
    });
  }

  // ----------------------------------------------------------------------------
  set (key, result) {
    super.set(key, result);

    const data = BSON.serialize(result, false, true, false);
    this.client.set(this.prefix + key, data, { expires: this.cacheTimeout }, () => {});
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

    const dbPath = options.getConfig('sqliteFile');

    if (typeof (dbPath) !== 'string') {
      console.log('ERROR - sqliteFile must be set for sqlite cache');
      process.exit(1);
    }

    this.tableName = `cache_${(options.name ?? 'default').replace(/[^a-zA-Z0-9_]/g, '_')}`;

    try {
      this.db = ArkimeUtil.createSQLiteDB(dbPath);

      this.db.exec(`
        CREATE TABLE IF NOT EXISTS ${this.tableName} (
          key TEXT PRIMARY KEY,
          value BLOB NOT NULL,
          expires INTEGER NOT NULL
        )
      `);
      this.db.exec(`CREATE INDEX IF NOT EXISTS ${this.tableName}_expires ON ${this.tableName}(expires)`);

      // Clean up expired entries every minute
      const cleanupInterval = setInterval(() => {
        try {
          this.db.prepare(`DELETE FROM ${this.tableName} WHERE expires <= ?`).run(Math.floor(Date.now() / 1000));
        } catch (e) { }
      }, 60 * 1000);
      cleanupInterval.unref();
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

    const row = this.db.prepare(`SELECT value FROM ${this.tableName} WHERE key = ? AND expires > ?`).get(key, Date.now() / 1000);
    if (!row) {
      return undefined;
    }

    try {
      const result = BSON.deserialize(row.value, { promoteBuffers: true });
      super.set(key, result);
      return result;
    } catch (e) {
      return undefined;
    }
  }

  // ----------------------------------------------------------------------------
  set (key, result) {
    super.set(key, result);

    const data = BSON.serialize(result, false, true, false);
    const expires = Math.floor(Date.now() / 1000) + this.cacheTimeout;
    this.db.prepare(`INSERT OR REPLACE INTO ${this.tableName} (key, value, expires) VALUES (?, ?, ?)`).run(key, data, expires);
  }
}
