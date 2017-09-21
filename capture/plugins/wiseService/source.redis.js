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

var util           = require('util')
  , wiseSource     = require('./wiseSource.js')
  , redis          = require('redis')
  ;

//////////////////////////////////////////////////////////////////////////////////
function RedisSource (api, section) {
  RedisSource.super_.call(this, api, section);
  this.url      = api.getConfig(section, "url");
  if (this.url === undefined) {
    console.log(this.section, "- ERROR not loading since no url specified in config file");
    return;
  }

  this.column   = +api.getConfig(section, "column", 0);
  this.format   = api.getConfig(section, "format", "csv");
  this.template = api.getConfig(section, "template", undefined);


  this.tagsSetting();
  this.typeSetting();
  if (!this.formatSetting())
    return;

  this.client = redis.createClient({url: this.url});
  this[this.typeFunc] = RedisSource.prototype.fetch;
  this.api.addSource(this.section, this);
}
util.inherits(RedisSource, wiseSource);

//////////////////////////////////////////////////////////////////////////////////
RedisSource.prototype.fetch = function (key, cb) {
  if (this.template !== undefined) {
    key = this.template.replace("%key%", key).replace("%type%", this.type);
  }

  this.client.get(key, (err, reply) => {
    if (reply === null) {
      return cb(null, undefined);
    }

    this.parse(reply, (ignorekey, result) => {
      var newresult = {num: result.num + this.tagsResult.num, buffer: Buffer.concat([result.buffer, this.tagsResult.buffer])};
      return cb(null, newresult);
    }, () => {});
  });
}
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var sections = api.getConfigSections().filter((e) => {return e.match(/^redis:/);});
  sections.forEach((section) => {
    var source = new RedisSource(api, section);
  });
};
//////////////////////////////////////////////////////////////////////////////////
