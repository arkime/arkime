/******************************************************************************/
/* Cache implementations
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
    this.cacheTimeout = parseInt(options.cacheTimeout ?? 24 * 60 * 60);
    this.cache = {};
  }

  // ----------------------------------------------------------------------------
  get (query, cb) {
    const cache = this.cache[query.typeName];

    // promise version
    if (!cb) {
      return new Promise((resolve, reject) => {
        resolve(cache ? cache.get(query.value) : undefined);
      });
    }

    cb(null, cache ? cache.get(query.value) : undefined);
  }

  // ----------------------------------------------------------------------------
  set (query, result) {
    let cache = this.cache[query.typeName];
    if (!cache) {
      cache = this.cache[query.typeName] = LRU({ max: this.cacheSize });
    }
    cache.set(query.value, result);
  }

  static createCache (options) {
    switch (options.type) {
    case 'memory':
      return new ArkimeCache(options);
    case 'redis':
      return new ArkimeRedisCache(options);
    case 'memcached':
      return new ArkimeMemcachedCache(options);
    default:
      console.log('Unknown cache type', options.type);
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
  get (query, cb) {
    // Convert promise to cb by calling ourselves
    if (!cb) {
      return new Promise((resolve, reject) => {
        this.get(query, (err, data) => {
          if (err) {
            reject(err);
          } else {
            resolve(data);
          }
        });
      });
    }

    // Check memory cache first
    super.get(query, (err, result) => {
      if (err || result) {
        return cb(err, result);
      }

      // Check redis
      this.client.getBuffer(query.typeName + '-' + query.value, (err, reply) => {
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
        super.set(query.value, bsonResult); // Set memory cache
        cb(null, bsonResult);
      });
    });
  };

  // ----------------------------------------------------------------------------
  set (query, result) {
    super.set(query, result);

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
    this.client.setex(query.typeName + '-' + query.value, this.cacheTimeout, data);
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
  get (query, cb) {
    // Convert promise to cb by calling ourselves
    if (!cb) {
      return new Promise((resolve, reject) => {
        this.get(query, (err, data) => {
          if (err) {
            reject(err);
          } else {
            resolve(data);
          }
        });
      });
    }

    // Check memory cache first
    super.get(query, (err, result) => {
      if (err || result) {
        return cb(err, result);
      }

      // Check memcache
      this.client.get(query.typeName + '-' + query.value, (err, reply) => {
        if (err || reply === null) {
          return cb(err, undefined);
        }
        const bsonResult = BSON.deserialize(reply, { promoteBuffers: true });
        super.set(query.value, bsonResult); // Set memory cache
        cb(null, bsonResult);
      });
    });
  };

  // ----------------------------------------------------------------------------
  set (query, result) {
    super.set(query, result);

    const data = BSON.serialize(result, false, true, false);
    this.client.set(query.typeName + '-' + query.value, data, { expires: this.cacheTimeout }, () => {});
  };
};
