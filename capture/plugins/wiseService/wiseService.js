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

var ini            = require('iniparser')
  , express        = require('express')
  , fs             = require('fs')
  , http           = require('http')
  , glob           = require('glob')
  , async          = require('async')
  , sprintf        = require('./sprintf.js').sprintf
  , csv            = require('csv')
  , request        = require('request')
  , iptrie         = require('iptrie')
  , wiseSource     = require('./wiseSource.js')
  , wiseCache      = require('./wiseCache.js')
  , cluster        = require("cluster")
  ;

require('console-stamp')(console, '[HH:MM:ss.l]');

var internals = {
  configFile: "/data/moloch/etc/wiseService.ini",
  debug: 0,
  fieldsTS: 0,
  fields: [],
  fieldsSize: 0,
  ip: {
    sources: []
  },
  domain: {
    sources: []
  },
  md5: {
    sources: []
  },
  email: {
    sources: []
  },
  url: {
    sources: []
  },
  printStats: [],
  sources: [],
  rstats: [0,0,0,0,0],
  fstats: [0,0,0,0,0],
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
    } else if (argv[i] === "--debug") {
      internals.debug++;
    } else if (argv[i] === "--workers") {
      i++;
      internals.workers = +argv[i];
    }
  }
}
processArgs(process.argv);

if (internals.workers > 1) {
  if (cluster.isMaster) {
    for (var i = 0; i < internals.workers; i++) {
      cluster.fork();
    }
    cluster.on('exit', function(worker, code, signal) {
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

  if (wiseSource.field2Pos[name] !== undefined) {
    return wiseSource.field2Pos[name];
  }

  var pos = internals.fields.length;
  newFieldsTS();
  internals.fields.push(field);
  internals.fieldsSize += field.length + 10;

  internals.fieldsBuf = new Buffer(internals.fieldsSize + 9);
  internals.fieldsBuf.writeUInt32BE(internals.fieldsTS, 0);
  internals.fieldsBuf.writeUInt32BE(0, 4);
  internals.fieldsBuf.writeUInt8(internals.fields.length, 8);
  var offset = 9;
  for (var i = 0; i < internals.fields.length; i++) {
    var len = internals.fieldsBuf.write(internals.fields[i], offset+2);
    internals.fieldsBuf.writeUInt16BE(len+1, offset);
    internals.fieldsBuf.writeUInt8(0, offset+2+len);
    offset += 3 + len;
  }
  internals.fieldsBuf.length = offset;

  wiseSource.pos2Field[pos] = name;
  wiseSource.field2Pos[name] = pos;
  return pos;
}
//////////////////////////////////////////////////////////////////////////////////
internals.sourceApi = {
  getConfig: getConfig,
  getConfigSections: getConfigSections,
  addField: addField,
  addView: function (name, view) {
    internals.views[name] = view;
  },
  addRightClick: function (name, rightClick) {
    internals.rightClicks[name] = rightClick;
  },
  debug: internals.debug,
  addSource: function(section, src) {
    src.inProgress = {ip: {}, domain: {}, email: {}, md5: {}, url: {}};
    internals.sources[section] = src;
    if (src.getIp) {
      internals.ip.sources.push(src);
    }
    if (src.getDomain) {
      internals.domain.sources.push(src);
    }
    if (src.getMd5) {
      internals.md5.sources.push(src);
    }
    if (src.getEmail) {
      internals.email.sources.push(src);
    }
    if (src.getURL) {
      internals.url.sources.push(src);
    }
    if (src.printStats) {
      internals.printStats.push(src);
    }
  },
  app: app
};
//////////////////////////////////////////////////////////////////////////////////
function loadSources() {
  glob(getConfig("wiseService", "sourcePath", "./") + "source.*.js", function (err, files) {
    files.forEach(function(file) {
      var src = require(file);
      src.initSource(internals.sourceApi);
    });
  });
}
//////////////////////////////////////////////////////////////////////////////////
//// APIs
//////////////////////////////////////////////////////////////////////////////////
app.get("/fields", function(req, res) {
  res.send(internals.fieldsBuf);
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
internals.type2Func = ["getIp", "getDomain", "getMd5", "getEmail", "getURL"];
internals.type2Name = ["ip", "domain", "md5", "email", "url"];
internals.name2Type = {ip:0, 0:0, domain:1, 1:1, md5:2, 2:2, email:3, 3:3, url:4, 4:4};

//////////////////////////////////////////////////////////////////////////////////
function processQuery(req, query, cb) {
  var typeName = internals.type2Name[query.type];
  var funcName = internals.type2Func[query.type];
  var typeInfo = internals[typeName];

  try {
    if (!typeInfo.global_allowed(query.value)) {
      return cb(null, wiseSource.emptyCombinedResult);
    }
  } catch (e) {
    console.log("ERROR", typeName, query, e);
  }

  // Fetch the cache for this query
  internals.cache.get(query, function(err, cacheResult) {
    if (req.timedout) {
      return cb("Timed out " + typeName + " " + query.value);
    }

    var now = Math.floor(Date.now()/1000);

    var cacheChanged = false;
    if (cacheResult === undefined) {
      cacheResult = {};
    }

    async.map(query.sources || typeInfo.sources, function(src, cb) {
      if (req.timedout) {
        return cb("Timed out " + typeName + " " + query.value + " " + src.section);
      }

      if (!typeInfo.source_allowed(src, query.value)) {
        // This source isn't allowed for query
        return setImmediate(cb, undefined);
      }

      if (cacheResult[src.section] === undefined || cacheResult[src.section].ts + src.cacheTimeout < now) {
        // Can't use the cache or there is no cache for this source
        delete cacheResult[src.section];

        // If already in progress then add to the list and return, cb called later;
        if (query.value in src.inProgress[typeName]) {
          src.inProgress[typeName][query.value].push(cb);
          return;
        }

        // First query for this value
        src.inProgress[typeName][query.value] = [cb];
        src[funcName](query.value, function (err, result) {
          if (!err && src.cacheTimeout !== -1) { // If err or cacheTimeout is -1 then don't cache
            cacheResult[src.section] = {ts:now, result:result};
            cacheChanged = true;
          }
          var inProgress = src.inProgress[typeName][query.value];
          delete src.inProgress[typeName][query.value];
          for (var i = 0, l = inProgress.length; i < l; i++) {
            inProgress[i](err, result);
          }
          return;
        });
      } else {
        // Woot, we can use the cache
        setImmediate(cb, null, cacheResult[src.section].result);
      }
    }, function (err, results) {
      // Combine all the results together
      if (err || req.timedout) {
        return cb(err || "Timed out " + typeName + " " + query.value);
      }
      if (internals.debug > 2) {
        console.log("RESULT", funcName, query.value, wiseSource.result2Str(wiseSource.combineResults(results)));
      }
      cb(null, wiseSource.combineResults(results));

      // Need to update the cache
      if (cacheChanged) {
        internals.cache.set(query, cacheResult);
      }
    });
  });
}
//////////////////////////////////////////////////////////////////////////////////
app.post("/get", function(req, res) {
  var offset = 0;

  var buffers = [];
  req.on('data', function (chunk) {
    buffers.push(chunk);
  }).once('end', function (err) {
    var queries = [];
    for (var buf = Buffer.concat(buffers); offset < buf.length; ) {
      var type = buf[offset];
      var len  = buf.readUInt16BE(offset+1);
      var value = buf.toString('utf8', offset+3, offset+3+len);
      if (internals.debug > 1) {
        console.log(internals.type2Func[type], value);
      }
      offset += 3 + len;
      queries.push({type: type, value: value});
      internals.rstats[type]++;
    }

    async.map(queries, function (query, cb) {
      processQuery(req, query, cb);
    }, function (err, results) {
      if (err || req.timedout) {
        console.log("Error", err || "Timed out" );
        return;
      }
      var buf = new Buffer(8);
      buf.writeUInt32BE(internals.fieldsTS, 0);
      buf.writeUInt32BE(0, 4);
      res.write(buf);
      for (var r = 0; r < results.length; r++) {
        if (results[r][0] > 0) {
          internals.fstats[queries[r].type]++;
        }
        res.write(results[r]);
      }
      res.end();
    });
  });
});
//////////////////////////////////////////////////////////////////////////////////
app.get("/:source/:type/:value", function(req, res) {
  var source = internals.sources[req.params.source];
  if (!source) {
    return res.end("Unknown source " + req.params.source);
  }

  var query = {type: internals.name2Type[req.params.type],
               value: req.params.value,
               sources: [source]};

  if (query.type === undefined) {
    return res.end("Unknown type " + req.params.type);
  }

  processQuery(req, query, function (err, result) {
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
  async.map(hashes, function(hash, doneCb) {
    async.map(srcs, function(src, cb) {
      if (internals.source_allowed[req.params.type](src, hash)) {
        src[fn](hash, cb);
      } else {
        setImmediate(cb, undefined);
      }
    }, function (err, results) {
      doneCb(null, results);
    });
  },
  function (err, results) {

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
app.get("/:type/:value", function(req, res) {
  var type = internals.name2Type[req.params.type];
  if (type === undefined) {
    return res.end("Unknown type " + req.params.type);
  }

  var query = {type: type,
               value: req.params.value};

  processQuery(req, query, function (err, result) {
    if (err || !result) {
      return res.end("Not found");
    }
    res.end(wiseSource.result2Str(result));
  });
});
//////////////////////////////////////////////////////////////////////////////////
if (getConfig("wiseService", "regressionTests")) {
  app.post('/shutdown', function(req, res) {
    process.exit(0);
    throw new Error("Exiting");
  });
}
//////////////////////////////////////////////////////////////////////////////////
function printStats()
{
  console.log(sprintf("REQUESTS: domain: %7d ip: %7d email: %7d md5: %7d",
      internals.rstats[1], internals.rstats[0], internals.rstats[3], internals.rstats[2]));
  console.log(sprintf("FOUND:    domain: %7d ip: %7d email: %7d md5: %7d",
      internals.fstats[1], internals.fstats[0], internals.fstats[3], internals.fstats[2]));
  internals.printStats.forEach(function(fn) {
    fn.printStats();
  });
}
//////////////////////////////////////////////////////////////////////////////////
internals.ip.global_allowed = function(value) {
  if (internals.excludeIPs.find(value)) {
    if (internals.debug > 0) {
      console.log("Found in Global IP Exclude", value);
    }
    return false;
  }
  return true;
};
internals.md5.global_allowed = function(value) {return true;};
internals.email.global_allowed = function(value) {
  for(var i = 0; i < internals.excludeEmails.length; i++) {
    if (value.match(internals.excludeEmails[i])) {
      if (internals.debug > 0) {
        console.log("Found in Global Email Exclude", value);
      }
      return false;
    }
  }
  return true;
};
internals.domain.global_allowed = function(value) {
  for(var i = 0; i < internals.excludeDomains.length; i++) {
    if (value.match(internals.excludeDomains[i])) {
      if (internals.debug > 0) {
        console.log("Found in Global Domain Exclude", value);
      }
      return false;
    }
  }
  return true;
};
internals.ip.source_allowed = function(src, value) {
  if (src.excludeIPs.find(value)) {
    if (internals.debug > 0) {
      console.log("Found in", src.section, "IP Exclude", value);
    }
    return false;
  }
  return true;
};
internals.md5.source_allowed = function(src, value) {return true;};
internals.email.source_allowed = function(src, value) {
  for(var i = 0; i < src.excludeEmails.length; i++) {
    if (value.match(src.excludeEmails[i])) {
      if (internals.debug > 0) {
        console.log("Found in", src.section, "Email Exclude", value);
      }
      return false;
    }
  }
  return true;
};
internals.domain.source_allowed = function(src, value) {
  for(var i = 0; i < src.excludeDomains.length; i++) {
    if (value.match(src.excludeDomains[i])) {
      if (internals.debug > 0) {
        console.log("Found in", src.section, "Domain Exclude", value);
      }
      return false;
    }
  }
  return true;
};
//////////////////////////////////////////////////////////////////////////////////
function loadExcludes() {
  ["excludeDomains", "excludeEmails"].forEach(function(type) {
    var items = getConfig("wiseService", type);
    internals[type] = [];
    if (!items) {return;}
    items.split(";").forEach(function(item) {
      internals[type].push(RegExp.fromWildExp(item, "ailop"));
    });
  });

  internals.excludeIPs = new iptrie.IPTrie();
  var items = getConfig("wiseService", "excludeIPs", "");
  items.split(";").forEach(function(item) {
    if (item === "") {
      return;
    }
    var parts = item.split("/");
    internals.excludeIPs.add(parts[0], +parts[1] || 32, true);
  });
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
  loadExcludes();
  loadSources();
  setInterval(printStats, 60*1000);
  var server = http.createServer(app);
  server
    .on('error', function (e) {
      console.log("ERROR - couldn't listen on port", getConfig("wiseService", "port", 8081), "is wiseService already running?");
      process.exit(1);
    })
    .on('listening', function (e) {
      console.log("Express server listening on port %d in %s mode", server.address().port, app.settings.env);
    })
    .listen(getConfig("wiseService", "port", 8081));
}

if (internals.workers <= 1 || cluster.isWorker) {
  main();
}
