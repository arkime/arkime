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

const ini = require('iniparser');
const express = require('express');
const fs = require('fs');
const http = require('http');
const https = require('https');
const glob = require('glob');
const async = require('async');
const sprintf = require('./sprintf.js').sprintf;
const iptrie = require('iptrie');
const WISESource = require('./wiseSource.js');
const wiseCache = require('./wiseCache.js');
const cluster = require('cluster');
const crypto = require('crypto');
const Redis = require('ioredis');
const memjs = require('memjs');
const favicon = require('serve-favicon');
const uuid = require('uuidv4').default;
const helmet = require('helmet');
const bp = require('body-parser');
const jsonParser = bp.json();
const axios = require('axios');
const passport = require('passport');
const DigestStrategy = require('passport-http').DigestStrategy;
const elasticsearch = require('elasticsearch');
const chalk = require('chalk');
const version = require('../viewer/version');
const path = require('path');

require('console-stamp')(console, '[HH:MM:ss.l]');

const internals = {
  configFile: `${version.config_prefix}/etc/wiseService.ini`,
  debug: 0,
  insecure: false,
  fieldsTS: 0,
  fields: [],
  fieldsSize: 0,
  sources: [],
  configDefs: {
    wiseService: {
      description: 'General settings that apply to WISE and all wise sources',
      singleton: true,
      service: true,
      fields: [
        { name: 'port', required: false, regex: '^[0-9]+$', help: 'Port that the wiseService runs on. Defaults to 8081' },
        { name: 'keyFile', required: false, help: 'Path to PEM encoded key file' },
        { name: 'certFile', required: false, help: 'Path to PEM encoded cert file' },
        { name: 'userNameHeader', required: true, help: 'How should auth be done: anonymous - no auth, digest - digest auth, any other value is the http header to use for username', regex: '.' },
        { name: 'httpRealm', ifField: 'userNameHeader', ifValue: 'digest', required: false, help: 'The realm to use for digest requests. Must be the same as viewer is using. Default Moloch' },
        { name: 'passwordSecret', ifField: 'userNameHeader', ifValue: 'digest', required: false, password: true, help: 'The secret used to encrypted password hashes. Must be the same as viewer is using. Default password' },
        { name: 'usersElasticsearch', required: false, help: 'The URL to connect to elasticsearch. Default http://localhost:9200' },
        { name: 'usersPrefix', required: false, help: 'The prefix used with db.pl --prefix for users elasticsearch, usually empty' },
        { name: 'sourcePath', required: false, help: 'Where to look for the source files. Defaults to "./"' }
      ]
    },
    cache: {
      description: 'Specify how WISE should cache results from sources that support it. Using a redis setup is especially useful when there are multiple WISE servers or large amount of results to cache.',
      singleton: true,
      service: true,
      fields: [
        { name: 'type', required: false, regex: '^(memory|redis|memcached)$', help: 'Where to cache results: memory (default), redis, memcached' },
        { name: 'cacheSize', required: false, help: 'How many elements to cache in memory. Defaults to 100000' },
        { name: 'redisURL', password: true, required: false, ifField: 'type', ifValue: 'redis', help: 'Format is redis://[:password@]host:port/db-number, redis-sentinel://[[sentinelPassword]:[password]@]host[:port]/redis-name/db-number, or redis-cluster://[:password@]host:port/db-number' },
        { name: 'redisFormat', required: false, ifField: 'type', ifValue: 'redis', help: 'Use 2 (default) if WISE 2.x & WISE 3.x in use or 3 if just WISE 3.x', regex: '[23]' },
        { name: 'memcachedURL', password: true, required: false, ifField: 'type', ifValue: 'memcached', help: 'Format is memcached://[user:pass@]server1[:11211],[user:pass@]server2[:11211],...' }
      ]
    }
  },
  configSchemes: {
  },
  types: {
  },
  views: {},
  valueActions: {},
  workers: 1,
  regressionTests: false,
  webconfig: false,
  configCode: crypto.randomBytes(20).toString('base64').replace(/[=+/]/g, '').substr(0, 6),
  startTime: Date.now()
};

internals.type2Name = ['ip', 'domain', 'md5', 'email', 'url', 'tuple', 'ja3', 'sha256'];

// ----------------------------------------------------------------------------
// Command Line Parsing
// ----------------------------------------------------------------------------
function processArgs (argv) {
  for (let i = 0, ilen = argv.length; i < ilen; i++) {
    if (argv[i] === '-c') {
      i++;
      internals.configFile = argv[i];
    } else if (argv[i] === '--insecure') {
      internals.insecure = true;
    } else if (argv[i] === '--debug') {
      internals.debug++;
    } else if (argv[i] === '--regressionTests') {
      internals.regressionTests = true;
    } else if (argv[i] === '--webconfig') {
      internals.webconfig = true;
      console.log(chalk.cyan(
        `${chalk.bgCyan.black('IMPORTANT')} - Config pin code is: ${internals.configCode}`
      ));
    } else if (argv[i] === '--workers') {
      i++;
      internals.workers = +argv[i];
    } else if (argv[i] === '--help') {
      console.log('wiseService.js [<options>]');
      console.log('');
      console.log('Options:');
      console.log('  --debug               Increase debug level, multiple are supported');
      console.log('  --webconfig           Allow the config to be edited from web page');
      console.log('  --workers <b>         Number of worker processes to create');
      console.log('  --insecure            Disable cert verification');

      process.exit(0);
    }
  }
}

processArgs(process.argv);

if (internals.workers > 1) {
  if (cluster.isMaster) {
    for (let i = 0; i < internals.workers; i++) {
      cluster.fork();
    }
    cluster.on('exit', (worker, code, signal) => {
      console.log('worker ' + worker.process.pid + ' died, restarting new worker');
      cluster.fork();
    });
  }
}
// ----------------------------------------------------------------------------
const app = express();
const logger = require('morgan');
const timeout = require('connect-timeout');

// super secret
app.use(helmet.hidePoweredBy());
app.use(helmet.xssFilter());
app.use(helmet.hsts({
  maxAge: 31536000,
  includeSubDomains: true
}));
// calculate nonce
app.use((req, res, next) => {
  res.locals.nonce = Buffer.from(uuid()).toString('base64');
  next();
});
app.use(helmet.contentSecurityPolicy({
  directives: {
    defaultSrc: ["'self'"],
    /* can remove unsafe-inline for css when this is fixed
    https://github.com/vuejs/vue-style-loader/issues/33 */
    styleSrc: ["'self'", "'unsafe-inline'"],
    scriptSrc: ["'self'", "'unsafe-eval'", (req, res) => `'nonce-${res.locals.nonce}'`],
    objectSrc: ["'none'"],
    imgSrc: ["'self'", 'data:'],
    frameSrc: ["'none'"]
  }
}));

function getConfig (section, name, d) {
  if (!internals.config[section]) {
    return d;
  }
  return internals.config[section][name] || d;
}

// Explicit sigint handler for running under docker
// See https://github.com/nodejs/node/issues/4182
process.on('SIGINT', function () {
  process.exit();
});

// ----------------------------------------------------------------------------
// Util
// ----------------------------------------------------------------------------
function noCacheJson (req, res, next) {
  res.header('Cache-Control', 'no-cache, private, no-store, must-revalidate, max-stale=0, post-check=0, pre-check=0');
  res.header('Content-Type', 'application/json');
  res.header('X-Content-Type-Options', 'nosniff');
  return next();
}

// ----------------------------------------------------------------------------
// Authentication
// ----------------------------------------------------------------------------
function getUser (name, cb) {
  internals.usersElasticSearch.get({ index: internals.usersPrefix + 'users', type: '_doc', id: name }, (err, result) => {
    console.log(err, result);
    if (err) { return cb(err); }
    return cb(null, result._source);
  });
}
// ----------------------------------------------------------------------------
// Decrypt the encrypted hashed password, it is still hashed
function store2ha1 (passstore) {
  try {
    const parts = passstore.split('.');
    if (parts.length === 2) {
      // New style with IV: IV.E
      const c = crypto.createDecipheriv('aes-256-cbc', internals.passwordSecret256, Buffer.from(parts[0], 'hex'));
      let d = c.update(parts[1], 'hex', 'binary');
      d += c.final('binary');
      return d;
    } else {
      // Old style without IV: E
      // eslint-disable-next-line node/no-deprecated-api
      const c = crypto.createDecipher('aes192', internals.passwordSecret);
      let d = c.update(passstore, 'hex', 'binary');
      d += c.final('binary');
      return d;
    }
  } catch (e) {
    console.log("passwordSecret set in the [default] section can not decrypt information.  You may need to re-add users if you've changed the secret.", e);
    process.exit(1);
  }
};
// ----------------------------------------------------------------------------
function setupAuth () {
  internals.userNameHeader = getConfig('wiseService', 'userNameHeader', 'anonymous');
  internals.passwordSecret = getConfig('wiseService', 'passwordSecret', 'password');
  internals.passwordSecret256 = crypto.createHash('sha256').update(internals.passwordSecret).digest();

  if (internals.userNameHeader === 'anonymous') {
    return;
  }

  const es = getConfig('wiseService', 'usersElasticsearch', 'http://localhost:9200');
  internals.usersPrefix = getConfig('wiseService', 'usersPrefix', '');

  if (internals.usersPrefix && internals.usersPrefix.charAt(internals.usersPrefix.length - 1) !== '_') {
    internals.usersPrefix += '_';
  } else {
    internals.usersPrefix = internals.usersPrefix || '';
  }

  internals.usersElasticSearch = new elasticsearch.Client({
    host: es,
    apiVersion: '7.4',
    requestTimeout: 300000,
    keepAlive: true,
    minSockets: 5,
    maxSockets: 6
  });

  if (internals.userNameHeader === 'digest') {
    passport.use(new DigestStrategy({ qop: 'auth', realm: getConfig('wiseService', 'httpRealm', 'Moloch') },
      function (userid, done) {
        getUser(userid, (err, user) => {
          if (err) { return done(err); }
          if (!user.enabled) { console.log('User', userid, 'not enabled'); return done('Not enabled'); }

          return done(null, user, { ha1: store2ha1(user.passStore) });
        });
      },
      function (options, done) {
        // TODO:  Should check nonce here
        return done(null, true);
      }
    ));
  }
}
// ----------------------------------------------------------------------------
function doAuth (req, res, next) {
  if (internals.userNameHeader === 'anonymous') {
    req.user = { userId: 'anonymous', enabled: true, createEnabled: true, webEnabled: true, headerAuthEnabled: false, emailSearch: true, removeEnabled: true, packetSearch: true };
    return next();
  }

  if (internals.userNameHeader !== 'digest') {
    if (req.headers[internals.userNameHeader] !== undefined) {
      return getUser(req.headers[internals.userNameHeader], (err, user) => {
        if (err) { return res.send(JSON.stringify({ success: false, text: 'Username not found' })); }
        if (!user.enabled) { return res.send(JSON.stringify({ success: false, text: 'Username not enabled' })); }
        req.user = user;
        return next();
      });
    } else if (internals.debug > 0) {
      console.log(`AUTH: looking for header ${internals.userNameHeader} in the headers`, req.headers);
      res.status(status || 403);
      return res.send(JSON.stringify({ success: false, text: 'Username not found' }));
    }
  }

  passport.authenticate('digest', { session: false })(req, res, function (err) {
    if (err) {
      res.status(403);
      return res.send(JSON.stringify({ success: false, text: err }));
    } else {
      return next();
    }
  });
}
// ----------------------------------------------------------------------------
function isConfigWeb (req, res, next) {
  if (!internals.webconfig) {
    return res.send({ success: false, text: 'Must start wiseService with --webconfig option' });
  }
  return next();
}

// ----------------------------------------------------------------------------
function checkAdmin (req, res, next) {
  if (req.user.createEnabled) {
    return next();
  } else {
    console.log(`${req.userId} is not an admin`);
    return res.send(JSON.stringify({ success: false, text: 'Not authorized, check log file' }));
  }
}

// ----------------------------------------------------------------------------
function checkConfigCode (req, res, next) {
  console.log(req.body);
  if (req.body !== undefined && req.body.configCode !== undefined && req.body.configCode === internals.configCode) {
    return next();
  } else {
    console.log(`Incorrect pin code used - Config pin code is: ${internals.configCode}`);
    return res.send(JSON.stringify({ success: false, text: 'Not authorized, check log file' })); // not specific error
  }
}

// ----------------------------------------------------------------------------
// Sources
// ----------------------------------------------------------------------------
function newFieldsTS () {
  const now = Math.floor(Date.now() / 1000);
  if (now <= internals.fieldsTS) {
    internals.fieldsTS++;
  } else {
    internals.fieldsTS = now;
  }
}
// ----------------------------------------------------------------------------
// https://coderwall.com/p/pq0usg/javascript-string-split-that-ll-return-the-remainder
function splitRemain (str, separator, limit) {
  str = str.split(separator);
  if (str.length <= limit) { return str; }

  const ret = str.splice(0, limit);
  ret.push(str.join(separator));

  return ret;
}
// ----------------------------------------------------------------------------
/**
 * When sources are created they get an api object to interact with the wise service.
 */
class WISESourceAPI {
  /**
   * Current debug level of wiseService
   * @type {integer}
   */
  debug = internals.debug;

  /**
   * Is wiseService running in insecure mode
   * @type {boolean}
   */
  insecure = internals.insecure;

  app = app;

  /**
   * Get from the config section a value or default
   *
   * @param {string} section - The section in the config file the key is in
   * @param {string} name - The key to get from the section
   * @param {string} [default] - the default value to return if key is not found in section
   * @returns {string} - The value found or the default value
   */
  getConfig (section, name, d) {
    return getConfig(section, name, d);
  }

  // ----------------------------------------------------------------------------
  /**
   * Get a list of all the sections in the config file
   *
   * @returns {string|Array} - A list of all the sections in the config file
   */
  getConfigSections () {
    return Object.keys(internals.config);
  }

  // ----------------------------------------------------------------------------
  /**
   * Get the full config for a section
   *
   * @param {string} section - The section of the config file to return
   * @returns {object} - A list of all the sections in the config file
   */
  getConfigSection (section) {
    return internals.config[section];
  }

  // ----------------------------------------------------------------------------
  /**
   * Add a field
   *
   * @param {string} field - An encoded field definition
   */
  addField (field) {
    let match = field.match(/field:([^;]+)/);
    const name = match[1];

    let db;
    if ((match = field.match(/db:([^;]+)/))) {
      db = match[1];
    }

    let friendly;
    if ((match = field.match(/friendly:([^;]+)/))) {
      friendly = match[1];
    }

    if (WISESource.field2Pos[name] !== undefined) {
      return WISESource.field2Pos[name];
    }

    if (internals.debug > 1) {
      console.log(`Adding field name:${name} db:${db} friendly:${friendly} from '${field}'`);
    }

    const pos = internals.fields.length;
    newFieldsTS();
    internals.fields.push(field);
    internals.fieldsSize += field.length + 10;

    // Create version 0 of fields buf
    if (internals.fields.length < 256) {
      internals.fieldsBuf0 = Buffer.alloc(internals.fieldsSize + 9);
      internals.fieldsBuf0.writeUInt32BE(internals.fieldsTS, 0);
      internals.fieldsBuf0.writeUInt32BE(0, 4);
      internals.fieldsBuf0.writeUInt8(internals.fields.length, 8);
      let offset = 9;
      for (let i = 0; i < internals.fields.length; i++) {
        const len = internals.fieldsBuf0.write(internals.fields[i], offset + 2);
        internals.fieldsBuf0.writeUInt16BE(len + 1, offset);
        internals.fieldsBuf0.writeUInt8(0, offset + 2 + len);
        offset += 3 + len;
      }
      internals.fieldsBuf0 = internals.fieldsBuf0.slice(0, offset);
    }

    // Create version 1 of fields buf
    internals.fieldsBuf1 = Buffer.alloc(internals.fieldsSize + 9);
    internals.fieldsBuf1.writeUInt32BE(internals.fieldsTS, 0);
    internals.fieldsBuf1.writeUInt32BE(1, 4);
    internals.fieldsBuf1.writeUInt16BE(internals.fields.length, 8);
    let offset = 10;
    for (let i = 0; i < internals.fields.length; i++) {
      const len = internals.fieldsBuf1.write(internals.fields[i], offset + 2);
      internals.fieldsBuf1.writeUInt16BE(len + 1, offset);
      internals.fieldsBuf1.writeUInt8(0, offset + 2 + len);
      offset += 3 + len;
    }
    internals.fieldsBuf1 = internals.fieldsBuf1.slice(0, offset);

    internals.fieldsMd5 = crypto.createHash('md5').update(internals.fieldsBuf1.slice(8)).digest('hex');

    WISESource.pos2Field[pos] = name;
    WISESource.field2Pos[name] = pos;
    WISESource.field2Info[name] = { pos: pos, friendly: friendly, db: db };
    return pos;
  }

  // ----------------------------------------------------------------------------
  /**
   * Add a view
   *
   * @param {string} name - Name of the new view
   * @param {string} view - An encoded view definition
   */
  addView (name, view) {
    if (view.includes('require:')) {
      let match = view.match(/require:([^;]+)/);
      const require = match[1];

      match = view.match(/title:([^;]+)/);
      if (!match) {
        console.log(`ERROR - ${name} view is missing 'title:' ${view}`);
        return;
      }
      const title = match[1];

      match = view.match(/fields:([^;]+)/);
      if (!match) {
        console.log(`ERROR - ${name} view is missing 'fields:' ${view}`);
        return;
      }
      const fields = match[1];

      // Can override the name in the view
      match = view.match(/section:([^;]+)/);
      if (match) {
        name = match[1];
      }

      let output = `if (session.${require})\n  div.sessionDetailMeta.bold ${title}\n  dl.sessionDetailMeta\n`;
      for (const field of fields.split(',')) {
        const info = WISESource.field2Info[field];
        if (!info) {
          continue;
        }
        if (!info.db) {
          console.log(`ERROR - missing db information for ${field}`);
          return;
        }
        const parts = splitRemain(info.db, '.', 1);
        if (parts.length === 1) {
          output += `    +arrayList(session, '${parts[0]}', '${info.friendly}', '${field}')\n`;
        } else {
          output += `    +arrayList(session.${parts[0]}, '${parts[1]}', '${info.friendly}', '${field}')\n`;
        }
      }
      internals.views[name] = output;
    } else {
      internals.views[name] = view;
    }
  }

  // ----------------------------------------------------------------------------
  /**
   * Activate a section of a source. Must be called if you want wise to query the source.
   * A section is an instance of a source, some sources can have multiple sections.
   *
   * @param {string} section - The section name
   * @param {WISESource} src - A WISESource object
   * @param {string|Array} types - An array of the types that this source supports
   */
  addSource (section, src, types) {
    if (section === undefined || src === undefined || types === undefined) {
      console.log(`ERROR - bad call to addSource for ${section}`);
      return;
    }
    internals.sources[section] = src;
    internals.sources[section].types = types;

    for (let i = 0; i < types.length; i++) {
      addType(types[i], src);
    }
  }

  // ----------------------------------------------------------------------------
  /**
   * Define all configuration for a field for a source
   * @typedef {Object} WISESourceAPI~SourceConfigField
   * @property {string} name - The name of the field
   * @property {boolean} [required=false] - Must the config value be filled out
   * @property {boolean} [password=false] - Is it a password type field that should be hidden
   * @property {string} [multiline] - If set this should be split using the value and shown in the UI as a text area
   * @property {string} help - The help text to show the user about the field
   * @property {string} [ifField] - Only show the field if the 'ifValue' field is set and is equal to 'ifValue'
   * @property {string} [ifValue] - Only show the field if the 'ifValue' field is set and is equal to 'ifValue'
   * @property {string} [regex] - The value must match the regex to be considered valid
   */

  /**
   * Define all the configuration for a source.
   * This is used by the UI to generate what to display to the admin.
   * @typedef {Object} WISESourceAPI~SourceConfig
   * @property {string} name - The name of the source
   * @property {boolean} singleton - Can there multiple instances of this source
   * @property {string} description - Friendly text about the source
   * @property {string|Array} types - List of WISE types the source supports
   * @property {boolean} [cacheable=true] - Can the source be cached by WISE
   * @property {WISESourceAPI~SourceConfigField|Array} fields - The fields for the source
   */

  /**
   * Add for each source config definition for the UI to use.
   *
   * @param {string} sourceName - The source name
   * @param {WISESourceAPI~SourceConfig} config - The configuration of this source type
   */
  addSourceConfigDef (sourceName, configDef) {
    if (internals.configDefs[sourceName] === undefined) {
      // ALW - should really merge all the types somehow here instead of type2Name
      const types = configDef.types || internals.type2Name;
      for (let i = 0; i < types.length; i++) {
        const type = types[i];
        let excludeName;
        if (type === 'url') {
          excludeName = 'excludeURLs';
        } else if (type === 'ip') {
          configDef.fields = configDef.fields.concat([
            { name: 'excludeIPs', required: false, multiline: ';', help: 'List of IPs or CIDRs to exclude in WISE lookups' },
            { name: 'onlyIPs', required: false, multiline: ';', help: 'If set, only IPs that match the list of IPs or CIDRs will be looked up by WISE' }
          ]);
          if (configDef.singleton === false && types.length > 0) {
            Object.assign(configDef.fields[configDef.fields.length - 2], { ifField: 'type', ifValue: type });
            Object.assign(configDef.fields[configDef.fields.length - 1], { ifField: 'type', ifValue: type });
          }
          continue;
        } else {
          excludeName = 'exclude' + type[0].toUpperCase() + type.slice(1) + 's';
        }

        configDef.fields = configDef.fields.concat(
          [{ name: excludeName, required: false, multiline: ';', help: 'List of modified glob patterns to exclude in WISE lookups' }]
        );

        if (configDef.singleton === false && types.length > 0) {
          Object.assign(configDef.fields[configDef.fields.length - 1], { ifField: 'type', ifValue: type });
        }
      }

      if (configDef.cacheable !== false) {
        configDef.fields = configDef.fields.concat(
          [{ name: 'cacheAgeMin', required: false, help: 'Minutes to cache items from previous lookup. (defaults to 60)', regex: '^[0-9]+$' }]
        );
      }

      if (configDef.singleton === false && types.length > 0) {
        configDef.fields = configDef.fields.concat([
          { name: 'fields', required: false, multiline: '\\n', help: 'Create a list of fields in Arkime, this may also be defined in the input file. See the Tagger Format docs for more information. In general they look like field:[EXPRESSION];db:[ES FIELD NAME];kind:[FIELD TYPE];friendly:[UI NAME];help:[HELP TEXT];shortcut:[JSON FIELD PATH or CSV column number]' },
          { name: 'view', required: false, multiline: '\\n', help: 'The view to show in session detail when opening up a session with unique fields, this also may be done in the input file. The value for view can either be written in simplified format or in more powerful pug format. For the pug format see Tagger Format in the docs for more information. Simple format looks like require:[toplevel db name];title:[title string];fields:[field1],[field2],[fieldN]' }
        ]);
      }

      internals.configDefs[sourceName] = configDef;
    }
  }

  // ----------------------------------------------------------------------------
  /**
   * Create a redis client from the provided url
   * @params {string} url - The redis url to connect to.
   * @params {string} section - The section this redis client is being created for
   */
  createRedisClient (url, section) {
    return createRedisClient(url, section);
  }

  // ----------------------------------------------------------------------------
  /**
   * Create a memcached client from the provided url
   * @params {string} url - The memcached url to connect to.
   * @params {string} section - The section this memcached client is being created for
   */
  createMemcachedClient (url, section) {
    return createMemcachedClient(url, section);
  }

  // ----------------------------------------------------------------------------
  /**
   * Define all configuration for a field for a source
   * @typedef {Object} WISESourceAPI~ValueAction
   * @property {string} name - The name of the value action to show the user
   * @property {string} [url] - The url to send the user, supports special subsitutions, must set url or func
   * @property {string} [func] - A javascript function body to call, will be passed the name and value and must return the value, must set url or func
   * @property {string} [actionType] - If set to 'fetch' this will replace the menu option with the results of url or func
   * @property {string} [category] - Which category of fields should the value action be shown for, must set fields or category
   * @property {string} [fields] - Which fields to show the value action for, must set fields or category
   * @property {string} [regex] - When set replaces %REGEX% in the url with the match
   */

  /**
   * Add a value action set
   * @params {string} name - The globally unique name of this action, not shown to user
   * @params {WISESourceAPI~ValueAction} action - The action
   */
  addValueAction (name, action) {
    internals.valueActions[name] = action;
  }

  isWebConfig () {
    return internals.webconfig;
  }

  funcName (typeName) {
    return funcName(typeName);
  }
}
// ----------------------------------------------------------------------------
function loadSources () {
  glob(getConfig('wiseService', 'sourcePath', path.join(__dirname, '/')) + 'source.*.js', (err, files) => {
    files.forEach((file) => {
      const src = require(file);
      src.initSource(internals.sourceApi);
    });
  });

  // ALW - should really merge all the types somehow here instead of type2Name
  for (let i = 0; i < internals.type2Name.length; i++) {
    const type = internals.type2Name[i];
    let excludeName;
    if (type === 'url') {
      excludeName = 'excludeURLs';
    } else if (type === 'ip') {
      internals.configDefs.wiseService.fields = internals.configDefs.wiseService.fields.concat(
        [{ name: 'excludeIPs', required: false, multiline: ';', help: 'List of IPs or CIDRs to exclude in lookups across ALL WISE sources' }]
      );
      continue;
    } else {
      excludeName = 'exclude' + type[0].toUpperCase() + type.slice(1) + 's';
    }

    internals.configDefs.wiseService.fields = internals.configDefs.wiseService.fields.concat(
      [{ name: excludeName, required: false, multiline: ';', help: 'List of modified glob patterns to exclude in lookups across ALL WISE sources' }]
    );
  }
}

// ----------------------------------------------------------------------------
// APIs
// ----------------------------------------------------------------------------
app.use(logger(':date \x1b[1m:method\x1b[0m \x1b[33m:url\x1b[0m :res[content-length] bytes :response-time ms'));
app.use(timeout(5 * 1000));

// Serve vue app
app.get(['/', '/config', '/statistics'], (req, res, next) => {
  res.sendFile(path.join(__dirname, '/vueapp/dist/index.html'));
});
app.use(favicon(path.join(__dirname, '/favicon.ico')));

// expose vue bundles (prod)
app.use('/static', express.static(path.join(__dirname, '/vueapp/dist/static')));
app.use('/app.css', express.static(path.join(__dirname, '/vueapp/dist/app.css')));

// expose vue bundle (dev)
app.use(['/app.js', '/vueapp/app.js'], express.static(path.join(__dirname, '/vueapp/dist/app.js')));
app.use(['/app.js.map', '/vueapp/app.js.map'], express.static(path.join(__dirname, '/vueapp/dist/app.js.map')));
app.use('/font-awesome', express.static(path.join(__dirname, '/../node_modules/font-awesome'), { maxAge: 600 * 1000 }));
app.use('/assets', express.static(path.join(__dirname, '/../assets'), { maxAge: 600 * 1000 }));

// ----------------------------------------------------------------------------
if (internals.regressionTests) {
  app.post('/shutdown', (req, res) => {
    process.exit(0);
  });
}
// ----------------------------------------------------------------------------
/**
 * GET - Health check URL
 *
 * @name "/_ns_/nstest.html"
 */
app.get('/_ns_/nstest.html', [noCacheJson], (req, res) => {
  res.end();
});
// ----------------------------------------------------------------------------
/**
 * GET - Used by capture to retrieve all the fields created by wise sources
 *
 * @name "/fields"
 * @param {integer} [ver=0] - Version of the encoded binary to return
 * @returns {binary}
 */
app.get('/fields', [noCacheJson], (req, res) => {
  if (req.query.ver === undefined || req.query.ver === '0') {
    if (internals.fields.length < 256) {
      res.send(internals.fieldsBuf0);
    } else {
      console.log("ERROR - This wise server has more then 255 fields, it can't be used with older moloch");
      return res.status(404).end();
    }
  } else {
    res.send(internals.fieldsBuf1);
  }
});
// ----------------------------------------------------------------------------
/**
 * GET - Used by viewer to retrieve all the views being created by wise sources
 *
 * @name "/views"
 * @returns {object} All the views
 */
app.get('/views', [noCacheJson], function (req, res) {
  res.send(internals.views);
});
// ----------------------------------------------------------------------------
/**
 * GET - Used by viewer to retrieve all the field value actions created by wise sources
 *
 * @name "/valueActions"
 * @returns {object|array} All the actions
 */
app.get(['/rightClicks', '/valueActions'], [noCacheJson], function (req, res) {
  res.send(internals.valueActions);
});

// ----------------------------------------------------------------------------
function globalAllowed (value) {
  for (let i = 0; i < this.excludes.length; i++) {
    if (value.match(this.excludes[i])) {
      if (internals.debug > 0) {
        console.log(`Found in Global ${this.name} Exclude`, value);
      }
      return false;
    }
  }
  return true;
}
// ----------------------------------------------------------------------------
function globalIPAllowed (value) {
  if (this.excludes.find(value)) {
    if (internals.debug > 0) {
      console.log('Found in Global IP Exclude', value);
    }
    return false;
  }
  return true;
}
// ----------------------------------------------------------------------------
function sourceAllowed (src, value) {
  const excludes = src[this.excludeName] || [];
  for (let i = 0; i < excludes.length; i++) {
    if (value.match(excludes[i])) {
      if (internals.debug > 0) {
        console.log('Found in', src.section, this.name, 'exclude', value);
      }
      return false;
    }
  }
  return true;
}
// ----------------------------------------------------------------------------
function sourceIPAllowed (src, value) {
  if (src.excludeIPs.find(value)) {
    if (internals.debug > 0) {
      console.log('Found in', src.section, 'IP Exclude', value);
    }
    return false;
  }
  if (src.onlyIPs && !src.onlyIPs.find(value)) {
    return false;
  }
  return true;
}
// ----------------------------------------------------------------------------
function funcName (typeName) {
  if (typeName === 'url') {
    return 'getURL';
  }

  return 'get' + typeName[0].toUpperCase() + typeName.slice(1);
}
// ----------------------------------------------------------------------------
// This function adds a new type to the internals.types map of types.
// If newSrc is defined will add it to already defined types as src to query.
function addType (type, newSrc) {
  let typeInfo = internals.types[type];
  if (!typeInfo) {
    typeInfo = internals.types[type] = {
      name: type,
      excludeName: 'exclude' + type[0].toUpperCase() + type.slice(1) + 's',
      funcName: funcName(type),
      sources: [],
      requestStats: 0,
      foundStats: 0,
      cacheHitStats: 0,
      cacheSrcHitStats: 0,
      cacheSrcMissStats: 0,
      cacheSrcRefreshStats: 0,
      excludes: [],
      globalAllowed: globalAllowed,
      sourceAllowed: sourceAllowed
    };

    if (type === 'url') {
      typeInfo.excludeName = 'excludeURLs';
    }

    if (type === 'ip') {
      typeInfo.excludeName = 'excludeIPs';
      typeInfo.globalAllowed = globalIPAllowed;
      typeInfo.sourceAllowed = sourceIPAllowed;
    }

    for (const src in internals.sources) {
      if (internals.sources[src][typeInfo.funcName]) {
        typeInfo.sources.push(internals.sources[src]);
        internals.sources[src].srcInProgress[type] = [];
      }
    }

    const items = getConfig('wiseService', typeInfo.excludeName, '');
    if (type === 'ip') {
      typeInfo.excludes = new iptrie.IPTrie();
      items.split(';').map(item => item.trim()).filter(item => item !== '').forEach((item) => {
        const parts = item.split('/');
        try {
          typeInfo.excludes.add(parts[0], +parts[1] || (parts[0].includes(':') ? 128 : 32), true);
        } catch (e) {
          console.log(`Error for '${item}'`, e);
          process.exit();
        }
      });
    } else {
      typeInfo.excludes = items.split(';').map(item => item.trim()).filter(item => item !== '').map(item => RegExp.fromWildExp(item, 'ailop'));
    }
  } else if (newSrc !== undefined) {
    typeInfo.sources.push(newSrc);
    newSrc.srcInProgress[type] = [];
  }
  return typeInfo;
}
// ----------------------------------------------------------------------------
function processQuery (req, query, cb) {
  let typeInfo = internals.types[query.typeName];

  // First time we've seen this typeName
  if (!typeInfo) {
    typeInfo = addType(query.typeName);
  }

  typeInfo.requestStats++;

  // md5/sha256 have content type
  if (query.typeName === 'md5' || query.typeName === 'sha256') {
    const parts = query.value.split(';');
    query.value = parts[0];
    query.contentType = parts[1];
  }

  // Check if globally allowed
  try {
    if (!typeInfo.globalAllowed(query.value)) {
      return cb(null, WISESource.emptyResult);
    }
  } catch (e) {
    console.log('ERROR', query.typeName, query.value, e);
  }

  // Fetch the cache for this query
  internals.cache.get(query, (err, cacheResult) => {
    if (req.timedout) {
      return cb('Timed out ' + query.typeName + ' ' + query.value);
    }

    const now = Math.floor(Date.now() / 1000);

    let cacheChanged = false;
    if (cacheResult === undefined) {
      cacheResult = {};
    } else {
      typeInfo.cacheHitStats++;
    }

    async.map(query.sources || typeInfo.sources, (src, cb) => {
      if (!typeInfo.sourceAllowed(src, query.value)) {
        // This source isn't allowed for query
        return setImmediate(cb, undefined);
      }

      src.requestStat++;
      if (cacheResult[src.section] === undefined || cacheResult[src.section].ts + src.cacheTimeout < now) {
        if (src.cacheTimeout === -1) {
          // Don't count as hit or miss
        } else if (cacheResult[src.section] === undefined) {
          src.cacheMissStat++;
          typeInfo.cacheSrcMissStats++;
        } else {
          src.cacheRefreshStat++;
          typeInfo.cacheSrcRefreshStats++;
        }

        // Can't use the cache or there is no cache for this source
        delete cacheResult[src.section];

        // If already in progress then add to the list and return, cb called later;
        if (query.value in src.srcInProgress[query.typeName]) {
          src.srcInProgress[query.typeName][query.value].push(cb);
          return;
        }

        // First query for this value
        src.srcInProgress[query.typeName][query.value] = [cb];
        const startTime = Date.now();
        src[typeInfo.funcName](src.fullQuery === true ? query : query.value, (err, result) => {
          src.recentAverageMS = (999.0 * src.recentAverageMS + (Date.now() - startTime)) / 1000.0;

          if (!err && result !== undefined) {
            src.directHitStat++;
            if (src.cacheTimeout !== -1) { // If err or cacheTimeout is -1 then don't cache
              cacheResult[src.section] = { ts: now, result: result };
              cacheChanged = true;
            }
          }
          if (err === 'dropped') {
            src.requestDroppedStat++;
            err = null;
            result = undefined;
          }
          const srcInProgress = src.srcInProgress[query.typeName][query.value];
          delete src.srcInProgress[query.typeName][query.value];
          for (let i = 0, l = srcInProgress.length; i < l; i++) {
            srcInProgress[i](err, result);
          }
        });
      } else {
        src.cacheHitStat++;
        typeInfo.cacheSrcHitStats++;
        // Woot, we can use the cache
        setImmediate(cb, null, cacheResult[src.section].result);
      }
    }, (err, results) => {
      // Combine all the results together
      if (err) {
        return cb(err);
      }
      if (internals.debug > 2) {
        console.log('RESULT', typeInfo.funcName, query.value, WISESource.result2JSON(WISESource.combineResults(results)));
      }

      if (req.timedout) {
        cb('Timed out ' + query.typeName + ' ' + query.value);
      } else {
        cb(null, WISESource.combineResults(results));
      }

      // Need to update the cache
      if (cacheChanged) {
        internals.cache.set(query, cacheResult);
      }
    });
  });
}
// ----------------------------------------------------------------------------
function processQueryResponse0 (req, res, queries, results) {
  const buf = Buffer.allocUnsafe(8);
  buf.writeUInt32BE(internals.fieldsTS, 0);
  buf.writeUInt32BE(0, 4);
  res.write(buf);
  for (let r = 0; r < results.length; r++) {
    if (results[r][0] > 0) {
      internals.types[queries[r].typeName].foundStats++;
    }
    res.write(results[r]);
  }
  res.end();
}
// ----------------------------------------------------------------------------
//
function processQueryResponse2 (req, res, queries, results) {
  const hashes = (req.query.hashes || '').split(',');

  const sendFields = !hashes.includes(internals.fieldsMd5);

  const buf = Buffer.allocUnsafe(42);
  buf.writeUInt32BE(0, 0);
  buf.writeUInt32BE(2, 4);
  buf.write(internals.fieldsMd5, 8);

  if (sendFields) {
    // Send all the fields
    res.write(buf.slice(0, 40));
    res.write(internals.fieldsBuf1.slice(8));
  } else {
    // Don't send the fields
    buf.writeUInt16BE(0, 40);
    res.write(buf);
  }

  // Send the results
  for (let r = 0; r < results.length; r++) {
    if (results[r][0] > 0) {
      internals.types[queries[r].typeName].foundStats++;
    }
    res.write(results[r]);
  }
  res.end();
}
// ----------------------------------------------------------------------------
/**
 * POST - Used by capture to lookup all the wise items
 *
 * @name "/get"
 * @param {integer} ver=0 - The format of the post data, version 0 and 2 supported
 * @param {string|array} hashes - A comma separated list of md5 hashes of field arrays that the client knows about.
 *                                If one of the hashes matches the current field array, then we don't send the field array.
 * @returns {binary} The encoded results
 */
app.post('/get', function (req, res) {
  let offset = 0;

  const buffers = [];
  req.on('data', (chunk) => {
    buffers.push(chunk);
  }).once('end', (err) => {
    const queries = [];
    try {
      for (const buf = Buffer.concat(buffers); offset < buf.length;) {
        const type = buf[offset];
        offset++;

        let typeName;
        if (type & 0x80) {
          typeName = buf.toString('utf8', offset, offset + (type & ~0x80));
          offset += (type & ~0x80);
        } else {
          typeName = internals.type2Name[type];
        }

        const len = buf.readUInt16BE(offset);
        offset += 2;

        const value = buf.toString('utf8', offset, offset + len);
        if (internals.debug > 1) {
          console.log(typeName, value);
        }
        offset += len;
        queries.push({ typeName: typeName, value: value });
      }
    } catch (err) {
      return res.end('Received malformed packet');
    }

    async.map(queries, (query, cb) => {
      processQuery(req, query, cb);
    }, (err, results) => {
      if (err || req.timedout) {
        console.log('Error', err || 'Timed out');
        return;
      }

      if (req.query.ver === '2') {
        processQueryResponse2(req, res, queries, results);
      } else {
        processQueryResponse0(req, res, queries, results);
      }
    });
  });
});
// ----------------------------------------------------------------------------
/**
 * GET - Used by wise UI to retrieve all the sources
 *
 * @name "/sources"
 * @returns {string|array} All the sources
 */
app.get('/sources', [noCacheJson], (req, res) => {
  return res.send(Object.keys(internals.sources).sort());
});
// ----------------------------------------------------------------------------
/**
 * GET - Used by wise UI to retrieve the raw file being used by the section.
 *       This is an authenticated API and requires wiseService to be started with --webconfig.
 *
 * @name "/source/:source/get"
 * @param {string} :source - The source to get the raw data for
 * @returns {object} All the views
 */
app.get('/source/:source/get', [isConfigWeb, doAuth, noCacheJson], (req, res) => {
  const source = internals.sources[req.params.source];
  if (!source) {
    return res.send({ success: false, text: `Source ${req.params.source} not found` });
  }

  if (!source.getSourceRaw) {
    return res.send({ success: false, text: 'Source does not support viewing' });
  }

  source.getSourceRaw((err, raw) => {
    if (err) {
      return res.send({ success: false, text: err });
    }
    return res.send({ success: true, raw: raw.toString('utf8') });
  });
});
// ----------------------------------------------------------------------------
/**
 * PUT - Used by wise UI to save the raw file being used by the source.
 *       This is an authenticated API and requires wiseService to be started with --webconfig.
 *
 * @name "/source/:source/put"
 * @param {string} :source - The source to put the raw data for
 * @returns {object} All the views
 */
app.put('/source/:source/put', [isConfigWeb, doAuth, noCacheJson, checkAdmin, jsonParser], (req, res) => {
  const source = internals.sources[req.params.source];
  if (!source) {
    return res.send({ success: false, text: `Source ${req.params.source} not found` });
  }

  if (!source.putSourceRaw) {
    return res.send({ success: false, text: 'Source does not support editing' });
  }

  const raw = req.body.raw;

  source.putSourceRaw(raw, (err) => {
    if (err) {
      return res.send({ success: false, text: err });
    }
    return res.send({ success: true, text: 'Saved' });
  });
});
// ----------------------------------------------------------------------------
/**
 * GET - Used by wise UI to retrieve all the configuration definitions for the various sources.
 *
 * @name "/config/defs"
 * @returns {object}
 */
app.get('/config/defs', [noCacheJson], function (req, res) {
  return res.send(internals.configDefs);
});
// ----------------------------------------------------------------------------
/**
 * GET - Used by wise UI to retrieve the current config.
 *       This is an authenticated API and requires wiseService to be started with --webconfig.
 *
 * @name "/config/get"
 * @returns {object}
 */
app.get('/config/get', [isConfigWeb, doAuth, noCacheJson], (req, res) => {
  const config = Object.keys(internals.config)
    .filter(key => internals.configDefs[key.split(':')[0]])
    .reduce((obj, key) => {
      // Deep Copy
      obj[key] = JSON.parse(JSON.stringify(internals.config[key]));

      // Replace passwords
      internals.configDefs[key.split(':')[0]].fields.forEach((item) => {
        if (item.password !== true) { return; }
        if (obj[key][item.name] === undefined || obj[key][item.name].length === 0) { return; }
        obj[key][item.name] = '********';
      });
      return obj;
    }, {});

  return res.send({
    success: true,
    config: config,
    filePath: internals.configFile
  });
});
// ----------------------------------------------------------------------------
/**
 * PUT - Used by wise UI to save the current config.
 *       This is an authenticated API, requires the pin code, and requires wiseService to be started with --webconfig.
 *
 * @name "/config/save"
 */
app.put('/config/save', [isConfigWeb, doAuth, noCacheJson, checkAdmin, jsonParser, checkConfigCode], (req, res) => {
  if (req.body.config === undefined) {
    return res.send({ success: false, text: 'Missing config' });
  }

  const config = req.body.config;
  if (internals.debug > 0) {
    console.log(config);
  }

  for (const section in config) {
    const sectionType = section.split(':')[0];
    const configDef = internals.configDefs[sectionType];
    if (configDef === undefined) {
      return res.send({ success: false, text: `Unknown section type ${sectionType}` });
    }
    if (configDef.singleton !== true && sectionType === section) {
      return res.send({ success: false, text: `Section ${section} must have a :uniquename` });
    }
    if (configDef.singleton === true && sectionType !== section) {
      return res.send({ success: false, text: `Section ${section} must not have a :uniquename` });
    }

    // Create new source files
    if (configDef.editable && config[section].file && !fs.existsSync(config[section].file)) {
      try {
        fs.writeFileSync(config[section].file, '');
      } catch (e) {
        return res.send({ success: false, text: 'New file could not be written to system' });
      }
    }

    for (const key in config[section]) {
      const field = configDef.fields.find(element => element.name === key);
      if (field === undefined) {
        return res.send({ success: false, text: `Section ${section} field ${key} unknown` });
      }
      if (field.password === true) {
        if (config[section][key] === '********') {
          config[section][key] = internals.config[section][key];
        }
      }
    };
  }

  internals.configScheme.save(req.body.config, (err) => {
    if (err) {
      return res.send({ success: false, text: err });
    } else {
      res.send({ success: true, text: 'Saved & Restarting' });
      setTimeout(() => { process.kill(process.pid, 'SIGUSR2'); }, 500);
      setTimeout(() => { process.exit(0); }, 1500);
    }
  });
});
// ----------------------------------------------------------------------------
/**
 * GET - Used by the wise UI to all the types known.
 *
 * @name "/types"
 * @returns {string|array} - all the types
 */
/**
 * GET - Used by the wise UI to retrieve all the types for a source, or if no source
 *       all the types known.
 *
 * @name "/types/:source"
 * @param {string} {:source} - the source to get the types for
 * @returns {string|array} - all the types for the source
 */
app.get('/types/:source?', [noCacheJson], (req, res) => {
  if (req.params.source) {
    if (internals.sources[req.params.source]) {
      return res.send(internals.sources[req.params.source].types.sort());
    } else {
      return res.send([]);
    }
  } else {
    return res.send(Object.keys(internals.types).sort());
  }
});
// ----------------------------------------------------------------------------
/**
 * GET - Query a single source for a key
 *
 * @name "/:source/:type/:key"
 * @param {string} {:source} - The source to get the results for
 * @param {string} {:type} - The type of the key
 * @param {string} {:key} - The key to get the results for
 * @returns {object|array} - The results for the query
 */
app.get('/:source/:typeName/:value', [noCacheJson], function (req, res) {
  const source = internals.sources[req.params.source];
  if (!source) {
    return res.end('Unknown source ' + req.params.source);
  }

  const query = {
    typeName: req.params.typeName,
    value: req.params.value,
    sources: [source]
  };

  processQuery(req, query, (err, result) => {
    if (err || !result) {
      return res.end('Not found');
    }
    res.send(WISESource.result2JSON(result));
  });
});
// ----------------------------------------------------------------------------
app.get('/dump/:source', [noCacheJson], function (req, res) {
  const source = internals.sources[req.params.source];
  if (!source) {
    return res.end('Unknown source ' + req.params.source);
  }

  if (!source.dump) {
    return res.end("The source doesn't support dump");
  }

  source.dump(res);
});
// ----------------------------------------------------------------------------
// ALW - Need to rewrite to use performQuery
/*
app.get("/bro/:type", [noCacheJson], function(req, res) {
  let hashes = req.query.items.split(",");
  let needsep = false;

  let fn = internals.type2Func[req.params.type];
  let srcs = internals[fn + "s"];
  async.map(hashes, (hash, doneCb) => {
    async.map(srcs, (src, cb) => {
      if (internals.source_allowed[req.params.type](src, hash)) {
        src[fn](hash, cb);
      } else {
        setImmediate(cb, undefined);
      }
    }, (err, results) => {
      doneCb(null, results);
    });
  },
  (err, results) => {

    for (let hashi = 0; hashi < hashes.length; hashi++) {
      if (hashi !== 0) {
        res.write("\tBRONEXT\t");
      }
      res.write(hashes[hashi]);
      res.write("\tBROIS\t");
      let resulti, found = false;
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
        let offset = 1;
        let buffer = results[hashi][resulti];
        for (let n = 0; n < buffer[0]; n++) {
          if (n !== 0) {
            res.write(" ");
          }
          let pos = buffer[offset++];
          let len = buffer[offset++];
          let value = buffer.toString('utf8', offset, offset+len-1);
          offset += len;
          res.write(WISESource.pos2Field[pos] + ": " + value);
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
// ----------------------------------------------------------------------------
/**
 * GET - Query all sources for a key
 *
 * @name "/:type/:key"
 * @param {string} {:type} - The type of the key
 * @param {string} {:key} - The key to get the results for
 * @returns {object|array} - The results for the query
 */
app.get('/:typeName/:value', [noCacheJson], function (req, res) {
  const query = {
    typeName: req.params.typeName,
    value: req.params.value
  };

  processQuery(req, query, (err, result) => {
    if (err || !result) {
      return res.end('Not found');
    }
    res.send(WISESource.result2JSON(result));
  });
});
// ----------------------------------------------------------------------------
/**
 * GET - Query for the stats
 *
 * @name "/stats"
 * @returns {object} - Object with array of stats per type and array of stats per source
 */
app.get('/stats', [noCacheJson], function (req, res) {
  const types = Object.keys(internals.types).sort();
  const sections = Object.keys(internals.sources).sort();

  const stats = { types: [], sources: [], startTime: internals.startTime };

  for (const type of types) {
    const typeInfo = internals.types[type];
    stats.types.push({
      type: type,
      request: typeInfo.requestStats,
      found: typeInfo.foundStats,
      cacheHit: typeInfo.cacheHitStats,
      cacheSrcHit: typeInfo.cacheSrcHitStats,
      cacheSrcMiss: typeInfo.cacheSrcMissStats,
      cacheSrcRefresh: typeInfo.cacheSrcRefreshStats
    });
  }

  for (const section of sections) {
    const src = internals.sources[section];
    stats.sources.push({
      source: section,
      request: src.requestStat,
      cacheHit: src.cacheHitStat,
      cacheMiss: src.cacheMissStat,
      cacheRefresh: src.cacheRefreshStat,
      directHit: src.directHitStat,
      requestDropped: src.requestDroppedStat,
      recentAverageMS: src.recentAverageMS.toFixed(4),
      items: src.itemCount()
    });
  }
  res.send(stats);
});

// ----------------------------------------------------------------------------
function createRedisClient (url, section) {
  // redis://[:pass]@host:port/db
  if (url.startsWith('redis://') || url.startsWith('rediss://')) {
    const match = url.match(/(rediss?):\/\/(:[^@]+@)?([^:/]+)(:[0-9]+)?\/([0-9]+)/);
    if (!match) {
      console.log(`${section} - ERROR - can't parse redis url '${url}' should be of form //[:pass@]redishost[:redisport]/redisDbNum`);
      process.exit(1);
    }

    if (internals.debug > 0) {
      console.log('REDIS:', url);
    }
    return new Redis(url);
  }

  // redis-sentinel://sentinelPassword:redisPassword@host:port[,hostN;portN]/name/db
  if (url.startsWith('redis-sentinel://')) {
    const match = url.match(/(redis-sentinel):\/\/(([^:]+)?:([^@]+)?@)?([^/]+)\/([^/]+)\/([0-9]+)(\/.+)?/);
    if (!match) {
      console.log(`${section} - ERROR - can't parse redis-sentinel url '${url}' should be of form //[sentinelPassword:redisPassword@]sentinelHost[:sentinelPort][,sentinelPortN[:sentinelPortN]]/redisName/redisDbNum`);
      process.exit(1);
    }

    const options = { sentinels: [], name: match[6], db: parseInt(match[7]) };
    match[5].split(',').forEach((hp) => {
      const hostport = hp.split(':');
      options.sentinels.push({ host: hostport[0], port: hostport[1] || 26379 });
    });

    if (match[3] && match[3] !== '') {
      options.sentinelPassword = match[3];
    }
    if (match[4] && match[4] !== '') {
      options.password = match[4];
    }

    if (internals.debug > 0) {
      console.log('REDIS-SENTINEL:', options);
    }
    return new Redis(options);
  }

  // redis-cluster://[:pass]@host:port/db
  if (url.startsWith('redis-cluster://')) {
    const match = url.match(/(redis-cluster):\/\/(:([^@]+)@)?([^/]+)\/([0-9]+)(\/.+)?/);
    if (!match) {
      console.log(`${section} - ERROR - can't parse redis-cluster url '${url}' should be of form //[:redisPassword@]redisHost[:redisPort][,redisHostN[:redisPortN]]/redisDbNum`);
      process.exit(1);
    }

    const hosts = [];
    match[4].split(',').forEach((hp) => {
      const hostport = hp.split(':');
      hosts.push({ host: hostport[0], port: hostport[1] || 6379 });
    });

    const options = { db: parseInt(match[5]) };
    if (match[3] && match[3] !== '') {
      options.password = match[3];
    }

    if (internals.debug > 0) {
      console.log('REDIS-CLUSTER: hosts', hosts, 'options', { redisOptions: options });
    }
    return new Redis.Cluster(hosts, { redisOptions: options });
  }

  console.log(`Unknown redis url '${url}'`);
  process.exit(1);
}

// ----------------------------------------------------------------------------
function createMemcachedClient (url, section) {
  // memcached://[user:pass@]server1[:11211],[user:pass@]server2[:11211],...
  if (url.startsWith('memcached://')) {
    if (internals.debug > 0) {
      console.log('MEMCACHED:', url);
    }
    return memjs.Client.create(url.substring(12));
  }

  console.log(`Unknown memcached url '${url}'`);
  process.exit(1);
}

// ----------------------------------------------------------------------------
function printStats () {
  const keys = Object.keys(internals.types).sort();
  const lines = [];
  lines[0] = '                   ';
  lines[1] = 'REQUESTS:          ';
  lines[2] = 'FOUND:             ';
  lines[3] = 'CACHE HIT:         ';
  lines[4] = 'CACHE SRC HIT:     ';
  lines[5] = 'CACHE SRC REFRESH: ';

  for (const key of keys) {
    const typeInfo = internals.types[key];
    lines[0] += sprintf(' %11s', key);
    lines[1] += sprintf(' %11d', typeInfo.requestStats);
    lines[2] += sprintf(' %11d', typeInfo.foundStats);
    lines[3] += sprintf(' %11d', typeInfo.cacheHitStats);
    lines[4] += sprintf(' %11d', typeInfo.cacheSrcHitStats);
    lines[5] += sprintf(' %11d', typeInfo.cacheSrcRefreshStats);
  }

  for (let i = 0; i < lines.length; i++) {
    console.log(lines[i]);
  }

  for (const section in internals.sources) {
    const src = internals.sources[section];
    console.log(sprintf('SRC %-30s    cached: %7d lookup: %9d refresh: %7d dropped: %7d avgMS: %7d',
      section, src.cacheHitStat, src.cacheMissStat, src.cacheRefreshStat, src.requestDroppedStat, src.recentAverageMS));
  }
}

// ----------------------------------------------------------------------------
// Error handling
app.use((req, res, next) => {
  res.status(404).send('Not found');
});

// ----------------------------------------------------------------------------
// jPaq
// ----------------------------------------------------------------------------
/*
 jPaq - A fully customizable JavaScript/JScript library
 http://jpaq.org/

 Copyright (c) 2011 Christopher West
 Licensed under the MIT license.
 http://jpaq.org/license/

 Version: 1.0.6.000m
 Revised: April 6, 2011
*/
/* eslint-disable */
RegExp.fromWildExp=function(c,a){for(var d=a&&a.indexOf("o")>-1,f,b,e="",g=a&&a.indexOf("l")>-1?"":"?",h=RegExp("~.|\\[!|"+(d?"{\\d+,?\\d*\\}|[":"[")+(a&&a.indexOf("p")>-1?"":"\\(\\)")+"\\{\\}\\\\\\.\\*\\+\\?\\:\\|\\^\\$%_#<>]");(f=c.search(h))>-1&&f<c.length;)e+=c.substring(0,f),e+=(b=c.match(h)[0])=="[!"?"[^":b.charAt(0)=="~"?"\\"+b.charAt(1):b=="*"||b=="%"?".*"+g:
b=="?"||b=="_"?".":b=="#"?"\\d":d&&b.charAt(0)=="{"?b+g:b=="<"?"\\b(?=\\w)":b==">"?"(?:\\b$|(?=\\W)\\b)":"\\"+b,c=c.substring(f+b.length);e+=c;a&&(/[ab]/.test(a)&&(e="^"+e),/[ae]/.test(a)&&(e+="$"));return RegExp(e,a?a.replace(/[^gim]/g,""):"")};
/* eslint-enable */

// ----------------------------------------------------------------------------
// Config Schemes - For each scheme supported implement a load/save function
// ----------------------------------------------------------------------------

// redis://[:pass]@host:port/db/key
internals.configSchemes.redis = {
  load: function (cb) {
    const redisParts = internals.configFile.split('/');
    if (redisParts.length !== 5) {
      throw new Error(`Invalid redis url - ${redisParts[0]}//[:pass@]redishost[:redisport]/redisDbNum/key`);
    }
    internals.configRedisKey = redisParts.pop();
    internals.configRedis = createRedisClient(redisParts.join('/'), 'config');

    internals.configRedis.get(internals.configRedisKey, function (err, result) {
      if (err) {
        return cb(err);
      }
      if (result === null) {
        internals.config = {};
      } else {
        internals.config = JSON.parse(result);
      }
      return cb();
    });
  },
  save: function (config, cb) {
    internals.configRedis.set(internals.configRedisKey, JSON.stringify(config), function (err, result) {
      cb(err);
    });
  }
};

// ----------------------------------------------------------------------------
// rediss://pass@host:port/db/key
internals.configSchemes.rediss = internals.configSchemes.redis;

// redis-sentinel://sentinelPassword:redisPassword@host:port/name/db/key
internals.configSchemes['redis-sentinel'] = {
  load: function (cb) {
    const redisParts = internals.configFile.split('/');
    redisParts[1] = 'stoperror';
    if (redisParts.length !== 6 || redisParts.some(p => p === '')) {
      throw new Error(`Invalid redis-sentinel url - ${redisParts[0]}//[sentinelPassword:redisPassword@]sentinelHost[:sentinelPort][,sentinelPortN:sentinelPortN]/redisName/redisDbNum`);
    }
    internals.configRedisKey = redisParts[5];
    internals.configRedis = createRedisClient(internals.configFile, 'config');

    internals.configRedis.get(internals.configRedisKey, function (err, result) {
      if (err) {
        return cb(err);
      }
      if (result === null) {
        internals.config = {};
      } else {
        internals.config = JSON.parse(result);
      }
      return cb();
    });
  },
  save: function (config, cb) {
    internals.configRedis.set(internals.configRedisKey, JSON.stringify(config), function (err, result) {
      cb(err);
    });
  }
};

// ----------------------------------------------------------------------------
// redis-cluster://[:pass]@host:port/db/key
internals.configSchemes['redis-cluster'] = {
  load: function (cb) {
    const redisParts = internals.configFile.split('/');
    redisParts[1] = 'stoperror';
    if (redisParts.length !== 5 || redisParts.some(p => p === '')) {
      throw new Error(`Invalid redis-cluster url - ${redisParts[0]}//[:redisPassword@]redishost[:redisport]/redisDbNum/key`);
    }
    internals.configRedisKey = redisParts[4];
    internals.configRedis = createRedisClient(internals.configFile, 'config');

    internals.configRedis.get(internals.configRedisKey, function (err, result) {
      if (err) {
        return cb(err);
      }
      if (result === null) {
        internals.config = {};
      } else {
        internals.config = JSON.parse(result);
      }
      return cb();
    });
  },
  save: function (config, cb) {
    internals.configRedis.set(internals.configRedisKey, JSON.stringify(config), function (err, result) {
      cb(err);
    });
  }
};

// ----------------------------------------------------------------------------
internals.configSchemes.elasticsearch = {
  load: function (cb) {
    const url = internals.configFile.replace('elasticsearch', 'http');
    if (!url.includes('/_doc/')) {
      throw new Error('Missing _doc in url, should be format elasticsearch://user:pass@host:port/INDEX/_doc/DOC');
    }

    axios.get(url)
      .then((response) => {
        internals.config = response.data._source;
        cb(null);
      })
      .catch((error) => {
        if (error.response && error.response.status === 404) {
          internals.config = {};
          return cb();
        }
        return cb(error);
      });
  },
  save: function (config, cb) {
    const url = internals.configFile.replace('elasticsearch', 'http');

    axios.post(url, JSON.stringify(config))
      .then((response) => {
        cb(null);
      })
      .catch((error) => {
        cb(error);
      });
  }
};

// ----------------------------------------------------------------------------
internals.configSchemes.elasticsearchs = {
  load: function (cb) {
    const url = internals.configFile.replace('elasticsearchs', 'https');
    if (!url.includes('/_doc/')) {
      throw new Error('Missing _doc in url, should be format elasticsearch://user:pass@host:port/INDEX/_doc/DOC');
    }

    axios.get(url)
      .then((response) => {
        internals.config = response.data._source;
        cb(null);
      })
      .catch((error) => {
        if (error.response && error.response.status === 404) {
          internals.config = {};
          return cb();
        }
        return cb(error);
      });
  },
  save: function (config, cb) {
    const url = internals.configFile.replace('elasticsearchs', 'https');

    axios.post(url, JSON.stringify(config))
      .then((response) => {
        cb();
      })
      .catch((error) => {
        cb(error);
      });
  }
};

// ----------------------------------------------------------------------------
internals.configSchemes.json = {
  load: function (cb) {
    internals.config = JSON.parse(fs.readFileSync(internals.configFile, 'utf8'));
    return cb();
  },
  save: function (config, cb) {
    try {
      fs.writeFileSync(internals.configFile, JSON.stringify(config, null, 1));
      cb();
    } catch (e) {
      cb(e.message);
    }
  }
};

// ----------------------------------------------------------------------------
internals.configSchemes.ini = {
  load: function (cb) {
    internals.config = ini.parseSync(internals.configFile);
    return cb();
  },
  save: function (config, cb) {
    function encode (str) {
      return str.replace(/[\n\r]/g, '\\n');
    }
    let output = '';
    Object.keys(config).forEach((section) => {
      output += `[${encode(section)}]\n`;
      Object.keys(config[section]).forEach((key) => {
        output += `${key}=${encode(config[section][key])}\n`;
      });
    });

    try {
      fs.writeFileSync(internals.configFile, output);
      cb(null);
    } catch (e) {
      cb(e.message);
    }
  }
};

// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------
function main () {
  internals.cache = wiseCache.createCache({
    getConfig: getConfig,
    createRedisClient: createRedisClient,
    createMemcachedClient: createMemcachedClient
  });

  internals.sourceApi = new WISESourceAPI();
  internals.sourceApi.addField('field:tags'); // Always add tags field so we have at least 1 field
  loadSources();

  if (internals.debug > 0) {
    setInterval(printStats, 60 * 1000);
  }

  let server;
  if (getConfig('wiseService', 'keyFile') && getConfig('wiseService', 'certFile')) {
    const keyFileData = fs.readFileSync(getConfig('wiseService', 'keyFile'));
    const certFileData = fs.readFileSync(getConfig('wiseService', 'certFile'));

    server = https.createServer({ key: keyFileData, cert: certFileData, secureOptions: crypto.constants.SSL_OP_NO_TLSv1 }, app);
  } else {
    server = http.createServer(app);
  }

  setTimeout(() => {
    server
      .on('error', (e) => {
        console.log("ERROR - couldn't listen on port", getConfig('wiseService', 'port', 8081), 'is wiseService already running?');
        process.exit(1);
      })
      .on('listening', (e) => {
        console.log('Express server listening on port %d in %s mode', server.address().port, app.settings.env);
      })
      .listen(getConfig('wiseService', 'port', 8081));
  }, 2000);
}

function buildConfigAndStart () {
  // The config is actually hidden
  if (internals.configFile.endsWith('.hiddenconfig')) {
    internals.configFile = fs.readFileSync(internals.configFile).toString().split('\n')[0].trim();
  }

  const parts = internals.configFile.split('://');
  if (parts.length === 1) {
    if (internals.configFile.endsWith('json')) {
      internals.configScheme = internals.configSchemes.json;
    } else {
      internals.configScheme = internals.configSchemes.ini;
    }
  } else {
    internals.configScheme = internals.configSchemes[parts[0]];
  }

  if (internals.configScheme === undefined) {
    throw new Error('Unknown scheme');
  }

  internals.configScheme.load((err) => {
    if (err) {
      console.log(`Error reading ${internals.configFile}:\n\n`, err);
      process.exit(1);
    }

    if (internals.debug > 1) {
      console.log('Config', internals.config);
    }

    setupAuth();
    if (internals.workers <= 1 || cluster.isWorker) {
      main();
    }
  });
}

buildConfigAndStart();
