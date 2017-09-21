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
  ;

//////////////////////////////////////////////////////////////////////////////////
function PassiveTotalSource (api, section) {
  PassiveTotalSource.super_.call(this, api, section);

  this.key = this.api.getConfig("passivetotal", "key");
  this.user = this.api.getConfig("passivetotal", "user");
  if (this.key === undefined) {
    console.log(this.section, "- No key defined");
    return;
  }
  if (this.user === undefined) {
    console.log(this.section, "- No user defined");
    return;
  }

  this.waiting      = [];
  this.processing   = {};

  this.api.addSource("passivetotal", this);

  setInterval(this.performQuery.bind(this), 500);

  var str =
    "if (session.passivetotal)\n" +
    "  div.sessionDetailMeta.bold PassiveTotal\n" +
    "  dl.sessionDetailMeta\n" +
    "    +arrayList(session.passivetotal, 'tags-term', 'Tags', 'passivetotal.tags')\n";

  this.tagsField = this.api.addField("field:passivetotal.tags;db:passivetotal.tags-term;kind:termfield;friendly:Tags;help:PassiveTotal Tags;count:true");

  this.api.addView("passivetotal", str);
}
util.inherits(PassiveTotalSource, wiseSource);

//////////////////////////////////////////////////////////////////////////////////
PassiveTotalSource.prototype.performQuery = function () {
  if (this.waiting.length === 0) {
    return;
  }

  if (this.api.debug > 0) {
    console.log(this.section, "- Fetching %d", this.waiting.length);
  }

  var options = {
      url: 'https://api.passivetotal.org/v2/enrichment/bulk',
      body: {additional: ["osint", "malware"],
             query: this.waiting},
      auth: {
        user: this.user,
        pass: this.key
      },
      method: 'GET',
      json: true
  };

  var req = request(options, (err, im, results) => {
    if (err) {
      console.log(this.section, "- Error parsing for request:\n", options, "\nresults:\n", results);
      results = {results:{}};
    }

    for (var resultname in results.results) {
      var result = results.results[resultname];
      var cbs = this.processing[resultname];
      if (!cbs) {
        return;
      }
      delete this.processing[resultname];

      var wiseResult;
      if (result.tags === undefined || result.tags.length === 0) {
        wiseResult = wiseSource.emptyResult;
      } else {
        var args = [];
        for (var i = 0; i < result.tags.length; i++) {
          if (typeof(result.tags[i]) === "string") {
            args.push(this.tagsField, result.tags[i]);
          }
        }

        wiseResult = {num: args.length/2, buffer: wiseSource.encode.apply(null, args)};
      }

      var cb;
      while ((cb = cbs.shift())) {
        cb(null, wiseResult);
      }
    }
  }).on('error', (err) => {
    console.log(this.section, err);
  });

  this.waiting.length = 0;
};
//////////////////////////////////////////////////////////////////////////////////
PassiveTotalSource.prototype.fetch = function(key, cb) {
  if (key in this.processing) {
    this.processing[key].push(cb);
    return;
  }

  this.processing[key] = [cb];
  this.waiting.push(key);
  if (this.waiting.length >= 25) {
    this.performQuery();
  }
};
//////////////////////////////////////////////////////////////////////////////////
PassiveTotalSource.prototype.getIp     = PassiveTotalSource.prototype.fetch;
PassiveTotalSource.prototype.getDomain = PassiveTotalSource.prototype.fetch;
//////////////////////////////////////////////////////////////////////////////////
var source;
exports.initSource = function(api) {
  source = new PassiveTotalSource(api, "passivetotal");
};
//////////////////////////////////////////////////////////////////////////////////
