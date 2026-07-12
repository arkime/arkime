/******************************************************************************/
/*
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
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

      let answered = false;
      const rowResults = [];
      try {
        this.parse(reply, (ignorekey, result) => {
          // Collect every row; cb must only be invoked once per query
          rowResults.push(result);
        }, (err) => {
          if (err) {
            console.log(`${this.section} -`, err);
            answered = true;
            return cb(null, undefined);
          }
          if (rowResults.length === 0) {
            console.log(`${this.section} - The keyPath ${this.keyPath} wasn't found even though document was returned`, reply);
            answered = true;
            return cb(null, undefined);
          }
          answered = true;
          return cb(null, WISESource.combineResults([...rowResults, this.tagsResult]));
        });
      } catch (err) {
        if (err) {
          console.log(`${this.section} -`, err);
        }
        // parse threw before producing a result; complete the query so the
        // wiseService in-progress entry isn't left hanging until the timeout
        if (!answered) {
          return cb(null, undefined);
        }
      }
    });
  }

  // ----------------------------------------------------------------------------
  fetchDomain (key, cb) {
    this.fetch(key, (err, result) => {
      if (result === undefined) {
        return this.fetch(key.substring(key.indexOf('.') + 1), cb);
      }
      return cb(err, result);
    });
  }
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
      { name: 'format', required: false, help: 'The format data is in: csv (default), tagger, jsonl, or json', regex: '^(csv|tagger|jsonl|json)$' },
      { name: 'column', required: false, help: 'The numerical column number to use as the key', regex: '^[0-9]*$', ifField: 'format', ifValue: 'csv' },
      { name: 'keyPath', required: true, help: 'The path of what field to use as the key', ifField: 'format', ifValue: ['jsonl', 'json'] },
      { name: 'template', required: false, help: 'The template when forming the key name. %key% = the key being looked up, %type% = the type being looked up' },
      { name: 'redisMethod', required: false, help: 'The lowercase redis method to retrieve values, defaults to "get"' }
    ]
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^redis:/); });
  sections.forEach((section) => {
    return new RedisSource(api, section);
  });
};
// ----------------------------------------------------------------------------
