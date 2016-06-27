/******************************************************************************/
/* wiseService.js -- Server requests between moloch and various intel services
 *                   and files
 *
 * Copyright 2012-2015 AOL Inc. All rights reserved.
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
  , cluster        = require("cluster")
  ;

require('console-stamp')(console, '[HH:MM:ss.l]');

var internals = {
  configFile: "/data/moloch/etc/wiseService.ini",
  debug: 0,
  fieldsTS: 0,
  fields: [],
  fieldsSize: 0,
  getIps: [],
  getDomains: [],
  getMd5s: [],
  getEmails: [],
  getURLs: [],
  printStats: [],
  sources: [],
  rstats: [0,0,0,0],
  fstats: [0,0,0,0],
  global_allowed: {},
  source_allowed: {},
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
    internals.sources[section] = src;
    if (src.getIp) {
      internals.getIps.push(src);
    }
    if (src.getDomain) {
      internals.getDomains.push(src);
    }
    if (src.getMd5) {
      internals.getMd5s.push(src);
    }
    if (src.getEmail) {
      internals.getEmails.push(src);
    }
    if (src.getURL) {
      internals.getURLs.push(src);
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
internals.funcNames = ["getIp", "getDomain", "getMd5", "getEmail", "getURL"];
internals.type2Name = ["ip", "domain", "md5", "email", "url"];
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
        console.log(internals.funcNames[type], value);
      }
      offset += 3 + len;
      queries.push({type: type, value: value});
      internals.rstats[type]++;
    }

    async.map(queries, function (query, cb) {
      var name = internals.type2Name[query.type];
      try {
        if (!internals.global_allowed[name](query.value)) {
          return cb(null, wiseSource.combineResults([]));
        }
      } catch (e) {
        console.log("ERROR", name, query, e);
      }
      async.map(internals[internals.funcNames[query.type] + "s"], function(src, cb) {
        if (req.timedout) {
          return cb("Timed out " + internals.type2Name[query.type] + " " + query.value + " " + src.section);
        }
        if (internals.source_allowed[name](src, query.value)) {
          src[internals.funcNames[query.type]](query.value, cb);
        } else {
          setImmediate(cb, undefined);
        }
      }, function (err, results) {
        if (err || req.timedout) {
          return cb(err || "Timed out " + internals.type2Name[query.type] + " " + query.value);
        }
        if (internals.debug > 2) {
          console.log("RESULT", internals.funcNames[query.type], query.value, wiseSource.result2Str(wiseSource.combineResults(results)));
        }
        cb(null, wiseSource.combineResults(results));
      });
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
internals.type2func = {ip:"getIp", 0:"getIp", domain:"getDomain", 1:"getDomain", md5:"getMd5", 2:"getMd5", email:"getEmail", 3:"getEmail", url:"getURL", 4:"getURL"};
app.get("/:source/:type/:value", function(req, res) {
  var source = internals.sources[req.params.source];
  if (!source) {
    return res.end("Unknown source " + req.params.source);
  }

  var fn = internals.type2func[req.params.type];
  if (!fn) {
    return res.end("Unknown type " + req.params.type);
  }

  if (!source[fn]) {
    return res.end("The source doesn't support the query " + fn);
  }

  source[fn](req.params.value, function (err, result) {
    if (!result) {
      return res.end("Not found");
    }
    res.end(wiseSource.result2Str(wiseSource.combineResults([result])));
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
app.get("/bro/:type", function(req, res) {
  var hashes = req.query.items.split(",");
  var needsep = false;

  var fn = internals.type2func[req.params.type];
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
      if (hashi != 0) {
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
          if (n != 0) {
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
//////////////////////////////////////////////////////////////////////////////////
app.get("/:type/:value", function(req, res) {
  var fn = internals.type2func[req.params.type];
  if (!fn) {
    return res.end("Unknown type " + req.params.type);
  }

  if (!internals.global_allowed[req.params.type](req.params.value)) {
    var result = wiseSource.combineResults([]);
    return res.end(wiseSource.result2Str(result));
  }

  async.map(internals[fn + "s"], function(src, cb) {
    if (internals.source_allowed[req.params.type](src, req.params.value)) {
      src[fn](req.params.value, cb);
    } else {
      setImmediate(cb, undefined);
    }
  }, function (err, results) {
    var result = wiseSource.combineResults(results);
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
internals.global_allowed.ip = function(value) {
  if (internals.excludeIPs.find(value)) {
    if (internals.debug > 0) {
      console.log("Found in Global IP Exclude", value);
    }
    return false;
  }
  return true;
};
internals.global_allowed.md5 = function(value) {return true;};
internals.global_allowed.email = function(value) {
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
internals.global_allowed.domain = function(value) {
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
internals.source_allowed.ip = function(src, value) {
  if (src.excludeIPs.find(value)) {
    if (internals.debug > 0) {
      console.log("Found in", src.section, "IP Exclude", value);
    }
    return false;
  }
  return true;
};
internals.source_allowed.md5 = function(src, value) {return true;};
internals.source_allowed.email = function(src, value) {
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
internals.source_allowed.domain = function(src, value) {
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
