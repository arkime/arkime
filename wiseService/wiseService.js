/******************************************************************************/
/* wiseService.js -- Server requests between moloch and various intel services
 *                   and files
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

const ini            = require('iniparser')
    , express        = require('express')
    , fs             = require('fs')
    , http           = require('http')
    , https          = require('https')
    , glob           = require('glob')
    , async          = require('async')
    , sprintf        = require('./sprintf.js').sprintf
    , iptrie         = require('iptrie')
    , wiseSource     = require('./wiseSource.js')
    , wiseCache      = require('./wiseCache.js')
    , cluster        = require("cluster")
    , crypto         = require("crypto")
  ;

require('console-stamp')(console, '[HH:MM:ss.l]');

var internals = {
  configFile: "/data/moloch/etc/wiseService.ini",
  debug: 0,
  insecure: false,
  fieldsTS: 0,
  fields: [],
  fieldsSize: 0,
  sources: [],
  types: {
  },
  views: {},
  rightClicks: {},
  workers: 1
};

//////////////////////////////////////////////////////////////////////////////////
//// Command Line Parsing
//////////////////////////////////////////////////////////////////////////////////
function processArgs(argv) {
  for (var i = 0, ilen = argv.length; i < ilen; i++) {
    if (argv[i] === "-c") {
      i++;
      internals.configFile = argv[i];
    } else if (argv[i] === "--insecure") {
      internals.insecure = true;
    } else if (argv[i] === "--debug") {
      internals.debug++;
    } else if (argv[i] === "--workers") {
      i++;
      internals.workers = +argv[i];
    } else if (argv[i] === "--help") {
      console.log("wiseService.js [<options>]");
      console.log("");
      console.log("Options:");
      console.log("  --debug               Increase debug level, multiple are supported");
      console.log("  --workers <b>         Number of worker processes to create");

      process.exit(0);
    }
  }
}
processArgs(process.argv);

if (internals.workers > 1) {
  if (cluster.isMaster) {
    for (var i = 0; i < internals.workers; i++) {
      cluster.fork();
    }
    cluster.on('exit', (worker, code, signal) => {
      console.log('worker ' + worker.process.pid + ' died, restarting new worker');
      cluster.fork();
    });
  }
}
//////////////////////////////////////////////////////////////////////////////////
//// Config
//////////////////////////////////////////////////////////////////////////////////
internals.config = ini.parseSync(internals.configFile);
var app = express();

var logger = require("morgan");
var timeout = require("connect-timeout");

app.use(logger(':date \x1b[1m:method\x1b[0m \x1b[33m:url\x1b[0m :res[content-length] bytes :response-time ms'));
app.use(timeout(5*1000));

function getConfig(section, name, d) {
  if (!internals.config[section]) {
    return d;
  }
  return internals.config[section][name] || d;
}

function getConfigSections() {
  return Object.keys(internals.config);
}

function getConfigSection(section) {
  return internals.config[section];
}

// Explicit sigint handler for running under docker
// See https://github.com/nodejs/node/issues/4182
process.on('SIGINT', function() {
    process.exit();
});

//////////////////////////////////////////////////////////////////////////////////
//// Sources
//////////////////////////////////////////////////////////////////////////////////
function newFieldsTS()
{
  var now = Math.floor(Date.now()/1000);
  if (now <= internals.fieldsTS) {
    internals.fieldsTS++;
  } else {
    internals.fieldsTS = now;
  }
}
//////////////////////////////////////////////////////////////////////////////////
function addField(field) {
  var match = field.match(/field:([^;]+)/);
  var name = match[1];

  var db;
  if ((match = field.match(/db:([^;]+)/))) {
    db = match[1];
  }

  var friendly;
  if ((match = field.match(/friendly:([^;]+)/))) {
    friendly = match[1];
  }

  if (wiseSource.field2Pos[name] !== undefined) {
    return wiseSource.field2Pos[name];
  }

  var pos = internals.fields.length;
  newFieldsTS();
  internals.fields.push(field);
  internals.fieldsSize += field.length + 10;

  // Create version 0 of fields buf
  if (internals.fields.length < 256) {
    internals.fieldsBuf0 = Buffer.alloc(internals.fieldsSize + 9);
    internals.fieldsBuf0.writeUInt32BE(internals.fieldsTS, 0);
    internals.fieldsBuf0.writeUInt32BE(0, 4);
    internals.fieldsBuf0.writeUInt8(internals.fields.length, 8);
    let offset = 9;
    for (let i = 0; i < internals.fields.length; i++) {
      let len = internals.fieldsBuf0.write(internals.fields[i], offset+2);
      internals.fieldsBuf0.writeUInt16BE(len+1, offset);
      internals.fieldsBuf0.writeUInt8(0, offset+2+len);
      offset += 3 + len;
    }
    internals.fieldsBuf0 = internals.fieldsBuf0.slice(0, offset);
  }

  // Create version 1 of fields buf
  internals.fieldsBuf1 = Buffer.alloc(internals.fieldsSize + 9);
  internals.fieldsBuf1.writeUInt32BE(internals.fieldsTS, 0);
  internals.fieldsBuf1.writeUInt32BE(1, 4);
  internals.fieldsBuf1.writeUInt16BE(internals.fields.length, 8);
  let offset = 10;
  for (let i = 0; i < internals.fields.length; i++) {
    let len = internals.fieldsBuf1.write(internals.fields[i], offset+2);
    internals.fieldsBuf1.writeUInt16BE(len+1, offset);
    internals.fieldsBuf1.writeUInt8(0, offset+2+len);
    offset += 3 + len;
  }
  internals.fieldsBuf1 = internals.fieldsBuf1.slice(0, offset);

  internals.fieldsMd5 = crypto.createHash('md5').update(internals.fieldsBuf1.slice(8)).digest("hex");

  wiseSource.pos2Field[pos] = name;
  wiseSource.field2Pos[name] = pos;
  wiseSource.field2Info[name] = {pos: pos, friendly: friendly, db: db};
  return pos;
}
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
internals.sourceApi = {
  getConfig: getConfig,
  getConfigSections: getConfigSections,
  getConfigSection: getConfigSection,
  addField: addField,
  addView: function (name, input) {
    if (input.includes("require:")) {
      var match = input.match(/require:([^;]+)/);
      var require = match[1];
      match = input.match(/title:([^;]+)/);
      var title = match[1];
      match = input.match(/fields:([^;]+)/);
      var fields = match[1];

      let output = `if (session.${require})\n  div.sessionDetailMeta.bold ${title}\n  dl.sessionDetailMeta\n`;
      for (let field of fields.split(",")) {
        let info = wiseSource.field2Info[field];
        if (!info) {
          continue;
        }
        if (!info.db) {
          console.log(`ERROR, missing db information for ${field}`);
          process.exit(0);
        }
        var parts = splitRemain(info.db, '.', 1);
        if (parts.length === 1) {
          output += `    +arrayList(session, '${parts[0]}', '${info.friendly}', '${field}')\n`;
        } else {
          output += `    +arrayList(session.${parts[0]}, '${parts[1]}', '${info.friendly}', '${field}')\n`;
        }
      }
      internals.views[name] = output;
    } else {
      internals.views[name] = input;
    }
  },
  addRightClick: function (name, rightClick) {
    internals.rightClicks[name] = rightClick;
  },
  debug: internals.debug,
  insecure: internals.insecure,
  addSource: function(section, src) {
    internals.sources[section] = src;

    // If a type has already registered add this source if we support it
    for (let type in internals.types) {
      let typeInfo = internals.types[type];
      if (src[typeInfo.funcName]) {
        typeInfo.sources.push(src);
        src.srcInProgress[type] = [];
      }
    }
  },
  funcName: funcName,
  app: app
};
//////////////////////////////////////////////////////////////////////////////////
function loadSources() {
  glob(getConfig("wiseService", "sourcePath", "./") + "source.*.js", (err, files) => {
    files.forEach((file) => {
      var src = require(file);
      src.initSource(internals.sourceApi);
    });
  });
}
//////////////////////////////////////////////////////////////////////////////////
//// APIs
//////////////////////////////////////////////////////////////////////////////////
app.get("/_ns_/nstest.html", function(req, res) {
  res.end();
});
//////////////////////////////////////////////////////////////////////////////////
app.get("/fields", function(req, res) {
  if (req.query.ver === undefined || req.query.ver === "0") {
    if (internals.fields.length < 256) {
      res.send(internals.fieldsBuf0);
    } else {
      console.log("ERROR - This wise server has more then 255 fields, it can't be used with older moloch");
      return res.status(404).end();
    }
  } else {
    res.send(internals.fieldsBuf1);
  }
});
//////////////////////////////////////////////////////////////////////////////////
app.get("/views", function(req, res) {
  res.send(internals.views);
});
//////////////////////////////////////////////////////////////////////////////////
app.get("/rightClicks", function(req, res) {
  res.send(internals.rightClicks);
});
//////////////////////////////////////////////////////////////////////////////////
internals.type2Name = ["ip", "domain", "md5", "email", "url", "tuple", "ja3", "sha256"];

//////////////////////////////////////////////////////////////////////////////////
function globalAllowed (value) {
  for(var i = 0; i < this.excludes.length; i++) {
    if (value.match(this.excludes[i])) {
      if (internals.debug > 0) {
        console.log(`Found in Global ${this.name} Exclude`, value);
      }
      return false;
    }
  }
  return true;
}
//////////////////////////////////////////////////////////////////////////////////
function globalIPAllowed (value) {
  if (this.excludes.find(value)) {
    if (internals.debug > 0) {
      console.log("Found in Global IP Exclude", value);
    }
    return false;
  }
  return true;
}
//////////////////////////////////////////////////////////////////////////////////
function sourceAllowed (src, value) {
  var excludes = src[this.excludeName] || [];
  for(var i = 0; i < excludes.length; i++) {
    if (value.match(excludes[i])) {
      if (internals.debug > 0) {
        console.log("Found in", src.section, this.name, "exclude", value);
      }
      return false;
    }
  }
  return true;
}
//////////////////////////////////////////////////////////////////////////////////
function sourceIPAllowed (src, value) {
  if (src.excludeIPs.find(value)) {
    if (internals.debug > 0) {
      console.log("Found in", src.section, "IP Exclude", value);
    }
    return false;
  }
  if (src.onlyIPs && !src.onlyIPs.find(value)) {
    return false;
  }
  return true;
}
//////////////////////////////////////////////////////////////////////////////////
function funcName(typeName) {
  if (typeName === 'url') {
    return 'getURL';
  }

  return 'get' + typeName[0].toUpperCase() + typeName.slice(1);
}
//////////////////////////////////////////////////////////////////////////////////
function processQuery(req, query, cb) {

  var typeInfo = internals.types[query.typeName];

  // First time we've seen this typeName
  if (!typeInfo) {
    typeInfo = internals.types[query.typeName] = {
      name: query.typeName,
      excludeName: 'exclude' + query.typeName[0].toUpperCase() + query.typeName.slice(1) + 's',
      funcName: funcName(query.typeName),
      sources: [],
      requestStats: 0,
      foundStats: 0,
      cacheHitStats: 0,
      cacheSrcHitStats: 0,
      cacheSrcRefreshStats: 0,
      excludes: [],
      globalAllowed: globalAllowed,
      sourceAllowed: sourceAllowed
    };

    if (query.typeName === 'url') {
      typeInfo.excludeName = 'excludeURLs';
    }

    if (query.typeName === 'ip') {
      typeInfo.excludeName = 'excludeIPs';
      typeInfo.globalAllowed = globalIPAllowed;
      typeInfo.sourceAllowed = sourceIPAllowed;
    }

    for (var src in internals.sources) {
      if (internals.sources[src][typeInfo.funcName]) {
        typeInfo.sources.push(internals.sources[src]);
        internals.sources[src].srcInProgress[query.typeName] = [];
      }
    }

    var items = getConfig("wiseService", typeInfo.excludeName, "");
    if (query.typeName === 'ip') {
      typeInfo.excludes = new iptrie.IPTrie();
      items.split(";").map(item => item.trim()).filter(item=>item !== "").forEach((item) => {
        let parts = item.split("/");
        try {
          typeInfo.excludes.add(parts[0], +parts[1] || (parts[0].includes(':')?128:32), true);
        } catch (e) {
          console.log(`Error for '${item}'`, e);
          process.exit();
        }
      });
    } else {
      typeInfo.excludes = items.split(";").map(item=>item.trim()).filter(item=>item !== "").map(item => RegExp.fromWildExp(item, "ailop"));
    }
  }

  typeInfo.requestStats++;

  // md5/sha256 have content type
  if (query.typeName === "md5" || query.typeName === "sha256") {
    var parts = query.value.split(";");
    query.value = parts[0];
    query.contentType = parts[1];
  }

  // Check if globally allowed
  try {
    if (!typeInfo.globalAllowed(query.value)) {
      return cb(null, wiseSource.emptyCombinedResult);
    }
  } catch (e) {
    console.log("ERROR", query.typeName, query.value, e);
  }

  // Fetch the cache for this query
  internals.cache.get(query, (err, cacheResult) => {
    if (req.timedout) {
      return cb("Timed out " + query.typeName + " " + query.value);
    }

    var now = Math.floor(Date.now()/1000);

    var cacheChanged = false;
    if (cacheResult === undefined) {
      cacheResult = {};
    } else {
      typeInfo.cacheHitStats++;
    }

    async.map(query.sources || typeInfo.sources, (src, cb) => {
      if (!typeInfo.sourceAllowed(src, query.value)) {
        // This source isn't allowed for query
        return setImmediate(cb, undefined);
      }

      if (cacheResult[src.section] === undefined || cacheResult[src.section].ts + src.cacheTimeout < now) {
        if (cacheResult[src.section] === undefined) {
          src.cacheMissStat++;
        } else {
          src.cacheRefreshStat++;
          typeInfo.cacheSrcRefreshStats++;
        }

        // Can't use the cache or there is no cache for this source
        delete cacheResult[src.section];

        // If already in progress then add to the list and return, cb called later;
        if (query.value in src.srcInProgress[query.typeName]) {
          src.srcInProgress[query.typeName][query.value].push(cb);
          return;
        }

        // First query for this value
        src.srcInProgress[query.typeName][query.value] = [cb];
        let startTime = Date.now();
        src[typeInfo.funcName](src.fullQuery===true?query:query.value, (err, result) => {
          src.average100MS = (99.0 * src.average100MS + (Date.now() - startTime))/100.0;

          if (!err && src.cacheTimeout !== -1 && result !== undefined) { // If err or cacheTimeout is -1 then don't cache
            cacheResult[src.section] = {ts:now, result:result};
            cacheChanged = true;
          }
          if (err === "dropped") {
            src.cacheDroppedStat++;
            err = null;
            result = undefined;
          }
          var srcInProgress = src.srcInProgress[query.typeName][query.value];
          delete src.srcInProgress[query.typeName][query.value];
          for (var i = 0, l = srcInProgress.length; i < l; i++) {
            srcInProgress[i](err, result);
          }
          return;
        });
      } else {
        src.cacheHitStat++;
        typeInfo.cacheSrcHitStats++;
        // Woot, we can use the cache
        setImmediate(cb, null, cacheResult[src.section].result);
      }
    }, (err, results) => {
      // Combine all the results together
      if (err) {
        return cb(err);
      }
      if (internals.debug > 2) {
        console.log("RESULT", typeInfo.funcName, query.value, wiseSource.result2Str(wiseSource.combineResults(results)));
      }

      if (req.timedout) {
        cb("Timed out " + query.typeName + " " + query.value);
      } else {
        cb(null, wiseSource.combineResults(results));
      }

      // Need to update the cache
      if (cacheChanged) {
        internals.cache.set(query, cacheResult);
      }
    });
  });
}
//////////////////////////////////////////////////////////////////////////////////
function processQueryResponse0(req, res, queries, results) {
  var buf = Buffer.allocUnsafe(8);
  buf.writeUInt32BE(internals.fieldsTS, 0);
  buf.writeUInt32BE(0, 4);
  res.write(buf);
  for (var r = 0; r < results.length; r++) {
    if (results[r][0] > 0) {
      internals.types[queries[r].typeName].foundStats++;
    }
    res.write(results[r]);
  }
  res.end();
}
//////////////////////////////////////////////////////////////////////////////////
//
function processQueryResponse2(req, res, queries, results) {
  var hashes = (req.query.hashes || "").split(",");

  const sendFields = !hashes.includes(internals.fieldsMd5);

  var buf = Buffer.allocUnsafe(42);
  buf.writeUInt32BE(0, 0);
  buf.writeUInt32BE(2, 4);
  buf.write(internals.fieldsMd5, 8);

  if (sendFields) {
    // Send all the fields
    res.write(buf.slice(0, 40));
    res.write(internals.fieldsBuf1.slice(8));
  } else {
    // Don't send the fields
    buf.writeUInt16BE(0, 40);
    res.write(buf);
  }

  // Send the results
  for (let r = 0; r < results.length; r++) {
    if (results[r][0] > 0) {
      internals.types[queries[r].typeName].foundStats++;
    }
    res.write(results[r]);
  }
  res.end();
}
//////////////////////////////////////////////////////////////////////////////////
app.post("/get", function(req, res) {
  var offset = 0;

  var buffers = [];
  req.on('data', (chunk) => {
    buffers.push(chunk);
  }).once('end', (err) => {
    var queries = [];
    for (var buf = Buffer.concat(buffers); offset < buf.length; ) {
      var type = buf[offset];
      offset++;

      var typeName;
      if (type & 0x80) {
        typeName = buf.toString('utf8', offset, offset + (type & ~0x80));
        offset += (type & ~0x80);
      } else {
        typeName = internals.type2Name[type];
      }

      var len  = buf.readUInt16BE(offset);
      offset += 2;

      var value = buf.toString('utf8', offset, offset+len);
      if (internals.debug > 1) {
        console.log(typeName, value);
      }
      offset += len;
      queries.push({typeName: typeName, value: value});
    }

    async.map(queries, (query, cb) => {
      processQuery(req, query, cb);
    }, (err, results) => {
      if (err || req.timedout) {
        console.log("Error", err || "Timed out" );
        return;
      }

      if (req.query.ver === "2") {
        processQueryResponse2(req, res, queries, results);
      } else {
        processQueryResponse0(req, res, queries, results);
      }
    });
  });
});
//////////////////////////////////////////////////////////////////////////////////
app.get("/:source/:typeName/:value", function(req, res) {
  var source = internals.sources[req.params.source];
  if (!source) {
    return res.end("Unknown source " + req.params.source);
  }

  var query = {typeName: req.params.typeName,
               value: req.params.value,
               sources: [source]};

  processQuery(req, query, (err, result) => {
    if (err || !result) {
      return res.end("Not found");
    }
    res.end(wiseSource.result2Str(result));
  });
});
//////////////////////////////////////////////////////////////////////////////////
app.get("/dump/:source", function(req, res) {
  var source = internals.sources[req.params.source];
  if (!source) {
    return res.end("Unknown source " + req.params.source);
  }

  if (!source.dump) {
    return res.end("The source doesn't support dump");
  }

  source.dump(res);
});
//////////////////////////////////////////////////////////////////////////////////
//ALW - Need to rewrite to use performQuery
/*
app.get("/bro/:type", function(req, res) {
  var hashes = req.query.items.split(",");
  var needsep = false;

  var fn = internals.type2Func[req.params.type];
  var srcs = internals[fn + "s"];
  async.map(hashes, (hash, doneCb) => {
    async.map(srcs, (src, cb) => {
      if (internals.source_allowed[req.params.type](src, hash)) {
        src[fn](hash, cb);
      } else {
        setImmediate(cb, undefined);
      }
    }, (err, results) => {
      doneCb(null, results);
    });
  },
  (err, results) => {

    for (var hashi = 0; hashi < hashes.length; hashi++) {
      if (hashi !== 0) {
        res.write("\tBRONEXT\t");
      }
      res.write(hashes[hashi]);
      res.write("\tBROIS\t");
      var resulti, found = false;
      for (resulti = 0; resulti < results[hashi].length; resulti++) {
        if (!results[hashi][resulti]) {
          continue;
        }
        if (found) {
          res.write("\tBROMORE\t");
        }
        found = true;
        res.write(srcs[resulti].section);
        res.write("\tBROSUB\t");
        var offset = 0;
        var buffer = results[hashi][resulti].buffer;
        for (var n = 0; n < results[hashi][resulti].num; n++) {
          if (n !== 0) {
            res.write(" ");
          }
          var pos = buffer[offset++];
          var len = buffer[offset++];
          var value = buffer.toString('utf8', offset, offset+len-1);
          offset += len;
          res.write(wiseSource.pos2Field[pos] + ": " + value);
        }
      }
      if (!found) {
        res.write("BRONONE");
      }
    }
    res.end();
  });
});
*/
//////////////////////////////////////////////////////////////////////////////////
app.get("/:typeName/:value", function(req, res) {
  var query = {typeName: req.params.typeName,
               value: req.params.value};

  processQuery(req, query, (err, result) => {
    if (err || !result) {
      return res.end("Not found");
    }
    res.end(wiseSource.result2Str(result));
  });
});
//////////////////////////////////////////////////////////////////////////////////
if (getConfig("wiseService", "regressionTests")) {
  app.post('/shutdown', (req, res) => {
    process.exit(0);
    throw new Error("Exiting");
  });
}
//////////////////////////////////////////////////////////////////////////////////
function printStats()
{
  var keys = Object.keys(internals.types).sort();
  var lines = [];
  lines[0] = "                   ";
  lines[1] = "REQUESTS:          ";
  lines[2] = "FOUND:             ";
  lines[3] = "CACHE HIT:         ";
  lines[4] = "CACHE SRC HIT:     ";
  lines[5] = "CACHE SRC REFRESH: ";

  for (var key of keys) {
    let typeInfo = internals.types[key];
    lines[0] += sprintf(" %11s", key);
    lines[1] += sprintf(" %11d", typeInfo.requestStats);
    lines[2] += sprintf(" %11d", typeInfo.foundStats);
    lines[3] += sprintf(" %11d", typeInfo.cacheHitStats);
    lines[4] += sprintf(" %11d", typeInfo.cacheSrcHitStats);
    lines[5] += sprintf(" %11d", typeInfo.cacheSrcRefreshStats);
  }

  for (var i = 0; i < lines.length; i++) {
    console.log(lines[i]);
  }

  for (var section in internals.sources) {
    let src = internals.sources[section];
    console.log(sprintf("SRC %-30s    cached: %7d lookup: %9d refresh: %7d dropped: %7d avgMS: %7d",
      section, src.cacheHitStat, src.cacheMissStat, src.cacheRefreshStat, src.cacheDroppedStat, src.average100MS));
  }
}
//////////////////////////////////////////////////////////////////////////////////
//// jPaq
//////////////////////////////////////////////////////////////////////////////////
/*
 jPaq - A fully customizable JavaScript/JScript library
 http://jpaq.org/

 Copyright (c) 2011 Christopher West
 Licensed under the MIT license.
 http://jpaq.org/license/

 Version: 1.0.6.000m
 Revised: April 6, 2011
*/
/* jshint ignore:start */
RegExp.fromWildExp=function(c,a){for(var d=a&&a.indexOf("o")>-1,f,b,e="",g=a&&a.indexOf("l")>-1?"":"?",h=RegExp("~.|\\[!|"+(d?"{\\d+,?\\d*\\}|[":"[")+(a&&a.indexOf("p")>-1?"":"\\(\\)")+"\\{\\}\\\\\\.\\*\\+\\?\\:\\|\\^\\$%_#<>]");(f=c.search(h))>-1&&f<c.length;)e+=c.substring(0,f),e+=(b=c.match(h)[0])=="[!"?"[^":b.charAt(0)=="~"?"\\"+b.charAt(1):b=="*"||b=="%"?".*"+g:
b=="?"||b=="_"?".":b=="#"?"\\d":d&&b.charAt(0)=="{"?b+g:b=="<"?"\\b(?=\\w)":b==">"?"(?:\\b$|(?=\\W)\\b)":"\\"+b,c=c.substring(f+b.length);e+=c;a&&(/[ab]/.test(a)&&(e="^"+e),/[ae]/.test(a)&&(e+="$"));return RegExp(e,a?a.replace(/[^gim]/g,""):"")};
/* jshint ignore:end */
//////////////////////////////////////////////////////////////////////////////////
//// Main
//////////////////////////////////////////////////////////////////////////////////
function main() {
  internals.cache = wiseCache.createCache({getConfig: getConfig});

  addField("field:tags"); // Always add tags field so we have at least 1 field

  loadSources();
  setInterval(printStats, 60*1000);

  var server;
  if (getConfig("wiseService", "keyFile") && getConfig("wiseService", "certFile")) {
    var keyFileData = fs.readFileSync(getConfig("wiseService", "keyFile"));
    var certFileData = fs.readFileSync(getConfig("wiseService", "certFile"));

    server = https.createServer({key: keyFileData, cert: certFileData, secureOptions: require('constants').SSL_OP_NO_TLSv1}, app);
  } else {
    server = http.createServer(app);
  }

  server
    .on('error', (e) => {
      console.log("ERROR - couldn't listen on port", getConfig("wiseService", "port", 8081), "is wiseService already running?");
      process.exit(1);
    })
    .on('listening', (e) => {
      console.log("Express server listening on port %d in %s mode", server.address().port, app.settings.env);
    })
    .listen(getConfig("wiseService", "port", 8081));
}

if (internals.workers <= 1 || cluster.isWorker) {
  main();
}
