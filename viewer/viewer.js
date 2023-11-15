/******************************************************************************/
/* viewer.js  -- The main arkime app
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const MIN_DB_VERSION = 79;

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
const version = require('../common/version');
const http = require('http');
const https = require('https');
const onHeaders = require('on-headers');
const helmet = require('helmet');
const uuid = require('uuid').v4;
const path = require('path');
const dayMs = 60000 * 60 * 24;
const User = require('../common/user');
const Auth = require('../common/auth');
const ArkimeUtil = require('../common/arkimeUtil');
const ArkimeConfig = require('../common/arkimeConfig');

// express app
const app = express();

// ============================================================================
// CONFIG & APP SETUP
// ============================================================================

// app.configure
const favicon = require('serve-favicon');
const bodyParser = require('body-parser');
const multer = require('multer');
const compression = require('compression');

// internal app deps
const internals = require('./internals');
internals.initialize(app);
const ViewerUtils = require('./viewerUtils');
const Notifier = require('../common/notifier');
const ViewAPIs = require('./apiViews');
const CronAPIs = require('./apiCrons');
const SessionAPIs = require('./apiSessions');
const ConnectionAPIs = require('./apiConnections');
const StatsAPIs = require('./apiStats');
const HuntAPIs = require('./apiHunts');
const UserAPIs = require('./apiUsers');
const HistoryAPIs = require('./apiHistory');
const ShortcutAPIs = require('./apiShortcuts');
const MiscAPIs = require('./apiMisc');

// registers a get and a post
app.getpost = (route, mw, func) => { app.get(route, mw, func); app.post(route, mw, func); };
app.deletepost = (route, mw, func) => { app.delete(route, mw, func); app.post(route, mw, func); };
app.enable('jsonp callback');
app.set('views', path.join(__dirname, '/views'));
app.set('view engine', 'pug');

app.use(ArkimeUtil.jsonParser);
app.use(bodyParser.urlencoded({ limit: '5mb', extended: true }));

app.use(compression());

// Explicit sigint handler for running under docker
// See https://github.com/nodejs/node/issues/4182
process.on('SIGINT', function () {
  process.exit();
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
    objectSrc: ["'self'", 'data:'],
    workerSrc: ["'self'", 'blob:'],
    frameSrc: ["'self'"],
    imgSrc: ["'self'", 'data:']
  }
});

const securityApp = express.Router();
app.use(securityApp);
ArkimeConfig.loaded(() => {
  // app security options -------------------------------------------------------
  const iframeOption = Config.get('iframe', 'deny');
  if (iframeOption === 'sameorigin' || iframeOption === 'deny') {
    securityApp.use(helmet.frameguard({ action: iframeOption }));
  } else {
    securityApp.use(helmet.frameguard({
      action: 'allow-from',
      domain: iframeOption
    }));
  }

  securityApp.use(helmet.hidePoweredBy());
  securityApp.use(helmet.xssFilter());
  if (Config.get('hstsHeader', false) && Config.isHTTPS()) {
    securityApp.use(helmet.hsts({
      maxAge: 31536000,
      includeSubDomains: true
    }));
  }

  // calculate nonce
  securityApp.use((req, res, next) => {
    res.locals.nonce = Buffer.from(uuid()).toString('base64');
    next();
  });
});

ArkimeUtil.logger(app);

// appwide middleware ---------------------------------------------------------
app.use((req, res, next) => {
  res.serverError = ArkimeUtil.serverError;

  req.url = req.url.replace(Config.basePath(), '/');
  return next();
});

// Don't allow cluster if not multiviewer except for the /api/session.*/send calls
app.use((req, res, next) => {
  if (!internals.multiES && req.query.cluster !== undefined) {
    delete req.query.cluster;
  }
  return next();
});

// client static files --------------------------------------------------------
app.use(favicon(path.join(__dirname, '/public/favicon.ico')));
// using fallthrough: false because there is no 404 endpoint (client router
// handles 404s) and sending index.html is confusing
app.use('/font-awesome', express.static(
  path.join(__dirname, '/../node_modules/font-awesome'),
  { maxAge: dayMs, fallthrough: false }
), ArkimeUtil.missingResource);
app.use(['/assets', '/logos'], express.static(
  path.join(__dirname, '../assets'),
  { maxAge: dayMs, fallthrough: false }
), ArkimeUtil.missingResource);

// regression test methods, before auth checks --------------------------------
if (ArkimeConfig.regressionTests) {
  // Override default lastUsed min write internal for tests
  User.lastUsedMinInterval = 1000;

  app.get('/regressionTests/makeToken', (req, res, next) => {
    req.user = {
      userId: req.query.arkimeRegressionUser ?? 'anonymous'
    };
    setCookie(req, res);
    return res.end();
  });

  app.post('/regressionTests/shutdown', function (req, res) {
    Db.close();
    process.exit(0);
  });
  app.post('/regressionTests/flushCache', function (req, res) {
    User.flushCache();
    Db.flushCache();
    res.send('{}');
  });
  app.get('/regressionTests/processCronQueries', async function (req, res) {
    internals.cronTimeout = 0;
    await Db.refresh();
    CronAPIs.processCronQueries();
    setTimeout(async function checkCronFinished () {
      if (internals.cronRunning) {
        setTimeout(checkCronFinished, 500);
      } else {
        await Db.refresh();
        res.send('{}');
      }
    }, 500);
  });
  // Make sure all jobs have run and return
  app.get('/regressionTests/processHuntJobs', async function (req, res) {
    await Db.flush();
    await Db.refresh();
    HuntAPIs.processHuntJobs();

    setTimeout(function checkHuntFinished () {
      if (internals.runningHuntJob) {
        setTimeout(checkHuntFinished, 1000);
      } else {
        Db.search('hunts', 'hunt', { query: { terms: { status: ['running', 'queued'] } } }, async function (err, result) {
          if (result.hits.total > 0) {
            HuntAPIs.processHuntJobs();
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
      // Shallow copy
      const cuser = Object.assign({}, user);
      res.send(cuser);
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
  StatsAPIs.getParliament
);

// stats apis - no auth -------------------------------------------------------
app.get( // es health endpoint
  ['/api/eshealth', '/eshealth.json'],
  [ArkimeUtil.noCacheJson],
  StatsAPIs.getESHealth
);

// password, testing, or anonymous mode setup ---------------------------------
Auth.app(app);

// check for arkimeUser
app.use(async (req, res, next) => {
  if (Auth.isAnonymousMode()) {
    return next();
  }
  // For receiveSession there is no user (so no role check can be done) AND must be s2s
  if (req.url.match(/^\/receiveSession/) || req.url.match(/^\/api\/sessions\/receive/)) {
    if (req.headers['x-arkime-auth'] === undefined) {
      return res.status(401).send('receive session only allowed s2s');
    } else {
      return next();
    }
  }

  if (!req.user.hasRole('arkimeUser')) {
    if (Config.debug) {
      console.log('Missing arkimeUser userId: %s roles: %s expanded roles: %s', req.user.userId, req.user.roles, await req.user.getRoles());
    }
    return res.status(403).send('Need arkimeUser role assigned');
  }
  next();
});

ArkimeConfig.loaded(() => {
  if (ArkimeConfig.regressionTests) {
    console.log('WARNING - Option --regressionTests was used, do NOT use in production, for testing only');
  }
});

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

  dirs = dirs.concat(Config.getArray('pluginsDir', `${version.config_prefix}/plugins`));
  dirs = dirs.concat(Config.getArray('parsersDir', `${version.config_prefix}/parsers`));

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
    ;
  });
}

function createActions (configKey, emitter, internalsKey) {
  const mrc = Config.configMap(configKey);
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

  const makers = internals.pluginEmitter.listeners(emitter);
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
    internals[internalsKey] = mrc;
  });
}

// ============================================================================
// API MIDDLEWARE
// ============================================================================
// security/access middleware -------------------------------------------------
function checkProxyRequest (req, res, next) {
  SessionAPIs.isLocalView(req.params.nodeName, function () {
    return next();
  },
  function () {
    return SessionAPIs.proxyRequest(req, res);
  });
}

function setCookie (req, res, next) {
  const cookieOptions = {
    path: Config.basePath(),
    sameSite: 'Strict',
    overwrite: true
  };

  if (Config.isHTTPS()) {
    cookieOptions.secure = true;
  }

  res.cookie( // send cookie for basic, non admin functions
    'ARKIME-COOKIE',
    Auth.obj2auth({
      date: Date.now(),
      pid: process.pid,
      userId: req.user.userId
    }),
    cookieOptions
  );

  if (next) {
    return next();
  }
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
    console.trace('bad token', req.token, diff, req.token.userId, req.user.userId);
    return res.serverError(500, 'Timeout - Please try reloading page and repeating the action');
  }

  return next();
}

// use for APIs that can be used from places other than just the UI
function checkHeaderToken (req, res, next) {
  if (req.headers.cookie || req.headers.referer) { // if there's a cookie or referer, check for token
    return checkCookieToken(req, res, next);
  } else { // if there's no cookie, just continue so the API still works
    return next();
  }
}

// used to disable endpoints in multi es mode
function disableInMultiES (req, res, next) {
  if (internals.multiES) {
    return res.serverError(401, 'Not supported in multies');
  }
  return next();
}

async function checkHuntAccess (req, res, next) {
  if (req.user.hasRole('arkimeAdmin')) {
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

function checkEsAdminUser (req, res, next) {
  if (internals.esAdminUsersSet) {
    if (internals.esAdminUsers.includes(req.user.userId)) {
      return next();
    }
  } else {
    if (req.user.hasRole('arkimeAdmin') && !internals.multiES) {
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

    if (req.user.getExpression()) {
      log.forcedExpression = req.user.getExpression();
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

    req._arkimeStartTime = new Date();

    function finish () {
      res.removeListener('finish', finish);

      log.queryTime = new Date() - req._arkimeStartTime;

      if (req._arkimeESQuery) { log.esQuery = req._arkimeESQuery; }
      if (req._arkimeESQueryIndices) { log.esQueryIndices = req._arkimeESQueryIndices; }

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

// exp to field middleware ----------------------------------------------------
function expToField (req, res, next) {
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
    console.log('%s query: %s', ArkimeUtil.sanitizeStr(req.url), ArkimeUtil.sanitizeStr(req.query));
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
  if (!req.user.hasRole('usersAdmin') || !req.user.hasRole('arkimeAdmin')) { return res.serverError(403, 'Need admin privileges'); }

  User.getUserCache(req.query.userId, (err, user) => {
    if (err || !user) {
      if (Auth.isAnonymousMode()) {
        req.settingUser = Object.assign(new User(), req.user);
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

// view middleware ------------------------------------------------------------
// * remove the string, 'shared:', that is added to shared views with the same
//   name as a user's personal view in the endpoint '/user/views'
// * remove any special characters except ('-', '_', ':', and ' ')
// * map req.params.(key|name) to req.body.(key|name)
function sanitizeViewName (req, res, next) {
  if (req.params.name !== undefined) {
    req.body.name = req.params.name;
    delete req.params.name;
  }
  if (req.params.key !== undefined) {
    req.body.key = req.params.key;
    delete req.params.key;
  }
  if (typeof req.body.name === 'string') {
    req.body.name = req.body.name.replace(/(^(shared:)+)/g, '');
    req.body.name = ArkimeUtil.removeSpecialChars(req.body.name);
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
      internals.writers.set(str, info);
    },
    getDb: function () { return Db; },
    getPcap: function () { return Pcap; }
  };
  const plugins = Config.getArray('viewerPlugins', '');
  const dirs = Config.getArray('pluginsDir', `${version.config_prefix}/plugins`);
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

  SessionAPIs.processSessionId(options.id, true, function (pcap, header) {
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
      session.tagsCnt = session.tags.length;
    }

    const remoteClusters = internals.remoteClusters;
    if (!remoteClusters) {
      console.log('ERROR - [remote-clusters] is not configured');
      return cb();
    }

    const sobj = remoteClusters[options.cluster];
    if (!sobj) {
      console.log('ERROR - [remote-clusters] does not contain %s', ArkimeUtil.sanitizeStr(options.cluster));
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
    _source: ['num', 'name', 'first', 'size', 'node', 'indexFilename'],
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
      const pcapDirs = Config.getFullArray(node, 'pcapDir');
      if (!pcapDirs) {
        return cb("ERROR - couldn't find pcapDir setting for node: " + node + '\nIf you have it set try running:\nnpm remove iniparser; npm cache clean; npm update iniparser');
      }
      // Create a mapping from device id to stat information and all directories on that device
      pcapDirs.forEach(function (pcapDir) {
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
ArkimeConfig.loaded(() => {
  if (Config.get('demoMode', false)) {
    console.log('WARNING - Starting in demo mode, some APIs disabled');
  }
});

app.all([
  '/api/histories',
  '/api/history/*',
  '/api/cron*',
  '/api/user/password*'
], (req, res, next) => {
  if (!req.user.isDemoMode()) {
    return next();
  }
  return res.serverError(403, 'Disabled in demo mode.');
});

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
app.get('/about', User.checkPermissions(['webEnabled']), (req, res) => {
  res.redirect('help');
});

// ============================================================================
// APIS
// ============================================================================
app.all([
  '/user/current',
  '/user/create',
  '/user/delete',
  '/user.css',
  '/user/list',
  '/user/password/change',
  '/user/settings',
  '/user/settings/update',
  '/user/columns',
  '/user/columns/create',
  '/user/columns/:name',
  '/user/columns/delete',
  '/user/spiview/fields',
  '/user/spiview/fields/create',
  '/user/spiview/fields/:name',
  '/user/spiview/fields/delete',
  '/user/:userId/acknowledgeMsg',
  '/user/update',
  '/state/:name',
  '/api/user/views',
  '/user/views',
  '/api/user/view',
  '/user/views/create',
  '/api/user/view/:id',
  '/user/views/delete',
  '/user/views/update',
  '/api/user/crons',
  '/user/cron',
  '/api/user/cron',
  '/user/cron/create',
  '/api/user/cron/:key',
  '/user/cron/delete',
  '/user/cron/update',
  '/notifierTypes',
  '/notifiers',
  '/notifiers/:id',
  '/notifiers/:id/test',
  '/history/list',
  '/history/list/:id',
  '/esindices/list',
  '/esindices/:index',
  '/esindices/:index/optimize',
  '/esindices/:index/close',
  '/esindices/:index/open',
  '/esindices/:index/shrink',
  '/estask/list',
  '/estask/cancel',
  '/estask/cancelById',
  '/estask/cancelAll',
  '/esadmin/list',
  '/esadmin/set',
  '/esadmin/reroute',
  '/esadmin/flush',
  '/esadmin/unflood',
  '/esadmin/clearcache',
  '/esshard/list',
  '/esshard/exclude/:type/:value',
  '/esshard/include/:type/:value',
  '/esrecovery/list',
  '/api/title',
  '/titleconfig',
  '/molochRightClick',
  '/file/list',
  '/api/:nodeName/:fileNum/filesize',
  '/:nodeName/:fileNum/filesize.json',
  '/fields',
  '/molochclusters',
  '/remoteclusters',
  '/clusters',
  '/upload',
  '/reverseDNS.txt',
  '/lookups',
  '/lookups/:id',
  '/hunt/list',
  '/hunt',
  '/hunt/:id',
  '/hunt/:id/cancel',
  '/hunt/:id/pause',
  '/hunt/:id/play',
  '/hunt/:id/removefromsessions',
  '/hunt/:id/users',
  '/hunt/:id/users/:user',
  '/:nodeName/hunt/:huntId/remote/:sessionId',
  '/spigraphhierarchy',
  '/addTags',
  '/removeTags'
], (req, res) => {
  res.status(404).end('Old API');
});

// user apis ------------------------------------------------------------------
app.get( // current user endpoint
  ['/api/user'],
  [ArkimeUtil.noCacheJson, User.checkPermissions(['webEnabled'])],
  User.apiGetUser
);

app.post( // create user endpoint
  ['/api/user'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, User.checkRole('usersAdmin')],
  User.apiCreateUser
);

app.delete( // user delete endpoint
  ['/api/user/:id'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, User.checkRole('usersAdmin')],
  User.apiDeleteUser
);
app.get( // user css endpoint
  ['/api/user[/.]css'],
  User.checkPermissions(['webEnabled']),
  UserAPIs.getUserCSS
);

app.post( // get users endpoint
  ['/api/users'],
  [ArkimeUtil.noCacheJson, recordResponseTime, logAction('users'), User.checkRole('usersAdmin')],
  User.apiGetUsers
);

app.post( // get users endpoint
  ['/api/users[./]csv'],
  [logAction('users.csv'), User.checkRole('usersAdmin')],
  User.apiGetUsersCSV
);

app.post( // (non-admin) list users (with role status for roleAssigners)
  '/api/users/min',
  [ArkimeUtil.noCacheJson, checkCookieToken, User.checkAssignableRole],
  User.apiGetUsersMin
);

app.post( // update user password endpoint
  ['/api/user/password'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), Auth.getSettingUserDb],
  User.apiUpdateUserPassword
);

app.get( // user settings endpoint
  ['/api/user/settings'],
  [ArkimeUtil.noCacheJson, recordResponseTime, Auth.getSettingUserDb, User.checkPermissions(['webEnabled']), setCookie],
  UserAPIs.getUserSettings
);

app.post( // update user settings endpoint
  ['/api/user/settings'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), Auth.getSettingUserDb],
  UserAPIs.updateUserSettings
);

app.get( // user custom columns endpoint
  ['/api/user/columns'],
  [ArkimeUtil.noCacheJson, getSettingUserCache, User.checkPermissions(['webEnabled'])],
  UserAPIs.getUserColumns
);

app.post( // create user custom columns endpoint
  ['/api/user/column'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), Auth.getSettingUserDb],
  UserAPIs.createUserColumns
);

app.put( // update user custom column endpoint
  ['/api/user/column/:name'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), Auth.getSettingUserDb],
  UserAPIs.updateUserColumns
);

app.delete( // delete user custom column endpoint
  ['/api/user/column/:name'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), Auth.getSettingUserDb],
  UserAPIs.deleteUserColumns
);

app.get( // user spiview fields endpoint
  ['/api/user/spiview'],
  [ArkimeUtil.noCacheJson, getSettingUserCache, User.checkPermissions(['webEnabled'])],
  UserAPIs.getUserSpiviewFields
);

app.post( // create spiview fields endpoint
  ['/api/user/spiview'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), Auth.getSettingUserDb],
  UserAPIs.createUserSpiviewFields
);

app.put( // update user spiview fields endpoint
  ['/api/user/spiview/:name'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), Auth.getSettingUserDb],
  UserAPIs.updateUserSpiviewFields
);

app.delete( // delete user spiview fields endpoint
  ['/api/user/spiview/:name'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), Auth.getSettingUserDb],
  UserAPIs.deleteUserSpiviewFields
);

app.put( // acknowledge message endpoint
  ['/api/user/:userId/acknowledge'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken],
  UserAPIs.acknowledgeMsg
);

app.post( // update user endpoint
  ['/api/user/:id'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, User.checkRole('usersAdmin')],
  User.apiUpdateUser
);

app.post( // assign or un-assign role from a user
  '/api/user/:id/assignment',
  [ArkimeUtil.noCacheJson, checkCookieToken, User.checkAssignableRole],
  User.apiUpdateUserRole
);

app.get( // user state endpoint
  ['/api/user/state/:name'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction()],
  UserAPIs.getUserState
);

app.post( // update/create user state endpoint
  ['/api/user/state/:name'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction()],
  UserAPIs.updateUserState
);

app.get( // user page configuration endpoint
  '/api/user/config/:page',
  [ArkimeUtil.noCacheJson, checkCookieToken, getSettingUserCache],
  UserAPIs.getPageConfig
);

app.get( // user roles endpoint
  '/api/user/roles',
  [ArkimeUtil.noCacheJson, checkCookieToken],
  User.apiRoles
);

// view apis ------------------------------------------------------------------
app.get( // get views endpoint
  ['/api/views'],
  [ArkimeUtil.noCacheJson, getSettingUserCache],
  ViewAPIs.apiGetViews
);

app.post( // create view endpoint
  ['/api/view'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), Auth.getSettingUserDb, sanitizeViewName],
  ViewAPIs.apiCreateView
);

app.delete( // delete view endpoint
  ['/api/view/:id'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), Auth.getSettingUserDb, Auth.checkResourceAccess(Db.getView, 'user'), sanitizeViewName],
  ViewAPIs.apiDeleteView
);

app.put( // update view endpoint
  ['/api/view/:id'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), Auth.getSettingUserDb, Auth.checkResourceAccess(Db.getView, 'user'), sanitizeViewName],
  ViewAPIs.apiUpdateView
);

// cron apis ------------------------------------------------------------------
app.get( // get cron queries endpoint
  ['/api/crons'],
  [ArkimeUtil.noCacheJson, getSettingUserCache],
  CronAPIs.getCrons
);

app.post( // create cron query endpoint
  ['/api/cron'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), Auth.getSettingUserDb],
  CronAPIs.createCron
);

app.delete( // delete cron endpoint
  ['/api/cron/:key'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), Auth.getSettingUserDb, Auth.checkResourceAccess(Db.getQuery, 'creator')],
  CronAPIs.deleteCron
);

app.post( // update cron endpoint
  ['/api/cron/:key'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), Auth.getSettingUserDb, Auth.checkResourceAccess(Db.getQuery, 'creator')],
  CronAPIs.updateCron
);

// notifier apis --------------------------------------------------------------
app.get( // notifier types endpoint
  ['/api/notifiertypes'],
  [ArkimeUtil.noCacheJson, User.checkRole('arkimeAdmin'), checkCookieToken],
  Notifier.apiGetNotifierTypes
);

app.get( // notifiers endpoint
  ['/api/notifiers'],
  [ArkimeUtil.noCacheJson, checkCookieToken],
  Notifier.apiGetNotifiers
);

app.post( // create notifier endpoint
  ['/api/notifier'],
  [ArkimeUtil.noCacheJson, Auth.getSettingUserDb, User.checkRole('arkimeAdmin'), checkCookieToken],
  Notifier.apiCreateNotifier
);

app.put( // update notifier endpoint
  ['/api/notifier/:id'],
  [ArkimeUtil.noCacheJson, Auth.getSettingUserDb, User.checkRole('arkimeAdmin'), checkCookieToken],
  Notifier.apiUpdateNotifier
);

app.delete( // delete notifier endpoint
  ['/api/notifier/:id'],
  [ArkimeUtil.noCacheJson, Auth.getSettingUserDb, User.checkRole('arkimeAdmin'), checkCookieToken],
  Notifier.apiDeleteNotifier
);

app.post( // test notifier endpoint
  ['/api/notifier/:id/test'],
  [ArkimeUtil.noCacheJson, getSettingUserCache, User.checkRole('arkimeAdmin'), checkCookieToken],
  Notifier.apiTestNotifier
);

// history apis ---------------------------------------------------------------
app.get( // get histories endpoint
  ['/api/histories'],
  [ArkimeUtil.noCacheJson, recordResponseTime, setCookie],
  HistoryAPIs.getHistories
);

app.delete( // delete history endpoint
  ['/api/history/:id'],
  [ArkimeUtil.noCacheJson, checkCookieToken, User.checkRole('arkimeAdmin'), User.checkPermissions(['removeEnabled'])],
  HistoryAPIs.deleteHistory
);

// stats apis -----------------------------------------------------------------

app.get( // stats endpoint
  ['/api/stats', '/stats.json'],
  [ArkimeUtil.noCacheJson, recordResponseTime, User.checkPermissions(['hideStats']), setCookie],
  StatsAPIs.getStats
);

app.get( // detailed stats endpoint
  ['/api/dstats', '/dstats.json'],
  [ArkimeUtil.noCacheJson, User.checkPermissions(['hideStats'])],
  StatsAPIs.getDetailedStats
);

app.get( // OpenSearch/Elasticsearch stats endpoint
  ['/api/esstats', '/esstats.json'],
  [ArkimeUtil.noCacheJson, recordResponseTime, User.checkPermissions(['hideStats']), setCookie],
  StatsAPIs.getESStats
);

app.get( // OpenSearch/Elasticsearch indices endpoint
  ['/api/esindices'],
  [ArkimeUtil.noCacheJson, recordResponseTime, User.checkPermissions(['hideStats']), setCookie],
  StatsAPIs.getESIndices
);

app.delete( // delete OpenSearch/Elasticsearch index endpoint
  ['/api/esindices/:index'],
  [ArkimeUtil.noCacheJson, recordResponseTime, User.checkRole('arkimeAdmin'), User.checkPermissions(['removeEnabled']), setCookie],
  StatsAPIs.deleteESIndex
);

app.post( // optimize OpenSearch/Elasticsearch index endpoint
  ['/api/esindices/:index/optimize'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, User.checkRole('arkimeAdmin')],
  StatsAPIs.optimizeESIndex
);

app.post( // close OpenSearch/Elasticsearch index endpoint
  ['/api/esindices/:index/close'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, User.checkRole('arkimeAdmin')],
  StatsAPIs.closeESIndex
);

app.post( // open OpenSearch/Elasticsearch index endpoint
  ['/api/esindices/:index/open'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, User.checkRole('arkimeAdmin')],
  StatsAPIs.openESIndex
);

app.post( // shrink OpenSearch/Elasticsearch index endpoint
  ['/api/esindices/:index/shrink'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, User.checkRole('arkimeAdmin')],
  StatsAPIs.shrinkESIndex
);

app.get( // OpenSearch/Elasticsearch tasks endpoint
  ['/api/estasks'],
  [ArkimeUtil.noCacheJson, recordResponseTime, User.checkPermissions(['hideStats']), setCookie],
  StatsAPIs.getESTasks
);

app.post( // cancel OpenSearch/Elasticsearch task endpoint
  ['/api/estasks/:id/cancel'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, User.checkRole('arkimeAdmin')],
  StatsAPIs.cancelESTask
);

app.post( // cancel OpenSearch/Elasticsearch task by opaque id endpoint
  ['/api/estasks/:id/cancelwith'],
  // should not have admin check so users can use, each user is name spaced
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, User.checkPermissions(['hideStats'])],
  StatsAPIs.cancelUserESTask
);

app.post( // cancel all OpenSearch/Elasticsearch tasks endpoint
  ['/api/estasks/cancelall'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, User.checkRole('arkimeAdmin')],
  StatsAPIs.cancelAllESTasks
);

app.get( // OpenSearch/Elasticsearch admin settings endpoint
  ['/api/esadmin'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkEsAdminUser, setCookie],
  StatsAPIs.getESAdminSettings
);

app.post( // set OpenSearch/Elasticsearch admin setting endpoint
  ['/api/esadmin/set'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkEsAdminUser, checkCookieToken],
  StatsAPIs.setESAdminSettings
);

app.post( // reroute OpenSearch/Elasticsearch admin endpoint
  ['/api/esadmin/reroute'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkEsAdminUser, checkCookieToken],
  StatsAPIs.rerouteES
);

app.post( // flush OpenSearch/Elasticsearch admin endpoint
  ['/api/esadmin/flush'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkEsAdminUser, checkCookieToken],
  StatsAPIs.flushES
);

app.post( // unflood OpenSearch/Elasticsearch admin endpoint
  ['/api/esadmin/unflood'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkEsAdminUser, checkCookieToken],
  StatsAPIs.unfloodES
);

app.post( // unflood OpenSearch/Elasticsearch admin endpoint
  ['/api/esadmin/clearcache'],
  [ArkimeUtil.noCacheJson, recordResponseTime, checkEsAdminUser, checkCookieToken],
  StatsAPIs.clearCacheES
);

app.get( // OpenSearch/Elasticsearch shards endpoint
  ['/api/esshards'],
  [ArkimeUtil.noCacheJson, recordResponseTime, User.checkPermissions(['hideStats']), setCookie],
  StatsAPIs.getESShards
);

app.post( // exclude OpenSearch/Elasticsearch shard endpoint
  ['/api/esshards/:type/:value/exclude'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, User.checkRole('arkimeAdmin')],
  StatsAPIs.excludeESShard
);

app.post( // include OpenSearch/Elasticsearch shard endpoint
  ['/api/esshards/:type/:value/include'],
  [ArkimeUtil.noCacheJson, logAction(), checkCookieToken, User.checkRole('arkimeAdmin')],
  StatsAPIs.includeESShard
);

app.get( // OpenSearch/Elasticsearch recovery endpoint
  ['/api/esrecovery'],
  [ArkimeUtil.noCacheJson, recordResponseTime, User.checkPermissions(['hideStats']), setCookie],
  StatsAPIs.getESRecovery
);

// session apis ---------------------------------------------------------------
app.getpost( // sessions endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/sessions', '/sessions.json'],
  [ArkimeUtil.noCacheJson, recordResponseTime, fillQueryFromBody, logAction('sessions'), setCookie],
  SessionAPIs.getSessions
);

app.getpost( // spiview endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/spiview', '/spiview.json'],
  [ArkimeUtil.noCacheJson, recordResponseTime, fillQueryFromBody, logAction('spiview'), setCookie],
  SessionAPIs.getSPIView
);

app.getpost( // spigraph endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/spigraph', '/spigraph.json'],
  [ArkimeUtil.noCacheJson, recordResponseTime, fillQueryFromBody, logAction('spigraph'), setCookie, expToField],
  SessionAPIs.getSPIGraph
);

app.getpost( // spigraph hierarchy endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/spigraphhierarchy'],
  [ArkimeUtil.noCacheJson, recordResponseTime, fillQueryFromBody, logAction('spigraphhierarchy'), setCookie],
  SessionAPIs.getSPIGraphHierarchy
);

app.getpost( // build query endoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/buildquery', '/buildQuery.json'],
  [ArkimeUtil.noCacheJson, fillQueryFromBody, logAction('query')],
  SessionAPIs.getQuery
);

app.getpost( // sessions csv endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/sessions[/.]csv', /\/sessions.csv.*/],
  [fillQueryFromBody, logAction('sessions.csv')],
  SessionAPIs.getSessionsCSV
);

app.getpost( // unique endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/unique', '/unique.txt'],
  [fillQueryFromBody, logAction('unique'), expToField],
  SessionAPIs.getUnique
);

app.getpost( // multiunique endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/multiunique', '/multiunique.txt'],
  [fillQueryFromBody, logAction('multiunique')],
  SessionAPIs.getMultiunique
);

app.get( // session detail (SPI) endpoint
  ['/api/session/:nodeName/:id/detail'],
  [logAction()],
  SessionAPIs.getDetail
);

app.get( // session packets endpoint
  ['/api/session/:nodeName/:id/packets'],
  [logAction(), User.checkPermissions(['hidePcap'])],
  SessionAPIs.getPackets
);

app.post( // add tags endpoint
  ['/api/sessions/addtags'],
  [ArkimeUtil.noCacheJson, checkHeaderToken, logAction('addTags')],
  SessionAPIs.addTags
);

app.post( // remove tags endpoint
  ['/api/sessions/removetags'],
  [ArkimeUtil.noCacheJson, checkHeaderToken, logAction('removeTags'), User.checkPermissions(['removeEnabled'])],
  SessionAPIs.removeTags
);

app.get( // session body file endpoint
  ['/api/session/:nodeName/:id/body/:bodyType/:bodyNum/:bodyName', '/:nodeName/:id/body/:bodyType/:bodyNum/:bodyName'],
  [checkProxyRequest],
  SessionAPIs.getRawBody
);

app.get( // session body file image endpoint
  ['/api/session/:nodeName/:id/bodypng/:bodyType/:bodyNum/:bodyName', '/:nodeName/:id/bodypng/:bodyType/:bodyNum/:bodyName'],
  [checkProxyRequest],
  SessionAPIs.getFilePNG
);

app.get( // session pcap endpoint
  ['/api/sessions[/.]pcap', /\/sessions.pcap.*/],
  [logAction(), User.checkPermissions(['disablePcapDownload'])],
  SessionAPIs.getPCAP
);

app.get( // session pcapng endpoint
  ['/api/sessions[/.]pcapng', /\/sessions.pcapng.*/],
  [logAction(), User.checkPermissions(['disablePcapDownload'])],
  SessionAPIs.getPCAPNG
);

app.get( // session node pcap endpoint
  ['/api/session/:nodeName/:id[/.]pcap*'],
  [checkProxyRequest, User.checkPermissions(['disablePcapDownload'])],
  SessionAPIs.getPCAPFromNode
);

app.get( // session node pcapng endpoint
  ['/api/session/:nodeName/:id[/.]pcapng'],
  [checkProxyRequest, User.checkPermissions(['disablePcapDownload'])],
  SessionAPIs.getPCAPNGFromNode
);

app.get( // session entire pcap endpoint
  ['/api/session/entire/:nodeName/:id[/.]pcap'],
  [checkProxyRequest, User.checkPermissions(['disablePcapDownload'])],
  SessionAPIs.getEntirePCAP
);

app.get( // session packets file image endpoint
  ['/api/session/raw/:nodeName/:id[/.]png'],
  [checkProxyRequest, User.checkPermissions(['disablePcapDownload'])],
  SessionAPIs.getPacketPNG
);

app.get( // session raw packets endpoint
  ['/api/session/raw/:nodeName/:id', '/:nodeName/raw/:id'],
  [checkProxyRequest, User.checkPermissions(['disablePcapDownload'])],
  SessionAPIs.getRawPackets
);

app.get( // session file bodyhash endpoint
  ['/api/sessions/bodyhash/:hash', '/bodyHash/:hash'],
  [logAction('bodyhash')],
  SessionAPIs.getBodyHash
);

app.get( // session file bodyhash endpoint
  ['/api/session/:nodeName/:id/bodyhash/:hash'],
  [checkProxyRequest],
  SessionAPIs.getBodyHashFromNode
);

app.get( // sessions get decodings endpoint
  ['/api/sessions/decodings', '/decodings'],
  [ArkimeUtil.noCacheJson],
  SessionAPIs.getDecodings
);

app.get( // session send to node endpoint - used by SessionAPIs.#sendSessionsList
  ['/api/session/:nodeName/:id/send'],
  [checkProxyRequest],
  SessionAPIs.sendSessionToNode
);

app.post( // sessions send to node endpoint - used by CronAPIs.#sendSessionsListQL
  ['/api/sessions/:nodeName/send'],
  [checkProxyRequest],
  SessionAPIs.sendSessionsToNode
);

app.post( // sessions send endpoint - used by vueapp
  ['/api/sessions/send'],
  SessionAPIs.sendSessions
);

app.post( // sessions recieve endpoint
  ['/api/sessions/receive', '/receiveSession'],
  [ArkimeUtil.noCacheJson],
  SessionAPIs.receiveSession
);

app.post( // delete data endpoint
  ['/api/delete', '/delete'],
  [ArkimeUtil.noCacheJson, checkCookieToken, logAction(), User.checkPermissions(['removeEnabled'])],
  SessionAPIs.deleteData
);

// connections apis -----------------------------------------------------------
app.getpost( // connections endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/connections', '/connections.json'],
  [ArkimeUtil.noCacheJson, recordResponseTime, fillQueryFromBody, logAction('connections'), setCookie],
  ConnectionAPIs.getConnections
);

app.getpost( // connections csv endpoint (POST or GET) - uses fillQueryFromBody to
  // fill the query parameters if the client uses POST to support POST and GET
  ['/api/connections[/.]csv', '/connections.csv'],
  [fillQueryFromBody, logAction('connections.csv')],
  ConnectionAPIs.getConnectionsCSV
);

// hunt apis ------------------------------------------------------------------
app.get( // hunts endpoint
  ['/api/hunts'],
  [ArkimeUtil.noCacheJson, disableInMultiES, recordResponseTime, User.checkPermissions(['packetSearch']), setCookie],
  HuntAPIs.getHunts
);

app.post( // create hunt endpoint
  ['/api/hunt'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt'), checkCookieToken, User.checkPermissions(['packetSearch'])],
  HuntAPIs.createHunt
);

app.delete( // delete hunt endpoint
  ['/api/hunt/:id'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt/:id'), checkCookieToken, User.checkPermissions(['packetSearch']), checkHuntAccess],
  HuntAPIs.deleteHunt
);

app.put( // update hunt endpoint
  ['/api/hunt/:id'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt/:id'), checkCookieToken, User.checkPermissions(['packetSearch']), checkHuntAccess],
  HuntAPIs.updateHunt
);

app.put( // cancel hunt endpoint
  ['/api/hunt/:id/cancel'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt/:id/cancel'), checkCookieToken, User.checkPermissions(['packetSearch']), checkHuntAccess],
  HuntAPIs.cancelHunt
);

app.put( // pause hunt endpoint
  ['/api/hunt/:id/pause'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt/:id/pause'), checkCookieToken, User.checkPermissions(['packetSearch']), checkHuntAccess],
  HuntAPIs.pauseHunt
);

app.put( // play hunt endpoint
  ['/api/hunt/:id/play'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt/:id/play'), checkCookieToken, User.checkPermissions(['packetSearch']), checkHuntAccess],
  HuntAPIs.playHunt
);

app.put( // remove from sessions hunt endpoint
  ['/api/hunt/:id/removefromsessions'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt/:id/removefromsessions'), checkCookieToken, User.checkPermissions(['packetSearch', 'removeEnabled']), checkHuntAccess],
  HuntAPIs.removeFromSessions
);

app.post( // add users to hunt endpoint
  ['/api/hunt/:id/users'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt/:id/users'), checkCookieToken, User.checkPermissions(['packetSearch']), checkHuntAccess],
  HuntAPIs.addUsers
);

app.delete( // remove users from hunt endpoint
  ['/api/hunt/:id/user/:user'],
  [ArkimeUtil.noCacheJson, disableInMultiES, logAction('hunt/:id/user/:user'), checkCookieToken, User.checkPermissions(['packetSearch']), checkHuntAccess],
  HuntAPIs.removeUsers
);

app.get( // remote hunt endpoint
  ['/api/hunt/:nodeName/:huntId/remote/:sessionId'],
  [ArkimeUtil.noCacheJson],
  HuntAPIs.remoteHunt
);

// shortcut apis ----------------------------------------------------------------
app.get( // get shortcuts endpoint
  ['/api/shortcuts'],
  [ArkimeUtil.noCacheJson, getSettingUserCache, recordResponseTime],
  ShortcutAPIs.getShortcuts
);

app.post( // create shortcut endpoint
  ['/api/shortcut'],
  [ArkimeUtil.noCacheJson, Auth.getSettingUserDb, logAction('shortcut'), checkCookieToken],
  ShortcutAPIs.createShortcut
);

app.put( // update shortcut endpoint
  ['/api/shortcut/:id'],
  [ArkimeUtil.noCacheJson, Auth.getSettingUserDb, logAction('shortcut/:id'), checkCookieToken, Auth.checkResourceAccess(Db.getShortcut, 'userId')],
  ShortcutAPIs.updateShortcut
);

app.delete( // delete shortcut endpoint
  ['/api/shortcut/:id'],
  [ArkimeUtil.noCacheJson, Auth.getSettingUserDb, logAction('shortcut/:id'), checkCookieToken, Auth.checkResourceAccess(Db.getShortcut, 'userId')],
  ShortcutAPIs.deleteShortcut
);

app.get( // sync shortcuts endpoint
  ['/api/syncshortcuts'],
  [ArkimeUtil.noCacheJson],
  ShortcutAPIs.syncShortcuts
);

// file apis ------------------------------------------------------------------
app.get( // fields endpoint
  ['/api/fields'],
  [ArkimeUtil.noCacheJson],
  MiscAPIs.getFields
);

app.get( // files endpoint
  ['/api/files'],
  [ArkimeUtil.noCacheJson, recordResponseTime, logAction('files'), User.checkPermissions(['hideFiles']), setCookie],
  MiscAPIs.getFiles
);

// menu actions apis ---------------------------------------------------------
app.get( // value actions endpoint
  ['/api/valueactions', '/api/valueActions'],
  [ArkimeUtil.noCacheJson, User.checkPermissions(['webEnabled'])],
  MiscAPIs.getValueActions
);

app.get( // field actions endpoint
  ['/api/fieldactions', '/api/fieldActions'],
  [ArkimeUtil.noCacheJson, User.checkPermissions(['webEnabled'])],
  MiscAPIs.getFieldActions
);

// reverse dns apis -----------------------------------------------------------
app.get( // reverse dns endpoint
  ['/api/reversedns'],
  [ArkimeUtil.noCacheJson, logAction()],
  MiscAPIs.getReverseDNS
);

// uploads apis ---------------------------------------------------------------
app.post(
  ['/api/upload'],
  [checkCookieToken, multer({ dest: '/tmp', limits: internals.uploadLimits }).single('file')],
  MiscAPIs.upload
);

// clusters apis --------------------------------------------------------------
app.get(
  ['/api/clusters'],
  [ArkimeUtil.noCacheJson],
  MiscAPIs.getClusters
);

app.get(
  ['/api/remoteclusters'],
  [ArkimeUtil.noCacheJson],
  MiscAPIs.getRemoteClusters
);

// app apis -------------------------------------------------------------------
app.get(
  '/api/appinfo',
  [ArkimeUtil.noCacheJson, checkCookieToken, getSettingUserCache, User.checkPermissions(['webEnabled'])],
  MiscAPIs.getAppInfo
);

// cyberchef apis -------------------------------------------------------------
app.get('/cyberchef.html', [cyberchefCspHeader], express.static( // cyberchef client file endpoint
  path.join(__dirname, '/public'),
  { maxAge: dayMs, fallthrough: false }
), ArkimeUtil.missingResource);

app.get( // cyberchef endpoint
  '/cyberchef/:nodeName/session/:id',
  [User.checkPermissions(['webEnabled']), checkProxyRequest, cyberchefCspHeader],
  MiscAPIs.cyberChef
);

app.use( // cyberchef UI endpoint
  ['/cyberchef/', '/modules/'],
  cyberchefCspHeader,
  MiscAPIs.getCyberChefUI
);

// ============================================================================
// VUE APP
// ============================================================================
const Vue = require('vue');
const vueServerRenderer = require('vue-server-renderer');

// using fallthrough: false because there is no 404 endpoint (client router
// handles 404s) and sending index.html is confusing
// expose vue bundles
app.use('/static', express.static(
  path.join(__dirname, '/vueapp/dist/static'),
  { maxAge: dayMs, fallthrough: false }
), ArkimeUtil.missingResource);

app.use(cspHeader, setCookie, (req, res) => {
  if (!req.user.webEnabled) {
    return res.status(403).send('Permission denied');
  }

  if (req.path === '/users' && !req.user.hasRole('usersAdmin')) {
    return res.status(403).send('Permission denied');
  }

  if (req.path === '/settings' && req.user.isDemoMode()) {
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

  const limit = req.user.hasRole('arkimeAdmin') ? Config.get('huntAdminLimit', 10000000) : Config.get('huntLimit', 1000000);

  const appContext = {
    theme,
    titleConfig,
    path: Config.basePath(),
    version: version.version,
    demoMode: req.user.isDemoMode(),
    multiViewer: internals.multiES,
    hasUsersES: !!Config.get('usersElasticsearch', false),
    themeUrl: theme === 'custom-theme' ? 'api/user/css' : '',
    huntWarn: Config.get('huntWarn', 100000),
    huntLimit: limit,
    nonce: res.locals.nonce,
    anonymousMode: Auth.isAnonymousMode() && !Config.regressionTests,
    businesDayStart: Config.get('businessDayStart', false),
    businessDayEnd: Config.get('businessDayEnd', false),
    businessDays: Config.get('businessDays', '1,2,3,4,5'),
    turnOffGraphDays: Config.get('turnOffGraphDays', 30),
    disableUserPasswordUI: Config.get('disableUserPasswordUI', true),
    logoutUrl: Auth.logoutUrl,
    defaultTimeRange: Config.get('defaultTimeRange', '1')
  };

  // Create a fresh Vue app instance
  const vueApp = new Vue({
    template: '<div id="app"></div>'
  });

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

// Replace the default express error handler
app.use(ArkimeUtil.expressErrorHandler);

// ============================================================================
// MAIN
// ============================================================================
async function main () {
  if (!fs.existsSync(path.join(process.cwd(), '/views/mixins.pug'))) {
    console.error('ERROR - ./views/mixins.pug missing - The viewer app MUST be run from inside the viewer directory');
    process.exit();
  }

  if (!fs.existsSync(path.join(__dirname, '/vueapp/dist/index.html')) && app.settings.env !== 'development') {
    console.log('WARNING - ./vueapp/dist/index.html missing - The viewer app must be run from inside the viewer directory');
  }

  Db.checkVersion(MIN_DB_VERSION);

  try {
    const health = await Db.healthCache();
    internals.clusterName = health.cluster_name;
  } catch (err) {
    console.log('ERROR - fetching OpenSearch/Elasticsearch health', err);
  }

  try {
    const { body: info } = await Db.nodesStats({
      metric: 'jvm,process,fs,os,indices,thread_pool'
    });
    info.nodes.timestamp = new Date().getTime();
    internals.previousNodesStats.push(info.nodes);
  } catch (err) {
    console.log('ERROR - fetching OpenSearch/Elasticsearch nodes stats', err);
  }

  setFieldLocals();
  setInterval(setFieldLocals, 2 * 60 * 1000);

  loadPlugins();

  const pcapWriteMethod = Config.get('pcapWriteMethod');
  const writer = internals.writers.get(pcapWriteMethod);
  if (!writer || writer.localNode === true) {
    expireCheckAll();
    setInterval(expireCheckAll, 60 * 1000);
  }

  createActions('value-actions', 'makeRightClick', 'rightClicks');
  setInterval(() => createActions('value-actions', 'makeRightClick', 'rightClicks'), 150 * 1000); // Check every 2.5 minutes
  createActions('field-actions', 'makeFieldActions', 'fieldActions');
  setInterval(() => createActions('field-actions', 'makeFieldActions', 'fieldActions'), 150 * 1000); // Check every 2.5 minutes

  const viewHost = Config.get('viewHost', undefined);
  if (internals.userNameHeader !== undefined && viewHost !== 'localhost' && viewHost !== '127.0.0.1') {
    console.log('SECURITY WARNING - when userNameHeader is set, viewHost should be localhost or use iptables');
  }

  const server = ArkimeUtil.createHttpServer(app, viewHost, Config.get('viewPort', '8005'));
  server.setTimeout(20 * 60 * 1000);
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
      console.log('  -c, --config <file|url>  Where to fetch the config file from');
      console.log('  -n <node name>           Node name section to use in config file, default first part of hostname');
      console.log('  --debug                  Increase debug level, multiple are supported');
      console.log('  --esprofile              Turn on profiling to es search queries');
      console.log('  --host <host name>       Host name to use, default os hostname');
      console.log('  --insecure               Disable certificate verification for https calls');

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

async function premain () {
  await Config.initialize();

  Db.initialize({
    host: internals.elasticBase,
    prefix: internals.prefix,
    usersHost: Config.getArray('usersElasticsearch'),
    // The default for usersPrefix should be '' if this is a multiviewer, otherwise Db.initialize will figure out
    usersPrefix: Config.get('usersPrefix', internals.multiES ? '' : undefined),
    nodeName: Config.nodeName(),
    hostName: Config.hostName(),
    esClientKey: Config.get('esClientKey', null),
    esClientCert: Config.get('esClientCert', null),
    esClientKeyPass: Config.get('esClientKeyPass', null),
    multiES: internals.multiES,
    insecure: ArkimeConfig.insecure,
    caTrustFile: Config.get('caTrustFile', null),
    requestTimeout: Config.get('elasticsearchTimeout', 300),
    esProfile: Config.esProfile,
    debug: Config.debug,
    esApiKey: Config.get('elasticsearchAPIKey', null),
    usersEsApiKey: Config.get('usersElasticsearchAPIKey', null),
    esBasicAuth: Config.get('elasticsearchBasicAuth', null),
    usersEsBasicAuth: Config.get('usersElasticsearchBasicAuth', null),
    isPrimaryViewer: CronAPIs.isPrimaryViewer,
    getCurrentUserCB: UserAPIs.getCurrentUserCB,
    maxConcurrentShardRequests: Config.get('esMaxConcurrentShardRequests')
  }, main);

  Notifier.initialize({
    prefix: Config.get('usersPrefix', Config.get('prefix', 'arkime')),
    esclient: User.getClient()
  });

  CronAPIs.initialize({
  });
}

premain();
