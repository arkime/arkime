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

var request        = require('request')
  , wiseSource     = require('./wiseSource.js')
  , util           = require('util')
  , LRU            = require('lru-cache')
  ;

//////////////////////////////////////////////////////////////////////////////////
function PassiveTotalSource (api, section) {
  PassiveTotalSource.super_.call(this, api, section);
  this.waiting    = [];
  this.outgoing   = 0;
  this.inprogress = 0;
  this.cached     = 0;
}
util.inherits(PassiveTotalSource, wiseSource);

//////////////////////////////////////////////////////////////////////////////////
PassiveTotalSource.prototype.performQuery = function () {
  var self = this;

  if (self.waiting.length === 0) {
    return;
  }

  if (self.api.debug > 0) {
    console.log("PassiveTotal - Fetching %d", self.waiting.length);
  }

  self.outgoing++;

  var options = {
      url: 'https://api.passivetotal.org/v2/enrichment/bulk',
      body: {additional: ["osint", "malware"],
             query: self.waiting},
      auth: {
        user: self.user,
        pass: self.key
      },
      method: 'GET',
      json: true
  };

  var req = request(options, function(err, im, results) {
    if (err) {
      console.log("Error parsing for request:\n", options, "\nresults:\n", results);
      results = {results:{}};
    } 

    for (var resultname in results.results) {
      var result = results.results[resultname];
      var info = self.cache.get(resultname);
      if (!info) {
        return;
      }
      if (result.tags === undefined || result.tags.length === 0) {
        info.result = wiseSource.emptyResult;
      } else {
        var args = [];
        for (var i = 0; i < result.tags.length; i++) {
          if (typeof(result.tags[i]) === "string") {
            args.push(self.tagsField, result.tags[i]);
          }
        }
        
        info.result = {num: args.length/2, buffer: wiseSource.encode.apply(null, args)};
      }

      var cb;
      while ((cb = info.cbs.shift())) {
        cb(null, info.result);
      }
    }
  }).on('error', function (err) {
    console.log(err);
  });

  self.waiting.length = 0;
};
//////////////////////////////////////////////////////////////////////////////////
PassiveTotalSource.prototype.init = function() {
  this.key = this.api.getConfig("passivetotal", "key");
  this.user = this.api.getConfig("passivetotal", "user");
  if (this.key === undefined) {
    console.log("PassiveTotal - No key defined");
    return;
  }
  if (this.user === undefined) {
    console.log("PassiveTotal - No user defined");
    return;
  }

  this.cache = LRU({max: this.api.getConfig("passivetotal", "cacheSize", 200000), 
                      maxAge: 1000 * 60 * +this.api.getConfig("passivetotal", "cacheAgeMin", "60")});

  this.api.addSource("passivetotal", this);
  setInterval(this.performQuery.bind(this), 500);

  var str = 
    "if (session.passivetotal)\n" +
    "  div.sessionDetailMeta.bold PassiveTotal\n" +
    "  dl.sessionDetailMeta\n" +
    "    +arrayList(session.passivetotal, 'tags-term', 'Tags', 'passivetotal.tags')\n"

  this.tagsField = this.api.addField("field:passivetotal.tags;db:passivetotal.tags-term;kind:termfield;friendly:Tags;help:PassiveTotal Tags;count:true");

  this.api.addView("passivetotal", str);
};

//////////////////////////////////////////////////////////////////////////////////
PassiveTotalSource.prototype.getDomain = function(domain, cb) {
  var info = this.cache.get(domain);
  if (info) {
    if (info.result !== undefined) {
      this.cached++;
      return cb(null, info.result);
    }
    this.inprogress++;
    info.cbs.push(cb);
    return;
  }
  info = {cbs:[cb]};
  this.cache.set(domain, info);
  this.waiting.push(domain);
  if (this.waiting.length >= 25) {
    this.performQuery();
  }
};
//////////////////////////////////////////////////////////////////////////////////
PassiveTotalSource.prototype.getIp = PassiveTotalSource.prototype.getDomain;
//////////////////////////////////////////////////////////////////////////////////
PassiveTotalSource.prototype.dump = function(res) {
  this.cache.forEach(function(value, key, cache) {
    if (value.result) {
      var str = "{key: \"" + key + "\", ops:\n" + 
        wiseSource.result2Str(wiseSource.combineResults([value.result])) + "},\n";
      res.write(str);
    }
  });
  res.end();
};
//////////////////////////////////////////////////////////////////////////////////
PassiveTotalSource.prototype.printStats = function() {
  console.log("PassiveTotal: outgoing:", this.outgoing, "cached:", this.cached, "inprogress:", this.inprogress, "size:", this.cache.itemCount);
};
//////////////////////////////////////////////////////////////////////////////////
var source;
exports.initSource = function(api) {
  source = new PassiveTotalSource(api, "passivetotal");
  source.init();
};
//////////////////////////////////////////////////////////////////////////////////
