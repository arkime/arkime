/******************************************************************************/
/* hodi - History of Observed Data Indictors
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
const elasticsearch = require('elasticsearch');
const LRU = require('lru-cache');

class HODISource extends WISESource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { fullQuery: true });

    this.contentTypes = {};
    const contentTypes = this.api.getConfig(section, 'contentTypes',
      'application/x-dosexec,application/vnd.ms-cab-compressed,application/pdf,application/x-shockwave-flash,application/x-java-applet,application/jar').split(',').map(item => item.trim());

    contentTypes.forEach((type) => { this.contentTypes[type] = 1; });

    this.esHost = api.getConfig('hodi', 'esHost');
    this.bulk = [];

    if (this.esHost === undefined) {
      console.log(this.section, '- No esHost defined');
      return;
    }

    this.domain = LRU({
      max: this.api.getConfig('hodi', 'cacheSize', 100000),
      maxAge: 1000 * 60 * +this.api.getConfig('hodi', 'cacheAgeMin', '5')
    });
    this.ip = LRU({
      max: this.api.getConfig('hodi', 'cacheSize', 100000),
      maxAge: 1000 * 60 * +this.api.getConfig('hodi', 'cacheAgeMin', '5')
    });
    this.md5 = LRU({
      max: this.api.getConfig('hodi', 'cacheSize', 100000),
      maxAge: 1000 * 60 * +this.api.getConfig('hodi', 'cacheAgeMin', '5')
    });
    this.email = LRU({
      max: this.api.getConfig('hodi', 'cacheSize', 100000),
      maxAge: 1000 * 60 * +this.api.getConfig('hodi', 'cacheAgeMin', '5')
    });

    this.client = new elasticsearch.Client({
      host: this.esHost,
      keepAlive: true,
      minSockets: 5,
      maxSockets: 51
    });

    ['hodi-domain', 'hodi-ip', 'hodi-md5', 'hodi-email'].forEach((index) => {
      this.client.indices.exists({ index: index }, (err, exists) => {
        if (exists) {
          this.client.indices.putSettings({
            index: index,
            body: {
              'index.refresh_interval': '60s'
            }
          });
          return;
        }
        this.client.indices.create({
          index: index,
          body: {
            settings: {
              'index.refresh_interval': '60s'
            },
            mappings: {
              hodi: {
                _all: { enabled: false },
                properties: {
                  firstSeen: { type: 'date', index: 'not_analyzed' },
                  lastSeen: { type: 'date', index: 'not_analyzed' },
                  count: { type: 'long', index: 'not_analyzed' }
                }
              }
            }
          }
        });
      });
    });

    this.api.addSource('hodi', this, ['domain', 'email', 'ip', 'md5']);
    setInterval(this.sendBulk.bind(this), 1000);
  }

  // ----------------------------------------------------------------------------
  sendBulk () {
    if (this.bulk.length === 0) {
      return;
    }
    if (this.api.debug > 0) {
      console.log('HODI', this.bulk.length);
    }
    this.client.bulk({ body: this.bulk });
    this.bulk = [];
  };

  // ----------------------------------------------------------------------------
  process (index, id, cb) {
    cb(null, undefined);

    const info = this[index].get(id);
    if (info) {
      return;
    }

    this[index].set(id, true);

    const date = new Date().toISOString();
    this.bulk.push({ update: { _index: `hodi-${index}`, _type: 'hodi', _id: id } });
    this.bulk.push({ script_file: 'hodi', params: { lastSeen: date }, upsert: { count: 1, firstSeen: date, lastSeen: date } });
    if (this.bulk.length >= 1000) {
      this.sendBulk();
    }
  };

  // ----------------------------------------------------------------------------
  getDomain (query, cb) {
    this.process('domain', query.value, cb);
  };

  // ----------------------------------------------------------------------------
  getIp (query, cb) {
    this.process('ip', query.value, cb);
  };

  // ----------------------------------------------------------------------------
  getMd5 (query, cb) {
    if (query.contentType === undefined || this.contentTypes[query.contentType] !== 1) {
      return cb(null, undefined);
    }

    this.process('md5', query.value, cb);
  };

  // ----------------------------------------------------------------------------
  getEmail (query, cb) {
    this.process('email', query.value, cb);
  };
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('hodi', {
    singleton: true,
    name: 'hodi',
    description: 'Experimental “History of Observed Data Indicators” plugin using Elasticsearch. This watches all queries to WISE and sends a feed to a configured elasticsearch cluster with firstSeen, lastSeen, and VERY rough count metric.',
    types: ['ip', 'domain', 'md5', 'email'],
    fields: [
      { name: 'contentTypes', required: true, help: 'Comma separated list of contentTypes to store md5 results for' },
      { name: 'esHost', required: true, help: 'The elasticsearch connection string, usually host:port' },
      { name: 'cacheSize', required: false, help: 'Maximum number of results to cache' }
    ]
  });

  return new HODISource(api, 'hodi');
};
