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

var wiseSource = require('./wiseSource.js');
var util = require('util');
var splunkjs = require('splunk-sdk');
var iptrie = require('iptrie');

// ----------------------------------------------------------------------------
function SplunkSource (api, section) {
  SplunkSource.super_.call(this, api, section);

  this.host = api.getConfig(section, 'host');
  this.username = api.getConfig(section, 'username');
  this.password = api.getConfig(section, 'password');
  this.version = api.getConfig(section, 'version', 5);
  this.port = api.getConfig(section, 'port', 8089);
  this.periodic = api.getConfig(section, 'periodic');
  this.query = api.getConfig(section, 'query');
  this.keyColumn = api.getConfig(section, 'keyColumn');

  this.typeSetting();
  this.tagsSetting();

  ['host', 'username', 'password', 'query', 'keyColumn'].forEach((item) => {
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

  api.addSource(section, this);

  this.sourceFields = [this.esResultField];
  for (var k in this.shortcuts) {
    if (this.sourceFields.indexOf(k) === -1) {
      this.sourceFields.push(k);
    }
  }
}
util.inherits(SplunkSource, wiseSource);

// ----------------------------------------------------------------------------
SplunkSource.prototype.periodicRefresh = function () {
  this.service.oneshotSearch(this.query, { output_mode: 'json', count: 0 }, (err, results) => {
    if (err) {
      console.log(this.section, '- ERROR', err);
      return;
    }

    var cache;
    if (this.type === 'ip') {
      cache = { items: new Map(), trie: new iptrie.IPTrie() };
    } else {
      cache = new Map();
    }

    for (let item of results.results) {
      var key = item[this.keyColumn];
      if (!key) { continue; }

      var args = [];
      for (var k in this.shortcuts) {
        if (item[k] !== undefined) {
          args.push(this.shortcuts[k]);
          if (Array.isArray(item[k])) {
            args.push(item[k][0]);
          } else {
            args.push(item[k]);
          }
        }
      }

      var newitem = { num: args.length / 2, buffer: wiseSource.encode.apply(null, args) };

      if (this.type === 'ip') {
        var parts = key.split('/');
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
SplunkSource.prototype.dump = function (res) {
  if (this.cache === undefined) {
    return res.end();
  }

  var cache = this.type === 'ip' ? this.cache.items : this.cache;
  cache.forEach((value, key) => {
    var str = `{key: "${key}", ops:\n` +
      wiseSource.result2Str(wiseSource.combineResults([this.tagsResult, value])) + '},\n';
    res.write(str);
  });
  res.end();
};
// ----------------------------------------------------------------------------
SplunkSource.prototype.sendResultPeriodic = function (key, cb) {
  if (!this.cache) {
    return cb(null, undefined);
  }

  var result = this.type === 'ip' ? this.cache.trie.find(key) : this.cache.get(key);

  // Not found, or found but no extra values to add
  if (!result) {
    return cb(null, undefined);
  }
  if (result.num === 0) {
    return cb(null, this.tagsResult);
  }

  // Found, so combine the two results (per item, and per source)
  var newresult = { num: result.num + this.tagsResult.num, buffer: Buffer.concat([result.buffer, this.tagsResult.buffer]) };
  return cb(null, newresult);
};

// ----------------------------------------------------------------------------
SplunkSource.prototype.sendResult = function (key, cb) {
  var query = this.query.replace('%%SEARCHTERM%%', key);

  this.service.oneshotSearch(query, { output_mode: 'json', count: 0 }, (err, results) => {
    if (err) {
      console.log(this.section, '- ERROR', err);
      return cb(null, undefined);
    }

    if (!results.results || results.results.length === 0) {
      return cb(null, undefined);
    }

    var item = results.results[0];

    var args = [];
    for (var k in this.shortcuts) {
      if (item[k] !== undefined) {
        args.push(this.shortcuts[k]);
        if (Array.isArray(item[k])) {
          args.push(item[k][0]);
        } else {
          args.push(item[k]);
        }
      }
    }
    var newresult = { num: args.length / 2 + this.tagsResult.num, buffer: Buffer.concat([wiseSource.encode.apply(null, args), this.tagsResult.buffer]) };
    return cb(null, newresult);
  });
};
// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef("splunk", {
    singleton: false,
    name: "splunk",
    description: "This source monitors configured files for right-click actions to send to all the viewer instances that connect to this WISE Server",
    fields: [
      { name: "username", required: true, help: "The Splunk username" },
      { name: "password", required: true, help: "The Splunk password" },
      { name: "host", required: true, help: "The Splunk hostname" },
      { name: "keyColumn", required: true, help: "The column to use from the returned data to use as the key" },
      { name: "periodic", required: false, help: "Should we do periodic queries or individual queries" },
      { name: "port", required: true, help: "The Splunk port" },
      { name: "query", required: true, help: "The query to run against Splunk. For non periodic queries the string %%SEARCHTERM%% will be replaced with the key" },
      { name: "version", required: false, help: "The Splunk api version to use (defaults to 5)" },
    ]
  });

  var sections = api.getConfigSections().filter((e) => { return e.match(/^splunk:/); });
  sections.forEach((section) => {
    return new SplunkSource(api, section);
  });
};
