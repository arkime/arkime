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
  , request        = require('request')
  ;

//////////////////////////////////////////////////////////////////////////////////
function URLSource (api, section) {
  URLSource.super_.call(this, api, section);
  var self = this;

  self.url     = api.getConfig(section, "url");
  self.reload  = +api.getConfig(section, "reload", -1)
  self.column  = +api.getConfig(section, "column", 0);
  this.keyColumn  = api.getConfig(section, "keyColumn", 0);
  self.type    = api.getConfig(section, "type");
  self.format  = api.getConfig(section, "format", "csv");
  self.headers = {};
  var headers = api.getConfig(section, "headers");
  if (headers) {
    headers.split(";").forEach(function(header) {
      var parts = header.split(":");
      if (parts.length === 2) {
        self.headers[parts[0].trim()] = parts[1].trim();
      }
    });
  }
  
  if (self.type === "ip") {
    self.cache = {items: [], trie: new iptrie.IPTrie()};
  } else {
    self.cache = {};
  }
  var fields = api.getConfig(section, "fields");
  if (fields !== undefined) {
    fields = fields.split("\\n");
    for (var i = 0; i < fields.length; i++) {
      this.parseFieldDef(fields[i]);
    }
  }

  var view = api.getConfig(section, "view");
  if (view !== undefined) {
    this.view = view.replace(/\\n/g, "\n");
  }

  if (this.view !== "") {
    this.api.addView(this.section, this.view);
  }
}
util.inherits(URLSource, wiseSource);
//////////////////////////////////////////////////////////////////////////////////
URLSource.prototype.load = function() {
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
        console.log("ERROR adding", self, key, e);
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
  request(self.url, {headers: self.headers}, function (error, response, body) {
    if (!error && response.statusCode === 200) {
      self.parse(body, setFunc, function(err) {
        if (err) {
          console.log("ERROR loading", self.section, err);
          return;
        }
        self.cache = newCache;
        console.log(self.section, "- Done Loading", count, "elements");
      });
    } else {
      console.log("Couldn't load", self.section, self.url, response, error);
    }
  });
};
//////////////////////////////////////////////////////////////////////////////////
URLSource.prototype.dump = function(res) {
  var cache = this.type === "ip"?this.cache.items:this.cache;
  for (var key in cache) {
    var str = "{key: \"" + key + "\", ops:\n" + 
      wiseSource.result2Str(wiseSource.combineResults([this.result, cache[key]])) + "},\n";
    res.write(str);
  }
  res.end();
};
//////////////////////////////////////////////////////////////////////////////////
URLSource.prototype.sendResult = function(key, cb) {
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
URLSource.prototype.init = function() {
  if (this.url === undefined) {
    console.log("URL - ERROR not loading", this.section, "since no url specified in config file");
    return;
  }

  if (!this.type) {
    console.log("URL - ERROR not loading", this.section, "since no type specified in config file");
    return;
  }

  var tagsField = this.api.addField("field:tags");
  var tags = this.api.getConfig(this.section, "tags");
  if (tags) {
    var args = [];
    tags.split(",").forEach(function (part) {
      args.push(tagsField, part);
    });
    this.result = {num: args.length/2, buffer: wiseSource.encode.apply(null, args)};
  } else {
    this.result = wiseSource.emptyResult;
  }


  if (this.format === "csv") {
    this.parse = this.parseCSV;
  } else if (this.format === "tagger") {
    this.parse = this.parseTagger;
  } else if (this.format === "json") {
    this.parse = this.parseJSON;
  } else {
    console.log("URL - ERROR not loading", this.section, "unknown data format", this.format);
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
    console.log("URL - ERROR not loading", this.section, "since unknown type specified in config file", this.type);
    return;
  }

  this.api.addSource(this.section, this);
  setImmediate(this.load.bind(this));
  if (this.reload > 0) {
    setInterval(this.load.bind(this), this.reload*1000*60);
  }
};
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var sections = api.getConfigSections().filter(function(e) {return e.match(/^url:/);});
  sections.forEach(function(section) {
    var source = new URLSource(api, section);
    source.init();
  });
};
//////////////////////////////////////////////////////////////////////////////////
