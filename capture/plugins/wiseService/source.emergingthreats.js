/******************************************************************************/
/*
 *
 * Copyright 2012-2014 AOL Inc. All rights reserved.
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
  ;

//////////////////////////////////////////////////////////////////////////////////
function EmergingThreatsSource (api, section) {
  EmergingThreatsSource.super_.call(this, api, section);
  this.key     = api.getConfig("emergingthreats", "key");
  this.ips     = {};
  this.domains = {};
}
util.inherits(EmergingThreatsSource, wiseSource);
//////////////////////////////////////////////////////////////////////////////////
EmergingThreatsSource.prototype.parseCategories = function(fn) 
{
  var self = this;
  var parser = csv.parse({skip_empty_lines:true}, function(err, data) {
    if (err) {
      console.log("Emerging Threats - Couldn't parse", fn, "csv", err);
      return;
    }

    self.categories = {};
    for (var i = 0; i < data.length; i++) {
      self.categories[data[i][0]] = data[i][1];
    }
  });
  fs.createReadStream('/tmp/categories.txt').pipe(parser);
};
//////////////////////////////////////////////////////////////////////////////////
EmergingThreatsSource.prototype.parse = function (fn, hash)
{
  var self = this;

  var parser = csv.parse({skip_empty_lines:true}, function(err, data) {
    if (err) {
      console.log("Emerging Threats - Couldn't parse", fn, "csv", err);
      return;
    }

    for (var i = 1; i < data.length; i++) {
      if (data[i].length !== 3) {
        continue;
      }
    
      var encoded = wiseSource.encode(self.categoryField, self.categories[data[i][1]] || ('Unknown - ' + data[i][1]),
                                      self.scoreField, "" + data[i][2]);
      if (hash[data[i][0]]) {
        hash[data[i][0]].num += 2;
        hash[data[i][0]].buffer = Buffer.concat([hash[data[i][0]].buffer, encoded]);
      } else {
        hash[data[i][0]] = {num: 2, buffer: encoded};
      }
    }
    console.log("ET - Done Loading", fn);
  });
  fs.createReadStream(fn).pipe(parser);
};
//////////////////////////////////////////////////////////////////////////////////
EmergingThreatsSource.prototype.loadFiles = function ()
{
  var self = this;
  console.log("ET - Downloading Files");
  wiseSource.request('https://rules.emergingthreatspro.com/' + self.key + '/reputation/categories.txt', '/tmp/categories.txt', function (statusCode) {

    self.parseCategories("/tmp/categories.txt");
  });

  wiseSource.request('https://rules.emergingthreatspro.com/' + self.key + '/reputation/iprepdata.csv', '/tmp/iprepdata.csv', function (statusCode) {
    if (statusCode === 200 || !self.ipsLoaded) {
      self.ipsLoaded = true;
      self.ips = {};
      self.parse("/tmp/iprepdata.csv", self.ips);
    }
  });

  wiseSource.request('https://rules.emergingthreatspro.com/' + self.key + '/reputation/domainrepdata.csv', '/tmp/domainrepdata.csv', function (statusCode) {
    if (statusCode === 200 || !self.domainsLoaded) {
      self.domainsLoaded = true;
      self.domains = {};
      self.parse("/tmp/domainrepdata.csv", self.domains);
    }
  });
};
//////////////////////////////////////////////////////////////////////////////////
EmergingThreatsSource.prototype.getDomain = function(domain, cb) {
  cb(null, this.domains[domain] || this.domains[domain.substring(domain.indexOf(".")+1)]);
};
//////////////////////////////////////////////////////////////////////////////////
EmergingThreatsSource.prototype.getIp = function(ip, cb) {
  cb(null, this.ips[ip]);
};
//////////////////////////////////////////////////////////////////////////////////
EmergingThreatsSource.prototype.dump = function(res) {
  var self = this;

  ["ips", "domains"].forEach(function (ckey) {
    res.write("" + ckey + ":\n");
    var cache = self[ckey];
    for (var key in cache) {
      var str = "{key: \"" + key + "\", ops:\n" + 
        wiseSource.result2Str(wiseSource.combineResults([cache[key]])) + "},\n";
      res.write(str);
    }
  });
  res.end();
};
//////////////////////////////////////////////////////////////////////////////////
EmergingThreatsSource.prototype.init = function ()
{
  if (this.key === undefined) {
    console.log("ET - No key defined");
    return;
  }

  this.api.addSource("emergingthreats", this);

  this.scoreField = this.api.addField("field:emergingthreats.score;db:et.score;kind:integer;friendly:Score;help:Emerging Threats Score;count:true");
  this.categoryField = this.api.addField("field:emergingthreats.category;db:et.category-term;kind:termfield;friendly:Category;help:Emerging Threats Category;count:true");

  this.api.addView("emergingthreats", 
    "if (session.et)\n" +
    "  div.sessionDetailMeta.bold Emerging Threats\n" +
    "  dl.sessionDetailMeta\n" +
    "    +arrayList(session.et, 'category-term', 'Category', 'emergingthreats.category')\n" +
    "    +arrayList(session.et, 'score', 'Score', 'emergingthreats.score')\n"
  );

  this.loadFiles();
  setInterval(this.loadFiles.bind(this), 60*60*1000); // Reload files every hour
};
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var source = new EmergingThreatsSource(api, "emergingthreats");
  source.init();
};
