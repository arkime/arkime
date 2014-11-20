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

var https          = require('https')
  , LRU            = require('lru-cache')
  ;
var internals = {
  waiting: [],
  statuses: {"-1": "malicious", "0":"unknown", "1":"benign"},
  outgoing: 0,
  inprogress: 0,
  cached: 0,
};
//////////////////////////////////////////////////////////////////////////////////
function getCategories() {
  var options = {
      host: 'sgraph.api.opendns.com',
      port: '443',
      path: '/domains/categories',
      method: 'GET',
      headers: {
          'Authorization': 'Bearer ' + internals.key,
      }
  };

  var response = "";
  var request = https.request(options, function(res) {
    res.on('data', function (chunk) {
      response += chunk;
    });
    res.on('end', function () {
      internals.categories = JSON.parse(response);
    });
  });
  request.on('error', function (err) {
    console.log(err);
  });

  request.end();
}
//////////////////////////////////////////////////////////////////////////////////
function performQuery()
{
  if (internals.waiting.length === 0) {
    return;
  }

  if (internals.api.debug > 0) {
    console.log("OpenDNS - Fetching %d", internals.waiting.length);
  }

  internals.outgoing++;

  // http://stackoverflow.com/questions/6158933/how-to-make-an-http-post-request-in-node-js/6158966
  // console.log("doing query:", waiting.length, "current cache", Object.keys(cache).length);
  var postData = JSON.stringify(internals.waiting);
  internals.waiting.length = 0;

  var postOptions = {
      host: 'sgraph.api.opendns.com',
      port: '443',
      path: '/domains/categorization/',
      method: 'POST',
      headers: {
          'Authorization': 'Bearer ' + internals.key,
          'Content-Type': 'application/x-www-form-urlencoded',
          'Content-Length': postData.length
      }
  };

  var response = "";
  var request = https.request(postOptions, function(res) {
    res.on('data', function (chunk) {
      response += chunk;
    });
    res.on('end', function (err) {
      var results;
      try {
        results = JSON.parse(response);
      } catch (e) {
        console.log("Error parsing for request:\n", postData, "\nresponse:\n", response);
        results = {};
      }
      for (var result in results) {
        var info = internals.cache.get(result);
        if (!info) {
          info = {};
          internals.cache.set(result, info);
        }
        var args = [internals.statusField, internals.statuses[results[result].status]];

        results[result].security_categories.forEach(function(value) {
          if (internals.categories[value]) {
            args.push(internals.scField, internals.categories[value]);
          } else {
            console.log("Bad OpenDNS SC", value);
          }
        });
        results[result].content_categories.forEach(function(value) {
          if (internals.categories[value]) {
            args.push(internals.ccField, internals.categories[value]);
          } else {
            console.log("Bad OpenDNS CC", value);
          }
        });

        info.result = {num: args.length/2, buffer: internals.api.encode.apply(null, args)};

        var cb;
        while ((cb = info.cbs.shift())) {
          cb(null, info.result);
        }
      }
    });
  });
  request.on('error', function (err) {
    console.log(err);
  });

  // post the data
  request.write(postData);
  request.end();
}
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  internals.key = api.getConfig("opendns", "key");
  if (internals.key === undefined) {
    console.log("OpenDNS - No key defined");
    return;
  }

  internals.api = api;
  internals.cache = LRU({max: api.getConfig("opendns", "cacheSize", 200000), 
                      maxAge: 1000 * 60 * +api.getConfig("opendns", "cacheAgeMin", "60")});

  api.addSource("opendns", exports);
  getCategories();
  setInterval(getCategories, 10*60*1000);
  setInterval(performQuery, 500);

  internals.statusField = api.addField("field:opendns.domain.status;db:opendns.dmstatus-term;kind:lotermfield;friendly:Status;help:OpenDNS domain security status;count:true");
  internals.scField = api.addField("field:opendns.domain.security;db:opendns.dmscat-term;kind:termfield;friendly:Security;help:OpenDNS domain security category;count:true");
  internals.ccField = api.addField("field:opendns.domain.content;db:opendns.dmccat-term;kind:termfield;friendly:Security;help:OpenDNS domain content category;count:true");
};
//////////////////////////////////////////////////////////////////////////////////
exports.getDomain = function(domain, cb) {
  var info = internals.cache.get(domain);
  if (info) {
    if (info.result) {
      internals.cached++;
      return cb(null, info.result);
    }
    internals.inprogress++;
    info.cbs.push(cb);
    return;
  }
  info = {cbs:[cb]};
  internals.cache.set(domain, info);
  internals.waiting.push(domain);
  if (internals.waiting.length > 1000) {
    performQuery();
  }
};
//////////////////////////////////////////////////////////////////////////////////
exports.printStats = function() {
  console.log("OpenDNS: outgoing:", internals.outgoing, "cached:", internals.cached, "inprogress:", internals.inprogress, "size:", internals.cache.itemCount);
};
