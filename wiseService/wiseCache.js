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

/******************************************************************************/
// Memory Cache
/******************************************************************************/

function WISEMemoryCache (options) {
  this.cacheSize = +options.cacheSize || 100000;
  this.cache = {};
}

// ----------------------------------------------------------------------------
WISEMemoryCache.prototype.get = function (query, cb) {
  const cache = this.cache[query.typeName];
  cb(null, cache ? cache.get(query.value) : undefined);
};

// ----------------------------------------------------------------------------
WISEMemoryCache.prototype.set = function (query, result) {
  let cache = this.cache[query.typeName];
  if (!cache) {
    cache = this.cache[query.typeName] = LRU({ max: this.cacheSize });
  }
  cache.set(query.value, result);
};

exports.WISEMemoryCache = WISEMemoryCache;

/******************************************************************************/
// Redis Cache
/******************************************************************************/

function WISERedisCache (redisType, options) {
  options = options || {};
  this.cacheSize = +options.cacheSize || 10000;
  this.cacheTimeout = options.getConfig('cache', 'cacheTimeout') * 60 || 24 * 60 * 60;
  this.cache = {};

  this.client = options.createRedisClient(redisType, 'cache');
}

// ----------------------------------------------------------------------------
WISERedisCache.prototype.get = function (query, cb) {
  // Check memory cache first
  let cache = this.cache[query.typeName];

  if (cache) {
    const result = cache.get(query.value);
    if (result !== undefined) {
      return cb(null, result);
    }
  } else {
    cache = this.cache[query.typeName] = LRU({ max: this.cacheSize });
  }

  // Check redis
  this.client.getBuffer(query.typeName + '-' + query.value, (err, reply) => {
    if (reply === null) {
      return cb(null, undefined);
    }
    const result = BSON.deserialize(reply, { promoteBuffers: true });

    for (const source in result) {
      // Redis uses old encoding, convert old to new when needed
      if (!Buffer.isBuffer(result[source].result)) {
        const newResult = Buffer.allocUnsafe(result[source].result.buffer.length + 1);
        newResult[0] = result[source].result.num;
        result[source].result.buffer.copy(newResult, 1);
        result[source].result = newResult;
      }
    }
    cb(null, result);

    cache.set(query.value, result); // Set memory cache
  });
};

// ----------------------------------------------------------------------------
WISERedisCache.prototype.set = function (query, result) {
  let cache = this.cache[query.typeName];

  if (!cache) {
    cache = this.cache[query.typeName] = LRU({ max: this.cacheSize });
  }

  cache.set(query.value, result);

  // Redis uses old encoding, convert new to old
  const newResult = {};
  for (const source in result) {
    newResult[source] = { ts: result[source].ts, result: { num: result[source].result[0], buffer: result[source].result.slice(1) } };
  }

  const data = BSON.serialize(newResult, false, true, false);
  this.client.setex(query.typeName + '-' + query.value, this.cacheTimeout, data);
};

exports.WISERedisCache = WISERedisCache;

/******************************************************************************/
// Load Cache
/******************************************************************************/
exports.createCache = function (options) {
  const type = options.getConfig('cache', 'type', 'memory');
  options.cacheSize = options.getConfig('cache', 'cacheSize');

  switch (type) {
  case 'memory':
    return new WISEMemoryCache(options);
  case 'redis':
  case 'redis-cluster':
  case 'redis-sentinel':
    return new WISERedisCache(type, options);
  default:
    console.log('Unknown cache type', type);
    process.exit(1);
  }
};
