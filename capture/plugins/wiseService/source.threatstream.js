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

  if (this.user === undefined) {
    console.log(this.section, "- No user defined");
    return;
  }

  if (this.key === undefined) {
    console.log(this.section, "- No key defined");
    return;
  }

  this.reloadTime = "never";
  api.app.get("/threatstream/_reloadTime", (req, res) => {
    res.send(this.reloadTime.toString());
  });

  switch (this.mode) {
  case "api":
    this.inProgress = 0;
    ThreatStreamSource.prototype.getDomain = getDomainApi;
    ThreatStreamSource.prototype.getIp     = getIpApi;
    ThreatStreamSource.prototype.getMd5    = getMd5Api;
    ThreatStreamSource.prototype.getEmail  = getEmailApi;
    this.loadTypes();
    break;
  case "zip":
    this.ips          = new HashTable();
    this.domains      = new HashTable();
    this.emails       = new HashTable();
    this.md5s         = new HashTable();
    this.urls         = new HashTable();
    this.cacheTimeout = -1;
    setImmediate(this.loadFile.bind(this));
    setInterval(this.loadFile.bind(this), 8*60*60*1000); // Reload file every 8 hours
    ThreatStreamSource.prototype.getDomain = getDomainZip;
    ThreatStreamSource.prototype.getIp     = getIpZip;
    ThreatStreamSource.prototype.getMd5    = getMd5Zip;
    ThreatStreamSource.prototype.getEmail  = getEmailZip;
    ThreatStreamSource.prototype.getURL    = getURLZip;
    ThreatStreamSource.prototype.dump      = dumpZip;
    api.addSource("threatstream", this);
    api.app.get("/threatstream/_reload", (req, res) => {
      this.loadFile.bind(this);
      res.send("Ok");
    });

    break;
  case "sqlite3":
    sqlite3           = require('sqlite3');
    this.cacheTimeout = -1;
    this.openDb();
    setInterval(this.openDb.bind(this), 5*60*1000);
    ThreatStreamSource.prototype.getDomain = getDomainSqlite3;
    ThreatStreamSource.prototype.getIp     = getIpSqlite3;
    ThreatStreamSource.prototype.getMd5    = getMd5Sqlite3;
    ThreatStreamSource.prototype.getEmail  = getEmailSqlite3;
    ThreatStreamSource.prototype.getURL    = getURLSqlite3;
    this.loadTypes();
    api.app.get("/threatstream/_reload", (req, res) => {
      this.openDB.bind(this)
      res.send("Ok");
    });
    break;
  default:
    console.log(this.section, "Unknown mode", this.mode);
    process.exit(0);
  }

  this.severityField = this.api.addField("field:threatstream.severity;db:threatstream.severity;kind:lotermfield;friendly:Severity;help:Threatstream Severity;count:true");
  this.confidenceField = this.api.addField("field:threatstream.confidence;db:threatstream.confidence;kind:integer;friendly:Confidence;help:Threatstream Confidence;count:true");
  this.idField = this.api.addField("field:threatstream.id;db:threatstream.id;kind:integer;friendly:Id;help:Threatstream Id;count:true");
  this.typeField = this.api.addField("field:threatstream.type;db:threatstream.type;kind:lotermfield;friendly:Type;help:Threatstream Type;count:true");
  this.maltypeField = this.api.addField("field:threatstream.maltype;db:threatstream.maltype;kind:lotermfield;friendly:Malware Type;help:Threatstream Malware Type;count:true");
  this.sourceField = this.api.addField("field:threatstream.source;db:threatstream.source;kind:termfield;friendly:Source;help:Threatstream Source;count:true");
  this.importIdField = this.api.addField("field:threatstream.importId;db:threatstream.importId;kind:integer;friendly:Import Id;help:Threatstream Import Id;count:true");

  this.api.addView("threatstream",
    "if (session.threatstream)\n" +
    "  div.sessionDetailMeta.bold Threatstream\n" +
    "  dl.sessionDetailMeta\n" +
    "    +arrayList(session.threatstream, 'severity', 'Severity', 'threatstream.severity')\n" +
    "    +arrayList(session.threatstream, 'confidence', 'Confidence', 'threatstream.confidence')\n" +
    "    +arrayList(session.threatstream, 'id', 'Id', 'threatstream.id')\n" +
    "    +arrayList(session.threatstream, 'importId', 'Import Id', 'threatstream.importId')\n" +
    "    +arrayList(session.threatstream, 'type', 'Type', 'threatstream.type')\n" +
    "    +arrayList(session.threatstream, 'maltype', 'Malware Type', 'threatstream.maltype')\n" +
    "    +arrayList(session.threatstream, 'source', 'Source', 'threatstream.source')\n"
  );

  this.api.addRightClick("threatstreamip", {name:"Threatstream", url:"https://ui.threatstream.com/detail/ip/%TEXT%", category:"ip"});
  this.api.addRightClick("threatstreamhost", {name:"Threatstream", url:"https://ui.threatstream.com/detail/domain/%HOST%", category:"host"});
  this.api.addRightClick("threatstreamemail", {name:"Threatstream", url:"https://ui.threatstream.com/detail/email/%TEXT%", category:"user"});
  this.api.addRightClick("threatstreammd5", {name:"Threatstream", url:"https://ui.threatstream.com/detail/md5/%TEXT%", category:"md5"});
  this.api.addRightClick("threatstreamimportid", {name:"Threatstream Import Lookup", url:"https://ui.threatstream.com/import/review/%TEXT%", fields:"threatstream.importId"});
  this.api.addRightClick("threatstreamid", {name:"Threatstream Id Lookup", url:"https://api.threatstream.com/api/v1/intelligence/%TEXT%/", fields:"threatstream.id"});
}
util.inherits(ThreatStreamSource, wiseSource);
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.parseFile = function()
{
  this.ips.clear();
  this.ips.reserve(2000000);
  this.domains.clear();
  this.domains.reserve(200000);
  this.emails.clear();
  this.emails.reserve(200000);
  this.md5s.clear();
  this.md5s.reserve(200000);
  this.urls.clear();
  this.urls.reserve(200000);

  fs.createReadStream('/tmp/threatstream.zip')
    .pipe(unzip.Parse())
    .on('entry', (entry) => {
      let bufs = [];
      entry.on('data', (buf) => {
        bufs.push(buf);
      }).on('end', () => {
        var json = JSON.parse(Buffer.concat(bufs));
        json.objects.forEach((item) => {
          if (item.state !== "active") {
            return;
          }

          var encoded;
          var num;
          try {
            if (item.maltype && item.maltype !== "null") {
              encoded = wiseSource.encode(this.severityField, item.severity.toLowerCase(),
                                          this.confidenceField, "" + item.confidence,
                                          this.idField, "" + item.id,
                                          this.typeField, item.itype.toLowerCase(),
                                          this.maltypeField, item.maltype.toLowerCase(),
                                          this.sourceField, item.source);
              num = 6;
            } else {
              encoded = wiseSource.encode(this.severityField, item.severity.toLowerCase(),
                                          this.confidenceField, "" + item.confidence,
                                          this.idField, "" + item.id,
                                          this.typeField, item.itype.toLowerCase(),
                                          this.sourceField, item.source);
              num = 5;
            }
          } catch (e) {
            console.log(this.section, "ERROR -", entry.path, e, item, e.stack);
            return;
          }


          if (item.itype.match(/(_ip|anon_proxy|anon_vpn)/)) {
            this.ips.put(item.srcip, {num: num, buffer: encoded});
          } else if (item.itype.match(/_domain|_dns/)) {
            this.domains.put(item.domain, {num: num, buffer: encoded});
          } else if (item.itype.match(/_email/)) {
            this.emails.put(item.email, {num: num, buffer: encoded});
          } else if (item.itype.match(/_md5/)) {
            this.md5s.put(item.md5, {num: num, buffer: encoded});
          } else if (item.itype.match(/_url/)) {
            if (item.url.lastIndexOf("http://", 0) === 0) {
              item.url = item.url.substring(7);
            }
            this.urls.put(item.url, {num: num, buffer: encoded});
          }
        });
        //console.log(this.section, "- Done", entry.path);
      });
    })
    .on('close', () => {
      console.log(this.section, "- Done Loading");
    });
};
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.loadFile = function() {
  console.log(this.section, "- Downloading files");
  wiseSource.request('https://api.threatstream.com/api/v1/intelligence/snapshot/download/?username=' + this.user + '&api_key=' + this.key,  '/tmp/threatstream.zip', (statusCode) => {
    if (statusCode === 200 || !this.loaded) {
      this.loaded = true;
      this.parseFile();
      this.reloadTime = new Date();
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
function getURLZip(url, cb) {
  url = `http://${url}`;
  cb(null, this.urls.get(url));
}
//////////////////////////////////////////////////////////////////////////////////
function dumpZip (res) {
  res.write("{");
  ["ips", "domains", "emails", "md5s", "urls"].forEach((ckey) => {
    res.write(`${ckey}: [\n`);
    this[ckey].forEach((key, value) => {
      var str = `{key: "${key}", ops:\n` +
        wiseSource.result2Str(wiseSource.combineResults([value])) + "},\n";
      res.write(str);
    });
    res.write("],\n");
  });
  res.write("}");
  res.end();
}
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.getApi = function(type, value, cb) {
  var options = {
      url: `https://api.threatstream.com/api/v2/intelligence/?username=${this.user}&api_key=${this.key}&status=active&${type}=${value}&itype=${this.types[type]}`,
      method: 'GET',
      forever: true
  };

  if (this.inProgress > 50) {
    return cb ("dropped");
  }

  this.inProgress++;
  request(options, (err, response, body) => {
    this.inProgress--;
    if (err) {
      console.log(this.section, "problem fetching ", options, err || response);
      return cb(null, wiseSource.emptyResult);
    }

    try {
      body = JSON.parse(body);
    } catch (e) {
      console.log(this.section, "Couldn't parse", body);
      return cb(null, wiseSource.emptyResult);
    }

    if (body.objects.length === 0) {
      return cb(null, wiseSource.emptyResult);
    }

    var args = [];
    body.objects.forEach((item) => {
      args.push(this.confidenceField, "" + item.confidence,
                this.idField, "" + item.id,
                this.typeField, item.itype.toLowerCase(),
                this.sourceField, item.source);

      if (item.maltype !== undefined && item.maltype !== "null") {
        args.push(this.maltypeField, item.maltype.toLowerCase());
      }
      if (item.severity !== undefined) {
        args.push(this.severityField, item.severity.toLowerCase());
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
  if (!this.db) {
    return cb("dropped");
  }

  this.db.all(`SELECT * FROM ts WHERE ${field} = ? AND state != "falsepos" AND itype IN (${this.typesWithQuotes[type]})`, value, (err, data) => {
    if (err) {
      console.log(this.section, "ERROR", err, data);
      return cb("dropped");
    }
    if (data.length === 0) {
      return cb(null, wiseSource.emptyResult);
    }

    var args = [];
    data.forEach((item) => {
      args.push(this.confidenceField, "" + item.confidence,
                this.idField, "" + item.id,
                this.typeField, item.itype.toLowerCase(),
                this.sourceField, item.source);

      if (item.maltype !== undefined && item.maltype !== null) {
        args.push(this.maltypeField, item.maltype.toLowerCase());
      }
      if (item.severity !== undefined) {
        args.push(this.severityField, item.severity.toLowerCase());
      }
      if (item.import_session_id !== undefined && +item.import_session_id !== 0) {
        args.push(this.importIdField, "" + item.import_session_id);
      }
    });
    var result = {num: args.length/2, buffer: wiseSource.encode.apply(null, args)};
    return cb(null, result);
  });
};
//////////////////////////////////////////////////////////////////////////////////
function getDomainSqlite3(domain, cb) {
  this.getSqlite3("domain", "domain", domain, (err, result) => {
    if (err || result !== wiseSource.emptyResult) {
      return cb(err, result);
    }
    this.getSqlite3("domain", "domain", domain.substring(domain.indexOf(".")+1), cb);
  });
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
function getURLSqlite3(url, cb) {
  url = `http://${url}`;
  return this.getSqlite3("url", "url", url, cb);
}
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.loadTypes = function() {
  // Threatstream doesn't have a way to just ask for type matches, so we need to figure out which itypes are various types.
  this.types = {};
  this.typesWithQuotes = {};
  request({url: "https://api.threatstream.com/api/v1/impact/?username="+this.user+"&api_key="+this.key+"&limit=1000", forever: true}, (err, response, body) => {
    if (err) {
      console.log(this.section, "ERROR - failed to load types", err);
      return;
    }

    body = JSON.parse(body);
    body.objects.forEach((item) => {
      if (this.types[item.value_type] === undefined) {
        this.types[item.value_type] = [item.name];
      } else {
        this.types[item.value_type].push(item.name);
      }
    });
    for (var key in this.types) {
      this.typesWithQuotes[key] = this.types[key].map((v) => {return "'" + v + "'";}).join(",");
      this.types[key] = this.types[key].join(",");
    }

    // Wait to register until request is done
    this.api.addSource("threatstream", this);
  });
};
//////////////////////////////////////////////////////////////////////////////////
ThreatStreamSource.prototype.openDb = function() {
  var dbFile = this.api.getConfig("threatstream", "dbFile", "ts.db");

  var dbStat;
  try {dbStat = fs.statSync(dbFile);} catch (e) {};

  var realDb;
  if (!dbStat || !dbStat.isFile()) {
    console.log(this.section, "ERROR - file doesn't exist", dbFile);
    process.exit();
  }

  // * Lock the real DB
  // * Copy the real db to .temp so that anything currently running still works
  // * Unlock the real DB and close
  // * create md5 index on .temp version
  // * Close .moloch db
  // * mv .temp to .moloch
  // * Open .moloch
  let beginImmediate = (err) => {
    // Repeat until we lock the DB
    if (err && err.code === "SQLITE_BUSY") {
      console.log(this.section, "Failed to lock sqlite DB", dbFile);
      return realDb.run("BEGIN IMMEDIATE", beginImmediate);
    }

    console.log(this.section, "- Copying DB", dbStat.mtime);
    exec (`/bin/cp -f ${dbFile} ${dbFile}.temp`,  (err, stdout, stderr) => {
      console.log(stdout, stderr);
      realDb.run("END", (err) => {
        realDb.close();
        realDb = null;

        var tempDb = new sqlite3.Database(dbFile + ".temp");
        tempDb.run("CREATE INDEX md5_index ON ts (md5)", (err) => {
          tempDb.close();
          if (this.db) {
            this.db.close();
          }
          this.db = null;
          exec (`/bin/rm -f ${dbFile}.moloch`,  (err, stdout, stderr) => {
            exec (`/bin/mv -f ${dbFile}.temp ${dbFile}.moloch`,  (err, stdout, stderr) => {
              this.db = new sqlite3.Database(`${dbFile}.moloch`, sqlite3.OPEN_READONLY);
              console.log(`${this.section} - Loaded DB`);
              this.reloadTime = new Date();
            });
          });
        });
      });
    });
  }


  // If the last copy time doesn't match start the copy process.
  // This will also run on startup.
  if (this.mtime !== dbStat.mtime.getTime()) {
    this.mtime = dbStat.mtime.getTime();
    realDb = new sqlite3.Database(dbFile);
    realDb.run("BEGIN IMMEDIATE", beginImmediate);
  } else if (!this.db) {
    // Open the DB if not already opened.
    this.db = new sqlite3.Database(dbFile + ".moloch", sqlite3.OPEN_READONLY);
  }
};
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var source = new ThreatStreamSource(api, "threatstream");
};
//////////////////////////////////////////////////////////////////////////////////
