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
  , simpleSource   = require('./simpleSource.js')
  , request        = require('request')
  ;

//////////////////////////////////////////////////////////////////////////////////
function URLSource (api, section) {
  URLSource.super_.call(this, api, section);
  this.url          = api.getConfig(section, "url");
  this.reload       = +api.getConfig(section, "reload", -1);
  this.headers      = {};
  var headers       = api.getConfig(section, "headers");
  this.cacheTimeout = -1;

  if (this.url === undefined) {
    console.log(this.section, "- ERROR not loading since no url specified in config file");
    return;
  }

  if (headers) {
    headers.split(";").forEach((header) => {
      var parts = header.split(":").map(item => item.trim());
      if (parts.length === 2) {
        this.headers[parts[0]] = parts[1];
      }
    });
  }

  if (!this.initSimple())
    return;

  setImmediate(this.load.bind(this));

  // Reload url every so often
  if (this.reload > 0) {
    setInterval(this.load.bind(this), this.reload*1000*60);
  }
}
util.inherits(URLSource, simpleSource);
//////////////////////////////////////////////////////////////////////////////////
URLSource.prototype.simpleSourceLoad = function(setFunc, cb) {
  request(this.url, {headers: this.headers}, (error, response, body) => {
    if (!error && response.statusCode === 200) {
      this.parse(body, setFunc, cb);
    } else {
      cb(error);
    }
  });
};
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var sections = api.getConfigSections().filter((e) => {return e.match(/^url:/);});
  sections.forEach((section) => {
    var source = new URLSource(api, section);
  });
};
//////////////////////////////////////////////////////////////////////////////////
