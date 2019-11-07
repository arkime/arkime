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

const MIN_DB_VERSION = 62;

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
    Db             = require('./db.js'),
    molochparser   = require('./molochparser.js'),
    passport       = require('passport'),
    DigestStrategy = require('passport-http').DigestStrategy,
    molochversion  = require('./version'),
    http           = require('http'),
    pug            = require('pug'),
    https          = require('https'),
    EventEmitter   = require('events').EventEmitter,
    PNG            = require('pngjs').PNG,
    decode         = require('./decode.js'),
    onHeaders      = require('on-headers'),
    glob           = require('glob'),
    unzip          = require('unzip'),
    helmet         = require('helmet'),
    uuid           = require('uuidv4').default;
} catch (e) {
  console.log ("ERROR - Couldn't load some dependancies, maybe need to 'npm update' inside viewer directory", e);
  process.exit(1);
  throw new Error("Exiting");
}

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
  CYBERCHEFVERSION: '9.4.0',
  elasticBase: Config.get("elasticsearch", "http://localhost:9200").split(","),
  esQueryTimeout: Config.get("elasticsearchTimeout", 300) + 's',
  userNameHeader: Config.get("userNameHeader"),
  httpAgent:   new http.Agent({keepAlive: true, keepAliveMsecs:5000, maxSockets: 40}),
  httpsAgent:  new https.Agent({keepAlive: true, keepAliveMsecs:5000, maxSockets: 40, rejectUnauthorized: !Config.insecure}),
  previousNodesStats: [],
  caTrustCerts: {},
  cronRunning: false,
  rightClicks: {},
  pluginEmitter: new EventEmitter(),
  writers: {},
  oldDBFields: {},
  isLocalViewRegExp: Config.get("isLocalViewRegExp")?new RegExp(Config.get("isLocalViewRegExp")):undefined,
  uploadLimits: {
  },

  cronTimeout: +Config.get("dbFlushTimeout", 5) + // How long capture holds items
               60 +                               // How long before ES reindexs
               20,                                // Transmit and extra time

//http://garethrees.org/2007/11/14/pngcrush/
  emptyPNG: Buffer.from("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAACklEQVR4nGMAAQAABQABDQottAAAAABJRU5ErkJggg==", 'base64'),
  PNG_LINE_WIDTH: 256,
  runningHuntJob: undefined,
  proccessHuntJobsInitialized: false,
  notifiers: undefined,
  prefix: Config.get('prefix', ''),
  lookupTypeMap: {
    ip: 'ip',
    integer: 'number',
    termfield: 'string',
    uptermfield: 'string',
    lotermfield: 'string'
  }
};

// make sure there's an _ after the prefix
if (internals.prefix && !internals.prefix.endsWith('_')) {
  internals.prefix = `${internals.prefix}_`;
}

if (Config.get("uploadFileSizeLimit")) {
  internals.uploadLimits.fileSize = parseInt(Config.get("uploadFileSizeLimit"));
}

if (internals.elasticBase[0].lastIndexOf('http', 0) !== 0) {
  internals.elasticBase[0] = "http://" + internals.elasticBase[0];
}

function isProduction() {
  return app.get('env') === 'production';
}

function userCleanup(suser) {
  suser.settings = suser.settings || {};
  if (suser.emailSearch === undefined) { suser.emailSearch = false; }
  if (suser.removeEnabled === undefined) { suser.removeEnabled = false; }
  // if multies and not users elasticsearch, disable admin privelages
  if (Config.get('multiES', false) && !Config.get('usersElasticsearch')) {
    suser.createEnabled = false;
  }
  let now = Date.now();
  let timespan = Config.get('regressionTests', false) ? 1 : 60000;
  // update user lastUsed time if not mutiES and it hasn't been udpated in more than a minute
  if (!Config.get('multiES', false) && (!suser.lastUsed || (now - suser.lastUsed) > timespan)) {
    suser.lastUsed = now;
    Db.setUser(suser.userId, suser, function (err, info) {
      if (err) {
        console.log('user lastUsed update error', err, info);
      }
    });
  }
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

const iframeOption = Config.get('iframe', 'deny');
if (iframeOption === 'sameorigin' || iframeOption === 'deny') {
  app.use(helmet.frameguard({ action: iframeOption }));
} else {
  app.use(helmet.frameguard({
    action: 'allow-from',
    domain: iframeOption
  }));
}

app.use(helmet.hidePoweredBy());
app.use(helmet.xssFilter());
if (Config.get('hstsHeader', false) && Config.isHTTPS()) {
  app.use(helmet.hsts({
    maxAge: 31536000,
    includeSubDomains: true
  }));
}
// calculate nonce
app.use((req, res, next) => {
  res.locals.nonce = Buffer.from(uuid()).toString('base64');
  next();
});
// define csp headers
const cspHeader = helmet.contentSecurityPolicy({
  directives: {
    defaultSrc: ["'self'"],
    /* can remove unsafe-inline for css when this is fixed
    https://github.com/vuejs/vue-style-loader/issues/33 */
    styleSrc: ["'self'", "'unsafe-inline'"],
    scriptSrc: ["'self'", "'unsafe-eval'", (req, res) => `'nonce-${res.locals.nonce}'`],
    objectSrc: ["'none'"],
    imgSrc: ["'self'", 'data:']
  }
});
const unsafeInlineCspHeader = helmet.contentSecurityPolicy({
  directives: {
    defaultSrc: ["'self'"],
    styleSrc: ["'self'", "'unsafe-inline'"],
    scriptSrc: ["'self'", "'unsafe-eval'", "'unsafe-inline'"],
    objectSrc: ["'self'", 'data:'],
    workerSrc: ["'self'", 'data:', 'blob:'],
    imgSrc: ["'self'", 'data:'],
    fontSrc: ["'self'", 'data:']
  }
});

function molochError (status, text) {
  /* jshint validthis: true */
  this.status(status || 403);
  return this.send(JSON.stringify({ success: false, text: text }));
}

app.use(function(req, res, next) {
  res.molochError = molochError;

  if (res.setTimeout) {
    res.setTimeout(10 * 60 * 1000); // Increase default from 2 min to 10 min
  }

  req.url = req.url.replace(Config.basePath(), "/");
  return next();
});
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ limit: "5mb", extended: true }));
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


app.use('/font-awesome', express.static(__dirname + '/../node_modules/font-awesome', { maxAge: 600 * 1000}));
app.use('/bootstrap', express.static(__dirname + '/node_modules/bootstrap', { maxAge: 600 * 1000}));

app.use("/", express.static(__dirname + '/public', { maxAge: 600 * 1000}));

if (Config.get("passwordSecret")) {
  app.locals.alwaysShowESStatus = false;
  app.use(function(req, res, next) {
    // 200 for NS
    if (req.url === "/_ns_/nstest.html") {
      return res.end();
    }

    // No auth for eshealth.json or parliament.json
    if (req.url.match(/^\/(parliament|eshealth).json/)) {
      return next();
    }

    // S2S Auth
    if (req.headers['x-moloch-auth']) {
      var obj = Config.auth2obj(req.headers['x-moloch-auth'], false);
      obj.path = obj.path.replace(Config.basePath(), "/");
      if (obj.path !== req.url) {
        console.log("ERROR - mismatch url", obj.path, req.url);
        return res.send("Unauthorized based on bad url, check logs on ", Config.hostName());
      }
      if (Math.abs(Date.now() - obj.date) > 120000) { // Request has to be +- 2 minutes
        console.log("ERROR - Denying server to server based on timestamp, are clocks out of sync?", Date.now(), obj.date);
        return res.send("Unauthorized based on timestamp - check that all moloch viewer machines have accurate clocks");
      }

      if (req.url.match(/^\/receiveSession/)) {
        return next();
      }

      Db.getUserCache(obj.user, function(err, suser) {
        if (err) {return res.send("ERROR - x-moloch getUser - user: " + obj.user + " err:" + err);}
        if (!suser || !suser.found) {return res.send(obj.user + " doesn't exist");}
        if (!suser._source.enabled) {return res.send(obj.user + " not enabled");}
        userCleanup(suser._source);
        req.user = suser._source;
        return next();
      });
      return;
    }

    // Header auth
    if (internals.userNameHeader !== undefined) {
      if (req.headers[internals.userNameHeader] !== undefined) {
        var userName = req.headers[internals.userNameHeader];
        Db.getUserCache(userName, function(err, suser) {
          if (err) {return res.send("ERROR - getUser - user: " + userName + " err:" + err);}
          if (!suser || !suser.found) {return res.send(userName + " doesn't exist");}
          if (!suser._source.enabled) {return res.send(userName + " not enabled");}
          if (!suser._source.headerAuthEnabled) {return res.send(userName + " header auth not enabled");}

          userCleanup(suser._source);
          req.user = suser._source;
          return next();
        });
        return;
      } else if (Config.debug) {
        console.log("DEBUG - Couldn't find userNameHeader of", internals.userNameHeader, "in", req.headers, "for", req.url);
      }
    }

    // Browser auth
    req.url = req.url.replace("/", Config.basePath());
    passport.authenticate('digest', {session: false})(req, res, function (err) {
      req.url = req.url.replace(Config.basePath(), "/");
      if (err) { return res.molochError(200, err); }
      else { return next(); }
    });
  });
} else if (Config.get("regressionTests", false)) {
  console.log('WARNING - The setting "regressionTests" is set to true, do NOT use in production, for testing only');
  app.locals.alwaysShowESStatus = true;
  app.locals.noPasswordSecret   = true;
  app.use(function(req, res, next) {
    var username = req.query.molochRegressionUser || "anonymous";
    req.user = {userId: username, enabled: true, createEnabled: username === "anonymous", webEnabled: true, headerAuthEnabled: false, emailSearch: true, removeEnabled: true, packetSearch: true, settings: {}, welcomeMsgNum: 1};
    Db.getUserCache(username, function(err, suser) {
      if (!err && suser && suser.found) {
        userCleanup(suser._source);
        req.user = suser._source;
      }
      next();
    });
  });
} else {
  /* Shared password isn't set, who cares about auth, db is only used for settings */
  console.log('WARNING - The setting "passwordSecret" is not set, all access is anonymous');
  app.locals.alwaysShowESStatus = true;
  app.locals.noPasswordSecret   = true;
  app.use(function(req, res, next) {
    req.user = {userId: "anonymous", enabled: true, createEnabled: false, webEnabled: true, headerAuthEnabled: false, emailSearch: true, removeEnabled: true, packetSearch: true, settings: {}, welcomeMsgNum: 1};
    Db.getUserCache("anonymous", function(err, suser) {
        if (!err && suser && suser.found) {
          req.user.settings = suser._source.settings || {};
          req.user.views = suser._source.views;
        }
      next();
    });
  });
}

// add lookups for queries
app.use(function (req, res, next) {
  if (!req.user) { return next(); }
  Db.getLookupsCache(req.user.userId, (err, lookupsMap) => {
    req.lookups = lookupsMap || {};
    return next();
  });
});

app.use(function(req, res, next) {
  if (!req.user || !req.user.userId) {
    return next();
  }

  var mrc = {};

  mrc.httpAuthorizationDecode = {fields: "http.authorization", func: `{
    if (value.substring(0,5) === "Basic")
      return {name: "Decoded:", value: atob(value.substring(6))};
    return undefined;
  }`};
  mrc.bodyHashMd5 = {category: "md5", url: "/%NODE%/%ID%/bodyHash/%TEXT%", name: "Download File"};
  mrc.bodyHashSha256 = {category: "sha256", url: "/%NODE%/%ID%/bodyHash/%TEXT%", name: "Download File"};

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

// Explicit sigint handler for running under docker
// See https://github.com/nodejs/node/issues/4182
process.on('SIGINT', function() {
    process.exit();
});

function loadFields() {
  Db.loadFields(function (err, data) {
    if (err) {data = [];}
    else {data = data.hits.hits;}

    // Everything will use dbField2 as dbField
    for (let i = 0, ilen = data.length; i < ilen; i++) {
      internals.oldDBFields[data[i]._source.dbField] = data[i]._source;
      data[i]._source.dbField = data[i]._source.dbField2;
      if (data[i]._source.portField2) {
        data[i]._source.portField = data[i]._source.portField2;
      } else {
        delete data[i]._source.portField;
      }
      delete data[i]._source.rawField;
    }
    Config.loadFields(data);
    app.locals.fieldsMap = JSON.stringify(Config.getFieldsMap());
    app.locals.fieldsArr = Config.getFields().sort(function(a,b) {return (a.exp > b.exp?1:-1);});
    createSessionDetail();
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
function safeStr(str) {
  return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/\"/g,'&quot;').replace(/\'/g, '&#39;').replace(/\//g, '&#47;');
}

// https://medium.com/dailyjs/rewriting-javascript-converting-an-array-of-objects-to-an-object-ec579cafbfc7
function arrayToObject(array, key)
{
  return array.reduce((obj, item) => {
    obj[item[key]] = item;
      return obj;
  }, {});
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

function parseCustomView(key, input) {
  var fieldsMap = Config.getFieldsMap();

  var match = input.match(/require:([^;]+)/);
  if (!match) {
    console.log(`custom-view ${key} missing require section`);
    process.exit(1);
  }
  var require = match[1];

  match = input.match(/title:([^;]+)/);
  var title = match[1] || key;

  match = input.match(/fields:([^;]+)/);
  if (!match) {
    console.log(`custom-view ${key} missing fields section`);
    process.exit(1);
  }
  var fields = match[1];

  var output = `  if (session.${require})\n    div.sessionDetailMeta.bold ${title}\n    dl.sessionDetailMeta\n`;

  for (let field of fields.split(",")) {
    let info = fieldsMap[field];
    if (!info) {
      continue;
    }
    var parts = splitRemain(info.dbField, '.', 1);
    if (parts.length === 1) {
      output += `      +arrayList(session, '${parts[0]}', '${info.friendlyName}', '${field}')\n`;
    } else {
      output += `      +arrayList(session.${parts[0]}, '${parts[1]}', '${info.friendlyName}', '${field}')\n`;
    }
  }

  return output;
}

function createSessionDetail() {
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

  var customViews = Config.keys("custom-views") || [];

  for (let key of customViews) {
    let view = Config.sectionGet("custom-views", key);
    found[key] = parseCustomView(key, view);
  }

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
                                 "div.session-detail(sessionid=session.id,hidePackets=hidePackets)\n" +
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

function createRightClicks() {

  var mrc = Config.configMap("right-click");
  for (var key in mrc) {
    if (mrc[key].fields) {
      mrc[key].fields = mrc[key].fields.split(",");
    }
    if (mrc[key].users) {
      var users = {};
      for (const item of mrc[key].users.split(",")) {
        users[item] = 1;
      }
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
                                                    }, false, secret);
}

function loadCaTrust(node) {
  var caTrustFile = Config.getFull(node, "caTrustFile");

  if (caTrustFile && caTrustFile.length > 0) {
    let certs = [];

    var caTrustFileLines = fs.readFileSync(caTrustFile, 'utf8');
    caTrustFileLines = caTrustFileLines.split("\n");

    var foundCert = [];

    for (let i = 0, ilen = caTrustFileLines.length; i < ilen; i++) {
      let line = caTrustFileLines[i];
      if (line.length === 0) {
        continue;
      }
      foundCert.push(line);
      if (line.match(/-END CERTIFICATE-/)) {
        certs.push(foundCert.join("\n"));
        foundCert = [];
      }
    }

    if (certs.length > 0) {
      return certs;
    }
  }

  return undefined;
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

  internals.caTrustCerts[node] = loadCaTrust(node);

  if (internals.caTrustCerts[node] !== undefined && internals.caTrustCerts[node].length > 0) {
    info.ca = internals.caTrustCerts[node];
    info.agent.options.ca = internals.caTrustCerts[node];
    return;
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
    if (Config.debug > 1) {
      console.log(`DEBUG: node:${node} is using ${url} because viewUrl was set for ${node} in config file`);
    }
    cb(null, url, url.slice(0, 5) === "https"?https:http);
    return;
  }

  Db.molochNodeStatsCache(node, function(err, stat) {
    if (err) {
      return cb(err);
    }

    if (Config.debug > 1) {
      console.log(`DEBUG: node:${node} is using ${stat.hostname} from elasticsearch stats index`);
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
      console.log("ERROR - getViewUrl - node:", req.params.nodeName, "err:", err);
      return res.send(`Can't find view url for '${safeStr(req.params.nodeName)}' check viewer logs on '${Config.hostName()}'`);
    }
    var info = url.parse(viewUrl);
    info.path = req.url;
    info.agent = (client === http?internals.httpAgent:internals.httpsAgent);
    addAuth(info, req.user, req.params.nodeName);
    addCaTrust(info, req.params.nodeName);

    var preq = client.request(info, function(pres) {
      if (pres.headers['content-type']) {
        res.setHeader('content-type', pres.headers['content-type']);
      }
      if (pres.headers['content-disposition']) {
        res.setHeader('content-disposition', pres.headers['content-disposition']);
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
      console.log("ERROR - Couldn't proxy request=", info, "\nerror=", e, "You might want to run viewer with two --debug for more info");
      res.send(`Error talking to node '${safeStr(req.params.nodeName)}' using host '${info.host}' check viewer logs on '${Config.hostName()}'`);
    });
    preq.end();
  });
}

function makeRequest (node, path, user, cb) {
  getViewUrl(node, function (err, viewUrl, client) {
    let info = url.parse(viewUrl);
    info.path = encodeURI(`${Config.basePath(node)}${path}`);
    info.agent = (client === http ? internals.httpAgent : internals.httpsAgent);
    addAuth(info, user, node);
    addCaTrust(info, node);
    let preq = client.request(info, function (pres) {
      let response = '';
      pres.on('data', function (chunk) {
        response += chunk;
      });
      pres.on('end', function () {
        cb(null, response);
      });
    });
    preq.on('error', function (err) {
      console.log(`Error with ${info.path} on remote viewer: ${err}`);
      cb(err);
    });
    preq.end();
  });
}

function isLocalView (node, yesCb, noCb) {
  if (internals.isLocalViewRegExp && node.match(internals.isLocalViewRegExp)) {
    if (Config.debug > 1) {
      console.log(`DEBUG: node:${node} is local view because matches ${internals.isLocalViewRegExp}`);
    }
    return yesCb();
  }

  var pcapWriteMethod = Config.getFull(node, "pcapWriteMethod");
  var writer = internals.writers[pcapWriteMethod];
  if (writer && writer.localNode === false) {
    if (Config.debug > 1) {
      console.log(`DEBUG: node:${node} is local view because of writer`);
    }
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

function checkCookieToken(req, res, next) {
  if (!req.headers['x-moloch-cookie']) {
    return res.molochError(500, 'Missing token');
  }

  req.token = Config.auth2obj(req.headers['x-moloch-cookie'], true);
  var diff = Math.abs(Date.now() - req.token.date);
  if (diff > 2400000 || /* req.token.pid !== process.pid || */
      req.token.userId !== req.user.userId) {

    console.trace('bad token', req.token);
    return res.molochError(500, 'Timeout - Please try reloading page and repeating the action');
  }

  return next();
}

function checkPermissions (permissions) {
  const inversePermissions = {
    hidePcap: true,
    hideFiles: true,
    hideStats: true,
    disablePcapDownload: true
  };

  return (req, res, next) => {
    for (let permission of permissions) {
      if ((!req.user[permission] && !inversePermissions[permission]) ||
        (req.user[permission] && inversePermissions[permission])) {
        console.log(`Permission denied to ${req.user.userId} while requesting resource: ${req._parsedUrl.pathname}, using permission ${permission}`);
        return res.molochError(403, 'You do not have permission to access this resource');
      }
    }
    next();
  };
}

function checkHuntAccess (req, res, next) {
  if (req.user.createEnabled) {
    // an admin can do anything to any hunt
    return next();
  } else {
    Db.get('hunts', 'hunt', req.params.id, (err, huntHit) => {
      if (err) {
        console.log('error', err);
        return res.molochError(500, err);
      }
      if (!huntHit || !huntHit.found) { throw 'Hunt not found'; }

      if (huntHit._source.userId === req.user.userId) {
        return next();
      }
      return res.molochError(403, `You cannot change another user's hunt unless you have admin privileges`);
    });
  }
}

function checkCronAccess (req, res, next) {
  if (req.user.createEnabled) {
    // an admin can do anything to any query
    return next();
  } else {
    Db.get('queries', 'query', req.body.key, (err, query) => {
      if (err || !query.found) {
        return res.molochError(403, 'Unknown cron query');
      }
      if (query._source.creator === req.user.userId) {
        return next();
      }
      return res.molochError(403, `You cannot change another user's cron query unless you have admin privileges`);
    });
  }
}

function noCacheJson(req, res, next) {
  res.header('Cache-Control', 'no-cache, private, no-store, must-revalidate, max-stale=0, post-check=0, pre-check=0');
  res.setHeader("Content-Type", 'application/json');
  return next();
}

function logAction(uiPage) {
  return function(req, res, next) {
    var log = {
      timestamp : Math.floor(Date.now()/1000),
      method    : req.method,
      userId    : req.user.userId,
      api       : req._parsedUrl.pathname,
      query     : req._parsedUrl.query,
      expression: req.query.expression
    };

    if (req.user.expression) {
      log.forcedExpression = req.user.expression;
    }

    if (uiPage) { log.uiPage = uiPage; }

    if (req.query.date && parseInt(req.query.date) === -1) {
      log.range = log.timestamp;
    } else if(req.query.startTime && req.query.stopTime) {
      log.range = req.query.stopTime - req.query.startTime;
    }

    if (req.query.view && req.user.views) {
      var view = req.user.views[req.query.view];
      if (view) {
        log.view = {
          name: req.query.view,
          expression: view.expression
        };
      }
    }

    // save the request body
    var avoidProps  = { password:true, newPassword:true, currentPassword:true };
    var bodyClone   = {};

    for (var key in req.body) {
      if (req.body.hasOwnProperty(key) && !avoidProps[key]) {
        bodyClone[key] = req.body[key];
      }
    }

    if (Object.keys(bodyClone).length > 0) {
      log.body = bodyClone;
    }

    res.logCounts = function(recordsReturned, recordsFiltered, recordsTotal) {
      log.recordsReturned = recordsReturned;
      log.recordsFiltered = recordsFiltered;
      log.recordsTotal    = recordsTotal;
    };

    req._molochStartTime = new Date();
    function finish () {
      log.queryTime = new Date() - req._molochStartTime;
      res.removeListener('finish', finish);
      Db.historyIt(log, function(err, info) {
        if (err) { console.log('log history error', err, info); }
      });
    }

    res.on('finish', finish);

    return next();
  };
}

function fieldToExp (req, res, next) {
  if (req.query.exp && !req.query.field) {
    var field = Config.getFieldsMap()[req.query.exp];
    if (field) { req.query.field = field.dbField; }
    else { req.query.field = req.query.exp; }
  }

  return next();
}

// record the time it took from the request to start
// until the headers are set to send the response
function recordResponseTime (req, res, next) {
  onHeaders(res, () => {
    let now = process.hrtime();
    let ms = ((now[0] - req._startAt[0]) * 1000) + ((now[1] - req._startAt[1]) / 1000000);
    ms = Math.ceil(ms);
    res.setHeader('X-Moloch-Response-Time', ms);
  });

  next();
}


//////////////////////////////////////////////////////////////////////////////////
//// Pages
//////////////////////////////////////////////////////////////////////////////////
// APIs disabled in demoMode, needs to be before real callbacks
if (Config.get('demoMode', false)) {
  console.log("WARNING - Starting in demo mode, some APIs disabled");
  app.all(['/settings', '/users', '/history/list'], function(req, res) {
    return res.send('Disabled in demo mode.');
  });

  app.get(['/user/cron', '/history/list'], function(req, res) {
    return res.molochError(403, "Disabled in demo mode.");
  });

  app.post(['/user/password/change', '/changePassword', '/tableState/:tablename'], function(req, res) {
    return res.molochError(403, "Disabled in demo mode.");
  });
}

app.get(['/', '/app'], function(req, res) {
  var question = req.url.indexOf("?");
  if (question === -1) {
    res.redirect("sessions");
  } else {
    res.redirect("sessions" + req.url.substring(question));
  }
});

app.get('/about', checkPermissions(['webEnabled']), (req, res) => {
  res.redirect('help');
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

    return res.send(cloneClusters(molochClusters));
  }

  var clustersClone = cloneClusters(app.locals.molochClusters);

  return res.send(clustersClone);
});

// custom user css
app.get('/user.css', checkPermissions(['webEnabled']), (req, res) => {
  fs.readFile("./views/user.styl", 'utf8', function(err, str) {
    function error(msg) {
      console.log('ERROR - user.css -', msg);
      return res.status(404).end();
    }

    var date = new Date().toUTCString();
    res.setHeader('Content-Type', 'text/css');
    res.setHeader('Date', date);
    res.setHeader('Cache-Control', 'public, max-age=0');
    res.setHeader('Last-Modified', date);

    if (err) { return error(err); }
    if (!req.user.settings.theme) { return error('no custom theme defined'); }

    var theme = req.user.settings.theme.split(':');

    if (!theme[1]) { return error('custom theme corrupted'); }

    var style = stylus(str);

    var colors = theme[1].split(',');

    if (!colors) { return error('custom theme corrupted'); }

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
      if (err) { return error(err); }
      return res.send(css);
    });
  });
});


/* User Endpoints ---------------------------------------------------------- */
// default settings for users with no settings
let settingDefaults = {
  timezone      : 'local',
  detailFormat  : 'last',
  showTimestamps: 'last',
  sortColumn    : 'firstPacket',
  sortDirection : 'desc',
  spiGraph      : 'node',
  connSrcField  : 'srcIp',
  connDstField  : 'ip.dst:port',
  numPackets    : 'last',
  theme         : 'default-theme'
};

// gets the current user
app.get('/user/current', checkPermissions(['webEnabled']), (req, res) => {
  let userProps = [ 'createEnabled', 'emailSearch', 'enabled', 'removeEnabled',
    'headerAuthEnabled', 'settings', 'userId', 'userName', 'webEnabled', 'packetSearch',
    'hideStats', 'hideFiles', 'hidePcap', 'disablePcapDownload', 'welcomeMsgNum',
    'lastUsed', 'timeLimit' ];

  let clone = {};

  for (let i = 0, ilen = userProps.length; i < ilen; ++i) {
    let prop = userProps[i];
    if (req.user.hasOwnProperty(prop)) {
      clone[prop] = req.user[prop];
    }
  }

  clone.canUpload = app.locals.allowUploads;

  // If no settings, use defaults
  if (clone.settings === undefined) { clone.settings = settingDefaults; }

  // Use settingsDefaults for any settings that are missing
  for (let item in settingDefaults) {
    if (clone.settings[item] === undefined) {
      clone.settings[item] = settingDefaults[item];
    }
  }

  return res.send(clone);
});

// express middleware to set req.settingUser to who to work on, depending if admin or not
function getSettingUser (req, res, next) {
  // If no userId parameter, or userId is ourself then req.user already has our info
  if (req.query.userId === undefined || req.query.userId === req.user.userId) {
    req.settingUser = req.user;
    return next();
  }

  // user is trying to get another user's settings without admin privilege
  if (!req.user.createEnabled) { return res.molochError(403, "Need admin privileges"); }

  Db.getUserCache(req.query.userId, function(err, user) {
    if (err || !user || !user.found) {
      if (app.locals.noPasswordSecret) {
        // TODO: send anonymous user's settings
        req.settingUser = {};
      } else {
        req.settingUser = null;
      }
      return next();
    }
    req.settingUser = user._source;
    return next();
  });
}

// express middleware to set req.settingUser to who to work on, depending if admin or not
function postSettingUser (req, res, next) {
  let userId;

  if (req.query.userId === undefined || req.query.userId === req.user.userId) {
    userId = req.user.userId;
  } else if (!req.user.createEnabled) {
    // user is trying to get another user's settings without admin privilege
    return res.molochError(403, "Need admin privileges");
  } else {
    userId = req.query.userId;
  }

  Db.getUser(userId, function(err, user) {
    if (err || !user || !user.found) {
      if (app.locals.noPasswordSecret) {
        // TODO: send anonymous user's settings
        req.settingUser = {};
      } else {
        req.settingUser = null;
      }
      return next();
    }
    req.settingUser = user._source;
    return next();
  });
}

function buildNotifiers () {
  internals.notifiers = {};

  let api = {
    register: function (str, info) {
      internals.notifiers[str] = info;
    }
  };

  // look for all notifier providers and initialize them
  let files = glob.sync(`${__dirname}/../notifiers/provider.*.js`);
  files.forEach((file) => {
    let plugin = require(file);
    plugin.init(api);
  });
}

function issueAlert (notifierName, alertMessage, continueProcess) {
  if (!internals.notifiers) { buildNotifiers(); }

  // find notifier
  Db.getUser('_moloch_shared', (err, sharedUser) => {
    if (!sharedUser || !sharedUser.found) {
      console.log('Cannot find notifier, no alert can be issued');
      return continueProcess();
    }

    sharedUser = sharedUser._source;

    sharedUser.notifiers = sharedUser.notifiers || {};

    let notifier = sharedUser.notifiers[notifierName];

    if (!notifier) {
      console.log('Cannot find notifier, no alert can be issued');
      return continueProcess();
    }

    let notifierDefinition;
    for (let n in internals.notifiers) {
      if (internals.notifiers[n].type === notifier.type) {
        notifierDefinition = internals.notifiers[n];
      }
    }
    if (!notifierDefinition) {
      console.log('Cannot find notifier definition, no alert can be issued');
      return continueProcess();
    }

    let config = {};
    // check that required notifier fields exist
    for (let field of notifierDefinition.fields) {
      if (field.required) {
        for (let configuredField of notifier.fields) {
          if (configuredField.name === field.name && !configuredField.value) {
            console.log(`Cannot find notifier field value: ${field.name}, no alert can be issued`);
            continueProcess();
          }
          config[field.name] = configuredField.value;
        }
      }
    }

    notifierDefinition.sendAlert(config, alertMessage);

    return continueProcess();
  });
}

app.get('/notifierTypes', checkCookieToken, function (req, res) {
  if (!internals.notifiers) {
    buildNotifiers();
  }

  return res.send(internals.notifiers);
});

// get created notifiers
app.get('/notifiers', checkCookieToken, function (req, res) {
  function cloneNotifiers(notifiers) {
    var clone = {};

    for (var key in notifiers) {
      if (notifiers.hasOwnProperty(key)) {
        var notifier = notifiers[key];
        clone[key] = {
          name: notifier.name,
          type : notifier.type
        };
      }
    }

    return clone;
  }

  Db.getUser('_moloch_shared', (err, sharedUser) => {
    if (!sharedUser || !sharedUser.found) {
      return res.send({});
    } else {
      sharedUser = sharedUser._source;
    }

    if (req.user.createEnabled) {
      return res.send(sharedUser.notifiers);
    }

    return res.send(cloneNotifiers(sharedUser.notifiers));
  });
});

// create a new notifier
app.post('/notifiers', [noCacheJson, getSettingUser, checkCookieToken], function (req, res) {
  let user = req.settingUser;
  if (!user.createEnabled) {
    return res.molochError(401, 'Need admin privelages to create a notifier');
  }

  if (!req.body.notifier) {
    return res.molochError(403, 'Missing notifier');
  }

  if (!req.body.notifier.name) {
    return res.molochError(403, 'Missing a unique notifier name');
  }

  if (!req.body.notifier.type) {
    return res.molochError(403, 'Missing notifier type');
  }

  if (!req.body.notifier.fields) {
    return res.molochError(403, 'Missing notifier fields');
  }

  if (!Array.isArray(req.body.notifier.fields)) {
    return res.molochError(403, 'Notifier fields must be an array');
  }

  req.body.notifier.name = req.body.notifier.name.replace(/[^-a-zA-Z0-9_: ]/g, '');

  if (!internals.notifiers) { buildNotifiers(); }

  let foundNotifier;
  for (let n in internals.notifiers) {
    let notifier = internals.notifiers[n];
    if (notifier.type === req.body.notifier.type) {
      foundNotifier = notifier;
    }
  }

  if (!foundNotifier) { return res.molochError(403, 'Unknown notifier type'); }

  // check that required notifier fields exist
  for (let field of foundNotifier.fields) {
    if (field.required) {
      for (let sentField of req.body.notifier.fields) {
        if (sentField.name === field.name && !sentField.value) {
          return res.molochError(403, `Missing a value for ${field.name}`);
        }
      }
    }
  }

  // save the notifier on the shared user
  Db.getUser('_moloch_shared', (err, sharedUser) => {
    if (!sharedUser || !sharedUser.found) {
      // sharing for the first time
      sharedUser = {
        userId: '_moloch_shared',
        userName: '_moloch_shared',
        enabled: false,
        webEnabled: false,
        emailSearch: false,
        headerAuthEnabled: false,
        createEnabled: false,
        removeEnabled: false,
        packetSearch: false,
        views: {},
        notifiers: {}
      };
    } else {
      sharedUser = sharedUser._source;
    }

    sharedUser.notifiers = sharedUser.notifiers || {};

    if (sharedUser.notifiers[req.body.notifier.name]) {
      console.log('Trying to add duplicate notifier', sharedUser);
      return res.molochError(403, 'Notifier already exists');
    }

    sharedUser.notifiers[req.body.notifier.name] = req.body.notifier;

    Db.setUser('_moloch_shared', sharedUser, (err, info) => {
      if (err) {
        console.log('/notifiers failed', err, info);
        return res.molochError(500, 'Creating notifier failed');
      }
      return res.send(JSON.stringify({
        success : true,
        text    : 'Successfully created notifier',
        name    : req.body.notifier.name
      }));
    });
  });
});

// update a notifier
app.put('/notifiers/:name', [noCacheJson, getSettingUser, checkCookieToken], function (req, res) {
  let user = req.settingUser;
  if (!user.createEnabled) {
    return res.molochError(401, 'Need admin privelages to update a notifier');
  }

  Db.getUser('_moloch_shared', (err, sharedUser) => {
    if (!sharedUser || !sharedUser.found) {
      return res.molochError(404, 'Cannot find notifer to udpate');
    } else {
      sharedUser = sharedUser._source;
    }

    sharedUser.notifiers = sharedUser.notifiers || {};

    if (!sharedUser.notifiers[req.params.name]) {
      return res.molochError(404, 'Cannot find notifer to udpate');
    }

    if (!req.body.notifier) {
      return res.molochError(403, 'Missing notifier');
    }

    if (!req.body.notifier.name) {
      return res.molochError(403, 'Missing a unique notifier name');
    }

    if (!req.body.notifier.type) {
      return res.molochError(403, 'Missing notifier type');
    }

    if (!req.body.notifier.fields) {
      return res.molochError(403, 'Missing notifier fields');
    }

    if (!Array.isArray(req.body.notifier.fields)) {
      return res.molochError(403, 'Notifier fields must be an array');
    }

    req.body.notifier.name = req.body.notifier.name.replace(/[^-a-zA-Z0-9_: ]/g, '');

    if (req.body.notifier.name !== req.body.key &&
      sharedUser.notifiers[req.body.notifier.name]) {
      return res.molochError(403, `${req.body.notifier.name} already exists`);
    }


    if (!internals.notifiers) { buildNotifiers(); }

    let foundNotifier;
    for (let n in internals.notifiers) {
      let notifier = internals.notifiers[n];
      if (notifier.type === req.body.notifier.type) {
        foundNotifier = notifier;
      }
    }

    if (!foundNotifier) { return res.molochError(403, 'Unknown notifier type'); }

    // check that required notifier fields exist
    for (let field of foundNotifier.fields) {
      if (field.required) {
        for (let sentField of req.body.notifier.fields) {
          if (sentField.name === field.name && !sentField.value) {
            return res.molochError(403, `Missing a value for ${field.name}`);
          }
        }
      }
    }

    sharedUser.notifiers[req.body.notifier.name] = req.body.notifier;
    // delete the old notifier if the name has changed
    if (sharedUser.notifiers[req.params.name] && req.body.notifier.name !== req.params.name) {
      sharedUser.notifiers[req.params.name] = null;
      delete sharedUser.notifiers[req.params.name];
    }

    Db.setUser('_moloch_shared', sharedUser, (err, info) => {
      if (err) {
        console.log('/notifiers update failed', err, info);
        return res.molochError(500, 'Updating notifier failed');
      }
      return res.send(JSON.stringify({
        success : true,
        text    : 'Successfully updated notifier',
        name    : req.body.notifier.name
      }));
    });
  });
});

// delete a notifier
app.delete('/notifiers/:name', [noCacheJson, getSettingUser, checkCookieToken], function (req, res) {
  let user = req.settingUser;
  if (!user.createEnabled) {
    return res.molochError(401, 'Need admin privelages to delete a notifier');
  }

  Db.getUser('_moloch_shared', (err, sharedUser) => {
    if (!sharedUser || !sharedUser.found) {
      return res.molochError(404, 'Cannot find notifer to remove');
    } else {
      sharedUser = sharedUser._source;
    }

    sharedUser.notifiers = sharedUser.notifiers || {};

    if (!sharedUser.notifiers[req.params.name]) {
      return res.molochError(404, 'Cannot find notifer to remove');
    }

    sharedUser.notifiers[req.params.name] = undefined;

    Db.setUser('_moloch_shared', sharedUser, (err, info) => {
      if (err) {
        console.log('/notifiers delete failed', err, info);
        return res.molochError(500, 'Deleting notifier failed');
      }
      return res.send(JSON.stringify({
        success : true,
        text    : 'Successfully deleted notifier',
        name    : req.params.name
      }));
    });
  });
});

// test a notifier
app.post('/notifiers/:name/test', [noCacheJson, getSettingUser, checkCookieToken], function (req, res) {
  let user = req.settingUser;
  if (!user.createEnabled) {
    return res.molochError(401, 'Need admin privelages to test a notifier');
  }

  function continueProcess () {
    return res.send(JSON.stringify({
      success : true,
      text    : `Successfully issued alert using the ${req.params.name} notifier.`
    }));
  }

  issueAlert(req.params.name, 'Test alert', continueProcess);
});

// gets a user's settings
app.get('/user/settings', [noCacheJson, getSettingUser, recordResponseTime, checkPermissions(['webEnabled'])], (req, res) => {
  if (!req.settingUser) {
    res.status(404);
    return res.send(JSON.stringify({success:false, text:'User not found'}));
  }

  let settings = req.settingUser.settings || settingDefaults;

  let cookieOptions = { path: app.locals.basePath, sameSite: 'Strict' };
  if (Config.isHTTPS()) { cookieOptions.secure = true; }

  res.cookie(
     'MOLOCH-COOKIE',
     Config.obj2auth({date: Date.now(), pid: process.pid, userId: req.user.userId}, true),
     cookieOptions
  );

  return res.send(settings);
});

// updates a user's settings
app.post('/user/settings/update', [noCacheJson, checkCookieToken, logAction(), postSettingUser], function(req, res) {
  if (!req.settingUser) {return res.molochError(403, 'Unknown user');}

  req.settingUser.settings = req.body;
  delete req.settingUser.settings.token;

  Db.setUser(req.settingUser.userId, req.settingUser, function(err, info) {
    if (err) {
      console.log('/user/settings/update error', err, info);
      return res.molochError(500, 'Settings update failed');
    }
    return res.send(JSON.stringify({
      success : true,
      text    : 'Updated settings successfully'
    }));
  });
});

function saveSharedView (req, res, user, view, endpoint, successMessage, errorMessage) {
  Db.getUser('_moloch_shared', (err, sharedUser) => {
    if (!sharedUser || !sharedUser.found) {
      // sharing for the first time
      sharedUser = {
        userId: '_moloch_shared',
        userName: '_moloch_shared',
        enabled: false,
        webEnabled: false,
        emailSearch: false,
        headerAuthEnabled: false,
        createEnabled: false,
        removeEnabled: false,
        packetSearch: false,
        views: {}
      };
    } else {
      sharedUser = sharedUser._source;
    }

    sharedUser.views = sharedUser.views || {};

    if (sharedUser.views[req.body.name]) {
      console.log('Trying to add duplicate shared view', sharedUser);
      return res.molochError(403, 'Shared view already exists');
    }

    sharedUser.views[req.body.name] = view;

    Db.setUser('_moloch_shared', sharedUser, (err, info) => {
      if (err) {
        console.log(endpoint, 'failed', err, info);
        return res.molochError(500, errorMessage);
      }
      return res.send(JSON.stringify({
        success : true,
        text    : successMessage,
        viewName: req.body.name,
        view    : view
      }));
    });
  });
}

// remove the string, 'shared:', that is added to shared views with the same
// name as a user's personal view in the endpoint '/user/views'
// also remove any special characters except ('-', '_', ':', and ' ')
function sanitizeViewName (req, res, next) {
  if (req.body.name) {
    req.body.name = req.body.name.replace(/(^shared:)|[^-a-zA-Z0-9_: ]/g, '');
  }
  next();
}

// removes a view from the user that created the view and adds it to the shared user
function shareView (req, res, user, endpoint, successMessage, errorMessage) {
  let view = user.views[req.body.name];
  view.shared = true;

  delete user.views[req.body.name]; // remove the view from the

  Db.setUser(user.userId, user, (err, info) => {
    if (err) {
      console.log(endpoint, 'failed', err, info);
      return res.molochError(500, errorMessage);
    }
    // save the view on the shared user
    return saveSharedView(req, res, user, view, endpoint, successMessage, errorMessage);
  });
}

// removes a view from the shared user and adds it to the user that created the view
function unshareView (req, res, user, sharedUser, endpoint, successMessage, errorMessage) {
  Db.setUser('_moloch_shared', sharedUser, (err, info) => {
    if (err) {
      console.log(endpoint, 'failed', err, info);
      return res.molochError(500, errorMessage);
    }

    if (user.views[req.body.name]) { // the user already has a view with this name
      return res.molochError(403, 'A view already exists with this name.');
    }

    user.views[req.body.name] = {
      expression: req.body.expression,
      user: req.body.user, // keep the user so we know who created it
      shared: false,
      sessionsColConfig: req.body.sessionsColConfig
    };

    Db.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log(endpoint, 'failed', err, info);
        return res.molochError(500, errorMessage);
      }
      return res.send(JSON.stringify({
        success : true,
        text    : successMessage
      }));
    });
  });
}

// gets a user's views
app.get('/user/views', [noCacheJson, getSettingUser], function(req, res) {
  if (!req.settingUser) { return res.send({}); }

  Db.getUser('_moloch_shared', (err, sharedUser) => {
    if (sharedUser && sharedUser.found) {
      sharedUser = sharedUser._source;
      if (!req.settingUser.views) { req.settingUser.views = {}; }
      for (let viewName in sharedUser.views) {
        // check for views with the same name as a shared view so user specific views don't get overwritten
        let sharedViewName = viewName;
        if (req.settingUser.views[sharedViewName] && !req.settingUser.views[sharedViewName].shared) {
          sharedViewName = `shared:${sharedViewName}`;
        }
        req.settingUser.views[sharedViewName] = sharedUser.views[viewName];
      }
    }

    return res.send(req.settingUser.views || {});
  });
});

// creates a new view for a user
app.post('/user/views/create', [noCacheJson, checkCookieToken, logAction(), postSettingUser, sanitizeViewName], function (req, res) {
  if (!req.settingUser) {
    console.log('/user/views/create unknown user');
    return res.molochError(403, 'Unknown user');
  }

  if (!req.body.name)   { return res.molochError(403, 'Missing view name'); }
  if (!req.body.expression) { return res.molochError(403, 'Missing view expression'); }

  let user = req.settingUser;
  user.views = user.views || {};

  let newView = {
    expression: req.body.expression,
    user: user.userId
  };

  if (req.body.shared) {
    // save the view on the shared user
    newView.shared = true;
    saveSharedView(req, res, user, newView, '/user/views/create', 'Created shared view successfully', 'Create shared view failed');
  } else {
    newView.shared = false;
    if (user.views[req.body.name]) {
      return res.molochError(403, 'A view already exists with this name.');
    } else {
      user.views[req.body.name] = newView;
    }

    if (req.body.sessionsColConfig) {
      user.views[req.body.name].sessionsColConfig = req.body.sessionsColConfig;
    } else if (user.views[req.body.name].sessionsColConfig && !req.body.sessionsColConfig) {
      user.views[req.body.name].sessionsColConfig = undefined;
    }

    Db.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log('/user/views/create error', err, info);
        return res.molochError(500, 'Create view failed');
      }
      return res.send(JSON.stringify({
        success : true,
        text    : 'Created view successfully',
        viewName: req.body.name,
        view    : newView
      }));
    });
  }
});

// deletes a user's specified view
app.post('/user/views/delete', [noCacheJson, checkCookieToken, logAction(), postSettingUser, sanitizeViewName], function(req, res) {
  if (!req.settingUser) {
    console.log('/user/views/delete unknown user');
    return res.molochError(403, 'Unknown user');
  }

  if (!req.body.name) { return res.molochError(403, 'Missing view name'); }

  let user = req.settingUser;
  user.views = user.views || {};

  if (req.body.shared) {
    Db.getUser('_moloch_shared', (err, sharedUser) => {
      if (sharedUser && sharedUser.found) {
        sharedUser = sharedUser._source;
        sharedUser.views = sharedUser.views || {};
        if (sharedUser.views[req.body.name] === undefined) { return res.molochError(404, 'View not found'); }
        // only admins or the user that created the view can delete the shared view
        if (!user.createEnabled && sharedUser.views[req.body.name].user !== user.userId) {
          return res.molochError(401, `Need admin privelages to delete another user's shared view`);
        }
        delete sharedUser.views[req.body.name];
      }

      Db.setUser('_moloch_shared', sharedUser, (err, info) => {
        if (err) {
          console.log('/user/views/delete failed', err, info);
          return res.molochError(500, 'Delete shared view failed');
        }
        return res.send(JSON.stringify({
          success : true,
          text    : 'Deleted shared view successfully'
        }));
      });
    });
  } else {
    if (user.views[req.body.name] === undefined) { return res.molochError(404, 'View not found'); }
    delete user.views[req.body.name];

    Db.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log('/user/views/delete failed', err, info);
        return res.molochError(500, 'Delete view failed');
      }
      return res.send(JSON.stringify({
        success : true,
        text    : 'Deleted view successfully'
      }));
    });
  }
});

// shares/unshares a view
app.post('/user/views/toggleShare', [noCacheJson, checkCookieToken, logAction(), postSettingUser, sanitizeViewName], function (req, res) {
  if (!req.body.name)       { return res.molochError(403, 'Missing view name'); }
  if (!req.body.expression) { return res.molochError(403, 'Missing view expression'); }

  let view;
  let share = req.body.shared;
  let user = req.settingUser;
  user.views = user.views || {};

  if (share && user.views[req.body.name] === undefined) { return res.molochError(404, 'View not found'); }

  Db.getUser('_moloch_shared', (err, sharedUser) => {
    if (!sharedUser || !sharedUser.found) {
      // the shared user has not been created yet so there is no chance of duplicate views
      if (share) { // add the view to the shared user
        return shareView(req, res, user, '/user/views/toggleShare', 'Shared view successfully', 'Sharing view failed');
      }
      // if it not already a shared view and it's trying to be unshared, something went wrong, can't do it
      return res.molochError(404, 'Shared user not found. Cannot unshare a view without a shared user.');
    }

    sharedUser = sharedUser._source;
    sharedUser.views = sharedUser.views || {};

    if (share) { // if sharing, make sure the view doesn't already exist
      if (sharedUser.views[req.body.name]) { // duplicate detected
        return res.molochError(403, 'A shared view already exists with this name.');
      }
      return shareView(req, res, user, '/user/views/toggleShare', 'Shared view successfully', 'Sharing view failed');
    } else {
      // if unsharing, remove it from shared user and add it to current user
      if (sharedUser.views[req.body.name] === undefined) { return res.molochError(404, 'View not found'); }
      // only admins or the user that created the view can update the shared view
      if (!user.createEnabled && sharedUser.views[req.body.name].user !== user.userId) {
        return res.molochError(401, `Need admin privelages to unshare another user's shared view`);
      }
      // save the view for later to determine who the view belongs to
      view = sharedUser.views[req.body.name];
      // delete the shared view
      delete sharedUser.views[req.body.name];
      return unshareView(req, res, user, sharedUser, '/user/views/toggleShare', 'Unshared view successfully', 'Unsharing view failed');
    }
  });
});

// updates a user's specified view
app.post('/user/views/update', [noCacheJson, checkCookieToken, logAction(), postSettingUser, sanitizeViewName], function (req, res) {
  if (!req.body.name)       { return res.molochError(403, 'Missing view name'); }
  if (!req.body.expression) { return res.molochError(403, 'Missing view expression'); }
  if (!req.body.key)        { return res.molochError(403, 'Missing view key'); }

  let user = req.settingUser;
  user.views = user.views || {};

  if (req.body.shared) {
    Db.getUser('_moloch_shared', (err, sharedUser) => {
      if (sharedUser && sharedUser.found) {
        sharedUser = sharedUser._source;
        sharedUser.views = sharedUser.views || {};
        if (sharedUser.views[req.body.key] === undefined) { return res.molochError(404, 'View not found'); }
        // only admins or the user that created the view can update the shared view
        if (!user.createEnabled && sharedUser.views[req.body.name].user !== user.userId) {
          return res.molochError(401, `Need admin privelages to update another user's shared view`);
        }
        sharedUser.views[req.body.name] = {
          expression: req.body.expression,
          user: user.userId,
          shared: true,
          sessionsColConfig: req.body.sessionsColConfig
        };
        // delete the old one if the key (view name) has changed
        if (sharedUser.views[req.body.key] && req.body.name !== req.body.key) {
          sharedUser.views[req.body.key] = null;
          delete sharedUser.views[req.body.key];
        }
      }

      Db.setUser('_moloch_shared', sharedUser, (err, info) => {
        if (err) {
          console.log('/user/views/delete failed', err, info);
          return res.molochError(500, 'Update shared view failed');
        }
        return res.send(JSON.stringify({
          success : true,
          text    : 'Updated shared view successfully'
        }));
      });
    });
  } else {
    if (user.views[req.body.name]) {
      user.views[req.body.name].expression = req.body.expression;
    } else { // the name has changed, so create a new entry
      user.views[req.body.name] = {
        expression: req.body.expression,
        user: user.userId,
        shared: false,
        sessionsColConfig: req.body.sessionsColConfig
      };
    }

    // delete the old one if the key (view name) has changed
    if (user.views[req.body.key] && req.body.name !== req.body.key) {
      user.views[req.body.key] = null;
      delete user.views[req.body.key];
    }

    Db.setUser(user.userId, user, function(err, info) {
      if (err) {
        console.log('/user/views/update error', err, info);
        return res.molochError(500, 'Updating view failed');
      }
      return res.send(JSON.stringify({
        success : true,
        text    : 'Updated view successfully'
      }));
    });
  }
});

// gets a user's cron queries
app.get('/user/cron', [noCacheJson, getSettingUser], function(req, res) {
  if (!req.settingUser) {return res.molochError(403, 'Unknown user');}

  var user = req.settingUser;
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
});

// creates a new cron query for a user
app.post('/user/cron/create', [noCacheJson, checkCookieToken, logAction(), postSettingUser], function(req, res) {
  if (!req.settingUser) {return res.molochError(403, 'Unknown user');}

  if (!req.body.name)   { return res.molochError(403, 'Missing cron query name'); }
  if (!req.body.query)  { return res.molochError(403, 'Missing cron query expression'); }
  if (!req.body.action) { return res.molochError(403, 'Missing cron query action'); }
  if (!req.body.tags)   { return res.molochError(403, 'Missing cron query tag(s)'); }

  var document = {
    doc: {
      enabled : true,
      name    : req.body.name,
      query   : req.body.query,
      tags    : req.body.tags,
      action  : req.body.action,
    }
  };

  if (req.body.notifier) {
    document.doc.notifier = req.body.notifier;
  }

  var userId = req.settingUser.userId;

  Db.getMinValue("sessions2-*", "timestamp", (err, minTimestamp) => {
    if (err || minTimestamp === 0 || minTimestamp === null) {
      minTimestamp = Math.floor(Date.now()/1000);
    } else {
      minTimestamp = Math.floor(minTimestamp/1000);
    }

    if (+req.body.since === -1) {
      document.doc.lpValue =  document.doc.lastRun = minTimestamp;
    } else {
      document.doc.lpValue =  document.doc.lastRun =
         Math.max(minTimestamp, Math.floor(Date.now()/1000) - 60*60*parseInt(req.body.since || '0', 10));
    }
    document.doc.count = 0;
    document.doc.creator = userId || 'anonymous';

    Db.indexNow('queries', 'query', null, document.doc, function(err, info) {
      if (err) {
        console.log('/user/cron/create error', err, info);
        return res.molochError(500, 'Create cron query failed');
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
});

// deletes a user's specified cron query
app.post('/user/cron/delete', [noCacheJson, checkCookieToken, logAction(), postSettingUser, checkCronAccess], function(req, res) {
  if (!req.settingUser) {return res.molochError(403, 'Unknown user');}

  if (!req.body.key) { return res.molochError(403, 'Missing cron query key'); }

  Db.deleteDocument('queries', 'query', req.body.key, {refresh: true}, function(err, sq) {
    if (err) {
      console.log('/user/cron/delete error', err, sq);
      return res.molochError(500, 'Delete cron query failed');
    }
    res.send(JSON.stringify({
      success : true,
      text    : 'Deleted cron query successfully'
    }));
  });
});

// updates a user's specified cron query
app.post('/user/cron/update', [noCacheJson, checkCookieToken, logAction(), postSettingUser, checkCronAccess], function(req, res) {
  if (!req.settingUser) {return res.molochError(403, 'Unknown user');}

  if (!req.body.key)    { return res.molochError(403, 'Missing cron query key'); }
  if (!req.body.name)   { return res.molochError(403, 'Missing cron query name'); }
  if (!req.body.query)  { return res.molochError(403, 'Missing cron query expression'); }
  if (!req.body.action) { return res.molochError(403, 'Missing cron query action'); }
  if (!req.body.tags)   { return res.molochError(403, 'Missing cron query tag(s)'); }

  var document = {
    doc: {
      enabled : req.body.enabled,
      name    : req.body.name,
      query   : req.body.query,
      tags    : req.body.tags,
      action  : req.body.action,
      notifier: undefined
    }
  };

  if (req.body.notifier) {
    document.doc.notifier = req.body.notifier;
  }

  Db.get('queries', 'query', req.body.key, function(err, sq) {
    if (err || !sq.found) {
      console.log('/user/cron/update failed', err, sq);
      return res.molochError(403, 'Unknown query');
    }

    Db.update('queries', 'query', req.body.key, document, {refresh: true}, function(err, data) {
      if (err) {
        console.log('/user/cron/update error', err, document, data);
        return res.molochError(500, 'Cron query update failed');
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
app.post('/user/password/change', [noCacheJson, checkCookieToken, logAction(), postSettingUser], function(req, res) {
  if (!req.settingUser) {return res.molochError(403, 'Unknown user');}

  if (!req.body.newPassword || req.body.newPassword.length < 3) {
    return res.molochError(403, 'New password needs to be at least 3 characters');
  }

  if (!req.query.userId && (req.user.passStore !==
     Config.pass2store(req.token.userId, req.body.currentPassword) ||
     req.token.userId !== req.user.userId)) {
    return res.molochError(403, 'Current password mismatch');
  }

  var user = req.settingUser;
  user.passStore = Config.pass2store(user.userId, req.body.newPassword);

  Db.setUser(user.userId, user, function(err, info) {
    if (err) {
      console.log('/user/password/change error', err, info);
      return res.molochError(500, 'Update failed');
    }
    return res.send(JSON.stringify({
      success : true,
      text    : 'Changed password successfully'
    }));
  });
});

function oldDB2newDB(x) {
  if (!internals.oldDBFields[x]) {return x;}
  return internals.oldDBFields[x].dbField2;
}

// gets custom column configurations for a user
app.get('/user/columns', [noCacheJson, getSettingUser, checkPermissions(['webEnabled'])], (req, res) => {
  if (!req.settingUser) {return res.send([]);}

  // Fix for new names
  if (req.settingUser.columnConfigs) {
    for (var key in req.settingUser.columnConfigs) {
      let item = req.settingUser.columnConfigs[key];
      item.columns = item.columns.map(oldDB2newDB);
      if (item.order && item.order.length > 0) {
        item.order[0][0] = oldDB2newDB(item.order[0][0]);
      }
    }
  }

  return res.send(req.settingUser.columnConfigs || []);
});

// udpates custom column configurations for a user
app.put('/user/columns/:name', [noCacheJson, checkCookieToken, logAction(), postSettingUser], function(req, res) {
  if (!req.settingUser)   { return res.molochError(403, 'Unknown user'); }

  if (!req.body.name)     { return res.molochError(403, 'Missing custom column configuration name'); }
  if (!req.body.columns)  { return res.molochError(403, 'Missing columns'); }
  if (!req.body.order)    { return res.molochError(403, 'Missing sort order'); }

  let user = req.settingUser;
  user.columnConfigs = user.columnConfigs || [];

  // find the custom column configuration to update
  let found = false;
  for (let i = 0, ilen = user.columnConfigs.length; i < ilen; ++i) {
    if (req.body.name === user.columnConfigs[i].name) {
      found = true;
      user.columnConfigs[i] = req.body;
    }
  }

  if (!found) { return res.molochError(200, 'Custom column configuration not found'); }

  Db.setUser(user.userId, user, function(err, info) {
    if (err) {
      console.log('/user/columns udpate error', err, info);
      return res.molochError(500, 'Update custom column configuration failed');
    }
    return res.send(JSON.stringify({
      success   : true,
      text      : 'Updated column configuration',
      colConfig : req.body
    }));
  });
});

// creates a new custom column configuration for a user
app.post('/user/columns/create', [noCacheJson, checkCookieToken, logAction(), postSettingUser], function(req, res) {
  if (!req.settingUser) {return res.molochError(403, 'Unknown user');}

  if (!req.body.name)     { return res.molochError(403, 'Missing custom column configuration name'); }
  if (!req.body.columns)  { return res.molochError(403, 'Missing columns'); }
  if (!req.body.order)    { return res.molochError(403, 'Missing sort order'); }

  req.body.name = req.body.name.replace(/[^-a-zA-Z0-9\s_:]/g, '');
  if (req.body.name.length < 1) {
    return res.molochError(403, 'Invalid custom column configuration name');
  }

  var user = req.settingUser;
  user.columnConfigs = user.columnConfigs || [];

  // don't let user use duplicate names
  for (let i = 0, ilen = user.columnConfigs.length; i < ilen; ++i) {
    if (req.body.name === user.columnConfigs[i].name) {
      return res.molochError(403, 'There is already a custom column with that name');
    }
  }

  user.columnConfigs.push({
    name    : req.body.name,
    columns : req.body.columns,
    order   : req.body.order
  });

  Db.setUser(user.userId, user, function(err, info) {
    if (err) {
      console.log('/user/columns/create error', err, info);
      return res.molochError(500, 'Create custom column configuration failed');
    }
    return res.send(JSON.stringify({
      success : true,
      text    : 'Created custom column configuration successfully',
      name    : req.body.name
    }));
  });
});

// deletes a user's specified custom column configuration
app.post('/user/columns/delete', [noCacheJson, checkCookieToken, logAction(), postSettingUser], function(req, res) {
  if (!req.settingUser) {return res.molochError(403, 'Unknown user');}

  if (!req.body.name) { return res.molochError(403, 'Missing custom column configuration name'); }

  var user = req.settingUser;
  user.columnConfigs = user.columnConfigs || [];

  var found = false;
  for (let i = 0, ilen = user.columnConfigs.length; i < ilen; ++i) {
    if (req.body.name === user.columnConfigs[i].name) {
      user.columnConfigs.splice(i, 1);
      found = true;
      break;
    }
  }

  if (!found) { return res.molochError(200, "Column not found"); }

  Db.setUser(user.userId, user, function(err, info) {
    if (err) {
      console.log('/user/columns/delete failed', err, info);
      return res.molochError(500, 'Delete custom column configuration failed');
    }
    return res.send(JSON.stringify({
      success : true,
      text    : 'Deleted custom column configuration successfully'
    }));
  });
});

// gets custom spiview fields configurations for a user
app.get('/user/spiview/fields', [noCacheJson, getSettingUser, checkPermissions(['webEnabled'])], (req, res) => {
  if (!req.settingUser) {return res.send([]);}

  return res.send(req.settingUser.spiviewFieldConfigs || []);
});

// udpates custom spiview field configuration for a user
app.put('/user/spiview/fields/:name', [noCacheJson, checkCookieToken, logAction(), postSettingUser], function(req, res) {
  if (!req.settingUser) { return res.molochError(403, 'Unknown user'); }

  if (!req.body.name)   { return res.molochError(403, 'Missing custom spiview field configuration name'); }
  if (!req.body.fields) { return res.molochError(403, 'Missing fields'); }

  let user = req.settingUser;
  user.spiviewFieldConfigs = user.spiviewFieldConfigs || [];

  // find the custom spiview field configuration to update
  let found = false;
  for (let i = 0, ilen = user.spiviewFieldConfigs.length; i < ilen; ++i) {
    if (req.body.name === user.spiviewFieldConfigs[i].name) {
      found = true;
      user.spiviewFieldConfigs[i] = req.body;
    }
  }

  if (!found) { return res.molochError(200, 'Custom spiview field configuration not found'); }

  Db.setUser(user.userId, user, function(err, info) {
    if (err) {
      console.log('/user/spiview/fields udpate error', err, info);
      return res.molochError(500, 'Update spiview field configuration failed');
    }
    return res.send(JSON.stringify({
      success   : true,
      text      : 'Updated spiview field configuration',
      colConfig : req.body
    }));
  });
});

// creates a new custom spiview fields configuration for a user
app.post('/user/spiview/fields/create', [noCacheJson, checkCookieToken, logAction(), postSettingUser], function(req, res) {
  if (!req.settingUser) {return res.molochError(403, 'Unknown user');}

  if (!req.body.name)   { return res.molochError(403, 'Missing custom spiview field configuration name'); }
  if (!req.body.fields) { return res.molochError(403, 'Missing fields'); }

  req.body.name = req.body.name.replace(/[^-a-zA-Z0-9\s_:]/g, '');

  if (req.body.name.length < 1) {
    return res.molochError(403, 'Invalid custom spiview fields configuration name');
  }

  var user = req.settingUser;
  user.spiviewFieldConfigs = user.spiviewFieldConfigs || [];

  // don't let user use duplicate names
  for (let i = 0, ilen = user.spiviewFieldConfigs.length; i < ilen; ++i) {
    if (req.body.name === user.spiviewFieldConfigs[i].name) {
      return res.molochError(403, 'There is already a custom spiview fields configuration with that name');
    }
  }

  user.spiviewFieldConfigs.push({
    name  : req.body.name,
    fields: req.body.fields
  });

  Db.setUser(user.userId, user, function(err, info) {
    if (err) {
      console.log('/user/spiview/fields/create error', err, info);
      return res.molochError(500, 'Create custom spiview fields configuration failed');
    }
    return res.send(JSON.stringify({
      success : true,
      text    : 'Created custom spiview fields configuration successfully',
      name    : req.body.name
    }));
  });
});

// deletes a user's specified custom spiview fields configuration
app.post('/user/spiview/fields/delete', [noCacheJson, checkCookieToken, logAction(), postSettingUser], function(req, res) {
  if (!req.settingUser) {return res.molochError(403, 'Unknown user');}

  if (!req.body.name) { return res.molochError(403, 'Missing custom spiview fields configuration name'); }

  var user = req.settingUser;
  user.spiviewFieldConfigs = user.spiviewFieldConfigs || [];

  var found = false;
  for (let i = 0, ilen = user.spiviewFieldConfigs.length; i < ilen; ++i) {
    if (req.body.name === user.spiviewFieldConfigs[i].name) {
      user.spiviewFieldConfigs.splice(i, 1);
      found = true;
      break;
    }
  }

  if (!found) { return res.molochError(200, "Spiview fields not found"); }

  Db.setUser(user.userId, user, function(err, info) {
    if (err) {
      console.log('/user/spiview/fields/delete failed', err, info);
      return res.molochError(500, 'Delete custom spiview fields configuration failed');
    }
    return res.send(JSON.stringify({
      success : true,
      text    : 'Deleted custom spiview fields configuration successfully'
    }));
  });
});


app.get('/decodings', [noCacheJson], function(req, res) {
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
  Db.hostnameToNodeids(Config.hostName(), function(nodes) {
    // Current node name should always be checked too
    if (!nodes.includes(Config.nodeName())) {
      nodes.push(Config.nodeName());
    }

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
function addSortToQuery(query, info, d) {

  function addSortDefault() {
    if (d) {
      if (!query.sort) {
        query.sort = [];
      }
      var obj = {};
      obj[d] = {order: "asc"};
      obj[d].missing = '_last';
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
      if (field === "firstPacket") {
        obj.firstPacket = {order: parts[1]};
      } else if (field === "lastPacket") {
        obj.lastPacket = {order: parts[1]};
      } else {
        obj[field] = {order: parts[1]};
      }

      obj[field].unmapped_type = "string";
      var fieldInfo  = Config.getDBFieldsMap()[field];
      if (fieldInfo) {
        if (fieldInfo.type === "ip") {
          obj[field].unmapped_type = "ip";
        } else if (fieldInfo.type === "integer") {
          obj[field].unmapped_type = "long";
        }
      }
      obj[field].missing = (parts[1] === 'asc'?'_last':'_first');
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

  for (let i = 0, ilen = parseInt(info.iSortingCols, 10); i < ilen; i++) {
    if (!info["iSortCol_" + i] || !info["sSortDir_" + i] || !info["mDataProp_" + info["iSortCol_" + i]]) {
      continue;
    }

    var obj = {};
    var field = info["mDataProp_" + info["iSortCol_" + i]];
    obj[field] = {order: info["sSortDir_" + i]};
    query.sort.push(obj);

    if (field === "firstPacket") {
      query.sort.push({firstPacket: {order: info["sSortDir_" + i]}});
    } else if (field === "lastPacket") {
      query.sort.push({lastPacket: {order: info["sSortDir_" + i]}});
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

  //jshint latedef: nofunc
  function process(parent, obj, item) {
    // console.log("\nprocess:\n", item, obj, typeof obj[item], "\n");
    if (item === "fileand" && typeof obj[item] === "string") {
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
            obj.bool.should.push({bool: {must: [{term: {node: file.node}}, {term: {fileId: file.num}}]}});
          });
        } else {
          obj.bool = {must: [{term: {node: files[0].node}}, {term: {fileId: files[0].num}}]};
        }
        if (finished && outstanding === 0) {
          doneCb(err);
        }
      });
    } else if (item === 'field' && obj.field === 'fileand') {
      obj.field = 'fileId';
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

function buildSessionQuery (req, buildCb) {
  // validate time limit is not exceeded
  let timeLimitExceeded = false;

  if (parseInt(req.query.date) > parseInt(req.user.timeLimit) ||
    (req.query.date === '-1') && req.user.timeLimit) {
    timeLimitExceeded = true;
  } else if (req.query.startTime && req.query.stopTime) {
    if (! /^[0-9]+$/.test(req.query.startTime)) {
      req.query.startTime = Date.parse(req.query.startTime.replace('+', ' ')) / 1000;
    } else {
      req.query.startTime = parseInt(req.query.startTime, 10);
    }

    if (! /^[0-9]+$/.test(req.query.stopTime)) {
      req.query.stopTime = Date.parse(req.query.stopTime.replace('+', ' ')) / 1000;
    } else {
      req.query.stopTime = parseInt(req.query.stopTime, 10);
    }

    if (req.user.timeLimit && (req.query.stopTime - req.query.startTime) / 3600 > req.user.timeLimit) {
      timeLimitExceeded = true;
    }
  }

  if (timeLimitExceeded) {
    console.log(`${req.user.userName} trying to exceed time limit: ${req.user.timeLimit} hours`);
    return buildCb(`User time limit (${req.user.timeLimit} hours) exceeded`, {});
  }

  var limit = Math.min(2000000, +req.query.length || +req.query.iDisplayLength || 100);

  var query = {from: req.query.start || req.query.iDisplayStart || 0,
               size: limit,
               timeout: internals.esQueryTimeout,
               query: {bool: {filter: []}}
              };

  if (query.from === 0) {
    delete query.from;
  }

  if (req.query.strictly === "true") {
    req.query.bounding = "both";
  }

  var interval;
  if ((req.query.date && req.query.date === '-1') ||
      (req.query.segments && req.query.segments === "all")) {
    interval = 60*60; // Hour to be safe
  } else if (req.query.startTime !== undefined && req.query.stopTime) {
    switch (req.query.bounding) {
    case "first":
      query.query.bool.filter.push({range: {firstPacket: {gte: req.query.startTime*1000, lte: req.query.stopTime*1000}}});
      break;
    default:
    case "last":
      query.query.bool.filter.push({range: {lastPacket: {gte: req.query.startTime*1000, lte: req.query.stopTime*1000}}});
      break;
    case "both":
      query.query.bool.filter.push({range: {firstPacket: {gte: req.query.startTime*1000}}});
      query.query.bool.filter.push({range: {lastPacket: {lte: req.query.stopTime*1000}}});
      break;
    case "either":
      query.query.bool.filter.push({range: {firstPacket: {lte: req.query.stopTime*1000}}});
      query.query.bool.filter.push({range: {lastPacket: {gte: req.query.startTime*1000}}});
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
      query.query.bool.filter.push({range: {firstPacket: {gte: req.query.startTime*1000}}});
      break;
    default:
    case "both":
    case "last":
      query.query.bool.filter.push({range: {lastPacket: {gte: req.query.startTime*1000}}});
      break;
    case "either":
      query.query.bool.filter.push({range: {firstPacket: {lte: req.query.stopTime*1000}}});
      query.query.bool.filter.push({range: {lastPacket: {gte: req.query.startTime*1000}}});
      break;
    case "database":
      query.query.bool.filter.push({range: {timestamp: {gte: req.query.startTime*1000}}});
      break;
    }

    if (req.query.date <= 5*24) {
      interval = 60; // minute
    } else {
      interval = 60 * 60; // hour
    }
  }

  switch (req.query.interval) {
    case 'second':
      interval = 1;
      break;
    case 'minute':
      interval = 60;
      break;
    case 'hour':
      interval = 60 * 60;
      break;
    case 'day':
      interval = 60 * 60 * 24;
      break;
    case 'week':
      interval = 60 * 60 * 24 * 7;
      break;
  }

  if (req.query.facets) {
    query.aggregations = {};
    // only add map aggregations if requested
    if (req.query.map === 'true') {
      query.aggregations = {
        mapG1: { terms: { field: 'srcGEO', size: 1000, min_doc_count: 1} },
        mapG2: { terms: { field: 'dstGEO', size: 1000, min_doc_count: 1} },
        mapG3: { terms: { field: 'http.xffGEO', size: 1000, min_doc_count: 1} }
      };
    }
    query.aggregations.dbHisto = {
      aggregations: {
        srcDataBytes: { sum: { field: 'srcDataBytes' } },
        dstDataBytes: { sum: { field: 'dstDataBytes' } },
        srcBytes: { sum: { field: 'srcBytes' } },
        dstBytes: { sum: { field: 'dstBytes' } },
        srcPackets: { sum: { field: 'srcPackets' } },
        dstPackets: { sum: { field: 'dstPackets' } }
      }
    };

    switch (req.query.bounding) {
    case 'first':
       query.aggregations.dbHisto.histogram = { field:'firstPacket', interval:interval*1000, min_doc_count:1 };
      break;
    case 'database':
      query.aggregations.dbHisto.histogram = { field:'timestamp', interval:interval*1000, min_doc_count:1 };
      break;
    default:
      query.aggregations.dbHisto.histogram = { field:'lastPacket', interval:interval*1000, min_doc_count:1 };
      break;
    }
  }

  addSortToQuery(query, req.query, 'firstPacket');

  let err = null;

  molochparser.parser.yy = {
    views: req.user.views,
    fieldsMap: Config.getFieldsMap(),
    prefix: internals.prefix,
    emailSearch: req.user.emailSearch === true,
    lookups: req.lookups,
    lookupTypeMap: internals.lookupTypeMap
  };

  if (req.query.expression) {
    //req.query.expression = req.query.expression.replace(/\\/g, "\\\\");
    try {
      query.query.bool.filter.push(molochparser.parse(req.query.expression));
    } catch (e) {
      err = e;
    }
  }

  if (!err && req.query.view) {
    addViewToQuery(req, query, continueBuildQuery, buildCb);
  } else {
    continueBuildQuery(req, query, err, buildCb);
  }
}

function addViewToQuery(req, query, continueBuildQueryCb, finalCb) {
  let err;
  let viewExpression;
  if (req.user.views && req.user.views[req.query.view]) { // it's a user's view
    try {
      viewExpression = molochparser.parse(req.user.views[req.query.view].expression);
      query.query.bool.filter.push(viewExpression);
    } catch (e) {
      console.log(`ERROR - User expression (${req.query.view}) doesn't compile -`, e);
      err = e;
    }
    continueBuildQueryCb(req, query, err, finalCb);
  } else { // it's a shared view
    Db.getUser('_moloch_shared', (err, sharedUser) => {
      if (sharedUser && sharedUser.found) {
        sharedUser = sharedUser._source;
        sharedUser.views = sharedUser.views || {};
        for (let viewName in sharedUser.views) {
          if (viewName === req.query.view) {
            viewExpression = sharedUser.views[viewName].expression;
            break;
          }
        }
        if (sharedUser.views[req.query.view]) {
          try {
            viewExpression = molochparser.parse(sharedUser.views[req.query.view].expression);
            query.query.bool.filter.push(viewExpression);
          } catch (e) {
            console.log(`ERROR - Shared user expression (${req.query.view}) doesn't compile -`, e);
            err = e;
          }
        }
        continueBuildQueryCb(req, query, err, finalCb);
      }
    });
  }
}

function continueBuildQuery(req, query, err, finalCb) {
  if (!err && req.user.expression && req.user.expression.length > 0) {
    try {
      // Expression was set by admin, so assume email search ok
      molochparser.parser.yy.emailSearch = true;
      var userExpression = molochparser.parse(req.user.expression);
      query.query.bool.filter.push(userExpression);
    } catch (e) {
      console.log(`ERROR - Forced expression (${req.user.expression}) doesn't compile -`, e);
      err = e;
    }
  }

  lookupQueryItems(query.query.bool.filter, function (lerr) {
    if (req.query.date === '-1' ||                                      // An all query
        (req.query.bounding || "last") !== "last" ||                    // Not a last bounded query
        Config.get("queryAllIndices", Config.get("multiES", false))) {  // queryAllIndices (default: multiES)
      return finalCb(err || lerr, query, "sessions2-*"); // Then we just go against all indices for a slight overhead
    }

    Db.getIndices(req.query.startTime, req.query.stopTime, Config.get("rotateIndex", "daily"), function(indices) {
      if (indices.length > 3000) { // Will url be too long
        return finalCb(err || lerr, query, "sessions2-*");
      } else {
        return finalCb(err || lerr, query, indices);
      }
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
    if (!fields.rootId || processedRo[fields.rootId]) {
      if (writes++ > 100) {
        writes = 0;
        setImmediate(nextCb);
      } else {
        nextCb();
      }
      return;
    }
    processedRo[fields.rootId] = true;

    query.query.bool.filter.push({term: {rootId: fields.rootId}});
    Db.searchPrimary(indices, 'session', query, null, function (err, result) {
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
    query.query.bool.filter.pop();

  }, function (err) {
    cb(err, list);
  });
}

function sessionsListFromQuery(req, res, fields, cb) {
  if (req.query.segments && req.query.segments.match(/^(time|all)$/) && fields.indexOf("rootId") === -1) {
    fields.push("rootId");
  }

  buildSessionQuery(req, function(err, query, indices) {
    if (err) {
      return res.send("Could not build query.  Err: " + err);
    }
    query._source = fields;
    if (Config.debug) {
      console.log("sessionsListFromQuery query", JSON.stringify(query, null, 1));
    }
    Db.searchPrimary(indices, 'session', query, null, function (err, result) {
      if (err || result.error) {
          console.log("ERROR - Could not fetch list of sessions.  Err: ", err,  " Result: ", result, "query:", query);
          return res.send("Could not fetch list of sessions.  Err: " + err + " Result: " + result);
      }
      var list = result.hits.hits;
      if (req.query.segments && req.query.segments.match(/^(time|all)$/)) {
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
  var processSegments = false;
  if (req && ((req.query.segments && req.query.segments.match(/^(time|all)$/)) || (req.body.segments && req.body.segments.match(/^(time|all)$/)))) {
    if (fields.indexOf("rootId") === -1) { fields.push("rootId"); }
    processSegments = true;
  }

  let list = [];
  let nonArrayFields = ["ipProtocol", "firstPacket", "lastPacket", "srcIp", "srcPort", "srcGEO", "dstIp", "dstPort", "dstGEO", "totBytes", "totDataBytes", "totPackets", "node", "rootId", "http.xffGEO"];
  let fixFields = nonArrayFields.filter(function(x) {return fields.indexOf(x) !== -1;});

  async.eachLimit(ids, 10, function(id, nextCb) {
    Db.getWithOptions(Db.sid2Index(id), 'session', Db.sid2Id(id), {_source: fields.join(",")}, function(err, session) {
      if (err) {
        return nextCb(null);
      }

      for (let i = 0; i < fixFields.length; i++) {
        var field = fixFields[i];
        if (session._source[field] && Array.isArray(session._source[field])) {
          session._source[field] = session._source[field][0];
        }
      }

      list.push(session);
      nextCb(null);
    });
  }, function(err) {
    if (processSegments) {
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
app.get('/history/list', [noCacheJson, recordResponseTime], function (req, res) {
  let userId;
  if (req.user.createEnabled) { // user is an admin, they can view all logs
    // if the admin has requested a specific user
    if (req.query.userId) { userId = req.query.userId; }
  } else { // user isn't an admin, so they can only view their own logs
    if (req.query.userId && req.query.userId !== req.user.userId) { return res.molochError(403, 'Need admin privileges'); }
    userId = req.user.userId;
  }

  let query = {
    sort: {},
    from: +req.query.start  || 0,
    size: +req.query.length || 1000
  };

  query.sort[req.query.sortField || 'timestamp'] = { order: req.query.desc === 'true' ? 'desc': 'asc'};

  if (req.query.searchTerm || userId) {
    query.query = { bool: { must: [] } };

    if (req.query.searchTerm) { // apply search term
      query.query.bool.must.push({
        query_string: {
          query : req.query.searchTerm,
          fields: ['expression','userId','api','view.name','view.expression']
        }
      });
    }

    if (userId) { // filter on userId
      query.query.bool.must.push({
        wildcard: { userId: '*' + userId + '*' }
      });
    }
  }

  if (req.query.api) { // filter on api endpoint
    if (!query.query) { query.query = { bool: { must: [] } }; }
    query.query.bool.must.push({
      wildcard: { api: '*' + req.query.api + '*' }
    });
  }

  if (req.query.exists) {
    if (!query.query) { query.query = { bool: { must: [] } }; }
    let existsArr = req.query.exists.split(',');
    for (let i = 0, len = existsArr.length; i < len; ++i) {
      query.query.bool.must.push({
        exists: { field:existsArr[i] }
      });
    }
  }

  // filter history table by a time range
  if (req.query.startTime && req.query.stopTime) {
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

    if (!query.query) { query.query = { bool: {} }; }
    query.query.bool.filter = [{
      range: { timestamp: {
        gte: req.query.startTime,
        lte: req.query.stopTime
      } }
    }];
  }

  Promise.all([Db.searchHistory(query),
               Db.numberOfLogs()
              ])
  .then(([logs, total]) => {
    if (logs.error) { throw logs.error; }

    let results = { total:logs.hits.total, results:[] };
    for (let i = 0, ilen = logs.hits.hits.length; i < ilen; i++) {
      let hit = logs.hits.hits[i];
      let log = hit._source;
      log.id = hit._id;
      log.index = hit._index;
      if (!req.user.createEnabled) {
        // remove forced expression for reqs made by nonadmin users
        log.forcedExpression = undefined;
      }
      results.results.push(log);
    }
    let r = {
      recordsTotal: total.count,
      recordsFiltered: results.total,
      data: results.results
    };
    res.send(r);
  }).catch(err => {
    console.log('ERROR - /history/logs', err);
    return res.molochError(500, 'Error retrieving log history - ' + err);
  });
});

app.delete('/history/list/:id', [noCacheJson, checkPermissions(['createEnabled', 'removeEnabled'])], function (req, res) {
  if (!req.query.index) { return res.molochError(403, 'Missing history index'); }

  Db.deleteHistoryItem(req.params.id, req.query.index, function(err, result) {
    if (err || result.error) {
      console.log('ERROR - deleting history item', err || result.error);
      return res.molochError(500, 'Error deleting history item');
    } else {
      res.send(JSON.stringify({success: true, text: 'Deleted history item successfully'}));
    }
  });
});


app.get('/fields', function(req, res) {
  if (!app.locals.fieldsMap) {
    res.status(404);
    res.send('Cannot locate fields');
  }

  if (req.query && req.query.array) {
    res.send(app.locals.fieldsArr);
  } else {
    res.send(app.locals.fieldsMap);
  }
});

app.get('/file/list', [noCacheJson, logAction('files'), checkPermissions(['hideFiles']), recordResponseTime], (req, res) => {
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

  Promise.all([Db.search('files', 'file', query),
               Db.numberOfDocuments('files')
              ])
  .then(([files, total]) => {
    if (files.error) {throw files.error;}

    var results = {total: files.hits.total, results: []};
    for (let i = 0, ilen = files.hits.hits.length; i < ilen; i++) {
      var fields = files.hits.hits[i]._source || files.hits.hits[i].fields;
      if (fields.locked === undefined) {
        fields.locked = 0;
      }
      fields.id = files.hits.hits[i]._id;
      results.results.push(fields);
    }

    var r = {recordsTotal: total.count,
             recordsFiltered: results.total,
             data: results.results};
    res.logCounts(r.data.length, r.recordsFiltered, r.total);
    res.send(r);

  }).catch((err) => {
    console.log("ERROR - /file/list", err);
    return res.send({recordsTotal: 0, recordsFiltered: 0, data: []});
  });
});

app.get('/titleconfig', checkPermissions(['webEnabled']), (req, res) => {
  var titleConfig = Config.get('titleTemplate', '_cluster_ - _page_ _-view_ _-expression_');

  titleConfig = titleConfig.replace(/_cluster_/g, internals.clusterName)
    .replace(/_userId_/g, req.user?req.user.userId:"-")
    .replace(/_userName_/g, req.user?req.user.userName:"-");

  res.send(titleConfig);
});

app.get('/molochRightClick', [noCacheJson, checkPermissions(['webEnabled'])], (req, res) => {
  if(!app.locals.molochRightClick) {
    res.status(404);
    res.send('Cannot locate right clicks');
  }
  res.send(app.locals.molochRightClick);
});

// No auth necessary for eshealth.json
app.get('/eshealth.json', [noCacheJson], (req, res) => {
  Db.healthCache(function(err, health) {
    res.send(health);
  });
});

app.get('/esindices/list', [noCacheJson, recordResponseTime, checkPermissions(['hideStats'])], (req, res) => {
  async.parallel({
    indices: Db.indicesCache,
    indicesSettings: Db.indicesSettingsCache
  }, function (err, results) {
    if (err) {
      console.log ('ERROR -  /esindices/list', err);
      return res.send({
        recordsTotal: 0,
        recordsFiltered: 0,
        data: []
      });
    }

    const indices = results.indices;
    const indicesSettings = results.indicesSettings;

    let findices = [];

    // filtering
    if (req.query.filter !== undefined) {
      const regex = new RegExp(req.query.filter);
      for (const index of indices) {
        if (!index.index.match(regex)) { continue; }
        findices.push(index);
      }
    } else {
      findices = indices;
    }

    // Add more fields from indicesSettings
    for (const index of findices) {
      if (!indicesSettings[index.index]) { continue; }

      if (indicesSettings[index.index].settings['index.routing.allocation.require.molochtype']) {
        index.molochtype = indicesSettings[index.index].settings['index.routing.allocation.require.molochtype'];
      }

      if (indicesSettings[index.index].settings['index.routing.allocation.total_shards_per_node']) {
        index.shardsPerNode = indicesSettings[index.index].settings['index.routing.allocation.total_shards_per_node'];
      }

      index.creationDate = parseInt(indicesSettings[index.index].settings['index.creation_date']);
      index.versionCreated = parseInt(indicesSettings[index.index].settings['index.version.created']);
    }

    // sorting
    const sortField = req.query.sortField || 'index';
    if (sortField === 'index' || sortField === 'status' || sortField === 'health') {
      if (req.query.desc === 'true') {
        findices = findices.sort(function (a, b) { return b[sortField].localeCompare(a[sortField]); });
      } else {
        findices = findices.sort(function (a, b) { return a[sortField].localeCompare(b[sortField]); });
      }
    } else {
      if (req.query.desc === 'true') {
        findices = findices.sort(function (a,b) { return b[sortField] - a[sortField]; });
      } else {
        findices = findices.sort(function (a,b) { return a[sortField] - b[sortField]; });
      }
    }

    // send result
    return res.send({
      recordsTotal: indices.length,
      recordsFiltered: findices.length,
      data: findices
    });
  });
});

app.delete('/esindices/:index', [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])], (req, res) => {
  if (!req.params.index) {
    return res.molochError(403, 'Missing index to delete');
  }

  Db.deleteIndex([req.params.index], {}, (err, result) => {
    if (err) {
      res.status(404);
      return res.send(JSON.stringify({ success:false, text:'Error deleting index' }));
    }
    return res.send(JSON.stringify({ success: true, text: result }));
  });
});

app.post('/esindices/:index/optimize', [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])], (req, res) => {
  if (!req.params.index) {
    return res.molochError(403, 'Missing index to optimize');
  }

  Db.optimizeIndex([req.params.index], {}, (err, result) => {
    if (err) {
      console.log ("ERROR -", req.params.index, "optimize failed", err);
    }
  });

  // Always return right away, optimizeIndex might block
  return res.send(JSON.stringify({ success: true, text: {} }));
});

app.post('/esindices/:index/close', [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])], (req, res) => {
  if (!req.params.index) {
    return res.molochError(403, 'Missing index to close');
  }

  Db.closeIndex([req.params.index], {}, (err, result) => {
    if (err) {
      res.status(404);
      return res.send(JSON.stringify({ success:false, text:'Error closing index' }));
    }
    return res.send(JSON.stringify({ success: true, text: result }));
  });
});

app.post('/esindices/:index/open', [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])], (req, res) => {
  if (!req.params.index) {
    return res.molochError(403, 'Missing index to open');
  }

  Db.openIndex([req.params.index], {}, (err, result) => {
    if (err) {
      console.log ("ERROR -", req.params.index, "open failed", err);
    }
  });

  // Always return right away, openIndex might block
  return res.send(JSON.stringify({ success: true, text: {} }));
});

app.post('/esindices/:index/shrink', [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])], (req, res) => {
  if (!req.body || !req.body.target) {
    return res.molochError(403, 'Missing target');
  }

  let settingsParams = {
    body: {
      'index.routing.allocation.total_shards_per_node': null,
      'index.routing.allocation.require._name': req.body.target,
      'index.blocks.write': true
    }
  };

  Db.setIndexSettings(req.params.index, settingsParams, (err, results) => {
    if (err) {
      return res.send(JSON.stringify({
        success: false,
        text: err.message || 'Error shrinking index'
      }));
    }

    let shrinkParams = {
      body: {
        settings: {
          'index.routing.allocation.require._name': null,
          'index.blocks.write': null,
          'index.codec': 'best_compression',
          'index.number_of_shards': req.body.numShards || 1
        }
      }
    };

    // wait for no more reloacting shards
    let shrinkCheckInterval = setInterval(() => {
      Db.healthCachePromise()
        .then((result) => {
          if (result.relocating_shards === 0) {
            clearInterval(shrinkCheckInterval);
            Db.shrinkIndex(req.params.index, shrinkParams, (err, results) => {
              if (err) {
                console.log(`ERROR - ${req.params.index} shrink failed`, err);
              }
            });
          }
        });
    }, 10000);

    // always return right away, shrinking might take a while
    return res.send(JSON.stringify({ success: true }));
  });
});

app.get('/estask/list', [noCacheJson, recordResponseTime, checkPermissions(['hideStats'])], (req, res) => {
  Db.tasks(function (err, tasks) {
    if (err) {
      console.log ('ERROR -  /estask/list', err);
      return res.send({
        recordsTotal: 0,
        recordsFiltered: 0,
        data: []
      });
    }

    tasks = tasks.tasks;

    let regex;
    if (req.query.filter !== undefined) {
      regex = new RegExp(req.query.filter);
    }

    let rtasks = [];
    for (const key in tasks) {
      let task = tasks[key];

      task.taskId = key;
      if (task.children) {
        task.childrenCount = task.children.length;
      } else {
        task.childrenCount = 0;
      }
      delete task.children;

      if (req.query.cancellable && req.query.cancellable === 'true') {
        if (!task.cancellable) { continue; }
      }

      if (task.headers['X-Opaque-Id']) {
        let parts = splitRemain(task.headers['X-Opaque-Id'], '::', 1);
        task.user = (parts.length === 1?'':parts[0]);
      } else {
        task.user = '';
      }

      if (regex && (!task.action.match(regex) && !task.user.match(regex))) { continue; }

      rtasks.push(task);
    }

    const sortField = req.query.sortField || 'action';
    if (sortField === 'action' || sortField === 'user') {
      if (req.query.desc === 'true') {
        rtasks = rtasks.sort(function (a, b) { return b.action.localeCompare(a.index); });
      } else {
        rtasks = rtasks.sort(function (a, b) { return a.action.localeCompare(b.index); });
      }
    } else {
      if (req.query.desc === 'true') {
        rtasks = rtasks.sort(function (a, b) { return b[sortField] - a[sortField]; });
      } else {
        rtasks = rtasks.sort(function (a, b) { return a[sortField] - b[sortField]; });
      }
    }

    let size = parseInt(req.query.size) || 1000;
    if (rtasks.length > size) {
      rtasks = rtasks.slice(0, size);
    }

    return res.send({
      recordsTotal: Object.keys(tasks).length,
      recordsFiltered: rtasks.length,
      data: rtasks
    });
  });
});

app.post('/estask/cancel', [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])], (req, res) => {
  if (!req.body || !req.body.taskId) {
    return res.molochError(403, 'Missing/Empty required fields');
  }

  Db.taskCancel(req.body.taskId, (err, result) => {
    return res.send(JSON.stringify({ success: true, text: result }));
  });
});

app.post('/estask/cancelById', [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])], (req, res) => {
  if (!req.body || !req.body.cancelId) {
    return res.molochError(403, 'Missing cancel ID');
  }

  Db.cancelByOpaqueId(`${req.user.userId}::${req.body.cancelId}`, (err, result) => {
    return res.send(JSON.stringify({ success: true, text: result }));
  });
});

app.get('/esshard/list', [noCacheJson, recordResponseTime, checkPermissions(['hideStats'])], (req, res) => {
  Promise.all([
    Db.shards(),
    Db.getClusterSettings({flatSettings: true})
  ]).then(([shards, settings]) => {
    let ipExcludes = [];
    if (settings.persistent['cluster.routing.allocation.exclude._ip']) {
      ipExcludes = settings.persistent['cluster.routing.allocation.exclude._ip'].split(',');
    }

    let nodeExcludes = [];
    if (settings.persistent['cluster.routing.allocation.exclude._name']) {
      nodeExcludes = settings.persistent['cluster.routing.allocation.exclude._name'].split(',');
    }

    var regex;
    if (req.query.filter !== undefined) {
      regex = new RegExp(req.query.filter.toLowerCase());
    }

    let result = {};
    let nodes = {};

    for (var shard of shards) {
      if (shard.node === null || shard.node === "null") { shard.node = "Unassigned"; }

      if (! (req.query.show === 'all' ||
            shard.state === req.query.show ||    //  Show only matching stage
            (shard.state !== 'STARTED' && req.query.show === 'notstarted'))) {
        continue;
      }

      if (regex && !shard.index.toLowerCase().match(regex) && !shard.node.toLowerCase().match(regex)) { continue; }

      if (result[shard.index] === undefined) {
        result[shard.index] = {name: shard.index, nodes: {}};
      }
      if (result[shard.index].nodes[shard.node] === undefined) {
        result[shard.index].nodes[shard.node] = [];
      }
      result[shard.index].nodes[shard.node].push(shard);
      nodes[shard.node] = {ip: shard.ip, ipExcluded: ipExcludes.includes(shard.ip), nodeExcluded: nodeExcludes.includes(shard.node)};

      result[shard.index].nodes[shard.node]
        .sort((a, b) => {
          return a.shard - b.shard;
        });

      delete shard.node;
      delete shard.index;
    }

    let indices = Object.keys(result).map((k) => result[k]);
    if (req.query.desc === 'true') {
      indices = indices.sort(function (a, b) {
        return b.name.localeCompare(a.name);
      });
    } else {
      indices = indices.sort(function (a, b) {
        return a.name.localeCompare(b.name);
      });
    }
    res.send({nodes: nodes, indices: indices, nodeExcludes: nodeExcludes, ipExcludes: ipExcludes});
  });
});

app.post('/esshard/exclude/:type/:value', [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])], (req, res) => {
  if (Config.get("multiES", false)) { return res.molochError(401, "Not supported in multies"); }

  Db.getClusterSettings({flatSettings: true}, function(err, settings) {
    let exclude = [];
    let settingName;

    if (req.params.type === 'ip') {
      settingName = 'cluster.routing.allocation.exclude._ip';
    } else if (req.params.type === 'name') {
      settingName = 'cluster.routing.allocation.exclude._name';
    } else {
      return res.molochError(403, 'Unknown exclude type');
    }

    if (settings.persistent[settingName]) {
      exclude = settings.persistent[settingName].split(',');
    }

    if (!exclude.includes(req.params.value)) {
      exclude.push(req.params.value);
    }
    var query = {body: {persistent: {}}};
    query.body.persistent[settingName] = exclude.join(',');

    Db.putClusterSettings(query, function(err, settings) {
      if (err) {console.log("putSettings", err);}
      return res.send(JSON.stringify({ success: true, text: 'Excluded'}));
    });
  });
});

app.post('/esshard/include/:type/:value', [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])], (req, res) => {
  if (Config.get("multiES", false)) { return res.molochError(401, "Not supported in multies"); }

  Db.getClusterSettings({flatSettings: true}, function(err, settings) {
    let exclude = [];
    let settingName;

    if (req.params.type === 'ip') {
      settingName = 'cluster.routing.allocation.exclude._ip';
    } else if (req.params.type === 'name') {
      settingName = 'cluster.routing.allocation.exclude._name';
    } else {
      return res.molochError(403, 'Unknown include type');
    }

    if (settings.persistent[settingName]) {
      exclude = settings.persistent[settingName].split(',');
    }

    let pos = exclude.indexOf(req.params.value);
    if (pos > -1) {
      exclude.splice(pos, 1);
    }
    var query = {body: {persistent: {}}};
    query.body.persistent[settingName] = exclude.join(',');

    Db.putClusterSettings(query, function(err, settings) {
      if (err) {console.log("putSettings", err);}
      return res.send(JSON.stringify({ success: true, text: 'Included'}));
    });
  });
});

app.get('/esrecovery/list', [noCacheJson, recordResponseTime, checkPermissions(['hideStats'])], (req, res) => {
  const sortField = (req.query.sortField || 'index') + (req.query.desc === 'true' ? ':desc' : '');

  Promise.all([Db.recovery(sortField)]).then(([recoveries]) => {
    let regex;
    if (req.query.filter !== undefined) {
      regex = new RegExp(req.query.filter);
    }

    let result = [];

    for (const recovery of recoveries) {
      if (! (req.query.show === 'all' ||
        recovery.stage === req.query.show || // Show only matching stage
        (recovery.stage !== 'done' && req.query.show === 'notdone'))) {
        continue;
      }

      // filtering
      if (regex && !recovery.index.match(regex) &&
        !recovery.target_node.match(regex) &&
        !recovery.source_node.match(regex)) {
        continue;
      }

      result.push(recovery);
    }

    res.send({
      recordsTotal: recoveries.length,
      recordsFiltered: result.length,
      data: result
    });
  }).catch((err) => {
    console.log ('ERROR -  /esrecovery/list', err);
    return res.send({
      recordsTotal: 0,
      recordsFiltered: 0,
      data: []
    });
  });
});

app.get('/esstats.json', [noCacheJson, recordResponseTime, checkPermissions(['hideStats'])], (req, res) => {
  let stats = [];
  let r;

  Promise.all([Db.nodesStatsCache(),
               Db.nodesInfoCache(),
               Db.masterCache(),
               Db.healthCachePromise(),
               Db.getClusterSettings({flatSettings: true})
             ])
  .then(([nodesStats, nodesInfo, master, health, settings]) => {

    let ipExcludes = [];
    if (settings.persistent['cluster.routing.allocation.exclude._ip']) {
      ipExcludes = settings.persistent['cluster.routing.allocation.exclude._ip'].split(',');
    }

    let nodeExcludes = [];
    if (settings.persistent['cluster.routing.allocation.exclude._name']) {
      nodeExcludes = settings.persistent['cluster.routing.allocation.exclude._name'].split(',');
    }

    const now = new Date().getTime();
    while (internals.previousNodesStats.length > 1 && internals.previousNodesStats[1].timestamp + 10000 < now) {
      internals.previousNodesStats.shift();
    }

    let regex;
    if (req.query.filter !== undefined) {
      regex = new RegExp(req.query.filter);
    }

    const nodeKeys = Object.keys(nodesStats.nodes);
    for (let n = 0, nlen = nodeKeys.length; n < nlen; n++) {
      let node = nodesStats.nodes[nodeKeys[n]];

      if (nodeKeys[n] === 'timestamp' || (regex && !node.name.match(regex))) { continue; }

      let read = 0;
      let write = 0;
      let rejected = 0;
      let completed = 0;

      let writeInfo = node.thread_pool.bulk || node.thread_pool.write;

      const oldnode = internals.previousNodesStats[0][nodeKeys[n]];
      if (oldnode !== undefined && node.fs.io_stats !== undefined && oldnode.fs.io_stats !== undefined && 'total' in node.fs.io_stats) {
        const timediffsec = (node.timestamp - oldnode.timestamp)/1000.0;
        read = Math.max(0, Math.ceil((node.fs.io_stats.total.read_kilobytes - oldnode.fs.io_stats.total.read_kilobytes)/timediffsec*1024));
        write = Math.max(0, Math.ceil((node.fs.io_stats.total.write_kilobytes - oldnode.fs.io_stats.total.write_kilobytes)/timediffsec*1024));

        let writeInfoOld = oldnode.thread_pool.bulk || oldnode.thread_pool.write;

        completed = Math.max(0, Math.ceil((writeInfo.completed - writeInfoOld.completed)/timediffsec));
        rejected = Math.max(0, Math.ceil((writeInfo.rejected - writeInfoOld.rejected)/timediffsec));
      }

      const ip = (node.ip ? node.ip.split(':')[0] : node.host);

      let threadpoolInfo;
      let version = "";
      let molochtype;
      if (nodesInfo.nodes[nodeKeys[n]]) {
        threadpoolInfo = nodesInfo.nodes[nodeKeys[n]].thread_pool.bulk || nodesInfo.nodes[nodeKeys[n]].thread_pool.write;
        version = nodesInfo.nodes[nodeKeys[n]].version;
        if (nodesInfo.nodes[nodeKeys[n]].attributes) {
          molochtype = nodesInfo.nodes[nodeKeys[n]].attributes.molochtype;
        }
      } else {
        threadpoolInfo = { queue_size: 0 };
      }

      stats.push({
        name: node.name,
        ip: ip,
        ipExcluded: ipExcludes.includes(ip),
        nodeExcluded: nodeExcludes.includes(node.name),
        storeSize: node.indices.store.size_in_bytes,
        freeSize: node.roles.includes("data")?node.fs.total.available_in_bytes:0,
        docs: node.indices.docs.count,
        searches: node.indices.search.query_current,
        searchesTime: node.indices.search.query_time_in_millis,
        heapSize: node.jvm.mem.heap_used_in_bytes,
        nonHeapSize: node.jvm.mem.non_heap_used_in_bytes,
        cpu: node.process.cpu.percent,
        read: read,
        write: write,
        writesRejected: writeInfo.rejected,
        writesCompleted: writeInfo.completed,
        writesRejectedDelta: rejected,
        writesCompletedDelta: completed,
        writesQueueSize: threadpoolInfo.queue_size,
        load: node.os.load_average !== undefined ? /* ES 2*/ node.os.load_average : /*ES 5*/ node.os.cpu.load_average["5m"],
        version: version,
        molochtype: molochtype,
        roles: node.roles,
        isMaster: (master.length > 0 && node.name === master[0].node)
      });
    }

    if (req.query.sortField && stats.length > 1) {
      let field = req.query.sortField === 'nodeName'?'name':req.query.sortField;
      if (typeof(stats[0][field]) === 'string') {
        if (req.query.desc === 'true') {
          stats = stats.sort(function(a,b){ return b[field].localeCompare(a[field]); });
        } else {
          stats = stats.sort(function(a,b){ return a[field].localeCompare(b[field]); });
        }
      } else {
        if (req.query.desc === 'true') {
          stats = stats.sort(function(a,b){ return b[field] - a[field]; });
        } else {
          stats = stats.sort(function(a,b){ return a[field] - b[field]; });
        }
      }
    }

    nodesStats.nodes.timestamp = new Date().getTime();
    internals.previousNodesStats.push(nodesStats.nodes);

    r = {
      health: health,
      recordsTotal: nodeKeys.length,
      recordsFiltered: stats.length,
      data: stats
    };

    res.send(r);
  }).catch((err) => {
    console.log ('ERROR -  /esstats.json', err);
    r = {
      health: Db.healthCache(),
      recordsTotal: 0,
      recordsFiltered: 0,
      data: []
    };
    return res.send(r);
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

// No auth necessary for parliament.json
app.get('/parliament.json', [noCacheJson], (req, res) => {
  let query = {
    size: 500,
    _source: [
      'ver', 'nodeName', 'currentTime', 'monitoring', 'deltaBytes', 'deltaPackets', 'deltaMS',
      'deltaESDropped', 'deltaDropped', 'deltaOverloadDropped'
    ]
  };

  Promise.all([Db.search('stats', 'stat', query), Db.numberOfDocuments('stats')])
    .then(([stats, total]) => {
      if (stats.error) { throw stats.error; }

      let results = { total: stats.hits.total, results: [] };

      for (let i = 0, ilen = stats.hits.hits.length; i < ilen; i++) {
        let fields = stats.hits.hits[i]._source || stats.hits.hits[i].fields;

        if (stats.hits.hits[i]._source) {
          mergeUnarray(fields, stats.hits.hits[i].fields);
        }
        fields.id = stats.hits.hits[i]._id;

        // make sure necessary fields are not undefined
        let keys = [ 'deltaOverloadDropped', 'monitoring', 'deltaESDropped' ];
        for (const key of keys) {
          fields[key] = fields[key] || 0;
        }

        fields.deltaBytesPerSec         = Math.floor(fields.deltaBytes * 1000.0/fields.deltaMS);
        fields.deltaPacketsPerSec       = Math.floor(fields.deltaPackets * 1000.0/fields.deltaMS);
        fields.deltaESDroppedPerSec     = Math.floor(fields.deltaESDropped * 1000.0/fields.deltaMS);
        fields.deltaTotalDroppedPerSec  = Math.floor((fields.deltaDropped + fields.deltaOverloadDropped) * 1000.0/fields.deltaMS);

        results.results.push(fields);
      }

      res.send({
        data: results.results,
        recordsTotal: total.count,
        recordsFiltered: results.total
      });
    }).catch((err) => {
      console.log('ERROR - /parliament.json', err);
      res.send({ recordsTotal: 0, recordsFiltered: 0, data: [] });
    });
});

app.get('/stats.json', [noCacheJson, recordResponseTime, checkPermissions(['hideStats'])], (req, res) => {
  let query = {
    from: 0,
    size: 10000,
    query: {
      bool: {
        must: [],
        should: [],
        must_not: [
          { term: { hide: true } }
        ]
      }
    }
  };

  if (req.query.filter !== undefined && req.query.filter !== '') {
    const names = req.query.filter.split(',');
    for (let name of names) {
      name = name.trim();
      if (name !== '') {
        query.query.bool.should.push({
          wildcard: { nodeName: '*' + name + '*' }
        });
      }
    }
  }

  let rquery = {
    query: {term: {locked: 0}},
    size: 0,
    aggregations: {
      buckets: {
        terms: {field: "node", size: 1000},
        aggregations: {
          first: {min: {field: "first"}}
        }
      }
    }
  };

  if (req.query.hide !== undefined && req.query.hide !== 'none') {
    if (req.query.hide === 'old' || req.query.hide === 'both') {
      query.query.bool.must.push({ range: { currentTime: { gte: 'now-5m'} } });
    }
    if (req.query.hide === 'nosession' || req.query.hide === 'both') {
      query.query.bool.must.push({ range: { monitoring: { gte: '1'} } });
    }
  }

  let now = Math.floor(Date.now() / 1000);

  Promise.all([Db.search('stats', 'stat', query),
               Db.numberOfDocuments('stats'),
               Db.search('files', 'file', rquery)
  ]).then(([stats, total, retention]) => {
    if (stats.error) { throw stats.error; }

    if (retention.aggregations.buckets && retention.aggregations.buckets.buckets) {
      retention = arrayToObject(retention.aggregations.buckets.buckets, "key");
    } else {
      retention = {};
    }

    let results = { total: stats.hits.total, results: [] };

    for (let i = 0, ilen = stats.hits.hits.length; i < ilen; i++) {
      let fields = stats.hits.hits[i]._source || stats.hits.hits[i].fields;
      if (stats.hits.hits[i]._source) {
        mergeUnarray(fields, stats.hits.hits[i].fields);
      }
      fields.id = stats.hits.hits[i]._id;

      if (retention[fields.id]) {
        fields.retention                  = now - retention[fields.id].first.value;
      } else {
        fields.retention                  = 0;
      }

      fields.deltaBytesPerSec           = Math.floor(fields.deltaBytes * 1000.0/fields.deltaMS);
      fields.deltaWrittenBytesPerSec    = Math.floor(fields.deltaWrittenBytes * 1000.0/fields.deltaMS);
      fields.deltaUnwrittenBytesPerSec  = Math.floor(fields.deltaUnwrittenBytes * 1000.0/fields.deltaMS);
      fields.deltaBitsPerSec            = Math.floor(fields.deltaBytes * 1000.0/fields.deltaMS * 8);
      fields.deltaPacketsPerSec         = Math.floor(fields.deltaPackets * 1000.0/fields.deltaMS);
      fields.deltaSessionsPerSec        = Math.floor(fields.deltaSessions * 1000.0/fields.deltaMS);
      fields.deltaSessionBytesPerSec    = Math.floor(fields.deltaSessionBytes * 1000.0/fields.deltaMS);
      fields.sessionSizePerSec          = Math.floor(fields.deltaSessionBytes/fields.deltaSessions);
      fields.deltaDroppedPerSec         = Math.floor(fields.deltaDropped * 1000.0/fields.deltaMS);
      fields.deltaFragsDroppedPerSec    = Math.floor(fields.deltaFragsDropped * 1000.0/fields.deltaMS);
      fields.deltaOverloadDroppedPerSec = Math.floor(fields.deltaOverloadDropped * 1000.0/fields.deltaMS);
      fields.deltaESDroppedPerSec       = Math.floor(fields.deltaESDropped * 1000.0/fields.deltaMS);
      fields.deltaTotalDroppedPerSec    = Math.floor((fields.deltaDropped + fields.deltaOverloadDropped) * 1000.0/fields.deltaMS);
      results.results.push(fields);
    }

    // sort after all the results are aggregated
    req.query.sortField = req.query.sortField || 'nodeName';
    if (results.results[0] && results.results[0][req.query.sortField]) { // make sure the field exists to sort on
      results.results = results.results.sort((a, b) => {
        if (req.query.desc === 'true') {
          if (!isNaN(a[req.query.sortField])) {
            return b[req.query.sortField] - a[req.query.sortField];
          } else {
            return b[req.query.sortField].localeCompare(a[req.query.sortField]);
          }
        } else {
          if (!isNaN(a[req.query.sortField])) {
            return a[req.query.sortField] - b[req.query.sortField];
          } else {
            return a[req.query.sortField].localeCompare(b[req.query.sortField]);
          }
        }
      });
    }

    let from = +req.query.start || 0;
    let stop = from + (+req.query.length || 500);

    let r = {
      recordsTotal: total.count,
      recordsFiltered: results.results.length,
      data: results.results.slice(from, stop)
    };

    res.send(r);
  }).catch((err) => {
    console.log('ERROR - /stats.json', query, err);
    res.send({ recordsTotal: 0, recordsFiltered: 0, data: [] });
  });
});

app.get('/dstats.json', [noCacheJson, checkPermissions(['hideStats'])], (req, res) => {
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
    deltaWrittenBytesPerSec: {_source: ["deltaWrittenBytes", "deltaMS"], func: function(item) {return Math.floor(item.deltaWrittenBytes * 1000.0/item.deltaMS);}},
    deltaUnwrittenBytesPerSec: {_source: ["deltaUnwrittenBytes", "deltaMS"], func: function(item) {return Math.floor(item.deltaUnwrittenBytes * 1000.0/item.deltaMS);}},
    deltaPacketsPerSec: {_source: ["deltaPackets", "deltaMS"], func: function(item) {return Math.floor(item.deltaPackets * 1000.0/item.deltaMS);}},
    deltaSessionsPerSec: {_source: ["deltaSessions", "deltaMS"], func: function(item) {return Math.floor(item.deltaSessions * 1000.0/item.deltaMS);}},
    deltaSessionBytesPerSec: {_source: ["deltaSessionBytes", "deltaMS"], func: function(item) {return Math.floor(item.deltaSessionBytes * 1000.0/item.deltaMS);}},
    sessionSizePerSec: {_source: ["deltaSessionBytes", "deltaSessions"], func: function(item) {return Math.floor(item.deltaSessionBytes/item.deltaSessions);}},
    deltaDroppedPerSec: {_source: ["deltaDropped", "deltaMS"], func: function(item) {return Math.floor(item.deltaDropped * 1000.0/item.deltaMS);}},
    deltaFragsDroppedPerSec: {_source: ["deltaFragsDropped", "deltaMS"], func: function(item) {return Math.floor(item.deltaFragsDropped * 1000.0/item.deltaMS);}},
    deltaOverloadDroppedPerSec: {_source: ["deltaOverloadDropped", "deltaMS"], func: function(item) {return Math.floor(item.deltaOverloadDropped * 1000.0/item.deltaMS);}},
    deltaESDroppedPerSec: {_source: ["deltaESDropped", "deltaMS"], func: function(item) {return Math.floor(item.deltaESDropped * 1000.0/item.deltaMS);}},
    deltaTotalDroppedPerSec: {_source: ["deltaDropped", "deltaOverloadDropped", "deltaMS"], func: function(item) {return Math.floor((item.deltaDropped + item.deltaOverloadDropped) * 1000.0/item.deltaMS);}},
    cpu: {_source: ["cpu"], func: function (item) {return item.cpu * 0.01;}}
  };

  query._source = mapping[req.query.name]?mapping[req.query.name]._source:[req.query.name];
  query._source.push("nodeName", "currentTime");

  var func = mapping[req.query.name]?mapping[req.query.name].func:function(item) {return item[req.query.name];};

  Db.searchScroll('dstats', 'dstat', query, {filter_path: "_scroll_id,hits.total,hits.hits._source"}, function(err, result) {
    if (err || result.error) {
      console.log("ERROR - dstats", query, err || result.error);
    }
    var i, ilen;
    var data = {};
    var num = (req.query.stop - req.query.start)/req.query.step;

    var mult = 1;
    if (req.query.name === "freeSpaceM" || req.query.name === "usedSpaceM") {
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

app.get('/:nodeName/:fileNum/filesize.json', [noCacheJson, checkPermissions(['hideFiles'])], (req, res) => {
  Db.fileIdToFile(req.params.nodeName, req.params.fileNum, (file) => {
    if (!file) {
      return res.send({filesize: -1});
    }

    fs.stat(file.name, (err, stats) => {
      if (err || !stats) {
        return res.send({filesize: -1});
      } else {
        return res.send({filesize: stats.size});
      }
    });
  });
});

function mapMerge (aggregations) {
  let map = { src: {}, dst: {}, xffGeo: {} };

  if (!aggregations || !aggregations.mapG1) {
    return {};
  }

  aggregations.mapG1.buckets.forEach(function (item) {
    map.src[item.key] = item.doc_count;
  });

  aggregations.mapG2.buckets.forEach(function (item) {
    map.dst[item.key] = item.doc_count;
  });

  aggregations.mapG3.buckets.forEach(function (item) {
    map.xffGeo[item.key] = item.doc_count;
  });

  return map;
}

function graphMerge(req, query, aggregations) {
  let graph = {
    lpHisto: [],
    db1Histo: [],
    db2Histo: [],
    pa1Histo: [],
    pa2Histo: [],
    by1Histo: [],
    by2Histo: [],
    xmin: req.query.startTime * 1000|| null,
    xmax: req.query.stopTime * 1000 || null,
    interval: query.aggregations?query.aggregations.dbHisto.histogram.interval / 1000 || 60 : 60
  };

  if (!aggregations || !aggregations.dbHisto) {
    return graph;
  }

  graph.interval = query.aggregations?(query.aggregations.dbHisto.histogram.interval / 1000) || 60 : 60;

  aggregations.dbHisto.buckets.forEach(function (item) {
    let key = item.key;
    graph.lpHisto.push([key, item.doc_count]);
    graph.pa1Histo.push([key, item.srcPackets.value]);
    graph.pa2Histo.push([key, item.dstPackets.value]);
    graph.db1Histo.push([key, item.srcDataBytes.value]);
    graph.db2Histo.push([key, item.dstDataBytes.value]);
    graph.by1Histo.push([key, item.srcBytes.value]);
    graph.by2Histo.push([key, item.dstBytes.value]);
  });

  return graph;
}

function fixFields(fields, fixCb) {
  if (!fields.fileId) {
    fields.fileId = [];
    return fixCb(null, fields);
  }

  var files = [];
  async.forEachSeries(fields.fileId, function (item, cb) {
    Db.fileIdToFile(fields.node, item, function (file) {
      if (file && file.locked === 1) {
        files.push(file.name);
      }
      cb(null);
    });
  },
  function(err) {
    fields.fileId = files;
    fixCb(err, fields);
  });
}

/**
 * Flattens fields that are objects (only goes 1 level deep)
 *
 * @example
 * { http: { statuscode: [200, 302] } } => { "http.statuscode": [200, 302] }
 * @example
 * { cert: [ { alt: ["test.com"] } ] } => { "cert.alt": ["test.com"] }
 *
 * @param {object} fields The object containing fields to be flattened
 * @returns {object} fields The object with fields flattened
 */
function flattenFields(fields) {
  let newFields = {};

  for (let key in fields) {
    if (fields.hasOwnProperty(key)) {
      let field = fields[key];
      let baseKey = key + '.';
      if (typeof field === 'object' && !field.length) {
        // flatten out object
        for (let nestedKey in field) {
          if (field.hasOwnProperty(nestedKey)) {
            let nestedField = field[nestedKey];
            let newKey = baseKey + nestedKey;
            newFields[newKey] = nestedField;
          }
        }
        fields[key] = null;
        delete fields[key];
      } else if (Array.isArray(field)) {
        // flatten out list
        for (let nestedField of field) {
          if (typeof nestedField === 'object') {
            for (let nestedKey in nestedField) {
              let newKey = baseKey + nestedKey;
              if (newFields[newKey] === undefined) {
                newFields[newKey] = nestedField[nestedKey];
              } else if (Array.isArray(newFields[newKey])) {
                newFields[newKey].push(nestedField[nestedKey]);
              } else {
                newFields[newKey] = [newFields[newKey], nestedField[nestedKey]];
              }
            }
            fields[key] = null;
            delete fields[key];
          }
        }
      }
    }
  }

  for (let key in newFields) {
    if (newFields.hasOwnProperty(key)) {
      fields[key] = newFields[key];
    }
  }

  return fields;
}

app.use('/buildQuery.json', [noCacheJson, logAction('query')], function(req, res, next) {

  if (req.method === "POST") {
    req.query = req.body;
  } else if (req.method !== "GET") {
    next();
  }

  buildSessionQuery(req, function(bsqErr, query, indices) {
    if (bsqErr) {
      res.send({ recordsTotal: 0,
                 recordsFiltered: 0,
                 bsqErr: bsqErr.toString()
               });
      return;
    }

    if (req.query.fields) {
      query._source = queryValueToArray(req.query.fields);
    }

    res.send({"esquery": query, "indices": indices});
  });
});

app.get('/sessions.json', [noCacheJson, logAction('sessions'), recordResponseTime], function (req, res) {
  var graph = {};
  var map = {};

  let options;
  if (req.query.cancelId) { options = { cancelId: `${req.user.userId}::${req.query.cancelId}` }; }

  buildSessionQuery(req, function (bsqErr, query, indices) {
    if (bsqErr) {
      const r = {
        recordsTotal: 0,
        recordsFiltered: 0,
        graph: {},
        map: {},
        bsqErr: bsqErr.toString(),
        health: Db.healthCache(),
        data:[]
      };
      return res.send(r);
    }

    let addMissing = false;
    if (req.query.fields) {
      query._source = queryValueToArray(req.query.fields);
      ['node', 'srcIp', 'srcPort', 'dstIp', 'dstPort'].forEach((item) => {
        if (query._source.indexOf(item) === -1) {
          query._source.push(item);
        }
      });
    } else {
      addMissing = true;
      query._source = [
        'ipProtocol', 'rootId', 'totDataBytes', 'srcDataBytes',
        'dstDataBytes', 'firstPacket', 'lastPacket', 'srcIp', 'srcPort',
        'dstIp', 'dstPort', 'totPackets', 'srcPackets', 'dstPackets',
        'totBytes', 'srcBytes', 'dstBytes', 'node', 'http.uri', 'srcGEO',
        'dstGEO', 'email.subject', 'email.src', 'email.dst', 'email.filename',
        'dns.host', 'cert', 'irc.channel', 'http.xffGEO'
      ];
    }

    if (query.aggregations && query.aggregations.dbHisto) {
      graph.interval = query.aggregations.dbHisto.histogram.interval;
    }

    if (Config.debug) {
      console.log(`sessions.json ${indices} query`, JSON.stringify(query, null, 1));
    }

    Promise.all([Db.searchPrimary(indices, 'session', query, options),
                 Db.numberOfDocuments('sessions2-*'),
                 Db.healthCachePromise()
    ]).then(([sessions, total, health]) => {
      if (Config.debug) {
        console.log('sessions.json result', util.inspect(sessions, false, 50));
      }

      if (sessions.error) { throw sessions.err; }

      graph = graphMerge(req, query, sessions.aggregations);
      map = mapMerge(sessions.aggregations);

      var results = {total: sessions.hits.total, results: []};
      async.each(sessions.hits.hits, function (hit, hitCb) {
        var fields = hit._source || hit.fields;
        if (fields === undefined) {
          return hitCb(null);
        }
        //fields.index = hit._index;
        fields.id = Db.session2Sid(hit);

        if (req.query.flatten === '1') {
          fields = flattenFields(fields);
        }

        if (addMissing) {
          ['srcPackets', 'dstPackets', 'srcBytes', 'dstBytes', 'srcDataBytes', 'dstDataBytes'].forEach(function(item) {
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
        var r = {recordsTotal: total.count,
                 recordsFiltered: (results?results.total:0),
                 graph: graph,
                 health: health,
                 map: map,
                 data: (results?results.results:[])};
        res.logCounts(r.data.length, r.recordsFiltered, r.recordsTotal);
        try {
          res.send(r);
        } catch (c) {
        }
      });
    }).catch ((err) => {
      console.log('ERROR - /sessions.json error', err);
      var r = {recordsTotal: 0,
               recordsFiltered: 0,
               graph: {},
               map: {},
               health: Db.healthCache(),
               data:[]};
      res.send(r);
    });
  });
});

app.get('/spigraph.json', [noCacheJson, logAction('spigraph'), fieldToExp, recordResponseTime], function(req, res) {
  req.query.facets = 1;

  buildSessionQuery(req, function(bsqErr, query, indices) {
    var results = {items: [], graph: {}, map: {}};
    if (bsqErr) {
      return res.molochError(403, bsqErr.toString());
    }

    let options;
    if (req.query.cancelId) { options = { cancelId: `${req.user.userId}::${req.query.cancelId}` }; }

    delete query.sort;
    query.size = 0;
    var size = +req.query.size || 20;

    var field = req.query.field || 'node';

    if (req.query.exp === 'ip.dst:port') { field = 'ip.dst:port'; }

    if (field === 'ip.dst:port') {
      query.aggregations.field = { terms: { field: 'dstIp', size: size }, aggregations: { sub: { terms: { field: 'dstPort', size: size } } } };
    } else if (field === 'fileand') {
      query.aggregations.field = { terms: { field: 'node', size: 1000 }, aggregations: { sub: { terms: { field: 'fileId', size: size } } } };
    } else {
      query.aggregations.field = { terms: { field: field, size: size * 2 } };
    }

    Promise.all([
      Db.healthCachePromise(),
      Db.numberOfDocuments('sessions2-*'),
      Db.searchPrimary(indices, 'session', query, options)
    ]).then(([health, total, result]) => {
      if (result.error) { throw result.error; }

      results.health = health;
      results.recordsTotal = total.count;
      results.recordsFiltered = result.hits.total;

      results.graph = graphMerge(req, query, result.aggregations);
      results.map = mapMerge(result.aggregations);

      if (!result.aggregations) {
        result.aggregations = {field: {buckets: []}};
      }

      let aggs = result.aggregations.field.buckets;
      let filter = { term: {} };
      let sfilter = { term: {} };
      query.query.bool.filter.push(filter);

      if (field === 'ip.dst:port') {
        query.query.bool.filter.push(sfilter);
      }

      delete query.aggregations.field;

      let queriesInfo = [];
      function endCb () {
        queriesInfo = queriesInfo.sort((a, b) => {return b.doc_count - a.doc_count;}).slice(0, size * 2);
        let queries = queriesInfo.map((item) => {return item.query;});

        Db.msearch(indices, 'session', queries, options, function(err, result) {
          if (!result.responses) {
            return res.send(results);
          }

          result.responses.forEach(function(item, i) {
            var r = {name: queriesInfo[i].key, count: queriesInfo[i].doc_count};

            r.graph = graphMerge(req, query, result.responses[i].aggregations);
            if (r.graph.xmin === null) {
              r.graph.xmin = results.graph.xmin || results.graph.pa1Histo[0][0];
            }

            if (r.graph.xmax === null) {
              r.graph.xmax = results.graph.xmax || results.graph.pa1Histo[results.graph.pa1Histo.length - 1][0];
            }

            r.map = mapMerge(result.responses[i].aggregations);
            results.items.push(r);
            r.lpHisto = 0.0;
            r.dbHisto = 0.0;
            r.byHisto = 0.0;
            r.paHisto = 0.0;
            var graph = r.graph;
            for (let i = 0; i < graph.lpHisto.length; i++) {
              r.lpHisto += graph.lpHisto[i][1];
              r.dbHisto += graph.db1Histo[i][1] + graph.db2Histo[i][1];
              r.byHisto += graph.by1Histo[i][1] + graph.by2Histo[i][1];
              r.paHisto += graph.pa1Histo[i][1] + graph.pa2Histo[i][1];
            }
            if (results.items.length === result.responses.length) {
              var s = req.query.sort || 'lpHisto';
              results.items = results.items.sort(function (a, b) {
                var result;
                if (s === 'name') { result = a.name.localeCompare(b.name); }
                else { result = b[s] - a[s]; }
                return result;
              }).slice(0, size);
              return res.send(results);
            }
          });
        });
      }

      let intermediateResults = [];
      function findFileNames () {
        async.each(intermediateResults, function (fsitem, cb) {
          let split = fsitem.key.split(':');
          let node = split[0];
          let fileId = split[1];
          Db.fileIdToFile(node, fileId, function (file) {
            if (file && file.name) {
              queriesInfo.push({ key: file.name, doc_count: fsitem.doc_count, query: fsitem.query });
            }
            cb();
          });
        }, function () {
          endCb();
        });
      }

      aggs.forEach((item) => {
        if (field === 'ip.dst:port') {
          filter.term.dstIp = item.key;
          let sep = (item.key.indexOf(":") === -1)? ':' : '.';
          item.sub.buckets.forEach((sitem) => {
            sfilter.term.dstPort = sitem.key;
            queriesInfo.push({key: item.key + sep + sitem.key, doc_count: sitem.doc_count, query: JSON.stringify(query)});
          });
        } else if (field === 'fileand') {
          filter.term.node = item.key;
          item.sub.buckets.forEach((sitem) => {
            sfilter.term.fileand = sitem.key;
            intermediateResults.push({key: filter.term.node + ':' + sitem.key, doc_count: sitem.doc_count, query: JSON.stringify(query)});
          });
        } else {
          filter.term[field] = item.key;
          queriesInfo.push({key: item.key, doc_count: item.doc_count, query: JSON.stringify(query)});
        }
      });

      if (field === 'fileand') { return findFileNames(); }

      return endCb();
    }).catch((err) => {
      console.log('spigraph.json error', err);
      return res.molochError(403, errorString(err));
    });
  });
});

app.get('/spiview.json', [noCacheJson, logAction('spiview'), recordResponseTime], function(req, res) {

  if (req.query.spi === undefined) {
    return res.send({spi:{}, recordsTotal: 0, recordsFiltered: 0});
  }

  var spiDataMaxIndices = +Config.get("spiDataMaxIndices", 4);

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
      query.aggregations.protocols = {terms: {field: "protocol", size:1000}};
    }

    queryValueToArray(req.query.spi).forEach(function (item) {
      var parts = item.split(":");
      if (parts[0] === "fileand") {
        query.aggregations[parts[0]] = {terms: {field: "node", size: 1000}, aggregations: {fileId: {terms: {field: "fileId", size: parts.length>1?parseInt(parts[1],10):10}}}};
      } else {
        query.aggregations[parts[0]] = {terms: {field: parts[0]}};

        if (parts.length > 1) {
          query.aggregations[parts[0]].terms.size = parseInt(parts[1], 10);
        }
      }
    });
    query.size = 0;

    // console.log("spiview.json query", JSON.stringify(query), "indices", indices);

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
        Db.searchPrimary(indices, 'session', query, null, function (err, result) {
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

          if (result.aggregations.ipProtocol) {
            result.aggregations.ipProtocol.buckets.forEach(function (item) {
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
            delete result.aggregations.byHisto;
            delete result.aggregations.mapG1;
            delete result.aggregations.mapG2;
            delete result.aggregations.mapG3;
            delete result.aggregations.protocols;
          }

          sessionsCb(null, result.aggregations);
        });
      },
      total: function (totalCb) {
        Db.numberOfDocuments('sessions2-*', totalCb);
      },
      health: Db.healthCache
    },
    function(err, results) {
      function sendResult() {
        r = {health: results.health,
             recordsTotal: results.total,
             spi: results.spi,
             recordsFiltered: recordsFiltered,
             graph: graph,
             map: map,
             protocols: protocols,
             bsqErr: bsqErr
        };
        res.logCounts(r.spi.count, r.recordsFiltered, r.total);
        try {
          res.send(r);
        } catch (c) {
        }
      }

      if (!results.spi.fileand) {
        return sendResult();
      }

      var nresults = [];
      var sodc = 0;
      async.each(results.spi.fileand.buckets, function(nobucket, cb) {
        sodc += nobucket.fileId.sum_other_doc_count;
        async.each(nobucket.fileId.buckets, function (fsitem, cb) {
          Db.fileIdToFile(nobucket.key, fsitem.key, function(file) {
            if (file && file.name) {
              nresults.push({key: file.name, doc_count: fsitem.doc_count});
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
        return sendResult();
      });
    });
  });
});

app.get('/dns.json', [noCacheJson, logAction()], function(req, res) {
  console.log("dns.json", req.query);
  dns.reverse(req.query.ip, function (err, data) {
    if (err) {
      return res.send({hosts: []});
    }
    return res.send({hosts: data});
  });
});

function buildConnections(req, res, cb) {
  let dstipport;
  if (req.query.dstField === 'ip.dst:port') {
    dstipport = true;
    req.query.dstField = 'dstIp';
  }

  req.query.srcField       = req.query.srcField || 'srcIp';
  req.query.dstField       = req.query.dstField || 'dstIp';
  req.query.iDisplayLength = req.query.iDisplayLength || '5000';
  let fsrc                 = req.query.srcField;
  let fdst                 = req.query.dstField;
  let minConn              = req.query.minConn || 1;

  let dstIsIp = fdst.match(/(\.ip|Ip)$/);

  let nodesHash = {};
  let connects = {};

  let dbFieldsMap = Config.getDBFieldsMap();
  function updateValues (data, property, fields) {
    for (let i in fields) {
      let dbField = fields[i];
      let field = dbFieldsMap[dbField];
      if (data.hasOwnProperty(dbField)) {
        // sum integers
        if (field.type === 'integer' && field.category !== 'port') {
          property[dbField] = (property[dbField] || 0) + data[dbField];
        } else { // make a list of values
          if (!property[dbField]) { property[dbField] = []; }
          // make all values an array (because sometimes they are by default)
          let values = [ data[dbField] ];
          if (Array.isArray(data[dbField])) {
            values = data[dbField];
          }
          for (let value of values) {
            property[dbField].push(value);
          }
          if (property[dbField] && Array.isArray(property[dbField])) {
            property[dbField] = [ ...new Set(property[dbField]) ]; // unique only
          }
        }
      }
    }
  }

  function process (vsrc, vdst, f, fields) {
    // ES 6 is returning formatted timestamps instead of ms like pre 6 did
    // https://github.com/elastic/elasticsearch/issues/27740
    if (vsrc.length === 24 && vsrc[23] === 'Z' && vsrc.match(/^\d\d\d\d-\d\d-\d\dT\d\d:\d\d:\d\d.\d\d\dZ$/)) {
      vsrc = new Date(vsrc).getTime();
    }
    if (vdst.length === 24 && vdst[23] === 'Z' && vdst.match(/^\d\d\d\d-\d\d-\d\dT\d\d:\d\d:\d\d.\d\d\dZ$/)) {
      vdst = new Date(vdst).getTime();
    }

    if (nodesHash[vsrc] === undefined) {
      nodesHash[vsrc] = { id: `${vsrc}`, cnt: 0, sessions: 0 };
    }

    nodesHash[vsrc].sessions++;
    nodesHash[vsrc].type |= 1;
    updateValues(f, nodesHash[vsrc], fields);

    if (nodesHash[vdst] === undefined) {
      nodesHash[vdst] = { id: `${vdst}`, cnt: 0, sessions: 0 };
    }

    nodesHash[vdst].sessions++;
    nodesHash[vdst].type |= 2;
    updateValues(f, nodesHash[vdst], fields);

    let linkId = `${vsrc}->${vdst}`;
    if (connects[linkId] === undefined) {
      connects[linkId] = { value: 0, source: vsrc, target: vdst };
      nodesHash[vsrc].cnt++;
      nodesHash[vdst].cnt++;
    }

    connects[linkId].value++;
    updateValues(f, connects[linkId], fields);
  }

  buildSessionQuery(req, function(bsqErr, query, indices) {
    if (bsqErr) {
      return cb(bsqErr, 0, 0, 0);
    }
    query.query.bool.filter.push({exists: {field: req.query.srcField}});
    query.query.bool.filter.push({exists: {field: req.query.dstField}});

    // get the requested fields
    let fields = ['totBytes', 'totDataBytes', 'totPackets', 'node'];
    if (req.query.fields) { fields = req.query.fields.split(','); }
    query._source = fields;
    query.docvalue_fields = [fsrc, fdst];

    if (dstipport) {
      query._source.push('dstPort');
    }

    let options;
    if (req.query.cancelId) { options = { cancelId: `${req.user.userId}::${req.query.cancelId}` }; }

    if (Config.debug) {
      console.log('buildConnections query', JSON.stringify(query, null, 2));
    }

    Db.searchPrimary(indices, 'session', query, options, function (err, graph) {
      if (Config.debug) {
        console.log('buildConnections result', JSON.stringify(graph, null, 2));
      }

      if (err || graph.error) {
        console.log('Build Connections ERROR', err, graph.error);
        return cb(err || graph.error);
      }

      async.eachLimit(graph.hits.hits, 10, function (hit, hitCb) {
        let f = hit._source;
        f = flattenFields(f);

        let asrc = hit.fields[fsrc];
        let adst = hit.fields[fdst];

        if (asrc === undefined || adst === undefined) {
          return setImmediate(hitCb);
        }

        if (!Array.isArray(asrc)) {
          asrc = [asrc];
        }

        if (!Array.isArray(adst)) {
          adst = [adst];
        }

        for (let vsrc of asrc) {
          for (let vdst of adst) {
            if (dstIsIp && dstipport) {
              if (vdst.includes(':')) {
                vdst += '.' + f.dstPort;
              } else {
                vdst += ':' + f.dstPort;
              }
            }
            process(vsrc, vdst, f, fields);
          }
        }
        setImmediate(hitCb);
      }, function (err) {
        let nodes = [];
        let nodeKeys = Object.keys(nodesHash);
        if (Config.get('regressionTests', false)) {
          nodeKeys = nodeKeys.sort(function (a,b) { return nodesHash[a].id.localeCompare(nodesHash[b].id); });
        }
        for (let node of nodeKeys) {
          if (nodesHash[node].cnt < minConn) {
            nodesHash[node].pos = -1;
          } else {
            nodesHash[node].pos = nodes.length;
            nodes.push(nodesHash[node]);
          }
        }

        let links = [];
        for (let key in connects) {
          var c = connects[key];
          c.source = nodesHash[c.source].pos;
          c.target = nodesHash[c.target].pos;
          if (c.source >= 0 && c.target >= 0) {
            links.push(connects[key]);
          }
        }

        if (Config.debug) {
          console.log('nodesHash', nodesHash);
          console.log('connects', connects);
          console.log('nodes', nodes.length, nodes);
          console.log('links', links.length, links);
        }

        return cb(null, nodes, links, graph.hits.total);
      });
    });
  });
}

app.get('/connections.json', [noCacheJson, logAction('connections'), recordResponseTime], function (req, res) {
  let health;
  Db.healthCache(function (err, h) { health = h; });
  buildConnections(req, res, function (err, nodes, links, total) {
    if (err) { return res.molochError(403, err.toString()); }
    res.send({ health: health, nodes: nodes, links: links, recordsFiltered: total });
  });
});

app.get('/connections.csv', logAction(), function(req, res) {
  noCache(req, res, "text/csv");

  var seperator = req.query.seperator || ",";
  buildConnections(req, res, function (err, nodes, links, total) {
    if (err) {
      return res.send(err);
    }

    // write out the fields requested
    let fields = ['totBytes', 'totDataBytes', 'totPackets', 'node'];
    if (req.query.fields) { fields = req.query.fields.split(','); }

    res.write("Source, Destination, Sessions");
    let displayFields = {};
    for (let field of fields) {
      let fieldsMap = JSON.parse(app.locals.fieldsMap);
      for (let f in fieldsMap) {
        if (fieldsMap[f].dbField === field) {
          let friendlyName = fieldsMap[f].friendlyName;
          displayFields[field] = fieldsMap[f];
          res.write(`, ${friendlyName}`);
        }
      }
    }
    res.write('\r\n');

    for (let i = 0, ilen = links.length; i < ilen; i++) {
      res.write("\"" + nodes[links[i].source].id.replace('"', '""') + "\"" + seperator +
                "\"" + nodes[links[i].target].id.replace('"', '""') + "\"" + seperator +
                     links[i].value + seperator);
      for (let f = 0, flen = fields.length; f < flen; f++) {
        res.write(links[i][displayFields[fields[f]].dbField].toString());
        if (f !== flen - 1) { res.write(seperator); }
      }
      res.write('\r\n');
    }

    res.end();
  });
});

function csvListWriter(req, res, list, fields, pcapWriter, extension) {
  if (list.length > 0 && list[0].fields) {
    list = list.sort(function(a,b){return a.fields.lastPacket - b.fields.lastPacket;});
  } else if (list.length > 0 && list[0]._source) {
    list = list.sort(function(a,b){return a._source.lastPacket - b._source.lastPacket;});
  }

  var fieldObjects  = Config.getDBFieldsMap();

  if (fields) {
    var columnHeaders = [];
    for (let i = 0, ilen = fields.length; i < ilen; ++i) {
      if (fieldObjects[fields[i]] !== undefined) {
        columnHeaders.push(fieldObjects[fields[i]].friendlyName);
      }
    }
    res.write(columnHeaders.join(', '));
    res.write('\r\n');
  }

  for (var j = 0, jlen = list.length; j < jlen; j++) {
    var sessionData = flattenFields(list[j]._source || list[j].fields);
    sessionData._id = list[j]._id;

    if (!fields) { continue; }

    var values = [];
    for (let k = 0, klen = fields.length; k < klen; ++k) {
      let value = sessionData[fields[k]];
      if (fields[k] === 'ipProtocol' && value) {
        value = Pcap.protocol2Name(value);
      }

      if (Array.isArray(value)) {
        let singleValue = '"' + value.join(', ') +  '"';
        values.push(singleValue);
      } else {
        if (value === undefined) {
          value = '';
        } else if (typeof(value) === 'string' && value.includes(',')) {
          if (value.includes('"')) {
            value = value.replace(/"/g, '""');
          }
          value = '"' + value + '"';
        }
        values.push(value);
      }
    }

    res.write(values.join(','));
    res.write('\r\n');
  }

  res.end();
}

app.get(/\/sessions.csv.*/, logAction(), function(req, res) {
  noCache(req, res, "text/csv");

  // default fields to display in csv
  var fields = ["ipProtocol", "firstPacket", "lastPacket", "srcIp", "srcPort", "srcGEO", "dstIp", "dstPort", "dstGEO", "totBytes", "totDataBytes", "totPackets", "node"];
  // save requested fields because sessionsListFromQuery returns fields with
  // "rootId" appended onto the end
  var reqFields = fields;

  if (req.query.fields) {
    fields = reqFields = queryValueToArray(req.query.fields);
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

app.get('/multiunique.txt', logAction(), function(req, res) {
  noCache(req, res, 'text/plain; charset=utf-8');

  if (req.query.exp === undefined) {
    return res.send("Missing exp parameter");
  }

  let fields = [];
  let parts = req.query.exp.split(',');
  for (let i = 0; i < parts.length; i++) {
    let field = Config.getFieldsMap()[parts[i]];
    if (!field) {
      return res.send(`Unknown expression ${parts[i]}\n`);
    }
    fields.push(field);
  }

  let separator = req.query.separator || ', ';
  let doCounts = parseInt(req.query.counts, 10) || 0;

  let results = [];
  function printUnique(buckets, line) {
    for (let i = 0; i < buckets.length; i++) {
      if (buckets[i].field) {
        printUnique(buckets[i].field.buckets, line + buckets[i].key + separator);
      } else {
        results.push({line: line + buckets[i].key, count: buckets[i].doc_count});
      }
    }
  }

  buildSessionQuery(req, function(err, query, indices) {
    delete query.sort;
    delete query.aggregations;
    query.size = 0;

    if (!query.query.bool.must) {
      query.query.bool.must = [];
    }

    let lastQ = query;
    for (let i = 0; i < fields.length; i++) {
      query.query.bool.must.push({ exists: { field: fields[i].dbField } });
      lastQ.aggregations = {field: { terms : {field : fields[i].dbField, size: +Config.get('maxAggSize', 10000)}}};
      lastQ = lastQ.aggregations.field;
    }

    if (Config.debug > 2) {
      console.log("multiunique aggregations", indices, JSON.stringify(query, false, 2));
    }
    Db.searchPrimary(indices, 'session', query, null, function (err, result) {
      if (err) {
        console.log('multiunique ERROR', err);
        res.status(400);
        return res.end(err);
      }

      if (Config.debug > 2) {
        console.log('result', JSON.stringify(result, false, 2));
      }
      printUnique(result.aggregations.field.buckets, "");

      if (req.query.sort !== 'field') {
        results = results.sort(function(a, b) {return b.count - a.count;});
      }

      if (doCounts) {
        for (let i = 0; i < results.length; i++) {
          res.write(results[i].line + separator + results[i].count + '\n');
        }
      } else {
        for (let i = 0; i < results.length; i++) {
          res.write(results[i].line + '\n');
        }
      }
      return res.end();
    });
  });
});

app.get('/unique.txt', [logAction(), fieldToExp], function(req, res) {
  noCache(req, res, 'text/plain; charset=utf-8');

  if (req.query.field === undefined && req.query.exp === undefined) {
    return res.send('Missing field or exp parameter');
  }

  /* How should the results be written.  Use setImmediate to not blow stack frame */
  let writeCb;
  let doneCb;
  let items = [];
  let aggSize = +Config.get('maxAggSize', 10000);

  if (req.query.autocomplete !== undefined) {
    if (!Config.get('valueAutoComplete', !Config.get('multiES', false))) {
      res.send([]);
      return;
    }

    let spiDataMaxIndices = +Config.get('spiDataMaxIndices', 4);
    if (spiDataMaxIndices !== -1) {
      if (req.query.date === '-1' ||
          (req.query.date !== undefined && +req.query.date > spiDataMaxIndices)) {
        console.log(`INFO For autocomplete replacing date=${safeStr(req.query.date)} with ${spiDataMaxIndices}`);
        req.query.date = spiDataMaxIndices;
      }
    }

    aggSize = 1000; // lower agg size for autocomplete
    doneCb = function() {
      res.send(items);
    };
    writeCb = function (item) {
      items.push(item.key);
    };
  } else if (parseInt(req.query.counts, 10) || 0) {
    writeCb = function (item) {
      res.write(`${item.key}, ${item.doc_count}\n`);
    };
  } else {
    writeCb = function (item) {
      res.write(`${item.key}\n`);
    };
  }

  /* How should each item be processed. */
  let eachCb = writeCb;

  if (req.query.field.match(/(ip.src:port.src|a1:p1|srcIp:srtPort|ip.src:srcPort|ip.dst:port.dst|a2:p2|dstIp:dstPort|ip.dst:dstPort)/)) {
    eachCb = function(item) {
      let sep = (item.key.indexOf(':') === -1)? ':' : '.';
      item.field2.buckets.forEach((item2) => {
        item2.key = item.key + sep + item2.key;
        writeCb(item2);
      });
    };
  }

  buildSessionQuery(req, function(err, query, indices) {
    delete query.sort;
    delete query.aggregations;

    if (req.query.field.match(/(ip.src:port.src|a1:p1|srcIp:srcPort|ip.src:srcPort)/)) {
      query.aggregations = {field: { terms : {field : 'srcIp', size: aggSize}, aggregations: {field2: {terms: {field: 'srcPort', size: 100}}}}};
    } else if (req.query.field.match(/(ip.dst:port.dst|a2:p2|dstIp:dstPort|ip.dst:dstPort)/)) {
      query.aggregations = {field: { terms : {field : 'dstIp', size: aggSize}, aggregations: {field2: {terms: {field: 'dstPort', size: 100}}}}};
    } else if (req.query.field === 'fileand') {
      query.aggregations = { field: { terms : { field : 'node', size: aggSize }, aggregations: { field2: { terms: { field: 'fileId', size: 100 } } } } };
    } else {
      query.aggregations = {field: { terms : {field : req.query.field, size: aggSize}}};
    }

    query.size = 0;
    console.log('unique aggregations', indices, JSON.stringify(query));

    function findFileNames (result) {
      let intermediateResults = [];
      let aggs = result.aggregations.field.buckets;
      aggs.forEach((item) => {
        item.field2.buckets.forEach((sitem) => {
          intermediateResults.push({ key: item.key + ':' + sitem.key, doc_count: sitem.doc_count });
        });
      });

      async.each(intermediateResults, (fsitem, cb) => {
        let split = fsitem.key.split(':');
        let node = split[0];
        let fileId = split[1];
        Db.fileIdToFile(node, fileId, function (file) {
          if (file && file.name) {
            eachCb({key: file.name, doc_count: fsitem.doc_count });
          }
          cb();
        });
      }, function () {
        return res.end();
      });
    }

    Db.searchPrimary(indices, 'session', query, null, function (err, result) {
      if (err) {
        console.log('Error', query, err);
        return doneCb?doneCb():res.end();
      }
      if (Config.debug) {
        console.log('unique.txt result', util.inspect(result, false, 50));
      }
      if (!result.aggregations || !result.aggregations.field) {
        return doneCb ? doneCb() : res.end();
      }


      if (req.query.field === 'fileand') {
        return findFileNames(result);
      }

      for (let i = 0, ilen = result.aggregations.field.buckets.length; i < ilen; i++) {
        eachCb(result.aggregations.field.buckets[i]);
      }

      return doneCb ? doneCb() : res.end();
    });
  });
});

function processSessionIdDisk(session, headerCb, packetCb, endCb, limit) {
  let fields;

  function processFile(pcap, pos, i, nextCb) {
    pcap.ref();
    pcap.readPacket(pos, function(packet) {
      switch(packet) {
      case null:
        let msg = util.format(session._id, "in file", pcap.filename, "couldn't read packet at", pos, "packet #", i, "of", fields.packetPos.length);
        console.log("ERROR - processSessionIdDisk -", msg);
        endCb(msg, null);
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

  fields = session._source || session.fields;

  var fileNum;
  var itemPos = 0;
  async.eachLimit(fields.packetPos, limit || 1, function(pos, nextCb) {
    if (pos < 0) {
      fileNum = pos * -1;
      return nextCb(null);
    }

    // Get the pcap file for this node a filenum, if it isn't opened then do the filename lookup and open it
    var opcap = Pcap.get(fields.node + ":" + fileNum);
    if (!opcap.isOpen()) {
      Db.fileIdToFile(fields.node, fileNum, function(file) {
        if (!file) {
          console.log("WARNING - Only have SPI data, PCAP file no longer available.  Couldn't look up in file table", fields.node + '-' + fileNum);
          return nextCb("Only have SPI data, PCAP file no longer available for " + fields.node + '-' + fileNum);
        }
        if (file.kekId) {
          file.kek = Config.sectionGet("keks", file.kekId, undefined);
          if (file.kek === undefined) {
            console.log("ERROR - Couldn't find kek", file.kekId, "in keks section");
            return nextCb("Couldn't find kek " + file.kekId + " in keks section");
          }
        }

        var ipcap = Pcap.get(fields.node + ":" + file.num);

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
    options  = { _source: 'node,totPackets,packetLen,packetPos,srcIp,srcPort,ipProtocol' };
  }

  Db.getWithOptions(Db.sid2Index(id), 'session', Db.sid2Id(id), options, function(err, session) {
    if (err || !session.found) {
      console.log("session get error", err, session);
      return endCb("Session not found", null);
    }

    var fields = session._source || session.fields;

    if (maxPackets && fields.packetPos.length > maxPackets) {
      fields.packetPos.length = maxPackets;
    }

    /* Go through the list of prefetch the id to file name if we are running in parallel to
     * reduce the number of elasticsearch queries and problems
     */
    let outstanding = 0, i, ilen;

    function fileReadyCb (fileInfo) {
      outstanding--;
      if (i === ilen && outstanding === 0) {
        readyToProcess();
      }
    }

    for (i = 0, ilen = fields.packetPos.length; i < ilen; i++) {
      if (fields.packetPos[i] < 0) {
        outstanding++;
        Db.fileIdToFile(fields.node, -1 * fields.packetPos[i], fileReadyCb);
      }
    }

    function readyToProcess() {
      var pcapWriteMethod = Config.getFull(fields.node, "pcapWriteMethod");
      var psid = processSessionIdDisk;
      var writer = internals.writers[pcapWriteMethod];
      if (writer && writer.processSessionId) {
        psid = writer.processSessionId;
      }

      psid(session, headerCb, packetCb, function (err, fields) {
        if (!fields) {
          return endCb(err, fields);
        }

        if (!fields.tags) {
          fields.tags = [];
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
      console.log("ERROR - processSessionIdAndDecode", err);
      return doneCb(err);
    }
    packets = packets.filter(Boolean);
    if (packets.length === 0) {
      return doneCb(null, session, []);
    } else if (packets[0].ip === undefined) {
      return doneCb(null, session, []);
    } else if (packets[0].ip.p === 1) {
      Pcap.reassemble_icmp(packets, numPackets, function(err, results) {
        return doneCb(err, session, results);
      });
    } else if (packets[0].ip.p === 6) {
      var key = session.srcIp;
      Pcap.reassemble_tcp(packets, numPackets, key + ':' + session.srcPort, function(err, results) {
        return doneCb(err, session, results);
      });
    } else if (packets[0].ip.p === 17) {
      Pcap.reassemble_udp(packets, numPackets, function(err, results) {
        return doneCb(err, session, results);
      });
    } else if (packets[0].ip.p === 132) {
      Pcap.reassemble_sctp(packets, numPackets, function(err, results) {
        return doneCb(err, session, results);
      });
    } else {
      return doneCb(null, session, []);
    }
  },
  numPackets, 10);
}

function localSessionDetailReturnFull(req, res, session, incoming) {
  if (req.packetsOnly) { // only return packets
    res.render('sessionPackets.pug', {
      filename: 'sessionPackets',
      cache: isProduction(),
      compileDebug: !isProduction(),
      user: req.user,
      session: session,
      data: incoming,
      reqPackets: req.query.packets,
      query: req.query,
      basedir: "/",
      reqFields: Config.headers("headers-http-request"),
      resFields: Config.headers("headers-http-response"),
      emailFields: Config.headers("headers-email"),
      showFrames: req.query.showFrames
    }, function(err, data) {
      if (err) {
        console.trace("ERROR - localSession - ", err);
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
    options[key] = decodeOptions[key];
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

function sortFields(session) {
  if (session.tags) {
    session.tags = session.tags.sort();
  }
  if (session.http) {
    if (session.http.requestHeader) {
      session.http.requestHeader = session.http.requestHeader.sort();
    }
    if (session.http.responseHeader) {
      session.http.responseHeader = session.http.responseHeader.sort();
    }
  }
  if (session.email && session.email.headers) {
    session.email.headers = session.email.headers.sort();
  }
  if (session.ipProtocol) {
    session.ipProtocol = Pcap.protocol2Name(session.ipProtocol);
  }
}


function localSessionDetail(req, res) {
  if (!req.query) {
    req.query = { gzip: false, line: false, base: "natural", packets: 200 };
  }

  req.query.needgzip = req.query.gzip === "true" || false;
  req.query.needimage = req.query.image === "true" || false;
  req.query.line = req.query.line  || false;
  req.query.base = req.query.base  || "ascii";
  req.query.showFrames = req.query.showFrames === 'true' || false;

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
    if (err) {
      return res.end("Problem loading packets for " + safeStr(req.params.id) + " Error: " + err);
    }
    session.id = req.params.id;
    sortFields(session);

    if (req.query.showFrames && packets.length !== 0) {
      Pcap.packetFlow(session, packets, +req.query.packets || 200, function (err, results, sourceKey, destinationKey) {
        session._err = err;
        session.sourceKey = sourceKey;
        session.destinationKey = destinationKey;
        localSessionDetailReturn(req, res, session, results || []);
      });
    } else if (packets.length === 0) {
      session._err = "No pcap data found";
      localSessionDetailReturn(req, res, session, []);
    } else if (packets[0].ip === undefined) {
      session._err = "Couldn't decode pcap file, check viewer log";
      localSessionDetailReturn(req, res, session, []);
    } else if (packets[0].ip.p === 1) {
      Pcap.reassemble_icmp(packets, +req.query.packets || 200, function(err, results) {
        session._err = err;
        localSessionDetailReturn(req, res, session, results || []);
      });
    } else if (packets[0].ip.p === 6) {
      var key = session.srcIp;
      Pcap.reassemble_tcp(packets, +req.query.packets || 200, key + ':' + session.srcPort, function(err, results) {
        session._err = err;
        localSessionDetailReturn(req, res, session, results || []);
      });
    } else if (packets[0].ip.p === 17) {
      Pcap.reassemble_udp(packets, +req.query.packets || 200, function(err, results) {
        session._err = err;
        localSessionDetailReturn(req, res, session, results || []);
      });
    } else if (packets[0].ip.p === 132) {
      Pcap.reassemble_sctp(packets, +req.query.packets || 200, function(err, results) {
        session._err = err;
        localSessionDetailReturn(req, res, session, results || []);
      });
    } else if (packets[0].ip.p === 50) {
      Pcap.reassemble_esp(packets, +req.query.packets || 200, function(err, results) {
        session._err = err;
        localSessionDetailReturn(req, res, session, results || []);
      });
    } else if (packets[0].ip.p === 58) {
      Pcap.reassemble_icmp(packets, +req.query.packets || 200, function(err, results) {
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
app.get('/:nodeName/session/:id/detail', cspHeader, logAction(), (req, res) => {
  Db.getWithOptions(Db.sid2Index(req.params.id), 'session', Db.sid2Id(req.params.id), {}, function(err, session) {
    if (err || !session.found) {
      return res.end("Couldn't look up SPI data, error for session " + safeStr(req.params.id) + " Error: " +  err);
    }

    session = session._source;

    session.id = req.params.id;

    sortFields(session);

    let hidePackets = (session.fileId === undefined || session.fileId.length === 0)?"true":"false";
    fixFields(session, () => {
      pug.render(internals.sessionDetailNew, {
        filename    : "sessionDetail",
        cache       : isProduction(),
        compileDebug: !isProduction(),
        user        : req.user,
        session     : session,
        Db          : Db,
        query       : req.query,
        basedir     : "/",
        hidePackets : hidePackets,
        reqFields   : Config.headers("headers-http-request"),
        resFields   : Config.headers("headers-http-response"),
        emailFields : Config.headers("headers-email")
      }, function(err, data) {
        if (err) {
          console.trace("ERROR - fixFields - ", err);
          return req.next(err);
        }
        if (Config.debug > 1) {
          console.log("Detail Rendering", data.replace(/>/g, ">\n"));
        }
        res.send(data);
      });
    });
  });
});

/**
 * Get Session Packets
 */
app.get('/:nodeName/session/:id/packets', [logAction(), checkPermissions(['hidePcap'])], (req, res) => {
  isLocalView(req.params.nodeName, function () {
    noCache(req, res);
    req.packetsOnly = true;
    localSessionDetail(req, res);
  },
  function () {
    return proxyRequest(req, res);
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
      if (items === undefined || items.length === 0) {
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
    res.setHeader("Content-Type", "application/force-download");
    res.setHeader("Content-Disposition", "attachment; filename="+req.params.bodyName);
    return res.end(data);
  });
});

app.get('/:nodeName/:id/bodypng/:bodyType/:bodyNum/:bodyName', checkProxyRequest, function(req, res) {
  reqGetRawBody(req, function (err, data) {
    if (err || data === null || data.length === 0) {
      return res.send (internals.emptyPNG);
    }
    res.setHeader("Content-Type", "image/png");

    var png = new PNG({width: internals.PNG_LINE_WIDTH, height: Math.ceil(data.length/internals.PNG_LINE_WIDTH)});
    png.data = data;
    res.send(PNG.sync.write(png, {inputColorType:0, colorType: 0, bitDepth:8, inputHasAlpha:false}));
  });
});

/**
 * Get a file given a hash of that file
 */

app.get('/bodyHash/:hash', logAction('bodyhash'), function(req, res) {
  var hash = null;
  var nodeName = null;
  var sessionID = null;

  buildSessionQuery(req, function(bsqErr, query, indices) {
    if (bsqErr) {
      res.status(400);
      return res.end(bsqErr);
    }

    query.size = 1;
    query.sort = { lastPacket: { order: 'desc' } };
    query._source = ["node"];

    if (Config.debug) {
      console.log(`sessions.json ${indices} query`, JSON.stringify(query, null, 1));
    }
    Db.searchPrimary(indices, 'session', query, null, function (err, sessions) {
      if (err ) {
        console.log ("Error -> Db Search ", err);
        res.status(400);
        res.end(err);
      } else if (sessions.error) {
        console.log ("Error -> Db Search ", sessions.error);
        res.status(400);
        res.end(sessions.error);
      } else {
          if (Config.debug) {
            console.log("bodyHash result", util.inspect(sessions, false, 50));
          }
          if (sessions.hits.hits.length > 0) {

            nodeName = sessions.hits.hits[0]._source.node;
            sessionID = Db.session2Sid(sessions.hits.hits[0]);
            hash = req.params.hash;

            isLocalView(nodeName, function () { // get file from the local disk
              localGetItemByHash (nodeName, sessionID, hash, (err, item) => {
                if (err) {
                  res.status(400);
                  return res.end(err);
                } else if (item) {
                  noCache(req, res);
                  res.setHeader("content-type", "application/force-download");
                  res.setHeader("content-disposition", "attachment; filename="+ item.bodyName+".pellet");
                  return res.end(item.data);
                } else {
                  res.status(400);
                  return res.end("No Match");
                }
              });
            },
            function () { // get file from the remote disk
              var preq = util._extend({},req);
              preq.params.nodeName = nodeName;
              preq.params.id = sessionID;
              preq.params.hash = hash;
              preq.url = Config.basePath(nodeName) + nodeName + '/' + sessionID + '/bodyHash/' + hash;
              return proxyRequest(preq, res);
            });
          }
          else {
            res.status(400);
            res.end ("No Match Found");
          }
      }
    });
  });
});

app.get('/:nodeName/:id/bodyHash/:hash', checkProxyRequest, function(req, res) {
  localGetItemByHash (req.params.nodeName, req.params.id, req.params.hash, (err, item) => {
    if (err) {
       res.status(400);
       return res.end(err);
    } else if (item) {
      noCache(req, res);
      res.setHeader("content-type", "application/force-download");
      res.setHeader("content-disposition", "attachment; filename="+ item.bodyName+".pellet");
      return res.end(item.data);
    } else {
      res.status(400);
      return res.end("No Match");
    }
  });
});

function localGetItemByHash(nodeName, sessionID, hash, cb) {
  processSessionIdAndDecode(sessionID, 10000, function(err, session, incoming) {
    if (err) {
      return cb(err);
    }
    if (incoming.length === 0) {
      return cb(null, null);
    }
    var options = {
      id: sessionID,
      nodeName: nodeName,
      order: [],
      "ITEM-HTTP": {
        order: []
      },
      "ITEM-SMTP": {
        order: ["BODY-UNBASE64"]
      },
      "ITEM-HASH": {
        hash: hash
      },
      "ITEM-CB": {
      }
    };

    options.order.push("ITEM-HTTP");
    options.order.push("ITEM-SMTP");
    options.order.push("ITEM-HASH");
    options.order.push("ITEM-CB");
    options["ITEM-CB"].cb = function(err, items) {
      if (err) {
        return cb(err, null);
      }
      if (items === undefined || items.length === 0) {
        return cb("No match", null);
      }
      return cb(err, items[0]);
    };
    decode.createPipeline(options, options.order, new decode.Pcap2ItemStream(options, incoming));
  });
}

function writePcap(res, id, options, doneCb) {
  var b = Buffer.alloc(0xfffe);
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
        b = Buffer.alloc(0xfffe);
      }
      buffer.copy(b, boffset, 0, buffer.length);
      boffset += buffer.length;
    }
    cb(null);
  },
  function(err, session) {
    if (err) {
      console.trace("writePcap", err);
      return doneCb(err);
    }
    res.write(b.slice(0, boffset));
    doneCb(err);
  }, undefined, 10);
}

function writePcapNg(res, id, options, doneCb) {
  var b = Buffer.alloc(0xfffe);
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
      b = Buffer.alloc(0xfffe);
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
    delete session.packetPos;
    var json = JSON.stringify(session);

    var len = ((json.length + 20 + 3) >> 2) << 2;
    b = Buffer.alloc(len);

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

app.get('/:nodeName/pcapng/:id.pcapng', [checkProxyRequest, checkPermissions(['disablePcapDownload'])], (req, res) => {
  noCache(req, res, "application/vnd.tcpdump.pcap");
  writePcapNg(res, req.params.id, {writeHeader: !req.query || !req.query.noHeader || req.query.noHeader !== "true"}, function () {
    res.end();
  });
});

app.get('/:nodeName/pcap/:id.pcap', [checkProxyRequest, checkPermissions(['disablePcapDownload'])], (req, res) => {
  noCache(req, res, "application/vnd.tcpdump.pcap");

  writePcap(res, req.params.id, {writeHeader: !req.query || !req.query.noHeader || req.query.noHeader !== "true"}, function () {
    res.end();
  });
});

app.get('/:nodeName/raw/:id.png', checkProxyRequest, function(req, res) {
  noCache(req, res, "image/png");

  processSessionIdAndDecode(req.params.id, 1000, function(err, session, results) {
    if (err) {
      return res.send (internals.emptyPNG);
    }
    var size = 0;
    var i, ilen;
    for (i = (req.query.type !== 'dst'?0:1), ilen = results.length; i < ilen; i+=2) {
      size += results[i].data.length + 2*internals.PNG_LINE_WIDTH - (results[i].data.length % internals.PNG_LINE_WIDTH);
    }
    var buffer = Buffer.alloc(size, 0);
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

    var png = new PNG({width: internals.PNG_LINE_WIDTH, height: (size/internals.PNG_LINE_WIDTH)-1});
    png.data = buffer;
    res.send(PNG.sync.write(png, {inputColorType:0, colorType: 0, bitDepth:8, inputHasAlpha:false}));
  });
});

app.get('/:nodeName/raw/:id', checkProxyRequest, function(req, res) {
  noCache(req, res, "application/vnd.tcpdump.pcap");

  processSessionIdAndDecode(req.params.id, 10000, function(err, session, results) {
    if (err) {
      return res.send("Error");
    }
    for (let i = (req.query.type !== 'dst'?0:1), ilen = results.length; i < ilen; i+=2) {
      res.write(results[i].data);
    }
    res.end();
  });
});

app.get('/:nodeName/entirePcap/:id.pcap', [checkProxyRequest, checkPermissions(['disablePcapDownload'])], (req, res) => {
  noCache(req, res, "application/vnd.tcpdump.pcap");

  var options = {writeHeader: true};

  var query = { _source: ["rootId"],
                size: 1000,
                query: {term: {rootId: req.params.id}},
                sort: { lastPacket: { order: 'asc' } }
              };

  console.log("entirePcap query", JSON.stringify(query));

  Db.searchPrimary('sessions2-*', 'session', query, null, function (err, data) {
    async.forEachSeries(data.hits.hits, function(item, nextCb) {
      writePcap(res, Db.session2Sid(item), options, nextCb);
    }, function (err) {
      res.end();
    });
  });
});

function sessionsPcapList(req, res, list, pcapWriter, extension) {

  if (list.length > 0 && list[0].fields) {
    list = list.sort(function(a,b){return a.fields.lastPacket - b.fields.lastPacket;});
  } else if (list.length > 0 && list[0]._source) {
    list = list.sort(function(a,b){return a._source.lastPacket - b._source.lastPacket;});
  }

  var options = {writeHeader: true};

  async.eachLimit(list, 10, function(item, nextCb) {
    var fields = item._source || item.fields;
    isLocalView(fields.node, function () {
      // Get from our DISK
      pcapWriter(res, Db.session2Sid(item), options, nextCb);
    },
    function () {
      // Get from remote DISK
      getViewUrl(fields.node, function(err, viewUrl, client) {
        var buffer = Buffer.alloc(fields.pa*20 + fields.by);
        var bufpos = 0;
        var info = url.parse(viewUrl);
        info.path = Config.basePath(fields.node) + fields.node + "/" + extension + "/" + Db.session2Sid(item) + "." + extension;
        info.agent = (client === http?internals.httpAgent:internals.httpsAgent);

        addAuth(info, req.user, fields.node);
        addCaTrust(info, fields.node);
        var preq = client.request(info, function(pres) {
          pres.on('data', function (chunk) {
            if (bufpos + chunk.length > buffer.length) {
              var tmp = Buffer.alloc(buffer.length + chunk.length*10);
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

    sessionsListFromIds(req, ids, ["lastPacket", "node", "totBytes", "totPackets", "rootId"], function(err, list) {
      sessionsPcapList(req, res, list, pcapWriter, extension);
    });
  } else {
    sessionsListFromQuery(req, res, ["lastPacket", "node", "totBytes", "totPackets", "rootId"], function(err, list) {
      sessionsPcapList(req, res, list, pcapWriter, extension);
    });
  }
}

app.get(/\/sessions.pcapng.*/, [logAction(), checkPermissions(['disablePcapDownload'])], (req, res) => {
  return sessionsPcap(req, res, writePcapNg, "pcapng");
});

app.get(/\/sessions.pcap.*/, [logAction(), checkPermissions(['disablePcapDownload'])], (req, res) => {
  return sessionsPcap(req, res, writePcap, "pcap");
});

internals.usersMissing = {
  userId: '',
  userName: '',
  expression: '',
  enabled: 0,
  createEnabled: 0,
  webEnabled: 0,
  headerAuthEnabled: 0,
  emailSearch: 0,
  removeEnabled: 0,
  lastUsed: 0
};

app.post('/user/list', [noCacheJson, logAction('users'), recordResponseTime, checkPermissions(['createEnabled'])], (req, res) => {
  let columns = [ 'userId', 'userName', 'expression', 'enabled', 'createEnabled',
    'webEnabled', 'headerAuthEnabled', 'emailSearch', 'removeEnabled', 'packetSearch',
    'hideStats', 'hideFiles', 'hidePcap', 'disablePcapDownload', 'welcomeMsgNum',
    'lastUsed', 'timeLimit' ];

  let query = {
    _source: columns,
    sort: {},
    from: +req.body.start || 0,
    size: +req.body.length || 10000,
    query: { // exclude the shared user from results
      bool: { must_not: { term: { userId: '_moloch_shared' } } }
    }
  };

  if (req.body.filter) {
    query.query.bool.should = [
      { wildcard: { userName: '*' + req.body.filter + '*' } },
      { wildcard: { userId: '*' + req.body.filter + '*' } }
    ];
  }

  req.body.sortField = req.body.sortField || 'userId';
  query.sort[req.body.sortField] = { order: req.body.desc === true ? 'desc': 'asc' };
  query.sort[req.body.sortField].missing = internals.usersMissing[req.body.sortField];

  Promise.all([Db.searchUsers(query),
               Db.numberOfUsers()
              ])
  .then(([users, total]) => {
    if (users.error) { throw users.error; }
    let results = { total: users.hits.total, results: [] };
    for (let i = 0, ilen = users.hits.hits.length; i < ilen; i++) {
      let fields = users.hits.hits[i]._source || users.hits.hits[i].fields;
      fields.id = users.hits.hits[i]._id;
      fields.expression = fields.expression || '';
      fields.headerAuthEnabled = fields.headerAuthEnabled || false;
      fields.emailSearch = fields.emailSearch || false;
      fields.removeEnabled = fields.removeEnabled || false;
      fields.userName = safeStr(fields.userName || '');
      fields.packetSearch = fields.packetSearch || false;
      fields.timeLimit = fields.timeLimit || undefined;
      results.results.push(fields);
    }

    let r = {
      recordsTotal: total.count,
      recordsFiltered: results.total,
      data: results.results
    };

    res.send(r);
  }).catch((err) => {
    console.log('ERROR - /user/list', err);
    return res.send({recordsTotal: 0, recordsFiltered: 0, data: []});
  });
});

app.post('/user/create', [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])], (req, res) => {
  if (!req.body || !req.body.userId || !req.body.userName || !req.body.password) {
    return res.molochError(403, 'Missing/Empty required fields');
  }

  if (req.body.userId.match(/[^@\w.-]/)) {
    return res.molochError(403, 'User ID must be word characters');
  }

  if (req.body.userId === '_moloch_shared') {
    return res.molochError(403, 'User ID cannot be the same as the shared moloch user');
  }

  Db.getUser(req.body.userId, function(err, user) {
    if (!user || user.found) {
      console.log('Trying to add duplicate user', err, user);
      return res.molochError(403, 'User already exists');
    }

    let nuser = {
      userId: req.body.userId,
      userName: req.body.userName,
      expression: req.body.expression,
      passStore: Config.pass2store(req.body.userId, req.body.password),
      enabled: req.body.enabled === true,
      webEnabled: req.body.webEnabled === true,
      emailSearch: req.body.emailSearch === true,
      headerAuthEnabled: req.body.headerAuthEnabled === true,
      createEnabled: req.body.createEnabled === true,
      removeEnabled: req.body.removeEnabled === true,
      packetSearch: req.body.packetSearch === true,
      timeLimit: req.body.timeLimit,
      hideStats: req.body.hideStats === true,
      hideFiles: req.body.hideFiles === true,
      hidePcap: req.body.hidePcap === true,
      disablePcapDownload: req.body.disablePcapDownload === true,
      welcomeMsgNum: 0
    };

    // console.log('Creating new user', nuser);
    Db.setUser(req.body.userId, nuser, function(err, info) {
      if (!err) {
        return res.send(JSON.stringify({success: true, text:'User created succesfully'}));
      } else {
        console.log('ERROR - add user', err, info);
        return res.molochError(403, err);
      }
    });
  });
});

app.put('/user/:userId/acknowledgeMsg', [noCacheJson, logAction(), checkCookieToken], function (req, res) {
  if (!req.body.msgNum) {
    return res.molochError(403, 'Message number required');
  }

  Db.getUser(req.params.userId, function (err, user) {
    if (err || !user.found) {
      console.log('update user failed', err, user);
      return res.molochError(403, 'User not found');
    }
    user = user._source;

    user.welcomeMsgNum = parseInt(req.body.msgNum);

    Db.setUser(req.params.userId, user, function (err, info) {
      if (Config.debug) {
        console.log('setUser', user, err, info);
      }
      return res.send(JSON.stringify({
        success: true,
        text: `User, ${req.params.userId}, dismissed message ${req.body.msgNum}`
      }));
    });
  });
});

app.post('/user/delete', [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])], (req, res) => {
  if (req.body.userId === req.user.userId) {
    return res.molochError(403, 'Can not delete yourself');
  }

  Db.deleteUser(req.body.userId, function(err, data) {
    setTimeout(function () {
      res.send(JSON.stringify({success: true, text: 'User deleted successfully'}));
    }, 200);
  });
});

app.post('/user/update', [noCacheJson, logAction(), checkCookieToken, postSettingUser, checkPermissions(['createEnabled'])], (req, res) => {
  if (req.body.userId === undefined) {
    return res.molochError(403, 'Missing userId');
  }

  if (req.body.userId === "_moloch_shared") {
    return res.molochError(403, '_moloch_shared is a shared user. This users settings cannot be updated');
  }

  /*if (req.params.userId === req.user.userId && req.query.createEnabled !== undefined && req.query.createEnabled !== "true") {
    return res.send(JSON.stringify({success: false, text: "Can not turn off your own admin privileges"}));
  }*/

  Db.getUser(req.body.userId, function(err, user) {
    if (err || !user.found) {
      console.log('update user failed', err, user);
      return res.molochError(403, 'User not found');
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
        return res.molochError(403, 'Username can not be empty');
      } else {
        user.userName = req.body.userName;
      }
    }

    user.webEnabled = req.body.webEnabled === true;
    user.emailSearch = req.body.emailSearch === true;
    user.headerAuthEnabled = req.body.headerAuthEnabled === true;
    user.removeEnabled = req.body.removeEnabled === true;
    user.packetSearch = req.body.packetSearch === true;
    user.hideStats = req.body.hideStats === true;
    user.hideFiles = req.body.hideFiles === true;
    user.hidePcap = req.body.hidePcap === true;
    user.disablePcapDownload = req.body.disablePcapDownload === true;
    user.timeLimit = req.body.timeLimit ? parseInt(req.body.timeLimit) : undefined;

    // Can only change createEnabled if it is currently turned on
    if (req.body.createEnabled !== undefined && req.user.createEnabled) {
      user.createEnabled = req.body.createEnabled === true;
    }

    Db.setUser(req.body.userId, user, function(err, info) {
      if (Config.debug) {
        console.log("setUser", user, err, info);
      }
      return res.send(JSON.stringify({success: true, text:'User "' + req.body.userId + '" updated successfully'}));
    });
  });
});

app.post('/state/:name', [noCacheJson, logAction()], function(req, res) {
  Db.getUser(req.user.userId, function(err, user) {
    if (err || !user.found) {
      console.log("save state failed", err, user);
      return res.molochError(403, "Unknown user");
    }
    user = user._source;

    if (!user.tableStates) {
      user.tableStates = {};
    }
    user.tableStates[req.params.name] = req.body;
    Db.setUser(user.userId, user, function(err, info) {
      if (err) {
        console.log("state error", err, info);
        return res.molochError(403, "state update failed");
      }
      return res.send(JSON.stringify({success: true, text: "updated state successfully"}));
    });
  });
});

app.get('/state/:name', [noCacheJson], function(req, res) {
  if (!req.user.tableStates || !req.user.tableStates[req.params.name]) {
    return res.send("{}");
  }

  // Fix for new names
  if (req.params.name === "sessionsNew" && req.user.tableStates && req.user.tableStates.sessionsNew) {
    let item = req.user.tableStates.sessionsNew;
    if (item.visibleHeaders) {
      item.visibleHeaders = item.visibleHeaders.map(oldDB2newDB);
    }
    if (item.order && item.order.length > 0) {
      item.order[0][0] = oldDB2newDB(item.order[0][0]);
    }
  }

  return res.send(req.user.tableStates[req.params.name]);
});

//////////////////////////////////////////////////////////////////////////////////
//// Session Add/Remove Tags
//////////////////////////////////////////////////////////////////////////////////
function addTagsList (allTagNames, sessionList, doneCb) {
  if (!sessionList.length) {
    console.log('No sessions to add tags to');
    return doneCb(null);
  }

  async.eachLimit(sessionList, 10, function (session, nextCb) {
    if (!session._source && !session.fields) {
      console.log('No Fields', session);
      return nextCb(null);
    }

    let node = (Config.get('multiES', false) && session._node) ? session._node : undefined;

    Db.addTagsToSession(session._index, session._id, allTagNames, node, function (err, data) {
      if (err) { console.log('addTagsList error', session, err, data); }
      nextCb(null);
    });
  }, doneCb);
}

function removeTagsList(res, allTagNames, sessionList) {
  if (!sessionList.length) {
    return res.molochError(200, 'No sessions to remove tags from');
  }

  async.eachLimit(sessionList, 10, function(session, nextCb) {
    if (!session._source && !session.fields) {
      console.log('No Fields', session);
      return nextCb(null);
    }

    let node = (Config.get('multiES', false) && session._node) ? session._node : undefined;

    Db.removeTagsFromSession(session._index, session._id, allTagNames, node, function (err, data) {
      if (err) { console.log('removeTagsList error', session, err, data); }
      nextCb(null);
    });
  }, function (err) {
    return res.send(JSON.stringify({success: true, text: 'Tags removed successfully'}));
  });
}

app.post('/addTags', [noCacheJson, logAction()], function(req, res) {
  var tags = [];
  if (req.body.tags) {
    tags = req.body.tags.replace(/[^-a-zA-Z0-9_:,]/g, "").split(",");
  }

  if (tags.length === 0) { return res.molochError(200, "No tags specified"); }

  if (req.body.ids) {
    var ids = queryValueToArray(req.body.ids);

    sessionsListFromIds(req, ids, ["tags", "node"], function(err, list) {
      if (!list.length) {
        return res.molochError(200, 'No sessions to add tags to');
      }
      addTagsList(tags, list, function () {
        return res.send(JSON.stringify({success: true, text: "Tags added successfully"}));
      });
    });
  } else {
    sessionsListFromQuery(req, res, ["tags", "node"], function(err, list) {
      if (!list.length) {
        return res.molochError(200, 'No sessions to add tags to');
      }
      addTagsList(tags, list, function () {
        return res.send(JSON.stringify({success: true, text: "Tags added successfully"}));
      });
    });
  }
});

app.post('/removeTags', [noCacheJson, logAction(), checkPermissions(['removeEnabled'])], (req, res) => {
  var tags = [];
  if (req.body.tags) {
    tags = req.body.tags.replace(/[^-a-zA-Z0-9_:,]/g, "").split(",");
  }

  if (tags.length === 0) { return res.molochError(200, "No tags specified"); }

  if (req.body.ids) {
    var ids = queryValueToArray(req.body.ids);

    sessionsListFromIds(req, ids, ["tags"], function(err, list) {
      removeTagsList(res, tags, list);
    });
  } else {
    sessionsListFromQuery(req, res, ["tags"], function(err, list) {
      removeTagsList(res, tags, list);
    });
  }
});

//////////////////////////////////////////////////////////////////////////////////
//// Packet Search
//////////////////////////////////////////////////////////////////////////////////
function packetSearch (packet, options) {
  let found = false;

  switch (options.searchType) {
    case 'asciicase':
      if (packet.toString().includes(options.search)) {
        found = true;
      }
      break;
    case 'ascii':
      if (packet.toString().toLowerCase().includes(options.search.toLowerCase())) {
        found = true;
      }
      break;
    case 'regex':
      if (options.regex && packet.toString().match(options.regex)) {
        found = true;
      }
      break;
    case 'hex':
      if (packet.toString('hex').includes(options.search)) {
        found = true;
      }
      break;
    case 'hexregex':
      if (options.regex && packet.toString('hex').match(options.regex)) {
        found = true;
      }
      break;
    default:
      console.log('Invalid hunt search type');
  }

  return found;
}

function sessionHunt (sessionId, options, cb) {
  if (options.type === 'reassembled') {
    processSessionIdAndDecode(sessionId, options.size || 10000, function (err, session, packets) {
      if (err) {
        return cb(null, false);
      }

      let i = 0;
      let increment = 1;
      let len = packets.length;

      if (options.src && !options.dst) {
        increment = 2;
      } else if (options.dst && !options.src) {
        i = 1;
        increment = 2;
      }

      for (i; i < len; i+=increment) {
        if (packetSearch(packets[i].data, options)) { return cb(null, true); }
      }

      return cb(null, false);
    });
  } else if (options.type === 'raw') {
    let packets = [];
    processSessionId(sessionId, true, null, function (pcap, buffer, cb, i) {
      if (options.src === options.dst) {
        packets.push(buffer);
      } else {
          let packet = {};
          pcap.decode(buffer, packet);
          packet.data = buffer.slice(16);
          packets.push(packet);
      }
      cb(null);
    }, function(err, session) {
      if (err) {
        return cb(null, false);
      }

      let len = packets.length;
      if (options.src === options.dst) {
        // If search both src/dst don't need to check key
        for (let i = 0; i < len; i++) {
          if (packetSearch(packets[i], options)) { return cb(null, true); }
        }
      } else {
        // If searching src NOR dst need to check key
        let skey = Pcap.keyFromSession(session);
        for (let i = 0; i < len; i++) {
          let key = Pcap.key(packets[i]);
          let isSrc = key === skey;
          if (options.src && isSrc) {
            if (packetSearch(packets[i].data, options)) { return cb(null, true); }
          } else if (options.dst && !isSrc) {
            if (packetSearch(packets[i].data, options)) { return cb(null, true); }
          }
        }
      }
      return cb(null, false);
    },
    options.size || 10000, 10);
  }
}

function pauseHuntJobWithError (huntId, hunt, error, node) {
  let errorMsg = `${hunt.name} (${huntId}) hunt ERROR: ${error.value}.`;
  if (node) {
    errorMsg += ` On ${node} node`;
    error.node = node;
  }

  console.log(errorMsg);

  error.time = Math.floor(Date.now() / 1000);

  hunt.status = 'paused';

  if (!hunt.errors) {
    hunt.errors = [ error ];
  } else {
    hunt.errors.push(error);
  }

  function continueProcess () {
    Db.setHunt(huntId, hunt, (err, info) => {
      internals.runningHuntJob = undefined;
      if (err) {
        console.log('Error adding errors and pausing hunt job', err, info);
        return;
      }
      processHuntJobs();
    });
  }

  let message = `*${hunt.name}* hunt job paused with error: *${error.value}*\n*${hunt.matchedSessions}* matched sessions out of *${hunt.searchedSessions}* searched sessions`;
  issueAlert(hunt.notifier, message, continueProcess);
}

function updateHuntStats (hunt, huntId, session, searchedSessions, cb) {
  // update the hunt with number of matchedSessions and searchedSessions
  // and the date of the first packet of the last searched session
  let lastPacketTime = session.lastPacket;
  let now = Math.floor(Date.now() / 1000);

  if ((now - hunt.lastUpdated) >= 2) { // only update every 2 seconds
    Db.get('hunts', 'hunt', huntId, (err, huntHit) => {
      if (!huntHit || !huntHit.found) { // hunt hit not found, likely deleted
        return cb('undefined');
      }

      if (err) {
        let errorText = `Error finding hunt: ${hunt.name} (${huntId}): ${err}`;
        pauseHuntJobWithError(huntId, hunt, { value: errorText });
        return cb({ success: false, text: errorText });
      }

      hunt.status = huntHit._source.status;
      hunt.lastUpdated = now;
      hunt.searchedSessions = searchedSessions;
      hunt.lastPacketTime = lastPacketTime;

      Db.setHunt(huntId, hunt, () => {});

      if (hunt.status === 'paused') {
        return cb('paused');
      } else {
        return cb(null);
      }
    });
  } else {
    return cb(null);
  }
}

function updateSessionWithHunt (session, sessionId, hunt, huntId) {
  Db.addHuntToSession(Db.sid2Index(sessionId), Db.sid2Id(sessionId), huntId, hunt.name, (err, data) => {
    if (err) { console.log('add hunt info error', session, err, data); }
  });
}

function buildHuntOptions (hunt) {
  let options = {
    src: hunt.src,
    dst: hunt.dst,
    size: hunt.size,
    type: hunt.type,
    search: hunt.search,
    searchType: hunt.searchType
  };

  if (hunt.searchType === 'regex' || hunt.searchType === 'hexregex') {
    try {
      options.regex = new RegExp(hunt.search);
    } catch (e) {
      pauseHuntJobWithError(hunt.huntId, hunt, { value: `Hunt error with regex: ${e}` });
    }
  }

  return options;
}

// Actually do the search against ES and process the results.
function runHuntJob (huntId, hunt, query, user) {
  let options = buildHuntOptions(hunt);
  let searchedSessions;

  Db.search('sessions2-*', 'session', query, {scroll: '600s'}, function getMoreUntilDone (err, result) {
    if (err || result.error) {
      pauseHuntJobWithError(huntId, hunt, { value: `Hunt error searching sessions: ${err}` });
      return;
    }

    let hits = result.hits.hits;

    if (searchedSessions === undefined) {
      searchedSessions = hunt.searchedSessions || 0;
      // if the session query results length is not equal to the total sessions that the hunt
      // job is searching, update the hunt total sessions so that the percent works correctly
      if (hunt.totalSessions !== (result.hits.total + searchedSessions)) {
        hunt.totalSessions = result.hits.total + searchedSessions;
      }
    }

    async.forEachLimit(hits, 3, function (hit, cb) {
      searchedSessions++;
      let session = hit._source;
      let sessionId = Db.session2Sid(hit);
      let node = session.node;

      isLocalView(node, function () {
        sessionHunt(sessionId, options, function (err, matched) {
          if (err) {
            return pauseHuntJobWithError(huntId, hunt, { value: `Hunt error searching session (${sessionId}): ${err}` }, node);
          }

          if (matched) {
            hunt.matchedSessions++;
            updateSessionWithHunt(session, sessionId, hunt, huntId);
          }

          updateHuntStats(hunt, huntId, session, searchedSessions, cb);
        });
      },
      function () { // Check Remotely
        let path = `${node}/hunt/${huntId}/remote/${sessionId}`;

        makeRequest (node, path, user, (err, response) => {
          if (err) {
            return pauseHuntJobWithError(huntId, hunt, { value: `Error hunting on remote viewer: ${err}` }, node);
          }
          let json = JSON.parse(response);
          if (json.error) {
            console.log(`Error hunting on remote viewer: ${json.error} - ${path}`);
            return pauseHuntJobWithError(huntId, hunt, { value: `Error hunting on remote viewer: ${json.error}` }, node);
          }
          if (json.matched) { hunt.matchedSessions++; }
          return updateHuntStats(hunt, huntId, session, searchedSessions, cb);
        });
      });
    }, function (err) { // done running this section of hunt job

      // Some kind of error, stop now
      if (err === 'paused' || err === 'undefined') {
        internals.runningHuntJob = undefined;
        return;
      }

      // There might be more, issue another scroll
      if (result.hits.hits.length !== 0) {
        return Db.scroll({ body: { scroll_id: result._scroll_id }, scroll: '600s' }, getMoreUntilDone);
      }

      Db.clearScroll({ body: { scroll_id: result._scroll_id } });

      // We are totally done with this hunt
      hunt.status = 'finished';
      hunt.searchedSessions = hunt.totalSessions;

      function continueProcess () {
        Db.setHunt(huntId, hunt, (err, info) => {
          internals.runningHuntJob = undefined;
          processHuntJobs(); // Start new hunt
        });
      }

      if (hunt.notifier) {
        let message = `*${hunt.name}* hunt job finished:\n*${hunt.matchedSessions}* matched sessions out of *${hunt.searchedSessions}* searched sessions`;
        issueAlert(hunt.notifier, message, continueProcess);
      } else {
        return continueProcess();
      }
    });
  });
}

// Do the house keeping before actually running the hunt job
function processHuntJob (huntId, hunt) {
  let now = Math.floor(Date.now() / 1000);

  hunt.lastUpdated = now;
  if (!hunt.started) { hunt.started = now; }

  Db.setHunt(huntId, hunt, (err, info) => {
    if (err) {
      pauseHuntJobWithError(huntId, hunt, { value: `Error starting hunt job: ${err} ${info}` });
      return;
    }
  });

  // find the user that created the hunt
  Db.getUserCache(hunt.userId, function(err, user) {
    if (err && !user) {
      pauseHuntJobWithError(huntId, hunt, { value: err });
      return;
    }
    if (!user || !user.found) {
      pauseHuntJobWithError(huntId, hunt, { value: `User ${hunt.userId} doesn't exist` });
      return;
    }
    if (!user._source.enabled) {
      pauseHuntJobWithError(huntId, hunt, { value: `User ${hunt.userId} is not enabled` });
      return;
    }

    user = user._source;

    Db.getLookupsCache(hunt.userId, (err, lookups) => {
      molochparser.parser.yy = {
        emailSearch: user.emailSearch === true,
        fieldsMap: Config.getFieldsMap(),
        prefix: internals.prefix,
        lookups: lookups || {},
        lookupTypeMap: internals.lookupTypeMap
      };

      // build session query
      let query = {
        from: 0,
        size: 100, // Only fetch 100 items at a time
        query: { bool: { must: [{ exists: { field: 'fileId' } }], filter: [{}] } },
        _source: ['_id', 'node'],
        sort: { lastPacket: { order: 'asc' } }
      };

      // get the size of the query if it is being restarted
      if (hunt.lastPacketTime) {
        query.size = hunt.totalSessions - hunt.searchedSessions;
      }

      if (hunt.query.expression) {
        try {
          query.query.bool.filter.push(molochparser.parse(hunt.query.expression));
        } catch (e) {
          pauseHuntJobWithError(huntId, hunt, { value: `Couldn't compile hunt query expression: ${e}` });
          return;
        }
      }

      if (user.expression && user.expression.length > 0) {
        try {
          // Expression was set by admin, so assume email search ok
          molochparser.parser.yy.emailSearch = true;
          var userExpression = molochparser.parse(user.expression);
          query.query.bool.filter.push(userExpression);
        } catch (e) {
          pauseHuntJobWithError(huntId, hunt, { value: `Couldn't compile user forced expression (${user.expression}): ${e}` });
          return;
        }
      }

      lookupQueryItems(query.query.bool.filter, function (lerr) {
        query.query.bool.filter[0] = {
          range: {
            lastPacket: {
              gte: hunt.lastPacketTime || hunt.query.startTime * 1000,
              lt: hunt.query.stopTime * 1000
            }
          }
        };

        query._source = ['lastPacket', 'node', 'huntId', 'huntName'];

        if (Config.debug > 2) {
          console.log('HUNT', hunt.name, hunt.userId, '- start:', new Date(hunt.lastPacketTime || hunt.query.startTime * 1000), 'stop:', new Date(hunt.query.stopTime * 1000));
        }

        // do sessions query
        runHuntJob(huntId, hunt, query, user);
      });
    });
  });
}

// Kick off the process of running a hunt job
// cb is optional and is called either when a job has been started or end of function
function processHuntJobs (cb) {
  if (Config.debug) {
    console.log('HUNT - processing hunt jobs');
  }

  if (internals.runningHuntJob) { return (cb ? cb() : null); }
  internals.runningHuntJob = true;

  let query = {
    size: 10000,
    sort: { created: { order: 'asc' } },
    query: { terms: { status: ['queued', 'paused', 'running'] } }
  };

  Db.searchHunt(query)
    .then((hunts) => {
      if (hunts.error) { throw hunts.error; }

      for (let i = 0, ilen = hunts.hits.hits.length; i < ilen; i++) {
        var hit = hunts.hits.hits[i];
        var hunt = hit._source;
        let id = hit._id;

        if (hunt.status === 'running') { // there is a job already running
          internals.runningHuntJob = hunt;
          if (!internals.proccessHuntJobsInitialized) {
            internals.proccessHuntJobsInitialized = true;
            // restart the abandoned hunt
            processHuntJob(id, hunt);
          }
          return (cb ? cb() : null);
        } else if (hunt.status === 'queued') { // get the first queued hunt
          internals.runningHuntJob = hunt;
          hunt.status = 'running'; // update the hunt job
          processHuntJob(id, hunt);
          return (cb ? cb() : null);
        }
      }

      // Made to the end without starting a job
      internals.proccessHuntJobsInitialized = true;
      internals.runningHuntJob = undefined;
      return (cb?cb():null);
    }).catch(err => {
      console.log('Error fetching hunt jobs', err);
      return (cb?cb():null);
    });
}

function updateHuntStatus (req, res, status, successText, errorText) {
  Db.get('hunts', 'hunt', req.params.id, (err, hit) => {
    if (err) {
      console.log(errorText, err, hit);
      return res.molochError(500, errorText);
    }

    // don't let a user play a hunt job if one is already running
    if (status === 'running' && internals.runningHuntJob) {
      return res.molochError(403, 'You cannot start a new hunt until the running job completes or is paused.');
    }

    let hunt = hit._source;

    // if hunt is finished, don't allow pause
    if (hunt.status === 'finished' && status === 'paused') {
      return res.molochError(403, 'You cannot pause a completed hunt.');
    }

    // clear the running hunt job if this is it
    if (hunt.status === 'running') { internals.runningHuntJob = undefined; }
    hunt.status = status; // update the hunt job

    Db.setHunt(req.params.id, hunt, (err, info) => {
      if (err) {
        console.log(errorText, err, info);
        return res.molochError(500, errorText);
      }
      res.send(JSON.stringify({success: true, text: successText}));
      processHuntJobs();
    });
  });
}

app.post('/hunt', [noCacheJson, logAction('hunt'), checkCookieToken, checkPermissions(['packetSearch'])], (req, res) => {
  // make sure viewer is not multi
  if (Config.get('multiES', false)) { return res.molochError(401, 'Not supported in multies'); }
  // make sure all the necessary data is included in the post body
  if (!req.body.hunt) { return res.molochError(403, 'You must provide a hunt object'); }
  if (!req.body.hunt.totalSessions) { return res.molochError(403, 'This hunt does not apply to any sessions'); }
  if (!req.body.hunt.name) { return res.molochError(403, 'Missing hunt name'); }
  if (!req.body.hunt.size) { return res.molochError(403, 'Missing max mumber of packets to examine per session'); }
  if (!req.body.hunt.search) { return res.molochError(403, 'Missing packet search text'); }
  if (!req.body.hunt.src && !req.body.hunt.dst) {
    return res.molochError(403, 'The hunt must search source or destination packets (or both)');
  }
  if (!req.body.hunt.query) { return res.molochError(403, 'Missing query'); }
  if (req.body.hunt.query.startTime === undefined || req.body.hunt.query.stopTime === undefined) {
    return res.molochError(403, 'Missing fully formed query (must include start time and stop time)');
  }

  let searchTypes = [ 'ascii', 'asciicase', 'hex', 'wildcard', 'regex', 'hexregex' ];
  if (!req.body.hunt.searchType) { return res.molochError(403, 'Missing packet search text type'); }
  else if (searchTypes.indexOf(req.body.hunt.searchType) === -1) {
    return res.molochError(403, 'Improper packet search text type. Must be "ascii", "asciicase", "hex", "wildcard", "hexregex", or "regex"');
  }

  if (!req.body.hunt.type) { return res.molochError(403, 'Missing packet search type (raw or reassembled packets)'); }
  else if (req.body.hunt.type !== 'raw' && req.body.hunt.type !== 'reassembled') {
    return res.molochError(403, 'Improper packet search type. Must be "raw" or "reassembled"');
  }

  let limit = req.user.createEnabled ? Config.get('huntAdminLimit', 10000000) : Config.get('huntLimit', 1000000);
  if (parseInt(req.body.hunt.totalSessions) > limit) {
    return res.molochError(403, `This hunt applies to too many sessions. Narrow down your session search to less than ${limit} first.`);
  }

  let now = Math.floor(Date.now() / 1000);

  req.body.hunt.name = req.body.hunt.name.replace(/[^-a-zA-Z0-9_: ]/g, '');

  let hunt = req.body.hunt;
  hunt.created = now;
  hunt.status = 'queued'; // always starts as queued
  hunt.userId = req.user.userId;
  hunt.matchedSessions = 0; // start with no matches
  hunt.searchedSessions = 0; // start with no sessions searched
  hunt.query = { // only use the necessary query items
    expression: req.body.hunt.query.expression,
    startTime: req.body.hunt.query.startTime,
    stopTime: req.body.hunt.query.stopTime
  };

  Db.createHunt(hunt, function (err, result) {
    if (err) { console.log('create hunt error', err, result); }
    hunt.id = result._id;
    processHuntJobs( () => {
      return res.send(JSON.stringify({ success: true, hunt: hunt }));
    });
  });
});

app.get('/hunt/list', [noCacheJson, recordResponseTime, checkPermissions(['packetSearch'])], (req, res) => {
  if (Config.get('multiES', false)) { return res.molochError(401, 'Not supported in multies'); }

  let query = {
    sort: {},
    from: parseInt(req.query.start) || 0,
    size: parseInt(req.query.length) || 10000,
    query: { bool: { must: [] } }
  };

  query.sort[req.query.sortField || 'created'] = { order: req.query.desc === 'true' ? 'desc': 'asc'};

  if (req.query.history) { // only get finished jobs
    query.query.bool.must.push({ term: { status: 'finished' } });
    if (req.query.searchTerm) { // apply search term
      query.query.bool.must.push({
        query_string: {
          query : req.query.searchTerm,
          fields: ['name', 'userId']
        }
      });
    }
  } else { // get queued, paused, and running jobs
    query.from = 0;
    query.size = 1000;
    query.query.bool.must.push({ terms: { status: ['queued', 'paused', 'running'] } });
  }

  if (Config.debug) {
    console.log('hunt query:', JSON.stringify(query, null, 2));
  }

  Promise.all([Db.searchHunt(query),
               Db.numberOfHunts()])
    .then(([hunts, total]) => {
      if (hunts.error) { throw hunts.error; }

      let runningJob;

      let results = { total: hunts.hits.total, results: [] };
      for (let i = 0, ilen = hunts.hits.hits.length; i < ilen; i++) {
        const hit = hunts.hits.hits[i];
        let hunt = hit._source;
        hunt.id = hit._id;
        hunt.index = hit._index;
        // don't add the running job to the queue
        if (internals.runningHuntJob && hunt.status === 'running') {
          runningJob = hunt;
          continue;
        }

        // Since hunt isn't cached we can just modify
        if (!req.user.createEnabled && req.user.userId !== hunt.userId) {
          hunt.search = '';
          hunt.searchType = '';
          hunt.id = '';
          hunt.userId = '';
          delete hunt.query;
        }
        results.results.push(hunt);
      }

      const r = {
        recordsTotal: total.count,
        recordsFiltered: results.total,
        data: results.results,
        runningJob: runningJob
      };

      res.send(r);
    }).catch(err => {
      console.log('ERROR - /hunt/list', err);
      return res.molochError(500, 'Error retrieving hunts - ' + err);
    });
});

app.delete('/hunt/:id', [noCacheJson, logAction('hunt/:id'), checkCookieToken, checkPermissions(['packetSearch']), checkHuntAccess], (req, res) => {
  if (Config.get('multiES', false)) { return res.molochError(401, 'Not supported in multies'); }

  Db.deleteHuntItem(req.params.id, function (err, result) {
    if (err || result.error) {
      console.log('ERROR - deleting hunt item', err || result.error);
      return res.molochError(500, 'Error deleting hunt item');
    } else {
      res.send(JSON.stringify({success: true, text: 'Deleted hunt item successfully'}));
    }
  });
});

app.put('/hunt/:id/pause', [noCacheJson, logAction('hunt/:id/pause'), checkCookieToken, checkPermissions(['packetSearch']), checkHuntAccess], (req, res) => {
  if (Config.get('multiES', false)) { return res.molochError(401, 'Not supported in multies'); }
  updateHuntStatus(req, res, 'paused', 'Paused hunt item successfully', 'Error pausing hunt job');
});

app.put('/hunt/:id/play', [noCacheJson, logAction('hunt/:id/play'), checkCookieToken, checkPermissions(['packetSearch']), checkHuntAccess], (req, res) => {
  if (Config.get('multiES', false)) { return res.molochError(401, 'Not supported in multies'); }
  updateHuntStatus(req, res, 'queued', 'Queued hunt item successfully', 'Error starting hunt job');
});

app.get('/:nodeName/hunt/:huntId/remote/:sessionId', [noCacheJson], function (req, res) {
  let huntId = req.params.huntId;
  let sessionId = req.params.sessionId;

  // fetch hunt and session
  Promise.all([Db.get('hunts', 'hunt', huntId),
               Db.get(Db.sid2Index(sessionId), 'session', Db.sid2Id(sessionId))])
    .then(([hunt, session]) => {
      if (hunt.error || session.error) { res.send({ matched: false }); }

      hunt = hunt._source;
      session = session._source;

      let options = buildHuntOptions(hunt);

      sessionHunt(sessionId, options, function (err, matched) {
        if (err) {
          return res.send({ matched: false, error: err });
        }

        if (matched) {
          updateSessionWithHunt(session, sessionId, hunt, huntId);
        }

        return res.send({ matched: matched });
      });
    }).catch((err) => {
      console.log('ERROR - hunt/remote', err);
      res.send({ matched: false, error: err });
    });
});


//////////////////////////////////////////////////////////////////////////////////
//// Lookups
//////////////////////////////////////////////////////////////////////////////////
app.get('/lookups', [noCacheJson, getSettingUser, recordResponseTime], function (req, res) {
  // return nothing if we can't find the user
  const user = req.settingUser;
  if (!user) { return res.send({}); }

  const map = req.query.map && req.query.map === 'true';

  // only get lookups for setting user or shared
  let query = {
    query: {
      bool: {
        must: [
          {
            bool: {
              should: [
                { term: { shared: true } },
                { term: { userId: req.settingUser.userId } }
              ]
            }
          }
        ]

      }
    },
    sort: {},
    size: req.query.length || 50,
    from: req.query.start || 0
  };

  query.sort[req.query.sort || 'name'] = {
    order: req.query.desc === 'true' ? 'desc' : 'asc'
  };

  if (req.query.searchTerm) {
    query.query.bool.must.push({
      wildcard: { name: '*' + req.query.searchTerm + '*' }
    });
  }

  // if fieldType exists, filter it
  if (req.query.fieldType) {
    const fieldType = internals.lookupTypeMap[req.query.fieldType];

    if (fieldType) {
      query.query.bool.must.push({
        exists: { field: fieldType }
      });
    }
  }

  Promise.all([
    Db.searchLookups(query),
    Db.numberOfDocuments('lookups')
  ]).then(([lookups, total]) => {
    if (lookups.error) { throw lookups.error; }

    let results = { list: [], map: {} };
    for (const hit of lookups.hits.hits) {
      let lookup = hit._source;
      lookup.id = hit._id;

      if (lookup.number) {
        lookup.type = 'number';
      } else if (lookup.ip) {
        lookup.type = 'ip';
      } else {
        lookup.type = 'string';
      }

      const values = lookup[lookup.type];

      if (req.query.fieldFormat && req.query.fieldFormat === 'true') {
        const name = `$${lookup.name}`;
        lookup.exp = name;
        lookup.dbField = name;
        lookup.help = lookup.description ?
          `${lookup.description}: ${values.join(', ')}` :
          `${values.join(',')}`;
      }

      lookup.value = values.join('\n');
      delete lookup[lookup.type];

      if (map) {
        results.map[lookup.id] = lookup;
      } else {
        results.list.push(lookup);
      }
    }

    const sendResults = map ? results.map : {
      recordsTotal: total.count,
      recordsFiltered: lookups.hits.total,
      data: results.list
    };

    res.send(sendResults);
  }).catch((err) => {
    console.log('ERROR - /lookups', err);
    return res.molochError(500, 'Error retrieving lookups - ' + err);
  });
});

function createLookupsArray (lookupsString) {
  // split string on commas and newlines
  let values = lookupsString.split(/[,\n]+/g);

  // remove any empty values
  values = values.filter(function (val) {
    return val !== '';
  });

  return values;
}

app.post('/lookups', [noCacheJson, getSettingUser, logAction('lookups'), checkCookieToken], function (req, res) {
  // make sure all the necessary data is included in the post body
  if (!req.body.var) { return res.molochError(403, 'Missing shortcut'); }
  if (!req.body.var.name) { return res.molochError(403, 'Missing shortcut name'); }
  if (!req.body.var.type) { return res.molochError(403, 'Missing shortcut type'); }
  if (!req.body.var.value) { return res.molochError(403, 'Missing shortcut value'); }

  req.body.var.name = req.body.var.name.replace(/[^-a-zA-Z0-9_]/g, '');

  // return nothing if we can't find the user
  const user = req.settingUser;
  if (!user) { return res.send({}); }

  const query = {
    query: {
      bool: {
        must: [
          { term: { name: req.body.var.name } }
        ]
      }
    }
  };

  Db.searchLookups(query)
    .then((lookups) => {
      // search for lookup name collision
      for (const hit of lookups.hits.hits) {
        let lookup = hit._source;
        if (lookup.name === req.body.var.name) {
          return res.molochError(403, `A shortcut with the name, ${req.body.var.name}, already exists`);
        }
      }

      let variable = req.body.var;
      variable.userId = user.userId;

      // comma/newline separated value -> array of values
      const values = createLookupsArray(variable.value);
      variable[variable.type] = values;

      const type = variable.type;
      delete variable.type;
      delete variable.value;

      Db.createLookup(variable, user.userId, function (err, result) {
        if (err) {
          console.log('shortcut create failed', err, result);
          return res.molochError(500, 'Creating shortcut failed');
        }
        variable.id = result._id;
        variable.type = type;
        variable.value = values.join('\n');
        delete variable.ip;
        delete variable.string;
        delete variable.number;
        return res.send(JSON.stringify({ success: true, var: variable }));
      });
    }).catch((err) => {
      console.log('ERROR - /lookups', err);
      return res.molochError(500, 'Error creating lookup - ' + err);
    });
});

app.put('/lookups/:id', [noCacheJson, getSettingUser, logAction('lookups/:id'), checkCookieToken], function (req, res) {
  // make sure all the necessary data is included in the post body
  if (!req.body.var) { return res.molochError(403, 'Missing shortcut'); }
  if (!req.body.var.name) { return res.molochError(403, 'Missing shortcut name'); }
  if (!req.body.var.type) { return res.molochError(403, 'Missing shortcut type'); }
  if (!req.body.var.value) { return res.molochError(403, 'Missing shortcut value'); }

  let sentVar = req.body.var;

  Db.getLookup(req.params.id, (err, fetchedVar) => { // fetch variable
    if (err) {
      console.log('fetching shortcut to update failed', err, fetchedVar);
      return res.molochError(500, 'Fetching shortcut to update failed');
    }

    if (fetchedVar._source.locked) {
      return res.molochError(403, 'Locked Shortcut. Use db.pl script to update this shortcut.');
    }

    // only allow admins or lookup creator to update lookup item
    if (!req.user.createEnabled && req.settingUser.userId !== fetchedVar._source.userId) {
      return res.molochError(403, 'Permission denied');
    }

    // comma/newline separated value -> array of values
    const values = createLookupsArray(sentVar.value);
    sentVar[sentVar.type] = values;
    sentVar.userId = fetchedVar._source.userId;

    delete sentVar.type;
    delete sentVar.value;

    Db.setLookup(req.params.id, fetchedVar.userId, sentVar, (err, info) => {
      if (err) {
        console.log('shortcut update failed', err, info);
        return res.molochError(500, 'Updating shortcut failed');
      }

      sentVar.value = values.join('\n');

      return res.send(JSON.stringify({
        success : true,
        var     : sentVar,
        text    : 'Successfully updated shortcut'
      }));
    });
  });
});

app.delete('/lookups/:id', [noCacheJson, getSettingUser, logAction('lookups/:id'), checkCookieToken], function (req, res) {
  Db.getLookup(req.params.id, (err, variable) => { // fetch variable
    if (err) {
      console.log('fetching shortcut to delete failed', err, variable);
      return res.molochError(500, 'Fetching shortcut to delete failed');
    }

    // only allow admins or lookup creator to delete lookup item
    if (!req.user.createEnabled && req.settingUser.userId !== variable._source.userId) {
      return res.molochError(403, 'Permission denied');
    }

    Db.deleteLookup(req.params.id, variable.userId, function (err, result) {
      if (err || result.error) {
        console.log('ERROR - deleting shortcut', err || result.error);
        return res.molochError(500, 'Error deleting shortcut');
      } else {
        res.send(JSON.stringify({success: true, text: 'Deleted shortcut successfully'}));
      }
    });
  });
});

//////////////////////////////////////////////////////////////////////////////////
//// SPI/PCAP Delete/Scrub
//////////////////////////////////////////////////////////////////////////////////
function pcapScrub(req, res, sid, whatToRemove, endCb) {
  if (pcapScrub.scrubbingBuffers === undefined) {
    pcapScrub.scrubbingBuffers = [Buffer.alloc(5000), Buffer.alloc(5000), Buffer.alloc(5000)];
    pcapScrub.scrubbingBuffers[0].fill(0);
    pcapScrub.scrubbingBuffers[1].fill(1);
    const str = 'Scrubbed! Hoot! ';
    for (let i = 0; i < 5000;) {
      i += pcapScrub.scrubbingBuffers[2].write(str, i);
    }
  }

  function processFile (pcap, pos, i, nextCb) {
    pcap.ref();
    pcap.readPacket(pos, function (packet) {
      pcap.unref();
      if (packet) {
        if (packet.length > 16) {
          try {
            let obj = {};
            pcap.decode(packet, obj);
            pcap.scrubPacket(obj, pos, pcapScrub.scrubbingBuffers[0], whatToRemove === 'all');
            pcap.scrubPacket(obj, pos, pcapScrub.scrubbingBuffers[1], whatToRemove === 'all');
            pcap.scrubPacket(obj, pos, pcapScrub.scrubbingBuffers[2], whatToRemove === 'all');
          } catch (e) {
            console.log(`Couldn't scrub packet at ${pos} -`, e);
          }
          return nextCb(null);
        } else {
          console.log(`Couldn't scrub packet at ${pos}`);
          return nextCb(null);
        }
      }
    });
  }

  Db.getWithOptions(Db.sid2Index(sid), 'session', Db.sid2Id(sid), {_source: 'node,ipProtocol,packetPos'}, function (err, session) {
    let fileNum;
    let itemPos = 0;
    const fields = session._source || session.fields;

    if (whatToRemove === 'spi') { // just removing es data for session
      Db.deleteDocument(session._index, 'session', session._id, function (err, data) {
        return endCb(err, fields);
      });
    } else { // scrub the pcap
      async.eachLimit(fields.packetPos, 10, function (pos, nextCb) {
        if (pos < 0) {
          fileNum = pos * -1;
          return nextCb(null);
        }

        // Get the pcap file for this node a filenum, if it isn't opened then do the filename lookup and open it
        let opcap = Pcap.get(`write${fields.node}:${fileNum}`);
        if (!opcap.isOpen()) {
          Db.fileIdToFile(fields.node, fileNum, function (file) {
            if (!file) {
              console.log(`WARNING - Only have SPI data, PCAP file no longer available.  Couldn't look up in file table ${fields.node}-${fileNum}`);
              return nextCb(`Only have SPI data, PCAP file no longer available for ${fields.node}-${fileNum}`);
            }

            let ipcap = Pcap.get(`write${fields.node}:${file.num}`);

            try {
              ipcap.openReadWrite(file.name, file);
            } catch (err) {
              const errorMsg = `Couldn't open file for writing: ${err}`;
              console.log(`Error - ${errorMsg}`);
              return nextCb(errorMsg);
            }

            processFile(ipcap, pos, itemPos++, nextCb);
          });
        } else {
          processFile(opcap, pos, itemPos++, nextCb);
        }
      },
      function (pcapErr, results) {
        if (whatToRemove === 'all') { // also remove the session data
          Db.deleteDocument(session._index, 'session', session._id, function (err, data) {
            return endCb(pcapErr, fields);
          });
        } else { // just set who/when scrubbed the pcap
          // Do the ES update
          const document = {
            doc: {
              scrubby: req.user.userId || '-',
              scrubat: new Date().getTime()
            }
          };
          Db.update(session._index, 'session', session._id, document, function (err, data) {
            return endCb(pcapErr, fields);
          });
        }
      });
    }
  });
}

app.get('/:nodeName/delete/:whatToRemove/:sid', [checkProxyRequest, checkPermissions(['removeEnabled'])], (req, res) => {
  noCache(req, res);

  res.statusCode = 200;

  pcapScrub(req, res, req.params.sid, req.params.whatToRemove, (err) => {
    res.end();
  });
});

function scrubList(req, res, whatToRemove, list) {
  if (!list) { return res.molochError(200, 'Missing list of sessions'); }

  async.eachLimit(list, 10, function (item, nextCb) {
    const fields = item._source || item.fields;

    isLocalView(fields.node, function () {
      // Get from our DISK
      pcapScrub(req, res, Db.session2Sid(item), whatToRemove, nextCb);
    },
    function () {
      // Get from remote DISK
      let path = `${fields.node}/delete/${whatToRemove}/${Db.session2Sid(item)}`;
      makeRequest(fields.node, path, req.user, function (err, response) {
        setImmediate(nextCb);
      });
    });
  }, function (err) {
    let text;
    if (whatToRemove === 'all') {
      text = `Deletion PCAP and SPI of ${list.length} sessions complete. Give Elasticsearch 60 seconds to complete SPI deletion.`;
    } else if (whatToRemove === 'spi') {
      text = `Deletion SPI of ${list.length} sessions complete. Give Elasticsearch 60 seconds to complete SPI deletion.`;
    } else {
      text = `Scrubbing PCAP of ${list.length} sessions complete`;
    }
    return res.end(JSON.stringify({ success: true, text: text }));
  });
}

app.post('/delete', [noCacheJson, checkCookieToken, logAction(), checkPermissions(['removeEnabled'])], (req, res) => {
  if (req.query.removeSpi !== 'true' && req.query.removePcap !== 'true') {
    return res.molochError(403, `You can't delete nothing`);
  }

  let whatToRemove;
  if (req.query.removeSpi === 'true' && req.query.removePcap === 'true') {
    whatToRemove = 'all';
  } else if (req.query.removeSpi === 'true') {
    whatToRemove = 'spi';
  } else {
    whatToRemove = 'pcap';
  }

  if (req.body.ids) {
    const ids = queryValueToArray(req.body.ids);
    sessionsListFromIds(req, ids, ['node'], function (err, list) {
      scrubList(req, res, whatToRemove, list);
    });
  } else if (req.query.expression) {
    sessionsListFromQuery(req, res, ['node'], function (err, list) {
      scrubList(req, res, whatToRemove, list);
    });
  } else {
    return res.molochError(403, `Error: Missing expression. An expression is required so you don't delete everything.`);
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
      buffer = Buffer.alloc(0);
      ps = [];
    } else {
      buffer = Buffer.alloc(packetshdr.length + packetslen);
      var pos = 0;
      packetshdr.copy(buffer);
      pos += packetshdr.length;
      for(let i = 0, ilen = packets.length; i < ilen; i++) {
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
    session.packetPos = ps;
    delete session.fs;

    if (options.tags) {
      tags = options.tags.replace(/[^-a-zA-Z0-9_:,]/g, "").split(",");
      if (!session.tags) {
        session.tags = [];
      }
      session.tags = session.tags.concat(tags);
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
    var b = Buffer.alloc(12);
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
    tags: req.body.tags,
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
      req.body.tags === undefined) {
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
      tags: req.body.tags,
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
  if (!list) { return res.molochError(200, "Missing list of sessions"); }

  var saveId = Config.nodeName() + "-" + new Date().getTime().toString(36);

  async.eachLimit(list, 10, function(item, nextCb) {
    var fields = item._source || item.fields;
    let sid = Db.session2Sid(item);
    isLocalView(fields.node, function () {
      var options = {
        user: req.user,
        cluster: req.body.cluster,
        id: sid,
        saveId: saveId,
        tags: req.body.tags,
        nodeName: fields.node
      };
      // Get from our DISK
      internals.sendSessionQueue.push(options, nextCb);
    },
    function () {
      let path = `${fields.node}/sendSession/${sid}?saveId=${saveId}&cluster=${req.body.cluster}`;
      if (req.body.tags) {
        path += `&tags=${req.body.tags}`;
      }

      makeRequest(fields.node, path, req.user, (err, response) => {
        setImmediate(nextCb);
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
    if (!nodes[item.node]) {
      nodes[item.node] = [];
    }
    nodes[item.node].push(item.id);
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

app.post('/receiveSession', [noCacheJson], function receiveSession(req, res) {
  if (!req.query.saveId) { return res.molochError(200, "Missing saveId"); }

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
      Db.indexNow("files", "file", Config.nodeName() + "-" + saveId.seq, {num: saveId.seq, name: filename, first: session.firstPacket, node: Config.nodeName(), filesize: -1, locked: 1}, function() {
        cb(filename);
        saveId.filename = filename; // Don't set the saveId.filename until after the first request completes its callback.
      });
    });
  }

  function saveSession() {
    var id = session.id;
    delete session.id;
    Db.indexNow(Db.sid2Index(id), "session", Db.sid2Id(id), session, function(err, info) {
    });
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
      session.node = Config.nodeName();
      buffer = buffer.slice(sessionlen);

      if (filelen > 0) {
        req.pause();

        makeFilename(function (filename) {
          req.resume();
          session.packetPos[0] = - saveId.seq;
          session.fs = [saveId.seq];

          if (saveId.start === 0) {
            file = fs.createWriteStream(filename, {flags: "w"});
          } else {
            file = fs.createWriteStream(filename, {start: saveId.start, flags: "r+"});
          }
          writeHeader = saveId.start === 0;

          // Adjust packet location based on where we start writing
          if (saveId.start > 0) {
            for (var p = 1, plen = session.packetPos.length; p < plen; p++) {
              session.packetPos[p] += (saveId.start - 24);
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

    sessionsListFromIds(req, ids, ["node"], function(err, list) {
      sendSessionsList(req, res, list);
    });
  } else {
    sessionsListFromQuery(req, res, ["node"], function(err, list) {
      sendSessionsList(req, res, list);
    });
  }
});

app.post('/upload', [checkCookieToken, multer({dest:'/tmp', limits: internals.uploadLimits}).single('file')], function (req, res) {
  var exec = require('child_process').exec;

  var tags = '';
  if (req.body.tags) {
    var t = req.body.tags.replace(/[^-a-zA-Z0-9_:,]/g, '').split(',');
    t.forEach(function(tag) {
      if (tag.length > 0) {
        tags += ' --tag ' + tag;
      }
    });
  }

  var cmd = Config.get('uploadCommand')
     .replace('{TAGS}', tags)
     .replace('{NODE}', Config.nodeName())
     .replace('{TMPFILE}', req.file.path)
     .replace('{CONFIG}', Config.getConfigFile());

  console.log('upload command: ', cmd);
  exec(cmd, function (error, stdout, stderr) {
    if (error !== null) {
      console.log('<b>exec error: ' + error);
      res.status(500);
      res.write('<b>Upload command failed:</b><br>');
    }
    res.write(cmd);
    res.write('<br>');
    res.write('<pre>');
    res.write(stdout);
    res.end('</pre>');
    fs.unlinkSync(req.file.path);
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
  app.get('/processCronQueries', function(req, res) {
    processCronQueries();
    res.send("{}");
  });

  // Make sure all jobs have run and return
  app.get('/processHuntJobs', function (req, res) {
    processHuntJobs();

    setTimeout(function checkHuntFinished() {
      if (internals.runningHuntJob) {
        setTimeout(checkHuntFinished, 1000);
      } else {
        Db.search("hunts", "hunt", {query: {term: {status: "queued"}}}, function(err, result) {
          if (result.hits.total > 0) {
            processHuntJobs();
            setTimeout(checkHuntFinished, 1000);
          } else {
            res.send('{}');
          }
        });
      }
    }, 1000);
  });
}

//////////////////////////////////////////////////////////////////////////////////
// Cyberchef
//////////////////////////////////////////////////////////////////////////////////
app.use('/cyberchef/', unsafeInlineCspHeader, (req, res) => {
  let found = false;
  let path = req.path.substring(1);
  if (path === '') {
    path = `CyberChef_v${internals.CYBERCHEFVERSION}.html`;
  }

  fs.createReadStream(`public/CyberChef_v${internals.CYBERCHEFVERSION}.zip`)
    .pipe(unzip.Parse())
    .on('entry', function (entry) {
      if (entry.path === path) {
        entry.pipe(res);
        found = true;
      } else {
        entry.autodrain();
      }
    })
    .on('finish', function () {
      if (!found) {
        res.status(404).end('Page not found');
      }
    });
});

/* cyberchef endpoint - loads the src or dst packets for a session and
 * sends them to cyberchef */
app.get('/:nodeName/session/:id/cyberchef', checkPermissions(['webEnabled']), checkProxyRequest, unsafeInlineCspHeader, (req, res) => {
  processSessionIdAndDecode(req.params.id, 10000, function(err, session, results) {
    if (err) {
      console.log(`ERROR - /${req.params.nodeName}/session/${req.params.id}/cyberchef`, err);
      return res.end("Error - " + err);
    }

    let data = '';
    for (let i = (req.query.type !== 'dst'?0:1), ilen = results.length; i < ilen; i+=2) {
      data += results[i].data.toString('hex');
    }

    res.render('cyberchef.pug', { value: data });
  });
});

//////////////////////////////////////////////////////////////////////////////////
// Vue app
//////////////////////////////////////////////////////////////////////////////////
const Vue = require('vue');
const vueServerRenderer = require('vue-server-renderer');

// Factory function to create fresh Vue apps
function createApp () {
  return new Vue({
    template: `<div id="app"></div>`
  });
}

// expose vue bundles (prod)
app.use('/static', express.static(`${__dirname}/vueapp/dist/static`));
// expose vue bundle (dev)
app.use(['/app.js', '/vueapp/app.js'], express.static(`${__dirname}/vueapp/dist/app.js`));

app.use(cspHeader, (req, res) => {
  let cookieOptions = { path: app.locals.basePath, sameSite: 'Strict' };
  if (Config.isHTTPS()) { cookieOptions.secure = true; }

  // send cookie for basic, non admin functions
  res.cookie(
     'MOLOCH-COOKIE',
     Config.obj2auth({date: Date.now(), pid: process.pid, userId: req.user.userId}, true),
     cookieOptions
  );

  if (!req.user.webEnabled) {
    return res.status(403).send('Permission denied');
  }

  if (req.path === '/users' && !req.user.createEnabled) {
    return res.status(403).send('Permission denied');
  }

  if (req.path === '/settings' && Config.get('demoMode', false)) {
    return res.status(403).send('Permission denied');
  }

  const renderer = vueServerRenderer.createRenderer({
    template: fs.readFileSync('./vueapp/dist/index.html', 'utf-8')
  });

  let theme = req.user.settings.theme || 'default-theme';
  if (theme.startsWith('custom1')) { theme  = 'custom-theme'; }

  let titleConfig = Config.get('titleTemplate', '_cluster_ - _page_ _-view_ _-expression_')
    .replace(/_cluster_/g, internals.clusterName)
    .replace(/_userId_/g, req.user?req.user.userId:'-')
    .replace(/_userName_/g, req.user?req.user.userName:'-');

  let limit = req.user.createEnabled ? Config.get('huntAdminLimit', 10000000) : Config.get('huntLimit', 1000000);

  const appContext = {
    theme: theme,
    titleConfig: titleConfig,
    path: app.locals.basePath,
    version: app.locals.molochversion,
    devMode: Config.get('devMode', false),
    demoMode: Config.get('demoMode', false),
    multiViewer: Config.get('multiES', false),
    themeUrl: theme === 'custom-theme' ? 'user.css' : '',
    huntWarn: Config.get('huntWarn', 100000),
    huntLimit: limit,
    serverNonce: res.locals.nonce
  };

  // Create a fresh Vue app instance
  const vueApp = createApp();

  // Render the Vue instance to HTML
  renderer.renderToString(vueApp, appContext, (err, html) => {
    if (err) {
      console.log(err);
      if (err.code === 404) {
        res.status(404).end('Page not found');
      } else {
        res.status(500).end('Internal Server Error');
      }
      return;
    }

    res.send(html);
  });
});


//////////////////////////////////////////////////////////////////////////////////
//// Cron Queries
//////////////////////////////////////////////////////////////////////////////////

/* Process a single cron query.  At max it will process 24 hours worth of data
 * to give other queries a chance to run.  Because its timestamp based and not
 * lastPacket based since 1.0 it now search all indices each time.
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
    query.query.bool.filter[0] = {range: {timestamp: {gte: cq.lpValue*1000, lt: singleEndTime*1000}}};

    if (Config.debug > 2) {
      console.log("CRON", cq.name, cq.creator, "- start:", new Date(cq.lpValue*1000), "stop:", new Date(singleEndTime*1000), "end:", new Date(endTime*1000), "remaining runs:", ((endTime-singleEndTime)/(24*60*60.0)));
    }

    Db.search('sessions2-*', 'session', query, {scroll: '600s'}, function getMoreUntilDone(err, result) {
      function doNext() {
        count += result.hits.hits.length;

        // No more data, all done
        if (result.hits.hits.length === 0) {
          Db.clearScroll({ body: { scroll_id: result._scroll_id } });
          return setImmediate(whilstCb, "DONE");
        } else {
          var document = { doc: { count: (query.count || 0) + count} };
          Db.update("queries", "query", options.qid, document, {refresh: true}, function () {});
        }

        query = {
          body: {
            scroll_id: result._scroll_id,
          },
          scroll: '600s'
        };

        Db.scroll(query, getMoreUntilDone);
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
          ids.push({id: hits[i]._id, node: hits[i]._source.node});
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
        sessionsListFromIds(null, ids, ["tags", "node"], function(err, list) {
          addTagsList(tags, list, doNext);
        });
      } else {
        console.log("Unknown action", cq);
        doNext();
      }
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

      // Go thru the queries, fetch the user, make the query
      async.eachSeries(Object.keys(queries), function (qid, forQueriesCb) {
        var cq = queries[qid];
        var cluster = null;

        if (Config.debug > 1) {
          console.log("CRON - Running", qid, cq);
        }

        if (!cq.enabled || endTime < cq.lpValue) {
          return forQueriesCb();
        }

        if (cq.action.indexOf("forward:") === 0) {
          cluster = cq.action.substring(8);
        }

        Db.getUserCache(cq.creator, function (err, user) {
          if (err && !user) {return forQueriesCb();}
          if (!user || !user.found) {console.log("User", cq.creator, "doesn't exist"); return forQueriesCb(null);}
          if (!user._source.enabled) {console.log("User", cq.creator, "not enabled"); return forQueriesCb();}
          user = user._source;

          let options = {
            user: user,
            cluster: cluster,
            saveId: Config.nodeName() + "-" + new Date().getTime().toString(36),
            tags: cq.tags.replace(/[^-a-zA-Z0-9_:,]/g, ""),
            qid: qid
          };

          Db.getLookupsCache(cq.creator, (err, lookups) => {
            molochparser.parser.yy = {
              emailSearch: user.emailSearch === true,
              fieldsMap: Config.getFieldsMap(),
              prefix: internals.prefix,
              lookups: lookups,
              lookupTypeMap: internals.lookupTypeMap
            };

            let query = {
              from: 0,
              size: 1000,
              query: {bool: {filter: [{}]}},
              _source: ["_id", "node"]
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
                let document = {
                  doc: {
                    lpValue: lpValue,
                    lastRun: Math.floor(Date.now()/1000),
                    count: (queries[qid].count || 0) + count
                  }
                };

                function continueProcess () {
                  Db.update('queries', 'query', qid, document, { refresh: true }, function () {
                    // If there is more time to catch up on, repeat the loop, although other queries
                    // will get processed first to be fair
                    if (lpValue !== endTime) { repeat = true; }
                    return forQueriesCb();
                  });
                }

                // issue alert via notifier if the count has changed and it has been at least 10 minutes
                if (cq.notifier && count && queries[qid].count !== document.doc.count &&
                  (!cq.lastNotified || (Math.floor(Date.now()/1000) - cq.lastNotified >= 600))) {
                  let newMatchCount = document.doc.lastNotifiedCount ? (document.doc.count - document.doc.lastNotifiedCount) : document.doc.count;
                  let message = `*${cq.name}* cron query match alert:\n*${newMatchCount} new* matches\n*${document.doc.count} total* matches`;
                  issueAlert(cq.notifier, message, continueProcess);
                } else {
                  return continueProcess();
                }
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

  Db.nodesStats({metric: 'jvm,process,fs,os,indices,thread_pool'}, function (err, info) {
    info.nodes.timestamp = new Date().getTime();
    internals.previousNodesStats.push(info.nodes);
  });

  expireCheckAll();
  setInterval(expireCheckAll, 60*1000);

  loadFields();
  setInterval(loadFields, 2*60*1000);

  loadPlugins();

  createRightClicks();
  setInterval(createRightClicks, 5*60*1000);

  if (Config.get("cronQueries", false)) { // this viewer will process the cron queries
    console.log("This node will process Cron Queries, delayed by", internals.cronTimeout, "seconds");
    setInterval(processCronQueries, 60*1000);
    setTimeout(processCronQueries, 1000);
    setInterval(processHuntJobs, 10000);
  }

  var server;
  if (Config.isHTTPS()) {
    server = https.createServer({key: Config.keyFileData, cert: Config.certFileData, secureOptions: require('constants').SSL_OP_NO_TLSv1}, app);
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
//// Command Line Parsing
//////////////////////////////////////////////////////////////////////////////////
function processArgs(argv) {
  for (var i = 0, ilen = argv.length; i < ilen; i++) {
    if (argv[i] === "--help") {
      console.log("node.js [<options>]");
      console.log("");
      console.log("Options:");
      console.log("  -c <config file>      Config file to use");
      console.log("  -host <host name>     Host name to use, default os hostname");
      console.log("  -n <node name>        Node name section to use in config file, default first part of hostname");
      console.log("  --debug               Increase debug level, multiple are supported");
      console.log("  --insecure            Disable cert verification");

      process.exit(0);
    }
  }
}
processArgs(process.argv);
//////////////////////////////////////////////////////////////////////////////////
//// DB
//////////////////////////////////////////////////////////////////////////////////
Db.initialize({host: internals.elasticBase,
               prefix: Config.get("prefix", ""),
               usersHost: Config.get("usersElasticsearch")?Config.get("usersElasticsearch").split(","):undefined,
               usersPrefix: Config.get("usersPrefix"),
               nodeName: Config.nodeName(),
               esClientKey: Config.get("esClientKey", null),
               esClientCert: Config.get("esClientCert", null),
               esClientKeyPass: Config.get("esClientKeyPass", null),
               dontMapTags: Config.get("multiES", false),
               insecure: Config.insecure,
               ca: loadCaTrust(internals.nodeName),
               requestTimeout: Config.get("elasticsearchTimeout", 300),
               debug: Config.debug
              }, main);
