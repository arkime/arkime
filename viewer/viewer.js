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

const MIN_DB_VERSION = 66;

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
const passport = require('passport');
const DigestStrategy = require('passport-http').DigestStrategy;
const version = require('./version');
const http = require('http');
const https = require('https');
const onHeaders = require('on-headers');
const helmet = require('helmet');
const uuid = require('uuidv4').default;
const path = require('path');

if (typeof express !== 'function') {
  console.log("ERROR - Need to run 'npm update' in viewer directory");
  process.exit(1);
}

// express app
const app = express();

// ============================================================================
// CONFIG & APP SETUP
// ============================================================================
passport.use(new DigestStrategy({ qop: 'auth', realm: Config.get('httpRealm', 'Moloch') },
  function (userid, done) {
    Db.getUserCache(userid, function (err, suser) {
      if (err && !suser) { return done(err); }
      if (!suser || !suser.found) { console.log('User', userid, "doesn't exist"); return done(null, false); }
      if (!suser._source.enabled) { console.log('User', userid, 'not enabled'); return done('Not enabled'); }

      userCleanup(suser._source);

      return done(null, suser._source, { ha1: Config.store2ha1(suser._source.passStore) });
    });
  },
  function (options, done) {
    return done(null, true);
  }
));

// app.configure
const logger = require('morgan');
const favicon = require('serve-favicon');
const bodyParser = require('body-parser');
const multer = require('multer');
const methodOverride = require('method-override');
const compression = require('compression');

// internal app deps
const { internals } = require('./internals')(app, Config);
const ViewerUtils = require('./viewerUtils')(Config, Db, molochparser, internals);
const notifierAPIs = require('./apiNotifiers')(Config, Db, internals);
const sessionAPIs = require('./apiSessions')(Config, Db, internals, molochparser, Pcap, version, ViewerUtils);
const connectionAPIs = require('./apiConnections')(Config, Db, ViewerUtils, sessionAPIs);
const statsAPIs = require('./apiStats')(Config, Db, internals, ViewerUtils);
const huntAPIs = require('./apiHunts')(Config, Db, internals, notifierAPIs, Pcap, sessionAPIs, ViewerUtils);
const userAPIs = require('./apiUsers')(Config, Db, internals, ViewerUtils);
const historyAPIs = require('./apiHistory')(Db);
const shortcutAPIs = require('./apiShortcuts')(Db, internals, ViewerUtils);
const miscAPIs = require('./apiMisc')(Config, Db, internals, sessionAPIs, ViewerUtils);

// registers a get and a post
app.getpost = (route, mw, func) => { app.get(route, mw, func); app.post(route, mw, func); };
app.deletepost = (route, mw, func) => { app.delete(route, mw, func); app.post(route, mw, func); };
app.enable('jsonp callback');
app.set('views', path.join(__dirname, '/views'));
app.set('view engine', 'pug');

app.use(passport.initialize());
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
const cspHeader = helmet.contentSecurityPolicy({
  directives: {
    defaultSrc: ["'self'"],
    /* can remove unsafe-inline for css when this is fixed
    https://github.com/vuejs/vue-style-loader/issues/33 */
    styleSrc: ["'self'", "'unsafe-inline'"],
    scriptSrc: ["'self'", "'unsafe-eval'", (req, res) => `'nonce-${res.locals.nonce}'`],
    objectSrc: ["'none'"],
    imgSrc: ["'self'", 'data:']
  }
});

const unsafeInlineCspHeader = helmet.contentSecurityPolicy({
  directives: {
    defaultSrc: ["'self'"],
    styleSrc: ["'self'", "'unsafe-inline'"],
    scriptSrc: ["'self'", "'unsafe-eval'", "'unsafe-inline'"],
    objectSrc: ["'self'", 'data:'],
    workerSrc: ["'self'", 'data:', 'blob:'],
    imgSrc: ["'self'", 'data:'],
    fontSrc: ["'self'", 'data:']
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
  res.molochError = molochError;

  req.url = req.url.replace(Config.basePath(), '/');
  return next();
});

// client static files --------------------------------------------------------
app.use(favicon(path.join(__dirname, '/public/favicon.ico')));
app.use('/font-awesome', express.static(path.join(__dirname, '/../node_modules/font-awesome'), { maxAge: 600 * 1000 }));
app.use('/', express.static(path.join(__dirname, '/public'), { maxAge: 600 * 1000 }));
app.use('/assets', express.static(path.join(__dirname, '../assets'), { maxAge: 600 * 1000 }));
app.use('/logos', express.static(path.join(__dirname, '../assets'), { maxAge: 600 * 1000 }));

// password, testing, or anonymous mode setup ---------------------------------
if (Config.get('passwordSecret')) {
  app.use(function (req, res, next) {
    // 200 for NS
    if (req.url === '/_ns_/nstest.html') {
      return res.end();
    }

    // No auth for eshealth.json or parliament.json
    if (req.url.match(/^\/(parliament|eshealth).json/)) {
      return next();
    }

    // S2S Auth
    if (req.headers['x-moloch-auth']) {
      const obj = Config.auth2obj(req.headers['x-moloch-auth'], false);
      obj.path = obj.path.replace(Config.basePath(), '/');
      if (obj.path !== req.url) {
        console.log('ERROR - mismatch url', obj.path, req.url);
        return res.send('Unauthorized based on bad url, check logs on ', Config.hostName());
      }
      if (Math.abs(Date.now() - obj.date) > 120000) { // Request has to be +- 2 minutes
        console.log('ERROR - Denying server to server based on timestamp, are clocks out of sync?', Date.now(), obj.date);
        return res.send('Unauthorized based on timestamp - check that all moloch viewer machines have accurate clocks');
      }

      // Don't look up user for receiveSession
      if (req.url.match(/^\/receiveSession/) || req.url.match(/^\/api\/sessions\/receive/)) {
        return next();
      }

      Db.getUserCache(obj.user, function (err, suser) {
        if (err) { return res.send('ERROR - x-moloch getUser - user: ' + obj.user + ' err:' + err); }
        if (!suser || !suser.found) { return res.send(obj.user + " doesn't exist"); }
        if (!suser._source.enabled) { return res.send(obj.user + ' not enabled'); }
        userCleanup(suser._source);
        req.user = suser._source;
        return next();
      });
      return;
    }

    if (req.url.match(/^\/receiveSession/) || req.url.match(/^\/api\/sessions\/receive/)) {
      return res.send('receive session only allowed s2s');
    }

    function ucb (err, suser, userName) {
      if (err) { return res.send(`ERROR - getUser - user: ${userName} err: ${err}`); }
      if (!suser || !suser.found) { return res.send(`${userName} doesn't exist`); }
      if (!suser._source.enabled) { return res.send(`${userName} not enabled`); }
      if (!suser._source.headerAuthEnabled) { return res.send(`${userName} header auth not enabled`); }

      userCleanup(suser._source);
      req.user = suser._source;
      return next();
    }

    // Header auth
    if (internals.userNameHeader !== undefined) {
      if (req.headers[internals.userNameHeader] !== undefined) {
        // Check if we require a certain header+value to be present
        // as in the case of an apache plugin that sends AD groups
        if (internals.requiredAuthHeader !== undefined && internals.requiredAuthHeaderVal !== undefined) {
          const authHeader = req.headers[internals.requiredAuthHeader];
          if (authHeader === undefined) {
            return res.send('Missing authorization header');
          }
          let authorized = false;
          authHeader.split(',').forEach(headerVal => {
            if (headerVal.trim() === internals.requiredAuthHeaderVal) {
              authorized = true;
            }
          });
          if (!authorized) {
            return res.send('Not authorized');
          }
        }

        const userName = req.headers[internals.userNameHeader];

        Db.getUserCache(userName, (err, suser) => {
          if (internals.userAutoCreateTmpl === undefined) {
            return ucb(err, suser, userName);
          } else if ((err && err.toString().includes('Not Found')) ||
             (!suser || !suser.found)) { // Try dynamic creation
            const nuser = JSON.parse(new Function('return `' +
                   internals.userAutoCreateTmpl + '`;').call(req.headers));
            Db.setUser(userName, nuser, (err, info) => {
              if (err) {
                console.log('Elastic search error adding user: (' + userName + '):(' + JSON.stringify(nuser) + '):' + err);
              } else {
                console.log('Added user:' + userName + ':' + JSON.stringify(nuser));
              }
              return Db.getUserCache(userName, ucb);
            });
          } else {
            return ucb(err, suser, userName);
          }
        });
        return;
      } else if (Config.debug) {
        console.log('DEBUG - Couldn\'t find userNameHeader of', internals.userNameHeader, 'in', req.headers, 'for', req.url);
      }
    }

    // Browser auth
    req.url = req.url.replace('/', Config.basePath());
    passport.authenticate('digest', { session: false })(req, res, function (err) {
      req.url = req.url.replace(Config.basePath(), '/');
      if (err) { return res.molochError(200, err); } else { return next(); }
    });
  });
} else if (Config.get('regressionTests', false)) {
  console.log('WARNING - The setting "regressionTests" is set to true, do NOT use in production, for testing only');
  internals.noPasswordSecret = true;
  app.use(function (req, res, next) {
    const username = req.query.molochRegressionUser || 'anonymous';
    req.user = { userId: username, enabled: true, createEnabled: username === 'anonymous', webEnabled: true, headerAuthEnabled: false, emailSearch: true, removeEnabled: true, packetSearch: true, settings: {}, welcomeMsgNum: 1 };
    Db.getUserCache(username, function (err, suser) {
      if (!err && suser && suser.found) {
        userCleanup(suser._source);
        req.user = suser._source;
      }
      next();
    });
  });
} else {
  /* Shared password isn't set, who cares about auth, db is only used for settings */
  console.log('WARNING - The setting "passwordSecret" is not set, all access is anonymous');
  internals.noPasswordSecret = true;
  app.use(function (req, res, next) {
    req.user = internals.anonymousUser;
    Db.getUserCache('anonymous', (err, suser) => {
      if (!err && suser && suser.found) {
        req.user.settings = suser._source.settings || {};
        req.user.views = suser._source.views;
        req.user.columnConfigs = suser._source.columnConfigs;
        req.user.spiviewFieldConfigs = suser._source.spiviewFieldConfigs;
        req.user.tableStates = suser._source.tableStates;
      }
      next();
    });
  });
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
  const require = match[1];

  match = input.match(/title:([^;]+)/);
  const title = match[1] || key;

  match = input.match(/fields:([^;]+)/);
  if (!match) {
    console.log(`custom-view ${key} missing fields section`);
    process.exit(1);
  }
  const fields = match[1];

  let output = `  if (session.${require})\n    div.sessionDetailMeta.bold ${title}\n    dl.sessionDetailMeta\n`;

  for (const field of fields.split(',')) {
    const info = fieldsMap[field];
    if (!info) {
      continue;
    }
    const parts = ViewerUtils.splitRemain(info.dbField, '.', 1);
    if (parts.length === 1) {
      output += `      +arrayList(session, '${parts[0]}', '${info.friendlyName}', '${field}')\n`;
    } else {
      output += `      +arrayList(session.${parts[0]}, '${parts[1]}', '${info.friendlyName}', '${field}')\n`;
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
function molochError (status, text) {
  this.status(status || 403);
  return this.send(JSON.stringify({ success: false, text: text }));
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
    'MOLOCH-COOKIE',
    Config.obj2auth({
      date: Date.now(),
      pid: process.pid,
      userId: req.user.userId
    }, true),
    cookieOptions
  );

  return next();
}

function checkCookieToken (req, res, next) {
  if (!req.headers['x-moloch-cookie']) {
    return res.molochError(500, 'Missing token');
  }

  req.token = Config.auth2obj(req.headers['x-moloch-cookie'], true);
  const diff = Math.abs(Date.now() - req.token.date);
  if (diff > 2400000 || /* req.token.pid !== process.pid || */
      req.token.userId !== req.user.userId) {
    console.trace('bad token', req.token);
    return res.molochError(500, 'Timeout - Please try reloading page and repeating the action');
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
        return res.molochError(403, 'You do not have permission to access this resource');
      }
    }
    next();
  };
}

// used to disable endpoints in multi es mode
function disableInMultiES (req, res, next) {
  if (Config.get('multiES', false)) {
    return res.molochError(401, 'Not supported in multies');
  }
  return next();
}

function checkHuntAccess (req, res, next) {
  if (req.user.createEnabled) {
    // an admin can do anything to any hunt
    return next();
  } else {
    Db.getHunt(req.params.id, (err, huntHit) => {
      if (err) {
        console.log('error', err);
        return res.molochError(500, err);
      }
      if (!huntHit || !huntHit.found) { throw new Error('Hunt not found'); }

      if (huntHit._source.userId === req.user.userId) {
        return next();
      }
      return res.molochError(403, 'You cannot change another user\'s hunt unless you have admin privileges');
    });
  }
}

function checkCronAccess (req, res, next) {
  if (req.user.createEnabled) {
    // an admin can do anything to any query
    return next();
  } else {
    Db.get('queries', 'query', req.body.key, (err, query) => {
      if (err || !query.found) {
        return res.molochError(403, 'Unknown cron query');
      }
      if (query._source.creator === req.user.userId) {
        return next();
      }
      return res.molochError(403, 'You cannot change another user\'s cron query unless you have admin privileges');
    });
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
  return res.molochError(403, 'You do not have permission to access this resource');
}

// no cache middleware --------------------------------------------------------
function noCacheJson (req, res, next) {
  res.header('Cache-Control', 'no-cache, private, no-store, must-revalidate, max-stale=0, post-check=0, pre-check=0');
  res.setHeader('Content-Type', 'application/json');
  return next();
}

// log middleware -------------------------------------------------------------
function logAction (uiPage) {
  return (req, res, next) => {
    const log = {
      timestamp: Math.floor(Date.now() / 1000),
      method: req.method,
      userId: req.user.userId,
      api: req._parsedUrl.pathname,
      query: req._parsedUrl.query,
      expression: req.query.expression
    };

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
    const avoidProps = {
      password: true, newPassword: true, currentPassword: true
    };
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
      log.queryTime = new Date() - req._molochStartTime;
      res.removeListener('finish', finish);
      Db.historyIt(log, function (err, info) {
        if (err) { console.log('log history error', err, info); }
      });
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
    res.setHeader('X-Moloch-Response-Time', ms);
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
  if (!req.user.createEnabled) { return res.molochError(403, 'Need admin privileges'); }

  Db.getUserCache(req.query.userId, function (err, user) {
    if (err || !user || !user.found) {
      if (internals.noPasswordSecret) {
        req.settingUser = JSON.parse(JSON.stringify(req.user));
        delete req.settingUser.found;
      } else {
        req.settingUser = null;
      }
      return next();
    }
    req.settingUser = user._source;
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
    return res.molochError(403, 'Need admin privileges');
  } else {
    userId = req.query.userId;
  }

  Db.getUser(userId, function (err, user) {
    if (err || !user || !user.found) {
      if (internals.noPasswordSecret) {
        req.settingUser = JSON.parse(JSON.stringify(req.user));
        delete req.settingUser.found;
      } else {
        return res.molochError(403, 'Unknown user');
      }
      return next();
    }
    req.settingUser = user._source;
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

// user helpers ---------------------------------------------------------------
function userCleanup (suser) {
  suser.settings = suser.settings || {};
  if (suser.emailSearch === undefined) { suser.emailSearch = false; }
  if (suser.removeEnabled === undefined) { suser.removeEnabled = false; }
  // if multies and not users elasticsearch, disable admin privelages
  if (Config.get('multiES', false) && !Config.get('usersElasticsearch')) {
    suser.createEnabled = false;
  }
  const now = Date.now();
  const timespan = Config.get('regressionTests', false) ? 1 : 60000;
  // update user lastUsed time if not mutiES and it hasn't been udpated in more than a minute
  if (!Config.get('multiES', false) && (!suser.lastUsed || (now - suser.lastUsed) > timespan)) {
    suser.lastUsed = now;
    Db.setLastUsed(suser.userId, now, function (err, info) {
      if (Config.debug && err) {
        console.log('DEBUG - user lastUsed update error', err, info);
      }
    });
  }
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
      console.log('ERROR - arkime-clusters is not configured for ' + options.cluster);
      return cb();
    }

    const path = '/api/sessions/receive?saveId=' + options.saveId;
    const url = new URL(path, sobj.url);
    const client = url.protocol === 'https:' ? https : http;
    const reqOptions = {
      method: 'POST',
      agent: client === http ? internals.httpAgent : internals.httpsAgent
    };

    ViewerUtils.addAuth(reqOptions, options.user, options.nodeName, path, sobj.serverSecret || sobj.passwordSecret);
    ViewerUtils.addCaTrust(reqOptions, options.nodeName);

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
        cb();
      });
    });

    preq.on('error', (e) => {
      console.log("ERROR - Couldn't connect to ", url, '\nerror=', e);
      cb();
    });

    const sessionStr = JSON.stringify(session);
    const b = Buffer.alloc(12);
    b.writeUInt32BE(Buffer.byteLength(sessionStr), 0);
    b.writeUInt32BE(buffer.length, 8);
    preq.write(b);
    preq.write(sessionStr);
    preq.write(buffer);
    preq.end();
  }, undefined, 10);
}

internals.sendSessionQueue = async.queue(sendSessionWorker, 10);

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
        let path = `${Config.basePath(node) + node}/sendSessions?saveId=${pOptions.saveId}&cluster=${pOptions.cluster}`;
        if (pOptions.tags) { path += `&tags=${pOptions.tags}`; }
        const url = new URL(path, viewUrl);
        const reqOptions = {
          method: 'POST',
          agent: client === http ? internals.httpAgent : internals.httpsAgent
        };

        ViewerUtils.addAuth(reqOptions, pOptions.user, node, path);
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
// Search for all files on a set of nodes in a set of directories.
// If less then size items are returned we don't delete anything.
// Doesn't support mounting sub directories in main directory, don't do it.
function expireDevice (nodes, dirs, minFreeSpaceG, nextCb) {
  const query = {
    _source: ['num', 'name', 'first', 'size', 'node'],
    from: '0',
    size: 200,
    query: {
      bool: {
        must: [
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
    query.query.bool.must[1].bool.should.push(obj);
  });

  // Keep at least 10 files
  Db.search('files', 'file', query, function (err, data) {
    if (err || data.error || !data.hits || data.hits.total <= 10) {
      return nextCb();
    }
    async.forEachSeries(data.hits.hits, function (item, forNextCb) {
      if (data.hits.total <= 10) {
        return forNextCb('DONE');
      }

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
    return res.molochError(403, 'Disabled in demo mode.');
  });

  app.post(['/user/password/change', '/changePassword', '/tableState/:tablename'], (req, res) => {
    return res.molochError(403, 'Disabled in demo mode.');
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

app.get(['/remoteclusters', '/molochclusters'], function (req, res) {
  function cloneClusters (clusters) {
    const clone = {};

    for (const key in clusters) {
      if (clusters[key]) {
        const cluster = clusters[key];
        clone[key] = {
          name: cluster.name,
          url: cluster.url
        };
      }
    }

    return clone;
  }

  if (!internals.remoteClusters) {
    res.status(404);
    return res.send('Cannot locate remote clusters');
  }

  const clustersClone = cloneClusters(internals.remoteClusters);

  return res.send(clustersClone);
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
  [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  userAPIs.createUser
);

app.delete( // user delete endpoint
  ['/api/user/:id', '/user/delete'],
  [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  userAPIs.deleteUser
);
app.post( // user delete endpoint for backwards compatibility with API 0.x-2.x
  ['/user/delete'],
  [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  userAPIs.deleteUser
);

app.get( // user css endpoint
  ['/api/user[/.]css', '/user.css'],
  checkPermissions(['webEnabled']),
  userAPIs.getUserCSS
);

app.post( // get users endpoint
  ['/api/users', '/user/list'],
  [noCacheJson, recordResponseTime, logAction('users'), checkPermissions(['createEnabled'])],
  userAPIs.getUsers
);

app.post( // update user password endpoint
  ['/api/user/password', '/user/password/change'],
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.updateUserPassword
);

app.get( // user settings endpoint
  ['/api/user/settings', '/user/settings'],
  [noCacheJson, recordResponseTime, getSettingUserDb, checkPermissions(['webEnabled']), setCookie],
  userAPIs.getUserSettings
);

app.post( // udpate user settings endpoint
  ['/api/user/settings', '/user/settings/update'],
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.updateUserSettings
);

app.get( // user views endpoint
  ['/api/user/views', '/user/views'],
  [noCacheJson, getSettingUserCache],
  userAPIs.getUserViews
);

app.post( // create user view endpoint
  ['/api/user/view', '/user/views/create'],
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb, sanitizeViewName],
  userAPIs.createUserView
);

app.deletepost( // delete user view endpoint
  ['/api/user/view/:name', '/user/views/delete'],
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb, sanitizeViewName],
  userAPIs.deleteUserView
);

app.post( // (un)share a user view endpoint
  ['/api/user/view/:name/toggleshare', '/user/views/toggleShare'],
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb, sanitizeViewName],
  userAPIs.userViewToggleShare
);

app.put( // update user view endpoint
  ['/api/user/view/:key', '/user/views/update'],
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb, sanitizeViewName],
  userAPIs.updateUserView
);
app.post( // update user view endpoint for backwards compatibility with API 0.x-2.x
  ['/user/views/update'],
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb, sanitizeViewName],
  userAPIs.updateUserView
);

app.get( // user cron queries endpoint
  ['/api/user/crons', '/user/cron'],
  [noCacheJson, getSettingUserCache],
  userAPIs.getUserCron
);

app.post( // create user cron query
  ['/api/user/cron', '/user/cron/create'],
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.createUserCron
);

app.delete( // delete user cron endpoint
  ['/api/user/cron/:key', '/user/cron/delete'],
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb, checkCronAccess],
  userAPIs.deleteUserCron
);
app.post( // delete user cron endpoint for backwards compatibility with API 0.x-2.x
  '/user/cron/delete',
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb, checkCronAccess],
  userAPIs.deleteUserCron
);

app.post( // update user cron endpoint
  ['/api/user/cron/:key', '/user/cron/update'],
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb, checkCronAccess],
  userAPIs.updateUserCron
);

app.get( // user custom columns endpoint
  ['/api/user/columns', '/user/columns'],
  [noCacheJson, getSettingUserCache, checkPermissions(['webEnabled'])],
  userAPIs.getUserColumns
);

app.post( // create user custom columns endpoint
  ['/api/user/column', '/user/columns/create'],
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.createUserColumns
);

app.put( // update user custom column endpoint
  ['/api/user/column/:name', '/user/columns/:name'],
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.updateUserColumns
);

app.deletepost( // delete user custom column endpoint (DELETE and POST)
  ['/api/user/column/:name', '/user/columns/delete'],
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.deleteUserColumns
);

app.get( // user spiview fields endpoint
  ['/api/user/spiview', '/user/spiview/fields'],
  [noCacheJson, getSettingUserCache, checkPermissions(['webEnabled'])],
  userAPIs.getUserSpiviewFields
);

app.post( // create spiview fields endpoint
  ['/api/user/spiview', '/user/spiview/fields/create'],
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.createUserSpiviewFields
);

app.put( // update user spiview fields endpoint
  ['/api/user/spiview/:name', '/user/spiview/fields/:name'],
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.updateUserSpiviewFields
);

app.deletepost( // delete user spiview fields endpoint (DELETE and POST)
  ['/api/user/spiview/:name', '/user/spiview/fields/delete'],
  [noCacheJson, checkCookieToken, logAction(), getSettingUserDb],
  userAPIs.deleteUserSpiviewFields
);

app.put( // acknowledge message endoint
  ['/api/user/:userId/acknowledge', '/user/:userId/acknowledgeMsg'],
  [noCacheJson, logAction(), checkCookieToken],
  userAPIs.acknowledgeMsg
);

app.post( // update user endpoint
  ['/api/user/:id', '/user/update'],
  [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  userAPIs.updateUser
);

app.get( // user state endpoint
  ['/api/user/state/:name', '/state/:name'],
  [noCacheJson, checkCookieToken, logAction()],
  userAPIs.getUserState
);

app.post( // update/create user state endpoint
  ['/api/user/state/:name', '/state/:name'],
  [noCacheJson, checkCookieToken, logAction()],
  userAPIs.updateUserState
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
  [noCacheJson, getSettingUserDb, checkPermissions(['createEnabled']), checkCookieToken],
  notifierAPIs.createNotifier
);

app.put( // update notifier endpoint
  ['/api/notifier/:name', '/notifiers/:name'],
  [noCacheJson, getSettingUserDb, checkPermissions(['createEnabled']), checkCookieToken],
  notifierAPIs.updateNotifier
);

app.delete( // delete notifier endpoint
  ['/api/notifier/:name', '/notifiers/:name'],
  [noCacheJson, getSettingUserDb, checkPermissions(['createEnabled']), checkCookieToken],
  notifierAPIs.deleteNotifier
);

app.post( // test notifier endpoint
  ['/api/notifier/:name/test', '/notifiers/:name/test'],
  [noCacheJson, getSettingUserCache, checkPermissions(['createEnabled']), checkCookieToken],
  notifierAPIs.testNotifier
);

// history apis ---------------------------------------------------------------
app.get( // get histories endpoint
  ['/api/histories', '/history/list'],
  [noCacheJson, recordResponseTime, setCookie],
  historyAPIs.getHistories
);

app.delete( // delete history endpoint
  ['/api/history/:id', '/history/list/:id'],
  [noCacheJson, checkCookieToken, checkPermissions(['createEnabled', 'removeEnabled'])],
  historyAPIs.deleteHistory
);

// stats apis -----------------------------------------------------------------
app.get( // es health endpoint
  ['/api/eshealth', '/eshealth.json'],
  statsAPIs.getESHealth
);

app.get( // stats endpoint
  ['/api/stats', '/stats.json'],
  [noCacheJson, recordResponseTime, checkPermissions(['hideStats']), setCookie],
  statsAPIs.getStats
);

app.get( // detailed stats endpoint
  ['/api/dstats', '/dstats.json'],
  [noCacheJson, checkPermissions(['hideStats'])],
  statsAPIs.getDetailedStats
);

app.get( // elasticsearch stats endpoint
  ['/api/esstats', '/esstats.json'],
  [noCacheJson, recordResponseTime, checkPermissions(['hideStats']), setCookie],
  statsAPIs.getESStats
);

app.get( // elasticsearch indices endpoint
  ['/api/esindices', '/esindices/list'],
  [noCacheJson, recordResponseTime, checkPermissions(['hideStats']), setCookie],
  statsAPIs.getESIndices
);

app.delete( // delete elasticsearch index endpoint
  ['/api/esindices/:index', '/esindices/:index'],
  [noCacheJson, recordResponseTime, checkPermissions(['createEnabled', 'removeEnabled']), setCookie],
  statsAPIs.deleteESIndex
);

app.post( // optimize elasticsearch index endpoint
  ['/api/esindices/:index/optimize', '/esindices/:index/optimize'],
  [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  statsAPIs.optimizeESIndex
);

app.post( // close elasticsearch index endpoint
  ['/api/esindices/:index/close', '/esindices/:index/close'],
  [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  statsAPIs.closeESIndex
);

app.post( // open elasticsearch index endpoint
  ['/api/esindices/:index/open', '/esindices/:index/open'],
  [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  statsAPIs.openESIndex
);

app.post( // shrink elasticsearch index endpoint
  ['/api/esindices/:index/shrink', '/esindices/:index/shrink'],
  [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  statsAPIs.shrinkESIndex
);

app.get( // elasticsearch tasks endpoint
  ['/api/estasks', '/estask/list'],
  [noCacheJson, recordResponseTime, checkPermissions(['hideStats']), setCookie],
  statsAPIs.getESTasks
);

app.post( // cancel elasticsearch task endpoint
  ['/api/estasks/:id/cancel', '/estask/cancel'],
  [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  statsAPIs.cancelESTask
);

app.post( // cancel elasticsearch task by opaque id endpoint
  ['/api/estasks/:id/cancelwith', '/estask/cancelById'],
  // should not have createEnabled check so users can use, each user is name spaced
  [noCacheJson, logAction(), checkCookieToken],
  statsAPIs.cancelUserESTask
);

app.post( // cancel all elasticsearch tasks endpoint
  ['/api/estasks/cancelall', '/estask/cancelAll'],
  [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  statsAPIs.cancelAllESTasks
);

app.get( // elasticsearch admin settings endpoint
  ['/api/esadmin', '/esadmin/list'],
  [noCacheJson, recordResponseTime, checkEsAdminUser, setCookie],
  statsAPIs.getESAdminSettings
);

app.post( // set elasticsearch admin setting endpoint
  ['/api/esadmin/set', '/esadmin/set'],
  [noCacheJson, recordResponseTime, checkEsAdminUser, checkCookieToken],
  statsAPIs.setESAdminSettings
);

app.post( // reroute elasticsearch admin endpoint
  ['/api/esadmin/reroute', '/esadmin/reroute'],
  [noCacheJson, recordResponseTime, checkEsAdminUser, checkCookieToken],
  statsAPIs.rerouteES
);

app.post( // flush elasticsearch admin endpoint
  ['/api/esadmin/flush', '/esadmin/flush'],
  [noCacheJson, recordResponseTime, checkEsAdminUser, checkCookieToken],
  statsAPIs.flushES
);

app.post( // unflood elasticsearch admin endpoint
  ['/api/esadmin/unflood', '/esadmin/unflood'],
  [noCacheJson, recordResponseTime, checkEsAdminUser, checkCookieToken],
  statsAPIs.unfloodES
);

app.post( // unflood elasticsearch admin endpoint
  ['/api/esadmin/clearcache', '/esadmin/clearcache'],
  [noCacheJson, recordResponseTime, checkEsAdminUser, checkCookieToken],
  statsAPIs.clearCacheES
);

app.get( // elasticsearch shards endpoint
  ['/api/esshards', '/esshard/list'],
  [noCacheJson, recordResponseTime, checkPermissions(['hideStats']), setCookie],
  statsAPIs.getESShards
);

app.post( // exclude elasticsearch shard endpoint
  ['/api/esshards/:type/:value/exclude', '/esshard/exclude/:type/:value'],
  [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  statsAPIs.excludeESShard
);

app.post( // include elasticsearch shard endpoint
  ['/api/esshards/:type/:value/include', '/esshard/include/:type/:value'],
  [noCacheJson, logAction(), checkCookieToken, checkPermissions(['createEnabled'])],
  statsAPIs.includeESShard
);

app.get( // elasticsearch recovery endpoint
  ['/api/esrecovery', '/esrecovery/list'],
  [noCacheJson, recordResponseTime, checkPermissions(['hideStats']), setCookie],
  statsAPIs.getESRecovery
);

// parliament apis ------------------------------------------------------------
app.get( // parliament endpoint (no auth necessary)
  ['/api/parliament', '/parliament.json'],
  [noCacheJson],
  statsAPIs.getParliament
);

// session apis ---------------------------------------------------------------
app.getpost( // sessions endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/sessions', '/sessions.json'],
  [noCacheJson, recordResponseTime, logAction('sessions'), setCookie, fillQueryFromBody],
  sessionAPIs.getSessions
);

app.getpost( // spiview endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/spiview', '/spiview.json'],
  [noCacheJson, recordResponseTime, logAction('spiview'), setCookie, fillQueryFromBody],
  sessionAPIs.getSPIView
);

app.getpost( // spigraph endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/spigraph', '/spigraph.json'],
  [noCacheJson, recordResponseTime, logAction('spigraph'), setCookie, fillQueryFromBody, fieldToExp],
  sessionAPIs.getSPIGraph
);

app.getpost( // spigraph hierarchy endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/spigraphhierarchy', '/spigraphhierarchy'],
  [noCacheJson, recordResponseTime, logAction('spigraphhierarchy'), setCookie, fillQueryFromBody],
  sessionAPIs.getSPIGraphHierarchy
);

app.getpost( // build query endoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/buildquery', '/buildQuery.json'],
  [noCacheJson, logAction('query'), fillQueryFromBody],
  sessionAPIs.getQuery
);

app.getpost( // sessions csv endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/sessions[/.]csv', /\/sessions.csv.*/],
  [logAction('sessions.csv'), fillQueryFromBody],
  sessionAPIs.getSessionsCSV
);

app.getpost( // unique endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/unique', '/unique.txt'],
  [logAction('unique'), fillQueryFromBody, fieldToExp],
  sessionAPIs.getUnique
);

app.getpost( // multiunique endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/multiunique', '/multiunique.txt'],
  [logAction('multiunique'), fillQueryFromBody, fieldToExp],
  sessionAPIs.getMultiunique
);

app.get( // session detail (SPI) endpoint
  ['/api/session/:nodeName/:id/detail', '/:nodeName/session/:id/detail'],
  [cspHeader, logAction()],
  sessionAPIs.getDetail
);

app.get( // session packets endpoint
  ['/api/session/:nodeName/:id/packets', '/:nodeName/session/:id/packets'],
  [logAction(), checkPermissions(['hidePcap'])],
  sessionAPIs.getPackets
);

app.post( // add tags endpoint
  ['/api/sessions/addtags', '/addTags'],
  [noCacheJson, checkHeaderToken, logAction('addTags')],
  sessionAPIs.addTags
);

app.post( // remove tags endpoint
  ['/api/sessions/removetags', '/removeTags'],
  [noCacheJson, checkHeaderToken, logAction('removeTags'), checkPermissions(['removeEnabled'])],
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
  [noCacheJson],
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
  [noCacheJson],
  sessionAPIs.receiveSession
);

app.post( // delete data endpoint
  ['/api/delete', '/delete'],
  [noCacheJson, checkCookieToken, logAction(), checkPermissions(['removeEnabled'])],
  sessionAPIs.deleteData
);

// connections apis -----------------------------------------------------------
app.getpost( // connections endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/connections', '/connections.json'],
  [noCacheJson, recordResponseTime, logAction('connections'), setCookie, fillQueryFromBody],
  connectionAPIs.getConnections
);

app.getpost( // connections csv endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/connections[/.]csv', '/connections.csv'],
  [logAction('connections.csv'), fillQueryFromBody],
  connectionAPIs.getConnectionsCSV
);

// hunt apis ------------------------------------------------------------------
app.get( // hunts endpoint
  ['/api/hunts', '/hunt/list'],
  [noCacheJson, disableInMultiES, recordResponseTime, checkPermissions(['packetSearch']), setCookie],
  huntAPIs.getHunts
);

app.post( // create hunt endpoint
  ['/api/hunt', '/hunt'],
  [noCacheJson, disableInMultiES, logAction('hunt'), checkCookieToken, checkPermissions(['packetSearch'])],
  huntAPIs.createHunt
);

app.delete( // delete hunt endpoint
  ['/api/hunt/:id', '/hunt/:id'],
  [noCacheJson, disableInMultiES, logAction('hunt/:id'), checkCookieToken, checkPermissions(['packetSearch']), checkHuntAccess],
  huntAPIs.deleteHunt
);

app.put( // pause hunt endpoint
  ['/api/hunt/:id/pause', '/hunt/:id/pause'],
  [noCacheJson, disableInMultiES, logAction('hunt/:id/pause'), checkCookieToken, checkPermissions(['packetSearch']), checkHuntAccess],
  huntAPIs.pauseHunt
);

app.put( // play hunt endpoint
  ['/api/hunt/:id/play', '/hunt/:id/play'],
  [noCacheJson, disableInMultiES, logAction('hunt/:id/play'), checkCookieToken, checkPermissions(['packetSearch']), checkHuntAccess],
  huntAPIs.playHunt
);

app.post( // add users to hunt endpoint
  ['/api/hunt/:id/users', '/hunt/:id/users'],
  [noCacheJson, disableInMultiES, logAction('hunt/:id/users'), checkCookieToken, checkPermissions(['packetSearch']), checkHuntAccess],
  huntAPIs.addUsers
);

app.delete( // remove users from hunt endpoint
  ['/api/hunt/:id/user/:user', '/hunt/:id/users/:user'],
  [noCacheJson, disableInMultiES, logAction('hunt/:id/user/:user'), checkCookieToken, checkPermissions(['packetSearch']), checkHuntAccess],
  huntAPIs.removeUsers
);

app.get( // remote hunt endpoint
  ['/api/hunt/:nodeName/:huntId/remote/:sessionId', '/:nodeName/hunt/:huntId/remote/:sessionId'],
  [noCacheJson],
  huntAPIs.remoteHunt
);

// shortcut apis ----------------------------------------------------------------
app.get( // get shortcuts endpoint
  ['/api/shortcuts', '/lookups'],
  [noCacheJson, getSettingUserCache, recordResponseTime],
  shortcutAPIs.getShortcuts
);

app.post( // create shortcut endpoint
  ['/api/shortcut', '/lookups'],
  [noCacheJson, getSettingUserDb, logAction('shortcut'), checkCookieToken],
  shortcutAPIs.createShortcut
);

app.put( // update shortcut endpoint
  ['/api/shortcut/:id', '/lookups/:id'],
  [noCacheJson, getSettingUserDb, logAction('shortcut/:id'), checkCookieToken],
  shortcutAPIs.updateShortcut
);

app.delete( // delete shortcut endpoint
  ['/api/shortcut/:id', '/lookups/:id'],
  [noCacheJson, getSettingUserDb, logAction('shortcut/:id'), checkCookieToken],
  shortcutAPIs.deleteShortcut
);

// file apis ------------------------------------------------------------------
app.get( // fields endpoint
  ['/api/fields', '/fields'],
  miscAPIs.getFields
);

app.get( // files endpoint
  ['/api/files', '/file/list'],
  [noCacheJson, recordResponseTime, logAction('files'), checkPermissions(['hideFiles']), setCookie],
  miscAPIs.getFiles
);

app.get( // filesize endpoint
  ['/api/:nodeName/:fileNum/filesize', '/:nodeName/:fileNum/filesize.json'],
  [noCacheJson, checkPermissions(['hideFiles'])],
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
  [noCacheJson, checkPermissions(['webEnabled'])],
  miscAPIs.getValueActions
);

// reverse dns apis -----------------------------------------------------------
app.get( // reverse dns endpoint
  ['/api/reversedns', '/reverseDNS.txt'],
  [noCacheJson, logAction()],
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

// cyberchef apis -------------------------------------------------------------
app.get( // cyberchef endpoint
  '/cyberchef/:nodeName/session/:id',
  [checkPermissions(['webEnabled']), checkProxyRequest, unsafeInlineCspHeader],
  miscAPIs.cyberChef
);

app.use( // cyberchef UI endpoint
  ['/cyberchef/', '/modules/'],
  unsafeInlineCspHeader,
  miscAPIs.getCyberChefUI
);

// ============================================================================
// REGRESSION TEST CONFIGURATION
// ============================================================================
if (Config.get('regressionTests')) {
  app.post('/shutdown', function (req, res) {
    Db.close();
    process.exit(0);
  });
  app.post('/flushCache', function (req, res) {
    Db.flushCache();
    res.send('{}');
  });
  app.get('/processCronQueries', function (req, res) {
    internals.processCronQueries();
    res.send('{}');
  });

  // Make sure all jobs have run and return
  app.get('/processHuntJobs', function (req, res) {
    huntAPIs.processHuntJobs();

    setTimeout(function checkHuntFinished () {
      if (internals.runningHuntJob) {
        setTimeout(checkHuntFinished, 1000);
      } else {
        Db.search('hunts', 'hunt', { query: { term: { status: 'queued' } } }, function (err, result) {
          if (result.hits.total > 0) {
            huntAPIs.processHuntJobs();
            setTimeout(checkHuntFinished, 1000);
          } else {
            res.send('{}');
          }
        });
      }
    }, 1000);
  });
}

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

// expose vue bundles (prod)
app.use('/static', express.static(path.join(__dirname, '/vueapp/dist/static')));
app.use('/app.css', express.static(path.join(__dirname, '/vueapp/dist/app.css')));
// expose vue bundle (dev)
app.use(['/app.js', '/vueapp/app.js'], express.static(path.join(__dirname, '/vueapp/dist/app.js')));
app.use(['/app.js.map', '/vueapp/app.js.map'], express.static(path.join(__dirname, '/vueapp/dist/app.js.map')));

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

  let theme = req.user.settings.theme || 'default-theme';
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
    themeUrl: theme === 'custom-theme' ? 'api/user/css' : '',
    huntWarn: Config.get('huntWarn', 100000),
    huntLimit: limit,
    serverNonce: res.locals.nonce,
    anonymousMode: !!internals.noPasswordSecret && !Config.get('regressionTests', false),
    businesDayStart: Config.get('businessDayStart', false),
    businessDayEnd: Config.get('businessDayEnd', false),
    businessDays: Config.get('businessDays', '1,2,3,4,5')
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
    query.query.bool.filter[0] = { range: { timestamp: { gte: cq.lpValue * 1000, lt: singleEndTime * 1000 } } };

    if (Config.debug > 2) {
      console.log('CRON', cq.name, cq.creator, '- start:', new Date(cq.lpValue * 1000), 'stop:', new Date(singleEndTime * 1000), 'end:', new Date(endTime * 1000), 'remaining runs:', ((endTime - singleEndTime) / (24 * 60 * 60.0)));
    }

    Db.search('sessions2-*', 'session', query, { scroll: internals.esScrollTimeout }, function getMoreUntilDone (err, result) {
      function doNext () {
        count += result.hits.hits.length;

        // No more data, all done
        if (result.hits.hits.length === 0) {
          Db.clearScroll({ body: { scroll_id: result._scroll_id } });
          return setImmediate(whilstCb, 'DONE');
        } else {
          const document = { doc: { count: (query.count || 0) + count } };
          Db.update('queries', 'query', options.qid, document, { refresh: true }, function () {});
        }

        query = {
          body: {
            scroll_id: result._scroll_id
          },
          scroll: internals.esScrollTimeout
        };

        Db.scroll(query, getMoreUntilDone);
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
          ids.push({ id: hits[i]._id, node: hits[i]._source.node });
        }

        sendSessionsListQL(options, ids, doNext);
      } else if (cq.action.indexOf('tag') === 0) {
        for (i = 0, ilen = hits.length; i < ilen; i++) {
          ids.push(hits[i]._id);
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

        ViewerUtils.getUserCacheIncAnon(cq.creator, (err, user) => {
          if (err && !user) {
            return forQueriesCb();
          }
          if (!user || !user.found) {
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

          Db.getShortcutsCache(cq.creator, (err, shortcuts) => {
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
              console.log("Couldn't compile cron query expression", cq, e);
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
                const document = {
                  doc: {
                    lpValue: lpValue,
                    lastRun: Math.floor(Date.now() / 1000),
                    count: (queries[qid].count || 0) + count
                  }
                };

                function continueProcess () {
                  Db.update('queries', 'query', qid, document, { refresh: true }, function () {
                    // If there is more time to catch up on, repeat the loop, although other queries
                    // will get processed first to be fair
                    if (lpValue !== endTime) { repeat = true; }
                    return forQueriesCb();
                  });
                }

                // issue alert via notifier if the count has changed and it has been at least 10 minutes
                if (cq.notifier && count && queries[qid].count !== document.doc.count &&
                  (!cq.lastNotified || (Math.floor(Date.now() / 1000) - cq.lastNotified >= 600))) {
                  const newMatchCount = document.doc.lastNotifiedCount ? (document.doc.count - document.doc.lastNotifiedCount) : document.doc.count;
                  const message = `*${cq.name}* cron query match alert:\n*${newMatchCount} new* matches\n*${document.doc.count} total* matches`;
                  notifierAPIs.issueAlert(cq.notifier, message, continueProcess);
                } else {
                  return continueProcess();
                }
              });
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
function main () {
  if (!fs.existsSync(path.join(__dirname, '/vueapp/dist/index.html')) && app.settings.env !== 'development') {
    console.log('WARNING - ./vueapp/dist/index.html missing - The viewer app must be run from inside the viewer directory');
  }

  Db.checkVersion(MIN_DB_VERSION, Config.get('passwordSecret') !== undefined);
  Db.healthCache(function (err, health) {
    internals.clusterName = health.cluster_name;
  });

  Db.nodesStats({ metric: 'jvm,process,fs,os,indices,thread_pool' }, function (err, info) {
    info.nodes.timestamp = new Date().getTime();
    internals.previousNodesStats.push(info.nodes);
  });

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
  setInterval(createRightClicks, 5 * 60 * 1000);

  if (Config.get('cronQueries', false)) { // this viewer will process the cron queries
    console.log('This node will process Cron Queries, delayed by', internals.cronTimeout, 'seconds');
    setInterval(internals.processCronQueries, 60 * 1000);
    setTimeout(internals.processCronQueries, 1000);
    setInterval(huntAPIs.processHuntJobs, 10000);
  }

  let server;
  if (Config.isHTTPS()) {
    server = https.createServer({
      key: Config.keyFileData,
      cert: Config.certFileData,
      secureOptions: require('crypto').constants.SSL_OP_NO_TLSv1
    }, app);
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
      console.log('  --insecure            Disable cert verification');

      process.exit(0);
    }
  }
}
processArgs(process.argv);

// ============================================================================
// DB
// ============================================================================
Db.initialize({
  host: internals.elasticBase,
  prefix: Config.get('prefix', ''),
  usersHost: Config.get('usersElasticsearch') ? Config.getArray('usersElasticsearch', ',', '') : undefined,
  usersPrefix: Config.get('usersPrefix'),
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
  getSessionBySearch: Config.get('getSessionBySearch', '')
}, main);
