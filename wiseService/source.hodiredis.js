/******************************************************************************/
/* hodiredis - History of Observed Data Indictors
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
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
const redis = require('ioredis');

class HODIRedisSource extends WISESource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { dontCache: true, fullQuery: true });

    this.contentTypes = {};
    const contentTypes = this.api.getConfig(section, 'contentTypes',
      'application/x-dosexec,application/vnd.ms-cab-compressed,application/pdf,application/x-shockwave-flash,application/x-java-applet,application/jar').split(',').map(item => item.trim());

    contentTypes.forEach((type) => { this.contentTypes[type] = 1; });
    this.url = api.getConfig(section, 'url');
    if (this.url === undefined) {
      console.log(this.section, '- ERROR not loading since no url specified in config file');
      return;
    }

    this.client = redis.createClient({ url: this.url });
    this.api.addSource(section, this, ['domain', 'email', 'ip', 'md5']);

    const tagsField = this.api.addField('field:tags');
    this.tagsDomain = WISESource.encodeResult(tagsField, 'nbs-domain');
    this.tagsMd5 = WISESource.encodeResult(tagsField, 'nbs-md5');
    this.tagsEmail = WISESource.encodeResult(tagsField, 'nbs-email');
    this.tagsIp = WISESource.encodeResult(tagsField, 'nbs-ip');
  }

  // ----------------------------------------------------------------------------
  process (key, tag, cb) {
    const date = new Date();

    this.client.hsetnx(key, 'first', date.getTime(), (err, result) => {
      if (result === 1) {
        return cb(null, tag);
      } else {
        return cb(null, undefined);
      }
    });
    this.client.hset(key, 'last', date.getTime());
    this.client.hincrby(key, 'count', 1);
    this.client.hincrby(key, `count:${date.getFullYear()}:${date.getMonth() + 1}`, 1);
  };

  // ----------------------------------------------------------------------------
  getDomain (query, cb) {
    return this.process(`d:${query.value}`, this.tagsDomain, cb);
  };

  // ----------------------------------------------------------------------------
  getMd5 (query, cb) {
    if (query.contentType === undefined || this.contentTypes[query.contentType] !== 1) {
      return cb(null, undefined);
    }

    return this.process(`h:${query.value}`, this.tagsMd5, cb);
  };

  // ----------------------------------------------------------------------------
  getEmail (query, cb) {
    return this.process(`e:${query.value}`, this.tagsEmail, cb);
  };

  // ----------------------------------------------------------------------------
  getIp (query, cb) {
    return this.process(`a:${query.value}`, this.tagsIp, cb);
  };
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('hodiredis', {
    singleton: true,
    name: 'hodiredis',
    description: 'Experimental “History of Observed Data Indicators” plugin using Redis. This watches all queries to WISE and sends a feed to a configured elasticsearch cluster with firstSeen, lastSeen, and VERY rough count metric.',
    types: ['ip', 'domain', 'md5', 'email'],
    cacheable: false,
    fields: [
      { name: 'contentTypes', required: true, help: 'Comma separated list of contentTypes to store md5 results for' },
      { name: 'url', required: true, help: 'The format is [redis:]//[[user][:password@]]host:port[/db-number]' }
    ]
  });

  return new HODIRedisSource(api, 'hodiredis');
};
