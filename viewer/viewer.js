/******************************************************************************/
/* viewer.js  -- The main moloch app
 *
 * Copyright 2012 The AOL Moloch Authors.  All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
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

//// Modules
//////////////////////////////////////////////////////////////////////////////////
var Config         = require('./config.js'),
    express        = require('express'),
    connect        = require('connect'),
    connectTimeout = require('connect-timeout'),
    request        = require('request'),
    stylus         = require('stylus'),
    util           = require('util'),
    fs             = require('fs-ext'),
    async          = require('async'),
    hexy           = require('hexy'),
    url            = require('url'),
    decode         = require('./decode.js'),
    Db             = require('./db.js'),
    os             = require('os'),
    zlib           = require('zlib'),
    molochparser   = require('./molochparser.js'),
    passport       = require('passport'),
    DigestStrategy = require('passport-http').DigestStrategy,
    HTTPParser     = process.binding('http_parser').HTTPParser,
    molochversion  = require('./version');

var app, httpAgent;
if (Config.isHTTPS()) {
  var httpsOptions = {
    key: fs.readFileSync(Config.get("keyFile")),
    cert: fs.readFileSync(Config.get("certFile"))
  };
  app = express.createServer(httpsOptions);
  httpAgent = require('https');
} else {
  app = express.createServer();
  httpAgent = require('http');
}

//////////////////////////////////////////////////////////////////////////////////
//// Config
//////////////////////////////////////////////////////////////////////////////////

passport.use(new DigestStrategy({qop: 'auth', realm: Config.getFull("default", "httpRealm", "Moloch")},
  function(userid, done) {
    Db.get("users", "user", userid, function(err, suser) {
      if (err) {return done(err);}
      if (!suser || !suser.exists) {console.log(userid, "doesn't exist"); return done(null, false);}
      if (!suser._source.enabled) {console.log(userid, "not enabled"); return done("Not enabled");}

      return done(null, suser._source, {ha1: Config.store2ha1(suser._source.passStore)});
    });
  },
  function (options, done) {
      //TODO:  Should check nonce here
      return done(null, true);
  }
));


app.configure(function() {
  app.enable("jsonp callback");
  app.set('views', __dirname + '/views');
  app.set('view engine', 'jade');
  app.set('view options', {molochversion: molochversion.version, isIndex: false});

  app.use(express.favicon(__dirname + '/public/favicon.ico'));
  app.use(passport.initialize());
  app.use(express.bodyParser());
  app.use(connectTimeout({ time: 30*60*1000 }));
  app.use(express.logger({ format: ':date \x1b[1m:method\x1b[0m \x1b[33m:url\x1b[0m :res[content-length] bytes :response-time ms' }));
  app.use(express.methodOverride());
  app.use(stylus.middleware({
    src: __dirname + '/views',
    dest: __dirname + '/public'
  }));
  app.use(express['static'](__dirname + '/public', { maxAge: 60 * 1000}));
  if (Config.get("passwordSecret")) {
    app.use(function(req, res, next) {

      // No auth for stats
      if (req.url.indexOf("/stats.json") === 0 || req.url.indexOf("/dstats.json") === 0) {
        return next();
      }

      // S2S Auth
      if (req.headers['x-moloch-auth']) {
        var obj = Config.auth2obj(req.headers['x-moloch-auth']);
        if (obj.path !== req.url) {
          console.log("ERROR - mismatch url", obj.path, req.url);
          return res.end("Unauthorized, bad url");
        }
        if (Math.abs(Date.now() - obj.date) > 60000) { // Request has to be +- 60 seconds
          console.log("ERROR - Denying server to server, are clocks out of sync?");
          return res.end("Unauthorized from time");
        }

        Db.get("users", "user", obj.user, function(err, suser) {
          if (err) {return res.end("ERROR - " +  err);}
          if (!suser || !suser.exists) {return res.end(obj.user + " doesn't exist");}
          if (!suser._source.enabled) {return res.end(obj.user + " not enabled");}
          req.user = suser._source;
          return next();
        });
        return;
      }

      // Browser auth
      passport.authenticate('digest', {session: false})(req, res, function (err) {
        if (err) {
          res.end(JSON.stringify({success: false, text: err}));
          return;
        } else {
          return next();
        }
      });
    });
  } else {
    /* Shared password isn't set, who cares about auth */
    app.use(function(req, res, next) {
      req.user = {userId: "anonymous", enabled: true, createEnabled: false, webEnabled: true};
      next();
    });
  }
});


function isEmptyObject(object) { for(var i in object) { return false; } return true; }
//////////////////////////////////////////////////////////////////////////////////
//// DB
//////////////////////////////////////////////////////////////////////////////////


var escInfo = Config.get("elasticsearch", "localhost:9200").split(':');
Db.initialize({host : escInfo[0], port: escInfo[1]});

function deleteFile(node, id, path, cb) {
  fs.unlink(path, function() {
    Db.deleteDocument('files', 'file', id, function(err, data) {
      cb(null);
    });
  });
}

function isLocalView(node, yesCB, noCB) {
  Db.nodeStatsCache(node, function(err, stat) {
    if (err || stat.hostname !== os.hostname()) {
      noCB();
    } else {
      yesCB();
    }
  });
}

function dbCheck() {
  var index;

  ["stats", "dstats", "tags", "sequence", "files", "users"].forEach(function(index) {
    Db.status(index, function(err, status) {
      if (err || status.error) {
        console.log("ERROR - Issue with index '" + index + "' make sure db/init.sh <eshost> has been run", err, status);
        process.exit(1);
      }
    });
  });

  if (Config.get("passwordSecret")) {
    Db.numberOfDocuments("users", function(err, num) {
      if (num === 0) {
        console.log("WARNING - No users are defined, use node viewer/addUser.js to add one, or turn off auth by unsetting passwordSecret");
      }
    });
  }
}

//////////////////////////////////////////////////////////////////////////////////
//// Pages
//////////////////////////////////////////////////////////////////////////////////
app.get('/', function(req, res) {
  if (!req.user.webEnabled) {
    return res.end("Moloch Permision Denied");
  }
  res.render('index', {
    user: req.user,
    title: 'Home',
    isIndex: true
  });
});

app.get('/about', function(req, res) {
  if (!req.user.webEnabled) {
    return res.end("Moloch Permision Denied");
  }
  res.render('about', {
    user: req.user,
    title: 'About'
  });
});

app.get('/files', function(req, res) {
  if (!req.user.webEnabled) {
    return res.end("Moloch Permision Denied");
  }
  res.render('files', {
    user: req.user,
    title: 'Files'
  });
});

app.get('/users', function(req, res) {
  if (!req.user.webEnabled) {
    return res.end("Moloch Permision Denied");
  }
  res.render('users', {
    user: req.user,
    title: 'Users'
  });
});

app.get('/password', function(req, res) {
  if (!req.user.webEnabled) {
    return res.end("Moloch Permision Denied");
  }
  res.render('password', {
    user: req.user,
    title: 'Change Password'
  });
});

app.get('/stats', function(req, res) {
  if (!req.user.webEnabled) {
    return res.end("Moloch Permision Denied");
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
      nodes: nodes
    });
  });
});

app.get('/:nodeName/statsDetail', function(req, res) {
  if (!req.user.webEnabled) {
    return res.end("Moloch Permision Denied");
  }
  res.render('statsDetail', {
    user: req.user,
    layout: false,
    nodeName: req.params.nodeName
  });
});

//////////////////////////////////////////////////////////////////////////////////
//// EXPIRING
//////////////////////////////////////////////////////////////////////////////////
function statG(dir, func) {
  fs.statVFS(dir, function(err,stat) {
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
                query: { terms: {node: nodes}},
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
      fs.stat(pcapDir, function(err,stat) {
        cb(null, {node: node, stat: stat});
      });
    },
    function (err, allInfo) {
      // Now gow through all the local nodes and check them
      async.forEachSeries(allInfo, function (info, cb) {
        expireCheckOne(info, allInfo, cb);
      }, function (err) {
      });
    });
  });
}

//////////////////////////////////////////////////////////////////////////////////
//// APIs
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
    obj[info["mDataProp_" + info["iSortCol_" + i]]] = {order: info["sSortDir_" + i]};
    query.sort.push(obj);
  }
}

function noCache(req, res) {
  res.header('Cache-Control', 'no-cache, private, no-store, must-revalidate, max-stale=0, post-check=0, pre-check=0');
}

app.get('/stats.json', function(req, res) {
  noCache(req, res);

  var columns = ["", "_id", "currentTime", "totalPackets", "totalK", "totalSessions", "monitoring", "freeSpaceM", "deltaPackets", "deltaBytes", "deltaSessions", "deltaDropped", "deltaMS"];
  var limit = (req.query.iDisplayLength?Math.min(parseInt(req.query.iDisplayLength, 10),10000):500);

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
  console.log("stats query", JSON.stringify(query));

  async.parallel({
    stats: function (cb) {
      Db.search('stats', 'stat', query, function(err, result) {
        var i;
        if (err || result.error) {
          res.send({total: 0, results: []});
        } else {
          var results = {total: result.hits.total, results: []};
          for (i = 0; i < result.hits.hits.length; i++) {
            result.hits.hits[i].fields.id = result.hits.hits[i]._id;
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
               }
              };

  Db.search('dstats', 'dstat', query, function(err, result) {
    var i;
    var data = [];
    data[query.size] = 0;
    var num = (req.query.stop - req.query.start)/req.query.step;

    for (i = 0; i < num; i++) {
      data[i] = 0;
    }

    var mult = 1;
    if (req.query.name === "freeSpaceM") {
      mult = 1000000;
    }

    if (req.query.interval === 5) {
      mult  = mult/5;
    }

    for (i = 0; i < result.hits.hits.length; i++) {
      var pos = Math.floor((result.hits.hits[i]._source.currentTime - req.query.start)/req.query.step);
      data[pos] = mult*result.hits.hits[i]._source[req.query.name];
    }
    res.send(data);
  });
});

app.get('/files.json', function(req, res) {
  noCache(req, res);

  var columns = ["num", "node", "name", "first", "size"];
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
          res.send({total: 0, results: []});
        } else {
          var results = {total: result.hits.total, results: []};
          for (i = 0; i < result.hits.hits.length; i++) {
            result.hits.hits[i].fields.id = result.hits.hits[i]._id;
            results.results.push(result.hits.hits[i].fields);
          }

          async.forEach(results.results, function (item, cb) {
            fs.stat(item.name, function (err, stats) {
              if (err || !stats) {
                item.size = -1;
              } else {
                item.size = stats.size/1000000.0;
              }
              cb(null);
            });
          }, function (err) {
            cb(null, results);
          });
        }
      });
    },
    total: function (cb) {
      Db.numberOfDocuments('files', cb);
    }
  },
  function(err, results) {
    var r = {sEcho: req.query.sEcho,
             iTotalRecords: results.total,
             iTotalDisplayRecords: results.files.total,
             aaData: results.files.results};
    res.send(r);
  });
});

app.post('/users.json', function(req, res) {
  var fields = ["userId", "userName", "expression", "enabled", "createEnabled", "webEnabled"];
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
            result.hits.hits[i].fields.expression = result.hits.hits[i].fields.expression || "";
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

function twoDigitString(value) {
  return (value < 10) ? ("0" + value) : value.toString();
}

function getIndices(startTime, stopTime, cb) {
  var indices = [];
  startTime = Math.floor(startTime/86400)*86400;
  Db.status("sessions-*", function(err, status) {

    if (err || status.error) {
    return cb("");
    }

    while (startTime < stopTime) {
      var d = new Date(startTime*1000);
      var iname = "sessions-" +
        twoDigitString(d.getUTCFullYear()%100) +
        twoDigitString(d.getUTCMonth()+1) +
        twoDigitString(d.getUTCDate());
      if (status.indices[iname]) {
        indices.push(iname);
      }
      startTime += 86400;
    }
    return cb(indices.join());
  });
}

/* async convert tag strings to numbers in an already built query */
function lookupQueryTags(query, doneCb) {
  var outstanding = 0;
  var finished = 0;

  function process(parent, obj, item) {
    if ((item === "ta" || item === "hh") && typeof obj[item] === "string") {
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
          delete parent.term;
          if (item === "ta") {
            parent.terms = {ta: terms};
          } else {
            parent.terms = {hh: terms};
          }
          outstanding--;
          if (finished && outstanding === 0) {
            doneCb();
          }
        });
      } else {
        outstanding++;
        var tag = (item === "hh"?"http:header:" + obj[item].toLowerCase():obj[item]);

        Db.tagNameToId(tag, function (id) {
          obj[item] = id;
          outstanding--;
          if (finished && outstanding === 0) {
            doneCb();
          }
        });
      }
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
    return doneCb();
  }

  finished = 1;
}

function buildSessionQuery(req, buildCb) {
  var columns = ["ro", "db", "fp", "lp", "a1", "p1", "a2", "p2", "pa", "by", "no", "us", "g1", "g2"];
  var limit = (req.query.iDisplayLength?Math.min(parseInt(req.query.iDisplayLength, 10),10000):100);
  var i;


  var query = {fields: columns,
               facets: {
                 dbHisto: {histogram : {key_field: "lp", value_field: "db", interval: 60, size:1440}},
                 paHisto: {histogram : {key_field: "lp", value_field: "pa", interval: 60, size:1440}},
                 map1: {terms : {field: "g1", size:1000}},
                 map2: {terms : {field: "g2", size:1000}}
               },
               from: req.query.iDisplayStart || 0,
               size: limit,
               query: {filtered: {query: {},
                                  filter: {}}}
              };

  
  if (req.query.date && req.query.date === '-1') {
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
    query.query.filtered.query.range = {lp: {gte: req.query.startTime, lte: req.query.stopTime}};
  } else {
    req.query.startTime = (Math.floor(Date.now() / 1000) - 60*60*parseInt(req.query.date, 10));
    req.query.stopTime = Date.now()/1000;
    query.query.filtered.query.range = {lp: {from: req.query.startTime}};
  }


  addSortToQuery(query, req.query, "fp");

  var err = null;
  if (req.query.expression) {
    try {
      query.query.filtered.filter = molochparser.parse(req.query.expression);
    } catch (e) {
      err = e;
    }
  }

  if (req.user.expression && req.user.expression.length > 0) {
    try {
      var userExpression = molochparser.parse(req.user.expression);
      if (isEmptyObject(query.query.filtered.filter)) {
        query.query.filtered.filter = userExpression;
      } else {
        query.query.filtered.filter = {and: [userExpression, query.query.filtered.filter]};
      }
    } catch (e) {
      console.log("ERR - User expression doesn't compile", req.user.expression, e);
    }
  }

  lookupQueryTags(query.query.filtered, function () {
    if (req.query.date && req.query.date === '-1') {
      return buildCb(err, query, "sessions*");
    }

    getIndices(req.query.startTime, req.query.stopTime, function(indices) {
      return buildCb(err, query, indices);
    });
  });
}

app.get('/sessions.json', function(req, res) {
  var map = {};
  var lpHisto = [];
  var dbHisto = [];
  var paHisto = [];
  var i;

  buildSessionQuery(req, function(bsqErr, query, indices) {
    if (bsqErr) {
      var r = {sEcho: req.query.sEcho,
               iTotalRecords: 0,
               iTotalDisplayRecords: 0,
               lpHisto: {entries: []},
               dbHisto: {entries: []},
               bsqErr: bsqErr.toString(),
               map: [],
               aaData:[]};
      res.send(r);
      return;
    }
    console.log("sessions.json query", JSON.stringify(query));

    async.parallel({
      sessions: function (sessionsCb) {
        Db.searchPrimary(indices, 'session', query, function(err, result) {
          //console.log("sessions query = ", util.inspect(result, false, 50));
          if (err || result.error) {
            console.log("sessions.json error", err);
            sessionsCb(null, {total: 0, results: []});
            return;
          }

          if (!result.facets) {
            result.facets = {map1: {terms: []}, map2: {terms: []}, dpHisto: {entries: []}, lpHisto: {entries: []}};
          }

          result.facets.dbHisto.entries.forEach(function (item) {
            lpHisto.push([item.key*1000, item.count]);
            dbHisto.push([item.key*1000, item.total]);
          });

          result.facets.paHisto.entries.forEach(function (item) {
            paHisto.push([item.key*1000, item.total]);
          });

          result.facets.map1.terms.forEach(function (item) {
            if (item.count < 0) {
              item.count = 0x7fffffff;
            }
            map[item.term] = item.count;
          });

          result.facets.map2.terms.forEach(function (item) {
            if (item.count < 0) {
              item.count = 0x7fffffff;
            }
            if (!map[item.term]) {
              map[item.term] = 0;
            }
            map[item.term] += item.count;
          });

          var results = {total: result.hits.total, results: []};
          for (i = 0; i < result.hits.hits.length; i++) {
            if (!result.hits.hits[i] || !result.hits.hits[i].fields) {
              continue;
            }
            result.hits.hits[i].fields.index = result.hits.hits[i]._index;
            result.hits.hits[i].fields.id = result.hits.hits[i]._id;
            if (!result.hits.hits[i].fields.us) {
              result.hits.hits[i].fields.us = [];
            }
            results.results.push(result.hits.hits[i].fields);
          }
          sessionsCb(null, results);
        });
      },
      total: function (totalCb) {
        Db.numberOfDocuments('sessions-*', totalCb);
      }
    },
    function(err, results) {
      console.log("total = ", results.total, "display total = ", (results.sessions?results.sessions.total:0));
      var r = {sEcho: req.query.sEcho,
               iTotalRecords: results.total,
               iTotalDisplayRecords: (results.sessions?results.sessions.total:0),
               lpHisto: lpHisto,
               dbHisto: dbHisto,
               paHisto: paHisto,
               map: map,
               aaData: (results.sessions?results.sessions.results:[])};
      try {
        res.send(r);
      } catch (c) {
      }
    });
  });
});

app.get('/uniqueValue.json', function(req, res) {
  noCache(req, res);
  var query;

  if (req.query.type === "tags") {
    query = {bool: {must: {wildcard: {_id: req.query.filter + "*"}},
                  must_not: {wildcard: {_id: "http:header:*"}}
                     }
          };
  } else if (req.query.type === "header") {
    query = {wildcard: {_id: "http:header:" + req.query.filter + "*"}};
  }

  console.log("uniqueValue query", JSON.stringify(query));
  Db.search('tags', 'tag', {size:200, query: query}, function(err, result) {
    var terms = [];
    if (req.query.type === "header") {
      result.hits.hits.forEach(function (hit) {
        terms.push(hit._id.substring(12));
      });
    } else {
      result.hits.hits.forEach(function (hit) {
        terms.push(hit._id);
      });
    }
    res.send(terms);
  });
});

app.get('/unique.txt', function(req, res) {
  noCache(req, res);
  var doCounts = parseInt(req.query.counts, 10) || 0;
  var doIp =  (req.query.field === "a1" || req.query.field === "a2");

  buildSessionQuery(req, function(err, query, indices) {
    query.fields = [req.query.field];
    if (req.query.field === "us") {
      query.size = 200000;
      delete query.facets;
      if (isEmptyObject(query.query.filtered.filter)) {
        query.query.filtered.filter = {exists: {field: req.query.field}};
      } else {
        query.query.filtered.filter = {and: [{exists: {field: req.query.field}}, query.query.filtered.filter]};
      }
    } else {
      query.facets = {facets: { terms : {field : req.query.field, size: 100000}}};
      query.size = 0;
    }

    console.log("unique query", indices, JSON.stringify(query));

    Db.searchPrimary(indices, 'session', query, function(err, result) {
      //console.log("unique result", util.inspect(result, false, 100));
      if (req.query.field === "us") {
        var counts = {};
        result.hits.hits.forEach(function (item) {
          if (!item.fields || !item.fields.us) {
            return;
          }
          item.fields.us.forEach(function (url) {
            if (counts[url]) {
              counts[url]++;
            } else {
              counts[url] = 1;
            }
          });
        });

        for (var key in counts) {
          if (doCounts) {
            res.write(counts[key] + ", ");
          }
          res.write(key +"\n");
        }
        res.end();
        return;
      }

      result.facets.facets.terms.forEach(function (item) {
        if (doIp) {
          res.write(decode.inet_ntoa(item.term));
        } else {
          res.write(item.term);
        }

        if (doCounts) {
          res.write(", " + item.count);
        }
        res.write("\n");
      });
      res.end();
    });
  });
});

function processSessionId(id, headerCb, packetCb, endCb, maxPackets) {
  function processFile(fd, pos, nextCb) {
    var buffer = new Buffer(5000);
    fs.read(fd, buffer, 0, 16, pos, function (err, bytesRead, buffer) {
      if (bytesRead !== 16) {
        return packetCb(buffer.slice(0,0), nextCb);
      }
      var len = buffer.readInt32LE(8);
      fs.read(fd, buffer, 16, len, pos+16, function (err, bytesRead, buffer) {
        return packetCb(buffer.slice(0,16+len), nextCb);
      });
    });
  }

  Db.get('sessions-' + id.substr(0,6), 'session', id, function(err, session) {
    if (err || !session.exists) {
      endCb("Not Found", null);
      return;
    }
    var lastFile = -1;
    var fileHandle = null;

    if (maxPackets && session._source.ps.length > maxPackets) {
      session._source.ps.length = maxPackets;
    }

    async.forEachSeries(session._source.ps, function(item, nextCb) {
      // javascript doesn't have 64bit bitwise operations
      var file = Math.floor(item / 0xfffffffff);
      var pos  = item % 0x1000000000;

      if (file !== lastFile) {
        if (lastFile !== -1) {
          if (fileHandle) {
            fs.close(fileHandle);
          }
        }

        Db.get('files', 'file', session._source.no + '-' + file, function (err, fresult) {
          if (err || !fresult._source) {
            console.log("ERROR - Not found", session._source.no + '-' + file, fresult);
            nextCb("ERROR - Not found", session._source.no + '-' + file);
            return;
          }
          fs.open(fresult._source.name, "r", function (err, fd) {
            if (lastFile === -1 && headerCb) {
              var hbuffer = new Buffer(24);
              fs.readSync(fd, hbuffer, 0, 24, 0);
              headerCb(hbuffer);
            }
            fileHandle = fd;
            processFile(fileHandle, pos, nextCb);
            lastFile = file;
          });
        });
      } else {
        processFile(fileHandle, pos, nextCb);
      }
    },
    function (err, results)
    {
      if (fileHandle) {
        fs.close(fileHandle);
      }

      async.parallel([
        function(parallelCb) {
          if (!session._source.ta) {
            session._source.ta = [];
            return parallelCb(null);
          }
          async.map(session._source.ta, function (item, cb) {
            Db.tagIdToName(item, function (name) {
              cb(null, name);
            });
          },
          function(err, results) {
            session._source.ta = results;
            parallelCb(null);
          });
        },
        function(parallelCb) {
          if (!session._source.hh) {
            return parallelCb(null);
          }
          async.map(session._source.hh, function (item, cb) {
            Db.tagIdToName(item, function (name) {
              cb(null, name.substring(12));
            });
          },
          function(err, results) {
            session._source.hh = results;
            parallelCb(null);
          });
        }],
        function(err, results) {
          endCb(null, session._source);
        }
      );
    });
  });
}

function localSessionDetailReturnFull(req, res, session, results) {
  var i;

  if (req.query.base === "hex") {
    var format = {};
    format.numbering = (req.query.line === "true"?"hex_bytes":"none");
    for (i = 0; i < results.length; i++) {
      results[i].data = '<pre>' + hexy.hexy(results[i].data, format).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;') + '</pre>';
    }
  } else if (req.query.base === "ascii") {
    for (i = 0; i < results.length; i++) {
      results[i].data = '<pre>' + results[i].data.toString("binary").replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;') + '</pre>';
    }
  } else if (req.query.base === "utf8") {
    for (i = 0; i < results.length; i++) {
      results[i].data = '<pre>' + results[i].data.toString("utf8").replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;') + '</pre>';
    }
  } else {
    for (i = 0; i < results.length; i++) {
      results[i].data = results[i].data.toString().replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/\n/g, '<br>');
    }
  }

  res.render('sessionDetail', {
    user: req.user,
    layout: false,
    session: session,
    data: results,
    query: req.query
  });
}

function localSessionDetailReturn(req, res, session, results) {
  if (results.length > 200) {
    results.length = 200;
  }

  if (req.query.gzip === "true") {
    var parsers = [new HTTPParser(HTTPParser.REQUEST), new HTTPParser(HTTPParser.RESPONSE)];

    parsers[0].onBody = parsers[1].onBody = function(buf, start, len) {
      var pos = this.pos;
      //console.log(pos, "onBody slice (", start, start+len, ") gzip:", this.gzip);

      // This isn't a gziped request
      if (!this.gzip) {
        return;
      }

      // Copy over the headers
      if (!results[pos]) {
        results[pos] = {data: buf.slice(0, start)};
      }

      if (!this.inflator) {
        this.inflator = zlib.createGunzip()
          .on("data", function (b) {
            var tmp = Buffer.concat([results[pos].data, new Buffer(b)]);
            results[pos].data = tmp;
          })
          .on("error", function (e) {
          })
          .on("end", function () {
          });
      }

      this.inflator.write(buf.slice(start,start+len));
    };

    parsers[0].onMessageComplete = parsers[1].onMessageComplete = function() {
      var pos = this.pos;

      //console.log("onMessageComplete", pos, this.gzip);
      var nextCb = this.nextCb;
      if (this.inflator) {
        this.inflator.end(null, function () {
          if (nextCb) {
            process.nextTick(nextCb);
          }
        });
        this.inflator = null;
      } else {
        results[pos] = origresults[pos];
        if (nextCb) {
          process.nextTick(nextCb);
        }
      }
    };

    parsers[0].onHeadersComplete = parsers[1].onHeadersComplete = function(info) {
      var h;
      this.gzip = false;
      for (h = 0; h < info.headers.length; h += 2) {
        if (info.headers[h].match(/Content-Encoding/i)) {
          if (info.headers[h+1].match(/gzip/i)) {
            this.gzip = true;
          }
          break;
        }
      }
    };

    var origresults = results;
    results = [];

    var p = 0;
    async.forEachSeries(origresults, function(item, nextCb) {
      var pos = p;
      p++;
      parsers[(pos%2)].pos = pos;

      if (!item) {
      } else if (item.data.length === 0) {
        results[pos] = {data: item.data};
        process.nextTick(nextCb);
      } else {
        //console.log("Doing", pos/*, item.data.toString()*/);
        //parsers[(pos%2)].nextCb = nextCb;
        var out = parsers[(pos%2)].execute(item.data, 0, item.data.length);
        process.nextTick(nextCb);
      }
    }, function (err) {
      /*parsers[0].nextCb = null;
      parsers[1].nextCb = null;
      parsers[0].finish();
      parsers[1].finish();*/
      process.nextTick(function() {localSessionDetailReturnFull(req, res, session, results);});
    });
  } else {
    localSessionDetailReturnFull(req, res, session, results);
  }
}


function localSessionDetail(req, res) {
  if (!req.query) {
    req.query = {gzip: false, line: false, base: "natural"};
  }

  req.query.gzip = req.query.gzip || false;
  req.query.line = req.query.line || false;
  req.query.base = req.query.base || "hex";

  var packets = [];
  processSessionId(req.params.id, null, function (buffer, cb) {
    var obj = {};
    if (buffer.length > 16) {
      decode.pcap(buffer, obj);
    } else {
      obj = {ip: {p: "Empty"}};
    }
    packets.push(obj);
    cb(null);
  },
  function(err, session) {
    if (err) {
      res.end("Error");
      return;
    }
    session.id = req.params.id;
    session.ta = session.ta.sort();
    if (session.hh) {
      session.hh = session.hh.sort();
    }
    //console.log("session", util.inspect(session, false, 15));
    /* Now reassembly the packets */
    if (packets.length === 0) {
      localSessionDetailReturn(req, res, session, [{data: "No pcap data found"}]);
    } else if (packets[0].ip.p === 1) {
      decode.reassemble_icmp(packets, function(err, results) {
        localSessionDetailReturn(req, res, session, results);
      });
    } else if (packets[0].ip.p === 6) {
      decode.reassemble_tcp(packets, function(err, results) {
        localSessionDetailReturn(req, res, session, results);
      });
    } else if (packets[0].ip.p === 17) {
      decode.reassemble_udp(packets, function(err, results) {
        localSessionDetailReturn(req, res, session, results);
      });
    } else {
      localSessionDetailReturn(req, res, session, [{data: "Unknown ip.p=" + packets[0].ip.p}]);
    }
  },
  200);
}

function getViewUrl(node, cb) {
  var url = Config.getFull(node, "viewUrl");
  if (url) {
    cb(null, url);
    return;
  }

  Db.nodeStatsCache(node, function(err, stat) {
    if (Config.isHTTPS(node)) {
      cb(null, "https://" + stat.hostname + ":" + Config.getFull(node, "viewPort", "8005"));
    } else {
      cb(null, "http://" + stat.hostname + ":" + Config.getFull(node, "viewPort", "8005"));
    }
  });
}

function addAuth(info, user, node) {
    if (!info.headers) {
        info.headers = {};
    }
    info.headers['x-moloch-auth'] = Config.obj2auth({date: Date.now(),
                                                     user: user.userId,
                                                     node: node,
                                                     path: info.path
                                                    });
}

function proxyRequest (req, res) {
  noCache(req, res);

  getViewUrl(req.params.nodeName, function(err, viewUrl) {
    if (err) {
      console.log(err);
      res.end("Check logs on " + os.hostname());
    }
    var info = url.parse(viewUrl);
    info.path = req.url;
    info.rejectUnauthorized = true;
    addAuth(info, req.user, req.params.nodeName);

    var preq = httpAgent.request(info, function(pres) {
      pres.on('data', function (chunk) {
        res.write(chunk);
      });
      pres.on('end', function () {
        res.end();
      });
    });

    preq.on('error', function (e) {
      console.log("error = ", e);
      res.end("Unknown error, check logs on " + os.hostname());
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

function writePcap(res, id, writeHeader, doneCb) {
  var b = new Buffer(100000);
  var boffset = 0;

  processSessionId(id, function (buffer) {
    if (writeHeader) {
      res.write(buffer);
      writeHeader = 0;
    }
  },
  function (buffer, cb) {
    if (boffset + buffer.length > b.length) {
      res.write(b.slice(0, boffset));
      boffset = 0;
    }
    buffer.copy(b, boffset, 0, buffer.length);
    boffset += buffer.length;
    cb(null);
  },
  function(err, session) {
    if (err) {
      console.log("writePcap", err);
    }
    res.write(b.slice(0, boffset));
    doneCb(err, writeHeader);
  });
}

app.get('/:nodeName/pcap/:id.pcap', function(req, res) {
  noCache(req, res);

  res.setHeader("Content-Type", "application/vnd.tcpdump.pcap");
  res.statusCode = 200;

  isLocalView(req.params.nodeName, function () {
    writePcap(res, req.params.id, !req.query || !req.query.noHeader || req.query.noHeader !== "true", function () {
      res.end();
    });
  },
  function() {
    proxyRequest(req, res);
  });
});

app.get('/:nodeName/entirePcap/:id.pcap', function(req, res) {
  noCache(req, res);

  isLocalView(req.params.nodeName, function () {
    var query = { fields: ["ro"],
                  size: 1000,
                  query: {term: {ro: req.params.id}},
                  sort: { lp: { order: 'asc' } }
                };

    console.log(JSON.stringify(query));

    res.setHeader("Content-Type", "application/vnd.tcpdump.pcap");
    res.statusCode = 200;
    
    Db.searchPrimary('sessions*', 'session', query, function(err, data) {
      var firstHeader = 1;

      async.forEachSeries(data.hits.hits, function(item, nextCb) {
        writePcap(res, item._id, firstHeader, function (err, stillNeedWriteHeader) {
          firstHeader = stillNeedWriteHeader;
          nextCb(err);
        });
      }, function (err) {
        res.end();
      });
    });
  },
  function() {
    proxyRequest(req, res);
  });
});

app.get('/sessions.pcap', function(req, res) {
  noCache(req, res);

  res.setHeader("Content-Type", "application/vnd.tcpdump.pcap");
  res.statusCode = 200;

  buildSessionQuery(req, function(err, query, indices) {
    delete query.facets;
    query.fields = ["no"];
    Db.searchPrimary(indices, 'session', query, function(err, result) {
      var firstHeader = 1;

      async.forEachSeries(result.hits.hits, function(item, nextCb) {
        isLocalView(item.fields.no, function () {
          // Get from our DISK
          writePcap(res, item._id, firstHeader, function (err, stillNeedWriteHeader) {
            firstHeader = stillNeedWriteHeader;
            nextCb(err);
          });
        },
        function () {
          // Get from remote DISK
          getViewUrl(item.fields.no, function(err, viewUrl) {
            var info = url.parse(viewUrl);

            if (firstHeader) {
              info.path = '/' + item.fields.no + "/pcap/" + item._id + ".pcap";
            } else {
              info.path = '/' + item.fields.no + "/pcap/" + item._id + ".pcap?noHeader=true";
            }

            addAuth(info, req.user, item.fields.no);
            var preq = httpAgent.request(info, function(pres) {
              pres.on('data', function (chunk) {
                firstHeader = 0; // Don't reset until we actually get data
                res.write(chunk);
              });
              pres.on('end', function () {
                nextCb(null);
              });
            });
            preq.on('error', function (e) {
              console.log("error = ", e);
              nextCb(null);
            });
            preq.end();
          });
        });
      }, function(err) {
        res.end();
      });
    });
  });
});

app.post('/deleteUser/:userId', function(req, res) {
  if (!req.user.createEnabled) {
    return res.end(JSON.stringify({success: false, text: "Need admin privileges"}));
  }

  if (req.params.userId === req.user.userId) {
    return res.end("Can not delete yourself");
  }

  console.log("Deleting ", req.params.userId);
  Db.deleteDocument('users', 'user', req.params.userId, function(err, data) {
    res.end("Success!");
  });
});

app.post('/addUser', function(req, res) {
  if (!req.user.createEnabled) {
    return res.end(JSON.stringify({success: false, text: "Need admin privileges"}));
  }

  if (!req.body || !req.body.userId || !req.body.userName || !req.body.password) {
    return res.end(JSON.stringify({success: false, text: "Missing/Empty required fields"}));
  }

  Db.get("users", 'user', req.body.userId, function(err, user) {
    if (err || user.exists) {
      return res.end(JSON.stringify({success: false, text: "User already exists"}));
    }

    var nuser = {
      userId: req.body.userId,
      userName: req.body.userName,
      expression: req.body.expression,
      passStore: Config.pass2store(req.body.userId, req.body.password),
      enabled: (req.body.enabled || "false") === "true",
      webEnabled: (req.body.webEnabled || "true") === "true",
      createEnabled: (req.body.createEnabled || "false") === "true"
    };

    console.log("nuser", nuser);
    Db.indexNow("users", "user", req.body.userId, nuser, function(err, info) {
      console.log("add user", err, info);
      if (!err) {
        return res.end(JSON.stringify({success: true}));
      } else {
        return res.end(JSON.stringify({success: false, text: err}));
      }
    });
  });


});

app.post('/updateUser/:userId', function(req, res) {
  if (req.params.userId !== req.user.userId &&
      !req.user.createEnabled) {
    return res.end(JSON.stringify({success: false, text: "Need admin privileges"}));
  }

  Db.get("users", 'user', req.params.userId, function(err, user) {
    if (err || !user.exists) {
      return res.end(JSON.stringify({success: false, text: "User not found"}));
    }
    user = user._source;

    if (req.query.enabled) {
      user.enabled = req.query.enabled === "true";
    }

    if (req.query.webEnabled) {
      user.webEnabled = req.query.webEnabled === "true";
    }

    if (req.user.createEnabled && req.query.createEnabled) {
      user.createEnabled = req.query.createEnabled === "true";
    }

    Db.indexNow("users", "user", req.params.userId, user, function(err, info) {
      return res.end(JSON.stringify({success: true}));
    });
  });
});

app.post('/changePassword', function(req, res) {
  if (req.user.passStore !== Config.pass2store(req.user.userId, req.body.currentPassword)) {
    res.end(JSON.stringify({success: false, text: "Current password mismatch"}));
    return;
  }

  req.user.passStore = Config.pass2store(req.user.userId, req.body.newPassword);
  Db.indexNow("users", "user", req.user.userId, req.user, function(err, info) {
    if (!err) {
      res.end(JSON.stringify({success: false, text: err}));
      return;
    }

    res.end(JSON.stringify({success: true}));
  });
});
//////////////////////////////////////////////////////////////////////////////////
//// Main
//////////////////////////////////////////////////////////////////////////////////
dbCheck();
expireCheckAll();
setInterval(expireCheckAll, 5*60*1000);
app.listen(Config.get("viewPort", "8005"));
console.log("Express server listening on port %d in %s mode", app.address().port, app.settings.env);

