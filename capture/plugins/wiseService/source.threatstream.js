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
/*jshint
  node: true, plusplus: false, curly: true, eqeqeq: true, immed: true, latedef: true, newcap: true, nonew: true, undef: true, strict: true, trailing: true
*/
'use strict';

var fs             = require('fs')
  , unzip          = require('unzip')
  ;
var internals = {
  ips:     {},
  domains: {},
  emails:  {},
  md5s:    {}
};
//////////////////////////////////////////////////////////////////////////////////
function parseFile()
{
  internals.ips = {};
  internals.domains = {};
  internals.emails = {};
  internals.md5s = {};

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
          if (item.maltype && item.maltype !== "null") {
            encoded = internals.api.encode(internals.severityField, item.severity,
                                 internals.confidenceField, "" + item.confidence,
                                 internals.idField, "" + item.id,
                                 internals.typeField, item.itype,
                                 internals.maltypeField, item.maltype);
            num = 5;
          } else {
            encoded = internals.api.encode(internals.severityField, item.severity,
                                 internals.confidenceField, "" + item.confidence,
                                 internals.idField, "" + item.id,
                                 internals.typeField, item.itype);
            num = 4;
          }
                                             

          if (item.itype.match(/(_ip|anon_proxy|anon_vpn)/)) {
            internals.ips[item.ip] = {num: num, buffer: encoded};
          } else if (item.itype.match(/_domain|_dns/)) {
            internals.domains[item.domain] = {num: num, buffer: encoded};
          } else if (item.itype.match(/_email/)) {
            internals.emails[item.email] = {num: num, buffer: encoded};
          } else if (item.itype.match(/_md5/)) {
            internals.md5s[item.md5] = {num: num, buffer: encoded};
          }
        });
      });
    })
    .on('close', function () {
      console.log("Threatstream - Done Loading");
    });
}
//////////////////////////////////////////////////////////////////////////////////
function loadFile()
{
  console.log("Threatstream - Downloading files");
  internals.api.request('https://api.threatstream.com/api/v1/intelligence/snapshot/download/?username=' + internals.user + '&api_key=' + internals.key,  '/tmp/threatstream.zip', function (statusCode) {
    if (statusCode === 200 || !internals.loaded) {
      internals.loaded = true;
      parseFile();
    }
  });
}
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  internals.key = api.getConfig("threatstream", "key");
  internals.user = api.getConfig("threatstream", "user");
  internals.api = api;
  if (internals.user === undefined) {
    console.log("Threatstream - No user defined");
    return;
  }

  if (internals.key === undefined) {
    console.log("Threatstream - No key defined");
    return;
  }

  api.addSource("threatstream", exports);

  internals.severityField = api.addField("field:threatstream.severity;db:threatstream.severity-term;kind:lotermfield;friendly:Severity;help:Threatstream Severity;shortcut:0;count:true");
  internals.confidenceField = api.addField("field:threatstream.confidence;db:threatstream.confidence;kind:integer;friendly:Confidence;help:Threatstream Confidence;shortcut:1;count:true");
  internals.idField = api.addField("field:threatstream.id;db:threatstream.id;kind:integer;friendly:Id;help:Threatstream Id;shortcut:2;count:true");
  internals.typeField = api.addField("field:threatstream.type;db:threatstream.type-term;kind:lotermfield;friendly:Type;help:Threatstream Type;shortcut:3;count:true");
  internals.maltypeField = api.addField("field:threatstream.maltype;db:threatstream.maltype-term;kind:lotermfield;friendly:Malware Type;help:Threatstream Malware Type;shortcut:4;count:true");


  loadFile();
  setInterval(loadFile, 8*60*60*1000); // Reload file every 8 hours
};
//////////////////////////////////////////////////////////////////////////////////
exports.getDomain = function(domain, cb) {
  var domains = internals.domains;
  cb(null, domains[domain] || domains[domain.substring(domain.indexOf(".")+1)]);
};
//////////////////////////////////////////////////////////////////////////////////
exports.getIp = function(ip, cb) {
  cb(null, internals.ips[ip]);
};
//////////////////////////////////////////////////////////////////////////////////
exports.getMd5 = function(md5, cb) {
  cb(null, internals.md5s[md5]);
};
//////////////////////////////////////////////////////////////////////////////////
exports.getEmail = function(email, cb) {
  cb(null, internals.emails[email]);
};
