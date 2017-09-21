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

var dns            = require('dns')
  , iptrie         = require('iptrie')
  , wiseSource     = require('./wiseSource.js')
  , util           = require('util')
  ;
//////////////////////////////////////////////////////////////////////////////////
function removeArray(arr, value) {
  var pos = 0;
  while ((pos = arr.indexOf(value, pos)) !== -1) {
    arr.splice(pos, 1);
  }
  return arr;
}
//////////////////////////////////////////////////////////////////////////////////
function ReverseDNSSource (api, section) {
  ReverseDNSSource.super_.call(this, api, section);
  this.field        = api.getConfig("reversedns", "field");
  this.ips          = api.getConfig("reversedns", "ips");
  this.stripDomains = removeArray(api.getConfig("reversedns", "stripDomains", "").split(";"), "");

  if (this.field === undefined) {
    console.log(this.section, "- No field defined");
    return;
  }

  if (this.ips === undefined) {
    console.log(this.section, "- No ips defined");
    return;
  }

  this.theField = this.api.addField(`field:${this.field}`);
  this.trie = new iptrie.IPTrie();
  this.ips.split(";").forEach((item) => {
    if (item === "") {
      return;
    }
    var parts = item.split("/");
    this.trie.add(parts[0], +parts[1] || 32, true);
  });

  this.api.addSource("reversedns", this);
}
util.inherits(ReverseDNSSource, wiseSource);
//////////////////////////////////////////////////////////////////////////////////
ReverseDNSSource.prototype.getIp = function(ip, cb) {
  if (!this.trie.find(ip)) {
    return cb(null, undefined);
  }

  dns.reverse(ip, (err, domains) => {
    //console.log("answer", ip, err, domains);
    if (err || domains.length === 0) {
      return cb(null, wiseSource.emptyResult);
    }
    var args = [];
    for (var i = 0; i < domains.length; i++) {
      var domain = domains[i];
      if (this.stripDomains.length === 0) {
        var parts = domain.split(".");
        args.push(this.theField, parts[0].toLowerCase());
      } else {
        for (var j = 0; j < this.stripDomains.length; j++) {
          var stripDomain = this.stripDomains[j];
          if (domain.indexOf(stripDomain, domain.length - stripDomain.length) !== -1) {
            args.push(this.theField, domain.slice(0, domain.length - stripDomain.length));
          }
        }
      }
    }
    var wiseResult = {num: args.length/2, buffer: wiseSource.encode.apply(null, args)};
    cb(null, wiseResult);
  });
};
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var source = new ReverseDNSSource(api, "reversedns");
};
//////////////////////////////////////////////////////////////////////////////////
