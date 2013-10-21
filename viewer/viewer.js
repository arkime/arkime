/******************************************************************************/
/* viewer.js  -- The main moloch app
 *
 * Copyright 2012-2013 AOL Inc. All rights reserved.
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
"use strict";

var MIN_DB_VERSION = 12;

//// Modules
//////////////////////////////////////////////////////////////////////////////////
try {
var Config         = require('./config.js'),
    express        = require('express'),
    connectTimeout = require('connect-timeout'),
    stylus         = require('stylus'),
    util           = require('util'),
    fs             = require('fs-ext'),
    async          = require('async'),
    url            = require('url'),
    dns            = require('dns'),
    Pcap           = require('./pcap.js'),
    sprintf = require('./public/sprintf.js'),
    Db             = require('./db.js'),
    os             = require('os'),
    zlib           = require('zlib'),
    molochparser   = require('./molochparser.js'),
    passport       = require('passport'),
    DigestStrategy = require('passport-http').DigestStrategy,
    HTTPParser     = process.binding('http_parser').HTTPParser,
    molochversion  = require('./version'),
    httpAgent      = require('http'),
    httpsAgent     = require('https'),
    ForeverAgent   = require('forever-agent');
} catch (e) {
  console.log ("ERROR - Couldn't load some dependancies, maybe need to 'npm install' inside viewer directory", e);
  process.exit(1);
}

try {
  var Png = require('png').Png;
} catch (e) {console.log("WARNING - No png support, maybe need to 'npm install'", e);}

if (typeof express !== "function") {
    console.log("ERROR - Need to run 'npm install' in viewer directory");
    process.exit(1);
}
var app = express();

//////////////////////////////////////////////////////////////////////////////////
//// Config
//////////////////////////////////////////////////////////////////////////////////
var escInfo = Config.get("elasticsearch", "localhost:9200").split(':');

passport.use(new DigestStrategy({qop: 'auth', realm: Config.get("httpRealm", "Moloch")},
  function(userid, done) {
    Db.get("users", "user", userid, function(err, suser) {
      if (err) {return done(err);}
      if (!suser || !suser.exists) {console.log(userid, "doesn't exist"); return done(null, false);}
      if (!suser._source.enabled) {console.log(userid, "not enabled"); return done("Not enabled");}

      suser._source.settings = suser._source.settings || {};
      if (suser._source.emailSearch === undefined) {suser._source.emailSearch = false;}
      if (suser._source.removeEnabled === undefined) {suser._source.removeEnabled = false;}
      if (Config.get("multiES", false)) {suser._source.createEnabled = false;}

      return done(null, suser._source, {ha1: Config.store2ha1(suser._source.passStore)});
    });
  },
  function (options, done) {
      //TODO:  Should check nonce here
      return done(null, true);
  }
));

// Node 0.12 won't need this
var foreverAgent    = new ForeverAgent({minSockets: 21, maxSockets: 20});
var foreverAgentSSL = new ForeverAgent.SSL({minSockets: 21, maxSockets: 20});


app.configure(function() {
  app.enable("jsonp callback");
  app.set('views', __dirname + '/views');
  app.set('view engine', 'jade');
  app.locals.molochversion =  molochversion.version;
  app.locals.isIndex = false;
  app.locals.basePath = Config.basePath();
  app.locals.elasticBase = "http://" + (escInfo[0] === "localhost"?os.hostname():escInfo[0]) + ":" + escInfo[1];
  app.locals.fieldsMap = JSON.stringify(Config.getFieldsMap());
  app.locals.fieldsArr = Config.getFields().sort(function(a,b) {return (a.exp > b.exp?1:-1);});
  app.locals.allowUploads = Config.get("uploadCommand") !== undefined;
  app.locals.sendSession = Config.getObj("sendSession");

  app.use(express.favicon(__dirname + '/public/favicon.ico'));
  app.use(passport.initialize());
  app.use(function(req, res, next) {
    req.url = req.url.replace(Config.basePath(), "/");
    return next();
  });
  app.use(express.bodyParser({uploadDir: Config.get("pcapDir")}));
  app.use(connectTimeout({ time: 60*60*1000 }));
  app.use(express.logger({ format: ':date :username \x1b[1m:method\x1b[0m \x1b[33m:url\x1b[0m :res[content-length] bytes :response-time ms' }));
  app.use(express.compress());
  app.use(express.methodOverride());
  app.use("/", express['static'](__dirname + '/public', { maxAge: 600 * 1000}));
  if (Config.get("passwordSecret")) {
    app.use(function(req, res, next) {
      // 200 for NS
      if (req.url === "/_ns_/nstest.html") {
        return res.end();
      }

      // No auth for stats.json, dstats.json, esstats.json
      if (req.url.match(/^\/[e]*[ds]*stats.json/)) {
        return next();
      }

      // S2S Auth
      if (req.headers['x-moloch-auth']) {
        var obj = Config.auth2obj(req.headers['x-moloch-auth']);
        if (obj.path !== req.url) {
          console.log("ERROR - mismatch url", obj.path, req.url);
          return res.send("Unauthorized based on bad url, check logs on ", os.hostname());
        }
        if (Math.abs(Date.now() - obj.date) > 60000) { // Request has to be +- 60 seconds
          console.log("ERROR - Denying server to server based on timestamp, are clocks out of sync?");
          return res.send("Unauthorized based on timestamp - check that all moloch viewer machines have accurate clocks");
        }

        Db.get("users", "user", obj.user, function(err, suser) {
          if (err) {return res.send("ERROR - " +  err);}
          if (!suser || !suser.exists) {return res.send(obj.user + " doesn't exist");}
          if (!suser._source.enabled) {return res.send(obj.user + " not enabled");}
          req.user = suser._source;
          req.user.settings = req.user.settings || {};
          if (req.user.emailSearch === undefined) {req.user.emailSearch = false;}
          if (req.user.removeEnabled === undefined) {req.user.removeEnabled = false;}
          return next();
        });
        return;
      }

      // Header auth
      if (req.headers[Config.get("userNameHeader")] !== undefined) {
        var userName = req.headers[Config.get("userNameHeader")];
        Db.get("users", "user", userName, function(err, suser) {
          if (err) {return res.send("ERROR - " +  err);}
          if (!suser || !suser.exists) {return res.send(userName + " doesn't exist");}
          if (!suser._source.enabled) {return res.send(userName + " not enabled");}
          if (!suser._source.headerAuthEnabled) {return res.send(userName + " header auth not enabled");}
          req.user = suser._source;
          req.user.settings = req.user.settings || {};
          if (req.user.emailSearch === undefined) {req.user.emailSearch = false;}
          if (req.user.removeEnabled === undefined) {req.user.removeEnabled = false;}
          if (Config.get("multiES", false)) {req.user.createEnabled = false;}
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
    /* Shared password isn't set, who cares about auth */
    app.use(function(req, res, next) {
      req.user = {userId: "anonymous", enabled: true, createEnabled: false, webEnabled: true, headerAuthEnabled: false, emailSearch: true, removeEnabled: true, settings: {}};
      next();
    });
  }

  express.logger.token('username', function(req, res){ return req.user?req.user.userId:"-"; });
});

//////////////////////////////////////////////////////////////////////////////////
//// Utility
//////////////////////////////////////////////////////////////////////////////////
function isEmptyObject(object) { for(var i in object) { return false; } return true; }
function safeStr(str) {
  return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/\"/g,'&quot;').replace(/\'/g, '&#39;').replace(/\//g, '&#47;');
}

//http://garethrees.org/2007/11/14/pngcrush/
var emptyPNG = new Buffer("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAACklEQVR4nGMAAQAABQABDQottAAAAABJRU5ErkJggg==", 'base64');
var PNG_LINE_WIDTH = 256;

function twoDigitString(value) {
  return (value < 10) ? ("0" + value) : value.toString();
}

var FMEnum = Object.freeze({other: 0, ip: 1, tags: 2, hh: 3});
function fmenum(field) {
  if (field.match(/^(a1|a2|xff|dnsip|eip|socksip)$/) !== null) {
    return FMEnum.ip;
  } else if (field.match(/^(ta)$/) !== null) {
    return FMEnum.tags;
  } else if (field.match(/^(hh1|hh2)$/) !== null) {
    return FMEnum.hh;
  }
  return FMEnum.other;
}

//////////////////////////////////////////////////////////////////////////////////
//// DB
//////////////////////////////////////////////////////////////////////////////////
Db.initialize({host : escInfo[0],
               port: escInfo[1],
               /*agent: new httpAgent.Agent({maxSockets: 20}), // Forever agent doesn't work :(*/
               dontMapTags: Config.get("multiES", false)});

function deleteFile(node, id, path, cb) {
  fs.unlink(path, function() {
    Db.deleteDocument('files', 'file', id, function(err, data) {
      cb(null);
    });
  });
}

function isLocalView(node, yesCB, noCB) {
  if (node === Config.nodeName()) {
    return yesCB();
  }

  Db.molochNodeStatsCache(node, function(err, stat) {
    if (err || stat.hostname !== os.hostname()) {
      noCB();
    } else {
      yesCB();
    }
  });
}

//////////////////////////////////////////////////////////////////////////////////
//// Pages
//////////////////////////////////////////////////////////////////////////////////
app.get("/", function(req, res) {
  if (!req.user.webEnabled) {
    return res.send("Moloch Permision Denied");
  }
  res.render('index', {
    user: req.user,
    title: 'Home',
    titleLink: 'sessionsLink',
    isIndex: true
  });
});

app.get("/spiview", function(req, res) {
  if (!req.user.webEnabled) {
    return res.send("Moloch Permision Denied");
  }

  res.render('spiview', {
    user: req.user,
    title: 'SPI View',
    titleLink: 'spiLink',
    isIndex: true,
    reqFields: Config.headers("headers-http-request"),
    resFields: Config.headers("headers-http-response"),
    emailFields: Config.headers("headers-email")
  });
});

app.get("/spigraph", function(req, res) {
  if (!req.user.webEnabled) {
    return res.send("Moloch Permision Denied");
  }
  res.render('spigraph', {
    user: req.user,
    title: 'SPI Graph',
    titleLink: 'spigraphLink',
    isIndex: true
  });
});

app.get("/connections", function(req, res) {
  if (!req.user.webEnabled) {
    return res.send("Moloch Permision Denied");
  }
  res.render('connections', {
    user: req.user,
    title: 'Connections',
    titleLink: 'connectionsLink',
    isIndex: true
  });
});

app.get("/upload", function(req, res) {
  if (!req.user.webEnabled) {
    return res.send("Moloch Permision Denied");
  }
  res.render('upload', {
    user: req.user,
    title: 'Upload',
    titleLink: 'uploadLink',
    isIndex: false
  });
});

app.get('/about', function(req, res) {
  if (!req.user.webEnabled) {
    return res.send("Moloch Permision Denied");
  }
  res.render('about', {
    user: req.user,
    title: 'About',
    titleLink: 'aboutLink'
  });
});

app.get('/files', function(req, res) {
  if (!req.user.webEnabled) {
    return res.send("Moloch Permision Denied");
  }
  res.render('files', {
    user: req.user,
    title: 'Files',
    titleLink: 'filesLink'
  });
});

app.get('/users', function(req, res) {
  if (!req.user.webEnabled || !req.user.createEnabled) {
    return res.send("Moloch Permision Denied");
  }
  res.render('users', {
    user: req.user,
    title: 'Users',
    titleLink: 'usersLink',
    token: Config.obj2auth({date: Date.now(), pid: process.pid, userId: req.user.userId})
  });
});

app.get('/settings', function(req, res) {
  function render(user, cp) {
    if (user.settings === undefined) {user.settings = {};}
    res.render('settings', {
      user: req.user,
      suser: user,
      currentPassword: cp,
      token: Config.obj2auth({date: Date.now(), pid: process.pid, userId: req.user.userId, suserId: user.userId, cp:cp}),
      title: 'Settings',
      titleLink: 'settingsLink'
    });
  }

  if (!req.user.webEnabled) {
    return res.send("Moloch Permision Denied");
  }

  if (Config.get("disableChangePassword", false)) {
    return res.send("Disabled");
  }

  if (req.query.userId) {
    if (!req.user.createEnabled && req.query.userId !== req.user.userId) {
      return res.send("Moloch Permision Denied");
    }
    Db.get("users", 'user', req.query.userId, function(err, user) {
      if (err || !user.exists) {
        console.log("ERROR - /password error", err, user);
        return res.send("Unknown user");
      }
      render(user._source, 0);
    });
  } else {
    render(req.user, 1);
  }
});

app.get('/stats', function(req, res) {
  if (!req.user.webEnabled) {
    return res.send("Moloch Permision Denied");
  }

  var query = {size: 100};

  Db.search('stats', 'stat', query, function(err, data) {
    var hits = data.hits.hits;
    var nodes = [];
    hits.forEach(function(hit) {
      nodes.push(hit._id);
    });
    nodes.sort();
    res.render('stats', {
      user: req.user,
      title: 'Stats',
      titleLink: 'statsLink',
      nodes: nodes
    });
  });
});

app.get('/:nodeName/statsDetail', function(req, res) {
  if (!req.user.webEnabled) {
    return res.send("Moloch Permision Denied");
  }
  res.render('statsDetail', {
    user: req.user,
    nodeName: req.params.nodeName
  });
});

fs.unlink("./public/style.css", function () {}); // Remove old style.css file
app.get('/style.css', function(req, res) {
  fs.readFile("./views/style.styl", 'utf8', function(err, str) {
    if (err) {return console.log("ERROR - ", err);}
    var style = stylus(str, "./views");
    style.render(function(err, css){
      if (err) {return console.log("ERROR - ", err);}
      var date = new Date().toUTCString();
      res.setHeader('Content-Type', 'text/css');
      res.setHeader('Date', date);
      res.setHeader('Cache-Control', 'public, max-age=600');
      res.setHeader('Last-Modified', date);
      res.send(css);
    });
  });
});

//////////////////////////////////////////////////////////////////////////////////
//// EXPIRING
//////////////////////////////////////////////////////////////////////////////////
function statG(dir, func) {
  fs.statVFS(dir, function(err,stat) {
    if (err) {return console.log("ERROR with statVFS", dir, err);}
    var freeG = stat.f_frsize/1024.0*stat.f_bavail/(1024.0*1024.0);
    func(freeG);
  });
}

function expireOne (ourInfo, allInfo, minFreeG, pcapDir, nextCb) {

  var i;
  var nodes = [];

  // Find all the nodes that are on the same device
  for (i = 0; i < allInfo.length; i++) {
    if (allInfo[i].stat.dev === ourInfo.stat.dev) {
      nodes.push(allInfo[i].node);
    }
  }

  var query = { fields: [ 'num', 'name', 'first', 'size', 'node' ],
                from: '0',
                size: 10,
                query: { bool: {
                  must:     { terms: {node: nodes}},
                  must_not: { term: {locked: 1}}
                }},
                sort: { num: { order: 'asc' } } };

  var done = false;
  async.until(
    function () { // until test
      return done;
    },
    function (untilNextCb) { // until iterator
      Db.search('files', 'file', query, function(err, data) {
        console.log("expireOne result = \n", util.inspect(data, false, 12));
        if (err || data.error || data.hits.total <= query.size) {
          done = true;
          return untilNextCb();
        }

        async.forEachSeries(data.hits.hits, function(item, forNextCb) {
          statG(pcapDir, function(freeG) {
            console.log(freeG, "< ", minFreeG);
            if (freeG < minFreeG) {
              console.log("Deleting", item);
              deleteFile(item.fields.node, item._id, item.fields.name, forNextCb);
            } else {
              done = true;
              return forNextCb("Done");
            }
          });
        },
        function(err) {
          return untilNextCb();
        });
      });
    },
    function (err) {
      return nextCb();
    });
}

function expireCheckOne (ourInfo, allInfo, nextCb) {
  var node = ourInfo.node;
  var pcapDir = Config.getFull(node, "pcapDir");
  var freeSpaceG = Config.getFull(node, "freeSpaceG", 301);

  // Check if our pcap dir is full
  statG(pcapDir, function(freeG) {
    if (freeG < freeSpaceG) {
      expireOne(ourInfo, allInfo, freeSpaceG, pcapDir, nextCb);
    } else {
      nextCb();
    }
  });
}

function expireCheckAll () {
  // Find all the nodes running on this host
  Db.hostnameToNodeids(os.hostname(), function(nodes) {
    // Find all the pcap dirs for local nodes
    async.map(nodes, function (node, cb) {
      var pcapDir = Config.getFull(node, "pcapDir");
      if (typeof pcapDir !== "string") {
        return cb("ERROR - couldn't find pcapDir setting for node: " + node + "\nIf you have it set try running:\nnpm remove iniparser; npm cache clean; npm install iniparser");
      }
      fs.stat(pcapDir, function(err,stat) {
        cb(null, {node: node, stat: stat});
      });
    },
    function (err, allInfo) {
      if (err) {
        return console.log(err);
      }
      // Now gow through all the local nodes and check them
      async.forEachSeries(allInfo, function (info, cb) {
        expireCheckOne(info, allInfo, cb);
      }, function (err) {
      });
    });
  });
}
//////////////////////////////////////////////////////////////////////////////////
//// Sessions Query
//////////////////////////////////////////////////////////////////////////////////
function addSortToQuery(query, info, d) {
  if (!info || !info.iSortingCols || parseInt(info.iSortingCols, 10) === 0) {
    if (d) {
      if (!query.sort) {
        query.sort = [];
      }
      query.sort.push({});
      query.sort[query.sort.length-1][d] = {order: "asc"};
    }
    return;
  }

  if (!query.sort) {
    query.sort = [];
  }

  var i;
  for (i = 0; i < parseInt(info.iSortingCols, 10); i++) {
    if (!info["iSortCol_" + i] || !info["sSortDir_" + i] || !info["mDataProp_" + info["iSortCol_" + i]]) {
      continue;
    }

    var obj = {};
    var field = info["mDataProp_" + info["iSortCol_" + i]];
    obj[field] = {order: info["sSortDir_" + i]};
    query.sort.push(obj);
    if (field === "fp") {
      query.sort.push({fpd: {order: info["sSortDir_" + i]}});
    } else if (field === "lp") {
      query.sort.push({lpd: {order: info["sSortDir_" + i]}});
    }
  }
}

function getIndices(startTime, stopTime, cb) {
  var indices = [];
  Db.status("sessions-*", function(err, status) {

    if (err || status.error) {
      return cb("");
    }

    var rotateIndex = Config.get("rotateIndex", "daily");


    var offset = 86400;
    if (rotateIndex === "hourly") {
      offset = 3600;
    }

    startTime = Math.floor(startTime/offset)*offset;

    while (startTime < stopTime) {
      var iname;
      var d = new Date(startTime*1000);
      switch (rotateIndex) {
      case "monthly":
        iname = "sessions-" +
          twoDigitString(d.getUTCFullYear()%100) + 'm' +
          twoDigitString(d.getUTCMonth()+1);
        break;
      case "weekly":
        var jan = new Date(d.getUTCFullYear(), 0, 0);
        iname = "sessions-" +
          twoDigitString(d.getUTCFullYear()%100) + 'w' +
          twoDigitString(Math.floor((d - jan) / 604800000));
        break;
      case "hourly":
        iname = "sessions-" +
          twoDigitString(d.getUTCFullYear()%100) +
          twoDigitString(d.getUTCMonth()+1) +
          twoDigitString(d.getUTCDate()) + 'h' +
          twoDigitString(d.getUTCHours());
        break;
      default:
        iname = "sessions-" +
          twoDigitString(d.getUTCFullYear()%100) +
          twoDigitString(d.getUTCMonth()+1) +
          twoDigitString(d.getUTCDate());
        break;
      }

      startTime += offset;

      if (status.indices[iname] && (indices.length === 0 || iname !== indices[indices.length-1])) {
        indices.push(iname);
      }
    }

    if (indices.length === 0) {
      return cb("sessions-*");
    }

    return cb(indices.join());
  });
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
        delete parent.term;
        outstanding++;
        var query;
        if (item === "ta") {
          query = {bool: {must: {wildcard: {_id: obj[item]}},
                          must_not: {wildcard: {_id: "http:header:*"}}
                         }
                  };
        } else {
          query = {wildcard: {_id: "http:header:" + obj[item].toLowerCase()}};
        }
        Db.search('tags', 'tag', {size:500, fields:["id", "n"], query: query}, function(err, result) {
          var terms = [];
          result.hits.hits.forEach(function (hit) {
            terms.push(hit.fields.n);
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
      Db.fileNameToFile(name, function (file) {
        outstanding--;
        if (file === null) {
          err = "File '" + name + "' not found";
        } else {
          obj.bool = {must: [{term: {no: file.node}}, {term: {fs: file.num}}]};
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
  var limit = (req.query.iDisplayLength?Math.min(parseInt(req.query.iDisplayLength, 10),100000):100);
  var i;


  var query = {from: req.query.iDisplayStart || 0,
               size: limit,
               query: {filtered: {query: {}}}
              };

  var interval;
  if (req.query.date && req.query.date === '-1') {
    interval = 60*60; // Hour to be safe
    query.query.filtered.query.match_all = {};
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
    if (req.query.strictly === "true") {
      query.query.filtered.query.bool = {must: [{range: {fp: {gte: req.query.startTime}}}, {range: {lp: {lte: req.query.stopTime}}}]};
    } else {
      query.query.filtered.query.range = {lp: {gte: req.query.startTime, lte: req.query.stopTime}};
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
    query.query.filtered.query.range = {lp: {from: req.query.startTime}};
    if (req.query.date <= 5*24) {
      interval = 60; // minute
    } else {
      interval = 60*60; // hour
    }
  }

  if (req.query.facets) {
    query.facets = {
                     dbHisto: {histogram : {key_field: "lp", value_field: "db", interval: interval, size:1440}},
                     paHisto: {histogram : {key_field: "lp", value_field: "pa", interval: interval, size:1440}},
                     map1: {terms : {field: "g1", size:1000}},
                     map2: {terms : {field: "g2", size:1000}}
                   };
  }

  addSortToQuery(query, req.query, "fp");

  var err = null;
  molochparser.parser.yy = {emailSearch: req.user.emailSearch === true,
                              fieldsMap: Config.getFieldsMap()};
  if (req.query.expression) {
    req.query.expression = req.query.expression.replace(/\\/g, "\\\\");
    try {
      query.query.filtered.filter = molochparser.parse(req.query.expression);
    } catch (e) {
      err = e;
    }
  }

  if (!err && req.query.view && req.user.views && req.user.views[req.query.view]) {
    try {
      var viewExpression = molochparser.parse(req.user.views[req.query.view].expression);
      if (query.query.filtered.filter === undefined) {
        query.query.filtered.filter = viewExpression;
      } else {
        query.query.filtered.filter = {bool: {must: [viewExpression, query.query.filtered.filter]}};
      }
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
      if (query.query.filtered.filter === undefined) {
        query.query.filtered.filter = userExpression;
      } else {
        query.query.filtered.filter = {bool: {must: [userExpression, query.query.filtered.filter]}};
      }
    } catch (e) {
      console.log("ERR - Forced expression doesn't compile", req.user.expression, e);
      err = e;
    }
  }

  lookupQueryItems(query.query.filtered, function (lerr) {
    if (req.query.date && req.query.date === '-1') {
      return buildCb(err || lerr, query, "sessions*");
    }

    getIndices(req.query.startTime, req.query.stopTime, function(indices) {
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

  delete query.facets;
  if (req.query.segments === "all") {
    indices = "sessions-*";
    query.query.filtered.query = {match_all: {}};
  }

  // Do a ro search on each item
  var writes = 0;
  async.eachLimit(list, 10, function(item, nextCb) {
    if (!item.fields.ro || processedRo[item.fields.ro]) {
      if (writes++ > 100) {
        writes = 0;
        async.setImmediate(nextCb);
      } else {
        nextCb();
      }
      return;
    }
    processedRo[item.fields.ro] = true;

    query.query.filtered.filter = {term: {ro: item.fields.ro}};

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

function sessionsListFromQuery(req, fields, cb) {
  if (req.query.segments && fields.indexOf("ro") === -1) {
    fields.push("ro");
  }

  buildSessionQuery(req, function(err, query, indices) {
    query.fields = fields;
    Db.searchPrimary(indices, 'session', query, function(err, result) {
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

  async.eachLimit(ids, 10, function(id, nextCb) {
    Db.getWithOptions('sessions-' + id.substr(0,id.indexOf('-')), 'session', id, {fields: fields.join(",")}, function(err, session) {
      list.push(session);
      nextCb(null);
    });
  }, function(err) {
    if (req.query.segments) {
      buildSessionQuery(req, function(err, query, indices) {
        query.fields = fields;
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

function noCache(req, res) {
  res.header('Cache-Control', 'no-cache, private, no-store, must-revalidate, max-stale=0, post-check=0, pre-check=0');
}

var previousNodeStats = [];
Db.nodesStats({fs: 1}, function (err, info) {
  info.nodes.timestamp = new Date().getTime();
  previousNodeStats.push(info.nodes);
});


app.get('/esstats.json', function(req, res) {
  var stats = [];
  var r;

  async.parallel({
    nodes: function(nodesCb) {
      Db.nodesStats({jvm: 1, process: 1, fs: 1, search: 1, os: 1}, nodesCb);
    },
    health: function (healthCb) {
      Db.healthCache(healthCb);
    }
  },
  function(err, results) {
    if (err || !results.nodes) {
      console.log ("ERROR", err);
      r = {sEcho: req.query.sEcho,
           health: results.health,
           iTotalRecords: 0,
           iTotalDisplayRecords: 0,
           aaData: []};
      return res.send(r);
    }

    var now = new Date().getTime();
    while (previousNodeStats.length > 1 && previousNodeStats[1].timestamp + 10000 < now) {
      previousNodeStats.shift();
    }

    var nodes = Object.keys(results.nodes.nodes);
    for (var n = 0; n < nodes.length; n++) {
      var node = results.nodes.nodes[nodes[n]];
      stats.push({
        name: node.name,
        storeSize: node.indices.store.size_in_bytes,
        docs: node.indices.docs.count,
        searches: node.indices.search.query_current,
        searchesTime: node.indices.search.query_time_in_millis,
        heapSize: node.jvm.mem.heap_used_in_bytes,
        nonHeapSize: node.jvm.mem.non_heap_used_in_bytes,
        cpu: node.process.cpu.percent,
        read: 0,
        write: 0,
        load: node.os.load_average
      });

      var oldnode = previousNodeStats[0][nodes[n]];
      if (oldnode) {
        var olddisk = [0, 0], newdisk = [0, 0];
        for (var i = 0; i < oldnode.fs.data.length; i++) {
          olddisk[0] += oldnode.fs.data[i].disk_read_size_in_bytes;
          olddisk[1] += oldnode.fs.data[i].disk_write_size_in_bytes;
          newdisk[0] += node.fs.data[i].disk_read_size_in_bytes;
          newdisk[1] += node.fs.data[i].disk_write_size_in_bytes;
        }

        stats[stats.length-1].read  = Math.ceil((newdisk[0] - olddisk[0])/(node.timestamp - oldnode.timestamp));
        stats[stats.length-1].write = Math.ceil((newdisk[1] - olddisk[1])/(node.timestamp - oldnode.timestamp));
      }
    }

    results.nodes.nodes.timestamp = new Date().getTime();
    previousNodeStats.push(results.nodes.nodes);

    r = {sEcho: req.query.sEcho,
         health: results.health,
         iTotalRecords: stats.length,
         iTotalDisplayRecords: stats.length,
         aaData: stats};
    res.send(r);
  });
});

app.get('/stats.json', function(req, res) {
  noCache(req, res);

  var columns = ["", "_id", "currentTime", "totalPackets", "totalK", "totalSessions", "monitoring", "memory", "diskQueue", "freeSpaceM", "deltaPackets", "deltaBytes", "deltaSessions", "deltaDropped", "deltaMS"];
  var limit = (req.query.iDisplayLength?Math.min(parseInt(req.query.iDisplayLength, 10),1000000):500);

  var query = {fields: columns,
               from: req.query.iDisplayStart || 0,
               size: limit,
               script_fields: {
                 deltaBytesPerSec: {script :"floor(_source.deltaBytes * 1000.0/_source.deltaMS)"},
                 deltaPacketsPerSec: {script :"floor(_source.deltaPackets * 1000.0/_source.deltaMS)"},
                 deltaSessionsPerSec: {script :"floor(_source.deltaSessions * 1000.0/_source.deltaMS)"},
                 deltaDroppedPerSec: {script :"floor(_source.deltaDropped * 1000.0/_source.deltaMS)"}
               }
              };
  addSortToQuery(query, req.query, "_uid");

  async.parallel({
    stats: function (cb) {
      Db.search('stats', 'stat', query, function(err, result) {
        var i;
        if (err || result.error) {
          res.send({total: 0, results: []});
        } else {
          var results = {total: result.hits.total, results: []};
          for (i = 0; i < result.hits.hits.length; i++) {
            result.hits.hits[i].fields.id     = result.hits.hits[i]._id;
            result.hits.hits[i].fields.memory = result.hits.hits[i].fields.memory || 0;
            result.hits.hits[i].fields.diskQueue = result.hits.hits[i].fields.diskQueue || 0;
            results.results.push(result.hits.hits[i].fields);
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
    var r = {sEcho: req.query.sEcho,
             iTotalRecords: results.total,
             iTotalDisplayRecords: results.stats.total,
             aaData: results.stats.results};
    res.send(r);
  });
});

app.get('/dstats.json', function(req, res) {
  noCache(req, res);

  var query = {size: req.query.size || 1440,
               sort: { currentTime: { order: 'desc' } },
               query: {
                 filtered: {
                   query: {
                     match_all: {}
                   },
                   filter: {
                     and: [
                       {
                         term: { nodeName: req.query.nodeName}
                       },
                       {
                         numeric_range: { currentTime: { from: req.query.start, to: req.query.stop } }
                       },
                       {
                         term: { interval: req.query.interval || 60}
                       }
                     ]
                   }
                 }
               },
               fields: ["currentTime", req.query.name],
               script_fields: {
                 deltaBits: {script :"floor(_source.deltaBytes * 8.0)"},
                 deltaBytesPerSec: {script :"floor(_source.deltaBytes * 1000.0/_source.deltaMS)"},
                 deltaBitsPerSec: {script :"floor(_source.deltaBytes * 1000.0/_source.deltaMS * 8)"},
                 deltaPacketsPerSec: {script :"floor(_source.deltaPackets * 1000.0/_source.deltaMS)"},
                 deltaSessionsPerSec: {script :"floor(_source.deltaSessions * 1000.0/_source.deltaMS)"},
                 deltaDroppedPerSec: {script :"floor(_source.deltaDropped * 1000.0/_source.deltaMS)"}
               }
              };

  Db.search('dstats', 'dstat', query, function(err, result) {
    var i;
    var data = [];
    var num = (req.query.stop - req.query.start)/req.query.step;

    for (i = 0; i < num; i++) {
      data[i] = 0;
    }

    var mult = 1;
    if (req.query.name === "freeSpaceM") {
      mult = 1000000;
    }

    if (result && result.hits) {
      for (i = 0; i < result.hits.hits.length; i++) {
        var pos = Math.floor((result.hits.hits[i].fields.currentTime - req.query.start)/req.query.step);
        data[pos] = mult * (result.hits.hits[i].fields[req.query.name] || 0);
      }
    }
    res.send(data);
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

app.get('/files.json', function(req, res) {
  noCache(req, res);

  var columns = ["num", "node", "name", "locked", "first", "filesize"];
  var limit = (req.query.iDisplayLength?Math.min(parseInt(req.query.iDisplayLength, 10),10000):500);

  var query = {fields: columns,
               from: req.query.iDisplayStart || 0,
               size: limit
              };

  addSortToQuery(query, req.query, "num");

  async.parallel({
    files: function (cb) {
      Db.search('files', 'file', query, function(err, result) {
        var i;

        if (err || result.error) {
          return cb(err || result.error);
        }

        var results = {total: result.hits.total, results: []};
        for (i = 0; i < result.hits.hits.length; i++) {
          if (result.hits.hits[i].fields.locked === undefined) {
            result.hits.hits[i].fields.locked = 0;
          }
          result.hits.hits[i].fields.id = result.hits.hits[i]._id;
          results.results.push(result.hits.hits[i].fields);
        }

        async.forEach(results.results, function (item, cb) {
          if (item.filesize && item.filesize !== 0) {
            return cb(null);
          }

          isLocalView(item.node, function () {
            fs.stat(item.name, function (err, stats) {
              if (err || !stats) {
                item.filesize = -1;
              } else {
                item.filesize = stats.size;
                if (item.locked) {
                  Db.updateFileSize(item, stats.size);
                }
              }
              cb(null);
            });
          }, function () {
            item.filesize = -2;
            cb(null);
          });
        }, function (err) {
          cb(null, results);
        });
      });
    },
    total: function (cb) {
      Db.numberOfDocuments('files', cb);
    }
  },
  function(err, results) {
    if (err) {
      return res.send({total: 0, results: []});
    }

    var r = {sEcho: req.query.sEcho,
             iTotalRecords: results.total,
             iTotalDisplayRecords: results.files.total,
             aaData: results.files.results};
    res.send(r);
  });
});

app.post('/users.json', function(req, res) {
  var fields = ["userId", "userName", "expression", "enabled", "createEnabled", "webEnabled", "headerAuthEnabled", "emailSearch", "removeEnabled"];
  var limit = (req.body.iDisplayLength?Math.min(parseInt(req.body.iDisplayLength, 10),10000):500);

  var query = {fields: fields,
               from: req.body.iDisplayStart || 0,
               size: limit
              };

  addSortToQuery(query, req.body, "userId");

  async.parallel({
    users: function (cb) {
      Db.search('users', 'user', query, function(err, result) {
        var i;

        if (err || result.error) {
          res.send({total: 0, results: []});
        } else {
          var results = {total: result.hits.total, results: []};
          for (i = 0; i < result.hits.hits.length; i++) {
            result.hits.hits[i].fields.id = result.hits.hits[i]._id;
            result.hits.hits[i].fields.expression = safeStr(result.hits.hits[i].fields.expression || "");
            result.hits.hits[i].fields.headerAuthEnabled = result.hits.hits[i].fields.headerAuthEnabled || false;
            result.hits.hits[i].fields.emailSearch = result.hits.hits[i].fields.emailSearch || false;
            result.hits.hits[i].fields.removeEnabled = result.hits.hits[i].fields.removeEnabled || false;
            result.hits.hits[i].fields.userName = safeStr(result.hits.hits[i].fields.userName || "");
            results.results.push(result.hits.hits[i].fields);
          }
          cb(null, results);
        }
      });
    },
    total: function (cb) {
      Db.numberOfDocuments('users', cb);
    }
  },
  function(err, results) {
    var r = {sEcho: req.body.sEcho,
             iTotalRecords: results.total,
             iTotalDisplayRecords: results.users.total,
             aaData: results.users.results};
    res.send(r);
  });
});

function mapMerge(facets) {
  var map = {};

  facets.map1.terms.forEach(function (item) {
    if (item.count < 0) {
      item.count = 0x7fffffff;
    }
    map[item.term] = item.count;
  });

  facets.map2.terms.forEach(function (item) {
    if (item.count < 0) {
      item.count = 0x7fffffff;
    }
    if (!map[item.term]) {
      map[item.term] = 0;
    }
    map[item.term] += item.count;
  });
  return map;
}

function histoMerge(req, query, facets, graph) {
  graph.lpHisto = [];
  graph.dbHisto = [];
  graph.paHisto = [];
  graph.xmin = req.query.startTime  * 1000|| null;
  graph.xmax = req.query.stopTime * 1000 || null;
  graph.interval = query.facets?query.facets.dbHisto.histogram.interval || 60 : 60;

  facets.paHisto.entries.forEach(function (item) {
    graph.lpHisto.push([item.key*1000, item.count]);
    graph.paHisto.push([item.key*1000, item.total]);
  });

  facets.dbHisto.entries.forEach(function (item) {
    graph.dbHisto.push([item.key*1000, item.total]);
  });

}

app.get('/sessions.json', function(req, res) {
  var i;

  var graph = {
        interval: 60,
        lpHisto: [],
        dbHisto: [],
        paHisto: []
      };
  var map = {};
  buildSessionQuery(req, function(bsqErr, query, indices) {
    if (bsqErr) {
      var r = {sEcho: req.query.sEcho,
               iTotalRecords: 0,
               iTotalDisplayRecords: 0,
               graph: graph,
               map: map,
               bsqErr: bsqErr.toString(),
               aaData:[]};
      res.send(r);
      return;
    }
    query.fields = ["pr", "ro", "db", "fp", "lp", "a1", "p1", "a2", "p2", "pa", "by", "no", "us", "g1", "g2", "esub", "esrc", "edst", "efn", "dnsho", "tls", "ircch"];

    if (query.facets && query.facets.dbHisto) {
      graph.interval = query.facets.dbHisto.histogram.interval;
    }

    console.log("sessions.json query", JSON.stringify(query));

    async.parallel({
      sessions: function (sessionsCb) {
        Db.searchPrimary(indices, 'session', query, function(err, result) {
          //console.log("sessions query = ", util.inspect(result, false, 50));
          if (err || result.error) {
            console.log("sessions.json error", err, (result?result.error:null));
            sessionsCb(null, {total: 0, results: []});
            return;
          }

          if (!result.facets) {
            result.facets = {map1: {terms: []}, map2: {terms: []}, dbHisto: {entries: []}, paHisto: {entries: []}};
          }

          histoMerge(req, query, result.facets, graph);
          map = mapMerge(result.facets);

          var results = {total: result.hits.total, results: []};
          var hits = result.hits.hits;
          for (i = 0; i < hits.length; i++) {
            if (!hits[i] || !hits[i].fields) {
              continue;
            }
            hits[i].fields.index = hits[i]._index;
            hits[i].fields.id = hits[i]._id;
            results.results.push(hits[i].fields);
          }
          sessionsCb(null, results);
        });
      },
      total: function (totalCb) {
        Db.numberOfDocuments('sessions-*', totalCb);
      },
      health: function (healthCb) {
        Db.healthCache(healthCb);
      }
    },
    function(err, results) {
      var r = {sEcho: req.query.sEcho,
               iTotalRecords: results.total,
               iTotalDisplayRecords: (results.sessions?results.sessions.total:0),
               graph: graph,
               health: results.health,
               map: map,
               aaData: (results.sessions?results.sessions.results:[])};
      try {
        res.send(r);
      } catch (c) {
      }
    });
  });
});

app.get('/spigraph.json', function(req, res) {
  req.query.facets = 1;
  buildSessionQuery(req, function(bsqErr, query, indices) {
    var results = {items: [], graph: {}, map: {}, iTotalReords: 0};
    if (bsqErr) {
      results.bsqErr = bsqErr.toString();
      res.send(results);
      return;
    }

    delete query.sort;
    query.size = 0;
    var size = +req.query.size || 20;

    var field = req.query.field || "no";
    query.facets.field = {terms: {field: field, size: size}};

    /* Need the setImmediate so we don't blow max stack frames */
    var eachCb;
    switch (fmenum(field)) {
    case FMEnum.other:
      eachCb = function (item, cb) {async.setImmediate(cb);};
      break;
    case FMEnum.ip:
      eachCb = function(item, cb) {
        item.name = Pcap.inet_ntoa(item.name);
        async.setImmediate(cb);
      };
      break;
    case FMEnum.tags:
      eachCb = function(item, cb) {
        Db.tagIdToName(item.name, function (name) {
          item.name = name;
          async.setImmediate(cb);
        });
      };
      break;
    case FMEnum.hh:
      eachCb = function(item, cb) {
        Db.tagIdToName(item.name, function (name) {
          item.name = name.substring(12);
          async.setImmediate(cb);
        });
      };
      break;
    }

    Db.healthCache(function(err, health) {results.health = health;});
    Db.numberOfDocuments('sessions-*', function (err, total) {results.iTotalRecords = total;});
    Db.searchPrimary(indices, 'session', query, function(err, result) {
      if (err || result.error) {
        results.bsqErr = "Error performing query";
        console.log("spigraph.json error", err, (result?result.error:null));
        return res.send(results);
      }
      results.iTotalDisplayRecords = result.hits.total;
      results.map = mapMerge(result.facets);

      histoMerge(req, query, result.facets, results.graph);
      results.map = mapMerge(result.facets);

      var facets = result.facets.field.terms;
      var interval = query.facets.dbHisto.histogram.interval;
      var filter;

      if (query.query.filtered.filter === undefined) {
        query.query.filtered.filter = {term: {}};
        filter = query.query.filtered.filter;
      } else {
        query.query.filtered.filter = {bool: {must: [{term: {}}, query.query.filtered.filter]}};
        filter = query.query.filtered.filter.bool.must[0];
      }

      delete query.facets.field;

      var queries = [];
      facets.forEach(function(item) {
        filter.term[field] = item.term;
        queries.push(JSON.stringify(query));
      });

      Db.msearch(indices, 'session', queries, function(err, result) {
        if (!result.responses) {
          return res.send(results);
        }


        result.responses.forEach(function(item, i) {
          var r = {name: facets[i].term, count: facets[i].count, graph: {lpHisto: [], dbHisto: [], paHisto: []}};

          histoMerge(req, query, result.responses[i].facets, r.graph);
          if (r.graph.xmin === null) {
            r.graph.xmin = results.graph.xmin || results.graph.paHisto[0][0];
          }

          if (r.graph.xmax === null) {
            r.graph.xmax = results.graph.xmax || results.graph.paHisto[results.graph.paHisto.length-1][0];
          }

          r.map = mapMerge(result.responses[i].facets);
          eachCb(r, function () {
            results.items.push(r);
            if (results.items.length === result.responses.length) {
              results.items = results.items.sort(function(a,b) {return b.count - a.count;});
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
    return res.send({spi:{}});
  }

  var spiDataMaxIndices = +Config.get("spiDataMaxIndices", 1);

  if (req.query.date === '-1' && spiDataMaxIndices !== -1) {
    return res.send({spi: {}, bsqErr: "'All' date range not allowed for spiview query"});
  }

  buildSessionQuery(req, function(bsqErr, query, indices) {
    if (bsqErr) {
      var r = {spi: {},
               bsqErr: bsqErr.toString()
               };
      return res.send(r);
    }

    delete query.sort;

    if (!query.facets) {
      query.facets = {};
    }

    req.query.spi.split(",").forEach(function (item) {
      var parts = item.split(":");
      query.facets[parts[0]] = {terms: {field: parts[0], size:parseInt(parts[1], 10)}};
    });
    query.size = 0;

    //console.log("spiview.json query", JSON.stringify(query), "indices", indices);

    var graph;
    var map;

    var indicesa = indices.split(",");
    if (spiDataMaxIndices !== -1 && indicesa.length > spiDataMaxIndices) {
      bsqErr = "To save ES from blowing up, reducing number of spi data indices searched from " + indicesa.length + " to " + spiDataMaxIndices + ".  This can be increased by setting spiDataMaxIndices in the config file.  Indices being searched: ";
      indices = indicesa.slice(-spiDataMaxIndices).join(",");
      bsqErr += indices;
    }

    var iTotalDisplayRecords = 0;

    async.parallel({
      spi: function (sessionsCb) {
        Db.searchPrimary(indices, 'session', query, function(err, result) {
          if (err || result.error) {
            console.log("spiview.json error", err, (result?result.error:null));
            sessionsCb(null, {});
            return;
          }

          iTotalDisplayRecords = result.hits.total;

          if (result.facets.pr) {
            result.facets.pr.terms.forEach(function (item) {
              item.term = Pcap.protocol2Name(item.term);
            });
          }

          if (result.facets.dbHisto) {
            graph = {
              interval: query.facets.dbHisto.histogram.interval,
              lpHisto: [],
              dbHisto: [],
              paHisto: []
            };

            histoMerge(req, query, result.facets, graph);
            map = mapMerge(result.facets);
            delete result.facets.dbHisto;
            delete result.facets.paHisto;
            delete result.facets.map1;
            delete result.facets.map2;
          }

          sessionsCb(null, result.facets);
        });
      },
      total: function (totalCb) {
        Db.numberOfDocuments('sessions-*', totalCb);
      },
      health: function (healthCb) {
        Db.healthCache(healthCb);
      }
    },
    function(err, results) {
      function tags(container, field, doneCb, offset) {
        if (!container[field]) {
          return doneCb(null);
        }
        async.map(container[field].terms, function (item, cb) {
          Db.tagIdToName(item.term, function (name) {
            item.term = name.substring(offset);
            cb(null, item);
          });
        },
        function(err, tagsResults) {
          container[field].terms = tagsResults;
          doneCb(err);
        });
      }

      async.parallel([
        function(parallelCb) {
          tags(results.spi, "ta", parallelCb, 0);
        },
        function(parallelCb) {
          tags(results.spi, "hh", parallelCb, 12);
        },
        function(parallelCb) {
          tags(results.spi, "hh1", parallelCb, 12);
        },
        function(parallelCb) {
          tags(results.spi, "hh2", parallelCb, 12);
        }],
        function() {
          r = {health: results.health,
               iTotalRecords: results.total,
               spi: results.spi,
               iTotalDisplayRecords: iTotalDisplayRecords,
               graph: graph,
               map: map,
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

app.get('/connections.json', function(req, res) {
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
    cb();
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
      cb();
    });
  }

  buildSessionQuery(req, function(bsqErr, query, indices) {
    if (bsqErr) {
      var r = {};
      res.send(r);
      return;
    }

    if (query.query.filtered.filter === undefined) {
      query.query.filtered.filter = {bool: {must: [{exists: {field: req.query.srcField}}, {exists: {field: req.query.dstField}}]}};
    } else {
      query.query.filtered.filter = {bool: {must: [query.query.filtered.filter, {exists: {field: req.query.srcField}}, {exists: {field: req.query.dstField}}]}};
    }

    query.fields = ["by", "db", "pa", "no", fsrc, fdst];
    if (dstipport) {
      query.fields.push("p2");
    }

    console.log("connections.json query", JSON.stringify(query));

    async.parallel({
      health: function (healthCb) {
        Db.healthCache(healthCb);
      },
      graph: function (graphCb) {
        Db.searchPrimary(indices, 'session', query, graphCb);
      }
    },
    function(err, results) {
      if (err || results.graph.error) {
        console.log("connections.json error", err, results.graph.error);
        res.send({});
        return;
      }

      var i;

      async.eachLimit(results.graph.hits.hits, 10, function(hit, hitCb) {
        var f = hit.fields;
        if (f[fsrc] === undefined || f[fdst] === undefined) {
          return async.setImmediate(hitCb);
        }

        var asrc = Array.isArray(f[fsrc])? f[fsrc] : [f[fsrc]];
        var adst = Array.isArray(f[fdst])? f[fdst] : [f[fdst]];

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
          async.setImmediate(hitCb);
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

        res.send({health: results.health, nodes: nodes, links: links, iTotalDisplayRecords: results.graph.hits.total});
      });
    });
  });
});

function csvListWriter(req, res, list, pcapWriter, extension) {
  list = list.sort(function(a,b){return a.fields.lp - b.fields.lp;});
  res.write("Protocol, First Packet, Last Packet, Source IP, Source Port, Source Geo, Destination IP, Destination Port, Destination Geo, Packets, Bytes, Data Bytes, Node\r\n");

  var i;
  for (i = 0; i < list.length; i++) {
    if (!list[i].fields) {
      continue;
    }
    var f = list[i].fields;
    var pr;
    switch (f.pr) {
    case 1:
      pr = "icmp";
      break;
    case 6:
      pr = "tcp";
      break;
    case 17:
      pr =  "udp";
      break;
    }


    res.write(pr + ", " + f.fp + ", " + f.lp + ", " + Pcap.inet_ntoa(f.a1) + ", " + f.p1 + ", " + (f.g1||"") + ", "  + Pcap.inet_ntoa(f.a2) + ", " + f.p2 + ", " + (f.g2||"") + ", " + f.pa + ", " + f.by + ", " + f.db + ", " + f.no + "\r\n");
  }
  res.end();
}

app.get(/\/sessions.csv.*/, function(req, res) {
  res.setHeader("Content-Type", "text/csv");
  var fields = ["pr", "fp", "lp", "a1", "p1", "g1", "a2", "p2", "g2", "by", "db", "pa", "no"];

  if (req.query.ids) {
    var ids = req.query.ids.split(",");

    sessionsListFromIds(req, ids, fields, function(err, list) {
      csvListWriter(req, res, list);
    });
  } else {
    sessionsListFromQuery(req, fields, function(err, list) {
      csvListWriter(req, res, list);
    });
  }
});

app.get('/uniqueValue.json', function(req, res) {
  noCache(req, res);
  var query;

  if (req.query.type === "tags") {
    query = {bool: {must: {wildcard: {_id: req.query.filter + "*"}},
                  must_not: {wildcard: {_id: "http:header:*"}}
                     }
          };
  } else {
    query = {wildcard: {_id: "http:header:" + req.query.filter + "*"}};
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
  var writes = 0;
  if (parseInt(req.query.counts, 10) || 0) {
    writeCb = function (item, cb) {
      res.write("" + item.term + ", " + item.count + "\n");
      if (writes++ > 1000) {
        writes = 0;
        async.setImmediate(cb);
      } else {
        cb();
      }
    };
  } else {
    writeCb = function (item, cb) {
      res.write("" + item.term + "\n");
      if (writes++ > 1000) {
        writes = 0;
        async.setImmediate(cb);
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
      item.term = Pcap.inet_ntoa(item.term);
      writeCb(item, cb);
    };
    break;
  case FMEnum.tags:
    eachCb = function(item, cb) {
      Db.tagIdToName(item.name, function (name) {
        item.term = name;
        writeCb(item, cb);
      });
    };
    break;
  case FMEnum.hh:
    eachCb = function(item, cb) {
      Db.tagIdToName(item.name, function (name) {
        item.term = name.substring(12);
        writeCb(item, cb);
      });
    };
    break;
  }

  buildSessionQuery(req, function(err, query, indices) {
    delete query.sort;
    delete query.facets;

    if (req.query.field.match(/^(rawus|rawua)$/)) {
      var field = req.query.field.substring(3);
      query.size   = 200000;

      query.fields = [field];

      if (query.query.filtered.filter === undefined) {
        query.query.filtered.filter = {exists: {field: field}};
      } else {
        query.query.filtered.filter = {and: [query.query.filtered.filter, {exists: {field: field}}]};
      }

      console.log("unique query", indices, JSON.stringify(query));
      Db.searchPrimary(indices, 'session', query, function(err, result) {
        console.log("uniqueing");
        var counts = {};

        // Count up hits
        var hits = result.hits.hits;
        for (var i = 0; i < hits.length; i++) {
          var avalue = hits[i].fields[field];
          if (Array.isArray(avalue)) {
            for (var j = 0; j < avalue.length; j++) {
              var value = avalue[j];
              counts[value] = (counts[value] || 0) + 1;
            }
          } else {
            counts[avalue] = (counts[avalue] || 0) + 1;
          }
        }

        // Change to facet looking array
        var facets = [];
        for (var key in counts) {
          facets.push({term: key, count: counts[key]});
        }
        facets = facets.sort(function(a,b) {return b.count - a.count;});
        

        async.forEachSeries(facets, eachCb, function () {
          res.end();
        });
      });
    } else {
      console.log("unique facet", indices, JSON.stringify(query));
      query.facets = {facets: { terms : {field : req.query.field, size: 1000000}}};
      query.size = 0;
      Db.searchPrimary(indices, 'session', query, function(err, result) {
        async.forEachSeries(result.facets.facets.terms, eachCb, function () {
          res.end();
        });
      });
    }
  });
});

function processSessionId(id, fullSession, headerCb, packetCb, endCb, maxPackets, limit) {
  function processFile(pcap, pos, i, nextCb) {
    pcap.ref();
    pcap.readPacket(pos, function(packet) {
      switch(packet) {
      case null:
        endCb("Error loading data for session " + id, null);
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

  var options;
  if (!fullSession) {
    options  = {fields: "no,ps"};
  }

  Db.getWithOptions('sessions-' + id.substr(0,id.indexOf('-')), 'session', id, options, function(err, session) {
    var fields;

    if (err || !session.exists) {
      console.log("session get error", err, session);
      return endCb("Not Found", null);
    }

    if (fullSession) {
      fields = session._source;
    } else {
      fields = session.fields;
    }


    if (maxPackets && fields.ps.length > maxPackets) {
      fields.ps.length = maxPackets;
    }

    /* Old Format: Every item in array had file num (top 28 bits) and file pos (lower 36 bits)
     * New Format: Negative numbers are file numbers until next neg number, otherwise file pos */
    var newFormat = false;
    var fileNum;
    var itemPos = 0;
    async.eachLimit(fields.ps, limit || 1, function(item, nextCb) {
      var pos;

      if (item < 0) {
        newFormat = true;
        fileNum = item * -1;
        return nextCb(null);
      } else if (newFormat) {
        pos  = item;
      } else  {
        // javascript doesn't have 64bit bitwise operations
        fileNum = Math.floor(item / 0xfffffffff);
        pos  = item % 0x1000000000;
      }

      // Get the pcap file for this node a filenum, if it isn't opened then do the filename lookup and open it
      var opcap = Pcap.get(fields.no + ":" + fileNum);
      if (!opcap.isOpen()) {
        Db.fileIdToFile(fields.no, fileNum, function(file) {

          if (!file) {
            console.log("WARNING - Only have SPI data, PCAP file no longer available", fields.no + '-' + fileNum);
            return nextCb("Only have SPI data, PCAP file no longer available for " + fields.no + '-' + fileNum);
          }

          var ipcap = Pcap.get(fields.no + ":" + file.num);

          try {
            ipcap.open(file.name);
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
      function tags(container, field, doneCb, offset) {
        if (!container[field]) {
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

      async.parallel([
        function(parallelCb) {
          if (!fields.ta) {
            fields.ta = [];
            return parallelCb(null);
          }
          tags(fields, "ta", parallelCb, 0);
        },
        function(parallelCb) {
          tags(fields, "hh", parallelCb, 12);
        },
        function(parallelCb) {
          tags(fields, "hh1", parallelCb, 12);
        },
        function(parallelCb) {
          tags(fields, "hh2", parallelCb, 12);
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
          endCb(pcapErr, fields);
        }
      );
    });
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
      Pcap.reassemble_tcp(packets, Pcap.inet_ntoa(session.a1) + ':' + session.p1, function(err, results) {
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
  var i;

  for (var pos = 0; pos < input.length; pos += 16) {
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

    for (i = 0; i < line.length; i++) {
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

function localSessionDetailReturnFull(req, res, session, incoming) {
  var outgoing = [];
  for (var r = 0; r < incoming.length; r++) {
    outgoing[r]= {ts: incoming[r].ts, html: "", bytes:0};
    for (var p = 0; p < incoming[r].pieces.length; p++) {
      outgoing[r].bytes += incoming[r].pieces[p].raw.length;
      if (req.query.base === "hex") {
        outgoing[r].html += '<pre>' + toHex(incoming[r].pieces[p].raw, req.query.line === "true") + '</pre>';
      } else if (req.query.base === "ascii") {
        outgoing[r].html += '<pre>' + safeStr(incoming[r].pieces[p].raw.toString("binary")) + '</pre>';
      } else if (req.query.base === "utf8") {
        outgoing[r].html += '<pre>' + safeStr(incoming[r].pieces[p].raw.toString("utf8")) + '</pre>';
      } else {
        outgoing[r].html += safeStr(incoming[r].pieces[p].raw.toString()).replace(/\r?\n/g, '<br>');
      }

      if(incoming[r].pieces[p].bodyNum !== undefined) {
        var url = req.params.nodeName + "/" +
                  session.id + "/body/" +
                  incoming[r].pieces[p].bodyType + "/" +
                  incoming[r].pieces[p].bodyNum + "/" +
                  incoming[r].pieces[p].bodyName + ".pellet";

        if (incoming[r].pieces[p].bodyType === "image") {
          outgoing[r].html += "<img src=\"" + url + "\">";
        } else {
          outgoing[r].html += "<a class=\"imagetag-" + session.id + "\" href=\"" + url + "\">" + incoming[r].pieces[p].bodyName + "</a>";
        }
      }
    }
  }

  res.render('sessionDetail', {
    user: req.user,
    session: session,
    data: outgoing,
    query: req.query,
    reqFields: Config.headers("headers-http-request"),
    resFields: Config.headers("headers-http-response"),
    emailFields: Config.headers("headers-email")
  });
}


// Needs to be rewritten, this sucks
function gzipDecode(req, res, session, incoming) {
  var kind;

  var outgoing = [];

  if (incoming[0].data.slice(0,4).toString() === "HTTP") {
    kind = [HTTPParser.RESPONSE, HTTPParser.REQUEST];
  } else {
    kind = [HTTPParser.REQUEST, HTTPParser.RESPONSE];
  }
  var parsers = [new HTTPParser(kind[0]), new HTTPParser(kind[1])];

  parsers[0].onBody = parsers[1].onBody = function(buf, start, len) {
    //console.log("onBody", this.pos, this.gzip);
    var pos = this.pos;

    // This isn't a gziped request
    if (!this.gzip) {
      outgoing[pos] = {ts: incoming[pos].ts, pieces:[{raw: buf}]};
      return;
    }

    // Copy over the headers
    if (!outgoing[pos]) {
      outgoing[pos] = {ts: incoming[pos].ts, pieces:[{raw: buf.slice(0, start)}]};
    }

    if (!this.inflator) {
      this.inflator = zlib.createGunzip()
        .on("data", function (b) {
          var tmp = Buffer.concat([outgoing[pos].pieces[0].raw, new Buffer(b)]);
          outgoing[pos].pieces[0].raw = tmp;
        })
        .on("error", function (e) {
          outgoing[pos].pieces[0].raw = buf;
        })
        .on("end", function () {
        });
    }

    this.inflator.write(buf.slice(start,start+len));
  };

  parsers[0].onMessageComplete = parsers[1].onMessageComplete = function() {
    //console.log("onMessageComplete", this.pos, this.gzip);
    var pos = this.pos;

    if (pos > 0) {
      parsers[(pos+1)%2].reinitialize(kind[(pos+1)%2]);
    }

    var nextCb = this.nextCb;
    this.nextCb = null;
    if (this.inflator) {
      this.inflator.end(null, function () {
        async.setImmediate(nextCb);
      });
      this.inflator = null;
    } else {
      outgoing[pos] = {ts: incoming[pos].ts, pieces: [{raw: incoming[pos].data}]};
      if (nextCb) {
        async.setImmediate(nextCb);
      }
    }
  };

  parsers[0].onHeadersComplete = parsers[1].onHeadersComplete = function(info) {
    var h;
    this.gzip = false;
    for (h = 0; h < info.headers.length; h += 2) {
      // If Content-Type is gzip then stop, otherwise look for encoding
      if (info.headers[h].match(/Content-Type/i) && info.headers[h+1].match(/gzip/i)) {
        this.gzip = true;
        break;
      }

      // Seperate if since we break after 1 content-encoding no matter what
      if (info.headers[h].match(/Content-Encoding/i)) {
        if (info.headers[h+1].match(/gzip/i)) {
          this.gzip = true;
        }
        break;
      }
    }
    //console.log("onHeadersComplete", this.pos, this.gzip);
  };

  var p = 0;
  async.forEachSeries(incoming, function(item, nextCb) {
    var pos = p;
    p++;
    parsers[(pos%2)].pos = pos;

    if (!item) {
    } else if (item.data.length === 0) {
      outgoing[pos] = {ts: incoming[pos].ts, pieces:[{raw: item.data}]};
      async.setImmediate(nextCb);
    } else {
      parsers[(pos%2)].nextCb = nextCb;
      var out = parsers[(pos%2)].execute(item.data, 0, item.data.length);
      if (typeof out === "object") {
        outgoing[pos] = {ts: incoming[pos].ts, pieces:[{raw: item.data}]};
        console.log("ERROR", out);
      }
      if (parsers[(pos%2)].nextCb) {
        async.setImmediate(parsers[(pos%2)].nextCb);
        parsers[(pos%2)].nextCb = null;
      }
    }
  }, function (err) {
    req.query.needgzip = "false";
    parsers[0].finish();
    parsers[1].finish();
    setTimeout(localSessionDetailReturnFull, 100, req, res, session, outgoing);
  });
}

function imageDecodeHTTP(req, res, session, incoming, findBody) {
  var kind;

  if (incoming[0].data.slice(0,4).toString() === "HTTP") {
    kind = [HTTPParser.RESPONSE, HTTPParser.REQUEST];
  } else {
    kind = [HTTPParser.REQUEST, HTTPParser.RESPONSE];
  }
  var parsers = [new HTTPParser(kind[0]), new HTTPParser(kind[1])];

  var bodyNum = 0;
  var bodyType = "file";
  parsers[0].onBody = parsers[1].onBody = function(buf, start, len) {
    //console.log("onBody", this.pos, start, len);
    if (findBody === bodyNum) {
      return res.end(buf.slice(start));
    }

    var pos = this.pos;

    // Copy over the headers
    if (outgoing[pos] === undefined) {
      if (this.image) {
        outgoing[pos] = {ts: incoming[pos].ts, pieces: [{bodyNum: bodyNum, bodyType:"image", bodyName:"image" + bodyNum}]};
      } else {
        outgoing[pos] = {ts: incoming[pos].ts, pieces: [{bodyNum: bodyNum, bodyType:"file", bodyName:"file" + bodyNum}]};
      }
      outgoing[pos].pieces[0].raw = buf.slice(0, start);
    } else if (outgoing[pos].data === undefined) {
      outgoing[pos].pieces[0].raw = new Buffer(0);
    }
    bodyNum++;
  };

  parsers[0].onMessageComplete = parsers[1].onMessageComplete = function() {
    if (this.pos > 0 && this.hinfo && this.hinfo.statusCode !== 100) {
      parsers[(this.pos+1)%2].reinitialize(kind[(this.pos+1)%2]);
    }
    var pos = this.pos;

    //console.log("onMessageComplete", this.pos);

    if (!outgoing[pos]) {
      outgoing[pos] = {ts: incoming[pos].ts, pieces: [{raw: incoming[pos].data}]};
    }
  };

  parsers[0].onHeadersComplete = parsers[1].onHeadersComplete = function(info) {
    var pos = this.pos;
    this.hinfo = info;

    //console.log("onHeadersComplete", this.pos, info);

    var h;
    this.image = false;
    for (h = 0; h < info.headers.length; h += 2) {
      if (info.headers[h].match(/Content-Type/i)) {
        if (info.headers[h+1].match(/^image/i)) {
          this.image = true;
        }
        break;
      }
    }
  };

  var outgoing = [];

  var p = 0;
  async.forEachSeries(incoming, function(item, nextCb) {
    parsers[(p%2)].pos = p;
    //console.log("for", p);

    if (!item) {
    } else if (item.data.length === 0) {
      outgoing[p] = {ts: incoming[p].ts, pieces:[{raw: item.data}]};
    } else {
      var out = parsers[(p%2)].execute(item.data, 0, item.data.length);
      if (typeof out === "object") {
        outgoing[p] = {ts: incoming[p].ts, pieces:[{raw: item.data}]};
        console.log("ERROR", out);
      }

      if (!outgoing[p]) {
        outgoing[p] = {ts: incoming[p].ts, pieces: [{raw: incoming[p].data}]};
      }
    }

    if (res.finished === true) {
      return nextCb("Done!");
    } else {
      async.setImmediate(nextCb);
    }
    p++;
  }, function (err) {
    if (findBody === -1) {
      async.setImmediate(localSessionDetailReturnFull,req, res, session, outgoing);
    }
  });
}

function imageDecodeSMTP(req, res, session, incoming, findBody) {
  var outgoing = [];

  var STATES = {
    cmd: 1,
    header: 2,
    data: 3,
    mime: 4,
    mime_data: 5,
    ignore: 6
  };

  var states = [STATES.cmd, STATES.cmd];
  var bodyNum = 0;
  var bodyType = "file";
  var bodyName = "unknown";

  function parse(data, p) {
    var lines = data.toString("binary").replace(/\r?\n$/, '').split(/\r?\n|\r/);
    var state = states[p%2];
    var header = "";
    var mime;
    var boundaries = [];
    var pieces = [{raw: ""}];
    var b;
    var matches;

    linesloop:
    for (var l = 0; l < lines.length; l++) {
      switch (state) {
      case STATES.cmd:
        pieces[pieces.length-1].raw += lines[l] + "\n";

        if (lines[l].toUpperCase() === "DATA") {
          state = STATES.header;
          header = "";
          boundaries = [];
        } else if (lines[l].toUpperCase() === "STARTTLS") {
          state = STATES.ignore;
        }
        break;
      case STATES.header:
        pieces[pieces.length-1].raw += lines[l] + "\n";
        if (lines[l][0] === " " || lines[l][0] === "\t") {
          header += lines[l];
          continue;
        }
        if (header.substr(0, 13).toLowerCase() === "content-type:") {
          if ((matches = header.match(/boundary\s*=\s*("?)([^"]*)\1/))) {
            boundaries.push(matches[2]);
          }
        }
        if (lines[l] === "") {
          state = STATES.data;
          continue;
        }
        header = lines[l];
        break;
      case STATES.data:
        pieces[pieces.length-1].raw += lines[l] + "\n";
        if (lines[l] === ".") {
          state = STATES.cmd;
          continue;
        }

        if (lines[l][0] === '-') {
          for (b = 0; b < boundaries.length; b++) {
            if (lines[l].substr(2, boundaries[b].length) === boundaries[b]) {
              state = STATES.mime;
              mime = {line:"", base64:0};
              continue linesloop;
            }
          }
        }
        break;
      case STATES.mime:
        if (lines[l] === ".") {
          state = STATES.cmd;
          continue;
        }

        pieces[pieces.length-1].raw += lines[l] + "\n";

        if (lines[l][0] === " " || lines[l][0] === "\t") {
          mime.line += lines[l];
          continue;
        }
        if (mime.line.substr(0, 13).toLowerCase() === "content-type:") {
          if ((matches = mime.line.match(/boundary\s*=\s*("?)([^"]*)\1/))) {
            boundaries.push(matches[2]);
          }
          if ((matches = mime.line.match(/name\s*=\s*("?)([^"]*)\1/))) {
            bodyName = matches[2];
          }

          if (mime.line.match(/content-type: image/i)) {
            bodyType = "image";
          }

        } else if (mime.line.match(/content-disposition:/i)) {
          if ((matches = mime.line.match(/filename\s*=\s*("?)([^"]*)\1/))) {
            bodyName = matches[2];
          }
        } else if (mime.line.match(/content-transfer-encoding:.*base64/i)) {
          mime.base64 = 1;
          mime.doit = 1;
        }
        if (lines[l] === "") {
          if (mime.doit) {
            pieces[pieces.length-1].bodyNum = bodyNum+1;
            pieces[pieces.length-1].bodyType = bodyType;
            pieces[pieces.length-1].bodyName = bodyName;
            pieces.push({raw: ""});
            bodyType = "file";
            bodyName = "unknown";
            bodyNum++;
          }
          state = STATES.mimedata;
          continue;
        }
        mime.line = lines[l];
        break;
      case STATES.mimedata:
        if (lines[l] === ".") {
          if (findBody === bodyNum) {
            return res.end();
          }
          state = STATES.cmd;
          continue;
        }

        if (lines[l][0] === '-') {
          for (b = 0; b < boundaries.length; b++) {
            if (lines[l].substr(2, boundaries[b].length) === boundaries[b]) {
              if (findBody === bodyNum) {
                return res.end();
              }
              state = STATES.mime;
              mime = {line:"", base64:0};
              continue linesloop;
            }
          }
        }

        if (!mime.doit) {
          pieces[pieces.length-1].raw += lines[l] + "\n";
        } else if (findBody === bodyNum) {
          res.write(new Buffer(lines[l], 'base64'));
        }
        break;
      }
    }
    states[p%2] = state;

    return pieces;
  }

  var p = 0;
  for (p = 0; p < incoming.length; p++) {
    if (incoming[p].data.length === 0) {
      outgoing[p] = {ts: incoming[p].ts, pieces:[{raw: incoming[p].data}]};
    } else {
      outgoing[p] = {ts: incoming[p].ts, pieces: parse(incoming[p].data, p)};
    }
    if (res.finished === true) {
      break;
    }
  }

  if (findBody === -1) {
    async.setImmediate(localSessionDetailReturnFull, req, res, session, outgoing);
  }
}

function imageDecode(req, res, session, results, findBody) {
  if ((results[0].data.length >= 4 && results[0].data.slice(0,4).toString() === "HTTP") ||
      (results[1] && results[1].data.length >= 4 && results[1].data.slice(0,4).toString() === "HTTP")) {
    return imageDecodeHTTP(req, res, session, results, findBody);
  }

  if ((results[0].data.length >= 4 && results[0].data.slice(0,4).toString().match(/(HELO|EHLO)/)) ||
      (results[1] && results[1].data.length >= 4 && results[1].data.slice(0,4).toString().match(/(HELO|EHLO)/)) ||
      (results[2] && results[1].data.length >= 4 && results[2].data.slice(0,4).toString().match(/(HELO|EHLO)/))) {
    return imageDecodeSMTP(req, res, session, results, findBody);
  }

  req.query.needimage = "false";
  if (findBody === -1) {
    async.setImmediate(localSessionDetailReturn, req, res, session, results);
  }
}

function localSessionDetailReturn(req, res, session, incoming) {
  if (incoming.length > 200) {
    incoming.length = 200;
  }

  if (req.query.needgzip === "true" && incoming.length > 0) {
    return gzipDecode(req, res, session, incoming);
  }

  if (req.query.needimage === "true" && incoming.length > 0) {
    return imageDecode(req, res, session, incoming, -1);
  }

  var outgoing = [];
  for (var r = 0; r < incoming.length; r++) {
    outgoing.push({pieces: [{raw: incoming[r].data}], ts: incoming[r].ts});
  }
  localSessionDetailReturnFull(req, res, session, outgoing);
}


function localSessionDetail(req, res) {
  if (!req.query) {
    req.query = {gzip: false, line: false, base: "natural"};
  }

  req.query.needgzip  = req.query.gzip  || false;
  req.query.needimage = req.query.image || false;
  req.query.line  = req.query.line  || false;
  req.query.base  = req.query.base  || "ascii";

  var packets = [];
  processSessionId(req.params.id, true, null, function (pcap, buffer, cb, i) {
    var obj = {};
    if (buffer.length > 16) {
      try {
        pcap.decode(buffer, obj);
      } catch (e) {
        obj = {ip: {p: "Error decoding" + e}};
        console.trace(e);
      }
    } else {
      obj = {ip: {p: "Empty"}};
    }
    packets[i] = obj;
    cb(null);
  },
  function(err, session) {
    if (err && session === null) {
      return res.send("Couldn't look up SPI data, error for session " + req.params.id + " Error: " +  err);
    }
    session.id = req.params.id;
    session.ta = session.ta.sort();
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
      localSessionDetailReturn(req, res, session, [{data: err || "No pcap data found"}]);
    } else if (packets[0].ip === undefined) {
      localSessionDetailReturn(req, res, session, [{data: "Couldn't decode pcap file, check viewer log"}]);
    } else if (packets[0].ip.p === 1) {
      Pcap.reassemble_icmp(packets, function(err, results) {
        localSessionDetailReturn(req, res, session, results || [{data: err}]);
      });
    } else if (packets[0].ip.p === 6) {
      Pcap.reassemble_tcp(packets, Pcap.inet_ntoa(session.a1) + ':' + session.p1, function(err, results) {
        localSessionDetailReturn(req, res, session, results || [{data: err}]);
      });
    } else if (packets[0].ip.p === 17) {
      Pcap.reassemble_udp(packets, function(err, results) {
        localSessionDetailReturn(req, res, session, results || [{data: err}]);
      });
    } else {
      localSessionDetailReturn(req, res, session, [{data: "Unknown ip.p=" + packets[0].ip.p}]);
    }
  },
  req.query.needimage === "true"?10000:400, 10);
}

function getViewUrl(node, cb) {
  var url = Config.getFull(node, "viewUrl");
  if (url) {
    cb(null, url, url.slice(0, 5) === "https"?httpsAgent:httpAgent);
    return;
  }

  Db.molochNodeStatsCache(node, function(err, stat) {
    if (err) {
      return cb(err);
    }

    if (Config.isHTTPS(node)) {
      cb(null, "https://" + stat.hostname + ":" + Config.getFull(node, "viewPort", "8005"), httpsAgent);
    } else {
      cb(null, "http://" + stat.hostname + ":" + Config.getFull(node, "viewPort", "8005"), httpAgent);
    }
  });
}

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

function proxyRequest (req, res) {
  noCache(req, res);

  getViewUrl(req.params.nodeName, function(err, viewUrl, agent) {
    if (err) {
      console.log(err);
      res.send("Can't find view url for '" + req.params.nodeName + "' check viewer logs on " + os.hostname());
    }
    var info = url.parse(viewUrl);
    info.path = req.url;
    info.agent = (agent === httpAgent?foreverAgent:foreverAgentSSL);
    info.rejectUnauthorized = true;
    addAuth(info, req.user, req.params.nodeName);

    var preq = agent.request(info, function(pres) {
      pres.on('data', function (chunk) {
        res.write(chunk);
      });
      pres.on('end', function () {
        res.end();
      });
    });

    preq.on('error', function (e) {
      console.log("ERROR - Couldn't proxy request=", info, "\nerror=", e);
      res.send("Error talking to node '" + req.params.nodeName + "' using host '" + info.host + "' check viewer logs on " + os.hostname());
    });
    preq.end();
  });
}

app.get('/:nodeName/:id/sessionDetail', function(req, res) {
  noCache(req, res);

  isLocalView(req.params.nodeName, function () {
    localSessionDetail(req, res);
  },
  function () {
    proxyRequest(req, res);
  });
});


app.get('/:nodeName/:id/body/:bodyType/:bodyNum/:bodyName', function(req, res) {
  isLocalView(req.params.nodeName, function () {
    processSessionIdAndDecode(req.params.id, 10000, function(err, session, results) {
      if (err) {
        return res.send("Error");
      }
      if (req.params.bodyType === "file") {
        res.setHeader("Content-Type", "application/force-download");
      }
      return imageDecode(req, res, session, results, +req.params.bodyNum);
    });
  },
  function () {
    proxyRequest(req, res);
  });
});

app.get('/:nodeName/:id/bodypng/:bodyType/:bodyNum/:bodyName', function(req, res) {
  isLocalView(req.params.nodeName, function () {
    if (!Png) {
      return res.send (emptyPNG);
    }
    processSessionIdAndDecode(req.params.id, 10000, function(err, session, results) {
      if (err) {
        return res.send (emptyPNG);
      }
      res.setHeader("Content-Type", "image/png");
      var newres = {
        finished: false,
        fullbuf: new Buffer(0),
        write: function(buf) {
          this.fullbuf = Buffer.concat([this.fullbuf, buf]);
        },
        end: function(buf) {
          this.finished = true;
          if (buf) {this.write(buf);}
          if (this.fullbuf.length === 0) {
            return res.send (emptyPNG);
          }
          var png = new Png(this.fullbuf, PNG_LINE_WIDTH, Math.ceil(this.fullbuf.length/PNG_LINE_WIDTH), 'gray');
          var png_image = png.encodeSync();

          res.send(png_image);
        }
      };
      return imageDecode(req, newres, session, results, +req.params.bodyNum);
    });
  },
  function () {
    proxyRequest(req, res);
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

app.get('/:nodeName/pcapng/:id.pcapng', function(req, res) {
  noCache(req, res);

  res.setHeader("Content-Type", "application/vnd.tcpdump.pcap");
  res.statusCode = 200;

  isLocalView(req.params.nodeName, function () {
    writePcapNg(res, req.params.id, {writeHeader: !req.query || !req.query.noHeader || req.query.noHeader !== "true"}, function () {
      res.end();
    });
  },
  function() {
    proxyRequest(req, res);
  });
});

app.get('/:nodeName/pcap/:id.pcap', function(req, res) {
  noCache(req, res);

  res.setHeader("Content-Type", "application/vnd.tcpdump.pcap");
  res.statusCode = 200;

  isLocalView(req.params.nodeName, function () {
    writePcap(res, req.params.id, {writeHeader: !req.query || !req.query.noHeader || req.query.noHeader !== "true"}, function () {
      res.end();
    });
  },
  function() {
    proxyRequest(req, res);
  });
});

app.get('/:nodeName/raw/:id.png', function(req, res) {
  noCache(req, res);

  res.setHeader("Content-Type", "image/png");

  var PNG_LINE_WIDTH = 256;

  isLocalView(req.params.nodeName, function () {
    if (!Png) {
      return res.send (emptyPNG);
    }

    processSessionIdAndDecode(req.params.id, 100, function(err, session, results) {
      if (err) {
        return res.send (emptyPNG);
      }
      var size = 0;
      var i;
      for (i = (req.query.type !== 'dst'?0:1); i < results.length; i+=2) {
        size += results[i].data.length + 2*PNG_LINE_WIDTH - (results[i].data.length % PNG_LINE_WIDTH);
      }
      var buffer = new Buffer(size);
      var pos = 0;
      if (size === 0) {
        return res.send (emptyPNG);
      }
      for (i = (req.query.type !== 'dst'?0:1); i < results.length; i+=2) {
        results[i].data.copy(buffer, pos);
        pos += results[i].data.length;
        var fillpos = pos;
        pos += 2*PNG_LINE_WIDTH - (results[i].data.length % PNG_LINE_WIDTH);
        buffer.fill(0xff, fillpos, pos);
      }

      var png = new Png(buffer, PNG_LINE_WIDTH, (size/PNG_LINE_WIDTH)-1, 'gray');
      var png_image = png.encodeSync();

      res.send(png_image);
    });
  },
  function() {
    proxyRequest(req, res);
  });
});

app.get('/:nodeName/raw/:id', function(req, res) {
  noCache(req, res);

  res.setHeader("Content-Type", "application/vnd.tcpdump.pcap");
  res.statusCode = 200;

  isLocalView(req.params.nodeName, function () {
    processSessionIdAndDecode(req.params.id, 10000, function(err, session, results) {
      if (err) {
        return res.send("Error");
      }
      for (var i = (req.query.type !== 'dst'?0:1); i < results.length; i+=2) {
        res.write(results[i].data);
      }
      res.end();
    });
  },
  function() {
    proxyRequest(req, res);
  });
});

app.get('/:nodeName/entirePcap/:id.pcap', function(req, res) {
  noCache(req, res);

  res.setHeader("Content-Type", "application/vnd.tcpdump.pcap");
  res.statusCode = 200;

  var options = {writeHeader: true};

  isLocalView(req.params.nodeName, function () {
    var query = { fields: ["ro"],
                  size: 1000,
                  query: {term: {ro: req.params.id}},
                  sort: { lp: { order: 'asc' } }
                };

    console.log(JSON.stringify(query));

    Db.searchPrimary('sessions*', 'session', query, function(err, data) {
      async.forEachSeries(data.hits.hits, function(item, nextCb) {
        writePcap(res, item._id, options, nextCb);
      }, function (err) {
        res.end();
      });
    });
  },
  function() {
    proxyRequest(req, res);
  });
});

function sessionsPcapList(req, res, list, pcapWriter, extension) {

  list = list.sort(function(a,b){return a.fields.lp - b.fields.lp;});

  var options = {writeHeader: true};

  async.eachLimit(list, 10, function(item, nextCb) {
    isLocalView(item.fields.no, function () {
      // Get from our DISK
      pcapWriter(res, item._id, options, nextCb);
    },
    function () {
      // Get from remote DISK
      getViewUrl(item.fields.no, function(err, viewUrl, agent) {
        var buffer = new Buffer(item.fields.pa*20 + item.fields.by);
        var bufpos = 0;
        var info = url.parse(viewUrl);
        info.path = Config.basePath(item.fields.no) + item.fields.no + "/" + extension + "/" + item._id + "." + extension;
        info.agent = (agent === httpAgent?foreverAgent:foreverAgentSSL);

        addAuth(info, req.user, item.fields.no);
        var preq = agent.request(info, function(pres) {
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
            async.setImmediate(nextCb);
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
  noCache(req, res);

  res.setHeader("Content-Type", "application/vnd.tcpdump.pcap");
  res.statusCode = 200;

  if (req.query.ids) {
    var ids = req.query.ids.split(",");

    sessionsListFromIds(req, ids, ["lp", "no", "by", "pa", "ro"], function(err, list) {
      sessionsPcapList(req, res, list, pcapWriter, extension);
    });
  } else {
    sessionsListFromQuery(req, ["lp", "no", "by", "pa", "ro"], function(err, list) {
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


function checkToken(req, res, next) {
  if (!req.body.token) {
    return res.send(JSON.stringify({success: false, text: "Missing token"}));
  }

  req.token = Config.auth2obj(req.body.token);
  if (Math.abs(Date.now() - req.token.date) > 600000 || req.token.pid !== process.pid || req.token.userId !== req.user.userId) {
    console.trace("bad token", req.token);
    return res.send(JSON.stringify({success: false, text: "Timeout - Please try reloading page and repeating the action"}));
  }

  return next();
}

app.post('/deleteUser/:userId', checkToken, function(req, res) {
  if (!req.user.createEnabled) {
    return res.send(JSON.stringify({success: false, text: "Need admin privileges"}));
  }

  if (req.params.userId === req.user.userId) {
    return res.send(JSON.stringify({success: false, text: "Can not delete yourself"}));
  }

  Db.deleteDocument('users', 'user', req.params.userId, function(err, data) {
    return res.send(JSON.stringify({success: true, text: "User deleted"}));
  });
});

app.post('/addUser', checkToken, function(req, res) {
  if (!req.user.createEnabled) {
    return res.send(JSON.stringify({success: false, text: "Need admin privileges"}));
  }

  if (req.body.userId.match(/[^\w.-]/)) {
    return res.send(JSON.stringify({success: false, text: "User id must be word characters"}));
  }

  if (!req.body || !req.body.userId || !req.body.userName || !req.body.password) {
    return res.send(JSON.stringify({success: false, text: "Missing/Empty required fields"}));
  }

  Db.get("users", 'user', req.body.userId, function(err, user) {
    if (err || user.exists) {
      console.log("Adding duplicate user", err, user);
      return res.send(JSON.stringify({success: false, text: "User already exists"}));
    }

    var nuser = {
      userId: req.body.userId,
      userName: req.body.userName,
      expression: req.body.expression,
      passStore: Config.pass2store(req.body.userId, req.body.password),
      enabled: req.body.enabled  === "on",
      webEnabled: req.body.webEnabled  === "on",
      emailSearch: req.body.emailSearch  === "on",
      headerAuthEnabled: req.body.headerAuthEnabled === "on",
      createEnabled: req.body.createEnabled === "on"
    };

    console.log("Creating new user", nuser);
    Db.indexNow("users", "user", req.body.userId, nuser, function(err, info) {
      console.log("add user", err, info);
      if (!err) {
        return res.send(JSON.stringify({success: true}));
      } else {
        return res.send(JSON.stringify({success: false, text: err}));
      }
    });
  });
});

app.post('/updateUser/:userId', checkToken, function(req, res) {
  if (!req.user.createEnabled) {
    return res.send(JSON.stringify({success: false, text: "Need admin privileges"}));
  }

  Db.get("users", 'user', req.params.userId, function(err, user) {
    if (err || !user.exists) {
      console.log("update user failed", err, user);
      return res.send(JSON.stringify({success: false, text: "User not found"}));
    }
    user = user._source;

    if (req.query.enabled) {
      user.enabled = req.query.enabled === "true";
    }

    if (req.query.expression.match(/^\s*$/)) {
      delete user.expression;
    } else {
      user.expression = req.query.expression;
    }

    if (req.query.userName.match(/^\s*$/)) {
      console.log("empty username");
      return res.send(JSON.stringify({success: false, text: "Username can not be empty"}));
    } else {
      user.userName = req.query.userName;
    }

    if (req.query.webEnabled) {
      user.webEnabled = req.query.webEnabled === "true";
    }

    if (req.query.emailSearch) {
      user.emailSearch = req.query.emailSearch === "true";
    }

    if (req.query.headerAuthEnabled) {
      user.headerAuthEnabled = req.query.headerAuthEnabled === "true";
    }

    if (req.query.removeEnabled) {
      user.removeEnabled = req.query.removeEnabled === "true";
    }

    // Can only change createEnabled if it is currently turned on
    if (req.user.createEnabled && req.query.createEnabled) {
      user.createEnabled = req.query.createEnabled === "true";
    }

    //console.log(user);

    Db.indexNow("users", "user", req.params.userId, user, function(err, info) {
      return res.send(JSON.stringify({success: true}));
    });
  });
});

app.post('/changePassword', checkToken, function(req, res) {
  function error(text) {
    return res.send(JSON.stringify({success: false, text: text}));
  }

  if (Config.get("disableChangePassword", false)) {
    return error("Disabled");
  }

  if (!req.body.newPassword || req.body.newPassword.length < 3) {
    return error("New password needs to be at least 3 characters");
  }

  if (req.token.cp && (req.user.passStore !== Config.pass2store(req.token.suserId, req.body.currentPassword) ||
                   req.token.suserId !== req.user.userId)) {
    return error("Current password mismatch");
  }

  Db.get("users", 'user', req.token.suserId, function(err, user) {
    if (err || !user.exists) {
      console.log("changePassword failed", err, user);
      return error("Unknown user");
    }
    user = user._source;
    user.passStore = Config.pass2store(user.userId, req.body.newPassword);
    Db.indexNow("users", "user", user.userId, user, function(err, info) {
      if (err) {
        console.log(err, info);
        return error("Update failed");
      }
      return res.send(JSON.stringify({success: true, text: "Changed password successfully"}));
    });
  });
});

app.post('/changeSettings', checkToken, function(req, res) {
  function error(text) {
    return res.send(JSON.stringify({success: false, text: text}));
  }

  Db.get("users", 'user', req.token.suserId, function(err, user) {
    if (err || !user.exists) {
      console.log("changeSettings failed", err, user);
      return error("Unknown user");
    }

    user = user._source;
    user.settings = req.body;
    delete user.settings.token;

    Db.indexNow("users", "user", user.userId, user, function(err, info) {
      if (err) {
        console.log(err, info);
        return error("Change settings update failed");
      }
      return res.send(JSON.stringify({success: true, text: "Changed password successfully"}));
    });
  });
});

app.post('/updateView', checkToken, function(req, res) {
  function error(text) {
    return res.send(JSON.stringify({success: false, text: text}));
  }

  if (!req.body.viewName || !req.body.viewExpression) {
    return error("Missing viewName or viewExpression");
  }

  Db.get("users", 'user', req.token.suserId, function(err, user) {
    if (err || !user.exists) {
      console.log("updateView failed", err, user);
      return error("Unknown user");
    }

    user = user._source;
    user.views = user.views || {};
    req.body.viewName = req.body.viewName.replace(/[^-a-zA-Z0-9_: ]/g, "");
    if (user.views[req.body.viewName]) {
      user.views[req.body.viewName].expression = req.body.viewExpression;
    } else {
      user.views[req.body.viewName] = {expression: req.body.viewExpression};
    }
    
    Db.indexNow("users", "user", user.userId, user, function(err, info) {
      if (err) {
        console.log(err, info);
        return error("Create View update failed");
      }
      return res.send(JSON.stringify({success: true, text: "Updated view successfully"}));
    });
  });
});

app.post('/deleteView', checkToken, function(req, res) {
  function error(text) {
    return res.send(JSON.stringify({success: false, text: text}));
  }

  if (!req.body.view) {
    return error("Missing view");
  }

  Db.get("users", 'user', req.token.suserId, function(err, user) {
    if (err || !user.exists) {
      console.log("updateView failed", err, user);
      return error("Unknown user");
    }

    user = user._source;
    user.views = user.views || {};
    delete user.views[req.body.view];

    Db.indexNow("users", "user", user.userId, user, function(err, info) {
      if (err) {
        console.log(err, info);
        return error("Create View update failed");
      }
      return res.send(JSON.stringify({success: true, text: "Deleted view successfully"}));
    });
  });
});

//////////////////////////////////////////////////////////////////////////////////
//// Session Add/Remove Tags
//////////////////////////////////////////////////////////////////////////////////

function addTagsList(res, allTagIds, list) {
  async.eachLimit(list, 10, function(session, nextCb) {
    var tagIds = [];

    if (!session.fields || !session.fields.ta) {
      return nextCb(null);
    }

    // Find which tags need to be added to this session
    for (var i = 0; i < allTagIds.length; i++) {
      if (session.fields.ta.indexOf(allTagIds[i]) === -1) {
        tagIds.push(allTagIds[i]);
      }
    }

    // Do the ES update
    var document = {
      script: "ctx._source.ta += ta",
      params: {
        ta: tagIds
      }
    };
    Db.update('sessions-' + session._id.substr(0,session._id.indexOf('-')), 'session', session._id, document, function(err, data) {
      nextCb(null);
    });
  }, function (err) {
    return res.send(JSON.stringify({success: true, text: "Tags added successfully"}));
  });
}

function removeTagsList(res, allTagIds, list) {
  async.eachLimit(list, 10, function(session, nextCb) {
    var tagIds = [];

    if (!session.fields || !session.fields.ta) {
      return nextCb(null);
    }

    // Find which tags need to be added to this session
    for (var i = 0; i < allTagIds.length; i++) {
      if (session.fields.ta.indexOf(allTagIds[i]) !== -1) {
        tagIds.push(allTagIds[i]);
      }
    }

    // Do the ES update
    var document = {
      script: "ctx._source.ta.removeAll(ta)",
      params: {
        ta: tagIds
      }
    };
    Db.update('sessions-' + session._id.substr(0,session._id.indexOf('-')), 'session', session._id, document, function(err, data) {
      if (err) {
        console.log(err);
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
      var ids = req.body.ids.split(",");

      sessionsListFromIds(req, ids, ["ta"], function(err, list) {
        addTagsList(res, tagIds, list);
      });
    } else {
      sessionsListFromQuery(req, ["ta"], function(err, list) {
        addTagsList(res, tagIds, list);
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
      var ids = req.body.ids.split(",");

      sessionsListFromIds(req, ids, ["ta"], function(err, list) {
        removeTagsList(res, tagIds, list);
      });
    } else {
      sessionsListFromQuery(req, ["ta"], function(err, list) {
        removeTagsList(res, tagIds, list);
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

  Db.getWithOptions('sessions-' + id.substr(0,id.indexOf('-')), 'session', id, {fields: "no,pr,ps"}, function(err, session) {
    var fields = session.fields;

    /* Old Format: Every item in array had file num (top 28 bits) and file pos (lower 36 bits)
     * New Format: Negative numbers are file numbers until next neg number, otherwise file pos */
    var newFormat = false;
    var fileNum;
    var itemPos = 0;
    async.eachLimit(fields.ps, 10, function(item, nextCb) {
      var pos;

      if (item < 0) {
        newFormat = true;
        fileNum = item * -1;
        return nextCb(null);
      } else if (newFormat) {
        pos  = item;
      } else  {
        // javascript doesn't have 64bit bitwise operations
        fileNum = Math.floor(item / 0xfffffffff);
        pos  = item % 0x1000000000;
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
            ipcap.openReadWrite(file.name);
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
        Db.deleteDocument('sessions-' + session._id.substr(0,session._id.indexOf('-')), 'session', session._id, function(err, data) {
          endCb(pcapErr, fields);
        });
      } else {
        // Do the ES update
        var document = {
          script: "ctx._source.scrubat = at; ctx._source.scrubby = by",
          params: {
            by: req.user.userId || "-",
            at: new Date().getTime()
          }
        };
        Db.update('sessions-' + session._id.substr(0,session._id.indexOf('-')), 'session', session._id, document, function(err, data) {
          endCb(pcapErr, fields);
        });
      }
    });
  });
}

app.get('/:nodeName/scrub/:id', function(req, res) {
  if (!req.user.removeEnabled) {
    return res.send(JSON.stringify({success: false, text: "Need remove data privileges"}));
  }

  noCache(req, res);
  res.statusCode = 200;

  isLocalView(req.params.nodeName, function () {
    pcapScrub(req, res, req.params.id, false, function(err) {
      res.end();
    });
  },
  function() {
    proxyRequest(req, res);
  });
});

app.get('/:nodeName/delete/:id', function(req, res) {
  if (!req.user.removeEnabled) {
    return res.send(JSON.stringify({success: false, text: "Need remove data privileges"}));
  }

  noCache(req, res);
  res.statusCode = 200;

  isLocalView(req.params.nodeName, function () {
    pcapScrub(req, res, req.params.id, true, function(err) {
      res.end();
    });
  },
  function() {
    proxyRequest(req, res);
  });
});


function scrubList(req, res, entire, list) {
  if (!list) {
    return res.end(JSON.stringify({success: false, text: "Missing list of sessions"}));
  }

  async.eachLimit(list, 10, function(item, nextCb) {
    isLocalView(item.fields.no, function () {
      // Get from our DISK
      pcapScrub(req, res, item._id, entire, nextCb);
    },
    function () {
      // Get from remote DISK
      getViewUrl(item.fields.no, function(err, viewUrl, agent) {
        var info = url.parse(viewUrl);
        info.path = Config.basePath(item.fields.no) + item.fields.no + (entire?"/delete/":"/scrub/") + item._id;
        info.agent = (agent === httpAgent?foreverAgent:foreverAgentSSL);
        addAuth(info, req.user, item.fields.no);
        var preq = agent.request(info, function(pres) {
          pres.on('end', function () {
            async.setImmediate(nextCb);
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
    var ids = req.body.ids.split(",");

    sessionsListFromIds(req, ids, ["no"], function(err, list) {
      scrubList(req, res, false, list);
    });
  } else {
    sessionsListFromQuery(req, ["no"], function(err, list) {
      scrubList(req, res, false, list);
    });
  }
});

app.post('/delete', function(req, res) {
  if (!req.user.removeEnabled) {
    return res.send(JSON.stringify({success: false, text: "Need remove data privileges"}));
  }

  if (req.body.ids) {
    var ids = req.body.ids.split(",");

    sessionsListFromIds(req, ids, ["no"], function(err, list) {
      scrubList(req, res, true, list);
    });
  } else {
    sessionsListFromQuery(req, ["no"], function(err, list) {
      scrubList(req, res, true, list);
    });
  }
});

//////////////////////////////////////////////////////////////////////////////////
//// Sending/Receive sessions
//////////////////////////////////////////////////////////////////////////////////
function sendSession(req, res, id, nextCb) {
  var packetslen = 0;
  var packets = [];
  var packetshdr;
  var ps = [-1];
  var tags = [];

  if (!req.query.saveId) {
    return res.end(JSON.stringify({success: false, text: "Missing saveId"}));
  }

  processSessionId(id, true, function(pcap, header) {
    packetshdr = header;
  }, function (pcap, packet, cb, i) {
    packetslen += packet.length;
    packets[i] = packet;
    cb(null);
  }, function (err, session) {
    var buffer = new Buffer(packetshdr.length + packetslen);
    var pos = 0;
    packetshdr.copy(buffer);
    pos += packetshdr.length;
    for(var i = 0; i < packets.length; i++) {
      ps.push(pos);
      packets[i].copy(buffer, pos);
      pos += packets[i].length;
    }
    session.id = id;
    session.ps = ps;
    delete session.fs;

    if (req.query.tags) {
      tags = req.query.tags.replace(/[^-a-zA-Z0-9_:,]/g, "").split(",");
      if (!session.ta) {
        session.ta = [];
      }
      session.ta = session.ta.concat(tags);
    }

    var sobj = Config.getObj("sendSession");
    if (!sobj) {
      console.log("ERROR - sendSession is not configured");
      return nextCb();
    }

    var info = url.parse(sobj.url + "/receiveSession?saveId=" + req.query.saveId);
    addAuth(info, req.user, req.params.nodeName, sobj.passwordSecret);
    info.method = "POST";

    var result = "";
    var agent = info.protocol === "https:"?httpsAgent:httpAgent;
    var preq = agent.request(info, function(pres) {
      pres.on('data', function (chunk) {
        result += chunk;
      });
      pres.on('end', function () {
        result = JSON.parse(result);
        if (!result.success) {
          console.log("ERROR sending session ", result);
        }
        nextCb();
      });
    });

    preq.on('error', function (e) {
      console.log("ERROR - Couldn't connect to ", info, "\nerror=", e);
    });

    var sessionStr = JSON.stringify(session);
    var b = new Buffer(8);
    b.writeUInt32BE(sessionStr.length, 0);
    b.writeUInt32BE(buffer.length, 4);
    preq.write(b);
    preq.write(sessionStr);
    preq.write(buffer);
    preq.end();
  }, undefined, 10);
}

app.get('/:nodeName/sendSession/:id', function(req, res) {
  noCache(req, res);
  res.statusCode = 200;

  isLocalView(req.params.nodeName, function () {
    sendSession(req, res, req.params.id, function(err) {
      res.end();
    });
  },
  function() {
    proxyRequest(req, res);
  });
});


function sendSessionsList(req, res, list) {
  if (!list) {
    return res.end(JSON.stringify({success: false, text: "Missing list of sessions"}));
  }

  req.query.saveId = Config.nodeName() + "-" + new Date().getTime().toString(36);

  async.eachLimit(list, 10, function(item, nextCb) {
    isLocalView(item.fields.no, function () {
      // Get from our DISK
      sendSession(req, res, item._id, nextCb);
    },
    function () {
      // Get from remote DISK
      getViewUrl(item.fields.no, function(err, viewUrl, agent) {
        var info = url.parse(viewUrl);
        info.path = Config.basePath(item.fields.no) + item.fields.no + "/sendSession/" + item._id + "?saveId=" + req.query.saveId;
        info.agent = (agent === httpAgent?foreverAgent:foreverAgentSSL);
        if (req.query.tags) {
          info.path += "&tags=" + req.query.tags;
        }
        addAuth(info, req.user, item.fields.no);
        var preq = agent.request(info, function(pres) {
          pres.on('data', function (chunk) {
          });
          pres.on('end', function () {
            async.setImmediate(nextCb);
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

    if (saveId.inProgress) {
      return setTimeout(makeFilename, 100, cb);
    }

    saveId.inProgress = 1;
    Db.getSequenceNumber("fn-" + Config.nodeName(), function (err, seq) {
      saveId.filename = Config.get("pcapDir") + "/" + Config.nodeName() + "-" + seq + "-" + req.query.saveId + ".pcap";
      saveId.seq      = seq;
      Db.indexNow("files", "file", Config.nodeName() + "-" + saveId.seq, {num: saveId.seq, name: saveId.filename, first: session.fp, node: Config.nodeName(), filesize: -1, locked: 1}, function() {
        cb(saveId.filename);
      });
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
    if (written === filelen) {
      file.end();
    }
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
    if (sessionlen === -1 && (buffer.length >= 8)) {
      sessionlen = buffer.readUInt32BE(0);
      filelen    = buffer.readUInt32BE(4);
      buffer = buffer.slice(8);
    }

    // If we know the session len and haven't read the session
    if (sessionlen !== -1 && !session && buffer.length >= sessionlen) {
      session = JSON.parse(buffer.toString("utf8", 0, sessionlen));
      buffer = buffer.slice(sessionlen);

      req.pause();

      makeFilename(function (filename) {
        req.resume();
        session.ps[0] = - saveId.seq;
        session.fs = [saveId.seq];
        session.no = Config.nodeName();

        if (saveId.start === 0) {
          file = fs.createWriteStream(filename, {flags: "w"});
        } else {
          file = fs.createWriteStream(filename, {start: saveId.start, flags: "r+"});
        }
        writeHeader = saveId.start === 0;

        // Adjust packet location based on where we start writing
        if (saveId.start > 0) {
          for (var p = 1; p < session.ps.length; p++) {
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
            Db.indexNow('sessions-' + id.substr(0,id.indexOf('-')), "session", id, session, function(err, info) {
            });
          }
        );
      });
    }
  });

  req.on('end', function(chunk) {
    return res.send({success: true});
  });
});

app.post('/sendSessions', function(req, res) {
  if (req.body.ids) {
    var ids = req.body.ids.split(",");

    sessionsListFromIds(req, ids, ["no"], function(err, list) {
      sendSessionsList(req, res, list);
    });
  } else {
    sessionsListFromQuery(req, ["no"], function(err, list) {
      sendSessionsList(req, res, list);
    });
  }
});

app.post('/upload', function(req, res) {
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
              .replace("{TMPFILE}", req.files.file.path)
              .replace("{CONFIG}", Config.getConfigFile());
  console.log("upload command: ", cmd);
  child = exec(cmd, function (error, stdout, stderr) {
    res.write("<b>" + cmd + "</b><br>");
    res.write("<pre>");
    res.write(stdout);
    res.end("</pre>");
    if (error !== null) {
      console.log('exec error: ' + error);
    }
    fs.unlink(req.files.file.path);
  });
});
//////////////////////////////////////////////////////////////////////////////////
//// Main
//////////////////////////////////////////////////////////////////////////////////
Db.checkVersion(MIN_DB_VERSION, Config.get("passwordSecret") !== undefined);
expireCheckAll();
setInterval(expireCheckAll, 5*60*1000);

var server;
if (Config.isHTTPS()) {
 server = httpsAgent.createServer({key: fs.readFileSync(Config.get("keyFile")),
                             cert: fs.readFileSync(Config.get("certFile"))}, app).listen(Config.get("viewPort", "8005"));

} else {
  server = httpAgent.createServer(app).listen(Config.get("viewPort", "8005"));
}

httpAgent.globalAgent.maxSockets = httpsAgent.globalAgent.maxSockets = httpsAgent.maxSockets = httpAgent.maxSockets = 200;

if (server.address() === null) {
  console.log("ERROR - couldn't listen on port", Config.get("viewPort", "8005"), "is viewer already running?");
  process.exit(1);
}
console.log("Express server listening on port %d in %s mode", server.address().port, app.settings.env);

