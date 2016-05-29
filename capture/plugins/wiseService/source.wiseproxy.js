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
function WiseProxySource (api, section) {
  WiseProxySource.super_.call(this, api, section);

  this.url    = api.getConfig(section, "url");
  this.types  = api.getConfig(section, "types");
  this.mapping = [];
  this.buffer = new Buffer(10000);
  this.offset = 0;
  this.bufferInfo = [];

  this.caches = [
     LRU({max: this.api.getConfig(section, "cacheSize", 50000), maxAge: 1000 * 60 * +this.api.getConfig(section, "cacheAgeMin", "5")}),
     LRU({max: this.api.getConfig(section, "cacheSize", 50000), maxAge: 1000 * 60 * +this.api.getConfig(section, "cacheAgeMin", "5")}),
     LRU({max: this.api.getConfig(section, "cacheSize", 50000), maxAge: 1000 * 60 * +this.api.getConfig(section, "cacheAgeMin", "5")}),
     LRU({max: this.api.getConfig(section, "cacheSize", 50000), maxAge: 1000 * 60 * +this.api.getConfig(section, "cacheAgeMin", "5")})
       ];
}
util.inherits(WiseProxySource, wiseSource);
//////////////////////////////////////////////////////////////////////////////////
WiseProxySource.prototype.dump = function(res) {
  var self = this;
  for (var i = 0; i < 4; i++) {
    self.caches[i].forEach(function(key, value) {
      var str = "{key: \"" + key + "\", ops:\n" + wiseSource.result2Str(value) + "},\n";
      res.write(str);
    });
  }
  res.end();
};
//////////////////////////////////////////////////////////////////////////////////
WiseProxySource.prototype.performQuery = function() {
  var self = this;

  if (self.bufferInfo.length === 0) {
    return;
  }

  var options = {
      url: this.url + "/get",
      method: 'POST',
      body: self.buffer.slice(0, self.offset)
  };

  var bufferInfo = self.bufferInfo;
  self.bufferInfo = [];
  self.offset = 0;

  request(options, function(err, response, body) {
    if (err || response.statusCode != 200) {
      console.log("Wise proxy error", self.section, err || response);
      for (var i = 0; i < bufferInfo.length; i++) {
        bufferInfo[i].cb("Error");
      }

      return;
    }
    var body = new Buffer(body, "binary");
    var offset = 0;
    var fieldsTS = body.readUInt32BE(offset); offset += 4;
    if (fieldsTS != self.fieldsTS) {
      self.updateInfo();
    }
    var ver = body.readUInt32BE(offset); offset += 4;
    for (var i = 0; i < bufferInfo.length; i++) {
      var num = body[offset]; offset += 1;
      var bi = bufferInfo[i];

      if (num === 0) {
        self.caches[bi.type].set(bi.item, wiseSource.emptyResult);
        return bi.cb(null, wiseSource.emptyResult);
      }

      var args = [];
      for (var n = 0; n < num; n++) {
        var field = body[offset]; offset += 1;
        var len = body[offset]; offset += 1;
        var str = body.toString("ascii", offset, offset+len-1); offset += len;
        args.push(self.mapping[field], str);
      }
      var result = {num: args.length/2, buffer: wiseSource.encode.apply(null, args)};
      self.caches[bi.type].set(bi.item, result);
      return bi.cb(null, result);
    }
  });

};
//////////////////////////////////////////////////////////////////////////////////
WiseProxySource.prototype.fetch = function(type, item, cb) {
  var self = this;

  var c = self.caches[type].get(item);
  if (c !== undefined) {
    return cb(null, c);
  }

  self.buffer[self.offset] = type; self.offset++;
  self.buffer.writeUInt16BE(item.length, self.offset); self.offset += 2;
  self.buffer.write(item, self.offset); self.offset += item.length;
  self.bufferInfo.push({type: type, item: item, cb: cb});

  if (self.bufferInfo.length > 100) {
    self.performQuery();
  }
};
//////////////////////////////////////////////////////////////////////////////////
WiseProxySource.prototype.updateInfo = function() {
  var self = this;
  var options = {
      url: this.url + "/fields",
      method: 'GET'
  };

  request(options, function(err, response, body) {
    if (err) {
      console.log("Wise proxy problem fetching /fields", self.section, err || response);
      return;
    }
    var buf = new Buffer(body, "binary");
    var offset = 0;
    self.fieldsTS = buf.readUInt32BE(offset); offset += 4;
    var version = buf.readUInt32BE(offset); offset += 4;
    var length = buf[offset]; offset += 1;
    for (var i = 0; i < length; i++) {
      offset++;
      var len = buf[offset]; offset += 1;
      var str = buf.toString("ascii", offset, offset+len);

      offset += len;
      self.mapping[i] = self.api.addField(str);
    }
  });

  options = {
      url: this.url + "/views",
      method: 'GET',
      json: true
  };
  request(options, function(err, response, body) {
    if (err) {
      console.log("Wise proxy problem fetching /views", self.section, err || response);
      return;
    }
     for (var name in body) {
       self.api.addView(name, body[name]);
     }
  });

  options = {
      url: this.url + "/rightClicks",
      method: 'GET',
      json: true
  };
  request(options, function(err, response, body) {
    if (err) {
      console.log("Wise proxy problem fetching /rightClicks", self.section, err || response);
      return;
    }
     for (var name in body) {
       self.api.addView(name, body[name]);
     }
  });

};
//////////////////////////////////////////////////////////////////////////////////
WiseProxySource.prototype.init = function() {
  var self = this;

  if (this.url === undefined) {
    console.log("WiseProxy - ERROR not loading", this.section, "since no url specified in config file");
    return;
  }

  if (!this.types) {
    console.log("WiseProxy - ERROR not loading", this.section, "since no types specified in config file");
    return;
  }

  var types = this.types.split(",");

  for (var i = 0; i < types.length; i++) {
    switch(types[i]) {
    case "domain":
      this.getDomain = function(item, cb) {
        this.fetch(1, item, cb);
      }
      break;
    case "md5":
      this.getMd5 = function(item, cb) {
        this.fetch(2, item, cb);
      }
      break;
    case "ip":
      this.getIp = function(item, cb) {
        this.fetch(0, item, cb);
      }
      break;
    case "email":
      this.getEmail = function(item, cb) {
        this.fetch(3, item, cb);
      }
      break;
    }
  }

  self.updateInfo();
  setTimeout(this.updateInfo.bind(this), 5*60*1000);

  this.api.addSource(this.section, this);
  setInterval(this.performQuery.bind(this), 500);
};
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var sections = api.getConfigSections().filter(function(e) {return e.match(/^wiseproxy:/);});
  sections.forEach(function(section) {
    var source = new WiseProxySource(api, section);
    source.init();
  });
};
//////////////////////////////////////////////////////////////////////////////////
