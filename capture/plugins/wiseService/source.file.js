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
'use strict';

var fs             = require('fs')
  , util           = require('util')
  , wiseSource     = require('./wiseSource.js')
  , iptrie         = require('iptrie')
  ;

//////////////////////////////////////////////////////////////////////////////////
function FileSource (api, section) {
  FileSource.super_.call(this, api, section);

  this.file    = api.getConfig(section, "file");
  this.column  = +api.getConfig(section, "column", 0);
  this.type    = api.getConfig(section, "type");
  this.format  = api.getConfig(section, "format", "csv");
  if (this.type === "ip") {
    this.cache = {items: [], trie: new iptrie.IPTrie()};
  } else {
    this.cache = {};
  }
}
util.inherits(FileSource, wiseSource);
//////////////////////////////////////////////////////////////////////////////////
FileSource.prototype.load = function() {
  var self = this;
  var setFunc;
  var newCache;
  var count = 0;
  if (this.type === "ip") {
    newCache = {items: [], trie: new iptrie.IPTrie()};
    setFunc  = function(key, value) {
      var parts = key.split("/");
      try {
        newCache.trie.add(parts[0], +parts[1] || 32, value);
      } catch (e) {
        console.log("ERROR adding", this.section, key, e);
      }
      newCache.items[key] = value;
      count++;
    };
  } else {
    newCache ={};
    setFunc = function(key, value) {
      newCache[key] = value;
      count++;
    };
  }
  fs.readFile(self.file, function (err, body) {
    if (!err) {
      self.parse(body, setFunc, function(err) {
        if (err) {
          console.log("ERROR loading", self.section, err);
          return;
        }
        self.cache = newCache;
        console.log(self.section, "- Done Loading", count, "elements");
      });
    } else {
      console.log("Couldn't load", self.section, self.file, err);
    }
  });
};
//////////////////////////////////////////////////////////////////////////////////
FileSource.prototype.dump = function(res) {
  var cache = this.type === "ip"?this.cache.items:this.cache;
  for (var key in cache) {
    var str = "{key: \"" + key + "\", ops:\n" + 
      wiseSource.result2Str(wiseSource.combineResults([this.result, cache[key]])) + "},\n";
    res.write(str);
  }
  res.end();
};
//////////////////////////////////////////////////////////////////////////////////
FileSource.prototype.sendResult = function(key, cb) {
  var result = this.type === "ip"?this.cache.trie.find(key):this.cache[key];

  // Not found, or found but no extra values to add
  if (!result) {
    return cb(null, undefined);
  }
  if (result.num === 0) {
    return cb(null, this.result);
  }

  // Found, so combine the two results (per item, and per source)
  var newresult = {num: result.num + this.result.num, buffer: Buffer.concat([result.buffer, this.result.buffer])};
  return cb(null, newresult);
};
//////////////////////////////////////////////////////////////////////////////////
FileSource.prototype.init = function() {
  var self = this;

  if (this.file === undefined) {
    console.log("FILE - ERROR not loading", this.section, "since no file specified in config file");
    return;
  }

  if (!fs.existsSync(this.file)) {
    console.log("FILE - ERROR not loading", this.section, "since", this.file, "doesn't exist");
    return;
  }

  if (!this.type) {
    console.log("FILE - ERROR not loading", this.section, "since no type specified in config file");
    return;
  }

  var tagsField = this.api.addField("field:tags");
  var tags = this.api.getConfig(this.section, "tags");
  if (!tags) {
    console.log("FILE - ERROR not loading", this.section, "since no tags specified in config file");
    return;
  }
  var args = [];
  tags.split(",").forEach(function (part) {
    args.push(tagsField, part);
  });
  this.result = {num: args.length/2, buffer: wiseSource.encode.apply(null, args)};


  if (this.format === "csv") {
    this.parse = this.parseCSV;
  } else if (this.format === "tagger") {
    this.parse = this.parseTagger;
  } else {
    console.log("FILE - ERROR not loading", this.section, "unknown data format", this.format);
    return;
  }

  if (this.type === "domain") {
    this.getDomain = function(domain, cb) {
      if (this.cache[domain]) {
        return this.sendResult(domain, cb);
      }
      domain = domain.substring(domain.indexOf(".")+1);
      return this.sendResult(domain, cb);
    };
  } else if (this.type === "ip") {
    this.getIp = this.sendResult;
  } else if (this.type === "md5") {
    this.getMd5 = this.sendResult;
  } else if (this.type === "email") {
    this.getEmail = this.sendResult;
  } else {
    console.log("FILE - ERROR not loading", this.section, "since unknown type specified in config file", this.type);
    return;
  }

  this.api.addSource(this.section, this);
  setImmediate(this.load.bind(this));


  // Watch file for changes, combine multiple changes into one, on move restart watch after a pause
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
};
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var sections = api.getConfigSections().filter(function(e) {return e.match(/^file:/);});
  sections.forEach(function(section) {
    var source = new FileSource(api, section);
    source.init();
  });
};
//////////////////////////////////////////////////////////////////////////////////
