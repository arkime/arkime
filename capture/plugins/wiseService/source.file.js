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

var fs             = require('fs')
  , util           = require('util')
  , simpleSource   = require('./simpleSource.js')
  ;

//////////////////////////////////////////////////////////////////////////////////
function FileSource (api, section) {
  FileSource.super_.call(this, api, section);

  this.file    = api.getConfig(section, "file");
  this.cacheTimeout = -1;

  if (this.file === undefined) {
    console.log(this.section, "- ERROR not loading since no file specified in config file");
    return;
  }

  if (!fs.existsSync(this.file)) {
    console.log(this.section, "- ERROR not loading since", this.file, "doesn't exist");
    return;
  }

  if (!this.initSimple())
    return;

  setImmediate(this.load.bind(this));

  // Watch file for changes, combine multiple changes into one, on move restart watch after a pause
  var self = this;
  self.watchTimeout = null;
  self.watch = fs.watch(this.file, function watchCb(event, filename) {
    clearTimeout(self.watchTimeout);
    if (event === "rename") {
      self.watch.close();
      setTimeout(function () {
        self.load();
        self.watch = fs.watch(self.file, watchCb);
      }, 500);
    } else {
      self.watchTimeout = setTimeout(function () {
        self.watchTimeout = null;
        self.load();
      }, 2000);
    }
  });
}
util.inherits(FileSource, simpleSource);
//////////////////////////////////////////////////////////////////////////////////
FileSource.prototype.simpleSourceLoad = function(setFunc, cb) {
  var self = this;

  fs.readFile(self.file, function (err, body) {
    if (err) {
      return cb(err);
    }
    self.parse(body, setFunc, cb);
  });
};
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var sections = api.getConfigSections().filter(function(e) {return e.match(/^file:/);});
  sections.forEach(function(section) {
    var source = new FileSource(api, section);
  });
};
//////////////////////////////////////////////////////////////////////////////////
