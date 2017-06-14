/******************************************************************************/
/* viewer.js  -- The main moloch app
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

var MIN_DB_VERSION = 34;

//// Modules
//////////////////////////////////////////////////////////////////////////////////
try {
var Config         = require('./config.js'),
    express        = require('express'),
    stylus         = require('stylus'),
    util           = require('util'),
    fs             = require('fs-ext'),
    async          = require('async'),
    url            = require('url'),
    dns            = require('dns'),
    Pcap           = require('./pcap.js'),
    sprintf        = require('./public/sprintf.js'),
    Db             = require('./db.js'),
    os             = require('os'),
    zlib           = require('zlib'),
    molochparser   = require('./molochparser.js'),
    passport       = require('passport'),
    DigestStrategy = require('passport-http').DigestStrategy,
    HTTPParser     = process.binding('http_parser').HTTPParser,
    molochversion  = require('./version'),
    http           = require('http'),
    pug            = require('pug'),
    jade           = require('jade'),
    https          = require('https'),
    EventEmitter   = require('events').EventEmitter,
    decode         = require('./decode.js');
} catch (e) {
  console.log ("ERROR - Couldn't load some dependancies, maybe need to 'npm update' inside viewer directory", e);
  process.exit(1);
  throw new Error("Exiting");
}

try {
  var Png = require('png').Png;
} catch (e) {console.log("WARNING - No png support, maybe need to 'npm update'", e);}

if (typeof express !== "function") {
  console.log("ERROR - Need to run 'npm update' in viewer directory");
  process.exit(1);
  throw new Error("Exiting");
}
var app = express();

//////////////////////////////////////////////////////////////////////////////////
//// Config
//////////////////////////////////////////////////////////////////////////////////
var internals = {
  elasticBase: Config.get("elasticsearch", "http://localhost:9200").split(","),
  userNameHeader: Config.get("userNameHeader"),
  httpAgent:   new http.Agent({keepAlive: true, keepAliveMsecs:5000, maxSockets: 40}),
  httpsAgent:  new https.Agent({keepAlive: true, keepAliveMsecs:5000, maxSockets: 40}),
  previousNodeStats: [],
  caTrustCerts: {},
  cronRunning: false,
  rightClicks: {},
  pluginEmitter: new EventEmitter(),
  writers: {},

  cronTimeout: Math.max(+Config.get("tcpTimeout", 8*60), +Config.get("udpTimeout", 60), +Config.get("icmpTimeout", 10)) +
               +Config.get("dbFlushTimeout", 5) + 60 + 5,

//http://garethrees.org/2007/11/14/pngcrush/
  emptyPNG: new Buffer("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAACklEQVR4nGMAAQAABQABDQottAAAAABJRU5ErkJggg==", 'base64'),
  PNG_LINE_WIDTH: 256,
};

if (internals.elasticBase[0].lastIndexOf('http', 0) !== 0) {
  internals.elasticBase[0] = "http://" + internals.elasticBase[0];
}

function userCleanup(suser) {
  suser.settings = suser.settings || {};
  if (suser.emailSearch === undefined) {suser.emailSearch = false;}
  if (suser.removeEnabled === undefined) {suser.removeEnabled = false;}
  if (Config.get("multiES", false)) {suser.createEnabled = false;}
}

passport.use(new DigestStrategy({qop: 'auth', realm: Config.get("httpRealm", "Moloch")},
  function(userid, done) {
    Db.getUserCache(userid, function(err, suser) {
      if (err && !suser) {return done(err);}
      if (!suser || !suser.found) {console.log("User", userid, "doesn't exist"); return done(null, false);}
      if (!suser._source.enabled) {console.log("User", userid, "not enabled"); return done("Not enabled");}

      userCleanup(suser._source);

      return done(null, suser._source, {ha1: Config.store2ha1(suser._source.passStore)});
    });
  },
  function (options, done) {
      //TODO:  Should check nonce here
      return done(null, true);
  }
));

// app.configure
var logger = require("morgan");
var favicon = require("serve-favicon");
var bodyParser = require('body-parser');
var multer = require('multer');
var methodOverride = require('method-override');
var compression = require('compression');

app.enable("jsonp callback");
app.set('views', __dirname + '/views');
app.set('view engine', 'pug');
app.locals.molochversion =  molochversion.version;
app.locals.isIndex = false;
app.locals.basePath = Config.basePath();
app.locals.elasticBase = internals.elasticBase[0];
app.locals.allowUploads = Config.get("uploadCommand") !== undefined;
app.locals.molochClusters = Config.configMap("moloch-clusters");

app.use(favicon(__dirname + '/public/favicon.ico'));
app.use(passport.initialize());
app.use(function(req, res, next) {
  if (res.setTimeout) {
    res.setTimeout(10 * 60 * 1000); // Increase default from 2 min to 10 min
  }
  req.url = req.url.replace(Config.basePath(), "/");
  return next();
});
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));
//app.use(multer({dest: Config.get("pcapDir")}));

// send req to access log file or stdout
var _stream = process.stdout;
var _accesslogfile = Config.get("accessLogFile");
if (_accesslogfile) {
  _stream = fs.createWriteStream(_accesslogfile, {flags: 'a'});
}


app.use(logger(':date :username \x1b[1m:method\x1b[0m \x1b[33m:url\x1b[0m :status :res[content-length] bytes :response-time ms',{stream: _stream}));
app.use(compression());
app.use(methodOverride());


app.use('/font-awesome', express.static(__dirname + '/node_modules/font-awesome', { maxAge: 600 * 1000}));
app.use('/bootstrap', express.static(__dirname + '/node_modules/bootstrap', { maxAge: 600 * 1000}));

app.use('/cyberchef.htm', function(req, res, next) {
  res.setHeader("Vary", "Accept-Encoding");
  res.setHeader("Content-Encoding", "gzip");
  res.sendFile(__dirname + "/public/cyberchef.htm.gz");
});


app.use("/", express.static(__dirname + '/public', { maxAge: 600 * 1000}));
if (Config.get("passwordSecret")) {
  app.locals.alwaysShowESStatus = false;
  app.use(function(req, res, next) {
    // 200 for NS
    if (req.url === "/_ns_/nstest.html") {
      return res.end();
    }

    // No auth for stats.json, dstats.json, esstats.json, eshealth.json
    if (req.url.match(/^\/([e]*[ds]*stats|eshealth).json/)) {
      return next();
    }

    // S2S Auth
    if (req.headers['x-moloch-auth']) {
      var obj = Config.auth2obj(req.headers['x-moloch-auth']);
      obj.path = obj.path.replace(Config.basePath(), "/");
      if (obj.path !== req.url) {
        console.log("ERROR - mismatch url", obj.path, req.url);
        return res.send("Unauthorized based on bad url, check logs on ", os.hostname());
      }
      if (Math.abs(Date.now() - obj.date) > 120000) { // Request has to be +- 2 minutes
        console.log("ERROR - Denying server to server based on timestamp, are clocks out of sync?", Date.now(), obj.date);
        return res.send("Unauthorized based on timestamp - check that all moloch viewer machines have accurate clocks");
      }

      if (req.url.match(/^\/receiveSession/)) {
        return next();
      }

      Db.getUserCache(obj.user, function(err, suser) {
        if (err) {return res.send("ERROR - user: " + obj.user + " err:" + err);}
        if (!suser || !suser.found) {return res.send(obj.user + " doesn't exist");}
        if (!suser._source.enabled) {return res.send(obj.user + " not enabled");}
        userCleanup(suser._source);
        req.user = suser._source;
        return next();
      });
      return;
    }

    // Header auth
    if (internals.userNameHeader !== undefined && req.headers[internals.userNameHeader] !== undefined) {
      var userName = req.headers[internals.userNameHeader];
      Db.getUserCache(userName, function(err, suser) {
        if (err) {return res.send("ERROR - " +  err);}
        if (!suser || !suser.found) {return res.send(userName + " doesn't exist");}
        if (!suser._source.enabled) {return res.send(userName + " not enabled");}
        if (!suser._source.headerAuthEnabled) {return res.send(userName + " header auth not enabled");}

        userCleanup(suser._source);
        req.user = suser._source;
        return next();
      });
      return;
    }

    // Browser auth
    req.url = req.url.replace("/", Config.basePath());
    passport.authenticate('digest', {session: false})(req, res, function (err) {
      req.url = req.url.replace(Config.basePath(), "/");
      if (err) {
        res.send(JSON.stringify({success: false, text: err}));
        return;
      } else {
        return next();
      }
    });
  });
} else {
  /* Shared password isn't set, who cares about auth, db is only used for settings */
  app.locals.alwaysShowESStatus = true;
  app.locals.noPasswordSecret   = true;
  app.use(function(req, res, next) {
    req.user = {userId: "anonymous", enabled: true, createEnabled: Config.get("regressionTests", false), webEnabled: true, headerAuthEnabled: false, emailSearch: true, removeEnabled: true, settings: {}};
    Db.getUserCache("anonymous", function(err, suser) {
        if (!err && suser && suser.found) {
          req.user.settings = suser._source.settings;
          req.user.views = suser._source.views;
        }
      next();
    });
  });
}

app.use(function(req, res, next) {
  if (!req.user || !req.user.userId) {
    return next();
  }

  var mrc = {};
  for (var key in internals.rightClicks) {
    var rc = internals.rightClicks[key];
    if (!rc.users || rc.users[req.user.userId]) {
      mrc[key] = rc;
    }
  }
  app.locals.molochRightClick = mrc;
  next();
});

logger.token('username', function(req, res){ return req.user?req.user.userId:"-"; });

function loadFields() {
  Db.loadFields(function (data) {
    Config.loadFields(data);
    app.locals.fieldsMap = JSON.stringify(Config.getFieldsMap());
    app.locals.fieldsArr = Config.getFields().sort(function(a,b) {return (a.exp > b.exp?1:-1);});
  });
}

function loadPlugins() {
  var api = {
    registerWriter: function(str, info) {
      internals.writers[str] = info;
    },
    getDb: function() { return Db; },
    getPcap: function() { return Pcap; },
  };
  var plugins = Config.get("viewerPlugins", "").split(";");
  var dirs = Config.get("pluginsDir", "/data/moloch/plugins").split(";");
  plugins.forEach(function (plugin) {
    plugin = plugin.trim();
    if (plugin === "") {
      return;
    }
    var found = false;
    dirs.forEach(function (dir) {
      dir = dir.trim();
      if (found || dir === "") {
        return;
      }
      if (fs.existsSync(dir + "/" + plugin)) {
        found = true;
        var p = require(dir + "/" + plugin);
        p.init(Config, internals.pluginEmitter, api);
      }
    });
    if (!found) {
      console.log("WARNING - Couldn't find plugin", plugin, "in", dirs);
    }
  });
}

//////////////////////////////////////////////////////////////////////////////////
//// Utility
//////////////////////////////////////////////////////////////////////////////////
function isEmptyObject(object) { for(var i in object) { return false; } return true; }
function safeStr(str) {
  return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/\"/g,'&quot;').replace(/\'/g, '&#39;').replace(/\//g, '&#47;');
}

function twoDigitString(value) {
  return (value < 10) ? ("0" + value) : value.toString();
}

function queryValueToArray(val) {
  if (val === undefined || val === null) {
    return [];
  }
  if (!Array.isArray(val)) {
    val = [val];
  }
  return val.join(",").split(",");
}

var FMEnum = Object.freeze({other: 0, ip: 1, tags: 2, hh: 3});
function fmenum(field) {
  var fieldsMap = Config.getFieldsMap();
  if (field.match(/^(a1|a2|xff|dnsip|eip|socksip)$/) !== null ||
      fieldsMap[field] && fieldsMap[field].type === "ip") {
    return FMEnum.ip;
  } else if (field.match(/^(ta)$/) !== null) {
    return FMEnum.tags;
  } else if (field.match(/^(hh1|hh2)$/) !== null) {
    return FMEnum.hh;
  }
  return FMEnum.other;
}

function errorString(err, result) {
  var str;
  if (err && typeof err === "string") {
    str = err;
  } else if (err && typeof err.message === "string") {
    str = err.message;
  } else if (result && result.error) {
    str = result.error;
  } else {
    str = "Unknown issue, check logs";
    console.log(err, result);
  }

  if (str.match("IndexMissingException")) {
    return "Moloch's Elasticsearch database has no matching session indices for timeframe selected";
  } else {
    return "Elasticsearch error: " + str;
  }
}

// http://stackoverflow.com/a/10934946
function dot2value(obj, str) {
      return str.split(".").reduce(function(o, x) { return o[x]; }, obj);
}

function createSessionDetailNew() {
  var found = {};
  var dirs = [];

  dirs = dirs.concat(Config.get("pluginsDir", "/data/moloch/plugins").split(';'));
  dirs = dirs.concat(Config.get("parsersDir", "/data/moloch/parsers").split(';'));

  dirs.forEach(function(dir) {
    try {
      var files = fs.readdirSync(dir);
      // sort().reverse() so in this dir pug is processed before jade
      files.sort().reverse().forEach(function(file) {
        var sfile = file.replace(/\.(pug|jade)/, "");
        if (found[sfile]) {
          return;
        }
        if (file.match(/\.detail\.jade$/i)) {
          found[sfile] = fs.readFileSync(dir + "/" + file, 'utf8').replace(/^/mg, "  ") + "\n";
        } else if (file.match(/\.detail\.pug$/i)) {
          found[sfile] = "  include " + dir + "/" + file + "\n";
        }
      });
    } catch (e) {}
  });

  var makers = internals.pluginEmitter.listeners("makeSessionDetail");
  async.each(makers, function(cb, nextCb) {
    cb(function (err, items) {
      for (var k in items) {
        found[k] = items[k].replace(/^/mg, "  ") + "\n";
      }
      return nextCb();
    });
  }, function () {
    internals.sessionDetailNew = "include views/mixins.pug\n" +
                                 "div.sessionDetail(sessionid=session.id)\n" +
                                 "  include views/sessionDetail\n";
    Object.keys(found).sort().forEach(function(k) {
      internals.sessionDetailNew += found[k];
    });

    internals.sessionDetailNew = internals.sessionDetailNew.replace(/div.sessionDetailMeta.bold/g, "h4.sessionDetailMeta")
                                                           .replace(/dl.sessionDetailMeta/g, "dl")
                                                           .replace(/a.moloch-right-click.*molochexpr='([^']+)'.*#{(.*)}/g, "+clickableValue('$1', $2)")
                                                           ;
  });
}

function createSessionDetail() {
  createSessionDetailNew();
}

function createRightClicks() {

  var mrc = Config.configMap("right-click");
  for (var key in mrc) {
    if (mrc[key].fields) {
      mrc[key].fields = mrc[key].fields.split(",");
    }
    if (mrc[key].users) {
      var users = {};
      mrc[key].users.split(",").forEach(function(item) {
        users[item] = 1;
      });
      mrc[key].users = users;
    }
  }
  var makers = internals.pluginEmitter.listeners("makeRightClick");
  async.each(makers, function(cb, nextCb) {
    cb(function (err, items) {
      for (var k in items) {
        mrc[k] = items[k];
        if (mrc[k].fields && !Array.isArray(mrc[k].fields)) {
          mrc[k].fields = mrc[k].fields.split(",");
        }
      }
      return nextCb();
    });
  }, function () {
    internals.rightClicks = mrc;
  });
}

//https://coderwall.com/p/pq0usg/javascript-string-split-that-ll-return-the-remainder
function splitRemain(str, separator, limit) {
    str = str.split(separator);
    if(str.length <= limit) {return str;}

    var ret = str.splice(0, limit);
    ret.push(str.join(separator));

    return ret;
}

function arrayZeroFill(n) {
  var a = [];
  while (n > 0) {
    a.push(0);
    n--;
  }
  return a;
}

//////////////////////////////////////////////////////////////////////////////////
//// Requests
//////////////////////////////////////////////////////////////////////////////////

function addAuth(info, user, node, secret) {
    if (!info.headers) {
        info.headers = {};
    }
    info.headers['x-moloch-auth'] = Config.obj2auth({date: Date.now(),
                                                     user: user.userId,
                                                     node: node,
                                                     path: info.path
                                                    }, secret);
}

function addCaTrust(info, node) {
  if (!Config.isHTTPS(node)) {
    return;
  }

  if ((internals.caTrustCerts[node] !== undefined) && (internals.caTrustCerts[node].length > 0)) {
    info.ca = internals.caTrustCerts[node];
    info.agent.options.ca = internals.caTrustCerts[node];
    return;
  }

  var caTrustFile = Config.getFull(node, "caTrustFile");

  if (caTrustFile && caTrustFile.length > 0) {
    var caTrustFileLines = fs.readFileSync(caTrustFile, 'utf8');
    caTrustFileLines = caTrustFileLines.split("\n");

    var foundCert = [],
        line;

    internals.caTrustCerts[node] = [];

    for (var i = 0, ilen = caTrustFileLines.length; i < ilen; i++) {
      line = caTrustFileLines[i];
      if (line.length === 0) {
        continue;
      }
      foundCert.push(line);
      if (line.match(/-END CERTIFICATE-/)) {
        internals.caTrustCerts[node].push(foundCert.join("\n"));
        foundCert = [];
      }
    }

    if (internals.caTrustCerts[node].length > 0) {
      info.ca = internals.caTrustCerts[node];
      info.agent.options.ca = internals.caTrustCerts[node];
      return;
    }
  }
}

function noCache(req, res, ct) {
  res.header('Cache-Control', 'no-cache, private, no-store, must-revalidate, max-stale=0, post-check=0, pre-check=0');
  if (ct) {
    res.setHeader("Content-Type", ct);
  }
}

function getViewUrl(node, cb) {
  if (Array.isArray(node)) {
    node = node[0];
  }

  var url = Config.getFull(node, "viewUrl");
  if (url) {
    cb(null, url, url.slice(0, 5) === "https"?https:http);
    return;
  }

  Db.molochNodeStatsCache(node, function(err, stat) {
    if (err) {
      return cb(err);
    }

    if (Config.isHTTPS(node)) {
      cb(null, "https://" + stat.hostname + ":" + Config.getFull(node, "viewPort", "8005"), https);
    } else {
      cb(null, "http://" + stat.hostname + ":" + Config.getFull(node, "viewPort", "8005"), http);
    }
  });
}

function proxyRequest (req, res, errCb) {
  noCache(req, res);

  getViewUrl(req.params.nodeName, function(err, viewUrl, client) {
    if (err) {
      if (errCb) {
        return errCb(err);
      }
      console.log("ERROR - ", err);
      res.send("Can't find view url for '" + req.params.nodeName + "' check viewer logs on " + os.hostname());
    }
    var info = url.parse(viewUrl);
    info.path = req.url;
    info.agent = (client === http?internals.httpAgent:internals.httpsAgent);
    info.rejectUnauthorized = true;
    addAuth(info, req.user, req.params.nodeName);
    addCaTrust(info, req.params.nodeName);

    var preq = client.request(info, function(pres) {
      if (pres.headers['content-type']) {
        res.setHeader('content-type', pres.headers['content-type']);
      }
      pres.on('data', function (chunk) {
        res.write(chunk);
      });
      pres.on('end', function () {
        res.end();
      });
    });

    preq.on('error', function (e) {
      if (errCb) {
        return errCb(e);
      }
      console.log("ERROR - Couldn't proxy request=", info, "\nerror=", e);
      res.send("Error talking to node '" + req.params.nodeName + "' using host '" + info.host + "' check viewer logs on " + os.hostname());
    });
    preq.end();
  });
}

function isLocalView (node, yesCb, noCb) {
  var pcapWriteMethod = Config.getFull(node, "pcapWriteMethod");
  var writer = internals.writers[pcapWriteMethod];
  if (writer && writer.localNode === false) {
    return yesCb();
  }
  return Db.isLocalView(node, yesCb, noCb);
}

//////////////////////////////////////////////////////////////////////////////////
//// Middleware
//////////////////////////////////////////////////////////////////////////////////
function checkProxyRequest(req, res, next) {
  isLocalView(req.params.nodeName, function () {
    return next();
  },
  function () {
    return proxyRequest(req, res);
  });
}

function checkToken(req, res, next) {
  if (!req.body.token) {
    return res.send(JSON.stringify({success: false, text: "Missing token"}));
  }

  req.token = Config.auth2obj(req.body.token);
  var diff = Math.abs(Date.now() - req.token.date);
  if (diff > 2400000 || req.token.pid !== process.pid || req.token.userId !== req.user.userId) {
    console.trace("bad token", req.token);
    return res.send(JSON.stringify({success: false, text: "Timeout - Please try reloading page and repeating the action"}));
  }

  // Shorter token timeout if editing someone elses info
  if (req.token.suserId && req.token.userId !== req.user.userId && diff > 600000) {
    console.trace("admin bad token", req.token);
    return res.send(JSON.stringify({success: false, text: "Admin Timeout - Please try reloading page and repeating the action"}));
  }

  return next();
}


function checkCookieToken(req, res, next) {
  function error(text) {
    res.status(500);
    return res.send(JSON.stringify({ success: false, text: text }));
  }

  if (!req.headers['x-moloch-cookie']) {
    return error('Missing token');
  }

  req.token = Config.auth2obj(req.headers['x-moloch-cookie']);
  var diff = Math.abs(Date.now() - req.token.date);
  if (diff > 2400000 || req.token.pid !== process.pid ||
      req.token.userId !== req.user.userId) {
    console.trace('bad token', req.token);
    return error('Timeout - Please try reloading page and repeating the action');
  }

  return next();
}

function checkWebEnabled(req, res, next) {
  if (!req.user.webEnabled) {
    return res.send("Moloch Permision Denied");
  }

  return next();
}
//////////////////////////////////////////////////////////////////////////////////
//// Pages
//////////////////////////////////////////////////////////////////////////////////
// APIs disabled in demoMode, needs to be before real callbacks
if (Config.get('demoMode', false)) {
  console.log("WARNING - Starting in demo mode, some APIs disabled");
  app.all(['/settings', '/users'], function(req, res) {
    return res.send('Disabled in demo mode.');
  });

  app.get(['/user/settings', '/user/cron'], function(req, res) {
    res.status(403);
    return res.send(JSON.stringify({success: false, text: "Disabled in demo mode."}));
  });

  app.post(['/user/password/change', '/changePassword', '/tableState/:tablename'], function(req, res) {
    res.status(403);
    return res.send(JSON.stringify({success: false, text: "Disabled in demo mode."}));
  });
}

function makeTitle(req, page) {
  var title = Config.get("titleTemplate", "_cluster_ - _page_ _-view_ _-expression_");
  title = title.replace(/_cluster_/g, internals.clusterName)
               .replace(/_page_/g, page)
               .replace(/_userId_/g, req.user?req.user.userId:"-")
               .replace(/_userName_/g, req.user?req.user.userName:"-")
               ;
  return title;
}

app.get(['/', '/app'], function(req, res) {
  var question = req.url.indexOf("?");
  if (question === -1) {
    res.redirect("sessions");
  } else {
    res.redirect("sessions" + req.url.substring(question));
  }
});

app.get('/about', checkWebEnabled, function(req, res) {
  res.redirect("help");
});

app.get('/molochclusters', function(req, res) {
  function cloneClusters(clusters) {
    var clone = {};

    for (var key in app.locals.molochClusters) {
      if (app.locals.molochClusters.hasOwnProperty(key)) {
        var cluster = app.locals.molochClusters[key];
        clone[key] = {
          name: cluster.name,
          url : cluster.url
        };
      }
    }

    return clone;
  }

  if(!app.locals.molochClusters) {
    var molochClusters = Config.configMap("moloch-clusters");

    if (!molochClusters) {
      res.status(404);
      return res.send('Cannot locate right clicks');
    }

    var clustersClone = cloneClusters(molochClusters);

    return res.send(clustersClone);
  }

  var clustersClone = cloneClusters(app.locals.molochClusters);

  return res.send(clustersClone);
});

// angular app bundles
app.get('/app.bundle.js', function(req, res) {
  res.sendFile(__dirname + '/bundle/app.bundle.js');
});
app.get('/vendor.bundle.js', function(req, res) {
  res.sendFile(__dirname + '/bundle/vendor.bundle.js');
});

// source maps
app.get('/app.bundle.js.map', function(req, res) {
  res.sendFile(__dirname + '/bundle/app.bundle.js.map');
});
app.get('/vendor.bundle.js.map', function(req, res) {
  res.sendFile(__dirname + '/bundle/vendor.bundle.js.map');
});

// custom user css
app.get('/user.css', function(req, res) {
  fs.readFile("./views/user.styl", 'utf8', function(err, str) {
    if (err) { return console.log("ERROR - ", err); }
    if (!req.user.settings.theme) {
      return console.log("ERROR - no custom theme defined");
    }

    var style = stylus(str);

    var colors = req.user.settings.theme.split(':')[1].split(',');

    style.define('colorBackground', new stylus.nodes.Literal(colors[0]));
    style.define('colorForeground', new stylus.nodes.Literal(colors[1]));
    style.define('colorForegroundAccent', new stylus.nodes.Literal(colors[2]));

    style.define('colorWhite', new stylus.nodes.Literal('#FFFFFF'));
    style.define('colorBlack', new stylus.nodes.Literal('#333333'));
    style.define('colorGray', new stylus.nodes.Literal('#CCCCCC'));
    style.define('colorGrayDark', new stylus.nodes.Literal('#777777'));
    style.define('colorGrayDarker', new stylus.nodes.Literal('#555555'));
    style.define('colorGrayLight', new stylus.nodes.Literal('#EEEEEE'));
    style.define('colorGrayLighter', new stylus.nodes.Literal('#F6F6F6'));

    style.define('colorPrimary', new stylus.nodes.Literal(colors[3]));
    style.define('colorPrimaryLightest', new stylus.nodes.Literal(colors[4]));
    style.define('colorSecondary', new stylus.nodes.Literal(colors[5]));
    style.define('colorSecondaryLightest', new stylus.nodes.Literal(colors[6]));
    style.define('colorTertiary', new stylus.nodes.Literal(colors[7]));
    style.define('colorTertiaryLightest', new stylus.nodes.Literal(colors[8]));
    style.define('colorQuaternary', new stylus.nodes.Literal(colors[9]));
    style.define('colorQuaternaryLightest', new stylus.nodes.Literal(colors[10]));

    style.define('colorWater', new stylus.nodes.Literal(colors[11]));
    style.define('colorLand', new stylus.nodes.Literal(colors[12]));
    style.define('colorSrc', new stylus.nodes.Literal(colors[13]));
    style.define('colorDst', new stylus.nodes.Literal(colors[14]));

    style.render(function(err, css){
      if (err) {return console.log("ERROR - ", err);}
      var date = new Date().toUTCString();
      res.setHeader('Content-Type', 'text/css');
      res.setHeader('Date', date);
      res.setHeader('Cache-Control', 'public, max-age=0');
      res.setHeader('Last-Modified', date);
      res.send(css);
    });
  });
});


/* User Endpoints ---------------------------------------------------------- */
// default settings for users with no settings
var settingDefaults = {
  timezone      : 'local',
  detailFormat  : 'last',
  showTimestamps: 'last',
  sortColumn    : 'start',
  sortDirection : 'asc',
  spiGraph      : 'no',
  connSrcField  : 'a1',
  connDstField  : 'ip.dst:port',
  numPackets    : 'last',
  theme         : 'default-theme'
};

// gets the current user
app.get('/user/current', function(req, res) {
  Db.getUserCache(req.user.userId, function(err, user) {
    if (err || !user || !user.found) {
      if (app.locals.noPasswordSecret) {
        return res.send(req.user);
      } else {
        console.log('/user/current error', err, user);
        res.status(403);
        return res.send(JSON.stringify({success: false, text: 'Unknown user'}));
      }
    }

    var userProps = ['createEnabled', 'emailSearch', 'enabled', 'removeEnabled',
      'headerAuthEnabled', 'settings', 'userId', 'webEnabled'];

    var clone     = {};
    var source    = user._source;

    for (var i = 0, len = userProps.length; i < len; ++i) {
      var prop = userProps[i];
      if (source.hasOwnProperty(prop)) {
        clone[prop] = source[prop];
      }
    }

    clone['canUpload'] = app.locals.allowUploads;

    // If no settings, use defaults
    if (clone.settings === undefined) {clone.settings = settingDefaults;}

    // Use settingsDefaults for any settings that are missing
    for (var item in settingDefaults) {
      if (clone.settings[item] === undefined) {clone.settings[item] = settingDefaults[item];}
    }

    return res.send(clone);
  });
});

// gets a user's settings
app.get('/user/settings', function(req, res) {
  function error(status, text) {
    res.status(status || 403);
    return res.send(JSON.stringify({ success: false, text: text }));
  }

  var userId = req.user.userId;                         // get current user
  if (req.query.userId) { userId = req.query.userId; }  // or requested user

  if (req.query.userId && (req.query.userId !== req.user.userId) && !req.user.createEnabled) {
    // user is trying to get another user's settings without admin privilege
    return error(403, 'Need admin privileges');
  }

  Db.getUserCache(userId, function(err, user) {
    if (err || !user || !user.found) {
      if (app.locals.noPasswordSecret) {
        // TODO: send anonymous user's settings
        return res.send('{}');
      } else {
        console.log('Unknown user', err, user);
        return error(404, 'User not found');
      }
    }

    var settings = user._source.settings || settingDefaults;

    res.cookie(
       'MOLOCH-COOKIE',
       Config.obj2auth({date: Date.now(), pid: process.pid, userId: req.user.userId}),
       { path: app.locals.basePath }
    );

    return res.send(settings);
  });
});

// updates a user's settings
app.post('/user/settings/update', checkCookieToken, function(req, res) {
  function error(status, text) {
    res.status(status || 403);
    return res.send(JSON.stringify({ success: false, text: text }));
  }

  var userId = req.user.userId;                         // get current user
  if (req.query.userId) { userId = req.query.userId; }  // or requested user

  if (req.query.userId && (req.query.userId !== req.user.userId) && !req.user.createEnabled) {
    // user is trying to update another user's settings without admin privilege
    return error(403, 'Need admin privileges');
  }

  Db.getUser(userId, function(err, user) {
    if (err || !user.found) {
      console.log('/user/settings/update failed', err, user);
      return error(403, 'Unknown user');
    }

    user = user._source;
    user.settings = req.body;
    delete user.settings.token;

    Db.setUser(user.userId, user, function(err, info) {
      if (err) {
        console.log('/user/settings/update error', err, info);
        return error(500, 'Settings update failed');
      }
      return res.send(JSON.stringify({
        success : true,
        text    : 'Updated settings successfully'
      }));
    });
  });
});

// gets a user's views
app.get('/user/views', function(req, res) {
  var userId = req.user.userId;                         // get current user
  if (req.query.userId) { userId = req.query.userId; }  // or requested user

  if (req.query.userId && (req.query.userId !== req.user.userId) && !req.user.createEnabled) {
    // user is trying to get another user's views without admin privilege
    res.status(403);
    return res.send(JSON.stringify({success: false, text: 'Need admin privileges'}));
  }

  Db.getUserCache(userId, function(err, user) {
    if (err || !user || !user.found) {
      if (app.locals.noPasswordSecret) {
        // TODO: send anonymous user's views
        return res.send('{}');
      } else {
        console.log('Unknown user', err, user);
        return res.send('{}');
      }
    }

    var views = user._source.views || {};

    return res.send(views);
  });
});

// creates a new view for a user
app.post('/user/views/create', checkCookieToken, function(req, res) {
  function error(status, text) {
    res.status(status || 403);
    return res.send(JSON.stringify({ success: false, text: text }));
  }

  if (req.query.userId && (req.query.userId !== req.user.userId) && !req.user.createEnabled) {
    // user is trying to create a view for another user without admin privilege
    return error(403, 'Need admin privileges');
  }

  if (!req.body.viewName)   { return error(403, 'Missing view name'); }
  if (!req.body.expression) { return error(403, 'Missing view expression'); }

  var userId = req.user.userId;                         // get current user
  if (req.query.userId) { userId = req.query.userId; }  // or requested user

  Db.getUser(userId, function(err, user) {
    if (err || !user.found) {
      console.log('/user/views/create failed', err, user);
      return error(403, 'Unknown user');
    }

    user = user._source;
    user.views = user.views || {};
    var container = user.views;
    if (req.body.groupName) {
      req.body.groupName = req.body.groupName.replace(/[^-a-zA-Z0-9_: ]/g, '');
      if (!user.views._groups) {
        user.views._groups = {};
      }
      if (!user.views._groups[req.body.groupName]) {
        user.views._groups[req.body.groupName] = {};
      }
      container = user.views._groups[req.body.groupName];
    }
    req.body.viewName = req.body.viewName.replace(/[^-a-zA-Z0-9_: ]/g, '');
    if (container[req.body.viewName]) {
      container[req.body.viewName].expression = req.body.expression;
    } else {
      container[req.body.viewName] = {expression: req.body.expression};
    }

    Db.setUser(user.userId, user, function(err, info) {
      if (err) {
        console.log('/user/views/create error', err, info);
        return error(500, 'Create view failed');
      }
      return res.send(JSON.stringify({
        success : true,
        text    : 'Created view successfully',
        views   : user.views
      }));
    });
  });
});

// deletes a user's specified view
app.post('/user/views/delete', checkCookieToken, function(req, res) {
  function error(status, text) {
    res.status(status || 403);
    return res.send(JSON.stringify({ success: false, text: text }));
  }

  if (req.query.userId && (req.query.userId !== req.user.userId) && !req.user.createEnabled) {
    // user is trying to delete another user's view without admin privilege
    return error(403, 'Need admin privileges');
  }

  if (!req.body.view) { return error(403, 'Missing view'); }

  var userId = req.user.userId;                         // get current user
  if (req.query.userId) { userId = req.query.userId; }  // or requested user

  Db.getUser(userId, function(err, user) {
    if (err || !user.found) {
      console.log('/user/views/delete failed', err, user);
      return error(403, 'Unknown user');
    }

    user = user._source;
    user.views = user.views || {};
    delete user.views[req.body.view];

    Db.setUser(user.userId, user, function(err, info) {
      if (err) {
        console.log('/user/views/delete failed', err, info);
        return error(500, 'Delete view failed');
      }
      return res.send(JSON.stringify({
        success : true,
        text    : 'Deleted view successfully'
      }));
    });
  });
});

// updates a user's specified view
app.post('/user/views/update', function(req, res) {
  function error(status, text) {
    res.status(status || 403);
    return res.send(JSON.stringify({ success: false, text: text }));
  }

  if (req.query.userId && (req.query.userId !== req.user.userId) && !req.user.createEnabled) {
    // user is trying to update another user's view without admin privilege
    return error(403, 'Need admin privileges');
  }

  if (!req.body.name)       { return error(403, 'Missing view name'); }
  if (!req.body.expression) { return error(403, 'Missing view expression'); }
  if (!req.body.key)        { return error(403, 'Missing view key'); }

  var userId = req.user.userId;                         // get current user
  if (req.query.userId) { userId = req.query.userId; }  // or requested user

  Db.getUser(userId, function(err, user) {
    if (err || !user.found) {
      console.log('/user/views/update failed', err, user);
      return error(403, 'Unknown user');
    }

    user = user._source;
    user.views = user.views || {};
    var container = user.views;
    if (req.body.groupName) {
      req.body.groupName = req.body.groupName.replace(/[^-a-zA-Z0-9_: ]/g, '');
      if (!user.views._groups) {
        user.views._groups = {};
      }
      if (!user.views._groups[req.body.groupName]) {
        user.views._groups[req.body.groupName] = {};
      }
      container = user.views._groups[req.body.groupName];
    }
    req.body.name = req.body.name.replace(/[^-a-zA-Z0-9_: ]/g, '');
    if (container[req.body.name]) {
      container[req.body.name].expression = req.body.expression;
    } else {
      container[req.body.name] = {expression: req.body.expression};
    }

    // delete the old one if the key (view name) has changed
    if (user.views[req.body.key] && req.body.name !== req.body.key) {
      user.views[req.body.key] = null;
      delete user.views[req.body.key];
    }

    Db.setUser(user.userId, user, function(err, info) {
      if (err) {
        console.log('/user/views/update error', err, info);
        return error(500, 'Updating view failed');
      }
      return res.send(JSON.stringify({
        success : true,
        text    : 'Updated view successfully',
        views   : user.views
      }));
    });
  });
});

// gets a user's cron queries
app.get('/user/cron', function(req, res) {
  function error(text) {
    res.status(403);
    return res.send(JSON.stringify({ success: false, text: text }));
  }

  function sendCronQueries(user, cp) {
    if (user.settings === undefined) {user.settings = {};}
    Db.search('queries', 'query', {size:1000, query: {term: {creator: user.userId}}}, function (err, data) {
      if (err || data.error) {
        console.log('/user/cron error', err || data.error);
      }

      let queries = {};

      if (data && data.hits && data.hits.hits) {
        user.queries = {};
        data.hits.hits.forEach(function(item) {
          queries[item._id] = item._source;
        });
      }

      res.send(queries);
    });
  }

  if (req.query.userId) {
    if (!req.user.createEnabled && req.query.userId !== req.user.userId) {
      // user is trying to get another user's cron queries without admin privilege
      return error('Need admin privileges');
    }
    Db.getUser(req.query.userId, function(err, user) {
      if (err || !user.found) {
        console.log('/user/cron error', err, user);
        return error('Unknown user');
      }
      sendCronQueries(user._source, 0);
    });
  } else {
    sendCronQueries(req.user, 1);
  }
});

// creates a new cron query for a user
app.post('/user/cron/create', checkCookieToken, function(req, res) {
  function error(status, text) {
    res.status(status || 403);
    return res.send(JSON.stringify({ success: false, text: text }));
  }

  if (req.query.userId && (req.query.userId !== req.user.userId) && !req.user.createEnabled) {
    // user is trying to create a cron query for another user without admin privilege
    return error(403, 'Need admin privileges');
  }

  if (!req.body.name)   { return error(403, 'Missing cron query name'); }
  if (!req.body.query)  { return error(403, 'Missing cron query expression'); }
  if (!req.body.action) { return error(403, 'Missing cron query action'); }
  if (!req.body.tags)   { return error(403, 'Missing cron query tag(s)'); }

  var document = {
    doc: {
      enabled : true,
      name    : req.body.name,
      query   : req.body.query,
      tags    : req.body.tags,
      action  : req.body.action
    }
  };

  var userId = req.user.userId;                         // get current user
  if (req.query.userId) { userId = req.query.userId; }  // or requested user

  if (req.body.since === '-1') {
    document.doc.lpValue =  document.doc.lastRun = 0;
  } else {
    document.doc.lpValue =  document.doc.lastRun =
       Math.floor(Date.now()/1000) - 60*60*parseInt(req.body.since || '0', 10);
  }
  document.doc.count = 0;
  document.doc.creator = userId || 'anonymous';

  Db.indexNow('queries', 'query', null, document.doc, function(err, info) {
    if (err) {
      console.log('/user/cron/create error', err, info);
      return error(500, 'Create cron query failed');
    }
    if (Config.get('cronQueries', false)) {
      processCronQueries();
    }
    return res.send(JSON.stringify({
      success : true,
      text    : 'Created cron query successfully',
      key     : info._id
    }));
  });
});

// deletes a user's specified cron query
app.post('/user/cron/delete', checkCookieToken, function(req, res) {
  function error(status, text) {
    res.status(status || 403);
    return res.send(JSON.stringify({ success: false, text: text }));
  }

  if (req.query.userId && (req.query.userId !== req.user.userId) && !req.user.createEnabled) {
    // user is trying to delete a cron query for another user without admin privilege
    return error(403, 'Need admin privileges');
  }

  if (!req.body.key) { return error(403, 'Missing cron query key'); }

  Db.deleteDocument('queries', 'query', req.body.key, {refresh: 1}, function(err, sq) {
    if (err) {
      console.log('/user/cron/delete error', err, info);
      return error(500, 'Delete cron query failed');
    }
    res.send(JSON.stringify({
      success : true,
      text    : 'Deleted cron query successfully'
    }));
  });
});

// updates a user's specified cron query
app.post('/user/cron/update', checkCookieToken, function(req, res) {
  function error(status, text) {
    res.status(status || 403);
    return res.send(JSON.stringify({ success: false, text: text }));
  }

  if (req.query.userId && (req.query.userId !== req.user.userId) && !req.user.createEnabled) {
    // user is trying to update a cron query for another user without admin privilege
    return error(403, 'Need admin privileges');
  }

  if (!req.body.key)    { return error(403, 'Missing cron query key'); }
  if (!req.body.name)   { return error(403, 'Missing cron query name'); }
  if (!req.body.query)  { return error(403, 'Missing cron query expression'); }
  if (!req.body.action) { return error(403, 'Missing cron query action'); }
  if (!req.body.tags)   { return error(403, 'Missing cron query tag(s)'); }

  var document = {
    doc: {
      enabled : req.body.enabled,
      name    : req.body.name,
      query   : req.body.query,
      tags    : req.body.tags,
      action  : req.body.action
    }
  };

  Db.get('queries', 'query', req.body.key, function(err, sq) {
    if (err || !sq.found) {
      console.log('/user/cron/update failed', err, sq);
      return error(403, 'Unknown query');
    }

    Db.update('queries', 'query', req.body.key, document, {refresh: 1}, function(err, data) {
      if (err) {
        console.log('/user/cron/update error', err, document, data);
        return error(500, 'Cron query update failed');
      }
      if (Config.get('cronQueries', false)) {
        processCronQueries();
      }
      return res.send(JSON.stringify({
        success : true,
        text    : 'Updated cron query successfully'
      }));
    });
  });
});

// changes a user's password
app.post('/user/password/change', checkCookieToken, function(req, res) {
  function error(status, text) {
    res.status(status || 403);
    return res.send(JSON.stringify({ success: false, text: text }));
  }

  if (req.query.userId && (req.query.userId !== req.user.userId) && !req.user.createEnabled) {
    // user is trying to change password for another user without admin privilege
    return error(403, 'Need admin privileges');
  }

  if (!req.body.newPassword || req.body.newPassword.length < 3) {
    return error(403, 'New password needs to be at least 3 characters');
  }

  if (!req.query.userId && (req.user.passStore !==
     Config.pass2store(req.token.userId, req.body.currentPassword) ||
     req.token.userId !== req.user.userId)) {
    return error(403, 'Current password mismatch');
  }

  var userId = req.user.userId;                         // get current user
  if (req.query.userId) { userId = req.query.userId; }  // or requested user

  Db.getUser(userId, function(err, user) {
    if (err || !user.found) {
      console.log('/user/password/change error', err, user);
      return error(403, 'Unknown user');
    }

    user = user._source;
    user.passStore = Config.pass2store(user.userId, req.body.newPassword);

    Db.setUser(user.userId, user, function(err, info) {
      if (err) {
        console.log('/user/password/change error', err, info);
        return error(500, 'Update failed');
      }
      return res.send(JSON.stringify({
        success : true,
        text    : 'Changed password successfully'
      }));
    });
  });
});

// gets custom column configurations for a user
app.get('/user/columns', function(req, res) {
  Db.getUserCache(req.user.userId, function(err, user) {
    if (err || !user || !user.found) {
      if (app.locals.noPasswordSecret) {
        // TODO: send anonymous user's views
        return res.send('[]');
      } else {
        console.log('Unknown user', err, user);
        return res.send('[]');
      }
    }

    var columnConfigurations = user._source.columnConfigs || [];

    return res.send(columnConfigurations);
  });
});

// creates a new custom column configuration for a user
app.post('/user/columns/create', checkCookieToken, function(req, res) {
  function error(status, text) {
    res.status(status || 403);
    return res.send(JSON.stringify({ success: false, text: text }));
  }

  if (req.query.userId && (req.query.userId !== req.user.userId) && !req.user.createEnabled) {
    // user is trying to create a view for another user without admin privilege
    return error(403, 'Need admin privileges');
  }

  if (!req.body.name)     { return error(403, 'Missing custom column configuration name'); }
  if (!req.body.columns)  { return error(403, 'Missing columns'); }
  if (!req.body.order)    { return error(403, 'Missing sort order'); }

  var userId = req.user.userId;                         // get current user
  if (req.query.userId) { userId = req.query.userId; }  // or requested user

  Db.getUser(userId, function(err, user) {
    if (err || !user.found) {
      console.log('/user/columns/create failed', err, user);
      return error(403, 'Unknown user');
    }

    req.body.name = req.body.name.replace(/[^-a-zA-Z0-9\s_:]/g, '');

    if (req.body.name.length < 1) {
      return error(403, 'Invalid custom column configuration name');
    }

    user = user._source;
    user.columnConfigs = user.columnConfigs || [];

    var duplicate = false;
    // don't let user use duplicate names
    for (var i = 0, len = user.columnConfigs.length; i < len; ++i) {
      if (req.body.name === user.columnConfigs[i].name) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) { return error(403, 'There is already a custom column with that name'); }

    user.columnConfigs.push({
      name    : req.body.name,
      columns : req.body.columns,
      order   : req.body.order
    });

    Db.setUser(user.userId, user, function(err, info) {
      if (err) {
        console.log('/user/columns/create error', err, info);
        return error(500, 'Create custom column configuration failed');
      }
      return res.send(JSON.stringify({
        success : true,
        text    : 'Created custom column configuration successfully',
        name    : req.body.name
      }));
    });
  });
});

// deletes a user's specified custom column configuration
app.post('/user/columns/delete', checkCookieToken, function(req, res) {
  function error(status, text) {
    res.status(status || 403);
    return res.send(JSON.stringify({ success: false, text: text }));
  }

  if (req.query.userId && (req.query.userId !== req.user.userId) && !req.user.createEnabled) {
    // user is trying to delete another user's view without admin privilege
    return error(403, 'Need admin privileges');
  }

  if (!req.body.name) { return error(403, 'Missing custom column configuration name'); }

  var userId = req.user.userId;                         // get current user
  if (req.query.userId) { userId = req.query.userId; }  // or requested user

  Db.getUser(userId, function(err, user) {
    if (err || !user.found) {
      console.log('/user/columns/delete failed', err, user);
      return error(403, 'Unknown user');
    }

    user = user._source;
    user.columnConfigs = user.columnConfigs || [];

    for (var i = 0, len = user.columnConfigs.length; i < len; ++i) {
      if (req.body.name === user.columnConfigs[i].name) {
        user.columnConfigs.splice(i, 1);
        break;
      }
    }

    Db.setUser(user.userId, user, function(err, info) {
      if (err) {
        console.log('/user/columns/delete failed', err, info);
        return error(500, 'Delete custom column configuration failed');
      }
      return res.send(JSON.stringify({
        success : true,
        text    : 'Deleted custom column configuration successfully'
      }));
    });
  });
});

app.get('/decodings', function(req, res) {
  var decodeItems = decode.settings();
  res.send(JSON.stringify(decodeItems));
});


//////////////////////////////////////////////////////////////////////////////////
//// EXPIRING
//////////////////////////////////////////////////////////////////////////////////
// Search for all files on a set of nodes in a set of directories.
// If less then size items are returned we don't delete anything.
// Doesn't support mounting sub directories in main directory, don't do it.
function expireDevice (nodes, dirs, minFreeSpaceG, nextCb) {
  var query = { _source: [ 'num', 'name', 'first', 'size', 'node' ],
                  from: '0',
                  size: 200,
                 query: { bool: {
                    must: [
                          {terms: {node: nodes}},
                          { bool: {should: []}}
                        ],
                    must_not: { term: {locked: 1}}
                }},
                sort: { first: { order: 'asc' } } };

  Object.keys(dirs).forEach( function (pcapDir) {
    var obj = {wildcard: {}};
    if (pcapDir[pcapDir.length - 1] === "/") {
      obj.wildcard.name = pcapDir + "*";
    } else {
      obj.wildcard.name = pcapDir + "/*";
    }
    query.query.bool.must[1].bool.should.push(obj);
  });

  // Keep at least 10 files
  Db.search('files', 'file', query, function(err, data) {
      if (err || data.error || !data.hits || data.hits.total <= 10) {
        return nextCb();
      }
      async.forEachSeries(data.hits.hits, function(item, forNextCb) {
        if (data.hits.total <= 10) {
          return forNextCb("DONE");
        }

        var fields = item._source || item.fields;

        var freeG;
        try {
          var stat = fs.statVFS(fields.name);
          freeG = stat.f_frsize/1024.0*stat.f_bavail/(1024.0*1024.0);
        } catch (e) {
          console.log("ERROR", e);
          // File doesn't exist, delete it
          freeG = minFreeSpaceG - 1;
        }
        if (freeG < minFreeSpaceG) {
          data.hits.total--;
          console.log("Deleting", item);
          return Db.deleteFile(fields.node, item._id, fields.name, forNextCb);
        } else {
          return forNextCb("DONE");
        }
      }, function () {
        return nextCb();
      });
  });
}

function expireCheckDevice (nodes, stat, nextCb) {
  var doit = false;
  var minFreeSpaceG = 0;
  async.forEach(nodes, function(node, cb) {
    var freeSpaceG = Config.getFull(node, "freeSpaceG", "5%");
    if (freeSpaceG[freeSpaceG.length-1] === "%") {
      freeSpaceG = (+freeSpaceG.substr(0,freeSpaceG.length-1)) * 0.01 * stat.f_frsize/1024.0*stat.f_blocks/(1024.0*1024.0);
    }
    var freeG = stat.f_frsize/1024.0*stat.f_bavail/(1024.0*1024.0);
    if (freeG < freeSpaceG) {
      doit = true;
    }

    if (freeSpaceG > minFreeSpaceG) {
      minFreeSpaceG = freeSpaceG;
    }

    cb();
  }, function () {
    if (doit) {
      expireDevice(nodes, stat.dirs, minFreeSpaceG, nextCb);
    } else {
      return nextCb();
    }
  });
}

function expireCheckAll () {
  var devToStat = {};
  // Find all the nodes running on this host
  Db.hostnameToNodeids(os.hostname(), function(nodes) {
    // Find all the pcap dirs for local nodes
    async.map(nodes, function (node, cb) {
      var pcapDirs = Config.getFull(node, "pcapDir");
      if (typeof pcapDirs !== "string") {
        return cb("ERROR - couldn't find pcapDir setting for node: " + node + "\nIf you have it set try running:\nnpm remove iniparser; npm cache clean; npm update iniparser");
      }
      // Create a mapping from device id to stat information and all directories on that device
      pcapDirs.split(";").forEach(function (pcapDir) {
        if (!pcapDir) {
          return; // Skip empty elements.  Prevents errors when pcapDir has a trailing or double ;
        }
        pcapDir = pcapDir.trim();
        var fileStat = fs.statSync(pcapDir);
        var vfsStat = fs.statVFS(pcapDir);
        if (!devToStat[fileStat.dev]) {
          vfsStat.dirs = {};
          vfsStat.dirs[pcapDir] = {};
          devToStat[fileStat.dev] = vfsStat;
        } else {
          devToStat[fileStat.dev].dirs[pcapDir] = {};
        }
      });
      cb(null);
    },
    function (err) {
      // Now gow through all the local devices and check them
      var keys = Object.keys(devToStat);
      async.forEachSeries(keys, function (key, cb) {
        expireCheckDevice(nodes, devToStat[key], cb);
      }, function (err) {
      });
    });
  });
}
//////////////////////////////////////////////////////////////////////////////////
//// Sessions Query
//////////////////////////////////////////////////////////////////////////////////
function addSortToQuery(query, info, d, missing) {

  function addSortDefault() {
    if (d) {
      if (!query.sort) {
        query.sort = [];
      }
      var obj = {};
      obj[d] = {order: "asc"};
      if (missing && missing[d] !== undefined) {
        obj[d].missing = missing[d];
      }
      query.sort.push(obj);
    }
  }

  if (!info) {
    addSortDefault();
    return;
  }

  // New Method
  if (info.order) {
    if (info.order.length === 0) {
      addSortDefault();
      return;
    }

    if (!query.sort) {
      query.sort = [];
    }

    info.order.split(",").forEach(function(item) {
      var parts = item.split(":");
      var field = parts[0];

      var obj = {};
      if (field === "fp") {
        obj.fpd = {order: parts[1]};
      } else if (field === "lp") {
        obj.lpd = {order: parts[1]};
      } else {
        obj[field] = {order: parts[1]};
      }

      if (missing && missing[field] !== undefined) {
        obj[field].missing = missing[field];
      }
      query.sort.push(obj);
    });
    return;
  }

  // Old Method
  if (!info.iSortingCols || parseInt(info.iSortingCols, 10) === 0) {
    addSortDefault();
    return;
  }

  if (!query.sort) {
    query.sort = [];
  }

  for (var i = 0, ilen = parseInt(info.iSortingCols, 10); i < ilen; i++) {
    if (!info["iSortCol_" + i] || !info["sSortDir_" + i] || !info["mDataProp_" + info["iSortCol_" + i]]) {
      continue;
    }

    var obj = {};
    var field = info["mDataProp_" + info["iSortCol_" + i]];
    obj[field] = {order: info["sSortDir_" + i]};
    if (missing && missing[field] !== undefined) {
      obj[field].missing = missing[field];
    }
    query.sort.push(obj);

    if (field === "fp") {
      query.sort.push({fpd: {order: info["sSortDir_" + i]}});
    } else if (field === "lp") {
      query.sort.push({lpd: {order: info["sSortDir_" + i]}});
    }
  }
}

/* This method fixes up parts of the query that jison builds to what ES actually
 * understands.  This includes mapping all the tag fields from strings to numbers
 * and any of the filename stuff
 */
function lookupQueryItems(query, doneCb) {
  if (Config.get("multiES", false)) {
    return doneCb(null);
  }

  var outstanding = 0;
  var finished = 0;
  var err = null;

  function process(parent, obj, item) {
    //console.log("\nprocess:\n", item, obj, typeof obj[item], "\n");
    if ((item === "ta" || item === "hh" || item === "hh1" || item === "hh2") && (typeof obj[item] === "string" || Array.isArray(obj[item]))) {
      if (obj[item].indexOf("*") !== -1) {
        delete parent.wildcard;
        outstanding++;
        var query;
        if (item === "ta") {
          query = {bool: {must: {wildcard: {_uid: "tag#" + obj[item]}},
                          must_not: {wildcard: {_uid: "tag#" + "http:header:*"}}
                         }
                  };
        } else {
          query = {wildcard: {_uid: "tag#http:header:" + obj[item].toLowerCase()}};
        }
        Db.search('tags', 'tag', {size:500, _source:["id", "n"], query: query}, function(err, result) {
          var terms = [];
          result.hits.hits.forEach(function (hit) {
            var fields = hit._source || hit.fields;
            terms.push(fields.n);
          });
          parent.terms = {};
          parent.terms[item] = terms;
          outstanding--;
          if (finished && outstanding === 0) {
            doneCb(err);
          }
        });
      } else if (Array.isArray(obj[item])) {
        outstanding++;

        async.map(obj[item], function(str, cb) {
          var tag = (item !== "ta"?"http:header:" + str.toLowerCase():str);
          Db.tagNameToId(tag, function (id) {
            if (id === null) {
              console.log("Tag '" + tag + "' not found");
              cb(null, -1);
            } else {
              cb(null, id);
            }
          });
        },
        function (err, results) {
          outstanding--;
          obj[item] = results;
          if (finished && outstanding === 0) {
            doneCb(err);
          }
        });
      } else {
        outstanding++;
        var tag = (item !== "ta"?"http:header:" + obj[item].toLowerCase():obj[item]);

        Db.tagNameToId(tag, function (id) {
          outstanding--;
          if (id === null) {
            err = "Tag '" + tag + "' not found";
          } else {
            obj[item] = id;
          }
          if (finished && outstanding === 0) {
            doneCb(err);
          }
        });
      }
    } else if (item === "fileand" && typeof obj[item] === "string") {
      var name = obj.fileand;
      delete obj.fileand;
      outstanding++;
      Db.fileNameToFiles(name, function (files) {
        outstanding--;
        if (files === null || files.length === 0) {
          err = "File '" + name + "' not found";
        } else if (files.length > 1) {
          obj.bool = {should: []};
          files.forEach(function(file) {
            obj.bool.should.push({bool: {must: [{term: {no: file.node}}, {term: {fs: file.num}}]}});
          });
        } else {
          obj.bool = {must: [{term: {no: files[0].node}}, {term: {fs: files[0].num}}]};
        }
        if (finished && outstanding === 0) {
          doneCb(err);
        }
      });
    } else if (typeof obj[item] === "object") {
      convert(obj, obj[item]);
    }
  }

  function convert(parent, obj) {
    for (var item in obj) {
      process(parent, obj, item);
    }
  }

  convert(null, query);
  if (outstanding === 0) {
    return doneCb(err);
  }

  finished = 1;
}

function buildSessionQuery(req, buildCb) {
  var limit = Math.min(2000000, +req.query.length || +req.query.iDisplayLength || 100);
  var i;


  var query = {from: req.query.start || req.query.iDisplayStart || 0,
               size: limit,
               query: {bool: {filter: []}}
              };

  if (req.query.strictly === "true") {
    req.query.bounding = "both";
  }

  var interval;
  if ((req.query.date && req.query.date === '-1') ||
      (req.query.segments && req.query.segments === "all")) {
    interval = 60*60; // Hour to be safe
  } else if (req.query.startTime && req.query.stopTime) {
    if (! /^[0-9]+$/.test(req.query.startTime)) {
      req.query.startTime = Date.parse(req.query.startTime.replace("+", " "))/1000;
    } else {
      req.query.startTime = parseInt(req.query.startTime, 10);
    }

    if (! /^[0-9]+$/.test(req.query.stopTime)) {
      req.query.stopTime = Date.parse(req.query.stopTime.replace("+", " "))/1000;
    } else {
      req.query.stopTime = parseInt(req.query.stopTime, 10);
    }

    switch (req.query.bounding) {
    case "first":
      query.query.bool.filter.push({range: {fp: {gte: req.query.startTime, lte: req.query.stopTime}}});
      break;
    case "last":
    default:
      query.query.bool.filter.push({range: {lp: {gte: req.query.startTime, lte: req.query.stopTime}}});
      break;
    case "both":
      query.query.bool.filter.push({range: {fp: {gte: req.query.startTime}}});
      query.query.bool.filter.push({range: {lp: {lte: req.query.stopTime}}});
      break;
    case "either":
      query.query.bool.filter.push({range: {fp: {lte: req.query.stopTime}}});
      query.query.bool.filter.push({range: {lp: {gte: req.query.startTime}}});
      break;
    case "database":
      query.query.bool.filter.push({range: {timestamp: {gte: req.query.startTime*1000, lte: req.query.stopTime*1000}}});
      break;
    }

    var diff = req.query.stopTime - req.query.startTime;
    if (diff < 30*60) {
      interval = 1; // second
    } else if (diff <= 5*24*60*60) {
      interval = 60; // minute
    } else {
      interval = 60*60; // hour
    }
  } else {
    if (!req.query.date) {
      req.query.date = 1;
    }
    req.query.startTime = (Math.floor(Date.now() / 1000) - 60*60*parseInt(req.query.date, 10));
    req.query.stopTime = Date.now()/1000;

    switch (req.query.bounding) {
    case "first":
      query.query.bool.filter.push({range: {fp: {gte: req.query.startTime}}});
      break;
    case "both":
    case "last":
    default:
      query.query.bool.filter.push({range: {lp: {gte: req.query.startTime}}});
      break;
    case "either":
      query.query.bool.filter.push({range: {fp: {lte: req.query.stopTime}}});
      query.query.bool.filter.push({range: {lp: {gte: req.query.startTime}}});
      break;
    case "database":
      query.query.bool.filter.push({range: {timestamp: {gte: req.query.startTime*1000}}});
      break;
    }

    if (req.query.date <= 5*24) {
      interval = 60; // minute
    } else {
      interval = 60*60; // hour
    }
  }

  if (req.query.facets) {
    query.aggregations = {mapG1: {terms: {field: "g1", size:1000, min_doc_count:1}},
                          mapG2: {terms: {field: "g2", size:1000, min_doc_count:1}}};
    query.aggregations.dbHisto = {aggregations: {db1: {sum: {field:"db1"}}, db2: {sum: {field:"db2"}}, pa1: {sum: {field:"pa1"}}, pa2: {sum: {field:"pa2"}}}};

    switch (req.query.bounding) {
    case "first":
       query.aggregations.dbHisto.histogram = { field:'fp', interval:interval, min_doc_count:1 };
      break;
    case "database":
      query.aggregations.dbHisto.histogram = { field:'timestamp', interval:interval*1000, min_doc_count:1 };
      break;
    default:
      query.aggregations.dbHisto.histogram = { field:'lp', interval:interval, min_doc_count:1 };
      break;
    }
  }

  addSortToQuery(query, req.query, "fp");

  var err = null;
  molochparser.parser.yy = {emailSearch: req.user.emailSearch === true,
                                  views: req.user.views,
                              fieldsMap: Config.getFieldsMap()};
  if (req.query.expression) {
    //req.query.expression = req.query.expression.replace(/\\/g, "\\\\");
    try {
      query.query.bool.filter.push(molochparser.parse(req.query.expression));
    } catch (e) {
      err = e;
    }
  }

  if (!err && req.query.view && req.user.views && req.user.views[req.query.view]) {
    try {
      var viewExpression = molochparser.parse(req.user.views[req.query.view].expression);
      query.query.bool.filter.push(viewExpression);
    } catch (e) {
      console.log("ERR - User expression doesn't compile", req.user.views[req.query.view], e);
      err = e;
    }
  }

  if (!err && req.user.expression && req.user.expression.length > 0) {
    try {
      // Expression was set by admin, so assume email search ok
      molochparser.parser.yy.emailSearch = true;
      var userExpression = molochparser.parse(req.user.expression);
      query.query.bool.filter.push(userExpression);
    } catch (e) {
      console.log("ERR - Forced expression doesn't compile", req.user.expression, e);
      err = e;
    }
  }

  lookupQueryItems(query.query.bool.filter, function (lerr) {
    if (req.query.date && req.query.date === '-1') {
      return buildCb(err || lerr, query, "sessions-*");
    }

    Db.getIndices(req.query.startTime, req.query.stopTime, Config.get("rotateIndex", "daily"), function(indices) {
      return buildCb(err || lerr, query, indices);
    });
  });
}
//////////////////////////////////////////////////////////////////////////////////
//// Sessions List
//////////////////////////////////////////////////////////////////////////////////
function sessionsListAddSegments(req, indices, query, list, cb) {
  var processedRo = {};

  // Index all the ids we have, so we don't include them again
  var haveIds = {};
  list.forEach(function(item) {
    haveIds[item._id] = true;
  });

  delete query.aggregations;

  // Do a ro search on each item
  var writes = 0;
  async.eachLimit(list, 10, function(item, nextCb) {
    var fields = item._source || item.fields;
    if (!fields.ro || processedRo[fields.ro]) {
      if (writes++ > 100) {
        writes = 0;
        setImmediate(nextCb);
      } else {
        nextCb();
      }
      return;
    }
    processedRo[fields.ro] = true;

    query.query.bool.filter.push({term: {ro: fields.ro}});

    Db.searchPrimary(indices, 'session', query, function(err, result) {
      if (err || result === undefined || result.hits === undefined || result.hits.hits === undefined) {
        console.log("ERROR fetching matching sessions", err, result);
        return nextCb(null);
      }
      result.hits.hits.forEach(function(item) {
        if (!haveIds[item._id]) {
          haveIds[item._id] = true;
          list.push(item);
        }
      });
      return nextCb(null);
    });
  }, function (err) {
    cb(err, list);
  });
}

function sessionsListFromQuery(req, res, fields, cb) {
  if (req.query.segments && fields.indexOf("ro") === -1) {
    fields.push("ro");
  }

  buildSessionQuery(req, function(err, query, indices) {
    query._source = fields;
    Db.searchPrimary(indices, 'session', query, function(err, result) {
      if (err || result.error) {
          console.log("ERROR - Could not fetch list of sessions.  Err: ", err,  " Result: ", result, "query:", query);
          return res.send("Could not fetch list of sessions.  Err: " + err + " Result: " + result);
      }
      var list = result.hits.hits;
      if (req.query.segments) {
        sessionsListAddSegments(req, indices, query, list, function(err, list) {
          cb(err, list);
        });
      } else {
        cb(err, list);
      }
    });
  });
}

function sessionsListFromIds(req, ids, fields, cb) {
  var list = [];
  var nonArrayFields = ["pr", "fp", "lp", "a1", "p1", "g1", "a2", "p2", "g2", "by", "db", "pa", "no", "ro", "tipv61-term", "tipv62-term"];
  var fixFields = nonArrayFields.filter(function(x) {return fields.indexOf(x) !== -1;});

  // ES treats _source=no as turning off _source, very sad :(
  if (fields.length === 1 && fields[0] === "no") {
    fields.push("lp");
  }

  async.eachLimit(ids, 10, function(id, nextCb) {
    Db.getWithOptions(Db.id2Index(id), 'session', id, {_source: fields.join(",")}, function(err, session) {
      if (err) {
        return nextCb(null);
      }

      for (var i = 0; i < fixFields.length; i++) {
        var field = fixFields[i];
        if (session._source[field] && Array.isArray(session._source[field])) {
          session._source[field] = session._source[field][0];
        }
      }

      list.push(session);
      nextCb(null);
    });
  }, function(err) {
    if (req && req.query.segments) {
      buildSessionQuery(req, function(err, query, indices) {
        query._source = fields;
        sessionsListAddSegments(req, indices, query, list, function(err, list) {
          cb(err, list);
        });
      });
    } else {
      cb(err, list);
    }
  });
}

//////////////////////////////////////////////////////////////////////////////////
//// APIs
//////////////////////////////////////////////////////////////////////////////////
app.get('/fields', function(req, res) {
  if (!app.locals.fieldsMap) {
    res.status(404);
    res.send('Cannot locate fields');
  }

  if (req.query && req.query.array) {
    res.send(app.locals.fieldsArr)
  } else {
    res.send(app.locals.fieldsMap);
  }
});

app.get('/file/list', function(req, res) {
  var columns = ["num", "node", "name", "locked", "first", "filesize"];

  var query = {_source: columns,
               from: +req.query.start || 0,
               size: +req.query.length || 10,
               sort: {}

              };

  query.sort[req.query.sortField || "num"] = { order: req.query.desc === "true" ? "desc": "asc"};

  if (req.query.filter) {
    query.query = {wildcard: {name: "*" + req.query.filter + "*"}};
  }

  async.parallel({
    files: function (cb) {
      Db.search('files', 'file', query, function(err, result) {
        var results = {total: result.hits.total, results: []};
        if (err || result.error) {
          return cb(err || result.error);
        }

        for (var i = 0, ilen = result.hits.hits.length; i < ilen; i++) {
          var fields = result.hits.hits[i]._source || result.hits.hits[i].fields;
          if (fields.locked === undefined) {
            fields.locked = 0;
          }
          fields.id = result.hits.hits[i]._id;
          results.results.push(fields);
        }
        cb(null, results);
      });
    },
    total: function (cb) {
      Db.numberOfDocuments('files', cb);
    }
  },
  function(err, results) {
    if (err) {
      return res.send({recordsTotal: 0, recordsFiltered: 0, data: []});
    }

    var r = {recordsTotal: results.total,
             recordsFiltered: results.files.total,
             data: results.files.results};
    res.send(r);
  });
});

app.get('/titleconfig', checkWebEnabled, function(req, res) {
  var titleConfig = Config.get('titleTemplate', '_cluster_ - _page_ _-view_ _-expression_');

  titleConfig = titleConfig.replace(/_cluster_/g, internals.clusterName)
    .replace(/_userId_/g, req.user?req.user.userId:"-")
    .replace(/_userName_/g, req.user?req.user.userName:"-");

  res.send(titleConfig);
});

app.get('/molochRightClick', checkWebEnabled, function(req, res) {
  if(!app.locals.molochRightClick) {
    res.status(404);
    res.send('Cannot locate right clicks');
  }
  res.send(app.locals.molochRightClick);
});

app.get('/eshealth.json', function(req, res) {
  Db.healthCache(function(err, health) {
    res.send(health);
  });
});

app.get('/esstats.json', function(req, res) {
  var stats = [];
  var r;

  async.parallel({
    nodes: function(nodesCb) {
      Db.nodesStats({metric: "jvm,process,fs,os,indices"}, nodesCb);
    },
    health: Db.healthCache
  },
  function(err, results) {
    if (err || !results.nodes) {
      console.log ("ERROR", err);
      r = {draw: req.query.draw,
           health: results.health,
           recordsTotal: 0,
           recordsFiltered: 0,
           data: []};
      return res.send(r);
    }

    var now = new Date().getTime();
    while (internals.previousNodeStats.length > 1 && internals.previousNodeStats[1].timestamp + 10000 < now) {
      internals.previousNodeStats.shift();
    }

    var regex;
    if (req.query.filter !== undefined) {
      regex = new RegExp(req.query.filter);
    }


    var nodes = Object.keys(results.nodes.nodes);
    for (var n = 0, nlen = nodes.length; n < nlen; n++) {
      var node = results.nodes.nodes[nodes[n]];

      if (regex && !node.name.match(regex)) {continue;}

      var read = 0;
      var write = 0;

      var oldnode = internals.previousNodeStats[0][nodes[n]];
      if (node.fs.io_stats !== undefined && oldnode.fs.io_stats !== undefined) {
        var timediffsec = (node.timestamp - oldnode.timestamp)/1000.0;
        read = Math.ceil((node.fs.io_stats.total.read_kilobytes - oldnode.fs.io_stats.total.read_kilobytes)/timediffsec*1024);
        write = Math.ceil((node.fs.io_stats.total.write_kilobytes - oldnode.fs.io_stats.total.write_kilobytes)/timediffsec*1024);
      }

      stats.push({
        name: node.name,
        storeSize: node.indices.store.size_in_bytes,
        docs: node.indices.docs.count,
        searches: node.indices.search.query_current,
        searchesTime: node.indices.search.query_time_in_millis,
        heapSize: node.jvm.mem.heap_used_in_bytes,
        nonHeapSize: node.jvm.mem.non_heap_used_in_bytes,
        cpu: node.process.cpu.percent,
        read: read,
        write: write,
        load: node.os.load_average !== undefined ? /* ES 2*/ node.os.load_average : /*ES 5*/ node.os.cpu.load_average["5m"]
      });
    }

    if (req.query.sortField) {
      if (req.query.sortField === "nodeName") {
        if (req.query.desc === "true")
          stats = stats.sort(function(a,b){ return b.name.localeCompare(a.name); })
        else
          stats = stats.sort(function(a,b){ return a.name.localeCompare(b.name); })
      } else {
        var field = req.query.sortField;
        if (req.query.desc === "true")
          stats = stats.sort(function(a,b){ return b[field] - a[field]; })
        else
          stats = stats.sort(function(a,b){ return a[field] - b[field]; })
      }
    }

    results.nodes.nodes.timestamp = new Date().getTime();
    internals.previousNodeStats.push(results.nodes.nodes);

    r = {draw: req.query.draw,
         health: results.health,
         recordsTotal: stats.length,
         recordsFiltered: stats.length,
         data: stats};
    res.send(r);
  });
});

function mergeUnarray(to, from) {
  for (var key in from) {
    if (Array.isArray(from[key])) {
      to[key] = from[key][0];
    } else {
      to[key] = from[key];
    }
  }
}
app.get('/stats.json', function(req, res) {
  noCache(req, res);

  var query = {from: +req.query.start || 0,
               size: Math.min(10000, +req.query.length || 500)
              };

  if (req.query.filter !== undefined && req.query.filter !== '') {
    let names = req.query.filter.split(',');
    query.query = { bool: { should: [] } };
    for (let i = 0, len = names.length; i < len; ++i) {
      let name = names[i].trim();
      if (name !== '') {
        query.query.bool.should.push({
          wildcard: {nodeName: '*' + name + '*'}
        });
      }
    }
  }

  if (req.query.sortField !== undefined || req.query.desc !== undefined) {
    query.sort = {};
    req.query.sortField = req.query.sortField || "nodeName";
    query.sort[req.query.sortField] = { order: req.query.desc === "true" ? "desc": "asc"};
    query.sort[req.query.sortField].missing = internals.usersMissing[req.query.sortField];
  } else {
    addSortToQuery(query, req.query, "_uid");
  }

  async.parallel({
    stats: function (cb) {
      Db.search('stats', 'stat', query, function(err, result) {
        if (err || result.error) {
          console.log("ERROR - stats", query, err || result.error);
          res.send({total: 0, results: []});
        } else {
          var results = {total: result.hits.total, results: []};
          for (var i = 0, ilen = result.hits.hits.length; i < ilen; i++) {
            var fields = result.hits.hits[i]._source || result.hits.hits[i].fields;
            if (result.hits.hits[i]._source) {
              mergeUnarray(fields, result.hits.hits[i].fields);
            }
            fields.id        = result.hits.hits[i]._id;

            ["totalPackets", "totalK", "totalSessions",
             "monitoring", "tcpSessions", "udpSessions", "icmpSessions",
             "freeSpaceM", "freeSpaceP", "memory", "memoryP", "frags", "cpu",
             "diskQueue", "esQueue", "packetQueue", "closeQueue", "needSave", "fragsQueue",
             "deltaFragsDropped", "deltaOverloadDropped", "deltaESDropped"
            ].forEach(function(key) {
              fields[key] = fields[key] || 0;
            });

            fields.deltaBytesPerSec           = Math.floor(fields.deltaBytes * 1000.0/fields.deltaMS);
            fields.deltaBitsPerSec            = Math.floor(fields.deltaBytes * 1000.0/fields.deltaMS * 8);
            fields.deltaPacketsPerSec         = Math.floor(fields.deltaPackets * 1000.0/fields.deltaMS);
            fields.deltaSessionsPerSec        = Math.floor(fields.deltaSessions * 1000.0/fields.deltaMS);
            fields.deltaDroppedPerSec         = Math.floor(fields.deltaDropped * 1000.0/fields.deltaMS);
            fields.deltaFragsDroppedPerSec    = Math.floor(fields.deltaFragsDropped * 1000.0/fields.deltaMS);
            fields.deltaOverloadDroppedPerSec = Math.floor(fields.deltaOverloadDropped * 1000.0/fields.deltaMS);
            fields.deltaESDroppedPerSec       = Math.floor(fields.deltaESDropped * 1000.0/fields.deltaMS);
            fields.deltaTotalDroppedPerSec    = Math.floor((fields.deltaDropped + fields.deltaOverloadDropped) * 1000.0/fields.deltaMS);
            results.results.push(fields);
          }
          cb(null, results);
        }
      });
    },
    total: function (cb) {
      Db.numberOfDocuments('stats', cb);
    }
  },
  function(err, results) {
    var r = {draw: req.query.draw,
             recordsTotal: results.total,
             recordsFiltered: results.stats.total,
             data: results.stats.results};
    res.send(r);
  });
});

app.get('/dstats.json', function(req, res) {
  noCache(req, res);

  var nodeName = req.query.nodeName;

  var query = {
               query: {
                 bool: {
                   filter: [
                     {
                       range: { currentTime: { from: req.query.start, to: req.query.stop } }
                     },
                     {
                       term: { interval: req.query.interval || 60}
                     }
                   ]
                 }
               }
              };

  if (nodeName !== undefined && nodeName !== 'Total' && nodeName !== 'Average') {
    query.sort = {currentTime: {order: 'desc' }};
    query.size = req.query.size || 1440;
    query.query.bool.filter.push({term: { nodeName: nodeName}});
  } else {
    query.size = 100000;
  }

  var mapping = {
    deltaBits: {_source: ["deltaBytes"], func: function (item) {return Math.floor(item.deltaBytes * 8.0);}},
    deltaTotalDropped: {_source: ["deltaDropped", "deltaOverloadDropped"], func: function (item) {return Math.floor(item.deltaDropped + item.deltaOverloadDropped);}},
    deltaBytesPerSec: {_source: ["deltaBytes", "deltaMS"], func: function(item) {return Math.floor(item.deltaBytes * 1000.0/item.deltaMS);}},
    deltaBitsPerSec: {_source: ["deltaBytes", "deltaMS"], func: function(item) {return Math.floor(item.deltaBytes * 1000.0/item.deltaMS * 8);}},
    deltaPacketsPerSec: {_source: ["deltaPackets", "deltaMS"], func: function(item) {return Math.floor(item.deltaPackets * 1000.0/item.deltaMS);}},
    deltaSessionsPerSec: {_source: ["deltaSessions", "deltaMS"], func: function(item) {return Math.floor(item.deltaSessions * 1000.0/item.deltaMS);}},
    deltaDroppedPerSec: {_source: ["deltaDropped", "deltaMS"], func: function(item) {return Math.floor(item.deltaDropped * 1000.0/item.deltaMS);}},
    deltaFragsDroppedPerSec: {_source: ["deltaFragsDropped", "deltaMS"], func: function(item) {return Math.floor(item.deltaFragsDropped * 1000.0/item.deltaMS);}},
    deltaOverloadDroppedPerSec: {_source: ["deltaOverloadDropped", "deltaMS"], func: function(item) {return Math.floor(item.deltaOverloadDropped * 1000.0/item.deltaMS);}},
    deltaESDroppedPerSec: {_source: ["deltaESDropped", "deltaMS"], func: function(item) {return Math.floor(item.deltaESDropped * 1000.0/item.deltaMS);}},
    deltaTotalDroppedPerSec: {_source: ["deltaDropped", "deltaOverloadDropped", "deltaMS"], func: function(item) {return Math.floor((item.deltaDropped + item.deltaOverloadDropped) * 1000.0/item.deltaMS);}},
    cpu: {_source: ["cpu"], func: function (item) {return item.cpu * .01;}}
  };

  query._source = mapping[req.query.name]?mapping[req.query.name]._source:[req.query.name];
  query._source.push("nodeName", "currentTime");

  var func = mapping[req.query.name]?mapping[req.query.name].func:function(item) {return item[req.query.name]};

  Db.searchScroll('dstats', 'dstat', query, {filter_path: "_scroll_id,hits.total,hits.hits._source"}, function(err, result) {
    if (err || result.error) {
      console.log("ERROR - dstats", query, err || result.error);
    }
    var i, ilen;
    var data = {};
    var num = (req.query.stop - req.query.start)/req.query.step;

    var mult = 1;
    if (req.query.name === "freeSpaceM") {
      mult = 1000000;
    }

    //console.log("dstats.json result", util.inspect(result, false, 50));

    if (result && result.hits && result.hits.hits) {
      for (i = 0, ilen = result.hits.hits.length; i < ilen; i++) {
        var fields = result.hits.hits[i]._source;
        var pos = Math.floor((fields.currentTime - req.query.start)/req.query.step);

        if (data[fields.nodeName] === undefined) {
          data[fields.nodeName] = arrayZeroFill(num);
        }
        data[fields.nodeName][pos] = mult * func(fields);
      }
    }
    if (nodeName === undefined) {
      res.send(data);
    } else {
      if (data[nodeName] === undefined) {
        data[nodeName] = arrayZeroFill(num);
      }
      if (nodeName === 'Total' || nodeName === 'Average') {
        delete data[nodeName];
        var data2 = arrayZeroFill(num);
        var cnt = 0;
        for (var key in data) {
          for (i = 0; i < num; i++) {
            data2[i] += data[key][i];
          }
          cnt++;
        }
        if (nodeName === 'Average') {
          for (i = 0; i < num; i++) {
            data2[i] /= cnt;
          }
        }
        res.send(data2);
      } else {
        res.send(data[req.query.nodeName]);
      }
    }
  });
});

app.get('/:nodeName/:fileNum/filesize.json', function(req, res) {
  Db.fileIdToFile(req.params.nodeName, req.params.fileNum, function(file) {
    if (!file) {
      return res.send({filesize: -1});
    }

    fs.stat(file.name, function (err, stats) {
      if (err || !stats) {
        return res.send({filesize: -1});
      } else {
        return res.send({filesize: stats.size});
      }
    });
  });
});

function mapMerge(aggregations) {
  var map = {src: {}, dst: {}};
  if (!aggregations || !aggregations.mapG1) {
    return {};
  }

  aggregations.mapG1.buckets.forEach(function (item) {
    map.src[item.key] = item.doc_count;
  });

  aggregations.mapG2.buckets.forEach(function (item) {
    map.dst[item.key] = item.doc_count;
  });

  return map;
}

function graphMerge(req, query, aggregations) {
  var graph = {
    lpHisto: [],
    db1Histo: [],
    db2Histo: [],
    pa1Histo: [],
    pa2Histo: [],
    xmin: req.query.startTime * 1000|| null,
    xmax: req.query.stopTime * 1000 || null,
    interval: query.aggregations?query.aggregations.dbHisto.histogram.interval || 60 : 60
  };

  if (!aggregations || !aggregations.dbHisto) {
    return graph;
  }

  if (req.query.bounding === "database") {
    graph.interval = query.aggregations?(query.aggregations.dbHisto.histogram.interval/1000) || 60 : 60;
    aggregations.dbHisto.buckets.forEach(function (item) {
      var key = item.key;
      graph.lpHisto.push([key, item.doc_count]);
      graph.pa1Histo.push([key, item.pa1.value]);
      graph.pa2Histo.push([key, item.pa2.value]);
      graph.db1Histo.push([key, item.db1.value]);
      graph.db2Histo.push([key, item.db2.value]);
    });
  } else {
    aggregations.dbHisto.buckets.forEach(function (item) {
      var key = item.key*1000;
      graph.lpHisto.push([key, item.doc_count]);
      graph.pa1Histo.push([key, item.pa1.value]);
      graph.pa2Histo.push([key, item.pa2.value]);
      graph.db1Histo.push([key, item.db1.value]);
      graph.db2Histo.push([key, item.db2.value]);
    });
  }
  return graph;
}

function fixTagsField(container, field, doneCb, offset) {
  if (container[field] === undefined) {
    return doneCb(null);
  }
  async.map(container[field], function (item, cb) {
    Db.tagIdToName(item, function (name) {
      cb(null, name.substring(offset));
    });
  },
  function(err, results) {
    container[field] = results;
    doneCb(err);
  });
}

function fixTagBucketsField(container, field, doneCb, offset) {
  if (container[field] === undefined) {
    return doneCb(null);
  }
  async.map(container[field].buckets, function (item, cb) {
    Db.tagIdToName(item.key, function (name) {
      item.key = name.substring(offset);
      cb(null, item);
    });
  },
  function(err, results) {
    container[field].buckets = results;
    doneCb(err);
  });
}

function fixFields(fields, fixCb) {
  async.parallel([
    function(parallelCb) {
      fixTagsField(fields, "ta", parallelCb, 0);
    },
    function(parallelCb) {
      fixTagsField(fields, "hh", parallelCb, 12);
    },
    function(parallelCb) {
      fixTagsField(fields, "hh1", parallelCb, 12);
    },
    function(parallelCb) {
      fixTagsField(fields, "hh2", parallelCb, 12);
    },
    function(parallelCb) {
      var files = [];
      if (!fields.fs) {
        fields.fs = [];
        return parallelCb(null);
      }
      async.forEachSeries(fields.fs, function (item, cb) {
        Db.fileIdToFile(fields.no, item, function (file) {
          if (file && file.locked === 1) {
            files.push(file.name);
          }
          cb(null);
        });
      },
      function(err) {
        fields.fs = files;
        parallelCb(err);
      });
    }],
    function(err, results) {
      fixCb(err, fields);
    }
  );
}

/**
 * Flattens fields that are objects (only goes 1 level deep)
 *
 * @example
 * { http: { statuscode: [200, 302] } } => { "http.statuscode": [200, 302] }
 *
 * @param {object} fields The object containing fields to be flattened
 * @returns {object} fields The object with fields flattened
 */
function flattenFields(fields) {
  var newFields = {};

  for (var key in fields) {
    if (fields.hasOwnProperty(key)) {
      var field = fields[key];
      if (typeof field === 'object' && !field.length) {
        var baseKey = key + '.';
        for (var nestedKey in field) {
          if (field.hasOwnProperty(nestedKey)) {
            var nestedField = field[nestedKey];
            var newKey = baseKey + nestedKey;
            newFields[newKey] = nestedField;
          }
        }
        fields[key] = null;
        delete fields[key];
      }
    }
  }

  for (var key in newFields) {
    if (newFields.hasOwnProperty(key)) {
      fields[key] = newFields[key];
    }
  }

  return fields;
}

app.get('/sessions.json', function(req, res) {
  var i;

  var graph = {};
  var map = {};
  buildSessionQuery(req, function(bsqErr, query, indices) {
    if (bsqErr) {
      var r = {draw: req.query.draw,
               recordsTotal: 0,
               recordsFiltered: 0,
               graph: graph,
               map: map,
               bsqErr: bsqErr.toString(),
               health: Db.healthCache(),
               data:[]};
      res.send(r);
      return;
    }
    var addMissing = false;
    if (req.query.fields) {
      query._source = queryValueToArray(req.query.fields);
      ["no", "a1", "p1", "a2", "p2"].forEach(function(item) {
        if (query._source.indexOf(item) === -1) {
          query._source.push(item);
        }
      });
    } else {
      addMissing = true;
      query._source = ["pr", "ro", "db", "db1", "db2", "fp", "lp", "a1", "p1", "a2", "p2", "pa", "pa1", "pa2", "by", "by1", "by2", "no", "us", "g1", "g2", "esub", "esrc", "edst", "efn", "dnsho", "tls", "ircch", "tipv61-term", "tipv62-term"];
    }

    if (query.aggregations && query.aggregations.dbHisto) {
      graph.interval = query.aggregations.dbHisto.histogram.interval;
    }

    console.log("sessions.json query", JSON.stringify(query));

    async.parallel({
      sessions: function (sessionsCb) {
        Db.searchPrimary(indices, 'session', query, function(err, result) {
          if (Config.debug) {
            console.log("sessions.json result", util.inspect(result, false, 50));
          }
          if (err || result.error) {
            console.log("sessions.json error", err, (result?result.error:null));
            sessionsCb(null, {total: 0, results: []});
            return;
          }

          graph = graphMerge(req, query, result.aggregations);
          map = mapMerge(result.aggregations);

          var results = {total: result.hits.total, results: []};
          async.each(result.hits.hits, function (hit, hitCb) {
            var fields = hit._source || hit.fields;
            if (fields === undefined) {
              return hitCb(null);
            }
            fields.index = hit._index;
            fields.id = hit._id;

            if (req.query.flatten === '1') {
              fields = flattenFields(fields);
            }

            if (addMissing) {
              ["pa1", "pa2", "by1", "by2", "db1", "db2"].forEach(function(item) {
                if (fields[item] === undefined) {
                  fields[item] = -1;
                }
              });
              results.results.push(fields);
              return hitCb();
            } else {
              fixFields(fields, function() {
                results.results.push(fields);
                return hitCb();
              });
            }
          }, function () {
            sessionsCb(null, results);
          });
        });
      },
      total: function (totalCb) {
        Db.numberOfDocuments('sessions-*', totalCb);
      },
      health: Db.healthCache
    },
    function(err, results) {
      var r = {draw: req.query.draw,
               recordsTotal: results.total,
               recordsFiltered: (results.sessions?results.sessions.total:0),
               graph: graph,
               health: results.health,
               map: map,
               data: (results.sessions?results.sessions.results:[])};
      try {
        res.send(r);
      } catch (c) {
      }
    });
  });
});

app.get('/spigraph.json', function(req, res) {
  function error(text) {
    res.status(403);
    return res.send(JSON.stringify({success: false, text: text}));
  }

  req.query.facets = 1;
  buildSessionQuery(req, function(bsqErr, query, indices) {
    var results = {items: [], graph: {}, map: {}, iTotalRecords: 0};
    if (bsqErr) {
      return error(bsqErr.toString());
    }

    delete query.sort;
    query.size = 0;
    var size = +req.query.size || 20;

    var field = req.query.field || "no";
    query.aggregations.field = {terms: {field: field, size: size}};

    /* Need the setImmediate so we don't blow max stack frames */
    var eachCb;
    switch (fmenum(field)) {
    case FMEnum.other:
      eachCb = function (item, cb) {setImmediate(cb);};
      break;
    case FMEnum.ip:
      eachCb = function(item, cb) {
        item.name = Pcap.inet_ntoa(item.name);
        setImmediate(cb);
      };
      break;
    case FMEnum.tags:
      eachCb = function(item, cb) {
        Db.tagIdToName(item.name, function (name) {
          item.name = name;
          setImmediate(cb);
        });
      };
      break;
    case FMEnum.hh:
      eachCb = function(item, cb) {
        Db.tagIdToName(item.name, function (name) {
          item.name = name.substring(12);
          setImmediate(cb);
        });
      };
      break;
    }

    Db.healthCache(function(err, health) {results.health = health;});
    Db.numberOfDocuments('sessions-*', function (err, total) {results.recordsTotal = total;});
    Db.searchPrimary(indices, 'session', query, function(err, result) {
      if (err || result.error) {
        console.log("spigraph.json error", err, (result?result.error:null));
        return error(errorString(err, result));
      }
      results.recordsFiltered = result.hits.total;

      results.graph = graphMerge(req, query, result.aggregations);
      results.map = mapMerge(result.aggregations);

      if (!result.aggregations) {
        result.aggregations = {field: {buckets: []}};
      }

      var aggs = result.aggregations.field.buckets;
      var interval = query.aggregations.dbHisto.histogram.interval;
      var filter = {term: {}};
      query.query.bool.filter.push(filter);

      delete query.aggregations.field;

      var queries = [];
      aggs.forEach(function(item) {
        filter.term[field] = item.key;
        queries.push(JSON.stringify(query));
      });

      Db.msearch(indices, 'session', queries, function(err, result) {
        if (!result.responses) {
          return res.send(results);
        }

        result.responses.forEach(function(item, i) {
          var r = {name: aggs[i].key, count: aggs[i].doc_count};

          r.graph = graphMerge(req, query, result.responses[i].aggregations);
          if (r.graph.xmin === null) {
            r.graph.xmin = results.graph.xmin || results.graph.paHisto[0][0];
          }

          if (r.graph.xmax === null) {
            r.graph.xmax = results.graph.xmax || results.graph.paHisto[results.graph.paHisto.length-1][0];
          }

          r.map = mapMerge(result.responses[i].aggregations);
          eachCb(r, function () {
            results.items.push(r);
            r.lpHisto = 0.0;
            r.dbHisto = 0.0;
            r.paHisto = 0.0;
            var graph = r.graph;
            for (var i = 0; i < graph.lpHisto.length; i++) {
              r.lpHisto += graph.lpHisto[i][1];
              r.dbHisto += graph.db1Histo[i][1] + graph.db2Histo[i][1];
              r.paHisto += graph.pa2Histo[i][1] + graph.pa2Histo[i][1];
            }
            if (results.items.length === result.responses.length) {
              var s = req.query.sort || "lpHisto";
              results.items = results.items.sort(function (a, b) {
                var result;
                if (s === 'name') { result = a.name.localeCompare(b.name); }
                else { result = b[s] - a[s]; }
                return result;
              });
              return res.send(results);
            }
          });
        });
      });
    });
  });
});

app.get('/spiview.json', function(req, res) {
  if (req.query.spi === undefined) {
    return res.send({spi:{}, recordsTotal: 0, recordsFiltered: 0});
  }

  var spiDataMaxIndices = +Config.get("spiDataMaxIndices", 1);

  if (req.query.date === '-1' && spiDataMaxIndices !== -1) {
    return res.send({spi: {}, bsqErr: "'All' date range not allowed for spiview query"});
  }

  buildSessionQuery(req, function(bsqErr, query, indices) {
    if (bsqErr) {
      var r = {spi: {},
               bsqErr: bsqErr.toString(),
               health: Db.healthCache()
              };
      return res.send(r);
    }

    delete query.sort;

    if (!query.aggregations) {
      query.aggregations = {};
    }

    if (req.query.facets) {
      query.aggregations.protocols = {terms: {field: "prot-term", size:1000}};
    }

    queryValueToArray(req.query.spi).forEach(function (item) {
      var parts = item.split(":");
      if (parts[0] === "fileand") {
        query.aggregations[parts[0]] = {terms: {field: "no", size: 1000}, aggs: {fs: {terms: {field: "fs", size: parts.length>1?parseInt(parts[1],10):10}}}};
      } else {
        query.aggregations[parts[0]] = {terms: {field: parts[0]}};

        if (parts.length > 1) {
          query.aggregations[parts[0]].terms.size = parseInt(parts[1], 10);
        }
      }
    });
    query.size = 0;

    console.log("spiview.json query", JSON.stringify(query), "indices", indices);

    var graph;
    var map;

    var indicesa = indices.split(",");
    if (spiDataMaxIndices !== -1 && indicesa.length > spiDataMaxIndices) {
      bsqErr = "To save ES from blowing up, reducing number of spi data indices searched from " + indicesa.length + " to " + spiDataMaxIndices + ".  This can be increased by setting spiDataMaxIndices in the config file.  Indices being searched: ";
      indices = indicesa.slice(-spiDataMaxIndices).join(",");
      bsqErr += indices;
    }

    var recordsFiltered = 0;
    var protocols;

    async.parallel({
      spi: function (sessionsCb) {
        Db.searchPrimary(indices, 'session', query, function(err, result) {
          if (Config.debug) {
            console.log("spiview.json result", util.inspect(result, false, 50));
          }
          if (err || result.error) {
            bsqErr = errorString(err, result);
            console.log("spiview.json ERROR", err, (result?result.error:null));
            sessionsCb(null, {});
            return;
          }

          recordsFiltered = result.hits.total;

          if (!result.aggregations) {
            result.aggregations = {};
            for (var spi in query.aggregations) {
              result.aggregations[spi] = {sum_other_doc_count: 0, buckets: []};
            }
          }

          if (result.aggregations.pr) {
            result.aggregations.pr.buckets.forEach(function (item) {
              item.key = Pcap.protocol2Name(item.key);
            });
          }

          if (req.query.facets) {
            graph = graphMerge(req, query, result.aggregations);
            map = mapMerge(result.aggregations);
            protocols = {};
            result.aggregations.protocols.buckets.forEach(function (item) {
              protocols[item.key] = item.doc_count;
            });

            delete result.aggregations.dbHisto;
            delete result.aggregations.paHisto;
            delete result.aggregations.mapG1;
            delete result.aggregations.mapG2;
            delete result.aggregations.protocols;
          }

          sessionsCb(null, result.aggregations);
        });
      },
      total: function (totalCb) {
        Db.numberOfDocuments('sessions-*', totalCb);
      },
      health: Db.healthCache
    },
    function(err, results) {
      async.parallel([
        function(parallelCb) {
          if (!results.spi.fileand) {
            return parallelCb();
          }
          var nresults = [];
          var sodc = 0;
          async.each(results.spi.fileand.buckets, function(nobucket, cb) {
            sodc += nobucket.fs.sum_other_doc_count;
            async.each(nobucket.fs.buckets, function (fsitem, cb) {
              Db.fileIdToFile(nobucket.key, fsitem.key, function(file) {
                if (file && file.name) {
                  nresults.push({key: file.name, doc_count: fsitem.doc_count})
                }
                cb();
              });
            }, function () {
              cb();
            });
          }, function () {
            nresults = nresults.sort(function(a, b) {
              if (a.doc_count === b.doc_count) {
                return a.key.localeCompare(b.key);
              }
              return b.doc_count - a.doc_count;
            });
            results.spi.fileand = {doc_count_error_upper_bound: 0, sum_other_doc_count: sodc, buckets: nresults};
            parallelCb();
          });
        },
        function(parallelCb) {
          fixTagBucketsField(results.spi, "ta", parallelCb, 0);
        },
        function(parallelCb) {
          fixTagBucketsField(results.spi, "hh", parallelCb, 12);
        },
        function(parallelCb) {
          fixTagBucketsField(results.spi, "hh1", parallelCb, 12);
        },
        function(parallelCb) {
          fixTagBucketsField(results.spi, "hh2", parallelCb, 12);
        }],
        function() {
          r = {health: results.health,
               recordsTotal: results.total,
               spi: results.spi,
               recordsFiltered: recordsFiltered,
               graph: graph,
               map: map,
               protocols: protocols,
               bsqErr: bsqErr
          };
          try {
            res.send(r);
          } catch (c) {
          }
        }
      );
    });
  });
});

app.get('/dns.json', function(req, res) {
  console.log("dns.json", req.query);
  dns.reverse(req.query.ip, function (err, data) {
    if (err) {
      return res.send({hosts: []});
    }
    return res.send({hosts: data});
  });
});

function buildConnections(req, res, cb) {
  if (req.query.dstField === "ip.dst:port") {
    var dstipport = true;
    req.query.dstField = "a2";
  }

  req.query.srcField       = req.query.srcField || "a1";
  req.query.dstField       = req.query.dstField || "a2";
  var fsrc                 = req.query.srcField.replace(".snow", "");
  var fdst                 = req.query.dstField.replace(".snow", "");
  var minConn              = req.query.minConn  || 1;
  req.query.iDisplayLength = req.query.iDisplayLength || "5000";

  var nodesHash = {};
  var connects = {};
  var tsrc = fmenum(fsrc);
  var tdst = fmenum(fdst);

  function process(vsrc, vdst, f, cb) {
    if (nodesHash[vsrc] === undefined) {
      nodesHash[vsrc] = {id: ""+vsrc, db: 0, by: 0, pa: 0, cnt: 0, sessions: 0};
    }

    nodesHash[vsrc].sessions++;
    nodesHash[vsrc].by += f.by;
    nodesHash[vsrc].db += f.db;
    nodesHash[vsrc].pa += f.pa;
    nodesHash[vsrc].type |= 1;

    if (nodesHash[vdst] === undefined) {
      nodesHash[vdst] = {id: ""+vdst, db: 0, by: 0, pa: 0, cnt: 0, sessions: 0};
    }

    nodesHash[vdst].sessions++;
    nodesHash[vdst].by += f.by;
    nodesHash[vdst].db += f.db;
    nodesHash[vdst].pa += f.pa;
    nodesHash[vdst].type |= 2;

    var n = "" + vsrc + "->" + vdst;
    if (connects[n] === undefined) {
      connects[n] = {value: 0, source: vsrc, target: vdst, by: 0, db: 0, pa: 0, no: {}};
      nodesHash[vsrc].cnt++;
      nodesHash[vdst].cnt++;
    }

    connects[n].value++;
    connects[n].by += f.by;
    connects[n].db += f.db;
    connects[n].pa += f.pa;
    connects[n].no[f.no] = 1;
    return setImmediate(cb);
  }

  function processDst(vsrc, adst, f, cb) {
    async.each(adst, function(vdst, dstCb) {
      if (tdst === FMEnum.other) {
        process(vsrc, vdst, f, dstCb);
      } else if (tdst === FMEnum.ip) {
        vdst = Pcap.inet_ntoa(vdst);
        if (dstipport) {
          vdst += ":" + f.p2;
        }
        process(vsrc, vdst, f, dstCb);
      } else {
        Db.tagIdToName(vdst, function (name) {
          if (tdst === FMEnum.tags) {
            vdst = name;
          } else {
            vdst = name.substring(12);
          }
          process(vsrc, vdst, f, dstCb);
        });
      }
    }, function (err) {
      return setImmediate(cb);
    });
  }

  buildSessionQuery(req, function(bsqErr, query, indices) {
    if (bsqErr) {
      return cb(bsqErr, 0, 0, 0);
    }

    query.query.bool.filter.push({exists: {field: req.query.srcField}});
    query.query.bool.filter.push({exists: {field: req.query.dstField}});

    query._source = ["by", "db", "pa", "no"];
    if (Db.isES5) {
      query.docvalue_fields = [fsrc, fdst];
    } else {
      query.fields = [fsrc, fdst];
    }
    if (dstipport) {
      query._source.push("p2");
    }

    console.log("buildConnections query", JSON.stringify(query));

    Db.searchPrimary(indices, 'session', query, function (err, graph) {
    //console.log("buildConnections result", JSON.stringify(graph));
      if (err || graph.error) {
        console.log("Build Connections ERROR", err, graph.error);
        return cb(err || graph.error);
      }
      var i;

      async.eachLimit(graph.hits.hits, 10, function(hit, hitCb) {
        var f = hit._source;

        var asrc = hit.fields[fsrc];
        var adst = hit.fields[fdst];


        if (asrc === undefined || adst === undefined) {
          return setImmediate(hitCb);
        }

        if (!Array.isArray(asrc)) {
          asrc = [asrc];
        }

        if (!Array.isArray(adst)) {
          adst = [adst];
        }

        async.each(asrc, function(vsrc, srcCb) {
          if (tsrc === FMEnum.other) {
            processDst(vsrc, adst, f, srcCb);
          } else if (tsrc === FMEnum.ip) {
            vsrc = Pcap.inet_ntoa(vsrc);
            processDst(vsrc, adst, f, srcCb);
          } else {
            Db.tagIdToName(vsrc, function (name) {
              if (tsrc === FMEnum.tags) {
                vsrc = name;
              } else {
                vsrc = name.substring(12);
              }
              processDst(vsrc, adst, f, srcCb);
            });
          }
        }, function (err) {
          setImmediate(hitCb);
        });
      }, function (err) {
        var nodes = [];
        for (var node in nodesHash) {
          if (nodesHash[node].cnt < minConn) {
            nodesHash[node].pos = -1;
          } else {
            nodesHash[node].pos = nodes.length;
            nodes.push(nodesHash[node]);
          }
        }


        var links = [];
        for (var key in connects) {
          var c = connects[key];
          c.source = nodesHash[c.source].pos;
          c.target = nodesHash[c.target].pos;
          if (c.source >= 0 && c.target >= 0) {
            links.push(connects[key]);
          }
        }

        //console.log("nodesHash", nodesHash);
        //console.log("connects", connects);
        //console.log("nodes", nodes.length, nodes);
        //console.log("links", links.length, links);

        return cb(null, nodes, links, graph.hits.total);
      });
    });
  });
}

app.get('/connections.json', function(req, res) {
  var health;
  Db.healthCache(function(err, h) {health = h;});
  buildConnections(req, res, function (err, nodes, links, total) {
    if (err) {
      res.status(403);
      return res.send(JSON.stringify({success: false, text: err.toString()}));
    }
    res.send({health: health, nodes: nodes, links: links, recordsFiltered: total});
  });
});

app.get('/connections.csv', function(req, res) {
  res.setHeader("Content-Type", "application/force-download");
  var seperator = req.query.seperator || ",";
  buildConnections(req, res, function (err, nodes, links, total) {
    if (err) {
      return res.send(err);
    }

    res.write("Source, Destination, Sessions, Packets, Bytes, Databytes\r\n");
    for (var i = 0, ilen = links.length; i < ilen; i++) {
      res.write("\"" + nodes[links[i].source].id.replace('"', '""') + "\"" + seperator +
                "\"" + nodes[links[i].target].id.replace('"', '""') + "\"" + seperator +
                     links[i].value + seperator +
                     links[i].pa + seperator +
                     links[i].by + seperator +
                     links[i].db + "\r\n");
    }
    res.end();
  });
});

function csvListWriter(req, res, list, fields, pcapWriter, extension) {
  if (list.length > 0 && list[0].fields) {
    list = list.sort(function(a,b){return a.fields.lp - b.fields.lp;});
  } else if (list.length > 0 && list[0]._source) {
    list = list.sort(function(a,b){return a._source.lp - b._source.lp;});
  }

  var fieldObjects  = Config.getDBFieldsMap();

  if (fields) {
    var columnHeaders = [];
    for (var i = 0, len = fields.length; i < len; ++i) {
      columnHeaders.push(fieldObjects[fields[i]].friendlyName);
    }
    res.write(columnHeaders.join(', '));
    res.write('\r\n');
  }

  for (var j = 0, jlen = list.length; j < jlen; j++) {
    var sessionData = list[j]._source || list[j].fields;

    if (!fields) { continue; }

    var values = [];
    for (var k = 0, len = fields.length; k < len; ++k) {
      let value = sessionData[fields[k]];
      if (fields[k] === 'pr' && value) {
        switch (value) {
          case 1:
            value = 'icmp';
            break;
          case 6:
            value = 'tcp';
            break;
          case 17:
            value =  'udp';
            break;
          case 58:
            value =  'icmpv6';
            break;
        }
      } else if (fieldObjects[fields[k]].type === 'ip' && value) {
        value = Pcap.inet_ntoa(value);
      }

      if (Array.isArray(value)) {
        let singleValue = '"' + value.join(', ') +  '"';
        values.push(singleValue);
      } else {
        if (value === undefined) { value = ''; }
        values.push(value);
      }
    }

    res.write(values.join(','));
    res.write('\r\n');
  }

  res.end();
}

app.get(/\/sessions.csv.*/, function(req, res) {
  noCache(req, res, "text/csv");
  // default fields to display in csv
  var fields = ["pr", "fp", "lp", "a1", "p1", "g1", "a2", "p2", "g2", "by", "db", "pa", "no"];
  // save requested fields because sessionsListFromQuery returns fields with
  // "ro" appended onto the end
  var reqFields = fields;

  if (req.query.fields) {
    fields = reqFields = req.query.fields.split(',');
  }

  if (req.query.ids) {
    var ids = queryValueToArray(req.query.ids);
    sessionsListFromIds(req, ids, fields, function(err, list) {
      csvListWriter(req, res, list, reqFields);
    });
  } else {
    sessionsListFromQuery(req, res, fields, function(err, list) {
      csvListWriter(req, res, list, reqFields);
    });
  }
});

app.get('/uniqueValue.json', function(req, res) {
  if (!Config.get('valueAutoComplete', !Config.get('multiES', false))) {
    res.send([]);
    return;
  }

  noCache(req, res);

  var query;

  if (req.query.type === "tags") {
    query = {bool: {must: {wildcard: {_uid: "tag#" + req.query.filter + "*"}},
                  must_not: {wildcard: {_uid: "tag#http:header:*"}}
                     }
          };
  } else {
    query = {wildcard: {_uid: "tag#http:header:" + req.query.filter + "*"}};
  }

  console.log("uniqueValue query", JSON.stringify(query));
  Db.search('tags', 'tag', {size:200, query: query}, function(err, result) {
    var terms = [];
    if (req.query.type === "tags") {
      result.hits.hits.forEach(function (hit) {
        terms.push(hit._id);
      });
    } else {
      result.hits.hits.forEach(function (hit) {
        terms.push(hit._id.substring(12));
      });
    }
    res.send(terms);
  });
});

app.get('/unique.txt', function(req, res) {
  if (req.query.field === undefined) {
    return res.send("Missing field parameter");
  }

  noCache(req, res);

  /* How should the results be written.  Use setImmediate to not blow stack frame */
  var writeCb;
  var doneCb;
  var writes = 0;
  var items = [];
  var aggSize = 1000000;
  if (req.query.autocomplete !== undefined) {
    if (!Config.get('valueAutoComplete', !Config.get('multiES', false))) {
      res.send([]);
      return;
    }

    var spiDataMaxIndices = +Config.get("spiDataMaxIndices", 3);
    if (spiDataMaxIndices !== -1) {
      if (req.query.date === '-1' ||
          (req.query.date !== undefined && +req.query.date > spiDataMaxIndices)) {
        console.log("INFO For autocomplete replacing date="+ req.query.date, "with", spiDataMaxIndices);
        req.query.date = spiDataMaxIndices;
      }
    }

    aggSize = 1000;
    doneCb = function() {
      res.send(items);
    }
    writeCb = function (item, cb) {
      items.push(item.key);
      if (writes++ > 1000) {
        writes = 0;
        setImmediate(cb);
      } else {
        cb();
      }
    };
  } else if (parseInt(req.query.counts, 10) || 0) {
    writeCb = function (item, cb) {
      res.write("" + item.key + ", " + item.doc_count + "\n");
      if (writes++ > 1000) {
        writes = 0;
        setImmediate(cb);
      } else {
        cb();
      }
    };
  } else {
    writeCb = function (item, cb) {
      res.write("" + item.key + "\n");
      if (writes++ > 1000) {
        writes = 0;
        setImmediate(cb);
      } else {
        cb();
      }
    };
  }

  /* How should each item be processed. */
  var eachCb;
  switch (fmenum(req.query.field)) {
  case FMEnum.other:
    eachCb = writeCb;
    break;
  case FMEnum.ip:
    eachCb = function(item, cb) {
      item.key = Pcap.inet_ntoa(item.key);
      writeCb(item, cb);
    };
    break;
  case FMEnum.tags:
    eachCb = function(item, cb) {
      Db.tagIdToName(item.key, function (name) {
        item.key = name;
        writeCb(item, cb);
      });
    };
    break;
  case FMEnum.hh:
    eachCb = function(item, cb) {
      Db.tagIdToName(item.key, function (name) {
        item.key = name.substring(12);
        writeCb(item, cb);
      });
    };
    break;
  }

  if (req.query.field === "a1:p1" || req.query.field === "a2:p2") {
    eachCb = function(item, cb) {
      var key = Pcap.inet_ntoa(item.key);
      item.field2.buckets.forEach(function (item2) {
        item2.key = key + ":" + item2.key;
        writeCb(item2, function() {});
      });
      cb();
    };
  }

  buildSessionQuery(req, function(err, query, indices) {
    delete query.sort;
    delete query.aggregations;

    if (req.query.field.match(/^(rawus|rawua)$/)) {
      var field = req.query.field.substring(3);
      query.size   = 200000;

      query._source = [field];

      query.query.bool.filter.push({exists: {field: field}});

      console.log("unique query", indices, JSON.stringify(query));
      Db.searchPrimary(indices, 'session', query, function(err, result) {
        console.log(err);
        var counts = {};

        // Count up hits
        var hits = result.hits.hits;
        for (var i = 0, ilen = hits.length; i < ilen; i++) {
          var fields = hits[i]._source || hits[i].fields;
          var avalue = fields[field];
          if (Array.isArray(avalue)) {
            for (var j = 0, jlen = avalue.length; j < jlen; j++) {
              var value = avalue[j];
              counts[value] = (counts[value] || 0) + 1;
            }
          } else {
            counts[avalue] = (counts[avalue] || 0) + 1;
          }
        }

        // Change to aggregations looking array
        var aggregations = [];
        for (var key in counts) {
          aggregations.push({key: key, doc_count: counts[key]});
        }

        async.forEachSeries(aggregations, eachCb, function () {
          doneCb?doneCb():res.end();
        });
      });
    } else {
      if (req.query.field === "a1:p1") {
        query.aggregations = {field: { terms : {field : "a1", size: aggSize}, aggregations: {field2: {terms: {field: "p1", size: 100}}}}};
      } else if (req.query.field === "a2:p2") {
        query.aggregations = {field: { terms : {field : "a2", size: aggSize}, aggregations: {field2: {terms: {field: "p2", size: 100}}}}};
      } else  {
        query.aggregations = {field: { terms : {field : req.query.field, size: aggSize}}};
      }
      query.size = 0;
      console.log("unique aggregations", indices, JSON.stringify(query));
      Db.searchPrimary(indices, 'session', query, function(err, result) {
        if (err) {
          console.log("Error", query, err);
          return doneCb?doneCb():res.end();
        }
        if (Config.debug) {
          console.log("unique.txt result", util.inspect(result, false, 50));
        }

        async.forEachSeries(result.aggregations.field.buckets, eachCb, function () {
          doneCb?doneCb():res.end();
        });
      });
    }
  });
});

function processSessionIdDisk(session, headerCb, packetCb, endCb, limit) {
  function processFile(pcap, pos, i, nextCb) {
    pcap.ref();
    pcap.readPacket(pos, function(packet) {
      switch(packet) {
      case null:
        endCb("Error loading data for session " + session._id, null);
        break;
      case undefined:
        break;
      default:
        packetCb(pcap, packet, nextCb, i);
        break;
      }
      pcap.unref();
    });
  }

  var fields;

  fields = session._source || session.fields;

  var fileNum;
  var itemPos = 0;
  async.eachLimit(fields.ps, limit || 1, function(pos, nextCb) {
    if (pos < 0) {
      fileNum = pos * -1;
      return nextCb(null);
    }

    // Get the pcap file for this node a filenum, if it isn't opened then do the filename lookup and open it
    var opcap = Pcap.get(fields.no + ":" + fileNum);
    if (!opcap.isOpen()) {
      Db.fileIdToFile(fields.no, fileNum, function(file) {
        if (!file) {
          console.log("WARNING - Only have SPI data, PCAP file no longer available", fields.no + '-' + fileNum);
          return nextCb("Only have SPI data, PCAP file no longer available for " + fields.no + '-' + fileNum);
        }
        if (file.kekId) {
          file.kek = Config.sectionGet("keks", file.kekId, undefined);
          if (file.kek === undefined) {
            console.log("ERROR - Couldn't find kek", file.kekId, "in keks section");
            return nextCb("Couldn't find kek " + file.kekId + " in keks section");
          }
        }

        var ipcap = Pcap.get(fields.no + ":" + file.num);

        try {
          ipcap.open(file.name, file);
        } catch (err) {
          console.log("ERROR - Couldn't open file ", err);
          return nextCb("Couldn't open file " + err);
        }

        if (headerCb) {
          headerCb(ipcap, ipcap.readHeader());
          headerCb = null;
        }
        processFile(ipcap, pos, itemPos++, nextCb);
      });
    } else {
      if (headerCb) {
        headerCb(opcap, opcap.readHeader());
        headerCb = null;
      }
      processFile(opcap, pos, itemPos++, nextCb);
    }
  },
  function (pcapErr, results) {
    endCb(pcapErr, fields);
  });
}

function processSessionId(id, fullSession, headerCb, packetCb, endCb, maxPackets, limit) {
  var options;
  if (!fullSession) {
    options  = {_source: "no,pa,ps,psl,a1,p1,tipv61-term"};
  }

  Db.getWithOptions(Db.id2Index(id), 'session', id, options, function(err, session) {
    if (err || !session.found) {
      console.log("session get error", err, session);
      return endCb("Session not found", null);
    }

    var fields = session._source || session.fields;

    if (maxPackets && fields.ps.length > maxPackets) {
      fields.ps.length = maxPackets;
    }

    /* Go through the list of prefetch the id to file name if we are running in parallel to
     * reduce the number of elasticsearch queries and problems
     */
    var outstanding = 0;
    var saveInfo;
    for (var i = 0, ilen = fields.ps.length; i < ilen; i++) {
      if (fields.ps[i] < 0) {
        outstanding++;
        Db.fileIdToFile(fields.no, -1 * fields.ps[i], function (info) {
          outstanding--;
          if (i === 0) {
            saveInfo = info;
          }
          if (i === ilen && outstanding === 0) {
            i++; // So not called again below
            readyToProcess();
          }
        });
      }
    }

    if (i === ilen && outstanding === 0) {
      readyToProcess();
    }

    function readyToProcess() {
      var pcapWriteMethod = Config.getFull(fields.no, "pcapWriteMethod");
      var psid = processSessionIdDisk;
      var writer = internals.writers[pcapWriteMethod];
      if (writer && writer.processSessionId) {
        psid = writer.processSessionId;
      }

      psid(session, headerCb, packetCb, function (err, fields) {
        if (!fields) {
          return endCb(err, fields);
        }

        if (!fields.ta) {
          fields.ta = [];
        }

        fixFields(fields, endCb);
      }, limit);
    }
  });
}

function processSessionIdAndDecode(id, numPackets, doneCb) {
  var packets = [];
  processSessionId(id, true, null, function (pcap, buffer, cb, i) {
    var obj = {};
    if (buffer.length > 16) {
      pcap.decode(buffer, obj);
    } else {
      obj = {ip: {p: ""}};
    }
    packets[i] = obj;
    cb(null);
  },
  function(err, session) {
    if (err) {
      return doneCb("error");
    }
    packets = packets.filter(Boolean);
    if (packets.length === 0) {
      return doneCb(null, session, []);
    } else if (packets[0].ip === undefined) {
      return doneCb(null, session, []);
    } else if (packets[0].ip.p === 1) {
      Pcap.reassemble_icmp(packets, function(err, results) {
        return doneCb(err, session, results);
      });
    } else if (packets[0].ip.p === 6) {
      var key;
      if (session["tipv61-term"]) {
        key = session["tipv61-term"];
      } else {
        key = Pcap.inet_ntoa(session.a1);
      }
      Pcap.reassemble_tcp(packets, key + ':' + session.p1, function(err, results) {
        return doneCb(err, session, results);
      });
    } else if (packets[0].ip.p === 17) {
      Pcap.reassemble_udp(packets, function(err, results) {
        return doneCb(err, session, results);
      });
    } else {
      return doneCb(null, session, []);
    }
  },
  numPackets, 10);
}

// Some ideas from hexy.js
function toHex(input, offsets) {
  var out = "";
  var i, ilen;

  for (var pos = 0, poslen = input.length; pos < poslen; pos += 16) {
    var line = input.slice(pos, Math.min(pos+16, input.length));
    if (offsets) {
      out += sprintf.sprintf("<span class=\"sessionln\">%08d:</span> ", pos);
    }

    for (i = 0; i < 16; i++) {
      if (i % 2 === 0 && i > 0) {
        out += " ";
      }
      if (i < line.length) {
        out += sprintf.sprintf("%02x", line[i]);
      } else {
        out += "  ";
      }
    }

    out += " ";

    for (i = 0, ilen = line.length; i < ilen; i++) {
      if (line[i] <= 32 || line[i]  > 128) {
        out += ".";
      } else {
        out += safeStr(line.toString("ascii", i, i+1));
      }
    }
    out += "\n";
  }
  return out;
}

// Modified version of https://gist.github.com/penguinboy/762197
function flattenObject1 (obj) {
  var toReturn = {};

  for (var i in obj) {
    if (!obj.hasOwnProperty(i)) {
      continue;
    }

    if ((typeof obj[i]) === 'object' && !Array.isArray(obj[i])) {
      for (var x in obj[i]) {
        if (!obj[i].hasOwnProperty(x)) {
          continue;
        }

        toReturn[i + '.' + x] = obj[i][x];
      }
    } else {
      toReturn[i] = obj[i];
    }
  }
  return toReturn;
}

function localSessionDetailReturnFull(req, res, session, incoming) {
  if (req.packetsOnly) { // only return packets
    res.render('sessionPackets.pug', {
      filename: 'sessionPackets',
      user: req.user,
      session: session,
      data: incoming,
      reqPackets: req.query.packets,
      query: req.query,
      basedir: "/",
      reqFields: Config.headers("headers-http-request"),
      resFields: Config.headers("headers-http-response"),
      emailFields: Config.headers("headers-email")
    }, function(err, data) {
      if (err) {
        console.trace("ERROR - ", err);
        return req.next(err);
      }
      res.send(data);
    });
  } else { // return SPI data and packets
    res.send("HOW DID I GET HERE?");
    console.trace("HOW DID I GET HERE");
  }
}

function localSessionDetailReturn(req, res, session, incoming) {
  //console.log("ALW", JSON.stringify(incoming));
  var numPackets = req.query.packets || 200;
  if (incoming.length > numPackets) {
    incoming.length = numPackets;
  }

  if (incoming.length === 0) {
    return localSessionDetailReturnFull(req, res, session, []);
  }

  var options = {
    id: session.id,
    nodeName: req.params.nodeName,
    order: [],
    "ITEM-HTTP": {
      order: []
    },
    "ITEM-SMTP": {
      order: []
    },
    "ITEM-CB": {
    }
  };

  if (req.query.needgzip) {
    options["ITEM-HTTP"].order.push("BODY-UNCOMPRESS");
    options["ITEM-SMTP"].order.push("BODY-UNBASE64");
    options["ITEM-SMTP"].order.push("BODY-UNCOMPRESS");
  }

  options.order.push("ITEM-HTTP");
  options.order.push("ITEM-SMTP");

  var decodeOptions = JSON.parse(req.query.decode || "{}");
  for (var key in decodeOptions) {
    if (key.match(/^ITEM/)) {
      options.order.push(key);
    } else {
      options["ITEM-HTTP"].order.push(key);
      options["ITEM-SMTP"].order.push(key);
    }
    options[key] = decodeOptions[key]
  }

  if (req.query.needgzip) {
    options["ITEM-HTTP"].order.push("BODY-UNCOMPRESS");
    options["ITEM-SMTP"].order.push("BODY-UNCOMPRESS");
  }

  options.order.push("ITEM-BYTES");
  options.order.push("ITEM-SORTER");
  if (req.query.needimage) {
    options.order.push("ITEM-LINKBODY");
  }
  if (req.query.base === "hex") {
    options.order.push("ITEM-HEX");
    options["ITEM-HEX"]= {showOffsets: req.query.line === "true"};
  } else if (req.query.base === "ascii") {
    options.order.push("ITEM-ASCII");
  } else if (req.query.base === "utf8") {
    options.order.push("ITEM-UTF8");
  } else {
    options.order.push("ITEM-NATURAL");
  }
  options.order.push("ITEM-CB");
  options["ITEM-CB"].cb = function(err, outgoing) {
    localSessionDetailReturnFull(req, res, session, outgoing);
  };

  if (Config.debug) {
    console.log("Pipeline options", options);
  }

  decode.createPipeline(options, options.order, new decode.Pcap2ItemStream(options, incoming));
}


function localSessionDetail(req, res) {
  if (!req.query) {
    req.query = {gzip: false, line: false, base: "natural", packets: 200};
  }

  req.query.needgzip  = req.query.gzip === "true" || false;
  req.query.needimage = req.query.image === "true" || false;
  req.query.line  = req.query.line  || false;
  req.query.base  = req.query.base  || "ascii";

  var packets = [];
  processSessionId(req.params.id, !req.packetsOnly, null, function (pcap, buffer, cb, i) {
    var obj = {};
    if (buffer.length > 16) {
      try {
        pcap.decode(buffer, obj);
      } catch (e) {
        obj = {ip: {p: "Error decoding" + e}};
        console.trace("loadSessionDetail error", e.stack);
      }
    } else {
      obj = {ip: {p: "Empty"}};
    }
    packets[i] = obj;
    cb(null);
  },
  function(err, session) {
    if (err && session === null) {
      return res.end("Couldn't look up SPI data, error for session " + req.params.id + " Error: " +  err);
    }
    session.id = req.params.id;

    if (session.ta) {
      session.ta = session.ta.sort();
    }

    if (session.hh) {
      session.hh = session.hh.sort();
    }
    if (session.hh1) {
      session.hh1 = session.hh1.sort();
    }
    if (session.hh2) {
      session.hh2 = session.hh2.sort();
    }
    if (session.pr) {
      session.pr = Pcap.protocol2Name(session.pr);
    }
    //console.log("session", util.inspect(session, false, 15));
    /* Now reassembly the packets */
    if (packets.length === 0) {
      session._err = err || "No pcap data found";
      localSessionDetailReturn(req, res, session, []);
    } else if (packets[0].ip === undefined) {
      session._err = "Couldn't decode pcap file, check viewer log";
      localSessionDetailReturn(req, res, session, []);
    } else if (packets[0].ip.p === 1) {
      Pcap.reassemble_icmp(packets, function(err, results) {
        session._err = err;
        localSessionDetailReturn(req, res, session, results || []);
      });
    } else if (packets[0].ip.p === 6) {
      var key;
      if (session["tipv61-term"]) {
        key = session["tipv61-term"];
      } else {
        key = Pcap.inet_ntoa(session.a1);
      }
      Pcap.reassemble_tcp(packets, key + ':' + session.p1, function(err, results) {
        session._err = err;
        localSessionDetailReturn(req, res, session, results || []);
      });
    } else if (packets[0].ip.p === 17) {
      Pcap.reassemble_udp(packets, function(err, results) {
        session._err = err;
        localSessionDetailReturn(req, res, session, results || []);
      });
    } else if (packets[0].ip.p === 58) {
      Pcap.reassemble_icmp(packets, function(err, results) {
        session._err = err;
        localSessionDetailReturn(req, res, session, results || []);
      });
    } else {
      session._err = "Unknown ip.p=" + packets[0].ip.p;
      localSessionDetailReturn(req, res, session, []);
    }
  },
  req.query.needimage?10000:400, 10);
}

/**
 * Get SPI data for a session
 */
app.get('/:nodeName/session/:id/detail', function(req, res) {
  Db.getWithOptions(Db.id2Index(req.params.id), 'session', req.params.id, {}, function(err, session) {
    if (err || !session.found) {
      return res.end("Couldn't look up SPI data, error for session " + req.params.id + " Error: " +  err);
    }

    session = session._source;

    session.id = req.params.id;

    if (session.ta) {
      session.ta = session.ta.sort();
    }
    if (session.hh) {
      session.hh = session.hh.sort();
    }
    if (session.hh1) {
      session.hh1 = session.hh1.sort();
    }
    if (session.hh2) {
      session.hh2 = session.hh2.sort();
    }
    if (session.pr) {
      session.pr = Pcap.protocol2Name(session.pr);
    }

    fixFields(session, function() {
      pug.render(internals.sessionDetailNew, {
        filename    : "sessionDetail",
        user        : req.user,
        session     : session,
        query       : req.query,
        basedir     : "/",
        reqFields   : Config.headers("headers-http-request"),
        resFields   : Config.headers("headers-http-response"),
        emailFields : Config.headers("headers-email")
      }, function(err, data) {
        if (err) {
          console.trace("ERROR - ", err);
          return req.next(err);
        }
        res.send(data);
      });
    });
  });
});

/**
 * Get Session Packets
 */
app.get('/:nodeName/session/:id/packets', function(req, res) {
  isLocalView(req.params.nodeName, function () {
     noCache(req, res);
     req.packetsOnly = true;
     localSessionDetail(req, res);
   },
   function () {
     return proxyRequest(req, res, function (err) {
       Db.get(Db.id2Index(req.params.id), 'session', req.params.id, function(err, session) {
         var fields = session._source || session.fields;
         fields._err = "Couldn't connect to remote viewer to fetch packets";
         localSessionDetailReturnFull(req, res, fields, []);
       });
     });
   });
});

function reqGetRawBody(req, cb) {
  processSessionIdAndDecode(req.params.id, 10000, function(err, session, incoming) {
    if (err) {
      return cb(err);
    }


    if (incoming.length === 0) {
      return cb(null, null);
    }

    var options = {
      id: session.id,
      nodeName: req.params.nodeName,
      order: [],
      "ITEM-HTTP": {
        order: []
      },
      "ITEM-SMTP": {
        order: ["BODY-UNBASE64"]
      },
      "ITEM-CB": {
      },
      "ITEM-RAWBODY": {
        bodyNumber: +req.params.bodyNum
      }
    };

    if (req.query.needgzip) {
      options["ITEM-HTTP"].order.push("BODY-UNCOMPRESS");
      options["ITEM-SMTP"].order.push("BODY-UNCOMPRESS");
    }

    options.order.push("ITEM-HTTP");
    options.order.push("ITEM-SMTP");

    options.order.push("ITEM-RAWBODY");
    options.order.push("ITEM-CB");
    options["ITEM-CB"].cb = function(err, items) {
      if (err) {
        return cb(err);
      }
      if (items === undefined || items.length === 0) {
        return cb("No match");
      }
      cb(err, items[0].data);
    };

    decode.createPipeline(options, options.order, new decode.Pcap2ItemStream(options, incoming));
  });
}

app.get('/:nodeName/:id/body/:bodyType/:bodyNum/:bodyName', checkProxyRequest, function(req, res) {
  reqGetRawBody(req, function (err, data) {
    if (err) {
      console.trace(err);
      return res.end("Error");
    }

    if (req.params.bodyType === "file") {
      res.setHeader("Content-Type", "application/force-download");
    }
    res.end(data);
  });
});

app.get('/:nodeName/:id/bodypng/:bodyType/:bodyNum/:bodyName', checkProxyRequest, function(req, res) {
  if (!Png) {
    return res.send (internals.emptyPNG);
  }
  reqGetRawBody(req, function (err, data) {
    if (err || data === null || data.length === 0) {
      return res.send (internals.emptyPNG);
    }
    res.setHeader("Content-Type", "image/png");

    var png = new Png(data, internals.PNG_LINE_WIDTH, Math.ceil(data.length/internals.PNG_LINE_WIDTH), 'gray');
    var png_image = png.encodeSync();

    res.send(png_image);
  });
});

function writePcap(res, id, options, doneCb) {
  var b = new Buffer(0xfffe);
  var nextPacket = 0;
  var boffset = 0;
  var packets = {};

  processSessionId(id, false, function (pcap, buffer) {
    if (options.writeHeader) {
      res.write(buffer);
      options.writeHeader = false;
    }
  },
  function (pcap, buffer, cb, i) {
    // Save this packet in its spot
    packets[i] = buffer;

    // Send any packets we have in order
    while (packets[nextPacket]) {
      buffer = packets[nextPacket];
      delete packets[nextPacket];
      nextPacket++;

      if (boffset + buffer.length > b.length) {
        res.write(b.slice(0, boffset));
        boffset = 0;
        b = new Buffer(0xfffe);
      }
      buffer.copy(b, boffset, 0, buffer.length);
      boffset += buffer.length;
    }
    cb(null);
  },
  function(err, session) {
    if (err) {
      console.log("writePcap", err);
    }
    res.write(b.slice(0, boffset));
    doneCb(err);
  }, undefined, 10);
}

function writePcapNg(res, id, options, doneCb) {
  var b = new Buffer(0xfffe);
  var boffset = 0;

  processSessionId(id, true, function (pcap, buffer) {
    if (options.writeHeader) {
      res.write(pcap.getHeaderNg());
      options.writeHeader = false;
    }
  },
  function (pcap, buffer, cb) {
    if (boffset + buffer.length + 20 > b.length) {
      res.write(b.slice(0, boffset));
      boffset = 0;
      b = new Buffer(0xfffe);
    }

    /* Need to write the ng block, and conver the old timestamp */

    b.writeUInt32LE(0x00000006, boffset);               // Block Type
    var len = ((buffer.length + 20 + 3) >> 2) << 2;
    b.writeUInt32LE(len, boffset + 4);                  // Block Len 1
    b.writeUInt32LE(0, boffset + 8);                    // Interface Id

    // js has 53 bit numbers, this will over flow on Jun 05 2255
    var time = buffer.readUInt32LE(0)*1000000 + buffer.readUInt32LE(4);
    b.writeUInt32LE(Math.floor(time / 0x100000000), boffset + 12);         // Block Len 1
    b.writeUInt32LE(time % 0x100000000, boffset + 16);   // Interface Id

    buffer.copy(b, boffset + 20, 8, buffer.length - 8);     // cap_len, packet_len
    b.fill(0, boffset + 12 + buffer.length, boffset + 12 + buffer.length + (4 - (buffer.length%4)) % 4);   // padding
    boffset += len - 8;

    b.writeUInt32LE(0, boffset);                        // Options
    b.writeUInt32LE(len, boffset+4);                    // Block Len 2
    boffset += 8;

    cb(null);
  },
  function(err, session) {
    if (err) {
      console.log("writePcapNg", err);
      return;
    }
    res.write(b.slice(0, boffset));

    session.version = molochversion.version;
    delete session.ps;
    var json = JSON.stringify(session);

    var len = ((json.length + 20 + 3) >> 2) << 2;
    b = new Buffer(len);

    b.writeUInt32LE(0x80808080, 0);               // Block Type
    b.writeUInt32LE(len, 4);                      // Block Len 1
    b.write("MOWL", 8);                           // Magic
    b.writeUInt32LE(json.length, 12);             // Block Len 1
    b.write(json, 16);                            // Magic
    b.fill(0, 16 + json.length, 16 + json.length + (4 - (json.length%4)) % 4);   // padding
    b.writeUInt32LE(len, len-4);                  // Block Len 2
    res.write(b);

    doneCb(err);
  });
}

app.get('/:nodeName/pcapng/:id.pcapng', checkProxyRequest, function(req, res) {
  noCache(req, res, "application/vnd.tcpdump.pcap");
  writePcapNg(res, req.params.id, {writeHeader: !req.query || !req.query.noHeader || req.query.noHeader !== "true"}, function () {
    res.end();
  });
});

app.get('/:nodeName/pcap/:id.pcap', checkProxyRequest, function(req, res) {
  noCache(req, res, "application/vnd.tcpdump.pcap");

  writePcap(res, req.params.id, {writeHeader: !req.query || !req.query.noHeader || req.query.noHeader !== "true"}, function () {
    res.end();
  });
});

app.get('/:nodeName/raw/:id.png', checkProxyRequest, function(req, res) {
  noCache(req, res, "image/png");

  if (!Png) {
    return res.send (internals.emptyPNG);
  }

  processSessionIdAndDecode(req.params.id, 100, function(err, session, results) {
    if (err) {
      return res.send (internals.emptyPNG);
    }
    var size = 0;
    var i, ilen;
    for (i = (req.query.type !== 'dst'?0:1), ilen = results.length; i < ilen; i+=2) {
      size += results[i].data.length + 2*internals.PNG_LINE_WIDTH - (results[i].data.length % internals.PNG_LINE_WIDTH);
    }
    var buffer = new Buffer(size);
    var pos = 0;
    if (size === 0) {
      return res.send (internals.emptyPNG);
    }
    for (i = (req.query.type !== 'dst'?0:1), ilen = results.length; i < ilen; i+=2) {
      results[i].data.copy(buffer, pos);
      pos += results[i].data.length;
      var fillpos = pos;
      pos += 2*internals.PNG_LINE_WIDTH - (results[i].data.length % internals.PNG_LINE_WIDTH);
      buffer.fill(0xff, fillpos, pos);
    }

    var png = new Png(buffer, internals.PNG_LINE_WIDTH, (size/internals.PNG_LINE_WIDTH)-1, 'gray');
    var png_image = png.encodeSync();

    res.send(png_image);
  });
});

app.get('/:nodeName/raw/:id', checkProxyRequest, function(req, res) {
  noCache(req, res, "application/vnd.tcpdump.pcap");

  processSessionIdAndDecode(req.params.id, 10000, function(err, session, results) {
    if (err) {
      return res.send("Error");
    }
    for (var i = (req.query.type !== 'dst'?0:1), ilen = results.length; i < ilen; i+=2) {
      res.write(results[i].data);
    }
    res.end();
  });
});

app.get('/:nodeName/entirePcap/:id.pcap', checkProxyRequest, function(req, res) {
  noCache(req, res, "application/vnd.tcpdump.pcap");

  var options = {writeHeader: true};

  var query = { _source: ["ro"],
                size: 1000,
                query: {term: {ro: req.params.id}},
                sort: { lp: { order: 'asc' } }
              };

  console.log("entirePcap query", JSON.stringify(query));

  Db.searchPrimary('sessions-*', 'session', query, function(err, data) {
    async.forEachSeries(data.hits.hits, function(item, nextCb) {
      writePcap(res, item._id, options, nextCb);
    }, function (err) {
      res.end();
    });
  });
});

function sessionsPcapList(req, res, list, pcapWriter, extension) {

  if (list.length > 0 && list[0].fields) {
    list = list.sort(function(a,b){return a.fields.lp - b.fields.lp;});
  } else if (list.length > 0 && list[0]._source) {
    list = list.sort(function(a,b){return a._source.lp - b._source.lp;});
  }

  var options = {writeHeader: true};

  async.eachLimit(list, 10, function(item, nextCb) {
    var fields = item._source || item.fields;
    isLocalView(fields.no, function () {
      // Get from our DISK
      pcapWriter(res, item._id, options, nextCb);
    },
    function () {
      // Get from remote DISK
      getViewUrl(fields.no, function(err, viewUrl, client) {
        var buffer = new Buffer(fields.pa*20 + fields.by);
        var bufpos = 0;
        var info = url.parse(viewUrl);
        info.path = Config.basePath(fields.no) + fields.no + "/" + extension + "/" + item._id + "." + extension;
        info.agent = (client === http?internals.httpAgent:internals.httpsAgent);

        addAuth(info, req.user, fields.no);
        addCaTrust(info, fields.no);
        var preq = client.request(info, function(pres) {
          pres.on('data', function (chunk) {
            if (bufpos + chunk.length > buffer.length) {
              var tmp = new Buffer(buffer.length + chunk.length*10);
              buffer.copy(tmp, 0, 0, bufpos);
              buffer = tmp;
            }
            chunk.copy(buffer, bufpos);
            bufpos += chunk.length;
          });
          pres.on('end', function () {
            if (bufpos < 24) {
            } else if (options.writeHeader) {
              options.writeHeader = false;
              res.write(buffer.slice(0, bufpos));
            } else {
              res.write(buffer.slice(24, bufpos));
            }
            setImmediate(nextCb);
          });
        });
        preq.on('error', function (e) {
          console.log("ERROR - Couldn't proxy pcap request=", info, "\nerror=", e);
          nextCb(null);
        });
        preq.end();
      });
    });
  }, function(err) {
    res.end();
  });
}

function sessionsPcap(req, res, pcapWriter, extension) {
  noCache(req, res, "application/vnd.tcpdump.pcap");

  if (req.query.ids) {
    var ids = queryValueToArray(req.query.ids);

    sessionsListFromIds(req, ids, ["lp", "no", "by", "pa", "ro"], function(err, list) {
      sessionsPcapList(req, res, list, pcapWriter, extension);
    });
  } else {
    sessionsListFromQuery(req, res, ["lp", "no", "by", "pa", "ro"], function(err, list) {
      sessionsPcapList(req, res, list, pcapWriter, extension);
    });
  }
}

app.get(/\/sessions.pcapng.*/, function(req, res) {
  return sessionsPcap(req, res, writePcapNg, "pcapng");
});

app.get(/\/sessions.pcap.*/, function(req, res) {
  return sessionsPcap(req, res, writePcap, "pcap");
});


app.post('/changeSettings', checkToken, function(req, res) {
  function error(text) {
    return res.send(JSON.stringify({success: false, text: text}));
  }

  Db.getUser(req.token.suserId, function(err, user) {
    if (err || !user.found) {
      console.log("changeSettings failed", err, user);
      return error("Unknown user");
    }

    user = user._source;
    user.settings = req.body;
    delete user.settings.token;

    Db.setUser(user.userId, user, function(err, info) {
      if (err) {
        console.log("changeSettings error", err, info);
        return error("Change settings update failed");
      }
      return res.send(JSON.stringify({success: true, text: "Changed password successfully"}));
    });
  });
});

app.get('/user/settings', function(req, res) {
  Db.getUserCache(req.user.userId, function(err, user) {
    if (err || !user || !user.found) {
      if (app.locals.noPasswordSecret) {
        // TODO: send anonymous user's settings
        return res.send("{}");
      } else {
        console.log("Unknown user", err, user);
        return res.send("{}");
      }
    }

    var settings = user._source.settings || {};

    return res.send(settings);
  });
});

app.get('/user/views', function(req, res) {
  Db.getUserCache(req.user.userId, function(err, user) {
    if (err || !user || !user.found) {
      if (app.locals.noPasswordSecret) {
        // TODO: send anonymous user's views
        return res.send("{}");
      } else {
        console.log("Unknown user", err, user);
        return res.send("{}");
      }
    }

    var views = user._source.views || {};

    return res.send(views);
  });
});

app.post('/user/views/delete', checkCookieToken, function(req, res) {
  function error(text) {
    return res.send(JSON.stringify({success: false, text: text}));
  }

  if (!req.body.view) { return error("Missing view"); }

  Db.getUser(req.token.userId, function(err, user) {
    if (err || !user.found) {
      console.log("Delete view failed", err, user);
      return error("Unknown user");
    }

    user = user._source;
    user.views = user.views || {};
    delete user.views[req.body.view];

    Db.setUser(user.userId, user, function(err, info) {
      if (err) {
        console.log("Delete view failed", err, info);
        return error("Delete view failed");
      }
      return res.send(JSON.stringify({success: true, text: "Deleted view successfully"}));
    });
  });
});

internals.usersMissing = {
  userId: "",
  userName: "",
  expression: "",
  enabled: 0,
  createEnabled: 0,
  webEnabled: 0,
  headerAuthEnabled: 0,
  emailSearch: 0,
  removeEnabled: 0
};
app.post('/user/list', function(req, res) {
  var columns = ["userId", "userName", "expression", "enabled", "createEnabled", "webEnabled", "headerAuthEnabled", "emailSearch", "removeEnabled"];

  var query = {_source: columns,
               sort: {},
               from: +req.body.start || 0,
               size: +req.body.length || 10000
              };

  if (req.body.filter) {
    query.query = {bool: {should: [{wildcard: {userName: "*" + req.body.filter + "*"}},
                                   {wildcard: {userId: "*" + req.body.filter + "*"}}
                                  ]
                         }
                  };
  }

  req.body.sortField = req.body.sortField || "userId";
  query.sort[req.body.sortField] = { order: req.body.desc === true ? "desc": "asc"};
  query.sort[req.body.sortField].missing = internals.usersMissing[req.body.sortField];

  async.parallel({
    users: function (cb) {
      Db.searchUsers(query, function(err, result) {
        if (err || result.error) {
          console.log("ERROR - users.json", err || result.error);
          res.send({total: 0, results: []});
        } else {
          var results = {total: result.hits.total, results: []};
          for (var i = 0, ilen = result.hits.hits.length; i < ilen; i++) {
            var fields = result.hits.hits[i]._source || result.hits.hits[i].fields;
            fields.id = result.hits.hits[i]._id;
            fields.expression = safeStr(fields.expression || "");
            fields.headerAuthEnabled = fields.headerAuthEnabled || false;
            fields.emailSearch = fields.emailSearch || false;
            fields.removeEnabled = fields.removeEnabled || false;
            fields.userName = safeStr(fields.userName || "");
            results.results.push(fields);
          }
          cb(null, results);
        }
      });
    },
    total: function (cb) {
      Db.numberOfUsers(cb);
    }
  },
  function(err, results) {
    var r = {draw: req.body.draw,
             recordsTotal: results.total,
             recordsFiltered: results.users.total,
             data: results.users.results};
    res.send(r);
  });
});

app.post('/user/create', checkCookieToken, function(req, res) {
  function error(text) {
    res.status(403);
    return res.send(JSON.stringify({success: false, text: text}));
  }

  if (!req.user.createEnabled) {
    return error('Need admin privileges');
  }

  if (!req.body || !req.body.userId || !req.body.userName || !req.body.password) {
    return error('Missing/Empty required fields');
  }

  if (req.body.userId.match(/[^\w.-]/)) {
    return error('User ID must be word characters');
  }

  Db.getUser(req.body.userId, function(err, user) {
    if (!user || user.found) {
      console.log('Trying to add duplicate user', err, user);
      return error('User already exists');
    }

    var nuser = {
      userId: req.body.userId,
      userName: req.body.userName,
      expression: req.body.expression,
      passStore: Config.pass2store(req.body.userId, req.body.password),
      enabled: req.body.enabled === true,
      webEnabled: req.body.webEnabled === true,
      emailSearch: req.body.emailSearch === true,
      headerAuthEnabled: req.body.headerAuthEnabled === true,
      createEnabled: req.body.createEnabled === true,
      removeEnabled: req.body.removeEnabled === true
    };

    console.log('Creating new user', nuser);
    Db.setUser(req.body.userId, nuser, function(err, info) {
      if (!err) {
        return res.send(JSON.stringify({success: true, text:'User created succesfully'}));
      } else {
        console.log('ERROR - add user', err, info);
        return error(err);
      }
    });
  });
});

app.post('/user/delete', checkCookieToken, function(req, res) {
  function error(text) {
    res.status(403);
    return res.send(JSON.stringify({success: false, text: text}));
  }

  if (!req.user.createEnabled) {
    return error('Need admin privileges');
  }

  if (req.body.userId === req.user.userId) {
    return error('Can not delete yourself');
  }

  Db.deleteUser(req.body.userId, function(err, data) {
    setTimeout(function () {
      res.send(JSON.stringify({success: true, text: 'User deleted successfully'}));
    }, 200);
  });
});

app.post('/user/update', checkCookieToken, function(req, res) {
  function error(text) {
    res.status(403);
    return res.send(JSON.stringify({success: false, text: text}));
  }

  if (!req.user.createEnabled) {
    return error('Need admin privileges');
  }

  /*if (req.params.userId === req.user.userId && req.query.createEnabled !== undefined && req.query.createEnabled !== "true") {
    return res.send(JSON.stringify({success: false, text: "Can not turn off your own admin privileges"}));
  }*/

  Db.getUser(req.body.userId, function(err, user) {
    if (err || !user.found) {
      console.log('update user failed', err, user);
      return error('User not found');
    }
    user = user._source;

    user.enabled = req.body.enabled === true;

    if (req.body.expression !== undefined) {
      if (req.body.expression.match(/^\s*$/)) {
        delete user.expression;
      } else {
        user.expression = req.body.expression;
      }
    }

    if (req.body.userName !== undefined) {
      if (req.body.userName.match(/^\s*$/)) {
        console.log("ERROR - empty username", req.body);
        return error('Username can not be empty');
      } else {
        user.userName = req.body.userName;
      }
    }

    user.webEnabled = req.body.webEnabled === true;
    user.emailSearch = req.body.emailSearch === true;
    user.headerAuthEnabled = req.body.headerAuthEnabled === true;
    user.removeEnabled = req.body.removeEnabled === true;

    // Can only change createEnabled if it is currently turned on
    if (req.body.createEnabled !== undefined && req.user.createEnabled && req.body.createEnabled) {
      user.createEnabled = req.body.createEnabled === true;
    }

    Db.setUser(req.body.userId, user, function(err, info) {
      console.log(user, err, info);
      return res.send(JSON.stringify({success: true, text:'User "' + req.body.userId + '" updated successfully'}));
    });
  });
});

app.post('/tableState/:tablename', function(req, res) {
  function error(text) {
    return res.send(JSON.stringify({success: false, text: text}));
  }

  Db.getUser(req.user.userId, function(err, user) {
    if (err || !user.found) {
      console.log("save tableState failed", err, user);
      return error("Unknown user");
    }
    user = user._source;

    if (!user.tableStates) {
      user.tableStates = {};
    }
    user.tableStates[req.params.tablename] = req.body;
    Db.setUser(user.userId, user, function(err, info) {
      if (err) {
        console.log("tableState error", err, info);
        return error("tableState update failed");
      }
      return res.send(JSON.stringify({success: true, text: "updated table state successfully"}));
    });
  });
});

app.get('/tableState/:tablename', function(req, res) {
  Db.getUserCache(req.user.userId, function(err, user) {
    if (err || !user.found) {
      console.log("Unknown user", err, user);
      return res.send("{}");
    }
    user = user._source;
    if (!user.tableStates || !user.tableStates[req.params.tablename]) {
      return res.send("{}");
    }
    return res.send(user.tableStates[req.params.tablename]);
  });
});

//////////////////////////////////////////////////////////////////////////////////
//// Session Add/Remove Tags
//////////////////////////////////////////////////////////////////////////////////

function addTagsList(allTagIds, allTagNames, list, doneCb) {
  async.eachLimit(list, 10, function(session, nextCb) {
    var tagIds = [];

    var fields = session._source || session.fields;

    if (!fields) {
      console.log("No Fields", session);
      return nextCb(null);
    }

    if (fields.ta === undefined) {
      fields.ta = [];
      fields["tags-term"] = [];
    }


    // Find which tags need to be added to this session
    for (var i = 0, ilen = allTagIds.length; i < ilen; i++) {
      if (fields.ta.indexOf(allTagIds[i]) === -1) {
        fields.ta.push(allTagIds[i]);
      }
    }

    // Do the ES update
    var document = {
      doc: {
        ta: fields.ta
      }
    };

    // Do the same for tags-term if it exists.  (it won't for old sessions)
    if (fields["tags-term"]) {
      for (var i = 0, ilen = allTagNames.length; i < ilen; i++) {
        if (fields["tags-term"].indexOf(allTagNames[i]) === -1) {
          fields["tags-term"].push(allTagNames[i]);
        }
      }
      document.doc["tags-term"] = fields["tags-term"];
    }

    Db.update(Db.id2Index(session._id), 'session', session._id, document, function(err, data) {
      if (err) {
        console.log("CAN'T UPDATE", session, err, data);
      }
      nextCb(null);
    });
  }, doneCb);
}

function removeTagsList(res, allTagIds, allTagNames, list) {
  async.eachLimit(list, 10, function(session, nextCb) {
    var tagIds = [];

    var fields = session._source || session.fields;
    if (!fields || !fields.ta) {
      return nextCb(null);
    }

    // Find which tags need to be removed from this session
    for (var i = 0, ilen = allTagIds.length; i < ilen; i++) {
      var pos = fields.ta.indexOf(allTagIds[i]);
      if (pos !== -1) {
        fields.ta.splice(pos, 1);
      }
    }

    // Do the ES update
    var document = {
      doc: {
        ta: fields.ta
      }
    };

    // Do the same for tags-term if it exists.  (it won't for old sessions)
    if (fields["tags-term"]) {
      for (var i = 0, ilen = allTagNames.length; i < ilen; i++) {
        var pos = fields["tags-term"].indexOf(allTagNames[i]);
        if (pos !== -1) {
          fields["tags-term"].splice(pos, 1);
        }
      }
      document.doc["tags-term"] = fields["tags-term"];
    }

    Db.update(Db.id2Index(session._id), 'session', session._id, document, function(err, data) {
      if (err) {
        console.log("removeTagsList error", err);
      }
      nextCb(null);
    });
  }, function (err) {
    return res.send(JSON.stringify({success: true, text: "Tags removed successfully"}));
  });
}

function mapTags(tags, prefix, tagsCb) {
  async.map(tags, function (tag, cb) {
    Db.tagNameToId(prefix + tag, function (tagid) {
      if (tagid === -1) {
        Db.createTag(prefix + tag, function(tagid) {
          cb(null, tagid);
        });
      } else {
        cb(null, tagid);
      }
    });
  }, function (err, result) {
    tagsCb(null, result);
  });
}

app.post('/addTags', function(req, res) {
  var tags = [];
  if (req.body.tags) {
    tags = req.body.tags.replace(/[^-a-zA-Z0-9_:,]/g, "").split(",");
  }

  if (tags.length === 0) {
    return res.send(JSON.stringify({success: false, text: "No tags specified"}));
  }

  mapTags(tags, "", function(err, tagIds) {
    if (req.body.ids) {
      var ids = queryValueToArray(req.body.ids);

      sessionsListFromIds(req, ids, ["ta", "tags-term", "no"], function(err, list) {
        addTagsList(tagIds, tags, list, function () {
          return res.send(JSON.stringify({success: true, text: "Tags added successfully"}));
        });
      });
    } else {
      sessionsListFromQuery(req, res, ["ta", "tags-term", "no"], function(err, list) {
        addTagsList(tagIds, tags, list, function () {
          return res.send(JSON.stringify({success: true, text: "Tags added successfully"}));
        });
      });
    }
  });
});

app.post('/removeTags', function(req, res) {
  if (!req.user.removeEnabled) {
    return res.send(JSON.stringify({success: false, text: "Need remove data privileges"}));
  }
  var tags = [];
  if (req.body.tags) {
    tags = req.body.tags.replace(/[^-a-zA-Z0-9_:,]/g, "").split(",");
  }

  if (tags.length === 0) {
    return res.send(JSON.stringify({success: false, text: "No tags specified"}));
  }

  mapTags(tags, "", function(err, tagIds) {
    if (req.body.ids) {
      var ids = queryValueToArray(req.body.ids);

      sessionsListFromIds(req, ids, ["ta", "tags-term"], function(err, list) {
        removeTagsList(res, tagIds, tags, list);
      });
    } else {
      sessionsListFromQuery(req, res, ["ta", "tags-term"], function(err, list) {
        removeTagsList(res, tagIds, tags, list);
      });
    }
  });
});

function searchAndTagList(allTagIds, list, doneCb) {
  async.eachLimit(list, 10, function(session, nextCb) {
    var fields = session._source || session.fields;
    if (!fields) {
      console.log("No Fields", session);
      return nextCb(null);
    }

    var doit = false;
    if (fields.ta) {
      for (var i = 0, ilen = allTagIds.length; i < ilen; i++) {
        if (fields.ta.indexOf(allTagIds[i]) === -1) {
          doit = true;
          break;
        }
      }
    } else {
      doit = true;
    }

    if (doit) {
      console.log("doit", session._id);
    } else {
      console.log("dont doit", session._id);
    }
    nextCb(null);
  }, doneCb);
}

app.post('/searchAndTag', function(req, res) {
  var tags = [];
  var regex = req.body.regex;
  if (req.body.tags) {
    tags = req.body.tags.replace(/[^-a-zA-Z0-9_:,]/g, "").split(",");
  }

  if (tags.length === 0) {
    return res.send(JSON.stringify({success: false, text: "No tags specified"}));
  }

  if (regex.length === 0) {
    return res.send(JSON.stringify({success: false, text: "No regex specified"}));
  }

  mapTags(tags, "", function(err, tagIds) {
    if (req.body.ids) {
      var ids = queryValueToArray(req.body.ids);

      sessionsListFromIds(req, ids, ["ta", "tags-term", "no"], function(err, list) {
        searchAndTagList(tagIds, list, function () {
          return res.send(JSON.stringify({success: true, text: "Tags added successfully"}));
        });
      });
    } else {
      sessionsListFromQuery(req, res, ["ta", "tags-term", "no"], function(err, list) {
        searchAndTagList(tagIds, list, function () {
          return res.send(JSON.stringify({success: true, text: "Tags added successfully"}));
        });
      });
    }
  });
});

//////////////////////////////////////////////////////////////////////////////////
//// Pcap Delete/Scrub
//////////////////////////////////////////////////////////////////////////////////

function pcapScrub(req, res, id, entire, endCb) {
  if (pcapScrub.scrubbingBuffers === undefined) {
    pcapScrub.scrubbingBuffers = [new Buffer(5000), new Buffer(5000), new Buffer(5000)];
    pcapScrub.scrubbingBuffers[0].fill(0);
    pcapScrub.scrubbingBuffers[1].fill(1);
    var str = "Scrubbed! Hoot! ";
    for (var i = 0; i < 5000;) {
      i += pcapScrub.scrubbingBuffers[2].write(str, i);
    }
  }

  function processFile(pcap, pos, i, nextCb) {
    pcap.ref();
    pcap.readPacket(pos, function(packet) {
      pcap.unref();

      if (packet) {
        if (packet.length > 16) {
          try {
            var obj = {};
            pcap.decode(packet, obj);
            pcap.scrubPacket(obj, pos, pcapScrub.scrubbingBuffers[0], entire);
            pcap.scrubPacket(obj, pos, pcapScrub.scrubbingBuffers[1], entire);
            pcap.scrubPacket(obj, pos, pcapScrub.scrubbingBuffers[2], entire);
          } catch (e) {
            console.log("Couldn't scrub packet at ", pos, e);
          }
          return nextCb(null);
        } else {
          console.log("Couldn't scrub packet at ", pos);
          return nextCb(null);
        }
      }
    });
  }

  Db.getWithOptions(Db.id2Index(id), 'session', id, {_source: "no,pr,ps,psl"}, function(err, session) {
    var fields = session._source || session.fields;

    var fileNum;
    var itemPos = 0;
    async.eachLimit(fields.ps, 10, function(pos, nextCb) {
      if (pos < 0) {
        fileNum = pos * -1;
        return nextCb(null);
      }

      // Get the pcap file for this node a filenum, if it isn't opened then do the filename lookup and open it
      var opcap = Pcap.get("write"+fields.no + ":" + fileNum);
      if (!opcap.isOpen()) {
        Db.fileIdToFile(fields.no, fileNum, function(file) {

          if (!file) {
            console.log("WARNING - Only have SPI data, PCAP file no longer available", fields.no + '-' + fileNum);
            return nextCb("Only have SPI data, PCAP file no longer available for " + fields.no + '-' + fileNum);
          }

          var ipcap = Pcap.get("write"+fields.no + ":" + file.num);

          try {
            ipcap.openReadWrite(file.name, file);
          } catch (err) {
            console.log("ERROR - Couldn't open file for writing", err);
            return nextCb("Couldn't open file for writing " + err);
          }

          processFile(ipcap, pos, itemPos++, nextCb);
        });
      } else {
        processFile(opcap, pos, itemPos++, nextCb);
      }
    },
    function (pcapErr, results) {
      if (entire) {
        Db.deleteDocument(Db.id2Index(session._id), 'session', session._id, function(err, data) {
          endCb(pcapErr, fields);
        });
      } else {
        // Do the ES update
        var document = {
          doc: {
            scrubby: req.user.userId || "-",
            scrubat: new Date().getTime()
          }
        };
        Db.update(Db.id2Index(session._id), 'session', session._id, document, function(err, data) {
          endCb(pcapErr, fields);
        });
      }
    });
  });
}

app.get('/:nodeName/scrub/:id', checkProxyRequest, function(req, res) {
  if (!req.user.removeEnabled) {
    return res.send(JSON.stringify({success: false, text: "Need remove data privileges"}));
  }

  noCache(req, res);
  res.statusCode = 200;

  pcapScrub(req, res, req.params.id, false, function(err) {
    res.end();
  });
});

app.get('/:nodeName/delete/:id', checkProxyRequest, function(req, res) {
  if (!req.user.removeEnabled) {
    return res.send(JSON.stringify({success: false, text: "Need remove data privileges"}));
  }

  noCache(req, res);
  res.statusCode = 200;

  pcapScrub(req, res, req.params.id, true, function(err) {
    res.end();
  });
});


function scrubList(req, res, entire, list) {
  if (!list) {
    return res.end(JSON.stringify({success: false, text: "Missing list of sessions"}));
  }

  async.eachLimit(list, 10, function(item, nextCb) {
    var fields = item._source || item.fields;

    isLocalView(fields.no, function () {
      // Get from our DISK
      pcapScrub(req, res, item._id, entire, nextCb);
    },
    function () {
      // Get from remote DISK
      getViewUrl(fields.no, function(err, viewUrl, client) {
        var info = url.parse(viewUrl);
        info.path = Config.basePath(fields.no) + fields.no + (entire?"/delete/":"/scrub/") + item._id;
        info.agent = (client === http?internals.httpAgent:internals.httpsAgent);
        addAuth(info, req.user, fields.no);
        addCaTrust(info, fields.no);
        var preq = client.request(info, function(pres) {
          pres.on('end', function () {
            setImmediate(nextCb);
          });
        });
        preq.on('error', function (e) {
          console.log("ERROR - Couldn't proxy scrub request=", info, "\nerror=", e);
          nextCb(null);
        });
        preq.end();
      });
    });
  }, function(err) {
    return res.end(JSON.stringify({success: true, text: (entire?"Deleting of ":"Scrubbing of ") + list.length + " sessions complete"}));
  });
}

app.post('/scrub', function(req, res) {
  if (!req.user.removeEnabled) {
    return res.send(JSON.stringify({success: false, text: "Need remove data privileges"}));
  }

  if (req.body.ids) {
    var ids = queryValueToArray(req.body.ids);

    sessionsListFromIds(req, ids, ["no"], function(err, list) {
      scrubList(req, res, false, list);
    });
  } else if (req.query.expression) {
    sessionsListFromQuery(req, res, ["no"], function(err, list) {
      scrubList(req, res, false, list);
    });
  } else {
    res.status(403);
    return res.send(JSON.stringify({ success: false, text: 'Error: Missing expression. An expression is required so you don\'t scrub everything.' }));
  }
});

app.post('/delete', function(req, res) {
  if (!req.user.removeEnabled) {
    return res.send(JSON.stringify({success: false, text: "Need remove data privileges"}));
  }

  if (req.body.ids) {
    var ids = queryValueToArray(req.body.ids);

    sessionsListFromIds(req, ids, ["no"], function(err, list) {
      scrubList(req, res, true, list);
    });
  } else if (req.query.expression) {
    sessionsListFromQuery(req, res, ["no"], function(err, list) {
      scrubList(req, res, true, list);
    });
  } else {
    res.status(403);
    return res.send(JSON.stringify({ success: false, text: 'Error: Missing expression. An expression is required so you don\'t delete everything.' }));
  }
});

//////////////////////////////////////////////////////////////////////////////////
//// Sending/Receive sessions
//////////////////////////////////////////////////////////////////////////////////
function sendSessionWorker(options, cb) {
  var packetslen = 0;
  var packets = [];
  var packetshdr;
  var ps = [-1];
  var tags = [];

  if (!options.saveId) {
    return cb({success: false, text: "Missing saveId"});
  }

  if (!options.cluster) {
    return cb({success: false, text: "Missing cluster"});
  }

  processSessionId(options.id, true, function(pcap, header) {
    packetshdr = header;
  }, function (pcap, packet, pcb, i) {
    packetslen += packet.length;
    packets[i] = packet;
    pcb(null);
  }, function (err, session) {
    var buffer;
    if (err || !packetshdr) {
      console.log("WARNING - No PCAP only sending SPI data err:", err);
      buffer = new Buffer(0);
      ps = [];
    } else {
      buffer = new Buffer(packetshdr.length + packetslen);
      var pos = 0;
      packetshdr.copy(buffer);
      pos += packetshdr.length;
      for(var i = 0, ilen = packets.length; i < ilen; i++) {
        ps.push(pos);
        packets[i].copy(buffer, pos);
        pos += packets[i].length;
      }
    }
    if (!session) {
      console.log("no session" , session, "err", err, "id", options.id);
      return;
    }
    session.id = options.id;
    session.ps = ps;
    delete session.fs;

    if (options.tags) {
      tags = options.tags.replace(/[^-a-zA-Z0-9_:,]/g, "").split(",");
      if (!session.ta) {
        session.ta = [];
      }
      session.ta = session.ta.concat(tags);
    }

    var molochClusters = Config.configMap("moloch-clusters");
    if (!molochClusters) {
      console.log("ERROR - sendSession is not configured");
      return cb();
    }

    var sobj = molochClusters[options.cluster];
    if (!sobj) {
      console.log("ERROR - moloch-clusters is not configured for " + options.cluster);
      return cb();
    }

    var info = url.parse(sobj.url + "/receiveSession?saveId=" + options.saveId);
    addAuth(info, options.user, options.nodeName, sobj.serverSecret || sobj.passwordSecret);
    info.method = "POST";

    var result = "";
    var client = info.protocol === "https:"?https:http;
    info.agent = (client === http?internals.httpAgent:internals.httpsAgent);
    addCaTrust(info, options.nodeName);
    var preq = client.request(info, function(pres) {
      pres.on('data', function (chunk) {
        result += chunk;
      });
      pres.on('end', function () {
        result = JSON.parse(result);
        if (!result.success) {
          console.log("ERROR sending session ", result);
        }
        cb();
      });
    });

    preq.on('error', function (e) {
      console.log("ERROR - Couldn't connect to ", info, "\nerror=", e);
      cb();
    });

    var sessionStr = JSON.stringify(session);
    var b = new Buffer(12);
    b.writeUInt32BE(Buffer.byteLength(sessionStr), 0);
    b.writeUInt32BE(buffer.length, 8);
    preq.write(b);
    preq.write(sessionStr);
    preq.write(buffer);
    preq.end();
  }, undefined, 10);
}

internals.sendSessionQueue = async.queue(sendSessionWorker, 10);

app.get('/:nodeName/sendSession/:id', checkProxyRequest, function(req, res) {
  noCache(req, res);
  res.statusCode = 200;

  var options = {
    user: req.user,
    cluster: req.query.cluster,
    id: req.params.id,
    saveId: req.query.saveId,
    tags: req.query.tags,
    nodeName: req.params.nodeName
  };

  internals.sendSessionQueue.push(options, function () {
    res.end();
  });
});

app.post('/:nodeName/sendSessions', checkProxyRequest, function(req, res) {
  noCache(req, res);
  res.statusCode = 200;

  if (req.body.ids === undefined ||
      req.query.cluster === undefined ||
      req.query.saveId === undefined ||
      req.query.tags === undefined) {
    return res.end();
  }

  var count = 0;
  var ids = queryValueToArray(req.body.ids);
  ids.forEach(function(id) {
    var options = {
      user: req.user,
      cluster: req.query.cluster,
      id: id,
      saveId: req.query.saveId,
      tags: req.query.tags,
      nodeName: req.params.nodeName
    };

    count++;
    internals.sendSessionQueue.push(options, function () {
      count--;
      if (count === 0) {
        return res.end();
      }
    });
  });
});


function sendSessionsList(req, res, list) {
  if (!list) {
    return res.end(JSON.stringify({success: false, text: "Missing list of sessions"}));
  }

  var saveId = Config.nodeName() + "-" + new Date().getTime().toString(36);

  async.eachLimit(list, 10, function(item, nextCb) {
    var fields = item._source || item.fields;
    isLocalView(fields.no, function () {
      var options = {
        user: req.user,
        cluster: req.body.cluster,
        id: item._id,
        saveId: saveId,
        tags: req.query.tags,
        nodeName: fields.no
      };
      // Get from our DISK
      internals.sendSessionQueue.push(options, nextCb);
    },
    function () {
      // Get from remote DISK
      getViewUrl(fields.no, function(err, viewUrl, client) {
        var info = url.parse(viewUrl);
        info.path = Config.basePath(fields.no) + fields.no + "/sendSession/" + item._id + "?saveId=" + saveId + "&cluster=" + req.body.cluster;
        info.agent = (client === http?internals.httpAgent:internals.httpsAgent);
        if (req.query.tags) {
          info.path += "&tags=" + req.query.tags;
        }
        addAuth(info, req.user, fields.no);
        addCaTrust(info, fields.no);
        var preq = client.request(info, function(pres) {
          pres.on('data', function (chunk) {
          });
          pres.on('end', function () {
            setImmediate(nextCb);
          });
        });
        preq.on('error', function (e) {
          console.log("ERROR - Couldn't proxy sendSession request=", info, "\nerror=", e);
          nextCb(null);
        });
        preq.end();
      });
    });
  }, function(err) {
    return res.end(JSON.stringify({success: true, text: "Sending of " + list.length + " sessions complete"}));
  });
}

var qlworking = {};
function sendSessionsListQL(pOptions, list, nextQLCb) {
  if (!list) {
    return;
  }

  var nodes = {};

  list.forEach(function (item) {
    if (!nodes[item.no]) {
      nodes[item.no] = [];
    }
    nodes[item.no].push(item.id);
  });

  var keys = Object.keys(nodes);

  var count = 0;
  async.eachLimit(keys, 15, function(node, nextCb) {
    isLocalView(node, function () {
      var sent = 0;
      nodes[node].forEach(function(item) {
        var options = {
          id: item,
          nodeName: node
        };
        Db.merge(options, pOptions);

        // Get from our DISK
        internals.sendSessionQueue.push(options, function () {
          sent++;
          if (sent === nodes[node].length) {
            nextCb();
          }
        });
      });
    },
    function () {
      // Get from remote DISK
      getViewUrl(node, function(err, viewUrl, client) {
        var info = url.parse(viewUrl);
        info.method = "POST";
        info.path = Config.basePath(node) + node + "/sendSessions?saveId=" + pOptions.saveId + "&cluster=" + pOptions.cluster;
        info.agent = (client === http?internals.httpAgent:internals.httpsAgent);
        if (pOptions.tags) {
          info.path += "&tags=" + pOptions.tags;
        }
        addAuth(info, pOptions.user, node);
        addCaTrust(info, node);
        var preq = client.request(info, function(pres) {
          pres.on('data', function (chunk) {
            qlworking[info.path] = "data";
          });
          pres.on('end', function () {
            delete qlworking[info.path];
            count++;
            setImmediate(nextCb);
          });
        });
        preq.on('error', function (e) {
          delete qlworking[info.path];
          console.log("ERROR - Couldn't proxy sendSession request=", info, "\nerror=", e);
          setImmediate(nextCb);
        });
        preq.setHeader('content-type', "application/x-www-form-urlencoded");
        preq.write("ids=");
        preq.write(nodes[node].join(","));
        preq.end();
        qlworking[info.path] = "sent";
      });
    });
  }, function(err) {
    nextQLCb();
  });
}

app.post('/receiveSession', function receiveSession(req, res) {
  if (!req.query.saveId) {
    return res.send({success: false, text: "Missing saveId"});
  }

  // JS Static Variable :)
  receiveSession.saveIds = receiveSession.saveIds || {};

  var saveId = receiveSession.saveIds[req.query.saveId];
  if (!saveId) {
    saveId = receiveSession.saveIds[req.query.saveId] = {start: 0};
  }

  var sessionlen = -1;
  var filelen = -1;
  var written = 0;
  var session = null;
  var buffer;
  var file;
  var writeHeader;

  function makeFilename(cb) {
    if (saveId.filename) {
      return cb(saveId.filename);
    }

    // Just keep calling ourselves every 100 ms until we have a filename
    if (saveId.inProgress) {
      return setTimeout(makeFilename, 100, cb);
    }

    saveId.inProgress = 1;
    Db.getSequenceNumber("fn-" + Config.nodeName(), function (err, seq) {
      var filename = Config.get("pcapDir") + "/" + Config.nodeName() + "-" + seq + "-" + req.query.saveId + ".pcap";
      saveId.seq      = seq;
      Db.indexNow("files", "file", Config.nodeName() + "-" + saveId.seq, {num: saveId.seq, name: filename, first: session.fp, node: Config.nodeName(), filesize: -1, locked: 1}, function() {
        cb(filename);
        saveId.filename = filename; // Don't set the saveId.filename until after the first request completes its callback.
      });
    });
  }

  function saveSession() {
    function tags(container, field, prefix, cb) {
      if (!container[field]) {
        return cb(null);
      }

      mapTags(session[field], prefix, function (err, tagIds) {
        session[field] = tagIds;
        cb(null);
      });
    }

    async.parallel([
      function(parallelCb) {
        tags(session, "ta", "", parallelCb);
      },
      function(parallelCb) {
        tags(session, "hh1", "http:header:", parallelCb);
      },
      function(parallelCb) {
        tags(session, "hh2", "http:header:", parallelCb);
      }],
      function() {
        var id = session.id;
        delete session.id;
        Db.indexNow(Db.id2Index(id), "session", id, session, function(err, info) {
        });
      }
    );
  }

  function chunkWrite(chunk) {
    // Write full chunk if first packet and writeHeader or not first packet
    if (writeHeader || written !== 0) {
      writeHeader = false;
      file.write(chunk);
    } else {
      file.write(chunk.slice(24));
    }
    written += chunk.length; // Pretend we wrote it all
  }

  req.on('data', function(chunk) {
    // If the file is open, just write the current chunk
    if (file) {
      return chunkWrite(chunk);
    }

    // If no file is open, then save the current chunk to the end of the buffer.
    if (!buffer) {
      buffer = chunk;
    } else {
      buffer = Buffer.concat([buffer, chunk]);
    }

    // Found the lengths
    if (sessionlen === -1 && (buffer.length >= 12)) {
      sessionlen = buffer.readUInt32BE(0);
      filelen    = buffer.readUInt32BE(8);
      buffer = buffer.slice(12);
    }

    // If we know the session len and haven't read the session
    if (sessionlen !== -1 && !session && buffer.length >= sessionlen) {
      session = JSON.parse(buffer.toString("utf8", 0, sessionlen));
      session.no = Config.nodeName();
      buffer = buffer.slice(sessionlen);

      if (filelen > 0) {
        req.pause();

        makeFilename(function (filename) {
          req.resume();
          session.ps[0] = - saveId.seq;
          session.fs = [saveId.seq];

          if (saveId.start === 0) {
            file = fs.createWriteStream(filename, {flags: "w"});
          } else {
            file = fs.createWriteStream(filename, {start: saveId.start, flags: "r+"});
          }
          writeHeader = saveId.start === 0;

          // Adjust packet location based on where we start writing
          if (saveId.start > 0) {
            for (var p = 1, plen = session.ps.length; p < plen; p++) {
              session.ps[p] += (saveId.start - 24);
            }
          }

          // Filelen always includes header, if we don't write header subtract it
          saveId.start += filelen;
          if (!writeHeader) {
            saveId.start -= 24;
          }

          // Still more data in buffer, start of pcap
          if (buffer.length > 0) {
            chunkWrite(buffer);
          }

          saveSession();
        });
      } else {
        saveSession();
      }
    }
  });

  req.on('end', function(chunk) {
    if (file) {
      file.end();
    }
    return res.send({success: true});
  });
});

app.post('/sendSessions', function(req, res) {
  if (req.body.ids) {
    var ids = queryValueToArray(req.body.ids);

    sessionsListFromIds(req, ids, ["no"], function(err, list) {
      sendSessionsList(req, res, list);
    });
  } else {
    sessionsListFromQuery(req, res, ["no"], function(err, list) {
      sendSessionsList(req, res, list);
    });
  }
});

app.post('/upload', multer({dest:'/tmp'}).single('file'), function (req, res) {
  var exec = require('child_process').exec,
     child;

  var tags = "";
  if (req.body.tag) {
    var t = req.body.tag.replace(/[^-a-zA-Z0-9_:,]/g, "").split(",");
    t.forEach(function(tag) {
      if (tag.length > 0) {
        tags += " --tag " + tag;
      }
    });
  }

  var cmd = Config.get("uploadCommand")
     .replace("{TAGS}", tags)
     .replace("{NODE}", Config.nodeName())
     .replace("{TMPFILE}", req.file.path)
     .replace("{CONFIG}", Config.getConfigFile());
  console.log("upload command: ", cmd);
  child = exec(cmd, function (error, stdout, stderr) {
    res.write("<b>" + cmd + "</b><br>");
    res.write("<pre>");
    res.write(stdout);
    res.end("</pre>");
    if (error !== null) {
      console.log("exec error: " + error);
    }
    fs.unlink(req.file.path);
  });
});

if (Config.get("regressionTests")) {
  app.post('/shutdown', function(req, res) {
    Db.close();
    process.exit(0);
    throw new Error("Exiting");
  });
  app.post('/flushCache', function(req, res) {
    Db.flushCache();
    res.send("{}");
  });
}

app.use('/cyberchef.htm', function(req, res) {
  res.sendFile('./public/cyberchef.htm');
});

/* cyberchef endpoint - loads the src or dst packets for a session and
 * sends them to cyberchef */
app.get("/:nodeName/session/:id/cyberchef", checkWebEnabled, checkProxyRequest, function(req, res) {
  processSessionIdAndDecode(req.params.id, 10000, function(err, session, results) {
    if (err) { return res.send("Error"); }

    let data = '';
    for (var i = (req.query.type !== 'dst'?0:1), ilen = results.length; i < ilen; i+=2) {
      data += results[i].data.toString('hex');
    }

    res.render('cyberchef.pug', { value: data });
  });
});


app.use(express.static(__dirname + '/views'));
app.use(express.static(__dirname + '/bundle'));
app.use(function (req, res) {
  if (req.path === '/users' && !req.user.createEnabled) {
    return res.status(403).send('Permission denied');
  }

  if (req.path === '/settings' && Config.get('demoMode', false)) {
    return res.status(403).send('Permission denied');
  }

  // send cookie for basic, non admin functions
  res.cookie(
     'MOLOCH-COOKIE',
     Config.obj2auth({date: Date.now(), pid: process.pid, userId: req.user.userId}),
     { path: app.locals.basePath }
  );

  var theme = req.user.settings.theme || 'default-theme';
  if (theme.startsWith('custom1')) { theme  = 'custom-theme'; }

  res.render('app.pug', {
    theme   : theme,
    demoMode: Config.get('demoMode', false),
    devMode : Config.get('devMode', false),
    version : app.locals.molochversion
  });
});


//////////////////////////////////////////////////////////////////////////////////
//// Cron Queries
//////////////////////////////////////////////////////////////////////////////////

/* Process a single cron query.  At max it will process 24 hours worth of data
 * to give other queries a chance to run.  It searches for the first time range
 * where there is an available index.
 */
function processCronQuery(cq, options, query, endTime, cb) {
  if (Config.debug > 2) {
    console.log("CRON", cq.name, cq.creator, "- processCronQuery(", cq, options, query, endTime, ")");
  }

  var singleEndTime;
  var count = 0;
  async.doWhilst(function(whilstCb) {
    // Process at most 24 hours
    singleEndTime = Math.min(endTime, cq.lpValue + 24*60*60);
    query.query.bool.filter[0] = {range: {lp: {gt: cq.lpValue, lte: singleEndTime}}};

    if (Config.debug > 2) {
      console.log("CRON", cq.name, cq.creator, "- start:", new Date(cq.lpValue*1000), "stop:", new Date(singleEndTime*1000), "end:", new Date(endTime*1000), "remaining runs:", ((endTime-singleEndTime)/(24*60*60.0)));
    }

    Db.getIndices(cq.lpValue, singleEndTime, Config.get("rotateIndex", "daily"), function(indices) {

      // There are no matching indices, continue while loop
      if (indices === "sessions-*") {
        cq.lpValue += 24*60*60;
        return setImmediate(whilstCb, null);
      }

      // We have found some indices, now scroll thru ES
      Db.search(indices, 'session', query, {scroll: '600s'}, function getMoreUntilDone(err, result) {
        function doNext() {
          count += result.hits.hits.length;

          // No more data, all done
          if (result.hits.hits.length === 0) {
            return setImmediate(whilstCb, "DONE");
          } else {
            var document = { doc: { count: (query.count || 0) + count} };
            Db.update("queries", "query", options.qid, document, {refresh: 1}, function () {});
          }

          Db.scroll({
            body: result._scroll_id,
            scroll: '600s'
          }, getMoreUntilDone);
        }

        if (err || result.error) {
          console.log("cronQuery error", err, (result?result.error:null), "for", cq);
          return setImmediate(whilstCb, "ERR");
        }

        var ids = [];
        var hits = result.hits.hits;
        var i, ilen;
        if (cq.action.indexOf("forward:") === 0) {
          for (i = 0, ilen = hits.length; i < ilen; i++) {
            ids.push({id: hits[i]._id, no: hits[i]._source.no});
          }

          sendSessionsListQL(options, ids, doNext);
        } else if (cq.action.indexOf("tag") === 0) {
          for (i = 0, ilen = hits.length; i < ilen; i++) {
            ids.push(hits[i]._id);
          }

          if (Config.debug > 1) {
            console.log("CRON", cq.name, cq.creator, "- Updating tags:", ids.length);
          }

          var tags = options.tags.split(",");
          mapTags(tags, "", function(err, tagIds) {
            sessionsListFromIds(null, ids, ["ta", "tags-term", "no"], function(err, list) {
              addTagsList(tagIds, tags, list, doNext);
            });
          });
        } else {
          console.log("Unknown action", cq);
          doNext();
        }
      });
    });
  }, function () {
    if (Config.debug > 1) {
      console.log("CRON", cq.name, cq.creator, "- Continue process", singleEndTime, endTime);
    }
    return singleEndTime !== endTime;
  }, function (err) {
    cb(count, singleEndTime);
  });
}

function processCronQueries() {
  if (internals.cronRunning) {
    console.log("processQueries already running", qlworking);
    return;
  }
  internals.cronRunning = true;
  if (Config.debug) {
    console.log("CRON - cronRunning set to true");
  }

  var repeat;
  async.doWhilst(function(whilstCb) {
    repeat = false;
    Db.search("queries", "query", {size: 1000}, function(err, data) {
      if (err) {
        internals.cronRunning = false;
        console.log("processCronQueries", err);
        return setImmediate(whilstCb, err);
      }
      var queries = {};
      data.hits.hits.forEach(function(item) {
        queries[item._id] = item._source;
      });

      // Delayed by the max Timeout
      var endTime = Math.floor(Date.now()/1000) - internals.cronTimeout;

      // Save incase reload happens while running
      var molochClusters = Config.configMap("moloch-clusters");

      // Go thru the queries, fetch the user, make the query
      async.eachSeries(Object.keys(queries), function (qid, forQueriesCb) {
        var cq = queries[qid];
        var cluster = null;
        var req, res;

        if (Config.debug > 1) {
          console.log("CRON - Running", qid, cq);
        }

        if (!cq.enabled || endTime < cq.lpValue) {
          return forQueriesCb();
        }

        if (cq.action.indexOf("forward:") === 0) {
          cluster = cq.action.substring(8);
        }

        Db.getUserCache(cq.creator, function(err, user) {
          if (err && !user) {return forQueriesCb();}
          if (!user || !user.found) {console.log("User", cq.creator, "doesn't exist"); return forQueriesCb(null);}
          if (!user._source.enabled) {console.log("User", cq.creator, "not enabled"); return forQueriesCb();}
          user = user._source;

          var options = {
            user: user,
            cluster: cluster,
            saveId: Config.nodeName() + "-" + new Date().getTime().toString(36),
            tags: cq.tags.replace(/[^-a-zA-Z0-9_:,]/g, ""),
            qid: qid
          };

          molochparser.parser.yy = {emailSearch: user.emailSearch === true,
                                      fieldsMap: Config.getFieldsMap()};

          var query = {from: 0,
                       size: 1000,
                       query: {bool: {filter: [{}]}},
                       _source: ["_id", "no"]
                      };

          try {
            query.query.bool.filter.push(molochparser.parse(cq.query));
          } catch (e) {
            console.log("Couldn't compile cron query expression", cq, e);
            return forQueriesCb();
          }

          if (user.expression && user.expression.length > 0) {
            try {
              // Expression was set by admin, so assume email search ok
              molochparser.parser.yy.emailSearch = true;
              var userExpression = molochparser.parse(user.expression);
              query.query.bool.filter.push(userExpression);
            } catch (e) {
              console.log("Couldn't compile user forced expression", user.expression, e);
              return forQueriesCb();
            }
          }

          lookupQueryItems(query.query.bool.filter, function (lerr) {
            processCronQuery(cq, options, query, endTime, function (count, lpValue) {
              if (Config.debug > 1) {
                console.log("CRON - setting lpValue", new Date(lpValue*1000));
              }
              // Do the ES update
              var document = {
                doc: {
                  lpValue: lpValue,
                  lastRun: Math.floor(Date.now()/1000),
                  count: (queries[qid].count || 0) + count
                }
              };
              Db.update("queries", "query", qid, document, {refresh: 1}, function () {
                // If there is more time to catch up on, repeat the loop, although other queries
                // will get processed first to be fair
                if (lpValue !== endTime) {
                  repeat = true;
                }
                return forQueriesCb();
              });
            });
          });
        });
      }, function(err) {
        if (Config.debug > 1) {
          console.log("CRON - Finished one pass of all crons");
        }
        return setImmediate(whilstCb, err);
      });
    });
  }, function () {
    if (Config.debug > 1) {
       console.log("CRON - Process again: ", repeat);
    }
    return repeat;
  }, function (err) {
    if (Config.debug) {
      console.log("CRON - Should be up to date");
    }
    internals.cronRunning = false;
  });
}

//////////////////////////////////////////////////////////////////////////////////
//// Main
//////////////////////////////////////////////////////////////////////////////////
function main () {
  Db.checkVersion(MIN_DB_VERSION, Config.get("passwordSecret") !== undefined);
  Db.healthCache(function(err, health) {
    internals.clusterName = health.cluster_name;
  });

  Db.nodesStats({metric: "fs"}, function (err, info) {
    info.nodes.timestamp = new Date().getTime();
    internals.previousNodeStats.push(info.nodes);
  });

  expireCheckAll();
  setInterval(expireCheckAll, 60*1000);

  loadFields();
  setInterval(loadFields, 2*60*1000);

  loadPlugins();

  createSessionDetail();
  setInterval(createSessionDetail, 5*60*1000);

  createRightClicks();
  setInterval(createRightClicks, 5*60*1000);

  if (Config.get("cronQueries", false)) {
    console.log("This node will process Cron Queries, delayed by", internals.cronTimeout, "seconds");
    setInterval(processCronQueries, 60*1000);
    setTimeout(processCronQueries, 1000);
  }

  var server;
  if (Config.isHTTPS()) {
    server = https.createServer({key: Config.keyFileData, cert: Config.certFileData}, app);
  } else {
    server = http.createServer(app);
  }

  var viewHost = Config.get("viewHost", undefined);
  if (internals.userNameHeader !== undefined && viewHost !== "localhost" && viewHost !== "127.0.0.1") {
    console.log("SECURITY WARNING - when userNameHeader is set, viewHost should be localhost or use iptables");
  }

  server
    .on('error', function (e) {
      console.log("ERROR - couldn't listen on port", Config.get("viewPort", "8005"), "is viewer already running?");
      process.exit(1);
      throw new Error("Exiting");
    })
    .on('listening', function (e) {
      console.log("Express server listening on port %d in %s mode", server.address().port, app.settings.env);
    })
    .listen(Config.get("viewPort", "8005"), viewHost);
}
//////////////////////////////////////////////////////////////////////////////////
//// DB
//////////////////////////////////////////////////////////////////////////////////
Db.initialize({host: internals.elasticBase,
               prefix: Config.get("prefix", ""),
               usersHost: Config.get("usersElasticsearch"),
               usersPrefix: Config.get("usersPrefix"),
               nodeName: Config.nodeName(),
               dontMapTags: Config.get("multiES", false)}, main);
