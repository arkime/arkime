/******************************************************************************/
/* Cache implementations
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const LRU = require('lru-cache');
const Bson = require('bson');
const BSON = new Bson();
const ArkimeUtil = require('../common/arkimeUtil');

/******************************************************************************/
// Base Cache
/******************************************************************************/
class ArkimeCache {
  constructor (options) {
    this.cacheSize = parseInt(options.cacheSize ?? 100000);
    this.cacheTimeout = ArkimeUtil.parseTimeStr(options.cacheTimeout ?? 24 * 60 * 60);
    this.cache = LRU({ max: this.cacheSize });
  }

  // ----------------------------------------------------------------------------
  get (key, cb) {
    // promise version
    if (!cb) {
      return new Promise((resolve, reject) => {
        resolve(this.cache.get(key));
      });
    }

    cb(null, this.cache.get(key));
  }

  // ----------------------------------------------------------------------------
  set (key, result) {
    this.cache.set(key, result);
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
    default:
      console.log('ERROR - Unknown cache type', options.type);
      process.exit(1);
    }
  };
}

module.exports = ArkimeCache;

/******************************************************************************/
// Redis Cache
/******************************************************************************/
class ArkimeRedisCache extends ArkimeCache {
  constructor (options) {
    super(options);

    this.redisFormat = parseInt(options.getConfig('redisFormat', '2'));
    if (this.redisFormat !== 3) {
      this.redisFormat = 2;
    }
    this.client = ArkimeUtil.createRedisClient(options.getConfig('redisURL'), 'cache');
  }

  // ----------------------------------------------------------------------------
  get (key, cb) {
    // Convert promise to cb by calling ourselves
    if (!cb) {
      return new Promise((resolve, reject) => {
        this.get(key, (err, data) => {
          if (err) {
            reject(err);
          } else {
            resolve(data);
          }
        });
      });
    }

    // Check memory cache first
    super.get(key, (err, result) => {
      if (err || result) {
        return cb(err, result);
      }

      // Check redis
      this.client.getBuffer(key, (err, reply) => {
        if (err || reply === null) {
          return cb(null, undefined);
        }
        const bsonResult = BSON.deserialize(reply, { promoteBuffers: true });

        for (const source in bsonResult) {
          // Redis uses old encoding, convert old to new when needed
          if (!Buffer.isBuffer(bsonResult[source].result)) {
            const newResult = Buffer.allocUnsafe(bsonResult[source].result.buffer.length + 1);
            newResult[0] = bsonResult[source].result.num;
            bsonResult[source].result.buffer.copy(newResult, 1);
            bsonResult[source].result = newResult;
          }
        }
        super.set(key, bsonResult); // Set memory cache
        cb(null, bsonResult);
      });
    });
  };

  // ----------------------------------------------------------------------------
  set (key, result) {
    super.set(key, result);

    let newResult;
    if (this.redisFormat === 3) {
      newResult = result;
    } else {
      // Redis uses old encoding, convert new to old
      newResult = {};
      for (const source in result) {
        newResult[source] = { ts: result[source].ts, result: { num: result[source].result[0], buffer: result[source].result.slice(1) } };
      }
    }

    const data = BSON.serialize(newResult, false, true, false);
    this.client.setex(key, this.cacheTimeout, data);
  };
};

/******************************************************************************/
// Memcached Cache
/******************************************************************************/
class ArkimeMemcachedCache extends ArkimeCache {
  constructor (options) {
    super(options);

    this.client = ArkimeUtil.createMemcachedClient(options.getConfig('memcachedURL'), 'cache');
  }

  // ----------------------------------------------------------------------------
  get (key, cb) {
    // Convert promise to cb by calling ourselves
    if (!cb) {
      return new Promise((resolve, reject) => {
        this.get(key, (err, data) => {
          if (err) {
            reject(err);
          } else {
            resolve(data);
          }
        });
      });
    }

    // Check memory cache first
    super.get(key, (err, result) => {
      if (err || result) {
        return cb(err, result);
      }

      // Check memcache
      this.client.get(key, (err, reply) => {
        if (err || reply === null) {
          return cb(err, undefined);
        }
        const bsonResult = BSON.deserialize(reply, { promoteBuffers: true });
        super.set(key, bsonResult); // Set memory cache
        cb(null, bsonResult);
      });
    });
  };

  // ----------------------------------------------------------------------------
  set (key, result) {
    super.set(key, result);

    const data = BSON.serialize(result, false, true, false);
    this.client.set(key, data, { expires: this.cacheTimeout }, () => {});
  };
};

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
  get (key, cb) {
    if (!cb) {
      return this.store.get(key);
    }

    return new Promise((resolve, reject) => {
      this.store.get(key)
        .then(data => cb(null, data))
        .catch(err => cb(err, null));
    });
  }

  // ----------------------------------------------------------------------------
  set (key, result) {
    this.store.put(key, result);
  };
};
