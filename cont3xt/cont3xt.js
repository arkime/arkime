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
const version = require('../viewer/version');
const User = require('../common/user');
const Auth = require('../common/auth');
const ArkimeCache = require('../common/arkimeCache');
const LinkGroup = require('./linkGroup');
const Integration = require('./integration');
const Db = require('./db');
const bp = require('body-parser');
const jsonParser = bp.json();
// eslint-disable-next-line no-shadow
const crypto = require('crypto');
const logger = require('morgan');
const favicon = require('serve-favicon');
const dayMs = 60000 * 60 * 24;

const internals = {
  configFile: `${version.config_prefix}/etc/cont3xt.ini`,
  debug: 0,
  insecure: false,
  regressionTests: false
};

// ----------------------------------------------------------------------------
// Logging
// ----------------------------------------------------------------------------
app.use(logger(':date :username \x1b[1m:method\x1b[0m \x1b[33m:url\x1b[0m :res[content-length] bytes :response-time ms'));

logger.token('username', (req, res) => {
  return req.user ? req.user.userId : '-';
});

// ----------------------------------------------------------------------------
// Middleware
// ----------------------------------------------------------------------------
// missing resource error handler for static file endpoints
function missingResource (err, req, res, next) {
  res.status(404);
  const msg = `Cannot locate resource requsted from ${req.path}`;
  console.log(msg);
  return res.send(msg);
}

// ----------------------------------------------------------------------------
// Routes
// ----------------------------------------------------------------------------
// assets and fonts
// using fallthrough: false because there is no 404 endpoint (client router
// handles 404s) and sending index.html is confusing
app.use('/font-awesome', express.static(
  path.join(__dirname, '/../node_modules/font-awesome'),
  { maxAge: dayMs, fallthrough: false }
), missingResource);
app.use('/assets', express.static(
  path.join(__dirname, '/../assets'),
  { maxAge: dayMs, fallthrough: false }
), missingResource);
app.use('/public', express.static(
  path.join(__dirname, '/public'),
  { maxAge: dayMs, fallthrough: false }
), missingResource);
const integrationsStatic = express.static(
  path.join(__dirname, '/integrations'),
  { maxAge: dayMs, fallthrough: false }
);
app.use('/integrations', (req, res, next) => {
  if (req.url.endsWith('png')) {
    return integrationsStatic(req, res, (err) => { missingResource(err, req, res); });
  }
  return missingResource('Not png', req, res);
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

// check for cont3xt user
app.use(async (req, res, next) => {
  if (!req.user.hasRole('cont3xtUser')) {
    return res.send('NO!');
  }
  next();
});

app.get('/api/linkGroup', LinkGroup.apiGet);
app.put('/api/linkGroup', [jsonParser], LinkGroup.apiCreate);
app.put('/api/linkGroup/:id', [jsonParser], LinkGroup.apiUpdate);
app.delete('/api/linkGroup/:id', [jsonParser], LinkGroup.apiDelete);

app.get('/api/roles', User.apiRoles);
app.get('/api/user', User.apiGetUser);
app.post('/api/users', [jsonParser, User.checkRole('usersAdmin')], User.apiGetUsers);
app.post('/api/user', [jsonParser, User.checkRole('usersAdmin')], User.apiCreateUser);
app.delete('/api/user/:id', [jsonParser, User.checkRole('usersAdmin')], User.apiDeleteUser);
app.post('/api/user/:id', [jsonParser, User.checkRole('usersAdmin')], User.apiUpdateUser);

app.get('/api/integration', Integration.apiList);
app.post('/api/integration/search', [jsonParser], Integration.apiSearch);
app.post('/api/integration/:itype/:integration/search', [jsonParser], Integration.apiSingleSearch);
app.get('/api/settings', apiGetSettings);
app.put('/api/settings', [jsonParser], apiPutSettings);
app.get('/api/integration/settings', Integration.apiGetSettings);
app.put('/api/integration/settings', [jsonParser], Integration.apiPutSettings);
app.get('/api/integration/stats', Integration.apiStats);

// ----------------------------------------------------------------------------
// Cont3xt Web APIs
// ----------------------------------------------------------------------------

/**
 * Return all the cont3xt settings
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
 * Save all the cont3xt settings
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
), missingResource);
// expose vue bundle (dev)
app.use('/app.js', express.static(
  path.join(__dirname, '/vueapp/dist/app.js'),
  { fallthrough: false }
), missingResource);
app.use('/app.js.map', express.static(
  path.join(__dirname, '/vueapp/dist/app.js.map'),
  { fallthrough: false }
), missingResource);
// vue index page
app.use((req, res, next) => {
  const renderer = vueServerRenderer.createRenderer({
    template: fs.readFileSync(path.join(__dirname, '/vueapp/dist/index.html'), 'utf-8')
  });

  const appContext = {
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
      console.log('  -c                    config file');
      console.log('  --debug               Increase debug level, multiple are supported');
      console.log('  --insecure            Disable cert verification');

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
  if (!internals.config[section]) {
    return d;
  }
  return internals.config[section][sectionKey] ?? d;
}

// ----------------------------------------------------------------------------
// Initialize stuff
// ----------------------------------------------------------------------------
//
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
    mode: mode,
    userNameHeader: userNameHeader,
    passwordSecret: getConfig('cont3xt', 'passwordSecret', 'password')
  });

  const dbUrl = getConfig('cont3xt', 'dbUrl');
  const es = getConfig('cont3xt', 'elasticsearch', 'http://localhost:9200').split(',');
  const usersUrl = getConfig('cont3xt', 'usersUrl');
  let usersEs = getConfig('cont3xt', 'usersElasticsearch');

  Db.initialize({
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
    prefix: getConfig('cont3xt', 'usersPrefix', ''),
    apiKey: getConfig('cont3xt', 'usersElasticsearchAPIKey'),
    basicAuth: getConfig('cont3xt', 'usersElasticsearchBasicAuth')
  });

  const cache = ArkimeCache.createCache({
    type: getConfig('cache', 'type', 'memory'),
    cacheSize: getConfig('cache', 'cacheSize', '100000'),
    cacheTimeout: getConfig('cache', 'cacheTimeout'),
    getConfig: (key, value) => getConfig('cache', key, value)
  });

  Integration.initialize({
    debug: internals.debug,
    cache: cache,
    getConfig: getConfig
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
  } catch (err) {
    console.log(err);
    process.exit();
  }
  setupAuth();

  let server;
  if (getConfig('cont3xt', 'keyFile') && getConfig('cont3xt', 'certFile')) {
    const keyFileData = fs.readFileSync(getConfig('cont3xt', 'keyFile'));
    const certFileData = fs.readFileSync(getConfig('cont3xt', 'certFile'));

    server = https.createServer({ key: keyFileData, cert: certFileData, secureOptions: crypto.constants.SSL_OP_NO_TLSv1 }, app);
  } else {
    server = http.createServer(app);
  }

  server
    .on('error', (e) => {
      console.log("ERROR - couldn't listen on port", getConfig('cont3xt', 'port', 3218), 'is cont3xt already running?');
      process.exit(1);
    })
    .on('listening', (e) => {
      console.log('Express server listening on port %d in %s mode', server.address().port, app.settings.env);
    })
    .listen(getConfig('cont3xt', 'port', 3218));
}

main();
