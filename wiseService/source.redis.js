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

const WISESource = require('./wiseSource.js');
const ArkimeUtil = require('../common/arkimeUtil');

class RedisSource extends WISESource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { tagsSetting: true, typeSetting: true, formatSetting: 'csv' });

    this.keyPath = api.getConfig(section, 'keyPath');
    this.column = +api.getConfig(section, 'column', 0);
    this.template = api.getConfig(section, 'template', undefined);
    this.redisMethod = api.getConfig(section, 'redisMethod', 'get');

    this.client = ArkimeUtil.createRedisClient(api.getConfig(section, 'redisURL'), section);

    if (this.client[this.redisMethod] === undefined) {
      console.log(`${this.section} - Unknown redisMethod ${this.redisMethod}`);
      return;
    }

    if (this.type === 'domain') {
      this[this.typeFunc] = RedisSource.prototype.fetchDomain;
    } else {
      this[this.typeFunc] = RedisSource.prototype.fetch;
    }

    this.api.addSource(this.section, this, [this.type]);
  }

  // ----------------------------------------------------------------------------
  fetch (key, cb) {
    if (this.template !== undefined) {
      key = this.template.replace('%key%', key).replace('%type%', this.type);
    }

    this.client[this.redisMethod](key, (err, reply) => {
      if (err) {
        console.log(`${this.section} -`, err);
      }

      if (reply === undefined || reply === null) {
        return cb(null, undefined);
      }

      let found = false;
      try {
        this.parse(reply, (ignorekey, result) => {
          found = true;
          const newresult = WISESource.combineResults([result, this.tagsResult]);
          return cb(null, newresult);
        }, (err) => {
          if (err) {
            console.log(`${this.section} -`, err);
            return cb(null, undefined);
          }
          if (!found) {
            console.log(`${this.section} - The keyPath ${this.keyPath} wasn't found even though document was returned`, reply);
            return cb(null, undefined);
          }
        });
      } catch (err) {
        if (err) {
          console.log(`${this.section} -`, err);
        }
      }
    });
  };

  // ----------------------------------------------------------------------------
  fetchDomain (key, cb) {
    this.fetch(key, (err, result) => {
      if (result === undefined) {
        return this.fetch(key.substring(key.indexOf('.') + 1), cb);
      }
      return cb(err, result);
    });
  };
}
// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('redis', {
    singleton: false,
    name: 'redis',
    description: 'Link to the redis data',
    link: 'https://arkime.com/wise#redis',
    fields: [
      { name: 'redisURL', password: true, required: false, help: 'Format is redis://[:password@]host:port/db-number, redis-sentinel://[[sentinelPassword]:[password]@]host[:port]/redis-name/db-number, or redis-cluster://[:password@]host:port/db-number' },
      { name: 'type', required: true, help: 'The wise query type this source supports' },
      { name: 'tags', required: false, help: 'Comma separated list of tags to set for matches', regex: '^[-a-z0-9,]+' },
      { name: 'format', required: false, help: 'The format data is in: csv (default), tagger, or json', regex: '^(csv|tagger|json)$' },
      { name: 'column', required: false, help: 'The numerical column number to use as the key', regex: '^[0-9]*$', ifField: 'format', ifValue: 'csv' },
      { name: 'keyPath', required: true, help: 'The path of what field to use as the key', ifField: 'format', ifValue: 'json' },
      { name: 'template', required: true, help: 'The template when forming the key name. %key% = the key being looked up, %type% = the type being looked up' },
      { name: 'redisMethod', required: false, help: 'The lowercase redis method to retrieve values, defaults to "get"' }
    ]
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^redis:/); });
  sections.forEach((section) => {
    return new RedisSource(api, section);
  });
};
// ----------------------------------------------------------------------------
