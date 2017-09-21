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
  , wiseSource     = require('./wiseSource.js')
  , ini            = require('iniparser')
  ;

//////////////////////////////////////////////////////////////////////////////////
function RightClickSource (api, section) {
  RightClickSource.super_.call(this, api, section);

  this.file    = api.getConfig(section, "file");

  if (this.file === undefined) {
    console.log(this.section, "- ERROR not loading", this.section, "since no file specified in config file");
    return;
  }

  if (!fs.existsSync(this.file)) {
    console.log(this.section, "- ERROR not loading", this.section, "since", this.file, "doesn't exist");
    return;
  }

  setImmediate(this.load.bind(this));

  // Watch file for changes, combine multiple changes into one, on move restart watch after a pause
  this.watchTimeout = null;
  let watchCb = (event, filename) => {
    clearTimeout(this.watchTimeout);
    if (event === "rename") {
      this.watch.close();
      setTimeout(() => {
        this.load();
        this.watch = fs.watch(this.file, watchCb);
      }, 500);
    } else {
      this.watchTimeout = setTimeout(() => {
        this.watchTimeout = null;
        this.load();
      }, 2000);
    }
  };
  this.watch = fs.watch(this.file, watchCb);
}
util.inherits(RightClickSource, wiseSource);
//////////////////////////////////////////////////////////////////////////////////
RightClickSource.prototype.load = function() {
  if (!fs.existsSync(this.file)) {
    console.log(this.section, "- ERROR not loading", this.section, "since", this.file, "doesn't exist");
    return;
  }

  var config = ini.parseSync(this.file);
  var data = config["right-click"] || config;

  var keys = Object.keys(data);
  if (!keys) {return;}

  keys.forEach((key) => {
    var obj = {};
    data[key].split(';').forEach((element) => {
      var i = element.indexOf(':');
      if (i === -1) {
        return;
      }

      var parts = [element.slice(0, i), element.slice(i+1)];
      if (parts[1] === "true") {
        parts[1] = true;
      } else if (parts[1] === "false") {
        parts[1] = false;
      }
      obj[parts[0]] = parts[1];
    });
    if (obj.fields) {
      obj.fields = obj.fields.split(",");
    }
    if (obj.users) {
      var users = {};
      obj.users.split(",").forEach((item) => {
        users[item] = 1;
      });
      obj.users = users;
    }
    this.api.addRightClick(key, obj);
  });
};
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var sections = api.getConfigSections().filter((e) => {return e.match(/(^right-click$|^right-click:)/);});
  sections.forEach((section) => {
    var source = new RightClickSource(api, section);
  });
};
//////////////////////////////////////////////////////////////////////////////////
