/******************************************************************************/
/* hodiredis - History of Observed Data Indictors
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
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

var wiseSource     = require('./wiseSource.js')
  , util           = require('util')
  , redis          = require('redis')
  ;
//////////////////////////////////////////////////////////////////////////////////
function HODIRedisSource (api, section) {
  HODIRedisSource.super_.call(this, api, section);

  this.contentTypes = {};
  var contentTypes = this.api.getConfig(section, "contentTypes",
          "application/x-dosexec,application/vnd.ms-cab-compressed,application/pdf,application/x-shockwave-flash,application/x-java-applet,application/jar").split(",");

  contentTypes.forEach((type) => { this.contentTypes[type] = 1;});
  this.url      = api.getConfig(section, "url");
  if (this.url === undefined) {
    console.log(this.section, "- ERROR not loading since no url specified in config file");
    return;
  }

  this.fullQuery = true;
  this.client = redis.createClient({url: this.url});
  this.api.addSource(section, this);
  this.cacheTimeout = -1;

  var tagsField = this.api.addField("field:tags");
  this.tagsDomain = {num: 1, buffer: wiseSource.encode(tagsField, "nbs-domain")};
  this.tagsMd5 = {num: 1, buffer: wiseSource.encode(tagsField, "nbs-md5")};
  this.tagsEmail = {num: 1, buffer: wiseSource.encode(tagsField, "nbs-email")};
  this.tagsIp = {num: 1, buffer: wiseSource.encode(tagsField, "nbs-ip")};
}
util.inherits(HODIRedisSource, wiseSource);

//////////////////////////////////////////////////////////////////////////////////
HODIRedisSource.prototype.process = function(key, tag, cb) {
  var date = new Date();

  this.client.hsetnx(key, "first", date.getTime(), (err, result) => {
    if (result === 1) {
      return cb(null, tag);
    } else {
      return cb(null, undefined);
    }
  });
  this.client.hset(key, "last", date.getTime());
  this.client.hincrby(key, "count", 1);
  this.client.hincrby(key, `count:${date.getFullYear()}:${date.getMonth()+1}`, 1);

};
//////////////////////////////////////////////////////////////////////////////////
HODIRedisSource.prototype.getDomain = function(query, cb) {
  return this.process(`d:${query.value}`, this.tagsDomain, cb);
};
//////////////////////////////////////////////////////////////////////////////////
HODIRedisSource.prototype.getMd5 = function(query, cb) {
  if (query.contentType === undefined || this.contentTypes[query.contentType] !== 1) {
    return cb (null, undefined);
  }

  return this.process(`h:${query.value}`, this.tagsMd5, cb);
};
//////////////////////////////////////////////////////////////////////////////////
HODIRedisSource.prototype.getEmail = function(query, cb) {
  return this.process(`e:${query.value}`, this.tagsEmail, cb);
};
//////////////////////////////////////////////////////////////////////////////////
HODIRedisSource.prototype.getIp = function(query, cb) {
  return this.process(`a:${query.value}`, this.tagsIp, cb);
};
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var source = new HODIRedisSource(api, "hodiredis");
};
