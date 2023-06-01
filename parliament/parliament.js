#!/usr/bin/env node
'use strict';

const MIN_PARLIAMENT_VERSION = 3;

/* dependencies ------------------------------------------------------------- */
const express = require('express');
const http = require('http');
const https = require('https');
const fs = require('fs');
const favicon = require('serve-favicon');
const axios = require('axios');
const bp = require('body-parser');
const logger = require('morgan');
const jwt = require('jsonwebtoken');
const bcrypt = require('bcrypt');
const cryptoLib = require('crypto');
const glob = require('glob');
const os = require('os');
const helmet = require('helmet');
const uuid = require('uuid').v4;
const upgrade = require('./upgrade');
const path = require('path');
const chalk = require('chalk');
const dayMs = 60000 * 60 * 24;
const User = require('../common/user');
const Auth = require('../common/auth');
const version = require('../common/version');
const ArkimeUtil = require('../common/arkimeUtil');

/* app setup --------------------------------------------------------------- */
const app = express();
const router = express.Router();

const saltrounds = 13;

const issueTypes = {
  esRed: { on: true, name: 'ES Red', text: 'ES is red', severity: 'red', description: 'ES status is red' },
  esDown: { on: true, name: 'ES Down', text: ' ES is down', severity: 'red', description: 'ES is unreachable' },
  esDropped: { on: true, name: 'ES Dropped', text: 'ES is dropping bulk inserts', severity: 'yellow', description: 'the capture node is overloading ES' },
  outOfDate: { on: true, name: 'Out of Date', text: 'has not checked in since', severity: 'red', description: 'the capture node has not checked in' },
  noPackets: { on: true, name: 'Low Packets', text: 'is not receiving many packets', severity: 'red', description: 'the capture node is not receiving many packets' }
};

const settingsDefault = {
  general: {
    noPackets: 0,
    noPacketsLength: 10,
    outOfDate: 30,
    esQueryTimeout: 5,
    removeIssuesAfter: 60,
    removeAcknowledgedAfter: 15
  },
  commonAuth: {},
  notifiers: {}
};

const internals = {
  notifierTypes: {},
  authSetupCode: cryptoLib.randomBytes(20).toString('base64').replace(/[=+/]/g, '').substr(0, 10),
  insecure: false
};

const parliamentReadError = `\nYou must fix this before you can run Parliament.
  Try using parliament.example.json as a starting point`;

// keep a map of invalid tokens for when a user logs out before jwt expires
const invalidTokens = {};

(function () { // parse arguments
  const appArgs = process.argv.slice(2);
  let file, port;
  let debug = 0;

  function setPasswordHash (err, hash) {
    if (err) {
      console.log(`Error hashing password: ${err}`);
      return;
    }

    app.set('password', hash);
  }

  function help () {
    console.log('parliament.js [<config options>]\n');
    console.log('Config Options:');
    console.log('  -c, --config   Parliament config file to use');
    console.log('  --pass         Password for updating the parliament');
    console.log('  --port         Port for the web app to listen on');
    console.log('  --cert         Public certificate to use for https');
    console.log('  --key          Private certificate to use for https');
    console.log('  --debug        Increase debug level, multiple are supported');
    console.log('  --insecure     Disable certificate verification for https calls');

    process.exit(0);
  }

  for (let i = 0, len = appArgs.length; i < len; i++) {
    switch (appArgs[i]) {
    case '-c':
    case '--config':
      file = appArgs[i + 1];
      i++;
      break;

    case '--pass':
      bcrypt.hash(appArgs[i + 1], saltrounds, setPasswordHash);
      app.set('hasPass', true);
      i++;
      break;

    case '--port':
      port = appArgs[i + 1];
      i++;
      break;

    case '--cert':
      app.set('certFile', appArgs[i + 1]);
      i++;
      break;

    case '--key':
      app.set('keyFile', appArgs[i + 1]);
      i++;
      break;

    case '--dashboardOnly':
      app.set('dashboardOnly', true);
      break;

    case '--regressionTests':
      app.set('regressionTests', 1);
      break;

    case '--debug':
      debug++;
      break;

    case '--insecure':
      internals.insecure = true;
      break;

    case '-h':
    case '--help':
      help();
      break;

    default:
      console.log(`Unknown option ${appArgs[i]}`);
      help();
      break;
    }
  }

  if (file && !file.endsWith('.json') && file !== '/dev/null') {
    console.log(`ERROR: Parliament config filename ${file} must end with .json`);
    process.exit(1);
  }

  if (!appArgs.length) {
    console.log('WARNING: No config options were set, starting Parliament in view only mode with defaults.\n');
  }

  app.set('debug', debug);

  // set optional config options that reqiure defaults
  app.set('port', port ?? 8008);
  app.set('file', file ?? './parliament.json');

  internals.httpsAgent = new https.Agent({ rejectUnauthorized: !internals.insecure });
}());

if (app.get('regressionTests')) {
  app.post('/regressionTests/shutdown', function (req, res) {
    process.exit(0);
  });
  internals.authSetupCode = '0000000000';
}

// parliament object!
let parliament;

try { // check if the file exists
  fs.accessSync(app.get('file'), fs.constants.F_OK);
} catch (e) { // if the file doesn't exist, create it
  try { // write the new file
    parliament = { version: MIN_PARLIAMENT_VERSION };
    fs.writeFileSync(app.get('file'), JSON.stringify(parliament, null, 2), 'utf8');
  } catch (err) { // notify of error saving new parliament and exit
    console.log('Error creating new Parliament:\n\n', ArkimeUtil.sanitizeStr(e.stack));
    console.log(parliamentReadError);
    process.exit(1);
  }
}

try { // get the parliament file or error out if it's unreadable
  parliament = require(`${app.get('file')}`);
  // set the password if passed in when starting the server
  // IMPORTANT! this will overwrite any password in the parliament json file
  if (app.get('password')) {
    parliament.password = app.get('password');
  } else if (parliament.password) {
    // if the password is not supplied when starting the server,
    // use any existing password in the parliament json file
    app.set('hasPass', true);
    app.set('password', parliament.password);
  }
} catch (err) {
  console.log(`Error reading ${app.get('file') ?? 'your parliament file'}:\n\n`, ArkimeUtil.sanitizeStr(err.stack));
  console.log(parliamentReadError);
  process.exit(1);
}

// optional config code for setting up auth
if (!app.get('dashboardOnly') && !app.get('hasPass')) {
  // if we're not in dashboardOnly mode and we don't have a password
  console.log(chalk.cyan(
    `${chalk.bgCyan.black('IMPORTANT')} - Auth setup pin code is: ${internals.authSetupCode}`
  ));
}

// construct the issues file name
let issuesFilename = 'issues.json';
if (app.get('file').indexOf('.json') > -1) {
  const filename = app.get('file').replace(/\.json/g, '');
  issuesFilename = `${filename}.issues.json`;
}
app.set('issuesfile', issuesFilename);

// get the issues file or create it if it doesn't exist
let issues;
try {
  issues = require(issuesFilename);
} catch (err) {
  issues = [];
}

// define ids for groups and clusters
let globalGroupId = 0;
let globalClusterId = 0;

// save noPackets issues so that the time of issue can be compared to the
// noPacketsLength user setting (only issue alerts when the time the issue
// was encounterd exceeds the noPacketsLength user setting)
const noPacketsMap = {};

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
  styleSrc: ["'self'"],
  // need unsafe-eval for vue full build: https://vuejs.org/v2/guide/installation.html#CSP-environments
  scriptSrc: ["'self'", "'unsafe-eval'", (req, res) => `'nonce-${res.locals.nonce}'`],
  objectSrc: ["'none'"],
  imgSrc: ["'self'"]
};
if (process.env.NODE_ENV === 'development') {
  // need unsafe inline styles for hot module replacement
  cspDirectives.styleSrc.push("'unsafe-inline'");
}
const cspHeader = helmet.contentSecurityPolicy({
  directives: cspDirectives
});
app.use(cspHeader);

function setCookie (req, res, next) {
  if (parliament.authMode) {
    const cookieOptions = {
      path: '/parliament',
      sameSite: 'Strict',
      overwrite: true
    };
    // make cookie secure on https
    if (app.get('keyFile') && app.get('certFile')) { cookieOptions.secure = true; }

    res.cookie( // send cookie for basic, non admin functions
      'PARLIAMENT-COOKIE',
      Auth.obj2auth({
        date: Date.now(),
        pid: process.pid,
        userId: req.user.userId
      }),
      cookieOptions
    );
  }
  return next();
}

function checkCookieToken (req, res, next) {
  if (parliament.authMode) {
    if (!req.headers['x-parliament-cookie']) {
      return next(newError(500, 'Missing token'));
    }

    const cookie = req.headers['x-parliament-cookie'];
    req.token = Auth.auth2obj(cookie);
    const diff = Math.abs(Date.now() - req.token.date);
    if (diff > 2400000 || req.token.userId !== req.user.userId) {
      console.trace('bad token', req.token, diff, req.token.userId, req.user.userId);
      return next(newError(500, 'Timeout - Please try reloading page and repeating the action'));
    }
  }

  return next();
}

// using fallthrough: false because there is no 404 endpoint (client router
// handles 404s) and sending index.html is confusing
app.use('/parliament/font-awesome', express.static(
  path.join(__dirname, '/../node_modules/font-awesome'),
  { maxAge: dayMs, fallthrough: false }
), ArkimeUtil.missingResource);
app.use('/parliament/assets', express.static(
  path.join(__dirname, '/../assets'),
  { maxAge: dayMs, fallthrough: false }
), ArkimeUtil.missingResource);

// log requests
app.use(logger(':date \x1b[1m:method\x1b[0m \x1b[33m:url\x1b[0m :status :res[content-length] bytes :response-time ms', { stream: process.stdout }));

app.use(favicon(path.join(__dirname, '/favicon.ico')));

// define router to mount api related functions
app.use('/parliament/api', router);
router.use(bp.json());
router.use(bp.urlencoded({ extended: true }));

// Load notifier plugins for Parliament alerting
function loadNotifiers () {
  const api = {
    register: function (str, info) {
      internals.notifierTypes[str] = info;
    }
  };

  // look for all notifier providers and initialize them
  const files = glob.sync(path.join(__dirname, '/../common/notifier.*.js'));
  files.forEach((file) => {
    const plugin = require(file);
    plugin.init(api);
  });
}

loadNotifiers();

function newError (code, msg) {
  const error = new Error(msg);
  error.httpStatusCode = code;
  return error;
}

/* Middleware -------------------------------------------------------------- */
// App should always have parliament data
router.use((req, res, next) => {
  if (!parliament) {
    return next(newError(500, 'Unable to fetch parliament data.'));
  }

  next();
});

// Replace the default express error handler
app.use((err, req, res, next) => {
  console.log(ArkimeUtil.sanitizeStr(err.stack));
  res.status(err.httpStatusCode ?? 500).json({
    success: false,
    text: err.message ?? 'Error'
  });
});

// Verify token
function verifyToken (req, res, next) {
  function tokenError (req, res, errorText) {
    errorText = errorText ?? 'Token Error!';
    res.status(403).json({
      tokenError: true,
      success: false,
      text: `Permission Denied: ${errorText}`
    });
  }

  const hasAuth = !!app.get('password');
  if (!hasAuth) {
    return tokenError(req, res, 'No password set.');
  }

  // check for token in header, url parameters, or post parameters
  const token = req.body.token ?? req.query.token ?? req.headers['x-access-token'];

  if (!token) {
    return tokenError(req, res, 'No token provided.');
  }

  // check for invalid token
  if (invalidTokens[token]) {
    return tokenError(req, res, 'You\'ve been logged out. Please login again.');
  }

  // verifies token and expiration
  jwt.verify(token, app.get('password'), (err, decoded) => {
    if (err) {
      return tokenError(req, res, 'Failed to authenticate token. Try logging in again.');
    } else {
      // if everything is good, save to request for use in other routes
      req.decoded = decoded;
      next();
    }
  });
}

function checkAuthUpdate (req, res, next) {
  if (app.get('dashboardOnly')) {
    return next(newError(403, 'Your Parliament is in dasboard only mode.'));
  }

  if (app.get('password') ?? parliament.authMode) {
    return isAdmin(req, res, next);
  }

  if (req.body !== undefined && req.body.authSetupCode === internals.authSetupCode) {
    return next();
  } else {
    console.log(chalk.cyan(
      `${chalk.bgCyan.black('IMPORTANT')} - Incorrect auth setup code used! Code is: ${internals.authSetupCode}`
    ));
    return res.status(403).send(JSON.stringify({ success: false, text: 'Not authorized, check log file' })); // not specific error
  }
}

function isUser (req, res, next) {
  if (!parliament.authMode) { return verifyToken(req, res, next); }

  Auth.doAuth(req, res, () => {
    if (req.user.hasRole('parliamentUser')) {
      return next();
    }

    res.status(403).json({
      tokenError: true,
      success: false,
      text: 'Permission Denied: Not a Parliament user'
    });
  });
}

function isAdmin (req, res, next) {
  if (!parliament.authMode) { return verifyToken(req, res, next); }

  Auth.doAuth(req, res, () => {
    if (req.user.hasRole('parliamentAdmin')) {
      return next();
    }

    res.status(403).json({
      tokenError: true,
      success: false,
      text: 'Permission Denied: Not a Parliament admin'
    });
  });
}

/* Helper functions -------------------------------------------------------- */
// list of alerts that will be sent at every 10 seconds
let alerts = [];
// sends alerts in the alerts list
async function sendAlerts () {
  const promise = new Promise((resolve, reject) => {
    for (let index = 0, len = alerts.length; index < len; index++) {
      (function (i) {
        // timeout so that alerts are alerted in order
        setTimeout(() => {
          const alertToSend = alerts[i];
          const links = [];
          if (parliament.settings.general.includeUrl) {
            links.push({
              text: 'Parliament Dashboard',
              url: `${parliament.settings.general.hostname}?searchTerm=${alert.cluster}`
            });
          }
          alertToSend.notifier.sendAlert(alertToSend.config, alertToSend.message, links);
          if (app.get('debug')) {
            console.log('Sending alert:', alertToSend.message, JSON.stringify(alertToSend.config, null, 2));
          }
          if (i === len - 1) { resolve(); }
        }, 250 * i);
      })(index);
    }
  });

  promise.then(() => {
    alerts = []; // clear the queue
  });
}

// sorts the list of alerts by cluster title then sends them
// assumes that the alert message starts with the cluster title
function processAlerts () {
  if (alerts && alerts.length) {
    alerts.sort((a, b) => {
      return a.message.localeCompare(b.message);
    });

    sendAlerts();
  }
}

function formatIssueMessage (cluster, issue) {
  let message = '';

  if (issue.node) { message += `${issue.node} `; }

  message += `${issue.text}`;

  if (issue.value !== undefined) {
    let value = ': ';

    if (issue.type === 'esDropped') {
      value += issue.value.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ',');
    } else if (issue.type === 'outOfDate') {
      value += new Date(issue.value);
    } else {
      value += issue.value;
    }

    message += `${value}`;
  }

  return message;
}

function buildAlert (cluster, issue) {
  // if there are no notifiers set, skip everything, there's nowhere to alert
  if (!parliament.settings.notifiers) { return; }

  issue.alerted = Date.now();

  const message = `${cluster.title} - ${issue.message}`;

  for (const n in parliament.settings.notifiers) {
    const setNotifier = parliament.settings.notifiers[n];

    // keep looking for notifiers if the notifier is off
    if (!setNotifier || !setNotifier.on) { continue; }

    // quit before sending the alert if the alert is off
    if (!setNotifier.alerts[issue.type]) { continue; }

    const config = {};
    const notifierDef = internals.notifierTypes[setNotifier.type];

    for (const f in notifierDef.fields) {
      const fieldDef = notifierDef.fields[f];
      const field = setNotifier.fields[fieldDef.name];
      if (!field || (fieldDef.required && !field.value)) {
        // field doesn't exist, or field is required and doesn't have a value
        console.log(`Missing the ${field.name} field for ${n} alerting. Add it on the settings page.`);
        continue;
      }
      config[fieldDef.name] = field.value;
    }

    alerts.push({
      config,
      message,
      notifier: notifierDef,
      cluster: cluster.title
    });
  }
}

// Finds an issue in a cluster
function findIssue (clusterId, issueType, node) {
  for (const issue of issues) {
    if (issue.clusterId === clusterId &&
      issue.type === issueType &&
      issue.node === node) {
      return issue;
    }
  }
}

// Updates an existing issue or pushes a new issue onto the issue array
function setIssue (cluster, newIssue) {
  // build issue
  const issueType = issueTypes[newIssue.type];
  newIssue.text = issueType.text;
  newIssue.title = issueType.name;
  newIssue.severity = issueType.severity;
  newIssue.clusterId = cluster.id;
  newIssue.cluster = cluster.title;
  newIssue.message = formatIssueMessage(cluster, newIssue);
  newIssue.provisional = true;

  let existingIssue = false;

  // don't duplicate existing issues, update them
  for (const issue of issues) {
    if (issue.clusterId === newIssue.clusterId &&
        issue.type === newIssue.type &&
        issue.node === newIssue.node) {
      existingIssue = true;

      // this is at least the second time we've seen this issue
      // so it must be a persistent issue
      issue.provisional = false;

      if (Date.now() > issue.ignoreUntil && issue.ignoreUntil !== -1) {
        // the ignore has expired, so alert!
        issue.ignoreUntil = undefined;
        issue.alerted = undefined;
      }

      issue.lastNoticed = Date.now();

      // if the issue has not been acknowledged, ignored, or alerted, or
      // if the cluster is not a no alert cluster or multiviewer cluster,
      // build and issue an alert
      if (!issue.acknowledged && !issue.ignoreUntil &&
        !issue.alerted && cluster.type !== 'noAlerts' &&
        cluster.type !== 'multiviewer') {
        buildAlert(cluster, issue);
      }
    }
  }

  if (!existingIssue) {
    // this is the first time we've seen this issue
    // don't alert yet, but create the issue
    newIssue.firstNoticed = Date.now();
    newIssue.lastNoticed = Date.now();
    issues.push(newIssue);
  }

  if (app.get('debug') > 1) {
    console.log('Setting issue:', JSON.stringify(newIssue, null, 2));
  }

  const issuesError = validateIssues();
  if (!issuesError) {
    fs.writeFile(app.get('issuesfile'), JSON.stringify(issues, null, 2), 'utf8',
      (err) => {
        if (err) {
          console.log('Unable to write issue:', err.message ?? err);
        }
      }
    );
  }
}

// Retrieves the health of each cluster and updates the cluster with that info
function getHealth (cluster) {
  return new Promise((resolve, reject) => {
    const timeout = getGeneralSetting('esQueryTimeout') * 1000;

    const options = {
      url: `${cluster.localUrl ?? cluster.url}/eshealth.json`,
      method: 'GET',
      httpsAgent: internals.httpsAgent,
      timeout
    };

    axios(options)
      .then((response) => {
        cluster.healthError = undefined;

        let health;
        try {
          health = response.data;
        } catch (e) {
          cluster.healthError = 'ES health parse failure';
          console.log('Bad response for es health', cluster.localUrl ?? cluster.url);
          return resolve();
        }

        if (health) {
          cluster.status = health.status;
          cluster.totalNodes = health.number_of_nodes;
          cluster.dataNodes = health.number_of_data_nodes;

          if (cluster.status === 'red') { // alert on red es status
            setIssue(cluster, { type: 'esRed' });
          }
        }

        return resolve();
      })
      .catch((error) => {
        const message = error.message ?? error;

        setIssue(cluster, { type: 'esDown', value: message });

        cluster.healthError = message;

        if (app.get('debug')) {
          console.log('HEALTH ERROR:', options.url, message);
        }

        return resolve();
      });
  });
}

// Retrieves, then calculates stats for each cluster and updates the cluster with that info
function getStats (cluster) {
  return new Promise((resolve, reject) => {
    const timeout = getGeneralSetting('esQueryTimeout') * 1000;

    const options = {
      url: `${cluster.localUrl ?? cluster.url}/api/parliament`,
      method: 'GET',
      httpsAgent: internals.httpsAgent,
      timeout
    };

    // Get now before the query since we don't know how long query/response will take
    const now = Date.now() / 1000;
    axios(options)
      .then((response) => {
        cluster.statsError = undefined;

        if (response.data.bsqErr) {
          cluster.statsError = response.data.bsqErr;
          console.log('Get stats error', response.data.bsqErr);
          return resolve();
        }

        let stats;
        try {
          stats = response.data;
        } catch (e) {
          cluster.statsError = 'ES stats parse failure';
          console.log('Bad response for stats', cluster.localUrl ?? cluster.url);
          return resolve();
        }

        if (!stats || !stats.data) { return resolve(); }

        cluster.deltaBPS = 0;
        cluster.deltaTDPS = 0;
        cluster.molochNodes = 0;
        cluster.monitoring = 0;

        const outOfDate = getGeneralSetting('outOfDate');

        for (const stat of stats.data) {
          // sum delta bytes per second
          if (stat.deltaBytesPerSec) {
            cluster.deltaBPS += stat.deltaBytesPerSec;
          }

          // sum delta total dropped per second
          if (stat.deltaTotalDroppedPerSec) {
            cluster.deltaTDPS += stat.deltaTotalDroppedPerSec;
          }

          if (stat.monitoring) {
            cluster.monitoring += stat.monitoring;
          }

          if ((now - stat.currentTime) <= outOfDate && stat.deltaPacketsPerSec > 0) {
            cluster.molochNodes++;
          }

          // Look for issues
          if ((now - stat.currentTime) > outOfDate) {
            setIssue(cluster, {
              type: 'outOfDate',
              node: stat.nodeName,
              value: stat.currentTime * 1000
            });
          }

          // look for no packets issue
          if (stat.deltaPacketsPerSec <= getGeneralSetting('noPackets')) {
            const id = cluster.title + stat.nodeName;

            // only set the noPackets issue if there is a record of this cluster/node
            // having noPackets and that issue has persisted for the set length of time
            if (noPacketsMap[id] &&
              Date.now() - noPacketsMap[id] >= (getGeneralSetting('noPacketsLength') * 1000)) {
              setIssue(cluster, {
                type: 'noPackets',
                node: stat.nodeName,
                value: stat.deltaPacketsPerSec
              });
            } else if (!noPacketsMap[id]) {
              // if this issue has not been encountered yet, make a record of it
              noPacketsMap[id] = Date.now();
            }
          }

          if (stat.deltaESDroppedPerSec > 0) {
            setIssue(cluster, {
              type: 'esDropped',
              node: stat.nodeName,
              value: stat.deltaESDroppedPerSec
            });
          }
        }

        return resolve();
      })
      .catch((error) => {
        const message = error.message ?? error;

        setIssue(cluster, { type: 'esDown', value: message });

        cluster.statsError = message;

        if (app.get('debug')) {
          console.log('STATS ERROR:', options.url, message);
        }

        return resolve();
      });
  });
}

function buildNotifierTypes () {
  for (const n in internals.notifierTypes) {
    const notifier = internals.notifierTypes[n];
    // add alert issue types to notifiers
    notifier.alerts = issueTypes;
    // make fields a map
    const fieldsMap = {};
    for (const field of notifier.fields) {
      fieldsMap[field.name] = field;
    }
    notifier.fields = fieldsMap;
  }

  if (app.get('debug') > 1) {
    console.log('Built notifier alerts:', JSON.stringify(internals.notifierTypes, null, 2));
  }
}

// Initializes the parliament with ids for each group and cluster
// and sets up the parliament settings
function initializeParliament () {
  return new Promise((resolve, reject) => {
    if (parliament.version === undefined || parliament.version < MIN_PARLIAMENT_VERSION) {
      // notify of upgrade
      console.log(
        `WARNING - Current parliament version (${parliament.version ?? 1}) is less then required version (${MIN_PARLIAMENT_VERSION})
          Upgrading ${app.get('file')} file...\n`
      );

      // do the upgrade
      parliament = upgrade.upgrade(parliament, internals.notifierTypes);

      try { // write the upgraded file
        const upgradeParliamentError = validateParliament();
        if (!upgradeParliamentError) {
          fs.writeFileSync(app.get('file'), JSON.stringify(parliament, null, 2), 'utf8');
        }
      } catch (e) { // notify of error saving upgraded parliament and exit
        console.log('Error upgrading Parliament:\n\n', ArkimeUtil.sanitizeStr(e.stack));
        console.log(parliamentReadError);
        process.exit(1);
      }

      // notify of upgrade success
      console.log(`SUCCESS - Parliament upgraded to version ${MIN_PARLIAMENT_VERSION}`);
    }

    if (!parliament.groups) { parliament.groups = []; }

    // set id for each group/cluster
    for (const group of parliament.groups) {
      group.id = globalGroupId++;
      if (group.clusters) {
        for (const cluster of group.clusters) {
          cluster.id = globalClusterId++;
        }
      }
    }

    if (!parliament.settings) {
      parliament.settings = settingsDefault;
    }
    if (!parliament.settings.notifiers) {
      parliament.settings.notifiers = settingsDefault.notifiers;
    }
    if (!parliament.settings.general) {
      parliament.settings.general = settingsDefault.general;
    }
    if (!parliament.settings.general.outOfDate) {
      parliament.settings.general.outOfDate = settingsDefault.general.outOfDate;
    }
    if (!parliament.settings.general.noPackets) {
      parliament.settings.general.noPackets = settingsDefault.general.noPackets;
    }
    if (!parliament.settings.general.noPacketsLength) {
      parliament.settings.general.noPacketsLength = settingsDefault.general.noPacketsLength;
    }
    if (!parliament.settings.general.esQueryTimeout) {
      parliament.settings.general.esQueryTimeout = settingsDefault.general.esQueryTimeout;
    }
    if (!parliament.settings.general.removeIssuesAfter) {
      parliament.settings.general.removeIssuesAfter = settingsDefault.general.removeIssuesAfter;
    }
    if (!parliament.settings.general.removeAcknowledgedAfter) {
      parliament.settings.general.removeAcknowledgedAfter = settingsDefault.general.removeAcknowledgedAfter;
    }
    if (!parliament.settings.general.hostname) {
      parliament.settings.general.hostname = os.hostname();
    }

    if (app.get('debug')) {
      console.log('Parliament initialized!');
      console.log('Parliament groups:', JSON.stringify(parliament.groups, null, 2));
      console.log('Parliament general settings:', JSON.stringify(parliament.settings.general, null, 2));
    }

    buildNotifierTypes();

    const parliamentError = validateParliament();
    if (!parliamentError) {
      fs.writeFile(app.get('file'), JSON.stringify(parliament, null, 2), 'utf8',
        (err) => {
          if (err) {
            console.log('Parliament initialization error:', err.message ?? err);
            return reject(new Error('Parliament initialization error'));
          }

          return resolve();
        }
      );
    }
  });
}

// Chains all promises for requests for health and stats to update each cluster
// in the parliament
function updateParliament () {
  return new Promise((resolve, reject) => {
    const promises = [];
    for (const group of parliament.groups) {
      if (group.clusters) {
        for (const cluster of group.clusters) {
          // only get health for online clusters
          if (cluster.type !== 'disabled') {
            promises.push(getHealth(cluster));
          }
          // don't get stats for multiviewers or offline clusters
          if (cluster.type !== 'multiviewer' && cluster.type !== 'disabled') {
            promises.push(getStats(cluster));
          }
        }
      }
    }

    const issuesRemoved = cleanUpIssues();

    Promise.all(promises)
      .then(() => {
        if (issuesRemoved) { // save the issues that were removed
          const issuesError = validateIssues();
          if (!issuesError) {
            fs.writeFile(app.get('issuesfile'), JSON.stringify(issues, null, 2), 'utf8',
              (err) => {
                if (err) {
                  console.log('Unable to write issue:', err.message ?? err);
                }
              }
            );
          }
        }

        // save the data created after updating the parliament
        const parliamentError = validateParliament();
        if (!parliamentError) {
          fs.writeFile(app.get('file'), JSON.stringify(parliament, null, 2), 'utf8',
            (err) => {
              if (err) {
                console.log('Parliament update error:', err.message ?? err);
                return reject(new Error('Parliament update error'));
              }

              return resolve();
            });
        }

        if (app.get('debug')) {
          console.log('Parliament updated!');
          if (issuesRemoved) {
            console.log('Issues updated!');
          }
        }

        return resolve();
      })
      .catch((error) => {
        console.log('Parliament update error:', error.message ?? error);
        return resolve();
      });
  });
}

function cleanUpIssues () {
  let issuesRemoved = false;

  let len = issues.length;
  while (len--) {
    const issue = issues[len];
    const timeSinceLastNoticed = Date.now() - issue.lastNoticed || issue.firstNoticed;
    const removeIssuesAfter = getGeneralSetting('removeIssuesAfter') * 1000 * 60;
    const removeAcknowledgedAfter = getGeneralSetting('removeAcknowledgedAfter') * 1000 * 60;

    if (!issue.ignoreUntil) { // don't clean up any ignored issues, wait for the ignore to expire
      // remove issues that are provisional that haven't been seen since the last cycle
      if (issue.provisional && timeSinceLastNoticed >= 10000) {
        issuesRemoved = true;
        issues.splice(len, 1);
      }

      // remove all issues that have not been seen again for the removeIssuesAfter time, and
      // remove all acknowledged issues that have not been seen again for the removeAcknowledgedAfter time
      if ((!issue.acknowledged && timeSinceLastNoticed > removeIssuesAfter) ||
          (issue.acknowledged && timeSinceLastNoticed > removeAcknowledgedAfter)) {
        issuesRemoved = true;
        issues.splice(len, 1);
      }

      // if the issue was acknowledged but still persists, unacknowledge and alert again
      if (issue.acknowledged && (Date.now() - issue.acknowledged) > removeAcknowledgedAfter) {
        issue.alerted = undefined;
        issue.acknowledged = undefined;
      }
    }
  }

  return issuesRemoved;
}

function removeIssue (issueType, clusterId, nodeId) {
  let foundIssue = false;
  let len = issues.length;

  while (len--) {
    const issue = issues[len];
    if (issue.clusterId === parseInt(clusterId) &&
      issue.type === issueType &&
      issue.node === nodeId) {
      foundIssue = true;
      issues.splice(len, 1);
      if (issue.type === 'noPackets') {
        // also remove it from the no packets record
        delete noPacketsMap[issue.cluster + nodeId];
      }
    }
  }

  return foundIssue;
}

function getGeneralSetting (type) {
  let val = settingsDefault.general[type];
  if (parliament.settings && parliament.settings.general && parliament.settings.general[type]) {
    val = parliament.settings.general[type];
  }
  return val;
}

// Validates that the parliament object exists
// Use this before writing the parliament file
function validateParliament (next) {
  const len = Buffer.from(JSON.stringify(parliament, null, 2)).length;
  if (len < 320) {
    // if it's an empty file, don't save it, return an error
    const errorMsg = 'Error writing parliament data: empty or invalid parliament';
    console.log(errorMsg);
    if (next) {
      return newError(500, errorMsg);
    }
    return errorMsg;
  }
  return false;
}

// Writes the parliament to the parliament json file, updates the parliament
// with health and stats, then sends success or error
function writeParliament (req, res, next, successObj, errorText, sendParliament) {
  const parliamentError = validateParliament(next);
  if (parliamentError) {
    return next(parliamentError);
  }

  fs.writeFile(app.get('file'), JSON.stringify(parliament, null, 2), 'utf8',
    (err) => {
      if (app.get('debug')) {
        console.log('Wrote parliament file', err ?? '');
      }

      if (err) {
        const errorMsg = `Unable to write parliament data: ${err.message ?? err}`;
        console.log(errorMsg);
        return next(newError(500, errorMsg));
      }

      updateParliament()
        .then(() => {
          // send the updated parliament with the response
          if (sendParliament && successObj.parliament) {
            successObj.parliament = parliament;
          }
          return res.json(successObj);
        })
        .catch((err) => {
          return next(newError(500, errorText ?? 'Error updating parliament.'));
        });
    }
  );
}

// Validates that issues exist
// Use this before writing the issues file
function validateIssues (next) {
  const len = Buffer.from(JSON.stringify(issues, null, 2)).length;
  if (len < 2) {
    // if it's an empty file, don't save it, return an error
    const errorMsg = 'Error writing issue data: empty issues';
    console.log(errorMsg);
    if (next) {
      return newError(500, errorMsg);
    }
    return errorMsg;
  }
  return false;
}

// Writes the issues to the issues json file then sends success or error
function writeIssues (req, res, next, successObj, errorText, sendIssues) {
  const issuesError = validateIssues(next);
  if (issuesError) {
    return next(issuesError);
  }

  fs.writeFile(app.get('issuesfile'), JSON.stringify(issues, null, 2), 'utf8',
    (err) => {
      if (app.get('debug')) {
        console.log('Wrote issues file', err ?? '');
      }

      if (err) {
        const errorMsg = `Unable to write issue data: ${err.message ?? err}`;
        console.log(errorMsg);
        return next(newError(500, errorMsg));
      }

      // send the updated issues with the response
      if (sendIssues && successObj.issues) {
        successObj.issues = issues;
      }

      return res.json(successObj);
    }
  );
}

/* APIs -------------------------------------------------------------------- */
if (app.get('regressionTests')) {
  router.get('/regressionTests/makeToken', (req, res, next) => {
    req.user = {
      userId: req.query.molochRegressionUser ?? 'anonymous'
    };
    setCookie(req, res, next);
    return res.end();
  });
}

// Authenticate user
router.post('/auth', (req, res, next) => {
  if (app.get('dashboardOnly')) {
    return next(newError(403, 'Your Parliament is in dashboard only mode. You cannot login.'));
  }

  const hasAuth = !!app.get('password');
  if (!hasAuth) {
    return next(newError(401, 'No password set.'));
  }

  // check if password matches
  if (!bcrypt.compareSync(req.body.password, app.get('password'))) {
    return next(newError(401, 'Authentication failed.'));
  }

  const payload = { admin: true };

  const token = jwt.sign(payload, app.get('password'), {
    expiresIn: 60 * 60 * 24 // expires in 24 hours
  });

  res.json({ // return the information including token as JSON
    success: true,
    text: 'Here\'s your token!',
    token
  });
});

// logout a "session" by invalidating the token
router.post('/logout', (req, res, next) => {
  // check for token in header, url parameters, or post parameters
  const token = req.body.token ?? req.query.token ?? req.headers['x-access-token'];
  // add token to invalid token map
  if (token) { invalidTokens[token] = true; }

  return res.json({ loggedin: false });
});

// Get whether authentication or dashboardOnly mode is set
router.get('/auth', (req, res, next) => {
  const hasAuth = !!app.get('password') || !!parliament.settings.commonAuth;
  const dashboardOnly = !!app.get('dashboardOnly');
  return res.json({
    hasAuth,
    dashboardOnly
  });
});

// Get whether the user is logged in
// If it passes the verifyToken middleware, the user is logged in
router.get('/auth/loggedin', [isUser, setCookie], (req, res, next) => {
  return res.json({
    loggedin: true,
    commonAuth: !!parliament.authMode,
    isUser: !parliament.authMode ? true : req.user.hasRole('parliamentUser'),
    isAdmin: !parliament.authMode ? true : req.user.hasRole('parliamentAdmin')
  });
});

// Update (or create) common auth settings for the parliament
router.put('/auth/commonauth', [checkAuthUpdate], (req, res, next) => {
  if (app.get('dashboardOnly')) {
    return next(newError(403, 'Your Parliament is in dasboard only mode. You cannot setup auth.'));
  }

  if (!ArkimeUtil.isObject(req.body.commonAuth)) {
    return next(newError(422, 'Missing auth settings'));
  }

  // Go thru the secret fields and if the save still has ******** that means the user didn't change, so save what we have
  for (const s of ['passwordSecret', 'usersElasticsearchAPIKey', 'usersElasticsearchBasicAuth']) {
    if (req.body.commonAuth[s] === '********') {
      req.body.commonAuth[s] = parliament.settings.commonAuth[s];
    }
  }

  for (const s in req.body.commonAuth) {
    const setting = req.body.commonAuth[s];

    if (!ArkimeUtil.isString(setting)) {
      continue;
    }

    if (!parliament.settings.commonAuth) {
      parliament.settings.commonAuth = {};
    }

    parliament.settings.commonAuth[s] = setting;
  }

  const successObj = { success: true, text: 'Successfully updated your common auth settings.' };
  const errorText = 'Unable to update your common auth settings.';
  writeParliament(req, res, next, successObj, errorText);
});

// Update (or create) a password for the parliament
router.put('/auth/update', [checkAuthUpdate], (req, res, next) => {
  if (app.get('dashboardOnly')) {
    return next(newError(403, 'Your Parliament is in dasboard only mode. You cannot create a password.'));
  }

  if (!ArkimeUtil.isString(req.body.newPassword)) {
    return next(newError(422, 'You must provide a new password'));
  }

  const hasAuth = !!app.get('password');
  if (hasAuth) { // if the user has a password already set
    // check if the user has supplied their current password
    if (!ArkimeUtil.isString(req.body.currentPassword)) {
      return next(newError(401, 'You must provide your current password'));
    }
    // check if password matches
    if (!bcrypt.compareSync(req.body.currentPassword, app.get('password'))) {
      return next(newError(401, 'Authentication failed.'));
    }
  }

  bcrypt.hash(req.body.newPassword, saltrounds, (err, hash) => {
    if (err) {
      console.log(`Error hashing password: ${err}`);
      return next(newError(401, 'Hashing password failed.'));
    }

    app.set('password', hash);

    parliament.password = hash;

    const payload = { admin: true };

    const token = jwt.sign(payload, hash, {
      expiresIn: 60 * 60 * 24 // expires in 24 hours
    });

    // return the information including token as JSON
    const successObj = { success: true, text: 'Here\'s your new token!', token };
    const errorText = 'Unable to update your password.';
    writeParliament(req, res, next, successObj, errorText);
  });
});

router.get('/notifierTypes', [isAdmin, setCookie], (req, res) => {
  return res.json(internals.notifierTypes ?? {});
});

// Get the parliament settings object
router.get('/settings', [isAdmin, setCookie], (req, res, next) => {
  if (!parliament.settings) {
    return next(newError(500, 'Your settings are empty. Try restarting Parliament.'));
  }

  const settings = JSON.parse(JSON.stringify(parliament.settings));

  if (!settings.general) {
    settings.general = settingsDefault.general;
  }

  if (!settings.commonAuth) {
    settings.commonAuth = {};
  }

  // Hide the secrets
  for (const s of ['passwordSecret', 'usersElasticsearchAPIKey', 'usersElasticsearchBasicAuth']) {
    if (settings.commonAuth[s] !== undefined) {
      settings.commonAuth[s] = '********';
    }
  }

  return res.json(settings);
});

// Update the parliament general settings object
router.put('/settings', [isAdmin, checkCookieToken], (req, res, next) => {
  // save general settings
  for (const s in req.body.settings.general) {
    let setting = req.body.settings.general[s];

    if (s !== 'hostname' && s !== 'includeUrl') {
      if (isNaN(setting)) {
        return next(newError(422, `${s} must be a number.`));
      } else {
        setting = parseInt(setting);
      }
    }

    parliament.settings.general[s] = setting;
  }

  const successObj = { success: true, text: 'Successfully updated your settings.' };
  const errorText = 'Unable to update your settings.';
  writeParliament(req, res, next, successObj, errorText);
});

function verifyNotifierReqBody (req) {
  if (typeof req.body.notifier !== 'object') {
    return 'Missing notifier';
  }

  if (!ArkimeUtil.isString(req.body.notifier.name)) {
    return 'Missing notifier name';
  }

  if (!ArkimeUtil.isString(req.body.notifier.type)) {
    return 'Missing notifier type';
  }

  if (typeof req.body.notifier.fields !== 'object') {
    return 'Missing notifier fields';
  }

  if (typeof req.body.notifier.alerts !== 'object') {
    return 'Missing notifier alerts';
  }

  return undefined;
}

// Update an existing notifier
router.put('/notifiers/:name', [isAdmin, checkCookieToken], (req, res, next) => {
  if (req.params.name === '__proto__') {
    return next(newError(404, 'Bad name'));
  }

  if (!parliament.settings.notifiers[req.params.name]) {
    return next(newError(404, `${req.params.name} not found.`));
  }

  if (!ArkimeUtil.isString(req.body.key)) {
    return next(newError(422, 'Missing notifier key'));
  }

  const verifyMsg = verifyNotifierReqBody(req);
  if (verifyMsg) { return next(newError(422, verifyMsg)); }

  req.body.notifier.name = req.body.notifier.name.replace(/[^-a-zA-Z0-9_: ]/g, '');

  if (req.body.notifier.name !== req.body.key &&
    parliament.settings.notifiers[req.body.notifier.name]) {
    return next(newError(403, `${req.body.notifier.name} already exists. Notifier names must be unique`));
  }

  let foundNotifier;
  for (const n in internals.notifierTypes) {
    const notifier = internals.notifierTypes[n];
    if (notifier.type === req.body.notifier.type) {
      foundNotifier = notifier;
    }
  }

  if (!foundNotifier) {
    return next(newError(403, 'Unknown notifier type'));
  }

  // check that required notifier fields exist
  for (const f in foundNotifier.fields) {
    const field = foundNotifier.fields[f];
    for (const sf in req.body.notifier.fields) {
      const sentField = req.body.notifier.fields[sf];
      if (sentField.name === field.name && field.required && !sentField.value) {
        return next(newError(403, `Missing a value for ${field.name}`));
      }
    }
  }

  parliament.settings.notifiers[req.body.notifier.name] = req.body.notifier;
  // delete the old one if the key (notifier name) has changed
  if (parliament.settings.notifiers[req.body.key] &&
    req.body.notifier.name !== req.body.key) {
    parliament.settings.notifiers[req.body.key] = null;
    delete parliament.settings.notifiers[req.body.key];
  }

  const successObj = {
    success: true,
    newKey: req.body.notifier.name,
    text: `Successfully updated ${req.params.name} notifier.`
  };
  const errorText = `Cannot update ${req.params.name} notifier`;
  writeParliament(req, res, next, successObj, errorText);
});

// Remove a notifier
router.delete('/notifiers/:name', [isAdmin, checkCookieToken], (req, res, next) => {
  if (!parliament.settings.notifiers[req.params.name]) {
    return next(newError(403, `Cannot find ${req.params.name} notifier to remove`));
  }

  parliament.settings.notifiers[req.params.name] = undefined;

  const successObj = { success: true, text: `Successfully removed ${req.params.name} notifier.` };
  const errorText = `Cannot remove ${req.params.name} notifier`;
  writeParliament(req, res, next, successObj, errorText);
});

// Create a new notifier
router.post('/notifiers', [isAdmin, checkCookieToken], (req, res, next) => {
  const verifyMsg = verifyNotifierReqBody(req);
  if (verifyMsg) { return next(newError(422, verifyMsg)); }

  req.body.notifier.name = req.body.notifier.name.replace(/[^-a-zA-Z0-9_: ]/g, '');

  if (parliament.settings.notifiers[req.body.notifier.name]) {
    return next(newError(403, `${req.body.notifier.name} already exists. Notifier names must be unique`));
  }

  let foundNotifier;
  for (const n in internals.notifierTypes) {
    const notifier = internals.notifierTypes[n];
    if (notifier.type === req.body.notifier.type) {
      foundNotifier = notifier;
    }
  }

  if (!foundNotifier) {
    return next(newError(403, 'Unknown notifier type'));
  }

  // check that required notifier fields exist
  for (const f in foundNotifier.fields) {
    const field = foundNotifier.fields[f];
    for (const sf in req.body.notifier.fields) {
      const sentField = req.body.notifier.fields[sf];
      if (sentField.name === field.name && field.required && !sentField.value) {
        return next(newError(403, `Missing a value for ${field.name}`));
      }
    }
  }

  parliament.settings.notifiers[req.body.notifier.name] = req.body.notifier;

  const successObj = {
    success: true,
    name: req.body.notifier.name,
    text: `Successfully added ${req.body.notifier.name} notifier.`
  };
  const errorText = `Unable to add ${req.body.notifier.name} notifier.`;
  writeParliament(req, res, next, successObj, errorText);
});

// Update the parliament general settings object to the defaults
router.put('/settings/restoreDefaults', [isAdmin, checkCookieToken], (req, res, next) => {
  let type = 'all'; // default
  if (ArkimeUtil.isString(req.body.type)) {
    type = req.body.type;
  }

  if (type === 'general') {
    parliament.settings.general = JSON.parse(JSON.stringify(settingsDefault.general));
  } else if (type === 'all') {
    parliament.settings = JSON.parse(JSON.stringify(settingsDefault));
  } else {
    return next(newError(500, 'type must be general or all'));
  }

  const settings = JSON.parse(JSON.stringify(parliament.settings));

  const parliamentError = validateParliament(next);
  if (!parliamentError) {
    return next(parliamentError);
  }

  fs.writeFile(app.get('file'), JSON.stringify(parliament, null, 2), 'utf8',
    (err) => {
      if (err) {
        const errorMsg = `Unable to write parliament data: ${err.message ?? err}`;
        console.log(errorMsg);
        return next(newError(500, errorMsg));
      }

      return res.json({
        settings,
        text: `Successfully restored ${req.body.type} default settings.`
      });
    }
  );
});

// Get parliament with stats
router.get('/parliament', (req, res, next) => {
  const parliamentClone = JSON.parse(JSON.stringify(parliament));

  for (const group of parliamentClone.groups) {
    for (const cluster of group.clusters) {
      cluster.activeIssues = [];
      for (const issue of issues) {
        if (issue.clusterId === cluster.id &&
          !issue.acknowledged && !issue.ignoreUntil &&
          !issue.provisional) {
          cluster.activeIssues.push(issue);
        }
      }
    }
  }

  delete parliamentClone.settings;
  delete parliamentClone.password;

  return res.json(parliamentClone);
});

// Updates the parliament order of clusters and groups
router.put('/parliament', [isAdmin, checkCookieToken], (req, res, next) => {
  if (typeof req.body.reorderedParliament !== 'object') {
    return next(newError(422, 'You must provide the new parliament order'));
  }

  // remove any client only stuff
  for (const group of req.body.reorderedParliament.groups) {
    group.filteredClusters = undefined;
    for (const cluster of group.clusters) {
      cluster.issues = undefined;
      cluster.activeIssues = undefined;
    }
  }

  parliament.groups = req.body.reorderedParliament.groups;
  updateParliament();

  const successObj = { success: true, text: 'Successfully reordered items in your parliament.' };
  const errorText = 'Unable to update the order of items in your parliament.';
  writeParliament(req, res, next, successObj, errorText);
});

// Create a new group in the parliament
router.post('/groups', [isAdmin, checkCookieToken], (req, res, next) => {
  if (!ArkimeUtil.isString(req.body.title)) {
    return next(newError(422, 'A group must have a title'));
  }

  if (req.body.description && !ArkimeUtil.isString(req.body.description)) {
    return next(newError(422, 'A group must have a string description.'));
  }

  const newGroup = { title: req.body.title, id: globalGroupId++, clusters: [] };
  if (req.body.description) { newGroup.description = req.body.description; }

  parliament.groups.push(newGroup);

  const successObj = { success: true, group: newGroup, text: 'Successfully added new group.' };
  const errorText = 'Unable to add that group to your parliament.';
  writeParliament(req, res, next, successObj, errorText);
});

// Delete a group in the parliament
router.delete('/groups/:id', [isAdmin, checkCookieToken], (req, res, next) => {
  let index = 0;
  let foundGroup = false;
  for (const group of parliament.groups) {
    if (group.id === parseInt(req.params.id)) {
      parliament.groups.splice(index, 1);
      foundGroup = true;
      break;
    }
    ++index;
  }

  if (!foundGroup) {
    return next(newError(500, 'Unable to find group to delete.'));
  }

  const successObj = { success: true, text: 'Successfully removed the requested group.' };
  const errorText = 'Unable to remove that group from the parliament.';
  writeParliament(req, res, next, successObj, errorText);
});

// Update a group in the parliament
router.put('/groups/:id', [isAdmin, checkCookieToken], (req, res, next) => {
  if (!ArkimeUtil.isString(req.body.title)) {
    return next(newError(422, 'A group must have a title.'));
  }

  if (req.body.description && !ArkimeUtil.isString(req.body.description)) {
    return next(newError(422, 'A group must have a string description.'));
  }

  let foundGroup = false;
  for (const group of parliament.groups) {
    if (group.id === parseInt(req.params.id)) {
      group.title = req.body.title;
      group.description = req.body.description;
      foundGroup = true;
      break;
    }
  }

  if (!foundGroup) {
    return next(newError(500, 'Unable to find group to edit.'));
  }

  const successObj = { success: true, text: 'Successfully updated the requested group.' };
  const errorText = 'Unable to update that group in the parliament.';
  writeParliament(req, res, next, successObj, errorText);
});

// Create a new cluster within an existing group
router.post('/groups/:id/clusters', [isAdmin, checkCookieToken], (req, res, next) => {
  if (!ArkimeUtil.isString(req.body.title)) {
    return next(newError(422, 'A cluster must have a title.'));
  }

  if (!ArkimeUtil.isString(req.body.url)) {
    return next(newError(422, 'A cluster must have a url.'));
  }

  if (req.body.description && !ArkimeUtil.isString(req.body.description)) {
    return next(newError(422, 'A cluster must have a string description.'));
  }

  if (req.body.localUrl && !ArkimeUtil.isString(req.body.localUrl)) {
    return next(newError(422, 'A cluster must have a string localUrl.'));
  }

  if (req.body.type && !ArkimeUtil.isString(req.body.type)) {
    return next(newError(422, 'A cluster must have a string type.'));
  }

  const newCluster = {
    title: req.body.title,
    description: req.body.description,
    url: req.body.url,
    localUrl: req.body.localUrl,
    id: globalClusterId++,
    type: req.body.type
  };

  let foundGroup = false;
  for (const group of parliament.groups) {
    if (group.id === parseInt(req.params.id)) {
      group.clusters.push(newCluster);
      foundGroup = true;
      break;
    }
  }

  if (!foundGroup) {
    return next(newError(500, 'Unable to find group to place cluster.'));
  }

  const successObj = {
    success: true,
    cluster: newCluster,
    text: 'Successfully added the requested cluster.'
  };
  const errorText = 'Unable to add that cluster to the parliament.';
  writeParliament(req, res, next, successObj, errorText, true);
});

// Delete a cluster
router.delete('/groups/:groupId/clusters/:clusterId', [isAdmin, checkCookieToken], (req, res, next) => {
  let clusterIndex = 0;
  let foundCluster = false;
  for (const group of parliament.groups) {
    if (group.id === parseInt(req.params.groupId)) {
      for (const cluster of group.clusters) {
        if (cluster.id === parseInt(req.params.clusterId)) {
          group.clusters.splice(clusterIndex, 1);
          foundCluster = true;
          break;
        }
        ++clusterIndex;
      }
    }
  }

  if (!foundCluster) {
    return next(newError(500, 'Unable to find cluster to delete.'));
  }

  const successObj = { success: true, text: 'Successfully removed the requested cluster.' };
  const errorText = 'Unable to remove that cluster from your parliament.';
  writeParliament(req, res, next, successObj, errorText);
});

// Update a cluster
router.put('/groups/:groupId/clusters/:clusterId', [isAdmin, checkCookieToken], (req, res, next) => {
  if (!ArkimeUtil.isString(req.body.title)) {
    return next(newError(422, 'A cluster must have a title.'));
  }

  if (!ArkimeUtil.isString(req.body.url)) {
    return next(newError(422, 'A cluster must have a url.'));
  }

  if (req.body.description && !ArkimeUtil.isString(req.body.description)) {
    return next(newError(422, 'A cluster must have a string description.'));
  }

  if (req.body.localUrl && !ArkimeUtil.isString(req.body.localUrl)) {
    return next(newError(422, 'A cluster must have a string localUrl.'));
  }

  if (req.body.type && !ArkimeUtil.isString(req.body.type)) {
    return next(newError(422, 'A cluster must have a string type.'));
  }

  let foundCluster = false;
  for (const group of parliament.groups) {
    if (group.id === parseInt(req.params.groupId)) {
      for (const cluster of group.clusters) {
        if (cluster.id === parseInt(req.params.clusterId)) {
          cluster.url = req.body.url;
          cluster.title = req.body.title;
          cluster.localUrl = req.body.localUrl;
          cluster.description = req.body.description;
          cluster.hideDeltaBPS = typeof req.body.hideDeltaBPS === 'boolean' ? req.body.hideDeltaBPS : undefined;
          cluster.hideDataNodes = typeof req.body.hideDataNodes === 'boolean' ? req.body.hideDataNodes : undefined;
          cluster.hideDeltaTDPS = typeof req.body.hideDeltaTDPS === 'boolean' ? req.body.hideDeltaTDPS : undefined;
          cluster.hideTotalNodes = typeof req.body.hideTotalNodes === 'boolean' ? req.body.hideTotalNodes : undefined;
          cluster.hideMonitoring = typeof req.body.hideMonitoring === 'boolean' ? req.body.hideMonitoring : undefined;
          cluster.hideMolochNodes = typeof req.body.hideMolochNodes === 'boolean' ? req.body.hideMolochNodes : undefined;
          cluster.type = req.body.type;

          foundCluster = true;
          break;
        }
      }
    }
  }

  if (!foundCluster) {
    return next(newError(500, 'Unable to find cluster to update.'));
  }

  const successObj = { success: true, text: 'Successfully updated the requested cluster.' };
  const errorText = 'Unable to update that cluster in your parliament.';
  writeParliament(req, res, next, successObj, errorText);
});

// Get a list of issues
router.get('/issues', (req, res, next) => {
  let issuesClone = JSON.parse(JSON.stringify(issues));

  issuesClone = issuesClone.filter((issue) => {
    // always filter out provisional issues
    if (issue.provisional) { return false; }

    // filter ack'd issues
    if (req.query.hideAckd && req.query.hideAckd === 'true' && issue.acknowledged) {
      return false;
    }

    // filter ignored issues
    if (req.query.hideIgnored && req.query.hideIgnored === 'true' && issue.ignoreUntil) {
      return false;
    }

    // filter issues by type
    if (req.query.hideEsRed && req.query.hideEsRed === 'true' && issue.type === 'esRed') {
      return false;
    }
    if (req.query.hideEsDown && req.query.hideEsDown === 'true' && issue.type === 'esDown') {
      return false;
    }
    if (req.query.hideEsDropped && req.query.hideEsDropped === 'true' && issue.type === 'esDropped') {
      return false;
    }
    if (req.query.hideOutOfDate && req.query.hideOutOfDate === 'true' && issue.type === 'outOfDate') {
      return false;
    }
    if (req.query.hideNoPackets && req.query.hideNoPackets === 'true' && issue.type === 'noPackets') {
      return false;
    }

    // filter by search term
    if (req.query.filter) {
      const searchTerm = req.query.filter.toLowerCase();
      return issue.severity.toLowerCase().includes(searchTerm) ||
          (issue.node && issue.node.toLowerCase().includes(searchTerm)) ||
          issue.cluster.toLowerCase().includes(searchTerm) ||
          issue.message.toLowerCase().includes(searchTerm) ||
          issue.title.toLowerCase().includes(searchTerm) ||
          issue.text.toLowerCase().includes(searchTerm);
    }

    // we got past all the filters! include this issue in the results
    return true;
  });

  let type = 'string';
  const sortBy = req.query.sort;
  if (sortBy === 'ignoreUntil' ||
    sortBy === 'firstNoticed' ||
    sortBy === 'lastNoticed' ||
    sortBy === 'acknowledged') {
    type = 'number';
  }

  if (sortBy) {
    const order = req.query.order ?? 'desc';
    issuesClone.sort((a, b) => {
      if (type === 'number') {
        let aVal = 0;
        let bVal = 0;

        if (b[sortBy] !== undefined) { bVal = b[sortBy]; }
        if (a[sortBy] !== undefined) { aVal = a[sortBy]; }

        return order === 'asc' ? bVal - aVal : aVal - bVal;
      } else { // assume it's a string
        let aVal = '';
        let bVal = '';

        if (b[sortBy] !== undefined) { bVal = b[sortBy]; }
        if (a[sortBy] !== undefined) { aVal = a[sortBy]; }

        return order === 'asc' ? bVal.localeCompare(aVal) : aVal.localeCompare(bVal);
      }
    });
  }

  const recordsFiltered = issuesClone.length;

  if (req.query.length) { // paging
    const len = parseInt(req.query.length);
    const start = !req.query.start ? 0 : parseInt(req.query.start);

    issuesClone = issuesClone.slice(start, len + start);
  }

  return res.json({
    issues: issuesClone,
    recordsFiltered
  });
});

// acknowledge one or more issues
router.put('/acknowledgeIssues', [isUser, checkCookieToken], (req, res, next) => {
  if (!Array.isArray(req.body.issues) || !req.body.issues.length) {
    const message = 'Must specify the issue(s) to acknowledge.';
    return next(newError(422, message));
  }

  const now = Date.now();
  let count = 0;

  for (const i of req.body.issues) {
    const issue = findIssue(parseInt(i.clusterId), i.type, i.node);
    if (issue) {
      issue.acknowledged = now;
      count++;
    }
  }

  let errorText;
  if (!count) {
    errorText = 'Unable to acknowledge requested issue';
    if (req.body.issues.length > 1) { errorText += 's'; }
    return next(newError(500, errorText));
  }

  let successText = `Successfully acknowledged ${count} requested issue`;
  errorText = 'Unable to acknowledge the requested issue';
  if (count > 1) {
    successText += 's';
    errorText += 's';
  }

  const successObj = { success: true, text: successText, acknowledged: now };
  writeIssues(req, res, next, successObj, errorText);
});

// ignore one or more issues
router.put('/ignoreIssues', [isUser, checkCookieToken], (req, res, next) => {
  if (!Array.isArray(req.body.issues) || !req.body.issues.length) {
    const message = 'Must specify the issue(s) to ignore.';
    return next(newError(422, message));
  }

  const ms = req.body.ms ?? 3600000; // Default to 1 hour
  let ignoreUntil = Date.now() + ms;
  if (ms === -1) { ignoreUntil = -1; } // -1 means ignore it forever

  let count = 0;

  for (const i of req.body.issues) {
    const issue = findIssue(parseInt(i.clusterId), i.type, i.node);
    if (issue) {
      issue.ignoreUntil = ignoreUntil;
      count++;
    }
  }

  let errorText;
  if (!count) {
    errorText = 'Unable to ignore requested issue';
    if (req.body.issues.length > 1) { errorText += 's'; }
    return next(newError(500, errorText));
  }

  let successText = `Successfully ignored ${count} requested issue`;
  errorText = 'Unable to ignore the requested issue';
  if (count > 1) {
    successText += 's';
    errorText += 's';
  }

  const successObj = { success: true, text: successText, ignoreUntil };
  writeIssues(req, res, next, successObj, errorText);
});

// unignore one or more issues
router.put('/removeIgnoreIssues', [isUser, checkCookieToken], (req, res, next) => {
  if (!Array.isArray(req.body.issues) || !req.body.issues.length) {
    const message = 'Must specify the issue(s) to unignore.';
    return next(newError(422, message));
  }

  let count = 0;

  for (const i of req.body.issues) {
    const issue = findIssue(parseInt(i.clusterId), i.type, i.node);
    if (issue) {
      issue.ignoreUntil = undefined;
      issue.alerted = undefined; // reset alert time so it can alert again
      count++;
    }
  }

  let errorText;
  if (!count) {
    errorText = 'Unable to unignore requested issue';
    if (req.body.issues.length > 1) { errorText += 's'; }
    return next(newError(500, errorText));
  }

  let successText = `Successfully unignored ${count} requested issue`;
  errorText = 'Unable to unignore the requested issue';
  if (count > 1) {
    successText += 's';
    errorText += 's';
  }

  const successObj = { success: true, text: successText };
  writeIssues(req, res, next, successObj, errorText);
});

// Remove an issue with a cluster
router.put('/groups/:groupId/clusters/:clusterId/removeIssue', [isUser, checkCookieToken], (req, res, next) => {
  if (!ArkimeUtil.isString(req.body.type)) {
    const message = 'Must specify the issue type to remove.';
    return next(newError(422, message));
  }

  const foundIssue = removeIssue(req.body.type, req.params.clusterId, req.body.node);

  if (!foundIssue) {
    return next(newError(500, 'Unable to find issue to remove. Maybe it was already removed.'));
  }

  const successObj = { success: true, text: 'Successfully removed the requested issue.' };
  const errorText = 'Unable to remove that issue.';
  writeIssues(req, res, next, successObj, errorText);
});

// Remove all acknowledged all issues
router.put('/issues/removeAllAcknowledgedIssues', [isUser, checkCookieToken], (req, res, next) => {
  let count = 0;

  let len = issues.length;
  while (len--) {
    const issue = issues[len];
    if (issue.acknowledged) {
      count++;
      issues.splice(len, 1);
    }
  }

  if (!count) {
    return next(newError(400, 'There are no acknowledged issues to remove.'));
  }

  const successObj = { success: true, text: `Successfully removed ${count} acknowledged issues.` };
  const errorText = 'Unable to remove acknowledged issues.';
  writeIssues(req, res, next, successObj, errorText, true);
});

// remove one or more acknowledged issues
router.put('/removeSelectedAcknowledgedIssues', [isUser, checkCookieToken], (req, res, next) => {
  if (!Array.isArray(req.body.issues) || !req.body.issues.length) {
    const message = 'Must specify the acknowledged issue(s) to remove.';
    return next(newError(422, message));
  }

  let count = 0;

  // mark issues to remove
  for (const i of req.body.issues) {
    const issue = findIssue(parseInt(i.clusterId), i.type, i.node);
    if (issue && issue.acknowledged) {
      count++;
      issue.remove = true;
    }
  }

  if (!count) {
    return next(newError(400, 'There are no acknowledged issues to remove.'));
  }

  count = 0;
  let len = issues.length;
  while (len--) {
    const issue = issues[len];
    if (issue.remove) {
      count++;
      issues.splice(len, 1);
    }
  }

  let errorText;
  if (!count) {
    errorText = 'Unable to remove requested issue';
    if (req.body.issues.length > 1) { errorText += 's'; }
    return next(newError(500, errorText));
  }

  let successText = `Successfully removed ${count} requested issue`;
  errorText = 'Unable to remove the requested issue';
  if (count > 1) {
    successText += 's';
    errorText += 's';
  }

  const successObj = { success: true, text: successText };
  writeIssues(req, res, next, successObj, errorText);
});

// issue a test alert to a specified notifier
router.post('/testAlert', [isAdmin, checkCookieToken], (req, res, next) => {
  if (!ArkimeUtil.isString(req.body.notifier)) {
    return next(newError(422, 'Must specify the notifier.'));
  }

  const notifier = parliament.settings.notifiers[req.body.notifier];

  if (!notifier) {
    const errorText = 'Unable to find the requested notifier';
    return next(newError(500, errorText));
  }

  const config = {};

  for (const f in notifier.fields) {
    const field = notifier.fields[f];
    if (!field || (field.required && !field.value)) {
      // field doesn't exist, or field is required and doesn't have a value
      const message = `Missing the ${f} field for the ${notifier.name} notifier. Add it on the settings page.`;
      console.log(message);

      return next(newError(422, message));
    }
    config[f] = field.value;
  }

  internals.notifierTypes[notifier.type].sendAlert(
    config,
    `Test alert from the ${notifier.name} notifier!`,
    null,
    (response) => {
      // there should only be one error here because only one
      // notifier alert is sent at a time
      if (response.errors) {
        // eslint-disable-next-line no-unreachable-loop
        for (const e in response.errors) {
          return next(newError(500, response.errors[e]));
        }
      }

      return res.json({
        success: true,
        text: `Successfully issued alert using the ${notifier.name} notifier.`
      });
    }
  );
});

function setupAuth () {
  parliament.authMode = false;

  if (!parliament?.settings?.commonAuth?.userNameHeader) { return; }

  const commonAuth = parliament.settings.commonAuth;

  parliament.authMode = commonAuth.userNameHeader === 'digest' ? 'digest' : 'header';

  Auth.initialize({
    debug: app.get('debug'),
    mode: parliament.authMode,
    userNameHeader: parliament.authMode === 'digest' ? undefined : commonAuth.userNameHeader,
    passwordSecret: commonAuth.passwordSecret ?? 'password',
    passwordSecretSection: 'parliament',
    userAuthIps: undefined,
    basePath: '/parliament/api/',
    authConfig: {
      httpRealm: commonAuth.httpRealm ?? 'Moloch',
      userIdField: commonAuth.authUserIdField,
      discoverURL: commonAuth.authDiscoverURL,
      clientId: commonAuth.authClientId,
      clientSecret: commonAuth.authClientSecret,
      redirectURIs: commonAuth.authRedirectURIs,
      trustProxy: commonAuth.authTrustProxy
    }
  });

  User.initialize({
    insecure: internals.insecure,
    node: commonAuth.usersElasticsearch ?? 'http://localhost:9200',
    prefix: commonAuth.usersPrefix,
    apiKey: commonAuth.usersElasticsearchAPIKey,
    basicAuth: commonAuth.usersElasticsearchBasicAuth
  });
}

/* SIGNALS! ----------------------------------------------------------------- */
// Explicit sigint handler for running under docker
// See https://github.com/nodejs/node/issues/4182
process.on('SIGINT', function () {
  process.exit();
});

/* LISTEN! ----------------------------------------------------------------- */
// using fallthrough: false because there is no 404 endpoint (client router
// handles 404s) and sending index.html is confusing
// expose vue bundles (prod)
app.use(['/static', '/parliament/static'], express.static(
  path.join(__dirname, '/vueapp/dist/static'),
  { maxAge: dayMs, fallthrough: false }
), ArkimeUtil.missingResource);
// expose vue bundle (dev)
app.use(['/app.js', '/parliament/app.js'], express.static(
  path.join(__dirname, '/vueapp/dist/app.js'),
  { fallthrough: false }
), ArkimeUtil.missingResource);
app.use(['/app.js.map', '/parliament/app.js.map'], express.static(
  path.join(__dirname, '/vueapp/dist/app.js.map'),
  { fallthrough: false }
), ArkimeUtil.missingResource);

// vue index page
const Vue = require('vue');
const vueServerRenderer = require('vue-server-renderer');

// Factory function to create fresh Vue apps
function createApp () {
  return new Vue({
    template: '<div id="app"></div>'
  });
}

app.use((req, res, next) => {
  const renderer = vueServerRenderer.createRenderer({
    template: fs.readFileSync(path.join(__dirname, '/vueapp/dist/index.html'), 'utf-8')
  });

  const appContext = {
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

let server;
if (app.get('keyFile') && app.get('certFile')) {
  const certOptions = {
    key: fs.readFileSync(app.get('keyFile')),
    cert: fs.readFileSync(app.get('certFile'))
  };
  server = https.createServer(certOptions, app);
} else {
  server = http.createServer(app);
}

setupAuth();

server
  .on('error', function (e) {
    console.log(`ERROR - couldn't listen on port ${app.get('port')}, is Parliament already running?`);
    process.exit(1);
  })
  .on('listening', function (e) {
    console.log(`Express server listening on port ${server.address().port} in ${app.settings.env} mode`);
    if (app.get('debug')) {
      console.log('Debug Level', app.get('debug'));
      console.log('Parliament file:', app.get('file'));
      console.log('Issues file:', issuesFilename);
      if (app.get('dashboardOnly')) {
        console.log('Opening in dashboard only mode');
      }
    }
  })
  .listen(app.get('port'), () => {
    initializeParliament()
      .then(() => {
        updateParliament();
      })
      .catch((err) => {
        console.log('ERROR - never mind, couldn\'t initialize Parliament\n', err);
        process.exit(1);
      });

    setInterval(() => {
      updateParliament();
      processAlerts();
    }, 10000);
  });
