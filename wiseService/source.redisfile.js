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

const SimpleSource = require('./simpleSource.js');

class RedisFileSource extends SimpleSource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { reload: true });
    this.url = api.getConfig(section, 'url');
    if (this.url === undefined) {
      console.log(this.section, '- ERROR not loading since no url specified in config file');
      return;
    }
    this.key = api.getConfig(section, 'key');
    this.client = api.createRedisClient(api.getConfig(section, 'redisURL'), section);

    if (this.key === undefined) {
      console.log(this.section, '- ERROR not loading since no key specified in config file');
      delete this.client;
      return;
    }
  }

  // ----------------------------------------------------------------------------
  simpleSourceLoad (cb) {
    if (this.client && this.key) {
      this.client.get(this.key, cb);
    }
  }

  // ----------------------------------------------------------------------------
  getSourceRaw (cb) {
    this.client.get(this.key, cb);
  }

  // ----------------------------------------------------------------------------
  putSourceRaw (file, cb) {
    this.client.set(this.key, file, cb);
  }
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('redisfile', {
    singleton: false,
    name: 'redisfile',
    description: 'Like the url source, use a single redis key as the file. The redis key can be periodically reloaded.',
    cacheable: false,
    editable: true,
    fields: [
      { name: 'redisURL', password: true, required: false, ifField: 'type', ifValue: 'redis', help: 'Format is redis://[:password@]host:port/db-number, redis-sentinel://[[sentinelPassword]:[password]@]host[:port]/redis-name/db-number, or redis-cluster://[:password@]host:port/db-number' },
      { name: 'type', required: true, help: 'The wise query type this source supports' },
      { name: 'tags', required: false, help: 'Comma separated list of tags to set for matches', regex: '^[-a-z0-9,]+' },
      { name: 'format', required: false, help: 'The format data is in: csv (default), tagger, or json', regex: '^(csv|tagger|json)$' },
      { name: 'column', required: false, help: 'The numerical column number to use as the key', regex: '^[0-9]*$', ifField: 'format', ifValue: 'csv' },
      { name: 'arrayPath', required: false, help: "The path of where to find the array, if the json result isn't an array", ifField: 'format', ifValue: 'json' },
      { name: 'keyPath', required: false, help: 'The path of what field to use as the key', ifField: 'format', ifValue: 'json' },
      { name: 'key', required: true, help: 'The key in redis to fetch' },
      { name: 'reload', required: false, help: 'How often in minutes to refresh the file, or -1 (default) to never refresh it' }
    ]
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^redisfile:/); });
  sections.forEach((section) => {
    return new RedisFileSource(api, section);
  });
};
// ----------------------------------------------------------------------------
