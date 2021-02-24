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
const util = require('util');
const splunkjs = require('splunk-sdk');
const iptrie = require('iptrie');

class SplunkSource extends WISESource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { typeSetting: true, tagsSetting: true });

    this.host = api.getConfig(section, 'host');
    this.username = api.getConfig(section, 'username');
    this.password = api.getConfig(section, 'password');
    this.version = api.getConfig(section, 'version', 5);
    this.port = api.getConfig(section, 'port', 8089);
    this.periodic = api.getConfig(section, 'periodic');
    this.query = api.getConfig(section, 'query');
    this.arrayPath = api.getConfig(section, 'arrayPath');
    this.keyPath = api.getConfig(section, 'keyPath', api.getConfig(section, 'keyColumn', 0));

    ['host', 'username', 'password', 'query', 'keyPath'].forEach((item) => {
      if (this[item] === undefined) {
        console.log(this.section, `- ERROR not loading since no ${item} specified in config file`);
      }
    });

    if (this.periodic) {
      this.cacheTimeout = -1; // Don't cache
      this[this.api.funcName(this.type)] = this.sendResultPeriodic;
      setInterval(this.periodicRefresh.bind(this), 1000 * this.periodic);
    } else {
      this[this.api.funcName(this.type)] = this.sendResult;
    }

    this.service = new splunkjs.Service({ username: this.username, password: this.password, host: this.host, port: this.port, version: this.version });

    this.service.login((err, success) => {
      if (err) {
        console.log("ERROR - Couldn't login to splunk - ", util.inspect(err, false, 50));
        return;
      }
      if (this.periodic) {
        this.periodicRefresh();
      }

      console.log('Login was successful: ' + success);
    });

    api.addSource(section, this, [this.type]);

    this.sourceFields = [this.esResultField];
    for (const k in this.shortcuts) {
      if (this.sourceFields.indexOf(k) === -1) {
        this.sourceFields.push(k);
      }
    }
  }

  // ----------------------------------------------------------------------------
  periodicRefresh () {
    this.service.oneshotSearch(this.query, { output_mode: 'json', count: 0 }, (err, results) => {
      if (err) {
        console.log(this.section, '- ERROR', err);
        return;
      }

      if (results === undefined || results.results === undefined) {
        console.log(this.section, '- No results - ', results);
        return;
      }

      let cache;
      if (this.type === 'ip') {
        cache = { items: new Map(), trie: new iptrie.IPTrie() };
      } else {
        cache = new Map();
      }

      for (const item of results.results) {
        const key = item[this.keyPath];
        if (!key) { continue; }

        const args = [];
        for (const k in this.shortcuts) {
          if (item[k] !== undefined) {
            args.push(this.shortcuts[k].pos);
            if (Array.isArray(item[k])) {
              args.push(item[k][0]);
            } else {
              args.push(item[k]);
            }
          }
        }

        const newitem = WISESource.encodeResult.apply(null, args);

        if (this.type === 'ip') {
          const parts = key.split('/');
          cache.trie.add(parts[0], +parts[1] || (parts[0].includes(':') ? 128 : 32), newitem);
          cache.items.set(key, newitem);
        } else {
          cache.set(key, newitem);
        }
      }
      this.cache = cache;
    });
  };

  // ----------------------------------------------------------------------------
  dump (res) {
    if (this.cache === undefined) {
      return res.end();
    }

    const cache = this.type === 'ip' ? this.cache.items : this.cache;
    cache.forEach((value, key) => {
      const str = `{"key": "${key}", "ops":\n` +
        WISESource.result2JSON(WISESource.combineResults([this.tagsResult, value])) + '},\n';
      res.write(str);
    });
    res.end();
  };

  // ----------------------------------------------------------------------------
  sendResultPeriodic (key, cb) {
    if (!this.cache) {
      return cb(null, undefined);
    }

    const result = this.type === 'ip' ? this.cache.trie.find(key) : this.cache.get(key);

    // Not found, or found but no extra values to add
    if (!result) {
      return cb(null, undefined);
    }
    if (result[0] === 0) {
      return cb(null, this.tagsResult);
    }

    // Found, so combine the two results (per item, and per source)
    const newresult = WISESource.combineResults([result, this.tagsResult]);
    return cb(null, newresult);
  };

  // ----------------------------------------------------------------------------
  sendResult (key, cb) {
    const query = this.query.replace('%%SEARCHTERM%%', key);

    this.service.oneshotSearch(query, { output_mode: 'json', count: 0 }, (err, results) => {
      if (err) {
        console.log(this.section, '- ERROR', err);
        return cb(null, undefined);
      }

      if (!results.results || results.results.length === 0) {
        return cb(null, undefined);
      }

      const item = results.results[0];

      const args = [];
      for (const k in this.shortcuts) {
        if (item[k] !== undefined) {
          args.push(this.shortcuts[k]);
          if (Array.isArray(item[k])) {
            args.push(item[k][0]);
          } else {
            args.push(item[k]);
          }
        }
      }
      const newresult = WISESource.combineResults([WISESource.encodeResult.apply(null, args), this.tagsResult]);
      return cb(null, newresult);
    });
  };
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('splunk', {
    singleton: false,
    name: 'splunk',
    description: 'This source monitors configured files for right-click actions to send to all the viewer instances that connect to this WISE Server',
    displayable: true,
    fields: [
      { name: 'type', required: true, help: 'The wise query type this source supports' },
      { name: 'tags', required: false, help: 'Comma separated list of tags to set for matches', regex: '^[-a-z0-9,]+' },
      { name: 'username', required: true, help: 'The Splunk username' },
      { name: 'password', required: true, help: 'The Splunk password' },
      { name: 'host', required: true, help: 'The Splunk hostname' },
      { name: 'arrayPath', required: false, help: "The path of where to find the array, if the json result isn't an array", ifField: 'format', ifValue: 'json' },
      { name: 'keyColumn', required: true, help: 'The column to use from the returned data to use as the key' },
      { name: 'periodic', required: false, help: 'Should we do periodic queries or individual queries' },
      { name: 'port', required: true, help: 'The Splunk port' },
      { name: 'query', required: true, help: 'The query to run against Splunk. For non periodic queries the string %%SEARCHTERM%% will be replaced with the key' },
      { name: 'version', required: false, help: 'The Splunk api version to use (defaults to 5)' }
    ]
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^splunk:/); });
  sections.forEach((section) => {
    return new SplunkSource(api, section);
  });
};
