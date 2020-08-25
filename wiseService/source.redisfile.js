/******************************************************************************/
/*
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

var util = require('util');
var simpleSource = require('./simpleSource.js');

// ----------------------------------------------------------------------------
function RedisFileSource (api, section) {
  RedisFileSource.super_.call(this, api, section);
  this.url = api.getConfig(section, 'url');
  if (this.url === undefined) {
    console.log(this.section, '- ERROR not loading since no url specified in config file');
    return;
  }
  this.key = api.getConfig(section, 'key');
  this.reload = +api.getConfig(section, 'reload', -1);
  this.headers = {};
  var headers = api.getConfig(section, 'headers');
  this.cacheTimeout = -1;
  this.client = api.createRedisClient(api.getConfig(section, 'redisType', 'redis'), section);

  if (this.key === undefined) {
    console.log(this.section, '- ERROR not loading since no key specified in config file');
    return;
  }

  if (headers) {
    headers.split(';').forEach((header) => {
      var parts = header.split(':').map(item => item.trim());
      if (parts.length === 2) {
        this.headers[parts[0]] = parts[1];
      }
    });
  }

  if (!this.initSimple()) {
    return;
  }

  setImmediate(this.load.bind(this));

  // Reload key every so often
  if (this.reload > 0) {
    setInterval(this.load.bind(this), this.reload * 1000 * 60);
  }
}
util.inherits(RedisFileSource, simpleSource);
// ----------------------------------------------------------------------------
RedisFileSource.prototype.simpleSourceLoad = function (setFunc, cb) {
  this.client.get(this.key, (error, reply) => {
    if (reply === null) {
      cb(error);
    } else {
      this.parse(reply, setFunc, cb);
    }
  });
};
// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('redisfile', {
    singleton: false,
    name: 'redisfile',
    description: 'Like the file source, but fetch the file from redis instead of the file system',
    cacheable: false,
    fields: [
      { name: 'type', required: true, help: 'The wise query type this source supports' },
      { name: 'tags', required: true, help: 'Comma separated list of tags to set for matches', regex: '^[-a-z0-9,]+' },
      { name: 'format', required: true, help: 'The format of data, such as csv, tagger, or json', regex: '^(csv|tagger|json)$' },
      { name: 'column', required: false, help: 'For CSV formated data, this is the numerical column number to use as the key', regex: '^[0-9]*$' },
      { name: 'keyColumn', required: false, help: 'For JSON formated data, this is the path of what field to use as the key' },
      { name: 'key', required: true, help: 'The key in redis to fetch' },
      { name: 'url', required: true, help: 'The format is [redis:]//[[user][:password@]]host:port[/db-number]' },
      { name: 'redisType', required: true, help: 'The type of redis cluster:redis,redis-sentinel,redis-cluster' }
    ]
  });

  var sections = api.getConfigSections().filter((e) => { return e.match(/^redisfile:/); });
  sections.forEach((section) => {
    return new RedisFileSource(api, section);
  });
};
// ----------------------------------------------------------------------------
