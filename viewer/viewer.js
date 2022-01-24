/******************************************************************************/
/* viewer.js  -- The main arkime app
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

const MIN_DB_VERSION = 71;

// ============================================================================
// MODULES
// ============================================================================
const Config = require('./config.js');
const express = require('express');
const fs = require('fs');
const fse = require('fs-ext');
const async = require('async');
const Pcap = require('./pcap.js');
const Db = require('./db.js');
const molochparser = require('./molochparser.js');
const version = require('./version');
const http = require('http');
const https = require('https');
const onHeaders = require('on-headers');
const helmet = require('helmet');
const uuid = require('uuidv4').default;
const path = require('path');
const dayMs = 60000 * 60 * 24;
const User = require('../common/user');
const Auth = require('../common/auth');
const ArkimeUtil = require('../common/arkimeUtil');

if (typeof express !== 'function') {
  console.log("ERROR - Need to run 'npm update' in viewer directory");
  process.exit(1);
}

// express app
const app = express();

// ============================================================================
// CONFIG & APP SETUP
// ============================================================================

// app.configure
const logger = require('morgan');
const favicon = require('serve-favicon');
const bodyParser = require('body-parser');
const multer = require('multer');
const methodOverride = require('method-override');
const compression = require('compression');

// internal app deps
const { internals } = require('./internals')(app, Config);
const ViewerUtils = require('./viewerUtils')(Config, Db, internals);
const notifierAPIs = require('./apiNotifiers')(Config, Db, internals);
const sessionAPIs = require('./apiSessions')(Config, Db, internals, ViewerUtils);
const connectionAPIs = require('./apiConnections')(Config, Db, ViewerUtils, sessionAPIs);
const statsAPIs = require('./apiStats')(Config, Db, internals, ViewerUtils);
const huntAPIs = require('./apiHunts')(Config, Db, internals, notifierAPIs, sessionAPIs, ViewerUtils);
const userAPIs = require('./apiUsers')(Config, Db, internals, ViewerUtils);
const historyAPIs = require('./apiHistory')(Db);
const shortcutAPIs = require('./apiShortcuts')(Db, internals, ViewerUtils);
const miscAPIs = require('./apiMisc')(Config, Db, internals, sessionAPIs, userAPIs, ViewerUtils);

// registers a get and a post
app.getpost = (route, mw, func) => { app.get(route, mw, func); app.post(route, mw, func); };
app.deletepost = (route, mw, func) => { app.delete(route, mw, func); app.post(route, mw, func); };
app.enable('jsonp callback');
app.set('views', path.join(__dirname, '/views'));
app.set('view engine', 'pug');

app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ limit: '5mb', extended: true }));

app.use(compression());
app.use(methodOverride());

// Explicit sigint handler for running under docker
// See https://github.com/nodejs/node/issues/4182
process.on('SIGINT', function () {
  process.exit();
});

// app security options -------------------------------------------------------
const iframeOption = Config.get('iframe', 'deny');
if (iframeOption === 'sameorigin' || iframeOption === 'deny') {
  app.use(helmet.frameguard({ action: iframeOption }));
} else {
  app.use(helmet.frameguard({
    action: 'allow-from',
    domain: iframeOption
  }));
}

app.use(helmet.hidePoweredBy());
app.use(helmet.xssFilter());
if (Config.get('hstsHeader', false) && Config.isHTTPS()) {
  app.use(helmet.hsts({
    maxAge: 31536000,
    includeSubDomains: true
  }));
}

// calculate nonce
app.use((req, res, next) => {
  res.locals.nonce = Buffer.from(uuid()).toString('base64');
  next();
});

// define csp headers
const cspDirectives = {
  defaultSrc: ["'self'"],
  // need unsafe-inline for jquery flot (https://github.com/flot/flot/issues/1574, https://github.com/flot/flot/issues/828)
  styleSrc: ["'self'", "'unsafe-inline'"],
  // need unsafe-eval for vue full build: https://vuejs.org/v2/guide/installation.html#CSP-environments
  scriptSrc: ["'self'", "'unsafe-eval'", (req, res) => `'nonce-${res.locals.nonce}'`],
  objectSrc: ["'none'"],
  imgSrc: ["'self'", 'data:']
};
const cspHeader = helmet.contentSecurityPolicy({
  directives: cspDirectives
});
const cyberchefCspHeader = helmet.contentSecurityPolicy({
  directives: {
    defaultSrc: ["'self'"],
    styleSrc: ["'self'", "'unsafe-inline'"],
    scriptSrc: ["'self'", "'unsafe-eval'", "'unsafe-inline'"],
    objectSrc: ["'self'", 'data:']
  }
});

// logging --------------------------------------------------------------------
// send req to access log file or stdout
let _stream = process.stdout;
const _accesslogfile = Config.get('accessLogFile');
if (_accesslogfile) {
  _stream = fs.createWriteStream(_accesslogfile, { flags: 'a' });
}

const _loggerFormat = decodeURIComponent(Config.get(
  'accessLogFormat',
  ':date :username %1b[1m:method%1b[0m %1b[33m:url%1b[0m :status :res[content-length] bytes :response-time ms'
));
const _suppressPaths = Config.getArray('accessLogSuppressPaths', ';', '');

app.use(logger(_loggerFormat, {
  stream: _stream,
  skip: (req, res) => { return _suppressPaths.includes(req.path); }
}));

logger.token('username', (req, res) => {
  return req.user ? req.user.userId : '-';
});

// appwide middleware ---------------------------------------------------------
app.use((req, res, next) => {
  res.serverError = serverError;

  req.url = req.url.replace(Config.basePath(), '/');
  return next();
});

// client static files --------------------------------------------------------
app.use(favicon(path.join(__dirname, '/public/favicon.ico')));
// using fallthrough: false because there is no 404 endpoint (client router
// handles 404s) and sending index.html is confusing
app.use('/font-awesome', express.static(
  path.join(__dirname, '/../node_modules/font-awesome'),
  { maxAge: dayMs, fallthrough: false }
), missingResource);
app.use(['/assets', '/logos'], express.static(
  path.join(__dirname, '../assets'),
  { maxAge: dayMs, fallthrough: false }
), missingResource);

// regression test methods, before auth checks --------------------------------
if (Config.get('regressionTests')) {
  // Override default lastUsed min write internal for tests
  User.lastUsedMinInterval = 1000;

  app.post('/regressionTests/shutdown', function (req, res) {
    Db.close();
    process.exit(0);
  });
  app.post('/regressionTests/flushCache', function (req, res) {
    Db.flushCache();
    res.send('{}');
  });
  app.get('/regressionTests/processCronQueries', function (req, res) {
    internals.processCronQueries();
    res.send('{}');
  });
  // Make sure all jobs have run and return
  app.get('/regressionTests/processHuntJobs', async function (req, res) {
    await Db.flush();
    await Db.refresh();
    huntAPIs.processHuntJobs();

    setTimeout(function checkHuntFinished () {
      if (internals.runningHuntJob) {
        setTimeout(checkHuntFinished, 1000);
      } else {
        Db.search('hunts', 'hunt', { query: { terms: { status: ['running', 'queued'] } } }, async function (err, result) {
          if (result.hits.total > 0) {
            huntAPIs.processHuntJobs();
            await Db.refresh();
            setTimeout(checkHuntFinished, 1000);
          } else {
            await Db.refresh();
            res.send('{}');
          }
        });
      }
    }, 1000);
  });
  app.get('/regressionTests/deleteAllUsers', User.apiDeleteAllUsers);
  app.get('/regressionTests/getUser/:user', (req, res) => {
    User.getUser(req.params.user, (err, user) => {
      res.send(user);
    });
  });
}

// load balancer test - no authj ----------------------------------------------
app.use('/_ns_/nstest.html', function (req, res) {
  res.end();
});

// parliament apis - no auth --------------------------------------------------
app.get(
  ['/api/parliament', '/parliament.json'],
  [ArkimeUtil.noCacheJson],
  statsAPIs.getParliament
);

// stats apis - no auth -------------------------------------------------------
app.get( // es health endpoint
  ['/api/eshealth', '/eshealth.json'],
  statsAPIs.getESHealth
);

// password, testing, or anonymous mode setup ---------------------------------
if (Config.get('passwordSecret')) {
  app.use(function (req, res, next) {
    // S2S Auth
    if (req.headers['x-arkime-auth'] || req.headers['x-moloch-auth']) {
      return Auth.s2sAuth(req, res, next);
    }

    if (req.url.match(/^\/receiveSession/) || req.url.match(/^\/api\/sessions\/receive/)) {
      return res.send('receive session only allowed s2s');
    }

    // Header auth
    if (internals.userNameHeader !== undefined) {
      if (req.headers[Auth.userNameHeader] === undefined) {
        if (Auth.debug > 0) {
          console.log('DEBUG - Couldn\'t find userNameHeader of', internals.userNameHeader, 'in', req.headers, 'for', req.url);
        }
      } else {
        Auth.headerAuth(req, res, next);
        return; // Don't try browser auth
      }
    }

    // Browser auth
    return Auth.digestAuth(req, res, next);
  });
} else if (Config.get('regressionTests', false)) {
  console.log('WARNING - The setting "regressionTests" is set to true, do NOT use in production, for testing only');
  internals.noPasswordSecret = true;
  app.use(Auth.regressionTestsAuth);
} else {
  /* Shared password isn't set, who cares about auth, db is only used for settings */
  console.log('WARNING - The setting "passwordSecret" is not set, all access is anonymous');
  internals.noPasswordSecret = true;
  app.use(Auth.anonymousWithDBAuth);
}

// ============================================================================
// UTILITY
// ============================================================================
function parseCustomView (key, input) {
  const fieldsMap = Config.getFieldsMap();

  let match = input.match(/require:([^;]+)/);
  if (!match) {
    console.log(`custom-view ${key} missing require section`);
    process.exit(1);
  }
  const req = match[1];

  match = input.match(/title:([^;]+)/);
  const title = match[1] || key;

  match = input.match(/fields:([^;]+)/);
  if (!match) {
    console.log(`custom-view ${key} missing fields section`);
    process.exit(1);
  }
  const fields = match[1];

  const parts = req.split('.');
  let output = '  if (';
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
  output += `    div.sessionDetailMeta.bold ${title}\n    dl.sessionDetailMeta\n`;

  for (const field of fields.split(',')) {
    const info = fieldsMap[field];
    if (!info) {
      continue;
    }
    const pos = info.dbField.lastIndexOf('.');
    if (pos === -1) {
      output += `      +arrayList(session, '${info.dbField}', '${info.friendlyName}', '${field}')\n`;
    } else {
      output += `      +arrayList(session.${info.dbField.slice(0, pos)}, '${info.dbField.slice(pos + 1)}', '${info.friendlyName}', '${field}')\n`;
    }
  }

  return output;
}

function createSessionDetail () {
  const found = {};
  let dirs = [];

  dirs = dirs.concat(Config.getArray('pluginsDir', ';', `${version.config_prefix}/plugins`));
  dirs = dirs.concat(Config.getArray('parsersDir', ';', `${version.config_prefix}/parsers`));

  dirs.forEach(function (dir) {
    try {
      const files = fs.readdirSync(dir);
      // sort().reverse() so in this dir pug is processed before jade
      files.sort().reverse().forEach(function (file) {
        const sfile = file.replace(/\.(pug|jade)/, '');
        if (found[sfile]) {
          return;
        }
        if (file.match(/\.detail\.jade$/i)) {
          found[sfile] = fs.readFileSync(dir + '/' + file, 'utf8').replace(/^/mg, '  ') + '\n';
        } else if (file.match(/\.detail\.pug$/i)) {
          found[sfile] = '  include ' + dir + '/' + file + '\n';
        }
      });
    } catch (e) {}
  });

  const customViews = Config.keys('custom-views') || [];

  for (const key of customViews) {
    const view = Config.sectionGet('custom-views', key);
    found[key] = parseCustomView(key, view);
  }

  const makers = internals.pluginEmitter.listeners('makeSessionDetail');
  async.each(makers, function (cb, nextCb) {
    cb(function (err, items) {
      for (const k in items) {
        found[k] = items[k].replace(/^/mg, '  ') + '\n';
      }
      return nextCb();
    });
  }, function () {
    internals.sessionDetailNew = 'include views/mixins.pug\n' +
                                 'div.session-detail(sessionid=session.id,hidePackets=hidePackets)\n' +
                                 '  include views/sessionDetail\n';
    Object.keys(found).sort().forEach(function (k) {
      internals.sessionDetailNew += found[k];
    });

    internals.sessionDetailNew = internals.sessionDetailNew.replace(/div.sessionDetailMeta.bold/g, 'h4.sessionDetailMeta')
      .replace(/dl.sessionDetailMeta/g, 'dl')
      .replace(/a.moloch-right-click.*molochexpr='([^']+)'.*#{(.*)}/g, "+clickableValue('$1', $2)")
    ;
  });
}

function createRightClicks () {
  const mrc = Config.configMap('right-click');
  for (const key in mrc) {
    if (mrc[key].fields) {
      mrc[key].fields = mrc[key].fields.split(',');
    }
    if (mrc[key].users) {
      const users = {};
      for (const item of mrc[key].users.split(',')) {
        users[item] = 1;
      }
      mrc[key].users = users;
    }
  }
  const makers = internals.pluginEmitter.listeners('makeRightClick');
  async.each(makers, function (cb, nextCb) {
    cb(function (err, items) {
      for (const k in items) {
        mrc[k] = items[k];
        if (mrc[k].fields && !Array.isArray(mrc[k].fields)) {
          mrc[k].fields = mrc[k].fields.split(',');
        }
      }
      return nextCb();
    });
  }, function () {
    internals.rightClicks = mrc;
  });
}

// ============================================================================
// API MIDDLEWARE
// ============================================================================
// error middleware -----------------------------------------------------------
function serverError (resStatus, text) {
  this.status(resStatus || 403);
  return this.send(JSON.stringify({ success: false, text: text }));
}

// missing resource error handler for static file endpoints
function missingResource (err, req, res, next) {
  res.status(404);
  const msg = `Cannot locate resource requsted from ${req.path}`;
  console.log(msg);
  return res.send(msg);
}

// security/access middleware -------------------------------------------------
function checkProxyRequest (req, res, next) {
  sessionAPIs.isLocalView(req.params.nodeName, function () {
    return next();
  },
  function () {
    return sessionAPIs.proxyRequest(req, res);
  });
}

function setCookie (req, res, next) {
  const cookieOptions = {
    path: Config.basePath(),
    sameSite: 'Strict',
    overwrite: true
  };

  if (Config.isHTTPS()) { cookieOptions.secure = true; }

  res.cookie( // send cookie for basic, non admin functions
    'ARKIME-COOKIE',
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
  if (!req.headers['x-arkime-cookie']) {
    return res.serverError(500, 'Missing token');
  }

  const cookie = req.headers['x-arkime-cookie'];
  req.token = Auth.auth2obj(cookie);
  const diff = Math.abs(Date.now() - req.token.date);
  if (diff > 2400000 || /* req.token.pid !== process.pid || */
      req.token.userId !== req.user.userId) {
    console.trace('bad token', req.token);
    return res.serverError(500, 'Timeout - Please try reloading page and repeating the action');
  }

  return next();
}

// use for APIs that can be used from places other than just the UI
function checkHeaderToken (req, res, next) {
  if (req.headers.cookie) { // if there's a cookie, check header
    return checkCookieToken(req, res, next);
  } else { // if there's no cookie, just continue so the API still works
    return next();
  }
}

function checkPermissions (permissions) {
  const inversePermissions = {
    hidePcap: true,
    hideFiles: true,
    hideStats: true,
    disablePcapDownload: true
  };

  return (req, res, next) => {
    for (const permission of permissions) {
      if ((!req.user[permission] && !inversePermissions[permission]) ||
        (req.user[permission] && inversePermissions[permission])) {
        console.log(`Permission denied to ${req.user.userId} while requesting resource: ${req._parsedUrl.pathname}, using permission ${permission}`);
        return res.serverError(403, 'You do not have permission to access this resource');
      }
    }
    next();
  };
}

// used to disable endpoints in multi es mode
function disableInMultiES (req, res, next) {
  if (Config.get('multiES', false)) {
    return res.serverError(401, 'Not supported in multies');
  }
  return next();
}

async function checkHuntAccess (req, res, next) {
  if (req.user.createEnabled) {
    // an admin can do anything to any hunt
    return next();
  } else {
    try {
      const { body: huntHit } = await Db.getHunt(req.params.id);

      if (!huntHit || !huntHit.found) { throw new Error('Hunt not found'); }

      if (huntHit._source.userId === req.user.userId) {
        return next();
      }

      return res.serverError(403, 'You cannot change another user\'s hunt unless you have admin privileges');
    } catch (err) {
      console.log('ERROR - fetching hunt to check access', err);
      return res.serverError(500, err);
    }
  }
}

async function checkCronAccess (req, res, next) {
  if (req.user.createEnabled) {
    // an admin can do anything to any query
    return next();
  } else {
    try {
      const { body: query } = Db.get('queries', 'query', req.body.key);
      if (query._source.creator === req.user.userId) {
        return next();
      }
      return res.serverError(403, 'You cannot change another user\'s query unless you have admin privileges');
    } catch (err) {
      return res.serverError(403, 'Unknown query');
    }
  }
}

function checkEsAdminUser (req, res, next) {
  if (internals.esAdminUsersSet) {
    if (internals.esAdminUsers.includes(req.user.userId)) {
      return next();
    }
  } else {
    if (req.user.createEnabled && Config.get('multiES', false) === false) {
      return next();
    }
  }
  return res.serverError(403, 'You do not have permission to access this resource');
}

// log middleware -------------------------------------------------------------
function logAction (uiPage) {
  return (req, res, next) => {
    const log = {
      timestamp: Math.floor(Date.now() / 1000),
      method: req.method,
      userId: req.user.userId,
      api: req._parsedUrl.pathname,
      expression: req.query.expression,
      query: ''
    };

    const avoidProps = {
      password: true, newPassword: true, currentPassword: true, cancelId: true
    };

    // parse query from req.query because query params might only be in body
    // and put into req.query by fillQueryFromBody, so you might not find them
    // in req._parsedUrl.query. need query for opening history item
    for (const item in req.query) {
      if (!avoidProps[item]) {
        log.query += `${item}=${req.query[item]}&`;
      }
    }
    log.query = log.query.slice(0, -1);

    if (req.user.expression) {
      log.forcedExpression = req.user.expression;
    }

    if (uiPage) { log.uiPage = uiPage; }

    if (req.query.date && parseInt(req.query.date) === -1) {
      log.range = log.timestamp;
    } else if (req.query.startTime && req.query.stopTime) {
      log.range = req.query.stopTime - req.query.startTime;
    }

    if (req.query.view && req.user.views) {
      const view = req.user.views[req.query.view];
      if (view) {
        log.view = {
          name: req.query.view,
          expression: view.expression
        };
      }
    }

    // save the request body
    const bodyClone = {};

    for (const key in req.body) {
      if (req.body[key] && !avoidProps[key]) {
        bodyClone[key] = req.body[key];
      }
    }

    if (Object.keys(bodyClone).length > 0) {
      log.body = bodyClone;
    }

    res.logCounts = (recordsReturned, recordsFiltered, recordsTotal) => {
      log.recordsTotal = recordsTotal;
      log.recordsReturned = recordsReturned;
      log.recordsFiltered = recordsFiltered;
    };

    req._molochStartTime = new Date();

    function finish () {
      res.removeListener('finish', finish);

      log.queryTime = new Date() - req._molochStartTime;

      if (req._molochESQuery) { log.esQuery = req._molochESQuery; }
      if (req._molochESQueryIndices) { log.esQueryIndices = req._molochESQueryIndices; }

      try {
        Db.historyIt(log);
      } catch (err) {
        console.log('log history error', err);
      }
    }

    res.on('finish', finish);

    return next();
  };
}

// field to exp middleware ----------------------------------------------------
function fieldToExp (req, res, next) {
  if (req.query.exp && !req.query.field) {
    const field = Config.getFieldsMap()[req.query.exp];
    if (field) {
      req.query.field = field.dbField;
    } else {
      req.query.field = req.query.exp;
    }
  }

  return next();
}

// response time middleware ---------------------------------------------------
// record the time it took from the request to start
// until the headers are set to send the response
function recordResponseTime (req, res, next) {
  onHeaders(res, () => {
    const now = process.hrtime();
    let ms = ((now[0] - req._startAt[0]) * 1000) + ((now[1] - req._startAt[1]) / 1000000);
    ms = Math.ceil(ms);
    res.setHeader('X-Arkime-Response-Time', ms);
  });

  next();
}

// query middleware -----------------------------------------------------------
// merge query and body objects into query parameters (duplicate params use body)
// to support both POST and GET requests for endpoints using this middleware
// POST sends req.body (and might have req.query too) and GET sends req.query
// all endpoints that use POST and GET (app.getpost) should look for req.query
function fillQueryFromBody (req, res, next) {
  if (req.method === 'POST') {
    const query = { // last object property overwrites the previous one
      ...req.query,
      ...req.body
    };
    req.query = query;
  }
  if (Config.debug > 1) {
    console.log(`${req.url} query`, req.query);
  }
  next();
}

// user middleware ------------------------------------------------------------
// express middleware to set req.settingUser to who to work on, depending if admin or not
// This returns the cached user
function getSettingUserCache (req, res, next) {
  // If no userId parameter, or userId is ourself then req.user already has our info
  if (req.query.userId === undefined || req.query.userId === req.user.userId) {
    req.settingUser = req.user;
    return next();
  }

  // user is trying to get another user's settings without admin privilege
  if (!req.user.createEnabled) { return res.serverError(403, 'Need admin privileges'); }

  User.getUserCache(req.query.userId, (err, user) => {
    if (err || !user) {
      if (internals.noPasswordSecret) {
        req.settingUser = JSON.parse(JSON.stringify(req.user));
        delete req.settingUser.found;
      } else {
        req.settingUser = null;
      }
      return next();
    }
    req.settingUser = user;
    return next();
  });
}

// express middleware to set req.settingUser to who to work on, depending if admin or not
// This returns fresh from db
function getSettingUserDb (req, res, next) {
  let userId;

  if (req.query.userId === undefined || req.query.userId === req.user.userId) {
    if (Config.get('regressionTests', false)) {
      req.settingUser = req.user;
      return next();
    }

    userId = req.user.userId;
  } else if (!req.user.createEnabled) {
    // user is trying to get another user's settings without admin privilege
    return res.serverError(403, 'Need admin privileges');
  } else {
    userId = req.query.userId;
  }

  User.getUser(userId, function (err, user) {
    if (err || !user) {
      if (internals.noPasswordSecret) {
        req.settingUser = JSON.parse(JSON.stringify(req.user));
        delete req.settingUser.found;
      } else {
        return res.serverError(403, 'Unknown user');
      }
      return next();
    }
    req.settingUser = user;
    return next();
  });
}

// view middleware ------------------------------------------------------------
// remove the string, 'shared:', that is added to shared views with the same
// name as a user's personal view in the endpoint '/user/views'
// also remove any special characters except ('-', '_', ':', and ' ')
function sanitizeViewName (req, res, next) {
  if (req.body.name) {
    req.body.name = req.body.name.replace(/(^(shared:)+)|[^-a-zA-Z0-9_: ]/g, '');
  }
  next();
}

// ============================================================================
// HELPERS
// ============================================================================
// app helpers ----------------------------------------------------------------
function setFieldLocals () {
  ViewerUtils.loadFields()
    .then((result) => {
      internals.fieldsMap = result.fieldsMap;
      internals.fieldsArr = result.fieldsArr;
      createSessionDetail();
    });
}

function loadPlugins () {
  const api = {
    registerWriter: function (str, info) {
      internals.writers[str] = info;
    },
    getDb: function () { return Db; },
    getPcap: function () { return Pcap; }
  };
  const plugins = Config.getArray('viewerPlugins', ';', '');
  const dirs = Config.getArray('pluginsDir', ';', `${version.config_prefix}/plugins`);
  plugins.forEach(function (plugin) {
    plugin = plugin.trim();
    if (plugin === '') {
      return;
    }
    let found = false;
    dirs.forEach(function (dir) {
      dir = dir.trim();
      if (found || dir === '') {
        return;
      }
      if (fs.existsSync(dir + '/' + plugin)) {
        found = true;
        const p = require(dir + '/' + plugin);
        p.init(Config, internals.pluginEmitter, api);
      }
    });
    if (!found) {
      console.log("WARNING - Couldn't find plugin", plugin, 'in', dirs);
    }
  });
}

// session helpers ------------------------------------------------------------
function sendSessionWorker (options, cb) {
  let packetslen = 0;
  const packets = [];
  let packetshdr;
  let ps = [-1];
  let tags = [];

  if (!options.saveId) {
    return cb({ success: false, text: 'Missing saveId' });
  }

  if (!options.cluster) {
    return cb({ success: false, text: 'Missing cluster' });
  }

  sessionAPIs.processSessionId(options.id, true, function (pcap, header) {
    packetshdr = header;
  }, function (pcap, packet, pcb, i) {
    packetslen += packet.length;
    packets[i] = packet;
    pcb(null);
  }, function (err, session) {
    let buffer;
    if (err || !packetshdr) {
      console.log('WARNING - No PCAP only sending SPI data err:', err);
      buffer = Buffer.alloc(0);
      ps = [];
    } else {
      buffer = Buffer.alloc(packetshdr.length + packetslen);
      let pos = 0;
      packetshdr.copy(buffer);
      pos += packetshdr.length;
      for (let i = 0, ilen = packets.length; i < ilen; i++) {
        ps.push(pos);
        packets[i].copy(buffer, pos);
        pos += packets[i].length;
      }
    }
    if (!session) {
      console.log('no session', session, 'err', err, 'id', options.id);
      return;
    }
    session.id = options.id;
    session.packetPos = ps;
    delete session.fileId;

    if (options.tags) {
      tags = options.tags.replace(/[^-a-zA-Z0-9_:,]/g, '').split(',');
      if (!session.tags) {
        session.tags = [];
      }
      session.tags = session.tags.concat(tags);
    }

    const remoteClusters = internals.remoteClusters;
    if (!remoteClusters) {
      console.log('ERROR - [remote-clusters] is not configured');
      return cb();
    }

    const sobj = remoteClusters[options.cluster];
    if (!sobj) {
      console.log('ERROR - [remote-clusters] does not contain ' + options.cluster);
      return cb();
    }

    const receivePath = '/api/sessions/receive?saveId=' + options.saveId;
    const url = new URL(receivePath, sobj.url);
    const client = url.protocol === 'https:' ? https : http;
    const reqOptions = {
      method: 'POST',
      agent: client === http ? internals.httpAgent : internals.httpsAgent
    };

    Auth.addS2SAuth(reqOptions, options.user, options.nodeName, receivePath, sobj.serverSecret || sobj.passwordSecret);
    ViewerUtils.addCaTrust(reqOptions, options.nodeName);

    const sessionStr = JSON.stringify(session);
    const b = Buffer.alloc(12);
    b.writeUInt32BE(Buffer.byteLength(sessionStr), 0);
    b.writeUInt32BE(buffer.length, 8);

    function doRequest (retry) {
      let result = '';
      const preq = client.request(url, reqOptions, (pres) => {
        pres.on('data', (chunk) => {
          result += chunk;
        });
        pres.on('end', () => {
          result = JSON.parse(result);
          if (!result.success) {
            console.log('ERROR sending session ', result);
          }
          return cb();
        });
      });

      preq.on('error', (e) => {
        if (retry) {
          console.log('Retrying', url, e);
          doRequest(false);
        } else {
          console.log("ERROR - Couldn't connect to ", url, '\nerror=', e);
          return cb();
        }
      });

      preq.write(b);
      preq.write(sessionStr);
      preq.write(buffer);
      preq.end();
    };
    doRequest(true);
  }, undefined, 10);
}

internals.sendSessionQueue = async.queue(sendSessionWorker, 5);

const qlworking = {};
function sendSessionsListQL (pOptions, list, nextQLCb) {
  if (!list) {
    return;
  }

  const nodes = {};

  list.forEach(function (item) {
    if (!nodes[item.node]) {
      nodes[item.node] = [];
    }
    nodes[item.node].push(item.id);
  });

  const keys = Object.keys(nodes);

  async.eachLimit(keys, 15, function (node, nextCb) {
    sessionAPIs.isLocalView(node, function () {
      let sent = 0;
      nodes[node].forEach(function (item) {
        const options = {
          id: item,
          nodeName: node
        };
        Db.merge(options, pOptions);

        // Get from our DISK
        internals.sendSessionQueue.push(options, function () {
          sent++;
          if (sent === nodes[node].length) {
            nextCb();
          }
        });
      });
    },
    function () {
      // Get from remote DISK
      ViewerUtils.getViewUrl(node, (err, viewUrl, client) => {
        let sendPath = `${Config.basePath(node) + node}/sendSessions?saveId=${pOptions.saveId}&cluster=${pOptions.cluster}`;
        if (pOptions.tags) { sendPath += `&tags=${pOptions.tags}`; }
        const url = new URL(sendPath, viewUrl);
        const reqOptions = {
          method: 'POST',
          agent: client === http ? internals.httpAgent : internals.httpsAgent
        };

        Auth.addS2SAuth(reqOptions, pOptions.user, node, sendPath);
        ViewerUtils.addCaTrust(reqOptions, node);

        const preq = client.request(url, reqOptions, (pres) => {
          pres.on('data', (chunk) => {
            qlworking[url.path] = 'data';
          });
          pres.on('end', () => {
            delete qlworking[url.path];
            setImmediate(nextCb);
          });
        });
        preq.on('error', (e) => {
          delete qlworking[url.path];
          console.log("ERROR - Couldn't proxy sendSession request=", url, '\nerror=', e);
          setImmediate(nextCb);
        });
        preq.setHeader('content-type', 'application/x-www-form-urlencoded');
        preq.write('ids=');
        preq.write(nodes[node].join(','));
        preq.end();
        qlworking[url.path] = 'sent';
      });
    });
  }, (err) => {
    nextQLCb();
  });
}

// ============================================================================
// EXPIRING
// ============================================================================
// Search the oldest 500 files on a set of nodes in a set of directories.
// If less then 10 items are returned we don't delete anything.
// Doesn't support mounting sub directories in main directory, don't do it.
function expireDevice (nodes, dirs, minFreeSpaceG, nextCb) {
  if (Config.debug > 0) {
    console.log('EXPIRE - device', nodes, dirs, minFreeSpaceG);
  }
  const query = {
    _source: ['num', 'name', 'first', 'size', 'node'],
    from: '0',
    size: 500,
    query: {
      bool: {
        filter: [
          { terms: { node: nodes } },
          { bool: { should: [] } }
        ],
        must_not: { term: { locked: 1 } }
      }
    },
    sort: { first: { order: 'asc' } }
  };

  Object.keys(dirs).forEach(function (pcapDir) {
    const obj = { wildcard: {} };
    if (pcapDir[pcapDir.length - 1] === '/') {
      obj.wildcard.name = pcapDir + '*';
    } else {
      obj.wildcard.name = pcapDir + '/*';
    }
    query.query.bool.filter[1].bool.should.push(obj);
  });

  if (Config.debug > 1) {
    console.log('EXPIRE - device query', JSON.stringify(query, false, 2));
  }

  // Keep at least 10 files
  Db.search('files', 'file', query, function (err, data) {
    if (err || data.error || !data.hits) {
      if (Config.debug > 0) {
        console.log('EXPIRE - device error', JSON.stringify(err, false, 2));
      }
      return nextCb();
    }

    if (Config.debug === 1) {
      console.log('EXPIRE - device results hits:', data.hits.hits.length);
    } else if (Config.debug > 1) {
      console.log('EXPIRE - device results', JSON.stringify(err, false, 2), JSON.stringify(data, false, 2));
    }

    if (data.hits.total <= 10) {
      if (Config.debug > 0) {
        console.log('EXPIRE - device results not deleting any files since 10 or less');
      }
      return nextCb();
    }

    async.forEachSeries(data.hits.hits, function (item, forNextCb) {
      const fields = item._source || item.fields;

      let freeG;
      try {
        const stat = fse.statVFS(fields.name);
        freeG = stat.f_frsize / 1024.0 * stat.f_bavail / (1024.0 * 1024.0);
      } catch (e) {
        console.log('ERROR', e);
        // File doesn't exist, delete it
        freeG = minFreeSpaceG - 1;
      }
      if (freeG < minFreeSpaceG) {
        data.hits.total--;
        console.log('Deleting', item);
        if (item.indexFilename) {
          fs.unlink(item.indexFilename, () => {});
        }
        return Db.deleteFile(fields.node, item._id, fields.name, forNextCb);
      } else {
        if (Config.debug > 0) {
          console.log('EXPIRE - device not deleting', freeG, minFreeSpaceG, fields.name);
        }
        return forNextCb('DONE');
      }
    }, function () {
      return nextCb();
    });
  });
}

function expireCheckDevice (nodes, stat, nextCb) {
  let doit = false;
  let minFreeSpaceG = 0;
  async.forEach(nodes, function (node, cb) {
    let freeSpaceG = Config.getFull(node, 'freeSpaceG', '5%');
    if (freeSpaceG[freeSpaceG.length - 1] === '%') {
      freeSpaceG = (+freeSpaceG.substr(0, freeSpaceG.length - 1)) * 0.01 * stat.f_frsize / 1024.0 * stat.f_blocks / (1024.0 * 1024.0);
    }
    const freeG = stat.f_frsize / 1024.0 * stat.f_bavail / (1024.0 * 1024.0);
    if (Config.debug > 0) {
      console.log(`EXPIRE check device node: ${node} free: ${freeG} freeSpaceG: ${freeSpaceG}`);
    }
    if (freeG < freeSpaceG) {
      doit = true;
    }

    if (freeSpaceG > minFreeSpaceG) {
      minFreeSpaceG = freeSpaceG;
    }

    cb();
  }, function () {
    if (doit) {
      expireDevice(nodes, stat.dirs, minFreeSpaceG, nextCb);
    } else {
      return nextCb();
    }
  });
}

function expireCheckAll () {
  const devToStat = {};
  // Find all the nodes running on this host
  Db.hostnameToNodeids(Config.hostName(), function (nodes) {
    // Current node name should always be checked too
    if (!nodes.includes(Config.nodeName())) {
      nodes.push(Config.nodeName());
    }

    // Find all the pcap dirs for local nodes
    async.map(nodes, function (node, cb) {
      const pcapDirs = Config.getFull(node, 'pcapDir');
      if (typeof pcapDirs !== 'string') {
        return cb("ERROR - couldn't find pcapDir setting for node: " + node + '\nIf you have it set try running:\nnpm remove iniparser; npm cache clean; npm update iniparser');
      }
      // Create a mapping from device id to stat information and all directories on that device
      pcapDirs.split(';').forEach(function (pcapDir) {
        if (!pcapDir) {
          return; // Skip empty elements.  Prevents errors when pcapDir has a trailing or double ;
        }
        pcapDir = pcapDir.trim();
        const fileStat = fs.statSync(pcapDir);
        const vfsStat = fse.statVFS(pcapDir);
        if (!devToStat[fileStat.dev]) {
          vfsStat.dirs = {};
          vfsStat.dirs[pcapDir] = {};
          devToStat[fileStat.dev] = vfsStat;
        } else {
          devToStat[fileStat.dev].dirs[pcapDir] = {};
        }
      });
      cb(null);
    },
    function (err) {
      // Now gow through all the local devices and check them
      const keys = Object.keys(devToStat);
      async.forEachSeries(keys, function (key, cb) {
        expireCheckDevice(nodes, devToStat[key], cb);
      }, function (err) {
      });
    });
  });
}

// ============================================================================
// REDIRECTS & DEMO SETUP
// ============================================================================
// APIs disabled in demoMode, needs to be before real callbacks
if (Config.get('demoMode', false)) {
  console.log('WARNING - Starting in demo mode, some APIs disabled');
  app.all(['/settings', '/users', '/history/list'], (req, res) => {
    return res.send('Disabled in demo mode.');
  });

  app.get(['/user/cron', '/history/list'], (req, res) => {
    return res.serverError(403, 'Disabled in demo mode.');
  });

  app.post(['/user/password/change', '/changePassword', '/tableState/:tablename'], (req, res) => {
    return res.serverError(403, 'Disabled in demo mode.');
  });
}

// redirect to sessions page and conserve params
app.get(['/', '/app'], (req, res) => {
  const question = req.url.indexOf('?');
  if (question === -1) {
    res.redirect('sessions');
  } else {
    res.redirect('sessions' + req.url.substring(question));
  }
});

// redirect to help page (keeps #)
app.get('/about', checkPermissions(['webEnabled']), (req, res) => {
  res.redirect('help');
});

// ============================================================================
// APIS
// ============================================================================
// user apis ------------------------------------------------------------------
app.get( // current user endpoint
  ['/api/user', '/user/current'],
  checkPermissions(['webEnabled']),
  userAPIs.getUser
);

app.post( // create user endpoint
  ['/api/user', '/user/create'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  userAPIs.createUser
);

app.delete( // user delete endpoint
  ['/api/user/:id', '/user/delete'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  userAPIs.deleteUser
);
app.post( // user delete endpoint for backwards compatibility with API 0.x-2.x
  ['/user/delete'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  userAPIs.deleteUser
);

app.get( // user css endpoint
  ['/api/user[/.]css', '/user.css'],
  checkPermissions(['webEnabled']),
  userAPIs.getUserCSS
);

app.post( // get users endpoint
  ['/api/users', '/user/list'],
  [ArkimeUtil.noCacheJson, recordResponseTime, logAction('users'), checkPermissions(['createEnabled'])],
  userAPIs.getUsers
);

app.post( // update user password endpoint
  ['/api/user/password', '/user/password/change'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.updateUserPassword
);

app.get( // user settings endpoint
  ['/api/user/settings', '/user/settings'],
  [ArkimeUtil.noCacheJson, recordResponseTime, getSettingUserDb, checkPermissions(['webEnabled']), setCookie],
  userAPIs.getUserSettings
);

app.post( // udpate user settings endpoint
  ['/api/user/settings', '/user/settings/update'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.updateUserSettings
);

app.get( // user views endpoint
  ['/api/user/views', '/user/views'],
  [ArkimeUtil.noCacheJson, getSettingUserCache],
  userAPIs.getUserViews
);

app.post( // create user view endpoint
  ['/api/user/view', '/user/views/create'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb, sanitizeViewName],
  userAPIs.createUserView
);

app.deletepost( // delete user view endpoint
  ['/api/user/view/:name', '/user/views/delete'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb, sanitizeViewName],
  userAPIs.deleteUserView
);

app.post( // (un)share a user view endpoint
  ['/api/user/view/:name/toggleshare', '/user/views/toggleShare'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb, sanitizeViewName],
  userAPIs.userViewToggleShare
);

app.put( // update user view endpoint
  ['/api/user/view/:key', '/user/views/update'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb, sanitizeViewName],
  userAPIs.updateUserView
);
app.post( // update user view endpoint for backwards compatibility with API 0.x-2.x
  ['/user/views/update'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb, sanitizeViewName],
  userAPIs.updateUserView
);

app.get( // user cron queries endpoint
  ['/api/user/crons', '/user/cron'],
  [ArkimeUtil.noCacheJson, getSettingUserCache],
  userAPIs.getUserCron
);

app.post( // create user cron query
  ['/api/user/cron', '/user/cron/create'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.createUserCron
);

app.delete( // delete user cron endpoint
  ['/api/user/cron/:key', '/user/cron/delete'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb, checkCronAccess],
  userAPIs.deleteUserCron
);
app.post( // delete user cron endpoint for backwards compatibility with API 0.x-2.x
  '/user/cron/delete',
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb, checkCronAccess],
  userAPIs.deleteUserCron
);

app.post( // update user cron endpoint
  ['/api/user/cron/:key', '/user/cron/update'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb, checkCronAccess],
  userAPIs.updateUserCron
);

app.get( // user custom columns endpoint
  ['/api/user/columns', '/user/columns'],
  [ArkimeUtil.noCacheJson, getSettingUserCache, checkPermissions(['webEnabled'])],
  userAPIs.getUserColumns
);

app.post( // create user custom columns endpoint
  ['/api/user/column', '/user/columns/create'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.createUserColumns
);

app.put( // update user custom column endpoint
  ['/api/user/column/:name', '/user/columns/:name'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.updateUserColumns
);

app.deletepost( // delete user custom column endpoint (DELETE and POST)
  ['/api/user/column/:name', '/user/columns/delete'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.deleteUserColumns
);

app.get( // user spiview fields endpoint
  ['/api/user/spiview', '/user/spiview/fields'],
  [ArkimeUtil.noCacheJson, getSettingUserCache, checkPermissions(['webEnabled'])],
  userAPIs.getUserSpiviewFields
);

app.post( // create spiview fields endpoint
  ['/api/user/spiview', '/user/spiview/fields/create'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.createUserSpiviewFields
);

app.put( // update user spiview fields endpoint
  ['/api/user/spiview/:name', '/user/spiview/fields/:name'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.updateUserSpiviewFields
);

app.deletepost( // delete user spiview fields endpoint (DELETE and POST)
  ['/api/user/spiview/:name', '/user/spiview/fields/delete'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.deleteUserSpiviewFields
);

app.put( // acknowledge message endoint
  ['/api/user/:userId/acknowledge', '/user/:userId/acknowledgeMsg'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken],
  userAPIs.acknowledgeMsg
);

app.post( // update user endpoint
  ['/api/user/:id', '/user/update'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  userAPIs.updateUser
);

app.get( // user state endpoint
  ['/api/user/state/:name', '/state/:name'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction()],
  userAPIs.getUserState
);

app.post( // update/create user state endpoint
  ['/api/user/state/:name', '/state/:name'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction()],
  userAPIs.updateUserState
);

app.get( // user page configuration endpoint
  '/api/user/config/:page',
  [ArkimeUtil.noCacheJson, checkCookieToken, getSettingUserCache],
  userAPIs.getPageConfig
);

app.get( // user roles endpoint
  '/api/user/roles',
  [ArkimeUtil.noCacheJson, checkCookieToken],
  User.apiRoles
);

// notifier apis --------------------------------------------------------------
app.get( // notifier types endpoint
  ['/api/notifiertypes', '/notifierTypes'],
  [checkPermissions(['createEnabled']), checkCookieToken],
  notifierAPIs.getNotifierTypes
);

app.get( // notifiers endpoint
  ['/api/notifiers', '/notifiers'],
  [checkCookieToken],
  notifierAPIs.getNotifiers
);

app.post( // create notifier endpoint
  ['/api/notifier', '/notifiers'],
  [ArkimeUtil.noCacheJson, getSettingUserDb, checkPermissions(['createEnabled']), checkCookieToken],
  notifierAPIs.createNotifier
);

app.put( // update notifier endpoint
  ['/api/notifier/:name', '/notifiers/:name'],
  [ArkimeUtil.noCacheJson, getSettingUserDb, checkPermissions(['createEnabled']), checkCookieToken],
  notifierAPIs.updateNotifier
);

app.delete( // delete notifier endpoint
  ['/api/notifier/:name', '/notifiers/:name'],
  [ArkimeUtil.noCacheJson, getSettingUserDb, checkPermissions(['createEnabled']), checkCookieToken],
  notifierAPIs.deleteNotifier
);

app.post( // test notifier endpoint
  ['/api/notifier/:name/test', '/notifiers/:name/test'],
  [ArkimeUtil.noCacheJson, getSettingUserCache, checkPermissions(['createEnabled']), checkCookieToken],
  notifierAPIs.testNotifier
);

// history apis ---------------------------------------------------------------
app.get( // get histories endpoint
  ['/api/histories', '/history/list'],
  [ArkimeUtil.noCacheJson, recordResponseTime, setCookie],
  historyAPIs.getHistories
);

app.delete( // delete history endpoint
  ['/api/history/:id', '/history/list/:id'],
  [ArkimeUtil.noCacheJson, checkCookieToken, checkPermissions(['createEnabled', 'removeEnabled'])],
  historyAPIs.deleteHistory
);

// stats apis -----------------------------------------------------------------

app.get( // stats endpoint
  ['/api/stats', '/stats.json'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkPermissions(['hideStats']), setCookie],
  statsAPIs.getStats
);

app.get( // detailed stats endpoint
  ['/api/dstats', '/dstats.json'],
  [ArkimeUtil.noCacheJson, checkPermissions(['hideStats'])],
  statsAPIs.getDetailedStats
);

app.get( // elasticsearch stats endpoint
  ['/api/esstats', '/esstats.json'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkPermissions(['hideStats']), setCookie],
  statsAPIs.getESStats
);

app.get( // elasticsearch indices endpoint
  ['/api/esindices', '/esindices/list'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkPermissions(['hideStats']), setCookie],
  statsAPIs.getESIndices
);

app.delete( // delete elasticsearch index endpoint
  ['/api/esindices/:index', '/esindices/:index'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkPermissions(['createEnabled', 'removeEnabled']), setCookie],
  statsAPIs.deleteESIndex
);

app.post( // optimize elasticsearch index endpoint
  ['/api/esindices/:index/optimize', '/esindices/:index/optimize'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  statsAPIs.optimizeESIndex
);

app.post( // close elasticsearch index endpoint
  ['/api/esindices/:index/close', '/esindices/:index/close'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  statsAPIs.closeESIndex
);

app.post( // open elasticsearch index endpoint
  ['/api/esindices/:index/open', '/esindices/:index/open'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  statsAPIs.openESIndex
);

app.post( // shrink elasticsearch index endpoint
  ['/api/esindices/:index/shrink', '/esindices/:index/shrink'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  statsAPIs.shrinkESIndex
);

app.get( // elasticsearch tasks endpoint
  ['/api/estasks', '/estask/list'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkPermissions(['hideStats']), setCookie],
  statsAPIs.getESTasks
);

app.post( // cancel elasticsearch task endpoint
  ['/api/estasks/:id/cancel', '/estask/cancel'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  statsAPIs.cancelESTask
);

app.post( // cancel elasticsearch task by opaque id endpoint
  ['/api/estasks/:id/cancelwith', '/estask/cancelById'],
  // should not have createEnabled check so users can use, each user is name spaced
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken],
  statsAPIs.cancelUserESTask
);

app.post( // cancel all elasticsearch tasks endpoint
  ['/api/estasks/cancelall', '/estask/cancelAll'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  statsAPIs.cancelAllESTasks
);

app.get( // elasticsearch admin settings endpoint
  ['/api/esadmin', '/esadmin/list'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkEsAdminUser, setCookie],
  statsAPIs.getESAdminSettings
);

app.post( // set elasticsearch admin setting endpoint
  ['/api/esadmin/set', '/esadmin/set'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkEsAdminUser, checkCookieToken],
  statsAPIs.setESAdminSettings
);

app.post( // reroute elasticsearch admin endpoint
  ['/api/esadmin/reroute', '/esadmin/reroute'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkEsAdminUser, checkCookieToken],
  statsAPIs.rerouteES
);

app.post( // flush elasticsearch admin endpoint
  ['/api/esadmin/flush', '/esadmin/flush'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkEsAdminUser, checkCookieToken],
  statsAPIs.flushES
);

app.post( // unflood elasticsearch admin endpoint
  ['/api/esadmin/unflood', '/esadmin/unflood'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkEsAdminUser, checkCookieToken],
  statsAPIs.unfloodES
);

app.post( // unflood elasticsearch admin endpoint
  ['/api/esadmin/clearcache', '/esadmin/clearcache'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkEsAdminUser, checkCookieToken],
  statsAPIs.clearCacheES
);

app.get( // elasticsearch shards endpoint
  ['/api/esshards', '/esshard/list'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkPermissions(['hideStats']), setCookie],
  statsAPIs.getESShards
);

app.post( // exclude elasticsearch shard endpoint
  ['/api/esshards/:type/:value/exclude', '/esshard/exclude/:type/:value'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  statsAPIs.excludeESShard
);

app.post( // include elasticsearch shard endpoint
  ['/api/esshards/:type/:value/include', '/esshard/include/:type/:value'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  statsAPIs.includeESShard
);

app.get( // elasticsearch recovery endpoint
  ['/api/esrecovery', '/esrecovery/list'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkPermissions(['hideStats']), setCookie],
  statsAPIs.getESRecovery
);

// session apis ---------------------------------------------------------------
app.getpost( // sessions endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/sessions', '/sessions.json'],
  [ArkimeUtil.noCacheJson, recordResponseTime, fillQueryFromBody, logAction('sessions'), setCookie],
  sessionAPIs.getSessions
);

app.getpost( // spiview endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/spiview', '/spiview.json'],
  [ArkimeUtil.noCacheJson, recordResponseTime, fillQueryFromBody, logAction('spiview'), setCookie],
  sessionAPIs.getSPIView
);

app.getpost( // spigraph endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/spigraph', '/spigraph.json'],
  [ArkimeUtil.noCacheJson, recordResponseTime, fillQueryFromBody, logAction('spigraph'), setCookie, fieldToExp],
  sessionAPIs.getSPIGraph
);

app.getpost( // spigraph hierarchy endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/spigraphhierarchy', '/spigraphhierarchy'],
  [ArkimeUtil.noCacheJson, recordResponseTime, fillQueryFromBody, logAction('spigraphhierarchy'), setCookie],
  sessionAPIs.getSPIGraphHierarchy
);

app.getpost( // build query endoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/buildquery', '/buildQuery.json'],
  [ArkimeUtil.noCacheJson, fillQueryFromBody, logAction('query')],
  sessionAPIs.getQuery
);

app.getpost( // sessions csv endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/sessions[/.]csv', /\/sessions.csv.*/],
  [fillQueryFromBody, logAction('sessions.csv')],
  sessionAPIs.getSessionsCSV
);

app.getpost( // unique endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/unique', '/unique.txt'],
  [fillQueryFromBody, logAction('unique'), fieldToExp],
  sessionAPIs.getUnique
);

app.getpost( // multiunique endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/multiunique', '/multiunique.txt'],
  [fillQueryFromBody, logAction('multiunique'), fieldToExp],
  sessionAPIs.getMultiunique
);

app.get( // session detail (SPI) endpoint
  ['/api/session/:nodeName/:id/detail', '/:nodeName/session/:id/detail'],
  [logAction()],
  sessionAPIs.getDetail
);

app.get( // session packets endpoint
  ['/api/session/:nodeName/:id/packets', '/:nodeName/session/:id/packets'],
  [logAction(), checkPermissions(['hidePcap'])],
  sessionAPIs.getPackets
);

app.post( // add tags endpoint
  ['/api/sessions/addtags', '/addTags'],
  [ArkimeUtil.noCacheJson, checkHeaderToken, logAction('addTags')],
  sessionAPIs.addTags
);

app.post( // remove tags endpoint
  ['/api/sessions/removetags', '/removeTags'],
  [ArkimeUtil.noCacheJson, checkHeaderToken, logAction('removeTags'), checkPermissions(['removeEnabled'])],
  sessionAPIs.removeTags
);

app.get( // session body file endpoint
  ['/api/session/:nodeName/:id/body/:bodyType/:bodyNum/:bodyName', '/:nodeName/:id/body/:bodyType/:bodyNum/:bodyName'],
  [checkProxyRequest],
  sessionAPIs.getRawBody
);

app.get( // session body file image endpoint
  ['/api/session/:nodeName/:id/bodypng/:bodyType/:bodyNum/:bodyName', '/:nodeName/:id/bodypng/:bodyType/:bodyNum/:bodyName'],
  [checkProxyRequest],
  sessionAPIs.getFilePNG
);

app.get( // session pcap endpoint
  ['/api/sessions[/.]pcap', /\/sessions.pcap.*/],
  [logAction(), checkPermissions(['disablePcapDownload'])],
  sessionAPIs.getPCAP
);

app.get( // session pcapng endpoint
  ['/api/sessions[/.]pcapng', /\/sessions.pcapng.*/],
  [logAction(), checkPermissions(['disablePcapDownload'])],
  sessionAPIs.getPCAPNG
);

app.get( // session node pcap endpoint
  ['/api/session/:nodeName/:id[/.]pcap*', '/:nodeName/pcap/:id.pcap'],
  [checkProxyRequest, checkPermissions(['disablePcapDownload'])],
  sessionAPIs.getPCAPFromNode
);

app.get( // session node pcapng endpoint
  ['/api/session/:nodeName/:id[/.]pcapng', '/:nodeName/pcapng/:id.pcapng'],
  [checkProxyRequest, checkPermissions(['disablePcapDownload'])],
  sessionAPIs.getPCAPNGFromNode
);

app.get( // session entire pcap endpoint
  ['/api/session/entire/:nodeName/:id[/.]pcap', '/:nodeName/entirePcap/:id.pcap'],
  [checkProxyRequest, checkPermissions(['disablePcapDownload'])],
  sessionAPIs.getEntirePCAP
);

app.get( // session packets file image endpoint
  ['/api/session/raw/:nodeName/:id[/.]png', '/:nodeName/raw/:id.png'],
  [checkProxyRequest, checkPermissions(['disablePcapDownload'])],
  sessionAPIs.getPacketPNG
);

app.get( // session raw packets endpoint
  ['/api/session/raw/:nodeName/:id', '/:nodeName/raw/:id'],
  [checkProxyRequest, checkPermissions(['disablePcapDownload'])],
  sessionAPIs.getRawPackets
);

app.get( // session file bodyhash endpoint
  ['/api/sessions/bodyhash/:hash', '/bodyHash/:hash'],
  [logAction('bodyhash')],
  sessionAPIs.getBodyHash
);

app.get( // session file bodyhash endpoint
  ['/api/session/:nodeName/:id/bodyhash/:hash', '/:nodeName/:id/bodyHash/:hash'],
  [checkProxyRequest],
  sessionAPIs.getBodyHashFromNode
);

app.get( // sessions get decodings endpoint
  ['/api/sessions/decodings', '/decodings'],
  [ArkimeUtil.noCacheJson],
  sessionAPIs.getDecodings
);

app.get( // session send to node endpoint
  ['/api/session/:nodeName/:id/send', '/:nodeName/sendSession/:id'],
  [checkProxyRequest],
  sessionAPIs.sendSessionToNode
);

app.post( // sessions send to node endpoint
  ['/api/sessions/:nodeName/send', '/:nodeName/sendSessions'],
  [checkProxyRequest],
  sessionAPIs.sendSessionsToNode
);

app.post( // sessions send endpoint
  ['/api/sessions/send', '/sendSessions'],
  sessionAPIs.sendSessions
);

app.post( // sessions recieve endpoint
  ['/api/sessions/receive', '/receiveSession'],
  [ArkimeUtil.noCacheJson],
  sessionAPIs.receiveSession
);

app.post( // delete data endpoint
  ['/api/delete', '/delete'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), checkPermissions(['removeEnabled'])],
  sessionAPIs.deleteData
);

// connections apis -----------------------------------------------------------
app.getpost( // connections endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/connections', '/connections.json'],
  [ArkimeUtil.noCacheJson, recordResponseTime, fillQueryFromBody, logAction('connections'), setCookie],
  connectionAPIs.getConnections
);

app.getpost( // connections csv endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/connections[/.]csv', '/connections.csv'],
  [fillQueryFromBody, logAction('connections.csv')],
  connectionAPIs.getConnectionsCSV
);

// hunt apis ------------------------------------------------------------------
app.get( // hunts endpoint
  ['/api/hunts', '/hunt/list'],
  [ArkimeUtil.noCacheJson, disableInMultiES, recordResponseTime, checkPermissions(['packetSearch']), setCookie],
  huntAPIs.getHunts
);

app.post( // create hunt endpoint
  ['/api/hunt', '/hunt'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt'), checkCookieToken, checkPermissions(['packetSearch'])],
  huntAPIs.createHunt
);

app.delete( // delete hunt endpoint
  ['/api/hunt/:id', '/hunt/:id'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt/:id'), checkCookieToken, checkPermissions(['packetSearch']), checkHuntAccess],
  huntAPIs.deleteHunt
);

app.put( // cancel hunt endpoint
  ['/api/hunt/:id/cancel', '/hunt/:id/cancel'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt/:id/cancel'), checkCookieToken, checkPermissions(['packetSearch']), checkHuntAccess],
  huntAPIs.cancelHunt
);

app.put( // pause hunt endpoint
  ['/api/hunt/:id/pause', '/hunt/:id/pause'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt/:id/pause'), checkCookieToken, checkPermissions(['packetSearch']), checkHuntAccess],
  huntAPIs.pauseHunt
);

app.put( // play hunt endpoint
  ['/api/hunt/:id/play', '/hunt/:id/play'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt/:id/play'), checkCookieToken, checkPermissions(['packetSearch']), checkHuntAccess],
  huntAPIs.playHunt
);

app.put( // remove from sessions hunt endpoint
  ['/api/hunt/:id/removefromsessions', '/hunt/:id/removefromsessions'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt/:id/removefromsessions'), checkCookieToken, checkPermissions(['packetSearch', 'removeEnabled']), checkHuntAccess],
  huntAPIs.removeFromSessions
);

app.post( // add users to hunt endpoint
  ['/api/hunt/:id/users', '/hunt/:id/users'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt/:id/users'), checkCookieToken, checkPermissions(['packetSearch']), checkHuntAccess],
  huntAPIs.addUsers
);

app.delete( // remove users from hunt endpoint
  ['/api/hunt/:id/user/:user', '/hunt/:id/users/:user'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt/:id/user/:user'), checkCookieToken, checkPermissions(['packetSearch']), checkHuntAccess],
  huntAPIs.removeUsers
);

app.get( // remote hunt endpoint
  ['/api/hunt/:nodeName/:huntId/remote/:sessionId', '/:nodeName/hunt/:huntId/remote/:sessionId'],
  [ArkimeUtil.noCacheJson],
  huntAPIs.remoteHunt
);

// shortcut apis ----------------------------------------------------------------
app.get( // get shortcuts endpoint
  ['/api/shortcuts', '/lookups'],
  [ArkimeUtil.noCacheJson, getSettingUserCache, recordResponseTime],
  shortcutAPIs.getShortcuts
);

app.post( // create shortcut endpoint
  ['/api/shortcut', '/lookups'],
  [ArkimeUtil.noCacheJson, getSettingUserDb, logAction('shortcut'), checkCookieToken],
  shortcutAPIs.createShortcut
);

app.put( // update shortcut endpoint
  ['/api/shortcut/:id', '/lookups/:id'],
  [ArkimeUtil.noCacheJson, getSettingUserDb, logAction('shortcut/:id'), checkCookieToken],
  shortcutAPIs.updateShortcut
);

app.delete( // delete shortcut endpoint
  ['/api/shortcut/:id', '/lookups/:id'],
  [ArkimeUtil.noCacheJson, getSettingUserDb, logAction('shortcut/:id'), checkCookieToken],
  shortcutAPIs.deleteShortcut
);

app.get( // sync shortcuts endpoint
  ['/api/syncshortcuts'],
  [ArkimeUtil.noCacheJson],
  shortcutAPIs.syncShortcuts
);

// file apis ------------------------------------------------------------------
app.get( // fields endpoint
  ['/api/fields', '/fields'],
  miscAPIs.getFields
);

app.get( // files endpoint
  ['/api/files', '/file/list'],
  [ArkimeUtil.noCacheJson, recordResponseTime, logAction('files'), checkPermissions(['hideFiles']), setCookie],
  miscAPIs.getFiles
);

app.get( // filesize endpoint
  ['/api/:nodeName/:fileNum/filesize', '/:nodeName/:fileNum/filesize.json'],
  [ArkimeUtil.noCacheJson, checkPermissions(['hideFiles'])],
  miscAPIs.getFileSize
);

// title apis -----------------------------------------------------------------
app.get( // titleconfig endpoint
  ['/api/title', '/titleconfig'],
  checkPermissions(['webEnabled']),
  miscAPIs.getPageTitle
);

// value actions apis ---------------------------------------------------------
app.get( // value actions endpoint
  ['/api/valueactions', '/api/valueActions', '/molochRightClick'],
  [ArkimeUtil.noCacheJson, checkPermissions(['webEnabled'])],
  miscAPIs.getValueActions
);

// reverse dns apis -----------------------------------------------------------
app.get( // reverse dns endpoint
  ['/api/reversedns', '/reverseDNS.txt'],
  [ArkimeUtil.noCacheJson, logAction()],
  miscAPIs.getReverseDNS
);

// uploads apis ---------------------------------------------------------------
app.post(
  ['/api/upload', '/upload'],
  [checkCookieToken, multer({ dest: '/tmp', limits: internals.uploadLimits }).single('file')],
  miscAPIs.upload
);

// clusters apis --------------------------------------------------------------
app.get(
  ['/api/clusters', '/clusters'],
  miscAPIs.getClusters
);

app.get(
  ['/remoteclusters', '/molochclusters'],
  miscAPIs.getRemoteClusters
);

// app apis -------------------------------------------------------------------
app.get(
  '/api/appinfo',
  [ArkimeUtil.noCacheJson, checkCookieToken, getSettingUserCache, checkPermissions(['webEnabled'])],
  miscAPIs.getAppInfo
);

// cyberchef apis -------------------------------------------------------------
app.get('/cyberchef.html', express.static( // cyberchef client file endpoint
  path.join(__dirname, '/public'),
  { maxAge: dayMs, fallthrough: false }
), missingResource, cyberchefCspHeader);

app.get( // cyberchef endpoint
  '/cyberchef/:nodeName/session/:id',
  [checkPermissions(['webEnabled']), checkProxyRequest, cyberchefCspHeader],
  miscAPIs.cyberChef
);

app.use( // cyberchef UI endpoint
  ['/cyberchef/', '/modules/'],
  cyberchefCspHeader,
  miscAPIs.getCyberChefUI
);

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

// using fallthrough: false because there is no 404 endpoint (client router
// handles 404s) and sending index.html is confusing
// expose vue bundles
app.use('/static', express.static(
  path.join(__dirname, '/vueapp/dist/static'),
  { maxAge: dayMs, fallthrough: false }
), missingResource);

app.use(cspHeader, setCookie, (req, res) => {
  if (!req.user.webEnabled) {
    return res.status(403).send('Permission denied');
  }

  if (req.path === '/users' && !req.user.createEnabled) {
    return res.status(403).send('Permission denied');
  }

  if (req.path === '/settings' && Config.get('demoMode', false)) {
    return res.status(403).send('Permission denied');
  }

  const renderer = vueServerRenderer.createRenderer({
    template: fs.readFileSync(path.join(__dirname, '/vueapp/dist/index.html'), 'utf-8')
  });

  let theme = req.user?.settings?.theme || 'default-theme';
  if (theme.startsWith('custom1')) { theme = 'custom-theme'; }

  const titleConfig = Config.get('titleTemplate', '_cluster_ - _page_ _-view_ _-expression_')
    .replace(/_cluster_/g, internals.clusterName)
    .replace(/_userId_/g, req.user ? req.user.userId : '-')
    .replace(/_userName_/g, req.user ? req.user.userName : '-');

  const limit = req.user.createEnabled ? Config.get('huntAdminLimit', 10000000) : Config.get('huntLimit', 1000000);

  const appContext = {
    theme: theme,
    titleConfig: titleConfig,
    path: Config.basePath(),
    version: version.version,
    devMode: Config.get('devMode', false),
    demoMode: Config.get('demoMode', false),
    multiViewer: Config.get('multiES', false),
    hasUsersES: !!Config.get('usersElasticsearch', false),
    themeUrl: theme === 'custom-theme' ? 'api/user/css' : '',
    huntWarn: Config.get('huntWarn', 100000),
    huntLimit: limit,
    nonce: res.locals.nonce,
    anonymousMode: !!internals.noPasswordSecret && !Config.get('regressionTests', false),
    businesDayStart: Config.get('businessDayStart', false),
    businessDayEnd: Config.get('businessDayEnd', false),
    businessDays: Config.get('businessDays', '1,2,3,4,5'),
    tmpRolesSupport: Config.get('tmpRolesSupport', false)
  };

  // Create a fresh Vue app instance
  const vueApp = createApp();

  // Render the Vue instance to HTML
  renderer.renderToString(vueApp, appContext, (err, html) => {
    if (err) {
      console.log(err);
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

// ============================================================================
// CRON QUERIES
// ============================================================================
/* Process a single cron query.  At max it will process 24 hours worth of data
 * to give other queries a chance to run.  Because its timestamp based and not
 * lastPacket based since 1.0 it now search all indices each time.
 */
function processCronQuery (cq, options, query, endTime, cb) {
  if (Config.debug > 2) {
    console.log('CRON', cq.name, cq.creator, '- processCronQuery(', cq, options, query, endTime, ')');
  }

  let singleEndTime;
  let count = 0;
  async.doWhilst((whilstCb) => {
    // Process at most 24 hours
    singleEndTime = Math.min(endTime, cq.lpValue + 24 * 60 * 60);
    query.query.bool.filter[0] = { range: { '@timestamp': { gte: cq.lpValue * 1000, lt: singleEndTime * 1000 } } };

    if (Config.debug > 2) {
      console.log('CRON', cq.name, cq.creator, '- start:', new Date(cq.lpValue * 1000), 'stop:', new Date(singleEndTime * 1000), 'end:', new Date(endTime * 1000), 'remaining runs:', ((endTime - singleEndTime) / (24 * 60 * 60.0)));
    }

    Db.searchSessions(['sessions2-*', 'sessions3-*'], query, { scroll: internals.esScrollTimeout }, function getMoreUntilDone (err, result) {
      async function doNext () {
        count += result.hits.hits.length;

        // No more data, all done
        if (result.hits.hits.length === 0) {
          Db.clearScroll({ body: { scroll_id: result._scroll_id } });
          return setImmediate(whilstCb, 'DONE');
        } else {
          const doc = { doc: { count: (query.count || 0) + count } };
          try {
            Db.update('queries', 'query', options.qid, doc, { refresh: true });
          } catch (err) {
            console.log('ERROR - updating query', err);
          }
        }

        query = {
          body: {
            scroll_id: result._scroll_id
          },
          scroll: internals.esScrollTimeout
        };

        try {
          const { body: results } = await Db.scroll(query);
          return getMoreUntilDone(null, results);
        } catch (err) {
          console.log('ERROR - issuing scroll for cron job', err);
          return getMoreUntilDone(err, {});
        }
      }

      if (err || result.error) {
        console.log('cronQuery error', err, (result ? result.error : null), 'for', cq);
        return setImmediate(whilstCb, 'ERR');
      }

      const ids = [];
      const hits = result.hits.hits;
      let i, ilen;
      if (cq.action.indexOf('forward:') === 0) {
        for (i = 0, ilen = hits.length; i < ilen; i++) {
          ids.push({ id: Db.session2Sid(hits[i]), node: hits[i]._source.node });
        }

        sendSessionsListQL(options, ids, doNext);
      } else if (cq.action.indexOf('tag') === 0) {
        for (i = 0, ilen = hits.length; i < ilen; i++) {
          ids.push(Db.session2Sid(hits[i]));
        }

        if (Config.debug > 1) {
          console.log('CRON', cq.name, cq.creator, '- Updating tags:', ids.length);
        }

        const tags = options.tags.split(',');
        sessionAPIs.sessionsListFromIds(null, ids, ['tags', 'node'], (err, list) => {
          sessionAPIs.addTagsList(tags, list, doNext);
        });
      } else {
        console.log('Unknown action', cq);
        doNext();
      }
    });
  }, () => {
    Db.refresh('sessions*');
    if (Config.debug > 1) {
      console.log('CRON', cq.name, cq.creator, '- Continue process', singleEndTime, endTime);
    }
    return singleEndTime !== endTime;
  }, (err) => {
    cb(count, singleEndTime);
  });
}

internals.processCronQueries = () => {
  if (internals.cronRunning) {
    console.log('processQueries already running', qlworking);
    return;
  }
  internals.cronRunning = true;
  if (Config.debug) {
    console.log('CRON - cronRunning set to true');
  }

  let repeat;
  async.doWhilst(function (whilstCb) {
    repeat = false;
    Db.search('queries', 'query', { size: 1000 }, (err, data) => {
      if (err) {
        internals.cronRunning = false;
        console.log('processCronQueries', err);
        return setImmediate(whilstCb, err);
      }

      const queries = {};
      data.hits.hits.forEach(function (item) {
        queries[item._id] = item._source;
      });

      // Delayed by the max Timeout
      const endTime = Math.floor(Date.now() / 1000) - internals.cronTimeout;

      // Go thru the queries, fetch the user, make the query
      async.eachSeries(Object.keys(queries), (qid, forQueriesCb) => {
        const cq = queries[qid];
        let cluster = null;

        if (Config.debug > 1) {
          console.log('CRON - Running', qid, cq);
        }

        if (!cq.enabled || endTime < cq.lpValue) {
          return forQueriesCb();
        }

        if (cq.action.indexOf('forward:') === 0) {
          cluster = cq.action.substring(8);
        }

        ViewerUtils.getUserCacheIncAnon(cq.creator, async (err, user) => {
          if (err && !user) {
            return forQueriesCb();
          }
          if (!user) {
            console.log(`User ${cq.creator} doesn't exist`);
            return forQueriesCb(null);
          }
          if (!user.enabled) {
            console.log(`User ${cq.creator} not enabled`);
            return forQueriesCb();
          }

          const options = {
            user: user,
            cluster: cluster,
            saveId: Config.nodeName() + '-' + new Date().getTime().toString(36),
            tags: cq.tags.replace(/[^-a-zA-Z0-9_:,]/g, ''),
            qid: qid
          };

          let shortcuts;
          try { // try to fetch shortcuts
            shortcuts = await Db.getShortcutsCache(cq.creator);
          } catch (err) { // don't need to do anything, there will just be no
            // shortcuts sent to the parser. but still log the error.
            console.log('ERROR - fetching shortcuts cache when processing periodic query', err);
          }

          // always complete building the query regardless of shortcuts
          molochparser.parser.yy = {
            emailSearch: user.emailSearch === true,
            fieldsMap: Config.getFieldsMap(),
            dbFieldsMap: Config.getDBFieldsMap(),
            prefix: internals.prefix,
            shortcuts: shortcuts,
            shortcutTypeMap: internals.shortcutTypeMap
          };

          const query = {
            from: 0,
            size: 1000,
            query: { bool: { filter: [{}] } },
            _source: ['_id', 'node']
          };

          try {
            query.query.bool.filter.push(molochparser.parse(cq.query));
          } catch (e) {
            console.log("Couldn't compile periodic query expression", cq, e);
            return forQueriesCb();
          }

          if (user.expression && user.expression.length > 0) {
            try {
              // Expression was set by admin, so assume email search ok
              molochparser.parser.yy.emailSearch = true;
              const userExpression = molochparser.parse(user.expression);
              query.query.bool.filter.push(userExpression);
            } catch (e) {
              console.log("Couldn't compile user forced expression", user.expression, e);
              return forQueriesCb();
            }
          }

          ViewerUtils.lookupQueryItems(query.query.bool.filter, (lerr) => {
            processCronQuery(cq, options, query, endTime, (count, lpValue) => {
              if (Config.debug > 1) {
                console.log('CRON - setting lpValue', new Date(lpValue * 1000));
              }
              // Do the ES update
              const doc = {
                doc: {
                  lpValue: lpValue,
                  lastRun: Math.floor(Date.now() / 1000),
                  count: (cq.count || 0) + count,
                  lastCount: count
                }
              };

              async function continueProcess () {
                try {
                  await Db.update('queries', 'query', qid, doc, { refresh: true });
                } catch (err) {
                  console.log('ERROR - updating query', err);
                }
                if (lpValue !== endTime) { repeat = true; }
                return forQueriesCb();
              }

              // issue alert via notifier if the count has changed and it has been at least 10 minutes
              if (cq.notifier && count && cq.count !== doc.doc.count &&
                (!cq.lastNotified || (Math.floor(Date.now() / 1000) - cq.lastNotified >= 600))) {
                const newMatchCount = cq.lastNotifiedCount ? (doc.doc.count - cq.lastNotifiedCount) : doc.doc.count;
                doc.doc.lastNotifiedCount = doc.doc.count;

                let urlPath = 'sessions?expression=';
                const tags = cq.tags.split(',');
                for (let t = 0, tlen = tags.length; t < tlen; t++) {
                  const tag = tags[t];
                  urlPath += `tags%20%3D%3D%20${tag}`; // encoded ' == '
                  if (t !== tlen - 1) { urlPath += '%20%26%26%20'; } // encoded ' && '
                }

                const message = `
*${cq.name}* periodic query match alert:
*${newMatchCount} new* matches
*${doc.doc.count} total* matches
${Config.arkimeWebURL()}${urlPath}
${cq.description}
                `;

                Db.refresh('*'); // Before sending alert make sure everything has been refreshed
                notifierAPIs.issueAlert(cq.notifier, message, continueProcess);
              } else {
                return continueProcess();
              }
            });
          });
        });
      }, (err) => {
        if (Config.debug > 1) {
          console.log('CRON - Finished one pass of all crons');
        }
        return setImmediate(whilstCb, err);
      });
    });
  }, () => {
    if (Config.debug > 1) {
      console.log('CRON - Process again: ', repeat);
    }
    return repeat;
  }, (err) => {
    if (Config.debug) {
      console.log('CRON - Should be up to date');
    }
    internals.cronRunning = false;
  });
};

// ============================================================================
// MAIN
// ============================================================================
async function main () {
  if (!fs.existsSync(path.join(__dirname, '/vueapp/dist/index.html')) && app.settings.env !== 'development') {
    console.log('WARNING - ./vueapp/dist/index.html missing - The viewer app must be run from inside the viewer directory');
  }

  Db.checkVersion(MIN_DB_VERSION, Config.get('passwordSecret') !== undefined);

  try {
    const health = await Db.healthCache();
    internals.clusterName = health.cluster_name;
  } catch (err) {
    console.log('ERROR - fetching ES health', err);
  }

  try {
    const { body: info } = await Db.nodesStats({
      metric: 'jvm,process,fs,os,indices,thread_pool'
    });
    info.nodes.timestamp = new Date().getTime();
    internals.previousNodesStats.push(info.nodes);
  } catch (err) {
    console.log('ERROR - fetching ES nodes stats', err);
  }

  setFieldLocals();
  setInterval(setFieldLocals, 2 * 60 * 1000);

  loadPlugins();

  const pcapWriteMethod = Config.get('pcapWriteMethod');
  const writer = internals.writers[pcapWriteMethod];
  if (!writer || writer.localNode === true) {
    expireCheckAll();
    setInterval(expireCheckAll, 60 * 1000);
  }

  createRightClicks();
  setInterval(createRightClicks, 150 * 1000); // Check every 2.5 minutes

  if (Config.get('cronQueries', false)) { // this viewer will process the cron queries
    console.log('This node will process Periodic Queries, delayed by', internals.cronTimeout, 'seconds');
    setInterval(internals.processCronQueries, 60 * 1000);
    setTimeout(internals.processCronQueries, 1000);
    setInterval(huntAPIs.processHuntJobs, 10000);
  }

  let server;
  if (Config.isHTTPS()) {
    const cryptoOption = require('crypto').constants.SSL_OP_NO_TLSv1;
    server = https.createServer({
      key: Config.keyFileData,
      cert: Config.certFileData,
      secureOptions: cryptoOption
    }, app);
    Config.setServerToReloadCerts(server, cryptoOption);
  } else {
    server = http.createServer(app);
  }

  const viewHost = Config.get('viewHost', undefined);
  if (internals.userNameHeader !== undefined && viewHost !== 'localhost' && viewHost !== '127.0.0.1') {
    console.log('SECURITY WARNING - when userNameHeader is set, viewHost should be localhost or use iptables');
  }

  server
    .on('error', function (e) {
      console.log("ERROR - couldn't listen on port", Config.get('viewPort', '8005'), 'is viewer already running?');
      process.exit(1);
    })
    .on('listening', function (e) {
      console.log('Express server listening on port %d in %s mode', server.address().port, app.settings.env);
    })
    .listen(Config.get('viewPort', '8005'), viewHost)
    .setTimeout(20 * 60 * 1000);
}

// ============================================================================
// COMMAND LINE PARSING
// ============================================================================
function processArgs (argv) {
  for (let i = 0, ilen = argv.length; i < ilen; i++) {
    if (argv[i] === '--help') {
      console.log('node.js [<options>]');
      console.log('');
      console.log('Options:');
      console.log('  -c <config file>      Config file to use');
      console.log('  -host <host name>     Host name to use, default os hostname');
      console.log('  -n <node name>        Node name section to use in config file, default first part of hostname');
      console.log('  --debug               Increase debug level, multiple are supported');
      console.log('  --esprofile           Turn on profiling to es search queries');
      console.log('  --insecure            Disable certificate verification for https calls');

      process.exit(0);
    }
  }
}
processArgs(process.argv);

// ============================================================================
// DB
// ============================================================================
process.on('unhandledRejection', (reason, p) => {
  console.trace('Unhandled Rejection at: Promise', p, 'reason:', reason, JSON.stringify(reason, false, 2));
  // application specific logging, throwing an error, or other logic here
});

Db.initialize({
  host: internals.elasticBase,
  prefix: internals.prefix,
  usersHost: Config.getArray('usersElasticsearch', ','),
  // The default for usersPrefix should be '' if this is a multiviewer, otherwise Db.initialize will figure out
  usersPrefix: Config.get('usersPrefix', Config.get('multiES', false) ? '' : undefined),
  nodeName: Config.nodeName(),
  hostName: Config.hostName(),
  esClientKey: Config.get('esClientKey', null),
  esClientCert: Config.get('esClientCert', null),
  esClientKeyPass: Config.get('esClientKeyPass', null),
  multiES: Config.get('multiES', false),
  insecure: Config.insecure,
  ca: Config.getCaTrustCerts(Config.nodeName()),
  requestTimeout: Config.get('elasticsearchTimeout', 300),
  esProfile: Config.esProfile,
  debug: Config.debug,
  esApiKey: Config.get('elasticsearchAPIKey', null),
  usersEsApiKey: Config.get('usersElasticsearchAPIKey', null),
  esBasicAuth: Config.get('elasticsearchBasicAuth', null),
  usersEsBasicAuth: Config.get('usersElasticsearchBasicAuth', null),
  cronQueries: Config.get('cronQueries', false)
}, main);
