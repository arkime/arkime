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
const fs = require('fs');
const app = express();
const path = require('path');
const version = require('../common/version');
const User = require('../common/user');
const Auth = require('../common/auth');
const ArkimeCache = require('../common/arkimeCache');
const ArkimeUtil = require('../common/arkimeUtil');
const ArkimeConfig = require('../common/arkimeConfig');
const LinkGroup = require('./linkGroup');
const Integration = require('./integration');
const Audit = require('./audit');
const Overview = require('./overview');
const View = require('./view');
const Db = require('./db');
const jsonParser = ArkimeUtil.jsonParser;
const logger = require('morgan');
const favicon = require('serve-favicon');
const helmet = require('helmet');
const uuid = require('uuid').v4;
const dayMs = 60000 * 60 * 24;

const internals = {};

// Process args before routes
processArgs(process.argv);

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
    path: internals.webBasePath,
    sameSite: 'Strict',
    overwrite: true
  };

  if (getConfig('cont3xt', 'keyFile') && getConfig('cont3xt', 'certFile')) {
    cookieOptions.secure = true;
  }

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

if (ArkimeConfig.regressionTests) {
  app.post('/regressionTests/shutdown', (req, res) => {
    console.log('Shutting down');
    process.exit(0);
  });

  app.post('/regressionTests/classify', [jsonParser], (req, res) => {
    res.send(req.body.map(item => Integration.classify(item)));
  });
}

// Set up auth, all APIs registered below will use passport
Auth.app(app);

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
app.post('/api/users/csv', [jsonParser, User.checkRole('usersAdmin'), setCookie], User.apiGetUsersCSV);
app.post('/api/users/min', [jsonParser, checkCookieToken, User.checkAssignableRole], User.apiGetUsersMin);
app.post('/api/user', [jsonParser, checkCookieToken, User.checkRole('usersAdmin')], User.apiCreateUser);
app.post('/api/user/password', [jsonParser, checkCookieToken, Auth.getSettingUserDb], User.apiUpdateUserPassword);
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

app.get('/api/overview', Overview.apiGet);
app.put('/api/overview', [jsonParser, checkCookieToken], Overview.apiCreate);
app.put('/api/overview/:id', [jsonParser, checkCookieToken], Overview.apiUpdate);
app.delete('/api/overview/:id', [jsonParser, checkCookieToken], Overview.apiDelete);

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
 * @returns {object} selectedOverviews - A mapping of the selected overview per iType, of shape {[iType]: overviewId}
 */
function apiGetSettings (req, res, next) {
  const cont3xt = req.user.cont3xt ?? {};
  res.send({
    success: true,
    settings: cont3xt.settings ?? {},
    linkGroup: cont3xt.linkGroup ?? {},
    selectedOverviews: cont3xt.selectedOverviews ?? {}
  });
}

// verify selectedOverviews, on error returns { msg: <errorMsg> }, on success returns { selectedOverviews }
function verifySelectedOverviews (selectedOverviews) {
  selectedOverviews = (
    ({ // only allow these properties in selectedOverviews
      domain, ip, url, email, phone, hash, text
    }) => ({ domain, ip, url, email, phone, hash, text })
  )(selectedOverviews);

  if (typeof selectedOverviews !== 'object') {
    return { msg: 'selectedOverviews must be an object' };
  }

  for (const selectedId of Object.values(selectedOverviews)) {
    if (!ArkimeUtil.isString(selectedId)) {
      return { msg: 'values in selectedOverviews must be string ids' };
    }
  }

  return { selectedOverviews };
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

    if (req.body?.selectedOverviews) {
      const { msg, selectedOverviews } = verifySelectedOverviews(req.body.selectedOverviews);
      if (msg) { return res.send({ success: false, text: msg }); }

      user.cont3xt.selectedOverviews = selectedOverviews;
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
    path: internals.webBasePath,
    disableUserPasswordUI: getConfig('cont3xt', 'disableUserPasswordUI', true)
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

// Replace the default express error handler
app.use(ArkimeUtil.expressErrorHandler);

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
    } else if (argv[i] === '--help') {
      console.log('cont3xt.js [<options>]');
      console.log('');
      console.log('Options:');
      console.log('  -c, --config <file|url>     Where to fetch the config file from');
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

const getConfig = ArkimeConfig.get;

// ----------------------------------------------------------------------------
// Initialize stuff
// ----------------------------------------------------------------------------
async function setupAuth () {
  Auth.initialize('cont3xt', {
    appAdminRole: 'cont3xtAdmin',
    passwordSecretSection: 'cont3xt',
    basePath: internals.webBasePath
  });

  const dbUrl = getConfig('cont3xt', 'dbUrl');
  const es = getConfig('cont3xt', 'elasticsearch', 'http://localhost:9200').split(',');
  const usersUrl = getConfig('cont3xt', 'usersUrl');
  let usersEs = getConfig('cont3xt', 'usersElasticsearch');

  await Db.initialize({
    insecure: ArkimeConfig.insecure,
    url: dbUrl,
    node: es,
    caTrustFile: getConfig('cont3xt', 'caTrustFile'),
    apiKey: getConfig('cont3xt', 'elasticsearchAPIKey'),
    basicAuth: getConfig('cont3xt', 'elasticsearchBasicAuth')
  });

  if (usersEs) {
    usersEs = usersEs.split(',');
  } else {
    usersEs = es;
  }

  User.initialize({
    insecure: ArkimeConfig.insecure,
    requestTimeout: getConfig('cont3xt', 'elasticsearchTimeout', 300),
    url: usersUrl,
    node: usersEs,
    caTrustFile: getConfig('cont3xt', 'caTrustFile'),
    clientKey: getConfig('cont3xt', 'esClientKey'),
    clientCert: getConfig('cont3xt', 'esClientCert'),
    clientKeyPass: getConfig('cont3xt', 'esClientKeyPass'),
    prefix: getConfig('cont3xt', 'usersPrefix'),
    apiKey: getConfig('cont3xt', 'usersElasticsearchAPIKey'),
    basicAuth: getConfig('cont3xt', 'usersElasticsearchBasicAuth', getConfig('cont3xt', 'elasticsearchBasicAuth'))
  });

  Audit.initialize({
    expireHistoryDays: getConfig('cont3xt', 'expireHistoryDays', 180)
  });

  Overview.initialize();

  const cache = ArkimeCache.createCache({
    type: getConfig('cache', 'type', 'memory'),
    cacheSize: getConfig('cache', 'cacheSize', '100000'),
    cacheTimeout: getConfig('cache', 'cacheTimeout'),
    getConfig: (key, value) => getConfig('cache', key, value)
  });

  Integration.initialize({
    cache
  });
}

async function main () {
  try {
    await ArkimeConfig.initialize({ defaultConfigFile: `${version.config_prefix}/etc/cont3xt.ini` });

    if (ArkimeConfig.debug === 0) {
      ArkimeConfig.debug = parseInt(getConfig('cont3xt', 'debug', 0));
    }
    if (ArkimeConfig.debug) {
      console.log('Debug Level', ArkimeConfig.debug);
    }
    internals.webBasePath = getConfig('cont3xt', 'webBasePath', '/');
  } catch (err) {
    console.log(err);
    process.exit();
  }
  await setupAuth();
  setupHSTS();

  const cont3xtHost = ArkimeConfig.get('cont3xt', 'cont3xtHost');
  if (Auth.mode === 'header' && cont3xtHost !== 'localhost' && cont3xtHost !== '127.0.0.1') {
    console.log('SECURITY WARNING - When using header auth, cont3xtHost should be localhost or use iptables');
  }

  ArkimeUtil.createHttpServer('cont3xt', app, cont3xtHost, ArkimeConfig.get('cont3xt', 'port', 3218));
}

main();
