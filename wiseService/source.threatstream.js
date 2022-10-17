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

const fs = require('fs');
const unzipper = require('unzipper');
const WISESource = require('./wiseSource.js');
const request = require('request');
const exec = require('child_process').exec;
const betterSqlite3 = require('better-sqlite3');
const ArkimeUtil = require('../common/arkimeUtil');

class ThreatStreamSource extends WISESource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { });
    this.user = api.getConfig('threatstream', 'user');
    this.key = api.getConfig('threatstream', 'key');
    this.mode = api.getConfig('threatstream', 'mode', 'zip');

    if (this.user === undefined) {
      console.log(this.section, '- No user defined');
      return;
    }

    if (this.key === undefined) {
      console.log(this.section, '- No key defined');
      return;
    }

    this.reloadTime = 'never';
    api.app.get('/threatstream/_reloadTime', (req, res) => {
      res.send(this.reloadTime.toString());
    });

    switch (this.mode) {
    case 'api':
      this.inProgress = 0;
      ThreatStreamSource.prototype.getDomain = ThreatStreamSource.prototype.getDomainApi;
      ThreatStreamSource.prototype.getIp = ThreatStreamSource.prototype.getIpApi;
      ThreatStreamSource.prototype.getMd5 = ThreatStreamSource.prototype.getMd5Api;
      ThreatStreamSource.prototype.getEmail = ThreatStreamSource.prototype.getEmailApi;
      this.loadTypes(false);
      break;
    case 'zip':
      this.ips = new Map();
      this.domains = new Map();
      this.emails = new Map();
      this.md5s = new Map();
      this.urls = new Map();
      setImmediate(this.loadFile.bind(this));
      setInterval(this.loadFile.bind(this), 8 * 60 * 60 * 1000); // Reload file every 8 hours
      ThreatStreamSource.prototype.getDomain = ThreatStreamSource.prototype.getDomainZip;
      ThreatStreamSource.prototype.getIp = ThreatStreamSource.prototype.getIpZip;
      ThreatStreamSource.prototype.getMd5 = ThreatStreamSource.prototype.getMd5Zip;
      ThreatStreamSource.prototype.getEmail = ThreatStreamSource.prototype.getEmailZip;
      ThreatStreamSource.prototype.getURL = ThreatStreamSource.prototype.getURLZip;
      ThreatStreamSource.prototype.dump = ThreatStreamSource.prototype.dumpZip;
      api.addSource('threatstream', this, ['domain', 'email', 'ip', 'md5', 'url']);
      api.app.get('/threatstream/_reload', (req, res) => {
        this.loadFile.bind(this);
        res.send('Ok');
      });

      break;
    case 'sqlite3':
    case 'sqlite3-copy':
      if (this.mode === 'sqlite3-copy') {
        this.openDbCopy();
        setInterval(this.openDbCopy.bind(this), 30 * 60 * 1000);
      } else {
        this.openDb();
      }
      ThreatStreamSource.prototype.getDomain = ThreatStreamSource.prototype.getDomainSqlite3;
      ThreatStreamSource.prototype.getIp = ThreatStreamSource.prototype.getIpSqlite3;
      ThreatStreamSource.prototype.getMd5 = ThreatStreamSource.prototype.getMd5Sqlite3;
      ThreatStreamSource.prototype.getEmail = ThreatStreamSource.prototype.getEmailSqlite3;
      ThreatStreamSource.prototype.getURL = ThreatStreamSource.prototype.getURLSqlite3;
      this.loadTypes(true);
      api.app.get('/threatstream/_reload', (req, res) => {
        this.openDB.bind(this);
        res.send('Ok');
      });
      break;
    default:
      console.log(this.section, 'Unknown mode', this.mode);
      process.exit(0);
    }

    this.severityField = this.api.addField('field:threatstream.severity;db:threatstream.severity;kind:lotermfield;friendly:Severity;help:Threatstream Severity;count:true');
    this.confidenceField = this.api.addField('field:threatstream.confidence;db:threatstream.confidence;kind:integer;friendly:Confidence;help:Threatstream Confidence;count:true');
    this.idField = this.api.addField('field:threatstream.id;db:threatstream.id;kind:integer;friendly:Id;help:Threatstream Id;count:true');
    this.typeField = this.api.addField('field:threatstream.type;db:threatstream.type;kind:lotermfield;friendly:Type;help:Threatstream Type;count:true');
    this.maltypeField = this.api.addField('field:threatstream.maltype;db:threatstream.maltype;kind:lotermfield;friendly:Malware Type;help:Threatstream Malware Type;count:true');
    this.sourceField = this.api.addField('field:threatstream.source;db:threatstream.source;kind:termfield;friendly:Source;help:Threatstream Source;count:true');
    this.importIdField = this.api.addField('field:threatstream.importId;db:threatstream.importId;kind:integer;friendly:Import Id;help:Threatstream Import Id;count:true');

    this.api.addView('threatstream',
      'require:threatstream;title:Threatstream;fields:threatstream.severity,threatstream.confidence,threatstream.id,threatstream.importId,threatstream.type,threatstream.maltype,threatstream.source');

    this.api.addValueAction('threatstreamip', { name: 'Threatstream', url: 'https://ui.threatstream.com/detail/ip/%TEXT%', category: 'ip' });
    this.api.addValueAction('threatstreamhost', { name: 'Threatstream', url: 'https://ui.threatstream.com/detail/domain/%HOST%', category: 'host' });
    this.api.addValueAction('threatstreamemail', { name: 'Threatstream', url: 'https://ui.threatstream.com/detail/email/%TEXT%', category: 'user' });
    this.api.addValueAction('threatstreammd5', { name: 'Threatstream', url: 'https://ui.threatstream.com/detail/md5/%TEXT%', category: 'md5' });
    this.api.addValueAction('threatstreamimportid', { name: 'Threatstream Import Lookup', url: 'https://ui.threatstream.com/import/review/%TEXT%', fields: 'threatstream.importId' });
    this.api.addValueAction('threatstreamid', { name: 'Threatstream Id Lookup', url: 'https://api.threatstream.com/api/v1/intelligence/%TEXT%/', fields: 'threatstream.id' });
  }

  // ----------------------------------------------------------------------------
  parseFile () {
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
      .pipe(unzipper.Parse())
      .on('entry', (entry) => {
        const bufs = [];
        entry.on('data', (buf) => {
          bufs.push(buf);
        }).on('end', () => {
          const json = JSON.parse(Buffer.concat(bufs));
          json.objects.forEach((item) => {
            if (item.state !== 'active') {
              return;
            }

            let encoded;
            try {
              if (item.maltype && item.maltype !== 'null') {
                encoded = WISESource.encodeResult(
                  this.severityField, item.severity.toLowerCase(),
                  this.confidenceField, '' + item.confidence,
                  this.idField, '' + item.id,
                  this.typeField, item.itype.toLowerCase(),
                  this.maltypeField, item.maltype.toLowerCase(),
                  this.sourceField, item.source
                );
              } else {
                encoded = WISESource.encodeResult(
                  this.severityField, item.severity.toLowerCase(),
                  this.confidenceField, '' + item.confidence,
                  this.idField, '' + item.id,
                  this.typeField, item.itype.toLowerCase(),
                  this.sourceField, item.source
                );
              }
            } catch (e) {
              console.log(this.section, 'ERROR -', entry.path, e, item, ArkimeUtil.sanitizeStr(e.stack));
              return;
            }

            if (item.itype.match(/(_ip|anon_proxy|anon_vpn)/)) {
              this.ips.set(item.srcip, encoded);
            } else if (item.itype.match(/_domain|_dns/)) {
              this.domains.set(item.domain, encoded);
            } else if (item.itype.match(/_email/)) {
              this.emails.set(item.email, encoded);
            } else if (item.itype.match(/_md5/)) {
              this.md5s.set(item.md5, encoded);
            } else if (item.itype.match(/_url/)) {
              if (item.url.lastIndexOf('http://', 0) === 0) {
                item.url = item.url.substring(7);
              }
              this.urls.set(item.url, encoded);
            }
          });
          // console.log(this.section, "- Done", entry.path);
        });
      })
      .on('close', () => {
        console.log(this.section, '- Done Loading');
      });
  };

  // ----------------------------------------------------------------------------
  loadFile () {
    console.log(this.section, '- Downloading files');
    WISESource.request('https://api.threatstream.com/api/v1/intelligence/snapshot/download/?username=' + this.user + '&api_key=' + this.key, '/tmp/threatstream.zip', (statusCode) => {
      if (statusCode === 200 || !this.loaded) {
        this.loaded = true;
        this.parseFile();
        this.reloadTime = new Date();
      }
    });
  };

  // ----------------------------------------------------------------------------
  getDomainZip (domain, cb) {
    const domains = this.domains;
    cb(null, domains.get(domain) || domains.get(domain.substring(domain.indexOf('.') + 1)));
  }

  // ----------------------------------------------------------------------------
  getIpZip (ip, cb) {
    cb(null, this.ips.get(ip));
  }

  // ----------------------------------------------------------------------------
  getMd5Zip (md5, cb) {
    cb(null, this.md5s.get(md5));
  }

  // ----------------------------------------------------------------------------
  getEmailZip (email, cb) {
    cb(null, this.emails.get(email));
  }

  // ----------------------------------------------------------------------------
  getURLZip (url, cb) {
    url = `http://${url}`;
    cb(null, this.urls.get(url));
  }

  // ----------------------------------------------------------------------------
  dumpZip (res) {
    res.write('{');
    ['ips', 'domains', 'emails', 'md5s', 'urls'].forEach((ckey) => {
      res.write(`${ckey}: [\n`);
      this[ckey].forEach((value, key) => {
        const str = `{"key": "${key}", "ops":\n` +
          WISESource.result2JSON(value) + '},\n';
        res.write(str);
      });
      res.write('],\n');
    });
    res.write('}');
    res.end();
  }

  // ----------------------------------------------------------------------------
  getApi (type, value, cb) {
    const options = {
      url: `https://api.threatstream.com/api/v2/intelligence/?username=${this.user}&api_key=${this.key}&status=active&${type}=${value}&itype=${this.types[type]}`,
      method: 'GET',
      forever: true
    };

    if (this.inProgress > 50) {
      return cb('dropped');
    }

    this.inProgress++;
    request(options, (err, response, body) => {
      this.inProgress--;
      if (err) {
        console.log(this.section, 'problem fetching ', options, err || response);
        return cb(null, WISESource.emptyResult);
      }

      try {
        body = JSON.parse(body);
      } catch (e) {
        console.log(this.section, "Couldn't parse", body);
        return cb(null, WISESource.emptyResult);
      }

      if (body.objects.length === 0) {
        return cb(null, WISESource.emptyResult);
      }

      const args = [];
      body.objects.forEach((item) => {
        args.push(
          this.confidenceField, '' + item.confidence,
          this.idField, '' + item.id,
          this.typeField, item.itype.toLowerCase(),
          this.sourceField, item.source
        );

        if (item.maltype !== undefined && item.maltype !== 'null') {
          args.push(this.maltypeField, item.maltype.toLowerCase());
        }
        if (item.severity !== undefined) {
          args.push(this.severityField, item.severity.toLowerCase());
        }
      });
      const result = WISESource.encodeResult.apply(null, args);
      return cb(null, result);
    });
  };

  // ----------------------------------------------------------------------------
  getDomainApi (domain, cb) {
    return this.getApi('domain', domain, cb);
  }

  // ----------------------------------------------------------------------------
  getIpApi (ip, cb) {
    return this.getApi('ip', ip, cb);
  }

  // ----------------------------------------------------------------------------
  getMd5Api (md5, cb) {
    return this.getApi('md5', md5, cb);
  }

  // ----------------------------------------------------------------------------
  getEmailApi (email, cb) {
    return this.getApi('email', email, cb);
  }

  // ----------------------------------------------------------------------------
  getSqlite3 (type, field, value, cb) {
    if (!this.db) {
      return cb('dropped');
    }

    try {
      const data = this.db.prepare(`SELECT * FROM ts WHERE ${field} = ? AND state != 'falsepos' AND itype IN (${this.typesWithQuotes[type]})`).all(value);

      if (!data || data.length === 0) {
        return cb(null, WISESource.emptyResult);
      }

      const args = [];
      data.forEach((item) => {
        args.push(
          this.confidenceField, '' + item.confidence,
          this.idField, '' + item.id,
          this.typeField, item.itype.toLowerCase(),
          this.sourceField, item.source
        );

        if (item.maltype !== undefined && item.maltype !== null) {
          args.push(this.maltypeField, item.maltype.toLowerCase());
        }
        if (item.severity !== undefined) {
          args.push(this.severityField, item.severity.toLowerCase());
        }
        if (item.import_session_id !== undefined && +item.import_session_id !== 0) {
          args.push(this.importIdField, '' + item.import_session_id);
        }
      });
      const result = WISESource.encodeResult.apply(null, args);
      return cb(null, result);
    } catch (err) {
      console.log(this.section, 'ERROR', err);
      return cb('dropped');
    }
  };

  // ----------------------------------------------------------------------------
  getDomainSqlite3 (domain, cb) {
    this.getSqlite3('domain', 'domain', domain, (err, result) => {
      if (err || result !== WISESource.emptyResult) {
        return cb(err, result);
      }
      this.getSqlite3('domain', 'domain', domain.substring(domain.indexOf('.') + 1), cb);
    });
  }

  // ----------------------------------------------------------------------------
  getIpSqlite3 (ip, cb) {
    return this.getSqlite3('ip', 'srcip', ip, cb);
  }

  // ----------------------------------------------------------------------------
  getMd5Sqlite3 (md5, cb) {
    return this.getSqlite3('md5', 'md5', md5, cb);
  }

  // ----------------------------------------------------------------------------
  getEmailSqlite3 (email, cb) {
    return this.getSqlite3('email', 'email', email, cb);
  }

  // ----------------------------------------------------------------------------
  getURLSqlite3 (url, cb) {
    url = `http://${url}`;
    return this.getSqlite3('url', 'url', url, cb);
  }

  // ----------------------------------------------------------------------------
  loadTypes (includeUrl) {
    // Threatstream doesn't have a way to just ask for type matches, so we need to figure out which itypes are various types.
    this.types = {};
    this.typesWithQuotes = {};
    request({ url: 'https://api.threatstream.com/api/v1/impact/?username=' + this.user + '&api_key=' + this.key + '&limit=1000', forever: true }, (err, response, body) => {
      if (err) {
        console.log(this.section, 'ERROR - failed to load types', err);
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
      for (const key in this.types) {
        this.typesWithQuotes[key] = this.types[key].map((v) => { return "'" + v + "'"; }).join(',');
        this.types[key] = this.types[key].join(',');
      }

      // Wait to register until request is done
      if (includeUrl) {
        this.api.addSource('threatstream', this, ['domain', 'email', 'ip', 'md5', 'url']);
      } else {
        this.api.addSource('threatstream', this, ['domain', 'email', 'ip', 'md5']);
      }
    });
  };

  // ----------------------------------------------------------------------------
  checkMd5Index (db, dbFile) {
    const results = db.prepare("SELECT name FROM sqlite_master WHERE type = 'index' AND name = 'md5_index'").all();
    if (!results || results.length === 0) {
      console.log(`ERROR - Must create the md5_index. Run:\n  echo "CREATE INDEX IF NOT EXISTS md5_index ON ts (md5)" | sqlite3 ${dbFile}`);
      process.exit();
    }
  }

  // ----------------------------------------------------------------------------
  openDbCopy () {
    const dbFile = this.api.getConfig('threatstream', 'dbFile', 'ts.db');

    let dbStat;
    try { dbStat = fs.statSync(dbFile); } catch (e) {}

    let realDb;
    if (!dbStat || !dbStat.isFile()) {
      console.log(this.section, "ERROR - file doesn't exist", dbFile);
      process.exit();
    }

    const copyIt = () => {
      try {
        // 1) Lock realDb
        realDb.prepare('BEGIN IMMEDIATE').run();

        // 2) Copy real Db to .temp so that the .moloch db still works
        console.log(this.section, '- Copying DB', dbStat.mtime);
        exec(`/bin/cp -f ${dbFile} ${dbFile}.temp`, (err, stdout, stderr) => {
          // 3) Unlock realDb and close
          realDb.prepare('END').run();
          realDb.close();
          realDb = null;

          // 4) Close current .moloch db if open
          if (this.db) {
            this.db.close();
            this.db = null;
          }

          // 5) Remove old .moloch and rename .tmp to .moloch
          exec(`/bin/rm -f ${dbFile}.moloch`, (err, subStdout, subStderr) => {
            exec(`/bin/mv -f ${dbFile}.temp ${dbFile}.moloch`, (err, subSubStdout, subSubStderr) => {
              // 6) open new .moloch file
              this.db = betterSqlite3(`${dbFile}.moloch`, { readonly: true, timeout: 1000 });
              console.log(`${this.section} - Loaded DB`);
              this.reloadTime = new Date();
            });
          });
        });
      } catch (err) {
        if (err.code === 'SQLITE_BUSY') {
          return setTimeout(() => { copyIt(); }, 30 * 1000); // Try to lock in 30 seconds
        }
        console.log('Threatstream failed', err);
      }
    };

    // If the last copy time doesn't match start the copy process.
    // This will also run on startup.
    if (this.mtime !== dbStat.mtime.getTime()) {
      this.mtime = dbStat.mtime.getTime();
      try {
        realDb = betterSqlite3(dbFile, { timeout: 1000 });
      } catch (err) {
        console.log(`ERROR - couldn't open threatstream db ${dbFile}`, err);
        process.exit();
      }
      this.checkMd5Index(realDb, dbFile);
      copyIt();
    }

    try {
      if (!this.db) {
        // Open the DB if not already opened.
        this.db = betterSqlite3(`${dbFile}.moloch`, { readonly: true, timeout: 1000 });
        console.log(`${this.section} - Loaded DB`);
      }
    } catch (err) {
    }
  };

  // ----------------------------------------------------------------------------
  openDb () {
    const dbFile = this.api.getConfig('threatstream', 'dbFile', 'ts.db');

    let dbStat;
    try { dbStat = fs.statSync(dbFile); } catch (e) {}

    if (!dbStat || !dbStat.isFile()) {
      console.log(this.section, "ERROR - file doesn't exist", dbFile);
      process.exit();
    }

    try {
      this.db = betterSqlite3(dbFile, { readonly: true, timeout: 1000 });
    } catch (err) {
      console.log(`ERROR - couldn't open threatstream db ${dbFile}`, err);
      process.exit();
    }
    console.log(`${this.section} - Loaded DB`);

    this.checkMd5Index(this.db, dbFile);

    setInterval(() => {
      try {
        const result = this.db.pragma('main.wal_checkpoint(TRUNCATE)');
        console.log('Threatstream Truncate - ', result);
        if (result.length > 0 && result[0].busy) {
          const result2 = this.db.pragma('main.wal_checkpoint(PASSIVE)');
          console.log('Threatstream Passive Truncate - ', result2);
        }
      } catch (err) {
        console.log('Threatstream truncate error', err);
      }
    }, 5 * 60 * 1000);
  };
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('threatstream', {
    singleton: true,
    name: 'threatstream',
    description: 'Anomali Threatstream source',
    link: 'https://arkime.com/wise#threatstream',
    types: ['ip', 'domain', 'md5', 'email', 'url'],
    fields: [
      { name: 'mode', required: true, help: 'Mode to work in zip, api, sqlite3, sqlite3-copy', regex: '^(zip|api|sqlite3|sqlite3-copy)$' },
      { name: 'user', required: true, help: 'Threatstream user' },
      { name: 'key', password: true, required: true, help: 'Threatstream key' },
      { name: 'dbFile', required: false, help: 'Path to the ts.db file when using sqlite3 mode' }
    ]
  });

  return new ThreatStreamSource(api, 'threatstream');
};
// ----------------------------------------------------------------------------
