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

var LRU = require('lru-cache')
  , redis = require('redis')
  , Bson = require('bson')
  , BSON = new Bson()
  ;


/******************************************************************************/
// Memory Cache
/******************************************************************************/

function WISEMemoryCache (options) {
  this.cacheSize = +options.cacheSize || 100000;
  this.cache = {};
}

//////////////////////////////////////////////////////////////////////////////////
WISEMemoryCache.prototype.get = function(query, cb) {
  var cache = this.cache[query.typeName];
  cb(null, cache?cache.get(query.value):undefined);
};

//////////////////////////////////////////////////////////////////////////////////
WISEMemoryCache.prototype.set = function(query, value) {
  var cache = this.cache[query.typeName];
  if (!cache) {
    cache = this.cache[query.typeName] = LRU({max: this.cacheSize});
  }
  cache.set(query.value, value);
};

exports.WISEMemoryCache = WISEMemoryCache;

/******************************************************************************/
// Redis Cache
/******************************************************************************/

function WISERedisCache (options) {
  options = options || {};
  this.cacheSize = +options.cacheSize || 10000;
  this.cache = {};

  options.return_buffers = true; // force buffers on for the bson decoding to work
  this.client = redis.createClient(options);
}

//////////////////////////////////////////////////////////////////////////////////
WISERedisCache.prototype.get = function(query, cb) {
  var value;

  // Check memory cache first
  var cache = this.cache[query.typeName];

  if (cache) {
    value = cache.get(query.value);
    if (value !== undefined) {
      return cb(null, value);
    }
  } else {
    cache = this.cache[query.typeName] = LRU({max: this.cacheSize});
  }

  // Check redis
  this.client.get(query.typeName + "-" + query.value, (err, reply) => {
    if (reply === null) {
      return cb(null, undefined);
    }
    value = BSON.deserialize(reply, {promoteBuffers: true});
    cb(null, value);

    cache.set(query.value, value); // Set memory cache
  });
};

//////////////////////////////////////////////////////////////////////////////////
WISERedisCache.prototype.set = function(query, value) {
  var cache = this.cache[query.typeName];

  if (!cache) {
    cache = this.cache[query.typeName] = LRU({max: this.cacheSize});
  }

  cache.set(query.value, value);

  var data = BSON.serialize(value, false, true, false);
  this.client.set(query.typeName + "-" + query.value, data);
};

exports.WISERedisCache = WISERedisCache;

/******************************************************************************/
// Load Cache
/******************************************************************************/
exports.createCache = function(options) {
  var type = options.getConfig("cache", "type", "memory");
  options.cacheSize = options.getConfig("cache", "cacheSize");

  switch (type) {
  case "memory":
    return new WISEMemoryCache(options);
  case "redis":
    options.url = options.getConfig("cache", "url");
    return new WISERedisCache(options);
  default:
    console.log("Unknown cache type", type);
    process.exit(1);
  }
};
