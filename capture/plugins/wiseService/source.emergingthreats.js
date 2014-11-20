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
  , exec           = require('child_process').exec
  , csv            = require('csv')
  ;
var internals = {
  ips:     {},
  domains: {},
  categories: {}
};
//////////////////////////////////////////////////////////////////////////////////
function parseCAT (fn) 
{
  internals.categories = {};
  var parser = csv.parse(function(err, data) {
    for (var i = 0; i < data.length; i++) {
      internals.categories[data[i][0]] = data[i][1];
    }
  });
  fs.createReadStream('/tmp/categories.txt').pipe(parser);
}
//////////////////////////////////////////////////////////////////////////////////
function parseCSV (fn, hash)
{
  var parser = csv.parse(function(err, data) {
    for (var i = 1; i < data.length; i++) {
      if (data[i].length !== 3) {
        continue;
      }
    
      var encoded = internals.api.encode(internals.categoryField, internals.categories[data[i][1]] || ('Unknown - ' + data[i][1]),
                                         internals.scoreField, "" + data[i][2]);
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

}
//////////////////////////////////////////////////////////////////////////////////
function loadFiles ()
{
  if (internals.skipFirstLoad) {
    internals.skipFirstLoad = false;
    parseCAT("/tmp/categories.txt");
    internals.ips = {};
    parseCSV("/tmp/iprepdata.csv", internals.ips);
    internals.domains = {};
    parseCSV("/tmp/domainrepdata.csv", internals.domains);
  } else {
    console.log("ET - Downloading Files");
    exec('wget -N "https://rules.emergingthreatspro.com/' + internals.key + '/reputation/categories.txt" -O /tmp/categories.txt', function (error, stdout, stderr) {

      parseCAT("/tmp/categories.txt");
    });

    exec('wget -N "https://rules.emergingthreatspro.com/' + internals.key + '/reputation/iprepdata.csv" -O /tmp/iprepdata.csv', function (error, stdout, stderr) {
      internals.ips = {};
      parseCSV("/tmp/iprepdata.csv", internals.ips);
    });

    exec('wget -N "https://rules.emergingthreatspro.com/' + internals.key + '/reputation/domainrepdata.csv" -O /tmp/domainrepdata.csv', function (error, stdout, stderr) {
      internals.domains = {};
      parseCSV("/tmp/domainrepdata.csv", internals.domains);
    });
  }
}
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  internals.api = api;
  internals.key = api.getConfig("emergingthreats", "key");
  if (internals.key === undefined) {
    console.log("ET - No key defined");
    return;
  }

  api.addSource("emergingthreats", exports);

  internals.scoreField = api.addField("field:emergingthreats.score;db:et.score;kind:integer;friendly:Score;help:Emerging Threats Score;count:true");
  internals.categoryField = api.addField("field:emergingthreats.category;db:et.category-term;kind:termfield;friendly:Category;help:Emerging Threats Category;count:true");

  internals.skipFirstLoad = fs.existsSync("/tmp/categories.txt") && fs.existsSync("/tmp/iprepdata.csv") && fs.existsSync("/tmp/domainrepdata.csv");
  loadFiles();
  setInterval(loadFiles, 60*60*1000); // Reload files every hour
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
