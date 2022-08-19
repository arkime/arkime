/******************************************************************************/
/* cont3xt.js  -- The main cont3xt app
 *
 * Copyright Yahoo Inc.
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

const express = require('express');
const ini = require('iniparser');
const fs = require('fs');
const app = express();
const http = require('http');
const https = require('https');
const path = require('path');
const version = require('../common/version');
const User = require('../common/user');
const Auth = require('../common/auth');
const ArkimeCache = require('../common/arkimeCache');
const ArkimeUtil = require('../common/arkimeUtil');
const LinkGroup = require('./linkGroup');
const Integration = require('./integration');
const Audit = require('./audit');
const View = require('./view');
const Db = require('./db');
const bp = require('body-parser');
const jsonParser = bp.json();
// eslint-disable-next-line no-shadow
const crypto = require('crypto');
const logger = require('morgan');
const favicon = require('serve-favicon');
const helmet = require('helmet');
const uuid = require('uuidv4').default;
const dayMs = 60000 * 60 * 24;

const internals = {
  configFile: `${version.config_prefix}/etc/cont3xt.ini`,
  debug: 0,
  insecure: false,
  regressionTests: false,
  options: {},
  debugged: {}
};

// ----------------------------------------------------------------------------
// Security
// ----------------------------------------------------------------------------
app.use(helmet.frameguard({ action: 'deny' })); // disallow iframing
app.use(helmet.hidePoweredBy()); // hide powered by Express header
app.use(helmet.xssFilter()); // disables browsers' buggy cross-site scripting filte
app.use(helmet.noSniff()); // mitigates MIME type sniffing

function setupHSTS () {
  if (getConfig('cont3xt', 'hstsHeader', false)) {
    app.use(helmet.hsts({
      maxAge: 31536000,
      includeSubDomains: true
    }));
  }
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
    styleSrc: ["'self'", "'unsafe-inline'"], // 'unsafe-inline' for vue inline styles
    // need unsafe-eval for vue full build: https://vuejs.org/v2/guide/installation.html#CSP-environments
    scriptSrc: ["'self'", "'unsafe-eval'", (req, res) => `'nonce-${res.locals.nonce}'`],
    objectSrc: ["'none'"],
    imgSrc: ["'self'", 'data:']
  }
});

function setCookie (req, res, next) {
  const cookieOptions = {
    path: getConfig('cont3xt', 'webBasePath', '/'),
    sameSite: 'Strict',
    overwrite: true
  };

  if (getConfig('cont3xt', 'keyFile') && getConfig('cont3xt', 'certFile')) { cookieOptions.secure = true; }

  res.cookie( // send cookie for basic, non admin functions
    'CONT3XT-COOKIE',
    Auth.obj2auth({
      date: Date.now(),
      pid: process.pid,
      userId: req.user.userId
    }),
    cookieOptions
  );

  return next();
}

function checkCookieToken (req, res, next) {
  if (!req.headers['x-cont3xt-cookie']) {
    return res.status(500).send({ success: false, text: 'Missing token' });
  }

  const cookie = req.headers['x-cont3xt-cookie'];
  req.token = Auth.auth2obj(cookie);
  const diff = Math.abs(Date.now() - req.token.date);
  if (diff > 2400000 || req.token.userId !== req.user.userId) {
    console.trace('bad token', req.token, diff, req.token.userId, req.user.userId);
    return res.status(500).send({ success: false, text: 'Timeout - Please try reloading page and repeating the action' });
  }

  return next();
}

// ----------------------------------------------------------------------------
// Logging
// ----------------------------------------------------------------------------
app.use(logger(':date :username \x1b[1m:method\x1b[0m \x1b[33m:url\x1b[0m :res[content-length] bytes :response-time ms'));

logger.token('username', (req, res) => {
  return req.user ? req.user.userId : '-';
});

// ----------------------------------------------------------------------------
// Load balancer test - no auth
// ----------------------------------------------------------------------------
app.use('/_ns_/nstest.html', function (req, res) {
  res.end();
});

// ----------------------------------------------------------------------------
// Middleware
// ----------------------------------------------------------------------------
app.use((req, res, next) => {
  res.serverError = ArkimeUtil.serverError;
  if (internals.webBasePath !== '/') {
    req.url = req.url.replace(internals.webBasePath, '/');
  }
  return next();
});

// ----------------------------------------------------------------------------
// Routes
// ----------------------------------------------------------------------------
// assets and fonts
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
app.use('/public', express.static(
  path.join(__dirname, '/public'),
  { maxAge: dayMs, fallthrough: false }
), ArkimeUtil.missingResource);
const integrationsStatic = express.static(
  path.join(__dirname, '/integrations'),
  { maxAge: dayMs, fallthrough: false }
);
app.use('/integrations', (req, res, next) => {
  if (req.path.endsWith('.png')) {
    return integrationsStatic(req, res, (err) => {
      ArkimeUtil.missingResource(err, req, res);
    });
  }
  return ArkimeUtil.missingResource('Not png', req, res);
});

app.use(favicon(path.join(__dirname, '/favicon.ico')));

app.post('/regressionTests/shutdown', (req, res) => {
  if (internals.regressionTests) {
    console.log('Shutting down');
    process.exit(0);
  }
  res.send('NO!');
});

app.use(Auth.doAuth);

// check for cont3xtUser
app.use((req, res, next) => {
  if (!req.user.hasRole('cont3xtUser')) {
    return res.send('Need cont3xtUser role assigned');
  }
  next();
});

app.get('/api/linkGroup', LinkGroup.apiGet);
app.put('/api/linkGroup', [jsonParser, checkCookieToken], LinkGroup.apiCreate);
app.put('/api/linkGroup/:id', [jsonParser, checkCookieToken], LinkGroup.apiUpdate);
app.delete('/api/linkGroup/:id', [jsonParser, checkCookieToken], LinkGroup.apiDelete);

app.get('/api/roles', [checkCookieToken], User.apiRoles);
app.get('/api/user', User.apiGetUser);
app.post('/api/users', [jsonParser, User.checkRole('usersAdmin'), setCookie], User.apiGetUsers);
app.post('/api/users/min', [jsonParser, checkCookieToken, User.checkAssignableRole], User.apiGetUsersMin);
app.post('/api/user', [jsonParser, checkCookieToken, User.checkRole('usersAdmin')], User.apiCreateUser);
app.post('/api/user/password', [jsonParser, checkCookieToken, ArkimeUtil.getSettingUserDb], User.apiUpdateUserPassword);
app.delete('/api/user/:id', [jsonParser, checkCookieToken, User.checkRole('usersAdmin')], User.apiDeleteUser);
app.post('/api/user/:id', [jsonParser, checkCookieToken, User.checkRole('usersAdmin')], User.apiUpdateUser);
app.post('/api/user/:id/assignment', [jsonParser, checkCookieToken, User.checkAssignableRole], User.apiUpdateUserRole);

app.get('/api/integration', Integration.apiList);
app.post('/api/integration/search', [jsonParser], Integration.apiSearch);
app.post('/api/integration/:itype/:integration/search', [jsonParser], Integration.apiSingleSearch);
app.get('/api/settings', apiGetSettings);
app.put('/api/settings', [jsonParser, checkCookieToken], apiPutSettings);
app.get('/api/integration/settings', [setCookie], Integration.apiGetSettings);
app.put('/api/integration/settings', [jsonParser, checkCookieToken], Integration.apiPutSettings);
app.get('/api/integration/stats', [setCookie], Integration.apiStats);

app.get('/api/views', [setCookie], View.apiGet);
app.post('/api/view', [jsonParser, checkCookieToken], View.apiCreate);
app.put('/api/view/:id', [jsonParser, checkCookieToken], View.apiUpdate);
app.delete('/api/view/:id', [jsonParser, checkCookieToken], View.apiDelete);

app.get('/api/audits', Audit.apiGet);
app.delete('/api/audit/:id', [jsonParser, checkCookieToken], Audit.apiDelete);

app.get('/api/health', (req, res) => { res.send({ success: true }); });

// ----------------------------------------------------------------------------
// Cont3xt Web APIs
// ----------------------------------------------------------------------------

/**
 * GET - /api/settings
 *
 * Returns all the settings relevant for the cont3xt settings page
 * @name /settings
 * @returns {boolean} success - True if the request was successful, false otherwise
 * @returns {object} settings - General cont3xt settings
 * @returns {LinkGroup[]} linkGroups - An array of link groups that the logged in user can view/edit
 */
function apiGetSettings (req, res, next) {
  const cont3xt = req.user.cont3xt ?? {};
  res.send({
    success: true,
    settings: cont3xt.settings ?? {},
    linkGroup: cont3xt.linkGroup ?? {}
  });
}

/**
 * PUT - /api/settings
 *
 * Updates the general cont3xt settings
 * @name /settings
 * @param {object} settings - General cont3xt settings for the logged in user
 * @returns {boolean} success - True if the request was successful, false otherwise
 * @returns {string} text - The success/error message to (optionally) display to the user
 */
function apiPutSettings (req, res, next) {
  let save = false;
  User.getUser(req.user.userId, (err, user) => {
    if (err || !user) {
      return res.send({ success: false, text: 'Fetching user issue' });
    }

    if (user.cont3xt === undefined) { user.cont3xt = {}; }

    if (req.body?.settings) {
      user.cont3xt.settings = req.body.settings;
      save = true;
    }

    if (req.body?.linkGroup) {
      user.cont3xt.linkGroup = req.body.linkGroup;
      save = true;
    }

    if (!save) {
      return res.send({ success: false, text: 'Nothing sent to change' });
    }

    user.save((err) => {
      res.send({ success: true, text: 'Saved' });
    });
  });
}

// ----------------------------------------------------------------------------
// VUE APP
// ----------------------------------------------------------------------------
const Vue = require('vue');
const vueServerRenderer = require('vue-server-renderer');

// Factory function to create fresh Vue apps
function createApp () {
  return new Vue({
    template: '<div id="app"></div>'
  });
}

// using fallthrough: false because there is no 404 endpoint (client router
// handles 404s) and sending index.html is confusing
// expose vue bundles (prod)
app.use('/static', express.static(
  path.join(__dirname, '/vueapp/dist/static'),
  { maxAge: dayMs, fallthrough: false }
), ArkimeUtil.missingResource);
// expose vue bundle (dev)
app.use('/app.js', express.static(
  path.join(__dirname, '/vueapp/dist/app.js'),
  { fallthrough: false }
), ArkimeUtil.missingResource);
app.use('/app.js.map', express.static(
  path.join(__dirname, '/vueapp/dist/app.js.map'),
  { fallthrough: false }
), ArkimeUtil.missingResource);
// vue index page
app.use(cspHeader, setCookie, (req, res, next) => {
  if (req.path === '/users' && !req.user.hasRole('usersAdmin')) {
    return res.status(403).send('Permission denied');
  }

  const renderer = vueServerRenderer.createRenderer({
    template: fs.readFileSync(path.join(__dirname, '/vueapp/dist/index.html'), 'utf-8')
  });

  const appContext = {
    nonce: res.locals.nonce,
    version: version.version,
    path: getConfig('cont3xt', 'webBasePath', '/')
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
// Command Line Parsing
// ----------------------------------------------------------------------------
function processArgs (argv) {
  for (let i = 0, ilen = argv.length; i < ilen; i++) {
    if (argv[i] === '-c') {
      i++;
      internals.configFile = argv[i];
    } else if (process.argv[i] === '-o') {
      i++;
      const equal = process.argv[i].indexOf('=');
      if (equal === -1) {
        console.log('Missing equal sign in', process.argv[i]);
        process.exit(1);
      }

      internals.options[process.argv[i].slice(0, equal)] = process.argv[i].slice(equal + 1);
    } else if (argv[i] === '--insecure') {
      internals.insecure = true;
    } else if (argv[i] === '--debug') {
      internals.debug++;
    } else if (argv[i] === '--regressionTests') {
      internals.regressionTests = true;
    } else if (argv[i] === '--help') {
      console.log('cont3xt.js [<options>]');
      console.log('');
      console.log('Options:');
      console.log('  -c <file>                   Where to fetch the config file from');
      console.log('  -o <section>.<key>=<value>  Override the config file');
      console.log('  --debug                     Increase debug level, multiple are supported');
      console.log('  --insecure                  Disable certificate verification for https calls');

      process.exit(0);
    }
  }
}

// ----------------------------------------------------------------------------
// Config - temporary
// ----------------------------------------------------------------------------

User.prototype.getCont3xtKeys = function () {
  const v = this.cont3xt?.keys;

  if (!v) {
    return undefined;
  }

  return Auth.auth2obj(v, Auth.passwordSecret256);
};

User.prototype.setCont3xtKeys = function (v) {
  if (!this.cont3xt) {
    this.cont3xt = {};
  }
  this.cont3xt.keys = Auth.obj2auth(v, Auth.passwordSecret256);
  this.save((err) => { console.log('SAVED', err); });
};

function getConfig (section, sectionKey, d) {
  const key = `${section}.${sectionKey}`;
  const value = internals.options[key] ?? internals.config[section]?.[sectionKey] ?? d;

  if (internals.debug > 0 && internals.debugged[key] === undefined) {
    console.log(`CONFIG - ${key} is ${value}`);
    internals.debugged[key] = 1;
  }

  return value;
}

// ----------------------------------------------------------------------------
// Initialize stuff
// ----------------------------------------------------------------------------
function setupAuth () {
  let userNameHeader = getConfig('cont3xt', 'userNameHeader', 'anonymous');
  let mode;
  if (internals.regressionTests) {
    mode = 'regressionTests';
  } else if (userNameHeader === 'anonymous') {
    mode = 'anonymousWithDB';
  } else if (userNameHeader === 'digest') {
    mode = userNameHeader;
    userNameHeader = undefined;
  } else {
    mode = 'header';
  }

  Auth.initialize({
    debug: internals.debug,
    mode,
    userNameHeader,
    passwordSecret: getConfig('cont3xt', 'passwordSecret', 'password'),
    basePath: internals.webBasePath,
    httpRealm: getConfig('cont3xt', 'httpRealm')
  });

  const dbUrl = getConfig('cont3xt', 'dbUrl');
  const es = getConfig('cont3xt', 'elasticsearch', 'http://localhost:9200').split(',');
  const usersUrl = getConfig('cont3xt', 'usersUrl');
  let usersEs = getConfig('cont3xt', 'usersElasticsearch');

  Db.initialize({
    insecure: internals.insecure,
    debug: internals.debug,
    url: dbUrl,
    node: es,
    apiKey: getConfig('cont3xt', 'elasticsearchAPIKey'),
    basicAuth: getConfig('cont3xt', 'elasticsearchBasicAuth')
  });

  if (usersEs) {
    usersEs = usersEs.split(',');
  } else {
    usersEs = es;
  }

  User.initialize({
    insecure: internals.insecure,
    requestTimeout: getConfig('cont3xt', 'elasticsearchTimeout', 300),
    debug: internals.debug,
    url: usersUrl,
    node: usersEs,
    clientKey: getConfig('cont3xt', 'esClientKey'),
    clientCert: getConfig('cont3xt', 'esClientCert'),
    clientKeyPass: getConfig('cont3xt', 'esClientKeyPass'),
    prefix: getConfig('cont3xt', 'usersPrefix'),
    apiKey: getConfig('cont3xt', 'usersElasticsearchAPIKey'),
    basicAuth: getConfig('cont3xt', 'usersElasticsearchBasicAuth')
  });

  Audit.initialize({
    debug: internals.debug,
    expireHistoryDays: getConfig('cont3xt', 'expireHistoryDays', 180)
  });

  const cache = ArkimeCache.createCache({
    type: getConfig('cache', 'type', 'memory'),
    cacheSize: getConfig('cache', 'cacheSize', '100000'),
    cacheTimeout: getConfig('cache', 'cacheTimeout'),
    getConfig: (key, value) => getConfig('cache', key, value)
  });

  Integration.initialize({
    debug: internals.debug,
    cache,
    getConfig
  });
}

async function main () {
  processArgs(process.argv);
  try {
    internals.config = ini.parseSync(internals.configFile);
    if (internals.debug === 0) {
      internals.debug = parseInt(getConfig('cont3xt', 'debug', 0));
    }
    if (internals.debug) {
      console.log('Debug Level', internals.debug);
    }
    internals.webBasePath = getConfig('cont3xt', 'webBasePath', '/');
  } catch (err) {
    console.log(err);
    process.exit();
  }
  setupAuth();
  setupHSTS();

  let server;
  if (getConfig('cont3xt', 'keyFile') && getConfig('cont3xt', 'certFile')) {
    const keyFileData = fs.readFileSync(getConfig('cont3xt', 'keyFile'));
    const certFileData = fs.readFileSync(getConfig('cont3xt', 'certFile'));

    server = https.createServer({ key: keyFileData, cert: certFileData, secureOptions: crypto.constants.SSL_OP_NO_TLSv1 }, app);
  } else {
    server = http.createServer(app);
  }

  const userNameHeader = getConfig('cont3xt', 'userNameHeader', 'anonymous');
  const cont3xtHost = getConfig('cont3xt', 'cont3xtHost', undefined);
  if (userNameHeader !== 'anonymous' && cont3xtHost !== 'localhost' && cont3xtHost !== '127.0.0.1') {
    console.log('SECURITY WARNING - when userNameHeader is set, cont3xtHost should be localhost or use iptables');
  }

  server
    .on('error', (e) => {
      console.log("ERROR - couldn't listen on host %s port %d is cont3xt already running?", cont3xtHost, getConfig('cont3xt', 'port', 3218));
      process.exit(1);
    })
    .on('listening', (e) => {
      console.log('Express server listening on host %s port %d in %s mode', server.address().address, server.address().port, app.settings.env);
    })
    .listen(getConfig('cont3xt', 'port', 3218), cont3xtHost);
}

main();
