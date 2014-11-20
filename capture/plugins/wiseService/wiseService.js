/******************************************************************************/
/* wiseService.js -- Server requests between moloch and various intel services
 *                   and files
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
/*jshint
  node: true, plusplus: false, curly: true, eqeqeq: true, immed: true, latedef: true, newcap: true, nonew: true, undef: true, strict: true, trailing: true
*/
'use strict';

var ini            = require('iniparser')
  , express        = require('express')
  , http           = require('http')
  , glob           = require('glob')
  , async          = require('async')
  , sprintf        = require('./sprintf.js').sprintf
  , csv            = require('csv')
  ;

require('console-stamp')(console, '[HH:MM:ss.l]');

var internals = {
  configFile: "/data/moloch/etc/wiseService.ini",
  debug: 0,
  fieldsTS: 0,
  fields: [],
  fieldsSize: 0,
  field2Pos: {},
  pos2Field: {},
  getIps: [],
  getDomains: [],
  getMd5s: [],
  getEmails: [],
  printStats: [],
  sources: [],
  rstats: [0,0,0,0],
  fstats: [0,0,0,0]
};

//////////////////////////////////////////////////////////////////////////////////
//// Command Line Parsing
//////////////////////////////////////////////////////////////////////////////////
function processArgs() {
  var args = [];
  for (var i = 0, ilen = process.argv.length; i < ilen; i++) {
    if (process.argv[i] === "-c") {
      i++;
      internals.configFile = process.argv[i];
    } else if (process.argv[i] === "--debug") {
      internals.debug++;
    } else {
      args.push(process.argv[i]);
    }
  }
  process.argv = args;
}
processArgs();
//////////////////////////////////////////////////////////////////////////////////
//// Config
//////////////////////////////////////////////////////////////////////////////////
internals.config = ini.parseSync(internals.configFile);
var app = express();
app.configure(function() {
  app.use(express.logger({ format: ':date \x1b[1m:method\x1b[0m \x1b[33m:url\x1b[0m :res[content-length] bytes :response-time ms'}));
  app.use(express.timeout(5*1000));
});
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

  if (internals.field2Pos[name] !== undefined) {
    return internals.field2Pos[name];
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

  internals.pos2Field[pos] = name;
  internals.field2Pos[name] = pos;
  return pos;
}
//////////////////////////////////////////////////////////////////////////////////
function doEncode()
{
  var a, len = 0;
  for (a = 1; a < arguments.length; a += 2) {
    len += 3 + arguments[a].length;
  }
  var buf = new Buffer(len);
  var offset = 0;
  for (a = 1; a < arguments.length; a += 2) {
      buf.writeUInt8(arguments[a-1], offset);
      buf.writeUInt8(arguments[a].length+1, offset+1);
      var l = buf.write(arguments[a], offset+2);
      buf.writeUInt8(0, offset+l+2);
      offset += 3 + l;
  }
  return buf;
}
//////////////////////////////////////////////////////////////////////////////////
function combineResults(results)
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
}
//////////////////////////////////////////////////////////////////////////////////
function result2Str(result, indent) {
  if (!indent) {
    indent = "";
  }
  
  var str = "[";
  var offset = 1;
  for (var i = 0; i < result[0]; i++) {
    var pos   = result[offset];
    var len   = result[offset+1]
    var value = result.toString('utf8', offset+2, offset+2+len-1);
    offset += 2 + len;
    if (i > 0) {
      str += ",\n";
    }
    str += indent + "{field: \"" + internals.pos2Field[pos] + "\", len: " + len + ", value: \"" + value + "\"}";
  }

  return str + "]\n";
}
//////////////////////////////////////////////////////////////////////////////////
internals.emptyResult = {num: 0, buffer: new Buffer(0)};
function parseCSV (body, options, cb) {
  var parser = csv.parse(body, {skip_empty_lines: true, comment: '#'}, function(err, data) {
    if (err) {
      return cb(err);
    }

    var cache = {};
    for (var i = 0; i < data.length; i++) {
      cache[data[i][options.column]] = internals.emptyResult;
    }
    cb(err, cache);
  });
}
//////////////////////////////////////////////////////////////////////////////////
function parseTagger (body, options, cb) {
  var lines = body.toString().split("\n");
  var cache = {};
  var shortcuts = {};
  for (var l = 0, llen = lines.length; l < llen; l++) {
    if (lines[l][0] === "#") {
      if (lines[l].lastIndexOf('#field:',0) === 0) {
        var pos = addField(lines[l].substring(1));
        var match = lines[l].match(/shortcut:([^;]+)/);
        if (match) {
          shortcuts[match[1]] = pos;
        }
      }
      continue;
    }

    if (lines[l].match(/^\s*$/)) {
      continue;
    }


    var args = [];
    var parts = lines[l].split(";");
    for (var p = 1; p < parts.length; p++) {
      var kv = parts[p].split('=');
      if (shortcuts[kv[0]] !== undefined) {
        args.push(shortcuts[kv[0]]);
      } else if (internals.field2Pos[kv[0]]) {
        args.push(internals.field2Pos[kv[0]]);
      } else {
        args.push(addField("field:" + kv[0]));
      }
      args.push(kv[1]);
    }
    cache[parts[0]] = {num: args.length/2, buffer: doEncode.apply(null, args)}
  }
  cb(null, cache);
}
//////////////////////////////////////////////////////////////////////////////////
internals.sourceApi = {
  getConfig: getConfig,
  getConfigSections: getConfigSections,
  addField: addField,
  encode: doEncode,
  combineResults: combineResults,
  result2Str: result2Str,
  parseCSV: parseCSV,
  parseTagger: parseTagger,
  debug: internals.debug,
  addSource: function(name, src) {
    internals.sources[name] = src;
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
    if (src.printStats) {
      internals.printStats.push(src);
    }
  }
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
internals.funcNames = ["getIp", "getDomain", "getMd5", "getEmail"];
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
      async.map(internals[internals.funcNames[query.type] + "s"], function(func, cb) {
        func[internals.funcNames[query.type]](query.value, cb);
      }, function (err, results) {
        cb(null, combineResults(results));
      });
    }, function (err, results) {
      var buf = new Buffer(8);
      buf.writeUInt32BE(internals.fieldsTS, 0);
      buf.writeUInt32BE(0, 4);
      res.write(buf);
      for (var r = 0; r < results.length; r++) {
        if (results[r][0] > 0)
          internals.fstats[queries[r].type]++;
        res.write(results[r]);
      }
      res.end();
    });
  });
});
//////////////////////////////////////////////////////////////////////////////////
internals.name2func = {ip:"getIp", 0:"getIp", domain:"getDomain", 1:"getDomain", md5:"getMd5", 2:"getMd5", email:"getEmail", 3:"getEmail"};
app.get("/:source/:type/:value", function(req, res) {
  var source = internals.sources[req.params.source];
  if (!source) {
    return res.end("Unknown source " + req.params.source);
  }

  var fn = internals.name2func[req.params.type];
  if (!fn) {
    return res.end("Unknown type " + req.params.type);
  }

  if (!source[fn]) {
    return res.end("The source doesn't support the query" + fn);
  }

  source[fn](req.params.value, function (err, result) {
    if (!result) {
      return res.end("Not found");
    }
    res.end(result2Str(combineResults([result])));
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
app.get("/:type/:value", function(req, res) {
  var fn = internals.name2func[req.params.type];
  if (!fn) {
    return res.end("Unknown type " + req.params.type);
  }
  async.map(internals[fn + "s"], function(func, cb) {
    func[fn](req.params.value, cb);
  }, function (err, results) {
    var result = combineResults(results);
    res.end(result2Str(result));
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
//// Main
//////////////////////////////////////////////////////////////////////////////////
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
