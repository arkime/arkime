/******************************************************************************/
/*
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
   ,csv            = require('csv')
  , wiseSource     = require('./wiseSource.js')
  , util           = require('util')
  , HashTable      = require('hashtable')
  ;
//////////////////////////////////////////////////////////////////////////////////
function AlienVaultSource (api, section) {
  AlienVaultSource.super_.call(this, api, section);
  this.key          = api.getConfig("alienvault", "key");
  this.cacheTimeout = -1;

  if (this.key === undefined) {
    console.log(this.section, "- No export key defined");
    return;
  }
  this.ips          = new HashTable();

  this.api.addSource("alienvault", this);

  this.idField          = this.api.addField("field:alienvault.id;db:alienvault.id;kind:integer;friendly:Id;help:Alien Vault ID;count:true");
  this.reliabilityField = this.api.addField("field:alienvault.reliability;db:alienvault.reliability;kind:integer;friendly:Reliability;help:Alient Vault Reliability;count:true");
  this.threatlevelField = this.api.addField("field:alienvault.threat-level;db:alienvault.threatlevel;kind:integer;friendly:Threat Level;help:Alient Vault Threat Level;count:true");
  this.activityField    = this.api.addField("field:alienvault.activity;db:alienvault.activity-term;kind:termfield;friendly:Activity;help:Alient Vault Activity;count:true");

  this.api.addView("alienvault", 
    "if (session.alienvault)\n" +
    "  div.sessionDetailMeta.bold AlienVault\n" +
    "  dl.sessionDetailMeta\n" +
    "    +arrayList(session.alienvault, 'activity-term', 'Activity', 'alienvault.activity')\n" +
    "    +arrayList(session.alienvault, 'threatlevel', 'Threat Level', 'alienvault.threat-level')\n" +
    "    +arrayList(session.alienvault, 'reliability', 'Reliability', 'alienvault.reliability')\n" +
    "    +arrayList(session.alienvault, 'id', 'Id', 'alienvault.id')\n"
  );

  setImmediate(this.loadFile.bind(this));
  setInterval(this.loadFile.bind(this), 2*60*60*1000); // Reload file every 2 hours
}
util.inherits(AlienVaultSource, wiseSource);
//////////////////////////////////////////////////////////////////////////////////
AlienVaultSource.prototype.parseFile = function()
{
  var self = this;
  var parser = csv.parse({delimiter: "#", skip_empty_lines:true}, function(err, data) {
    if (err) {
      console.log(self.section, "- Couldn't parse csv", err);
      return;
    }
    var count = 0;
    self.ips.clear();
    for (var i = 0; i < data.length; i++) {
      if (data[i].length < 8) {
        continue;
      }

      var encoded = wiseSource.encode(self.idField, data[i][7],
                                      self.reliabilityField, data[i][1],
                                      self.threatlevelField, data[i][2],
                                      self.activityField, data[i][3]);
      self.ips.put(data[i][0], {num: 4, buffer: encoded});
      count++;
    }
    console.log(self.section, "- Done Loading", count, "elements");
  });
  fs.createReadStream('/tmp/alienvault.data').pipe(parser);
};
//////////////////////////////////////////////////////////////////////////////////
AlienVaultSource.prototype.loadFile = function() {
  var self = this;
  console.log(self.section, "- Downloading files");

  var revision = 0;

  if (fs.existsSync("/tmp/alienvault.rev") && fs.existsSync("/tmp/alienvault.data")) {
    revision = + fs.readFileSync("/tmp/alienvault.rev").toString();
  }

  wiseSource.request('http://reputation.alienvault.com/' + self.key + '/reputation.rev',  '/tmp/alienvault.rev', function (statusCode) {
    var line = fs.readFileSync("/tmp/alienvault.rev").toString();
    if (+line !== revision) {
      if (fs.existsSync("/tmp/alienvault.data")) {
        fs.unlinkSync("//tmp/alienvault.data");
      }
      wiseSource.request('http://reputation.alienvault.com/' + self.key + '/reputation.data',  '/tmp/alienvault.data', function (statusCode) {
        if (statusCode === 200 || !self.loaded) {
          self.loaded = true;
          self.parseFile();
        }
      });
    } else {
      self.loaded = true;
      self.parseFile();
    }
  });
};
//////////////////////////////////////////////////////////////////////////////////
AlienVaultSource.prototype.getIp = function(ip, cb) {
  cb(null, this.ips.get(ip));
};
//////////////////////////////////////////////////////////////////////////////////
AlienVaultSource.prototype.dump = function(res) {
  var self = this;
  var cache = self.ips;
  self.ips.forEach(function(key, value) {
    var str = "{key: \"" + key + "\", ops:\n" + 
               wiseSource.result2Str(wiseSource.combineResults([value])) + "},\n";
    res.write(str);
  });
  res.end();
};
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var source = new AlienVaultSource(api, "alienvault");
};
