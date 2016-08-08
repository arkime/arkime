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

var fs             = require('fs')
  , unzip          = require('unzip')
  , wiseSource     = require('./wiseSource.js')
  , util           = require('util')
  , HashTable      = require('hashtable')
  , LRU            = require('lru-cache')
  , request        = require('request')
  , exec           = require('child_process').exec
  ;

var sqlite3;
//////////////////////////////////////////////////////////////////////////////////
function ThreatStreamSource (api, section) {
  ThreatStreamSource.super_.call(this, api, section);
  this.user    = api.getConfig("threatstream", "user");
  this.key     = api.getConfig("threatstream", "key");
  this.mode     = api.getConfig("threatstream", "mode", "zip");

  switch (this.mode) {
  case "api":
    this.inProgress = 0;
    break;
  case "zip":
    this.ips          = new HashTable();
    this.domains      = new HashTable();
    this.emails       = new HashTable();
    this.md5s         = new HashTable();
    this.cacheTimeout = -1;
    break;
  case "sqlite3":
    sqlite3           = require('sqlite3');
    this.cacheTimeout = -1;
    this.openDb();
    setInterval(this.openDb.bind(this), 60*1000);
    break;
  default:
    console.log("Unknown threatstream mode", this.mode);
    process.exit(0);
  }
}
util.inherits(ThreatStreamSource, wiseSource);
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.parseFile = function()
{
  var self = this;

  self.ips.clear();
  self.ips.reserve(2000000);
  self.domains.clear();
  self.domains.reserve(2000000);
  self.emails.clear();
  self.emails.reserve(2000000);
  self.md5s.clear();
  self.md5s.reserve(2000000);
  self.urls.clear();
  self.urls.reserve(200000);

  fs.createReadStream('/tmp/threatstream.zip')
    .pipe(unzip.Parse())
    .on('entry', function (entry) {
      var bufs = [];
      entry.on('data', function (buf) {
        bufs.push(buf);
      }).on('end', function () {
        var json = JSON.parse(Buffer.concat(bufs));
        json.objects.forEach(function (item) {
          if (item.state !== "active") {
            return;
          }

          var encoded;
          var num;
          try {
            if (item.maltype && item.maltype !== "null") {
              encoded = wiseSource.encode(self.severityField, item.severity.toLowerCase(),
                                          self.confidenceField, "" + item.confidence,
                                          self.idField, "" + item.id,
                                          self.typeField, item.itype.toLowerCase(),
                                          self.maltypeField, item.maltype.toLowerCase(),
                                          self.sourceField, item.source);
              num = 6;
            } else {
              encoded = wiseSource.encode(self.severityField, item.severity.toLowerCase(),
                                          self.confidenceField, "" + item.confidence,
                                          self.idField, "" + item.id,
                                          self.typeField, item.itype.toLowerCase(),
                                          self.sourceField, item.source);
              num = 5;
            }
          } catch (e) {
            console.log("ERROR -", entry.path, e, item, e.stack);
            return;
          }


          if (item.itype.match(/(_ip|anon_proxy|anon_vpn)/)) {
            self.ips.put(item.ip, {num: num, buffer: encoded});
          } else if (item.itype.match(/_domain|_dns/)) {
            self.domains.put(item.domain, {num: num, buffer: encoded});
          } else if (item.itype.match(/_email/)) {
            self.emails.put(item.email, {num: num, buffer: encoded});
          } else if (item.itype.match(/_md5/)) {
            self.md5s.put(item.md5, {num: num, buffer: encoded});
          } else if (item.itype.match(/_url/)) {
            self.urls.put(item.url, {num: num, buffer: encoded});
          }
        });
        //console.log("Threatstream - Done", entry.path);
      });
    })
    .on('close', function () {
      console.log("Threatstream - Done Loading");
    });
};
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.loadFile = function() {
  var self = this;

  console.log("Threatstream - Downloading files");
  wiseSource.request('https://api.threatstream.com/api/v1/intelligence/snapshot/download/?username=' + self.user + '&api_key=' + self.key,  '/tmp/threatstream.zip', function (statusCode) {
    if (statusCode === 200 || !self.loaded) {
      self.loaded = true;
      self.parseFile();
    }
  });
};
//////////////////////////////////////////////////////////////////////////////////
function getDomainZip(domain, cb) {
  var domains = this.domains;
  cb(null, domains.get(domain) || domains.get(domain.substring(domain.indexOf(".")+1)));
}
//////////////////////////////////////////////////////////////////////////////////
function getIpZip(ip, cb) {
  cb(null, this.ips.get(ip));
}
//////////////////////////////////////////////////////////////////////////////////
function getMd5Zip(md5, cb) {
  cb(null, this.md5s.get(md5));
}
//////////////////////////////////////////////////////////////////////////////////
function getEmailZip(email, cb) {
  cb(null, this.emails.get(email));
}
//////////////////////////////////////////////////////////////////////////////////
function dumpZip (res) {
  var self = this;
  ["ips", "domains", "emails", "md5s"].forEach(function (ckey) {
    res.write("" + ckey + ":\n");
    self[ckey].forEach(function(key, value) {
      var str = "{key: \"" + key + "\", ops:\n" +
        wiseSource.result2Str(wiseSource.combineResults([value])) + "},\n";
      res.write(str);
    });
  });
  res.end();
}
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.getApi = function(type, value, cb) {
  var self = this;

  var options = {
      url: "https://api.threatstream.com/api/v2/intelligence/?username=" + self.user + "&api_key=" + self.key + "&status=active&" + type + "=" + value + "&itype=" + self.types[type],
      method: 'GET',
      forever: true
  };

  if (self.inProgress > 50) {
    return cb ("dropped");
  }

  self.inProgress++;
  request(options, function(err, response, body) {
    self.inProgress--;
    if (err) {
      console.log("threatstream problem fetching ", options, err || response);
      return cb(null, wiseSource.emptyResult);
    }

    try {
      body = JSON.parse(body);
    } catch (e) {
      console.log("Couldn't parse", body);
      return cb(null, wiseSource.emptyResult);
    }

    if (body.objects.length === 0) {
      return cb(null, wiseSource.emptyResult);
    }

    var args = [];
    body.objects.forEach(function (item) {
      args.push(self.confidenceField, "" + item.confidence,
                self.idField, "" + item.id,
                self.typeField, item.itype.toLowerCase(),
                self.sourceField, item.source);

      if (item.maltype !== undefined && item.maltype !== "null") {
        args.push(self.maltypeField, item.maltype.toLowerCase());
      }
      if (item.severity !== undefined) {
        args.push(self.severityField, item.severity.toLowerCase());
      }
    });
    var result = {num: args.length/2, buffer: wiseSource.encode.apply(null, args)};
    return cb(null, result);
  });
};
//////////////////////////////////////////////////////////////////////////////////
function getDomainApi(domain, cb) {
  return this.getApi("domain", domain, cb);
}
//////////////////////////////////////////////////////////////////////////////////
function getIpApi(ip, cb) {
  return this.getApi("ip", ip, cb);
}
//////////////////////////////////////////////////////////////////////////////////
function getMd5Api(md5, cb) {
  return this.getApi("md5", md5, cb);
}
//////////////////////////////////////////////////////////////////////////////////
function getEmailApi(email, cb) {
  return this.getApi("email", email, cb);
}
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.getSqlite3 = function(type, field, value, cb) {
  var self = this;

  if (!this.db) {
    return cb("dropped");
  }

  this.db.all("SELECT * FROM ts WHERE " + field + " = ? AND itype IN (" + self.typesWithQuotes[type] + ")", value, function (err, data) {
    if (err) {
      console.log("ERROR", err, data);
      return cb("dropped");
    }
    if (data.length === 0) {
      return cb(null, wiseSource.emptyResult);
    }

    var args = [];
    data.forEach(function (item) {
      args.push(self.confidenceField, "" + item.confidence,
                self.idField, "" + item.id,
                self.typeField, item.itype.toLowerCase(),
                self.sourceField, item.source);

      if (item.maltype !== undefined && item.maltype !== null) {
        args.push(self.maltypeField, item.maltype.toLowerCase());
      }
      if (item.severity !== undefined) {
        args.push(self.severityField, item.severity.toLowerCase());
      }
    });
    var result = {num: args.length/2, buffer: wiseSource.encode.apply(null, args)};
    return cb(null, result);
  });
};
//////////////////////////////////////////////////////////////////////////////////
function getDomainSqlite3(domain, cb) {
  return this.getSqlite3("domain", "domain", domain, cb);
}
//////////////////////////////////////////////////////////////////////////////////
function getIpSqlite3(ip, cb) {
  return this.getSqlite3("ip", "srcip", ip, cb);
}
//////////////////////////////////////////////////////////////////////////////////
function getMd5Sqlite3(md5, cb) {
  return this.getSqlite3("md5", "md5", md5, cb);
}
//////////////////////////////////////////////////////////////////////////////////
function getEmailSqlite3(email, cb) {
  return this.getSqlite3("email", "email", email, cb);
}
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.loadTypes = function() {
  // Threatstream doesn't have a way to just ask for type matches, so we need to figure out which itypes are various types.
  var self = this;
  self.types = {};
  self.typesWithQuotes = {};
  request({url: "https://api.threatstream.com/api/v1/impact/?username="+self.user+"&api_key="+self.key+"&limit=1000", forever: true}, function(err, response, body) {
    if (err) {
      console.log("ERROR - Threatstream failed to load types", err);
      return;
    }

    body = JSON.parse(body);
    body.objects.forEach(function(item) {
      if (self.types[item.value_type] === undefined) {
        self.types[item.value_type] = [item.name];
      } else {
        self.types[item.value_type].push(item.name);
      }
    });
    for (var key in self.types) {
      self.typesWithQuotes[key] = self.types[key].map(function(v) {return "'" + v + "'";}).join(",");
      self.types[key] = self.types[key].join(",");
    }

    // Wait to register until request is done
    self.api.addSource("threatstream", self);
  });
};
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.openDb = function() {
  var self = this;
  var dbFile = self.api.getConfig("threatstream", "dbFile", "ts.db");

  var dbStat;
  try {dbStat = fs.statSync(dbFile);} catch (e) {};

  var realDb;
  if (!dbStat || !dbStat.isFile()) {
    console.log("ERROR - file doesn't exist", dbFile);
    process.exit();
  }

  // * Lock the real DB
  // * Copy the real db to .temp so that anything currently running still works
  // * Unlock the real DB and close
  // * create md5 index on .temp version
  // * Close .moloch db
  // * mv .temp to .moloch
  // * Open .moloch
  function beginImmediate(err) {
    console.log("Threatstream - Copying DB", dbStat.mtime);

    // Repeat until we lock the DB
    if (err && err.code === "SQLITE_BUSY") {
      console.log("Failed to lock sqlite DB", dbFile);
      return realDb.run("BEGIN IMMEDIATE", beginImmediate);
    }

    exec ("/bin/cp -f " + dbFile + " " + dbFile +".temp",  function(err, stdout, stderr) {
      console.log(stdout, stderr);
      realDb.run("END", function (err) {
        realDb.close();
        realDb = null;

        var tempDb = new sqlite3.Database(dbFile + ".temp");
        tempDb.run("CREATE INDEX md5_index ON ts (md5)", function (err) {
          tempDb.close();
          if (self.db) {
            self.db.close();
          }
          self.db = null;
          exec ("/bin/rm -f " + dbFile + ".moloch ",  function(err, stdout, stderr) {
            exec ("/bin/mv -f " + dbFile + ".temp " + dbFile + ".moloch",  function(err, stdout, stderr) {
              self.db = new sqlite3.Database(dbFile + ".moloch", sqlite3.OPEN_READONLY);
              console.log("Threatstream - Loaded DB");
            });
          });
        });
      });
    });
  }


  // If the last copy time doesn't match start the copy process.
  // This will also run on startup.
  if (self.mtime !== dbStat.mtime.getTime()) {
    self.mtime = dbStat.mtime.getTime();
    realDb = new sqlite3.Database(dbFile);
    realDb.run("BEGIN IMMEDIATE", beginImmediate);
  } else if (!self.db) {
    // Open the DB if not already opened.
    self.db = new sqlite3.Database(dbFile + ".moloch", sqlite3.OPEN_READONLY);
  }
};
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.init = function() {
  var self = this;
  if (this.user === undefined) {
    console.log("Threatstream - No user defined");
    return;
  }

  if (this.key === undefined) {
    console.log("Threatstream - No key defined");
    return;
  }

  this.severityField = this.api.addField("field:threatstream.severity;db:threatstream.severity-term;kind:lotermfield;friendly:Severity;help:Threatstream Severity;shortcut:0;count:true");
  this.confidenceField = this.api.addField("field:threatstream.confidence;db:threatstream.confidence;kind:integer;friendly:Confidence;help:Threatstream Confidence;shortcut:1;count:true");
  this.idField = this.api.addField("field:threatstream.id;db:threatstream.id;kind:integer;friendly:Id;help:Threatstream Id;shortcut:2;count:true");
  this.typeField = this.api.addField("field:threatstream.type;db:threatstream.type-term;kind:lotermfield;friendly:Type;help:Threatstream Type;shortcut:3;count:true");
  this.maltypeField = this.api.addField("field:threatstream.maltype;db:threatstream.maltype-term;kind:lotermfield;friendly:Malware Type;help:Threatstream Malware Type;shortcut:4;count:true");
  this.sourceField = this.api.addField("field:threatstream.source;db:threatstream.source-term;kind:termfield;friendly:Source;help:Threatstream Source;shortcut:5;count:true");

  this.api.addView("threatstream",
    "if (session.threatstream)\n" +
    "  div.sessionDetailMeta.bold Threatstream\n" +
    "  dl.sessionDetailMeta\n" +
    "    +arrayList(session.threatstream, 'severity-term', 'Severity', 'threatstream.severity')\n" +
    "    +arrayList(session.threatstream, 'confidence', 'Confidence', 'threatstream.confidence')\n" +
    "    +arrayList(session.threatstream, 'id', 'Id', 'threatstream.id')\n" +
    "    +arrayList(session.threatstream, 'type-term', 'Type', 'threatstream.type')\n" +
    "    +arrayList(session.threatstream, 'maltype-term', 'Malware Type', 'threatstream.maltype')\n" +
    "    +arrayList(session.threatstream, 'source-term', 'Source', 'threatstream.source')\n"
  );

  this.api.addRightClick("threatstreamip", {name:"Threat Stream", url:"https://ui.threatstream.com/detail/ip/%TEXT%", category:"ip"});
  this.api.addRightClick("threatstreamhost", {name:"Threat Stream", url:"https://ui.threatstream.com/detail/domain/%HOST%", category:"host"});
  this.api.addRightClick("threatstreamemail", {name:"Threat Stream", url:"https://ui.threatstream.com/detail/email/%TEXT%", category:"user"});
  this.api.addRightClick("threatstreammd5", {name:"Threat Stream", url:"https://ui.threatstream.com/detail/md5/%TEXT%", category:"md5"});

  switch (this.mode) {
  case "zip":
    this.loadFile();
    setInterval(this.loadFile.bind(this), 8*60*60*1000); // Reload file every 8 hours

    ThreatStreamSource.prototype.getDomain = getDomainZip;
    ThreatStreamSource.prototype.getIp = getIpZip;
    ThreatStreamSource.prototype.getMd5 = getMd5Zip;
    ThreatStreamSource.prototype.getEmail = getEmailZip;
    ThreatStreamSource.prototype.dump = dumpZip;
    break;
  case "api":
    ThreatStreamSource.prototype.getDomain = getDomainApi;
    ThreatStreamSource.prototype.getIp = getIpApi;
    ThreatStreamSource.prototype.getMd5 = getMd5Api;
    ThreatStreamSource.prototype.getEmail = getEmailApi;
    this.loadTypes();
    break;
  case "sqlite3":
    ThreatStreamSource.prototype.getDomain = getDomainSqlite3;
    ThreatStreamSource.prototype.getIp = getIpSqlite3;
    ThreatStreamSource.prototype.getMd5 = getMd5Sqlite3;
    ThreatStreamSource.prototype.getEmail = getEmailSqlite3;
    this.loadTypes();
    break;
  }
};
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var source = new ThreatStreamSource(api, "threatstream");
  source.init();
};
//////////////////////////////////////////////////////////////////////////////////
