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
  , unzip          = require('unzip')
  , wiseSource     = require('./wiseSource.js')
  , util           = require('util')
  , HashTable      = require('hashtable')
  ;
//////////////////////////////////////////////////////////////////////////////////
function ThreatStreamSource (api, section) {
  ThreatStreamSource.super_.call(this, api, section);
  this.user    = api.getConfig("threatstream", "user");
  this.key     = api.getConfig("threatstream", "key");
  this.ips     = new HashTable();
  this.domains = new HashTable();
  this.emails  = new HashTable();
  this.md5s    = new HashTable();
  this.urls    = new HashTable();
}
util.inherits(ThreatStreamSource, wiseSource);
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.parseFile = function()
{
  var self = this;

  self.ips.clear();
  self.ips.reserve(2000000);
  self.domains.clear();
  self.domains.reserve(2000000);
  self.emails.clear();
  self.emails.reserve(2000000);
  self.md5s.clear();
  self.md5s.reserve(2000000);
  self.urls.clear();
  self.urls.reserve(200000);

  fs.createReadStream('/tmp/threatstream.zip')
    .pipe(unzip.Parse())
    .on('entry', function (entry) {
      var bufs = [];
      entry.on('data', function (buf) {
        bufs.push(buf);
      }).on('end', function () {
        var json = JSON.parse(Buffer.concat(bufs));
        json.objects.forEach(function (item) {
          if (item.state !== "active") {
            return;
          }

          var encoded;
          var num;
          try {
            if (item.maltype && item.maltype !== "null") {
              encoded = wiseSource.encode(self.severityField, item.severity.toLowerCase(),
                                          self.confidenceField, "" + item.confidence,
                                          self.idField, "" + item.id,
                                          self.typeField, item.itype.toLowerCase(),
                                          self.maltypeField, item.maltype.toLowerCase(),
                                          self.sourceField, item.source);
              num = 6;
            } else {
              encoded = wiseSource.encode(self.severityField, item.severity.toLowerCase(),
                                          self.confidenceField, "" + item.confidence,
                                          self.idField, "" + item.id,
                                          self.typeField, item.itype.toLowerCase(),
                                          self.sourceField, item.source);
              num = 5;
            }
          } catch (e) {
            console.log("ERROR -", entry.path, e, item, e.stack);
            return;
          }
                                             

          if (item.itype.match(/(_ip|anon_proxy|anon_vpn)/)) {
            self.ips.put(item.ip, {num: num, buffer: encoded});
          } else if (item.itype.match(/_domain|_dns/)) {
            self.domains.put(item.domain, {num: num, buffer: encoded});
          } else if (item.itype.match(/_email/)) {
            self.emails.put(item.email, {num: num, buffer: encoded});
          } else if (item.itype.match(/_md5/)) {
            self.md5s.put(item.md5, {num: num, buffer: encoded});
          } else if (item.itype.match(/_url/)) {
            self.urls.put(item.url, {num: num, buffer: encoded});
          }
        });
        //console.log("Threatstream - Done", entry.path);
      });
    })
    .on('close', function () {
      console.log("Threatstream - Done Loading");
    });
};
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.loadFile = function() {
  var self = this;

  console.log("Threatstream - Downloading files");
  wiseSource.request('https://api.threatstream.com/api/v1/intelligence/snapshot/download/?username=' + self.user + '&api_key=' + self.key,  '/tmp/threatstream.zip', function (statusCode) {
    if (statusCode === 200 || !self.loaded) {
      self.loaded = true;
      self.parseFile();
    }
  });
};
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.init = function() {
  if (this.user === undefined) {
    console.log("Threatstream - No user defined");
    return;
  }

  if (this.key === undefined) {
    console.log("Threatstream - No key defined");
    return;
  }

  this.api.addSource("threatstream", this);

  this.severityField = this.api.addField("field:threatstream.severity;db:threatstream.severity-term;kind:lotermfield;friendly:Severity;help:Threatstream Severity;shortcut:0;count:true");
  this.confidenceField = this.api.addField("field:threatstream.confidence;db:threatstream.confidence;kind:integer;friendly:Confidence;help:Threatstream Confidence;shortcut:1;count:true");
  this.idField = this.api.addField("field:threatstream.id;db:threatstream.id;kind:integer;friendly:Id;help:Threatstream Id;shortcut:2;count:true");
  this.typeField = this.api.addField("field:threatstream.type;db:threatstream.type-term;kind:lotermfield;friendly:Type;help:Threatstream Type;shortcut:3;count:true");
  this.maltypeField = this.api.addField("field:threatstream.maltype;db:threatstream.maltype-term;kind:lotermfield;friendly:Malware Type;help:Threatstream Malware Type;shortcut:4;count:true");
  this.sourceField = this.api.addField("field:threatstream.source;db:threatstream.source-term;kind:termfield;friendly:Source;help:Threatstream Source;shortcut:5;count:true");

  this.api.addView("threatstream", 
    "if (session.threatstream)\n" +
    "  div.sessionDetailMeta.bold Threatstream\n" +
    "  dl.sessionDetailMeta\n" +
    "    +arrayList(session.threatstream, 'severity-term', 'Severity', 'threatstream.severity')\n" +
    "    +arrayList(session.threatstream, 'confidence', 'Confidence', 'threatstream.confidence')\n" +
    "    +arrayList(session.threatstream, 'id', 'Id', 'threatstream.id')\n" +
    "    +arrayList(session.threatstream, 'type-term', 'Type', 'threatstream.type')\n" +
    "    +arrayList(session.threatstream, 'maltype-term', 'Malware Type', 'threatstream.maltype')\n" +
    "    +arrayList(session.threatstream, 'source-term', 'Source', 'threatstream.source')\n"
  );

  this.api.addRightClick("threatstreamip", {name:"Threat Stream", url:"https://ui.threatstream.com/detail/ip/%TEXT%", category:"ip"});
  this.api.addRightClick("threatstreamhost", {name:"Threat Stream", url:"https://ui.threatstream.com/detail/domain/%HOST%", category:"host"});
  this.api.addRightClick("threatstreamemail", {name:"Threat Stream", url:"https://ui.threatstream.com/detail/email/%TEXT%", category:"user"});
  this.api.addRightClick("threatstreammd5", {name:"Threat Stream", url:"https://ui.threatstream.com/detail/md5/%TEXT%", category:"md5"});

  this.loadFile();
  setInterval(this.loadFile.bind(this), 8*60*60*1000); // Reload file every 8 hours
};
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.getDomain = function(domain, cb) {
  var domains = this.domains;
  cb(null, domains.get(domain) || domains.get(domain.substring(domain.indexOf(".")+1)));
};
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.getIp = function(ip, cb) {
  cb(null, this.ips.get(ip));
};
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.getMd5 = function(md5, cb) {
  cb(null, this.md5s.get(md5));
};
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.getEmail = function(email, cb) {
  cb(null, this.emails.get(email));
};
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.dump = function(res) {
  var self = this;
  ["ips", "domains", "emails", "md5s"].forEach(function (ckey) {
    res.write("" + ckey + ":\n");
    self[ckey].forEach(function(key, value) {
      var str = "{key: \"" + key + "\", ops:\n" + 
        wiseSource.result2Str(wiseSource.combineResults([value])) + "},\n";
      res.write(str);
    });
  });
  res.end();
};
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var source = new ThreatStreamSource(api, "threatstream");
  source.init();
};
//////////////////////////////////////////////////////////////////////////////////
