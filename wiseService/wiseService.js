/******************************************************************************/
/* wiseService.js -- Server requests between Arkime and various intel services
 *                   and files
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const express = require('express');
const fs = require('fs');
const glob = require('glob');
const async = require('async');
const sprintf = require('./sprintf.js').sprintf;
const iptrie = require('iptrie');
const User = require('../common/user');
const Auth = require('../common/auth');
const ArkimeUtil = require('../common/arkimeUtil');
const WISESource = require('./wiseSource.js');
const cluster = require('cluster');
const cryptoLib = require('crypto');
const favicon = require('serve-favicon');
const uuid = require('uuid').v4;
const helmet = require('helmet');
const jsonParser = ArkimeUtil.jsonParser;
const chalk = require('chalk');
const version = require('../common/version');
const path = require('path');
const ArkimeCache = require('../common/arkimeCache');
const ArkimeConfig = require('../common/arkimeConfig');
const RE2 = require('re2');

const dayMs = 60000 * 60 * 24;

require('console-stamp')(console, '[HH:MM:ss.l]');

const internals = {
  fieldsTS: 0,
  fields: [],
  fieldsSize: 0,
  sources: new Map(),
  configDefs: {
    wiseService: {
      description: 'General settings that apply to WISE and all wise sources',
      singleton: true,
      service: true,
      fields: [
        { name: 'port', required: false, regex: '^[0-9]+$', help: 'Port that the wiseService runs on. Defaults to 8081' },
        { name: 'keyFile', required: false, help: 'Path to PEM encoded key file' },
        { name: 'certFile', required: false, help: 'Path to PEM encoded cert file' },
        { name: 'caTrustFile', required: false, help: 'Path to PEM encoded CA file' },
        { name: 'authMode', required: true, help: 'How should auth be done: anonymous - no auth, basic - basic auth, digest - digest auth, header - http header auth, oidc - oidc auth, form - form auth', regex: '(anonymous|basic|digest|header|oidc|form|basic\\+oidc|basic\\+form|header\\+basic|header\\+digest|headerOnly|)' },
        { name: 'userNameHeader', required: true, help: 'the http header to use for username', ifField: 'authMode', ifValue: 'header' },
        { name: 'httpRealm', ifField: 'authMode', ifValue: 'digest', required: false, help: 'The realm to use for digest requests. Must be the same as viewer is using. Default Moloch' },
        { name: 'passwordSecret', ifField: 'authMode', ifValue: 'digest', required: false, password: true, help: 'The secret used to encrypted password hashes. Must be the same as viewer is using. Default password' },
        { name: 'usersElasticsearch', required: false, help: 'The URL to connect to OpenSearch/Elasticsearch. Default http://localhost:9200' },
        { name: 'usersElasticsearchAPIKey', required: false, help: 'OpenSearch/Elastisearch API key for users DB access', password: true },
        { name: 'usersElasticsearchBasicAuth', required: false, help: 'OpenSearch/Elastisearch Basic Auth', password: true },
        { name: 'userAuthIps', required: false, help: 'Comma separated list of CIDRs to allow authed requests from' },
        { name: 'usersPrefix', required: false, help: 'The prefix used with db.pl --prefix for users OpenSearch/Elasticsearch, if empty arkime_ is used' },
        { name: 'sourcePath', required: false, help: 'Where to look for the source files. Defaults to "./"' }
      ]
    },
    cache: {
      description: 'Specify how WISE should cache results from sources that support it. Using a redis setup is especially useful when there are multiple WISE servers or large amount of results to cache.',
      singleton: true,
      service: true,
      fields: [
        { name: 'type', required: false, regex: '^(memory|redis|memcached|lmdb)$', help: 'Where to cache results: memory (default), redis, memcached, lmdb' },
        { name: 'cacheSize', required: false, help: 'How many elements to cache in memory. Defaults to 100000' },
        { name: 'redisURL', password: true, required: false, ifField: 'type', ifValue: 'redis', help: 'Format is redis://[:password@]host:port/db-number, redis-sentinel://[[sentinelPassword]:[password]@]host[:port]/redis-name/db-number, or redis-cluster://[:password@]host:port/db-number' },
        { name: 'redisFormat', required: false, ifField: 'type', ifValue: 'redis', help: 'Use 2 (default) if WISE 2.x & WISE 3.x in use or 3 if just WISE 3.x', regex: '[23]' },
        { name: 'memcachedURL', password: true, required: false, ifField: 'type', ifValue: 'memcached', help: 'Format is memcached://[user:pass@]server1[:11211],[user:pass@]server2[:11211],...' },
        { name: 'lmdbDir', password: false, required: false, ifField: 'type', ifValue: 'lmdb', help: 'Directory for lmdb cache' }
      ]
    }
  },
  types: new Map(),
  views: new Map(),
  fieldActions: new Map(),
  valueActions: new Map(),
  workers: 1,
  webconfig: false,
  configCode: cryptoLib.randomBytes(20).toString('base64').replace(/[=+/]/g, '').substr(0, 6),
  startTime: Date.now()
};

internals.type2Name = ['ip', 'domain', 'md5', 'email', 'url', 'tuple', 'ja3', 'sha256'];

// ----------------------------------------------------------------------------
// Command Line Parsing
// ----------------------------------------------------------------------------
function processArgs (argv) {
  for (let i = 0, ilen = argv.length; i < ilen; i++) {
    if (process.argv[i] === '-o') {
      i++;
      const equal = process.argv[i].indexOf('=');
      if (equal === -1) {
        console.log('Missing equal sign in', process.argv[i]);
        process.exit(1);
      }

      ArkimeConfig.setOverride(process.argv[i].slice(0, equal), process.argv[i].slice(equal + 1));
    } else if (argv[i] === '--webcode') {
      i++;
      internals.configCode = argv[i];
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
      console.log('  -c, --config <file|url>     Where to fetch the config file from');
      console.log('  -o <section>.<key>=<value>  Override the config file');
      console.log('  --debug                     Increase debug level, multiple are supported');
      console.log('  --webconfig                 Allow the config to be edited from web page');
      console.log('  --workers <b>               Number of worker processes to create');
      console.log('  --insecure                  Disable certificate verification for https calls');

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
// define csp headers
const cspDirectives = {
  defaultSrc: ["'self'"],
  // unsafe-inline required for json editor (https://github.com/dirkliu/vue-json-editor)
  styleSrc: ["'self'", "'unsafe-inline'"],
  // need unsafe-eval for vue full build: https://vuejs.org/v2/guide/installation.html#CSP-environments
  scriptSrc: ["'self'", "'unsafe-eval'", (req, res) => `'nonce-${res.locals.nonce}'`],
  objectSrc: ["'none'"],
  imgSrc: ["'self'", 'data:'],
  // web worker required for json editor (https://github.com/dirkliu/vue-json-editor)
  workerSrc: ["'self'", 'blob:']
};
const cspHeader = helmet.contentSecurityPolicy({
  directives: cspDirectives
});

// Explicit sigint handler for running under docker
// See https://github.com/nodejs/node/issues/4182
process.on('SIGINT', function () {
  process.exit();
});

// ----------------------------------------------------------------------------
function setupAuth () {
  Auth.initialize({
    appAdminRole: 'wiseAdmin',
    passwordSecretSection: 'wiseService'
  });

  if (Auth.mode === 'anonymous') {
    return;
  }

  const es = ArkimeConfig.get('usersElasticsearch', 'http://localhost:9200');

  User.initialize({
    insecure: ArkimeConfig.insecure,
    node: es,
    caTrustFile: ArkimeConfig.get('caTrustFile'),
    prefix: ArkimeConfig.get('usersPrefix'),
    apiKey: ArkimeConfig.get('usersElasticsearchAPIKey'),
    basicAuth: ArkimeConfig.get('usersElasticsearchBasicAuth')
  });
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
/**
 * When sources are created they get an api object to interact with the wise service.
 */
class WISESourceAPI {
  /**
   * Current debug level of wiseService
   * @type {integer}
   */
  debug = ArkimeConfig.debug;

  /**
   * Is wiseService running in insecure mode
   * @type {boolean}
   */
  insecure = ArkimeConfig.insecure;

  app = app;

  /**
   * Get from the config section a value or default
   *
   * @param {string} section - The section in the config file the key is in
   * @param {string} sectionKey - The key to get from the section
   * @param {string} [default] - the default value to return if key is not found in section
   * @returns {string} - The value found or the default value
   */
  getConfig = ArkimeConfig.getFull;

  // ----------------------------------------------------------------------------
  /**
   * Get a list of all the sections in the config file
   *
   * @returns {string|Array} - A list of all the sections in the config file
   */
  getConfigSections = ArkimeConfig.getSections;

  // ----------------------------------------------------------------------------
  /**
   * Get the full config for a section
   *
   * @param {string} section - The section of the config file to return
   * @returns {object} - A list of all the sections in the config file
   */
  getConfigSection = ArkimeConfig.getSection;

  // ----------------------------------------------------------------------------
  /**
   * Add a field
   *
   * @param {string} field - An encoded field definition
   */
  addField (field) {
    let match = field.match(/field:([^;]+)/);
    const fieldName = match[1];

    let db;
    if ((match = field.match(/db:([^;]+)/))) {
      db = match[1];
    }

    let friendly;
    if ((match = field.match(/friendly:([^;]+)/))) {
      friendly = match[1];
    }

    if (WISESource.field2Pos.has(fieldName)) {
      return WISESource.field2Pos.get(fieldName);
    }

    if (ArkimeConfig.debug > 1) {
      console.log(`Adding field name:${fieldName} db:${db} friendly:${friendly} from '${field}'`);
    }

    const pos = internals.fields.length;
    newFieldsTS();
    internals.fields.push(field);
    internals.fieldsSize += field.length + 10;

    let offset;
    // Create version 0 of fields buf
    if (internals.fields.length < 256) {
      internals.fieldsBuf0 = Buffer.alloc(internals.fieldsSize + 9);
      internals.fieldsBuf0.writeUInt32BE(internals.fieldsTS, 0);
      internals.fieldsBuf0.writeUInt32BE(0, 4);
      internals.fieldsBuf0.writeUInt8(internals.fields.length, 8);
      offset = 9;
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
    offset = 10;
    for (let i = 0; i < internals.fields.length; i++) {
      const len = internals.fieldsBuf1.write(internals.fields[i], offset + 2);
      internals.fieldsBuf1.writeUInt16BE(len + 1, offset);
      internals.fieldsBuf1.writeUInt8(0, offset + 2 + len);
      offset += 3 + len;
    }
    internals.fieldsBuf1 = internals.fieldsBuf1.slice(0, offset);

    internals.fieldsMd5 = cryptoLib.createHash('md5').update(internals.fieldsBuf1.slice(8)).digest('hex');

    WISESource.pos2Field.set(pos, fieldName);
    WISESource.field2Pos.set(fieldName, pos);
    WISESource.field2Info.set(fieldName, { pos, friendly, db });
    return pos;
  }

  // ----------------------------------------------------------------------------
  /**
   * Add a view
   *
   * @param {string} viewName - Name of the new view
   * @param {string} view - An encoded view definition
   */
  addView (viewName, view) {
    if (view.includes('require:')) {
      let match = view.match(/require:([^;]+)/);
      const req = match[1];

      match = view.match(/title:([^;]+)/);
      if (!match) {
        console.log(`ERROR - ${viewName} view is missing 'title:' ${view}`);
        return;
      }
      const title = match[1];

      match = view.match(/fields:([^;]+)/);
      if (!match) {
        console.log(`ERROR - ${viewName} view is missing 'fields:' ${view}`);
        return;
      }
      const fields = match[1];

      // Can override the name in the view
      match = view.match(/section:([^;]+)/);
      if (match) {
        viewName = match[1];
      }

      const parts = req.split('.');
      let output = 'if (';
      for (let i = 0; i < parts.length; i++) {
        if (i > 0) {
          output += ' && ';
        }
        output += 'session';
        for (let j = 0; j <= i; j++) {
          output += `.${parts[j]}`;
        }
      }
      output += ')\n';
      output += `  div.sessionDetailMeta.bold ${title}\n  dl.sessionDetailMeta\n`;

      for (const field of fields.split(',')) {
        const info = WISESource.field2Info.get(field);
        if (!info) {
          continue;
        }
        if (!info.db) {
          console.log(`ERROR - missing db information for ${field}`);
          return;
        }
        const pos = info.db.lastIndexOf('.');
        if (pos === -1) {
          output += `    +arrayList(session, '${info.db}', '${info.friendly}', '${field}')\n`;
        } else {
          output += `    +arrayList(session.${info.db.slice(0, pos)}, '${info.db.slice(pos + 1)}', '${info.friendly}', '${field}')\n`;
        }
      }

      internals.views.set(viewName, output);
    } else {
      internals.views.set(viewName, view);
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
    src.types = types;
    internals.sources.set(section, src);

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
   * Define all configuration for a field for a source
   * @typedef {Object} WISESourceAPI~ValueAction
   * @property {string} key - The key must be unique and is also used as the right click menu name if the name field is missing
   * @property {string} name - The name of the value action to show the user
   * @property {string} [url] - The url to send the user, supports special subsitutions, must set url or func
   * @property {string} [func] - A javascript function body to call, will be passed the name and value and must return the value, must set url or func
   * @property {string} [actionType] - If set to 'fetch' this will replace the menu option with the results of url or func
   * @property {string} [category] - Which category of fields should the value action be shown for, must set fields or category. <a href="settings#right-click">View available categories</a>
   * @property {string} [fields] - Which fields to show the value action for, must set fields or category
   * @property {string} [regex] - When set replaces %REGEX% in the url with the match
   * @property {string} [users] - A comma separated list of user names that can see the right click item. If not set then all users can see the right click item.
   * @property {string} [notUsers] - (Since 3.0) A comma separated list of user names that can NOT see the right click item. This setting is applied before the users setting above.
   */

  /**
   * Add a value action set
   * @params {string} actionName - The globally unique name of this action, not shown to user
   * @params {WISESourceAPI~ValueAction} action - The action
   */
  addValueAction (actionName, action) {
    internals.valueActions.set(actionName, action);
  }

  /**
   * Add a field action set
   * @params {string} actionName - The globally unique name of this action, not shown to user
   * @params {WISESourceAPI~ValueAction} action - The action
   */
  addFieldAction (actionName, action) {
    internals.fieldActions.set(actionName, action);
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
  glob(ArkimeConfig.get('sourcePath', path.join(__dirname, '/')) + 'source.*.js', (err, files) => {
    files.forEach((file) => {
      try {
        const src = require(file);
        src.initSource(internals.sourceApi);
      } catch (err) {
        console.log(`WARNING - Couldn't load ${file}\n`, err);
      }
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
ArkimeUtil.logger(app);
app.use(timeout(5 * 1000));

// client static files --------------------------------------------------------
app.use(favicon(path.join(__dirname, '/favicon.ico')));

// using fallthrough: false because there is no 404 endpoint (client router
// handles 404s) and sending index.html is confusing
app.use('/font-awesome', express.static(
  path.join(__dirname, '/../node_modules/font-awesome'),
  { maxAge: dayMs, fallthrough: false }
), ArkimeUtil.missingResource);
app.use('/assets', express.static(
  path.join(__dirname, '/../assets'),
  { maxAge: dayMs, fallthrough: false }
), ArkimeUtil.missingResource);

// expose vue bundles (prod) - need to be here because of wildcard endpoint matches
app.use('/static', express.static(
  path.join(__dirname, '/vueapp/dist/static'),
  { maxAge: dayMs, fallthrough: false }
), ArkimeUtil.missingResource);

// expose vue bundle (dev)
app.use(['/app.js', '/vueapp/app.js'], express.static(
  path.join(__dirname, '/vueapp/dist/app.js'),
  { fallthrough: false }
), ArkimeUtil.missingResource);
app.use(['/app.js.map', '/vueapp/app.js.map'], express.static(
  path.join(__dirname, '/vueapp/dist/app.js.map'),
  { fallthrough: false }
), ArkimeUtil.missingResource);

// ----------------------------------------------------------------------------
if (ArkimeConfig.regressionTests) {
  app.post('/regressionTests/shutdown', (req, res) => {
    process.exit(0);
  });
  app.post('/regressionTests/checkCode', [jsonParser, checkConfigCode], (req, res) => {
    return res.send(JSON.stringify({ success: true, text: 'Authorized' }));
  });
}
// ----------------------------------------------------------------------------
/**
 * GET - Health check URL
 *
 * @name "/_ns_/nstest.html"
 */
app.get('/_ns_/nstest.html', [ArkimeUtil.noCacheJson], (req, res) => {
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
app.get('/fields', [ArkimeUtil.noCacheJson], (req, res) => {
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
app.get('/views', [ArkimeUtil.noCacheJson], function (req, res) {
  res.send(Object.fromEntries(internals.views));
});
// ----------------------------------------------------------------------------
/**
 * GET - Used by viewer to retrieve all the field value actions created by wise sources
 *
 * @name "/valueActions"
 * @returns {object|array} All the actions
 */
app.get(['/rightClicks', '/valueActions'], [ArkimeUtil.noCacheJson], function (req, res) {
  res.send(Object.fromEntries(internals.valueActions));
});
// ----------------------------------------------------------------------------
/**
 * GET - Used by viewer to retrieve all the field actions created by wise sources
 *
 * @name "/fieldActions"
 * @returns {object|array} All the field actions
 */
app.get('/fieldActions', [ArkimeUtil.noCacheJson], function (req, res) {
  res.send(Object.fromEntries(internals.fieldActions));
});

// ----------------------------------------------------------------------------
function globalAllowed (value) {
  for (let i = 0; i < this.excludes.length; i++) {
    if (value.match(this.excludes[i])) {
      if (ArkimeConfig.debug > 0) {
        console.log('Found in Global %s Exclude', this.name, value);
      }
      return false;
    }
  }
  return true;
}
// ----------------------------------------------------------------------------
function globalIPAllowed (value) {
  if (this.excludes.find(value)) {
    if (ArkimeConfig.debug > 0) {
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
      if (ArkimeConfig.debug > 0) {
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
    if (ArkimeConfig.debug > 0) {
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
  let typeInfo = internals.types.get(type);
  if (!typeInfo) {
    typeInfo = {
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
      globalAllowed,
      sourceAllowed
    };
    internals.types.set(type, typeInfo);

    if (type === 'url') {
      typeInfo.excludeName = 'excludeURLs';
    }

    if (type === 'ip') {
      typeInfo.excludeName = 'excludeIPs';
      typeInfo.globalAllowed = globalIPAllowed;
      typeInfo.sourceAllowed = sourceIPAllowed;
    }

    for (const src of internals.sources.keys()) {
      const source = internals.sources.get(src);
      if (source[typeInfo.funcName]) {
        typeInfo.sources.push(source);
        source.srcInProgress[type] = new Map();
      }
    }

    const items = ArkimeConfig.get(typeInfo.excludeName, '');
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
    newSrc.srcInProgress[type] = new Map();
  }
  return typeInfo;
}
// ----------------------------------------------------------------------------
function processQuery (req, query, cb) {
  if (query.typeName === '__proto__') {
    return cb('__proto__ invalid type name');
  }

  let typeInfo = internals.types.get(query.typeName);

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
  internals.cache.get(query.typeName + '-' + query.value, (err, cacheResult) => {
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

    async.map(query.sources || typeInfo.sources, (src, mapCb) => {
      if (!typeInfo.sourceAllowed(src, query.value)) {
        // This source isn't allowed for query
        return setImmediate(mapCb, undefined);
      }

      if (typeof src[typeInfo.funcName] !== 'function') {
        return setImmediate(mapCb, undefined);
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
        if (src.srcInProgress[query.typeName] && src.srcInProgress[query.typeName].has(query.value)) {
          src.srcInProgress[query.typeName].get(query.value).push(mapCb);
          return;
        }

        // First query for this value
        src.srcInProgress[query.typeName].set(query.value, [mapCb]);
        const startTime = Date.now();
        src[typeInfo.funcName](src.fullQuery === true ? query : query.value, (err, result) => {
          src.recentAverageMS = (999.0 * src.recentAverageMS + (Date.now() - startTime)) / 1000.0;

          if (!err && result !== undefined) {
            src.directHitStat++;
            if (src.cacheTimeout !== -1) { // If err or cacheTimeout is -1 then don't cache
              cacheResult[src.section] = { ts: now, result };
              cacheChanged = true;
            }
          }
          if (err === 'dropped') {
            src.requestDroppedStat++;
            err = null;
            result = undefined;
          }
          const srcInProgress = src.srcInProgress[query.typeName].get(query.value);
          src.srcInProgress[query.typeName].delete(query.value);
          for (let i = 0, l = srcInProgress.length; i < l; i++) {
            srcInProgress[i](err, result);
          }
        });
      } else {
        src.cacheHitStat++;
        typeInfo.cacheSrcHitStats++;
        // Woot, we can use the cache
        setImmediate(mapCb, null, cacheResult[src.section].result);
      }
    }, (err, results) => {
      // Combine all the results together
      if (err) {
        return cb(err);
      }
      if (ArkimeConfig.debug > 2) {
        console.log('RESULT', typeInfo.funcName, query.value, WISESource.result2JSON(WISESource.combineResults(results)));
      }

      if (req.timedout) {
        cb('Timed out ' + query.typeName + ' ' + query.value);
      } else {
        cb(null, WISESource.combineResults(results));
      }

      // Need to update the cache
      if (cacheChanged) {
        internals.cache.set(query.typeName + '-' + query.value, cacheResult);
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
      internals.types.get(queries[r].typeName).foundStats++;
    }
    res.write(results[r]);
  }
  res.end();
}
// ----------------------------------------------------------------------------
// pos len value
// 0   4   flags
// 4   2   2
// 8   32  md5 of fields
// 40  2   length of fields info if md5 unknown
// 42  L   fields info
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
      internals.types.get(queries[r].typeName).foundStats++;
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

        if (!typeName) {
          console.log('Couldn\'t find typeName');
          throw new Error('Could not make out typeName from query');
        }

        const len = buf.readUInt16BE(offset);
        offset += 2;

        const value = buf.toString('utf8', offset, offset + len);
        if (ArkimeConfig.debug > 1) {
          console.log('%s', typeName, value);
        }
        offset += len;
        queries.push({ typeName, value });
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
 * GET - Used by wise UI to retrieve all the sources (unathenticated).
 *
 * @name "/sources"
 * @returns {string|array} All the sources
 */
app.get('/sources', [ArkimeUtil.noCacheJson], (req, res) => {
  return res.send([...internals.sources.keys()].sort());
});
// ----------------------------------------------------------------------------
/**
 * GET - Used by wise UI to retrieve all the configuration definitions for the various sources (unauthenticated).
 *
 * @name "/config/defs"
 * @returns {object}
 */
app.get('/config/defs', [ArkimeUtil.noCacheJson], function (req, res) {
  return res.send(internals.configDefs);
});
// ----------------------------------------------------------------------------
/**
 * GET - Used by the wise UI to all the types known (unathenticated).
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
app.get('/types/:source?', [ArkimeUtil.noCacheJson], (req, res) => {
  if (req.params.source) {
    const source = internals.sources.get(req.params.source);
    if (source) {
      return res.send(source.types.sort());
    } else {
      return res.send([]);
    }
  } else {
    return res.send([...internals.types.keys()].sort());
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
app.get('/:source/:typeName/:value', [ArkimeUtil.noCacheJson], function (req, res, next) {
  // Poor route planning by ALW, shame
  if (req.params.source === 'source' && req.params.value === 'get') { return next(); }
  if (req.params.source === 'auth') { return next(); }

  const source = internals.sources.get(req.params.source);
  if (!source) {
    return res.end('Unknown source ' + ArkimeUtil.safeStr(req.params.source));
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
app.get('/dump/:source', [ArkimeUtil.noCacheJson], function (req, res) {
  const source = internals.sources.get(req.params.source);
  if (!source) {
    return res.end('Unknown source ' + ArkimeUtil.safeStr(req.params.source));
  }

  if (!source.dump) {
    return res.end("The source doesn't support dump");
  }

  source.dump(res);
});
// ----------------------------------------------------------------------------
// ALW - Need to rewrite to use performQuery
/*
app.get("/bro/:type", [ArkimeUtil.noCacheJson], function(req, res) {
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
          res.write(WISESource.pos2Field.get(pos) + ": " + value);
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
app.get('/:typeName/:value', [ArkimeUtil.noCacheJson], function (req, res, next) {
  // Poor route planning by ALW, shame
  if (req.params.typeName === 'config' && req.params.value === 'get') { return next(); }

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
app.get('/stats', [ArkimeUtil.noCacheJson], (req, res) => {
  const types = [...internals.types.keys()].sort();
  const sections = [...internals.sources.keys()].sort();

  const stats = { types: [], sources: [], startTime: internals.startTime };

  let re2;
  if (req.query.search) {
    re2 = new RE2(req.query.search.toLowerCase());
  }

  for (const type of types) {
    const typeInfo = internals.types.get(type);
    let match = true;
    if (re2) {
      match = type.toLowerCase().match(re2);
    }
    if (!match) { continue; }
    stats.types.push({
      type,
      request: typeInfo.requestStats,
      found: typeInfo.foundStats,
      cacheHit: typeInfo.cacheHitStats,
      cacheSrcHit: typeInfo.cacheSrcHitStats,
      cacheSrcMiss: typeInfo.cacheSrcMissStats,
      cacheSrcRefresh: typeInfo.cacheSrcRefreshStats
    });
  }

  for (const section of sections) {
    const src = internals.sources.get(section);
    let match = true;
    if (re2) {
      match = section.toLowerCase().match(re2);
    }
    if (!match) { continue; }
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
function printStats () {
  const keys = [...internals.types.keys()].sort();
  const lines = [];
  lines[0] = '                   ';
  lines[1] = 'REQUESTS:          ';
  lines[2] = 'FOUND:             ';
  lines[3] = 'CACHE HIT:         ';
  lines[4] = 'CACHE SRC HIT:     ';
  lines[5] = 'CACHE SRC REFRESH: ';

  for (const key of keys) {
    const typeInfo = internals.types.get(key);
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

  for (const section of [...internals.sources.keys()].sort()) {
    const src = internals.sources.get(section);
    console.log(sprintf('SRC %-30s    cached: %7d lookup: %9d refresh: %7d dropped: %7d avgMS: %7d',
      section, src.cacheHitStat, src.cacheMissStat, src.cacheRefreshStat, src.requestDroppedStat, src.recentAverageMS));
  }
}

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

// ============================================================================
// AUTHED ROUTES - only needed for webconfig, must be at bottom
// ============================================================================
function isWiseAdmin (req, res, next) {
  if (req.user.hasRole('wiseAdmin')) {
    return next();
  } else {
    console.log(`${req.userId} is not wiseAdmin`);
    return res.send(JSON.stringify({ success: false, text: 'Not authorized, check log file' }));
  }
}

// ----------------------------------------------------------------------------
function isWiseUser (req, res, next) {
  if (req.user.hasRole('wiseUser')) {
    return next();
  } else {
    console.log(`${req.userId} is not wiseUser`);
    return res.send(JSON.stringify({ success: false, text: 'Not authorized, check log file' }));
  }
}
// ----------------------------------------------------------------------------
if (internals.webconfig) {
  // Set up auth, all APIs registered below will use passport
  Auth.app(app);
  // ----------------------------------------------------------------------------
  /**
   * GET - Used by wise UI to retrieve the raw file being used by the section.
   *       This is an authenticated API and requires wiseService to be started with --webconfig.
   *
   * @name "/source/:source/get"
   * @param {string} :source - The source to get the raw data for
   * @returns {object} All the views
   */
  app.get('/source/:source/get', [isWiseUser, ArkimeUtil.noCacheJson], (req, res) => {
    const source = internals.sources.get(req.params.source);
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
  app.put('/source/:source/put', [isWiseAdmin, ArkimeUtil.noCacheJson, jsonParser], (req, res) => {
    const source = internals.sources.get(req.params.source);
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
   * GET - Used by wise UI to retrieve the current config.
   *       This is an authenticated API and requires wiseService to be started with --webconfig.
   *
   * @name "/config/get"
   * @returns {object}
   */
  app.get('/config/get', [isWiseUser, ArkimeUtil.noCacheJson], (req, res) => {
    const config = ArkimeConfig.getSections()
      .sort()
      .filter(key => internals.configDefs[key.split(':')[0]])
      .reduce((obj, key) => {
        // Deep Copy
        obj[key] = JSON.parse(JSON.stringify(ArkimeConfig.getSection(key)));

        // Replace passwords
        internals.configDefs[key.split(':')[0]].fields.forEach((item) => {
          if (item.password !== true) { return; }
          if (obj[key][item.name] === undefined || obj[key][item.name].length === 0) { return; }
          obj[key][item.name] = '********';
        });
        return obj;
      }, {});

    config.wiseService ??= {};
    config.cache ??= {};

    return res.send({
      success: true,
      config,
      filePath: ArkimeConfig.configFile
    });
  });
  // ----------------------------------------------------------------------------
  /**
   * PUT - Used by wise UI to save the current config.
   *       This is an authenticated API, requires the pin code, and requires wiseService to be started with --webconfig.
   *
   * @name "/config/save"
   */
  app.put('/config/save', [isWiseAdmin, ArkimeUtil.noCacheJson, jsonParser, checkConfigCode], (req, res) => {
    if (req.body.config === undefined) {
      return res.send({ success: false, text: 'Missing config' });
    }

    const config = req.body.config;
    if (ArkimeConfig.debug > 0) {
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
          console.log(`Section ${section} field ${key} unknown, deleting`);
          delete config[section][key];
        } else if (field.password === true) {
          if (config[section][key] === '********') {
            config[section][key] = ArkimeConfig.getFull(section, key);
          }
        }
      };
    }

    if (ArkimeConfig.regressionTests) {
      return res.send({ success: true, text: 'Would save, but regressionTests' });
    }

    // Make sure updateTime has increased incase of clock sku
    config.wiseService.updateTime = Math.max(Date.now(), internals.updateTime + 1);

    ArkimeConfig.replace(config);
    ArkimeConfig.save((err) => {
      if (err) {
        return res.send({ success: false, text: err });
      } else {
        res.send({ success: true, text: 'Saved & Restarting' });
        // Because of nodemon
        setTimeout(() => { process.kill(process.pid, 'SIGUSR2'); }, 500);
        setTimeout(() => { process.exit(0); }, 1500);
      }
    });
  });
} else {
  app.get(['/source/:source/get', '/config/get'], (req, res) => {
    return res.send({ success: false, text: 'Must start wiseService with --webconfig option' });
  });
  app.put(['/source/:source/put', '/config/save'], (req, res) => {
    return res.send({ success: false, text: 'Must start wiseService with --webconfig option' });
  });
}

// Replace the default express error handler
app.use(ArkimeUtil.expressErrorHandler);

// ============================================================================
// VUE APP
// ============================================================================
const Vue = require('vue');
const vueServerRenderer = require('vue-server-renderer');

// Factory function to create fresh Vue apps
function createApp () {
  return new Vue({
    template: '<div id="app"></div>'
  });
}

// Send back vue for every other request
app.use(cspHeader, (req, res, next) => {
  const renderer = vueServerRenderer.createRenderer({
    template: fs.readFileSync(path.join(__dirname, '/vueapp/dist/index.html'), 'utf-8')
  });

  const appContext = {
    logoutUrl: Auth.logoutUrl,
    nonce: res.locals.nonce,
    version: version.version
  };

  // Create a fresh Vue app instance
  const vueApp = createApp();

  // Render the Vue instance to HTML
  renderer.renderToString(vueApp, appContext, (err, html) => {
    if (err) {
      console.log('ERROR - fetching vue index page:', err);
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

// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------
function main () {
  internals.cache = ArkimeCache.createCache({
    type: ArkimeConfig.getFull('cache', 'type', 'memory'),
    cacheSize: ArkimeConfig.getFull('cache', 'cacheSize', '100000'),
    cacheTimeout: ArkimeConfig.getFull('cache', 'cacheTimeout'),
    getConfig: (key, value) => ArkimeConfig.getFull('cache', key, value)
  });

  internals.sourceApi = new WISESourceAPI();
  internals.sourceApi.addField('field:tags'); // Always add tags field so we have at least 1 field
  loadSources();

  if (ArkimeConfig.debug > 0) {
    setInterval(printStats, 60 * 1000);
  }

  // Wait 2 seconds to start listening so the sources have time to settle down
  setTimeout(() => {
    ArkimeUtil.createHttpServer(app, ArkimeConfig.get('wiseHost'), ArkimeConfig.get('port', 8081));
  }, 2000);
}

async function buildConfigAndStart () {
  // Load config
  await ArkimeConfig.initialize({
    defaultConfigFile: `${version.config_prefix}/etc/wiseService.ini`,
    defaultSections: 'wiseService'
  });

  internals.updateTime = ArkimeConfig.get('updateTime', 0);

  // Check if we need to restart, this is if there are multiple instances
  setInterval(async () => {
    await ArkimeConfig.reload();
    const updateTime = ArkimeConfig.get('updateTime', 0);
    if (updateTime > internals.updateTime) {
      console.log('New config file, restarting');
      // Because of nodemon
      setTimeout(() => { process.kill(process.pid, 'SIGUSR2'); }, 500);
      setTimeout(() => { process.exit(0); }, 1500);
    }
  }, ((3000 * 60) + (Math.random() * 3000 * 60))); // Check 3min + 0-3min

  if (internals.webconfig) {
    setupAuth();
  }
  if (internals.workers <= 1 || cluster.isWorker) {
    main();
  }
}

buildConfigAndStart();
