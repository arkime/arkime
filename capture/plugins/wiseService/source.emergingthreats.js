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

var fs             = require('fs')
  , csv            = require('csv')
  , wiseSource     = require('./wiseSource.js')
  , util           = require('util')
  , HashTable      = require('hashtable')
  ;

//////////////////////////////////////////////////////////////////////////////////
function EmergingThreatsSource (api, section) {
  EmergingThreatsSource.super_.call(this, api, section);
  this.key          = api.getConfig("emergingthreats", "key");

  if (this.key === undefined) {
    console.log(this.section, "- No key defined");
    return;
  }

  this.ips          = new HashTable();
  this.domains      = new HashTable();
  this.categories   = {};
  this.cacheTimeout = -1;

  this.api.addSource("emergingthreats", this);

  this.scoreField = this.api.addField("field:emergingthreats.score;db:et.score;kind:integer;friendly:Score;help:Emerging Threats Score;count:true");
  this.categoryField = this.api.addField("field:emergingthreats.category;db:et.category;kind:termfield;friendly:Category;help:Emerging Threats Category;count:true");

  this.api.addView("emergingthreats",
    "if (session.et)\n" +
    "  div.sessionDetailMeta.bold Emerging Threats\n" +
    "  dl.sessionDetailMeta\n" +
    "    +arrayList(session.et, 'category', 'Category', 'emergingthreats.category')\n" +
    "    +arrayList(session.et, 'score', 'Score', 'emergingthreats.score')\n"
  );

  setImmediate(this.loadFiles.bind(this));
  setInterval(this.loadFiles.bind(this), 60*60*1000); // Reload files every hour
}
util.inherits(EmergingThreatsSource, wiseSource);
//////////////////////////////////////////////////////////////////////////////////
EmergingThreatsSource.prototype.parseCategories = function(fn)
{
  var parser = csv.parse({skip_empty_lines:true}, (err, data) => {
    if (err) {
      console.log(this.section, "- Couldn't parse", fn, "csv", err);
      return;
    }

    this.categories = {};
    for (var i = 0; i < data.length; i++) {
      this.categories[data[i][0]] = data[i][1];
    }
  });
  fs.createReadStream('/tmp/categories.txt').pipe(parser);
};
//////////////////////////////////////////////////////////////////////////////////
EmergingThreatsSource.prototype.parse = function (fn, hash)
{
  var parser = csv.parse({skip_empty_lines:true}, (err, data) => {
    if (err) {
      console.log(this.section, "- Couldn't parse", fn, "csv", err);
      return;
    }

    for (var i = 1; i < data.length; i++) {
      if (data[i].length !== 3) {
        continue;
      }

      var encoded = wiseSource.encode(this.categoryField, this.categories[data[i][1]] || ('Unknown - ' + data[i][1]),
                                      this.scoreField, "" + data[i][2]);
      var value = hash.get(data[i][0]);
      if (value) {
        value.num += 2;
        value.buffer = Buffer.concat([value.buffer, encoded]);
      } else {
        hash.put(data[i][0], {num: 2, buffer: encoded});
      }
    }
    console.log(this.section, "- Done Loading", fn);
  });
  fs.createReadStream(fn).pipe(parser);
};
//////////////////////////////////////////////////////////////////////////////////
EmergingThreatsSource.prototype.loadFiles = function ()
{
  console.log(this.section, "- Downloading Files");
  wiseSource.request('https://rules.emergingthreatspro.com/' + this.key + '/reputation/categories.txt', '/tmp/categories.txt', (statusCode) => {

    this.parseCategories("/tmp/categories.txt");
  });

  wiseSource.request('https://rules.emergingthreatspro.com/' + this.key + '/reputation/iprepdata.csv', '/tmp/iprepdata.csv', (statusCode) => {
    if (statusCode === 200 || !this.ipsLoaded) {
      this.ipsLoaded = true;
      this.ips.clear();
      this.parse("/tmp/iprepdata.csv", this.ips);
    }
  });

  wiseSource.request('https://rules.emergingthreatspro.com/' + this.key + '/reputation/domainrepdata.csv', '/tmp/domainrepdata.csv', (statusCode) => {
    if (statusCode === 200 || !this.domainsLoaded) {
      this.domainsLoaded = true;
      this.domains.clear();
      this.parse("/tmp/domainrepdata.csv", this.domains);
    }
  });
};
//////////////////////////////////////////////////////////////////////////////////
EmergingThreatsSource.prototype.getDomain = function(domain, cb) {
  cb(null, this.domains.get(domain) || this.domains.get(domain.substring(domain.indexOf(".")+1)));
};
//////////////////////////////////////////////////////////////////////////////////
EmergingThreatsSource.prototype.getIp = function(ip, cb) {
  cb(null, this.ips.get(ip));
};
//////////////////////////////////////////////////////////////////////////////////
EmergingThreatsSource.prototype.dump = function(res) {
  ["ips", "domains"].forEach((ckey) => {
    res.write(`${ckey}:\n`);
    this[ckey].forEach((key, value) => {
      var str = `{key: "${key}", ops:\n` +
        wiseSource.result2Str(wiseSource.combineResults([value])) + "},\n";
      res.write(str);
    });
  });
  res.end();
};
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var source = new EmergingThreatsSource(api, "emergingthreats");
};
