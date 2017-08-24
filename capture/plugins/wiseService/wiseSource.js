/******************************************************************************/
/* Base class for data sources
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

var csv            = require('csv')
  , request        = require('request')
  , fs             = require('fs')
  , iptrie         = require('iptrie')
  ;

function WISESource (api, section) {
  var self = this;
  self.api = api;
  self.section = section;
  self.view = "";
  self.shortcuts = {};
  self.cacheTimeout = 60 * +this.api.getConfig(section, "cacheAgeMin", "60"); // Default an hour
  self.cacheHitStat = 0;
  self.cacheMissStat = 0;
  self.cacheRefreshStat = 0;
  self.cacheDroppedStat = 0;
  self.average100MS = 0;

  // Domain and Email wildcards to exclude from source
  ["excludeDomains", "excludeEmails", "excludeURLs"].forEach(function(type) {
    var items = api.getConfig(section, type);
    self[type] = [];
    if (!items) {return;}
    items.split(";").forEach(function(item) {
      if (item === "") {
        return;
      }
      self[type].push(RegExp.fromWildExp(item, "ailop"));
    });
  });

  // IP CIDRs to exclude from source
  self.excludeIPs = new iptrie.IPTrie();
  var items = api.getConfig(section, "excludeIPs", "");
  items.split(";").forEach(function(item) {
    if (item === "") {
      return;
    }
    var parts = item.split("/");
    self.excludeIPs.add(parts[0], +parts[1] || 32, true);
  });

  items = api.getConfig(section, "onlyIPs", undefined);
  if (items) {
    self.onlyIPs = new iptrie.IPTrie();
    items.split(";").forEach(function(item) {
      if (item === "") {
        return;
      }
      var parts = item.split("/");
      self.onlyIPs.add(parts[0], +parts[1] || 32, true);
    });
  }

  // fields defined for source
  var fields = api.getConfig(section, "fields");
  if (fields !== undefined) {
    fields = fields.split("\\n");
    for (var i = 0; i < fields.length; i++) {
      self.parseFieldDef(fields[i]);
    }
  }

  // views defined for source
  var view = api.getConfig(section, "view");
  if (view !== undefined) {
    self.view = view.replace(/\\n/g, "\n");
  }

  if (self.view !== "") {
    self.api.addView(self.section, self.view);
  }
}

module.exports = WISESource;

WISESource.emptyResult = {num: 0, buffer: new Buffer(0)};
WISESource.field2Pos = {};
WISESource.pos2Field = {};

//////////////////////////////////////////////////////////////////////////////////
//https://coderwall.com/p/pq0usg/javascript-string-split-that-ll-return-the-remainder
function splitRemain(str, separator, limit) {
    str = str.split(separator);
    if(str.length <= limit) {return str;}

    var ret = str.splice(0, limit);
    ret.push(str.join(separator));

    return ret;
}
//////////////////////////////////////////////////////////////////////////////////
WISESource.prototype.parseCSV = function (body, setCb, endCb) {
  var self = this;

  var parser = csv.parse(body, {skip_empty_lines: true, comment: '#', relax_column_count: true}, function(err, data) {
    if (err) {
      return endCb(err);
    }

    for (var i = 0; i < data.length; i++) {
      var args = [];
      for (var k in self.shortcuts) {
        if (data[i][k] !== undefined) {
          args.push(self.shortcuts[k]);
          args.push(data[i][k]);
        }
      }

      if (args.length === 0) {
        setCb(data[i][self.column], WISESource.emptyResult);
      } else {
        setCb(data[i][self.column], {num: args.length/2, buffer: WISESource.encode.apply(null, args)});
      }
    }
    endCb(err);
  });
};
//////////////////////////////////////////////////////////////////////////////////
WISESource.prototype.parseFieldDef = function(line) {
  if (line[0] === "#") {
    line = line.substring(1);
  }

  if (line.lastIndexOf('field:',0) === 0) {
    var pos = this.api.addField(line);
    var match = line.match(/shortcut:([^;]+)/);
    if (match) {
      this.shortcuts[match[1]] = pos;
    }
  } else if (line.lastIndexOf('view:',0) === 0) {
      this.view += line.substring(5) + "\n";
  }
};
//////////////////////////////////////////////////////////////////////////////////
WISESource.prototype.parseTagger = function(body, setCb, endCb) {
  var lines = body.toString().split(/\r?\n/);
  for (var l = 0, llen = lines.length; l < llen; l++) {
    if (lines[l][0] === "#") {
      this.parseFieldDef(lines[l]);
      continue;
    }

    if (lines[l].match(/^\s*$/)) {
      continue;
    }

    var args = [];
    var parts = lines[l].split(";");
    for (var p = 1; p < parts.length; p++) {
      var kv = splitRemain(parts[p], '=', 1);
      if (kv.length !== 2) {
        console.log("WARNING -", this.section, "- ignored extra piece '" + parts[p] + "' from line '" + lines[l] + "'");
        continue;
      }
      if (this.shortcuts[kv[0]] !== undefined) {
        args.push(this.shortcuts[kv[0]]);
      } else if (WISESource.field2Pos[kv[0]]) {
        args.push(WISESource.field2Pos[kv[0]]);
      } else {
        args.push(this.api.addField("field:" + kv[0]));
      }
      args.push(kv[1]);
    }
    setCb(parts[0], {num: args.length/2, buffer: WISESource.encode.apply(null, args)});
  }
  if (this.view !== "") {
    this.api.addView(this.section, this.view);
  }
  endCb(null);
};
//////////////////////////////////////////////////////////////////////////////////
WISESource.prototype.parseJSON = function (body, setCb, endCb) {
  var self = this;
  var json = JSON.parse(body);

  if (self.keyColumn === undefined) {
    return endCb("No keyColumn set");
  }

  for(var i = 0; i < json.length; i++) {
    var key = json[i][self.keyColumn];
    var args = [];
    if (key === undefined) {
      continue;
    }
    for (var k in self.shortcuts) {
      if (json[i][k] !== undefined && json[i][k] !== null) {
        args.push(self.shortcuts[k]);
        args.push(json[i][k]);
      }
    }
    setCb(key, {num: args.length/2, buffer: WISESource.encode.apply(null, args)});
  }
  endCb(null);
};
//////////////////////////////////////////////////////////////////////////////////
WISESource.combineResults = function(results)
{
  var a, num = 0, len = 1;
  for (a = 0; a < results.length; a++) {
    if (!results[a]) {
      continue;
    }
    num += results[a].num;
    len += results[a].buffer.length;
  }

  var buf = new Buffer(len);
  var offset = 1;
  for (a = 0; a < results.length; a++) {
    if (!results[a]) {
      continue;
    }

    results[a].buffer.copy(buf, offset);
    offset += results[a].buffer.length;
  }
  buf[0] = num;
  return buf;
};
//////////////////////////////////////////////////////////////////////////////////
WISESource.result2Str = function(result, indent) {
  if (!indent) {
    indent = "";
  }
  
  var str = "[";
  var offset = 1;
  for (var i = 0; i < result[0]; i++) {
    var pos   = result[offset];
    var len   = result[offset+1];
    var value = result.toString('utf8', offset+2, offset+2+len-1);
    offset += 2 + len;
    if (i > 0) {
      str += ",\n";
    }
    str += indent + "{field: \"" + WISESource.pos2Field[pos] + "\", len: " + len + ", value: \"" + value + "\"}";
  }

  return str + "]\n";
};
//////////////////////////////////////////////////////////////////////////////////
WISESource.encode = function ()
{
  var a, l, len = 0;
  for (a = 1; a < arguments.length; a += 2) {
    l = Buffer.byteLength(arguments[a]);
    if (l > 250) {
      arguments[a] = arguments[a].substring(0, 240);
    }
    len += 3 + Buffer.byteLength(arguments[a]);
  }

  var buf = new Buffer(len);
  var offset = 0;
  for (a = 1; a < arguments.length; a += 2) {
      buf.writeUInt8(arguments[a-1], offset);
      len = Buffer.byteLength(arguments[a]);
      buf.writeUInt8(len+1, offset+1);
      l = buf.write(arguments[a], offset+2);
      buf.writeUInt8(0, offset+l+2);
      offset += 3 + l;
  }
  return buf;
};
//////////////////////////////////////////////////////////////////////////////////
WISESource.request = function (url, file, cb) {
  var headers = {};
  if (file) {
    if (fs.existsSync(file)) {
      var stat = fs.statSync(file);

      // Don't download again if file is less then 1 minutes old
      if (Date.now() - stat.mtime.getTime() < 60000) {
        return setImmediate(cb, 304);
      }
      headers['If-Modified-Since'] = stat.mtime.toUTCString();
    }
  }
  var statusCode;
  console.log(url);
  request({url: url, headers: headers})
  .on('response', function(response) {
    statusCode = response.statusCode;
    if (response.statusCode === 200) {
      this.pipe(fs.createWriteStream(file));
    }
  })
  .on('error', function(error) {
    console.log(error);
  })
  .on('end', function() {
    setImmediate(cb, statusCode);
  })
  ;
};
//////////////////////////////////////////////////////////////////////////////////
WISESource.prototype.tagsSetting = function () {
  var tagsField = this.api.addField("field:tags");
  var tags = this.api.getConfig(this.section, "tags");
  if (tags) {
    var args = [];
    tags.split(",").forEach(function (part) {
      args.push(tagsField, part);
    });
    this.tagsResult = {num: args.length/2, buffer: WISESource.encode.apply(null, args)};
  } else {
    this.tagsResult = WISESource.emptyResult;
  }
}
//////////////////////////////////////////////////////////////////////////////////
WISESource.prototype.formatSetting = function () {
  this.format  = this.api.getConfig(this.section, "format", "csv");
  if (this.format === "csv") {
    this.parse = this.parseCSV;
  } else if (this.format === "tagger") {
    this.parse = this.parseTagger;
  } else if (this.format === "json") {
    this.parse = this.parseJSON;
  } else {
    console.log(this.section, "- ERROR not loading unknown data format", this.format);
    return false;
  }
  return true;
}
//////////////////////////////////////////////////////////////////////////////////
var typeName2Func = {ip: "getIp", domain: "getDomain", md5: "getMd5", email: "getEmail", url: "getURL", tuple: "getTuple", ja3: "getJa3"};
WISESource.prototype.typeSetting = function ()
{
  this.type     = this.api.getConfig(this.section, "type");
  this.typeFunc = typeName2Func[this.type];
  if (this.typeFunc === undefined) {
    console.log(this.section, "ERROR - unknown type", this.type);
    return;
  }
}
//////////////////////////////////////////////////////////////////////////////////
WISESource.emptyCombinedResult = WISESource.combineResults([]);
