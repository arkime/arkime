#!/usr/bin/env node
'use strict';

const MIN_PARLIAMENT_VERSION = 3;

/* dependencies ------------------------------------------------------------- */
const express = require('express');
const http    = require('http');
const https   = require('https');
const fs      = require('fs');
const favicon = require('serve-favicon');
const rp      = require('request-promise');
const bp      = require('body-parser');
const logger  = require('morgan');
const jwt     = require('jsonwebtoken');
const bcrypt  = require('bcrypt');
const glob    = require('glob');
const os      = require('os');
const upgrade = require('./upgrade');

/* app setup --------------------------------------------------------------- */
const app     = express();
const router  = express.Router();

const saltrounds = 13;

const issueTypes = {
  esRed: { on: true, name: 'ES Red', text: 'ES is red', severity: 'red', description: 'ES status is red' },
  esDown: { on: true, name: 'ES Down', text: ' ES is down', severity: 'red', description: 'ES is unreachable' },
  esDropped: { on: true, name: 'ES Dropped', text: 'ES is dropping bulk inserts', severity: 'yellow', description: 'the capture node is overloading ES' },
  outOfDate: { on: true, name: 'Out of Date', text: 'has not checked in since', severity: 'red', description: 'the capture node has not checked in' },
  noPackets: { on: true, name: 'Low Packets', text: 'is not receiving many packets', severity: 'red', description: 'the capture node is not receiving many packets' }
};

const settingsDefault = {
  general : {
    noPackets: 0,
    noPacketsLength: 10,
    outOfDate: 30,
    esQueryTimeout: 5,
    removeIssuesAfter: 60,
    removeAcknowledgedAfter: 15
  },
  notifiers: {}
};

const parliamentReadError = `\nYou must fix this before you can run Parliament.
  Try using parliament.example.json as a starting point`;

(function () { // parse arguments
  let appArgs = process.argv.slice(2);
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

  if (!appArgs.length) {
    console.log('WARNING: No config options were set, starting Parliament in view only mode with defaults.\n');
  }

  app.set('debug', debug);

  // set optional config options that reqiure defaults
  app.set('port', port || 8008);
  app.set('file', file || './parliament.json');
}());

if (app.get('regressionTests')) {
  app.post('/shutdown', function (req, res) {
    process.exit(0);
  });
}

// parliament object!
let parliament;

try { // check if the file exists
  fs.accessSync(app.get('file'), fs.constants.F_OK);
} catch (e) { // if the file doesn't exist, create it
  try { // write the new file
    parliament = { version: MIN_PARLIAMENT_VERSION };
    fs.writeFileSync(app.get('file'), JSON.stringify(parliament, null, 2), 'utf8');
  } catch (e) { // notify of error saving new parliament and exit
    console.log(`Error creating new Parliament:\n\n`, e.stack);
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
    app.set('password', parliament.password);
  }
} catch (e) {
  console.log(`Error reading ${app.get('file') || 'your parliament file'}:\n\n`, e.stack);
  console.log(parliamentReadError);
  process.exit(1);
}

// construct the issues file name
let issuesFilename = 'issues.json';
if (app.get('file').indexOf('.json') > -1) {
  let name = app.get('file').replace(/\.json/g, '');
  issuesFilename = `${name}.issues.json`;
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
let groupId = 0;
let clusterId = 0;

// save noPackets issues so that the time of issue can be compared to the
// noPacketsLength user setting (only issue alerts when the time the issue
// was encounterd exceeds the noPacketsLength user setting)
let noPacketsMap = {};

// super secret
app.disable('x-powered-by');

// expose vue bundles (prod)
app.use('/parliament/static', express.static(`${__dirname}/vueapp/dist/static`));
// expose vue bundle (dev)
app.use(['/app.js', '/vueapp/app.js'], express.static(`${__dirname}/vueapp/dist/app.js`));

app.use('/parliament/font-awesome', express.static(`${__dirname}/../node_modules/font-awesome`, { maxAge: 600 * 1000 }));

// log requests
app.use(logger(':date \x1b[1m:method\x1b[0m \x1b[33m:url\x1b[0m :status :res[content-length] bytes :response-time ms', { stream: process.stdout }));

app.use(favicon(`${__dirname}/favicon.ico`));

// define router to mount api related functions
app.use('/parliament/api', router);
router.use(bp.json());
router.use(bp.urlencoded({ extended: true }));

let internals = {
  notifierTypes: {}
};

// Load notifier plugins for Parliament alerting
function loadNotifiers () {
  let api = {
    register: function (str, info) {
      internals.notifierTypes[str] = info;
    }
  };

  // look for all notifier providers and initialize them
  let files = glob.sync(`${__dirname}/../notifiers/provider.*.js`);
  files.forEach((file) => {
    let plugin = require(file);
    plugin.init(api);
  });
}

loadNotifiers();

/* Middleware -------------------------------------------------------------- */
// App should always have parliament data
router.use((req, res, next) => {
  if (!parliament) {
    const error = new Error('Unable to fetch parliament data.');
    error.httpStatusCode = 500;
    return next(error);
  }

  next();
});

// Handle errors
app.use((err, req, res, next) => {
  console.log(err.stack);
  res.status(err.httpStatusCode || 500).json({
    success : false,
    text    : err.message || 'Error'
  });
});

// Verify token
function verifyToken (req, res, next) {
  function tokenError (req, res, errorText) {
    errorText = errorText || 'Token Error!';
    res.status(403).json({
      tokenError: true,
      success   : false,
      text      : `Permission Denied: ${errorText}`
    });
  }

  let hasAuth = !!app.get('password');
  if (!hasAuth) {
    return tokenError(req, res, 'No password set.');
  }

  // check for token in header, url parameters, or post parameters
  let token = req.body.token || req.query.token || req.headers['x-access-token'];

  if (!token) {
    return tokenError(req, res, 'No token provided.');
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

/* Helper functions -------------------------------------------------------- */
// list of alerts that will be sent at every 10 seconds
let alerts = [];
// sends alerts in the alerts list
async function sendAlerts () {
  let promise = new Promise((resolve, reject) => {
    for (let i = 0, len = alerts.length; i < len; i++) {
      (function (i) {
        // timeout so that alerts are alerted in order
        setTimeout(() => {
          let alert = alerts[i];
          let links = [];
          if (parliament.settings.general.includeUrl) {
            links.push({
              text: 'Parliament Dashboard',
              url: `${parliament.settings.general.hostname}?searchTerm=${alert.cluster}`
            });
          }
          alert.notifier.sendAlert(alert.config, alert.message, links);
          if (app.get('debug')) {
            console.log('Sending alert:', alert.message, JSON.stringify(alert.config, null, 2));
          }
          if (i === len - 1) { resolve(); }
        }, 250 * i);
      })(i);
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

  for (let n in parliament.settings.notifiers) {
    let setNotifier = parliament.settings.notifiers[n];

    // keep looking for notifiers if the notifier is off
    if (!setNotifier.on) { continue; }

    // quit before sending the alert if the alert is off
    if (!setNotifier.alerts[issue.type]) { continue; }

    let config = {};
    const notifierDef = internals.notifierTypes[setNotifier.type];

    for (let f in notifierDef.fields) {
      let fieldDef = notifierDef.fields[f];
      let field = setNotifier.fields[fieldDef.name];
      if (!field || (fieldDef.required && !field.value)) {
        // field doesn't exist, or field is required and doesn't have a value
        console.log(`Missing the ${field.name} field for ${n} alerting. Add it on the settings page.`);
        continue;
      }
      config[fieldDef.name] = field.value;
    }

    alerts.push({
      config: config,
      message: message,
      notifier: notifierDef,
      cluster: cluster.title
    });
  }
}

// Finds an issue in a cluster
function findIssue (clusterId, issueType, node) {
  for (let issue of issues) {
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
  let issueType = issueTypes[newIssue.type];
  newIssue.text = issueType.text;
  newIssue.title = issueType.name;
  newIssue.severity = issueType.severity;
  newIssue.clusterId = cluster.id;
  newIssue.cluster = cluster.title;
  newIssue.message = formatIssueMessage(cluster, newIssue);
  newIssue.provisional = true;

  let existingIssue = false;

  // don't duplicate existing issues, update them
  for (let issue of issues) {
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
        issue.alerted     = undefined;
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

  fs.writeFile(app.get('issuesfile'), JSON.stringify(issues, null, 2), 'utf8',
    (err) => {
      if (err) {
        console.log('Unable to write issue:', err.message || err);
      }
    }
  );
}

// Retrieves the health of each cluster and updates the cluster with that info
function getHealth (cluster) {
  return new Promise((resolve, reject) => {
    let timeout = getGeneralSetting('esQueryTimeout') * 1000;

    let options = {
      url: `${cluster.localUrl || cluster.url}/eshealth.json`,
      method: 'GET',
      rejectUnauthorized: false,
      timeout: timeout
    };

    rp(options)
      .then((response) => {
        cluster.healthError = undefined;

        let health;
        try {
          health = JSON.parse(response);
        } catch (e) {
          cluster.healthError = 'ES health parse failure';
          console.log('Bad response for es health', cluster.localUrl || cluster.url);
          return resolve();
        }

        if (health) {
          cluster.status      = health.status;
          cluster.totalNodes  = health.number_of_nodes;
          cluster.dataNodes   = health.number_of_data_nodes;

          if (cluster.status === 'red') { // alert on red es status
            setIssue(cluster, { type: 'esRed' });
          }
        }

        return resolve();
      })
      .catch((error) => {
        let message = error.message || error;

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
    let timeout = getGeneralSetting('esQueryTimeout') * 1000;

    let options = {
      url: `${cluster.localUrl || cluster.url}/parliament.json`,
      method: 'GET',
      rejectUnauthorized: false,
      timeout: timeout
    };

    // Get now before the query since we don't know how long query/response will take
    let now = Date.now() / 1000;
    rp(options)
      .then((response) => {
        cluster.statsError = undefined;

        if (response.bsqErr) {
          cluster.statsError = response.bsqErr;
          console.log('Get stats error', response.bsqErr);
          return resolve();
        }

        let stats;
        try {
          stats = JSON.parse(response);
        } catch (e) {
          cluster.statsError = 'ES stats parse failure';
          console.log('Bad response for stats', cluster.localUrl || cluster.url);
          return resolve();
        }

        if (!stats || !stats.data) { return resolve(); }

        cluster.deltaBPS = 0;
        cluster.deltaTDPS = 0;
        cluster.molochNodes = 0;
        cluster.monitoring = 0;

        let outOfDate = getGeneralSetting('outOfDate');

        for (let stat of stats.data) {
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
              type  : 'outOfDate',
              node  : stat.nodeName,
              value : stat.currentTime * 1000
            });
          }

          // look for no packets issue
          if (stat.deltaPacketsPerSec <= getGeneralSetting('noPackets')) {
            let now = Date.now();
            let id = cluster.title + stat.nodeName;

            // only set the noPackets issue if there is a record of this cluster/node
            // having noPackets and that issue has persisted for the set length of time
            if (noPacketsMap[id] &&
              now - noPacketsMap[id] >= (getGeneralSetting('noPacketsLength') * 1000)) {
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
              type  : 'esDropped',
              node  : stat.nodeName,
              value : stat.deltaESDroppedPerSec
            });
          }
        }

        return resolve();
      })
      .catch((error) => {
        let message = error.message || error;

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
  for (let n in internals.notifierTypes) {
    let notifier = internals.notifierTypes[n];
    // add alert issue types to notifiers
    notifier.alerts = issueTypes;
    // make fields a map
    let fieldsMap = {};
    for (let field of notifier.fields) {
      fieldsMap[field.name] = field;
    }
    notifier.fields = fieldsMap;
  }

  if (app.get('debug')) {
    console.log('Built notifier alerts:', JSON.stringify(internals.notifierTypes, null, 2));
  }
}

// Initializes the parliament with ids for each group and cluster
// and sets up the parliament settings
function initializeParliament () {
  return new Promise((resolve, reject) => {
    if (!parliament.version || parliament.version < MIN_PARLIAMENT_VERSION) {
      // notify of upgrade
      console.log(
        `WARNING - Current parliament version (${parliament.version || 1}) is less then required version (${MIN_PARLIAMENT_VERSION})
          Upgrading ${app.get('file')} file...\n`
      );

      // do the upgrade
      parliament = upgrade.upgrade(parliament, internals.notifierTypes);

      try { // write the upgraded file
        fs.writeFileSync(app.get('file'), JSON.stringify(parliament, null, 2), 'utf8');
      } catch (e) { // notify of error saving upgraded parliament and exit
        console.log(`Error upgrading Parliament:\n\n`, e.stack);
        console.log(parliamentReadError);
        process.exit(1);
      }

      // notify of upgrade success
      console.log(`SUCCESS - Parliament upgraded to version ${MIN_PARLIAMENT_VERSION}`);
    }

    if (!parliament.groups) { parliament.groups = []; }

    // set id for each group/cluster
    for (let group of parliament.groups) {
      group.id = groupId++;
      if (group.clusters) {
        for (let cluster of group.clusters) {
          cluster.id = clusterId++;
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

    fs.writeFile(app.get('file'), JSON.stringify(parliament, null, 2), 'utf8',
      (err) => {
        if (err) {
          console.log('Parliament initialization error:', err.message || err);
          return reject(new Error('Parliament initialization error'));
        }

        return resolve();
      }
    );
  });
}

// Chains all promises for requests for health and stats to update each cluster
// in the parliament
function updateParliament () {
  return new Promise((resolve, reject) => {
    let promises = [];
    for (let group of parliament.groups) {
      if (group.clusters) {
        for (let cluster of group.clusters) {
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

    let issuesRemoved = cleanUpIssues();

    Promise.all(promises)
      .then(() => {
        if (issuesRemoved) { // save the issues that were removed
          fs.writeFile(app.get('issuesfile'), JSON.stringify(issues, null, 2), 'utf8',
            (err) => {
              if (err) {
                console.log('Unable to write issue:', err.message || err);
              }
            }
          );
        }

        // save the data created after updating the parliament
        fs.writeFile(app.get('file'), JSON.stringify(parliament, null, 2), 'utf8',
          (err) => {
            if (err) {
              console.log('Parliament update error:', err.message || err);
              return reject(new Error('Parliament update error'));
            }

            return resolve();
          });

        if (app.get('debug')) {
          console.log('Parliament updated!');
          if (issuesRemoved) {
            console.log('Issues updated!');
          }
        }

        return resolve();
      })
      .catch((error) => {
        console.log('Parliament update error:', error.messge || error);
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

// Writes the parliament to the parliament json file, updates the parliament
// with health and stats, then sends success or error
function writeParliament (req, res, next, successObj, errorText, sendParliament) {
  fs.writeFile(app.get('file'), JSON.stringify(parliament, null, 2), 'utf8',
    (err) => {
      if (app.get('debug')) {
        console.log('Wrote parliament file', err || '');
      }

      if (err) {
        const errorMsg = `Unable to write parliament data: ${err.message || err}`;
        console.log(errorMsg);
        const error = new Error(errorMsg);
        error.httpStatusCode = 500;
        return next(error);
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
          const error = new Error(errorText || 'Error updating parliament.');
          error.httpStatusCode = 500;
          return next(error);
        });
    }
  );
}

// Writes the issues to the issues json file then sends success or error
function writeIssues (req, res, next, successObj, errorText, sendIssues) {
  fs.writeFile(app.get('issuesfile'), JSON.stringify(issues, null, 2), 'utf8',
    (err) => {
      if (app.get('debug')) {
        console.log('Wrote issues file', err || '');
      }

      if (err) {
        const errorMsg = `Unable to write issue data: ${err.message || err}`;
        console.log(errorMsg);
        const error = new Error(errorMsg);
        error.httpStatusCode = 500;
        return next(error);
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
// Authenticate user
router.post('/auth', (req, res, next) => {
  if (app.get('dashboardOnly')) {
    const error = new Error('Your Parliament is in dasboard only mode. You cannot login.');
    error.httpStatusCode = 403;
    return next(error);
  }

  let hasAuth = !!app.get('password');
  if (!hasAuth) {
    const error = new Error('No password set.');
    error.httpStatusCode = 401;
    return next(error);
  }

  // check if password matches
  if (!bcrypt.compareSync(req.body.password, app.get('password'))) {
    const error = new Error('Authentication failed.');
    error.httpStatusCode = 401;
    return next(error);
  }

  const payload = { admin:true };

  let token = jwt.sign(payload, app.get('password'), {
    expiresIn: 60 * 60 * 24 // expires in 24 hours
  });

  res.json({ // return the information including token as JSON
    success : true,
    text    : 'Here\'s your token!',
    token   : token
  });
});

// Get whether authentication or dashboardOnly mode is set
router.get('/auth', (req, res, next) => {
  let hasAuth = !!app.get('password');
  let dashboardOnly = !!app.get('dashboardOnly');
  return res.json({
    hasAuth: hasAuth,
    dashboardOnly: dashboardOnly
  });
});

// Get whether the user is logged in
// If it passes the verifyToken middleware, the user is logged in
router.get('/auth/loggedin', verifyToken, (req, res, next) => {
  return res.json({ loggedin:true });
});

// Update (or create) a password for the parliament
router.put('/auth/update', (req, res, next) => {
  if (app.get('dashboardOnly')) {
    const error = new Error('Your Parliament is in dasboard only mode. You cannot create a password.');
    error.httpStatusCode = 403;
    return next(error);
  }

  if (!req.body.newPassword) {
    const error = new Error('You must provide a new password');
    error.httpStatusCode = 422;
    return next(error);
  }

  let hasAuth = !!app.get('password');
  if (hasAuth) { // if the user has a password already set
    // check if the user has supplied their current password
    if (!req.body.currentPassword) {
      const error = new Error('You must provide your current password');
      error.httpStatusCode = 401;
      return next(error);
    }
    // check if password matches
    if (!bcrypt.compareSync(req.body.currentPassword, app.get('password'))) {
      const error = new Error('Authentication failed.');
      error.httpStatusCode = 401;
      return next(error);
    }
  }

  bcrypt.hash(req.body.newPassword, saltrounds, (err, hash) => {
    if (err) {
      console.log(`Error hashing password: ${err}`);
      const error = new Error('Hashing password failed.');
      error.httpStatusCode = 401;
      return next(error);
    }

    app.set('password', hash);

    parliament.password = hash;

    const payload = { admin:true };

    let token = jwt.sign(payload, hash, {
      expiresIn: 60 * 60 * 24 // expires in 24 hours
    });

    // return the information including token as JSON
    let successObj  = { success: true, text: 'Here\'s your new token!', token: token };
    let errorText   = 'Unable to update your password.';
    writeParliament(req, res, next, successObj, errorText);
  });
});

router.get('/notifierTypes', verifyToken, (req, res) => {
  return res.json(internals.notifierTypes || {});
});

// Get the parliament settings object
router.get('/settings', verifyToken, (req, res, next) => {
  if (!parliament.settings) {
    const error = new Error('Your settings are empty. Try restarting Parliament.');
    error.httpStatusCode = 500;
    return next(error);
  }

  let settings = JSON.parse(JSON.stringify(parliament.settings));

  if (!settings.general) {
    settings.general = settingsDefault.general;
  }

  return res.json(settings);
});

// Update the parliament general settings object
router.put('/settings', verifyToken, (req, res, next) => {
  // save general settings
  for (let s in req.body.settings.general) {
    let setting = req.body.settings.general[s];
    if (s !== 'hostname' && s !== 'includeUrl' && isNaN(setting)) {
      const error = new Error(`${s} must be a number.`);
      error.httpStatusCode = 422;
      return next(error);
    }
    if (s !== 'hostname' && s !== 'includeUrl') { setting = parseInt(setting); }
    parliament.settings.general[s] = setting;
  }

  let successObj  = { success: true, text: 'Successfully updated your settings.' };
  let errorText   = 'Unable to update your settings.';
  writeParliament(req, res, next, successObj, errorText);
});

// Update an existing notifier
router.put('/notifiers/:name', verifyToken, (req, res, next) => {
  if (!parliament.settings.notifiers[req.params.name]) {
    const error = new Error(`${req.params.name} not fount.`);
    error.httpStatusCode = 404;
    return next(error);
  }

  if (!req.body.key) {
    const error = new Error('Missing notifier key');
    error.httpStatusCode = 403;
    return next(error);
  }

  if (!req.body.notifier) {
    const error = new Error('Missing notifier');
    error.httpStatusCode = 403;
    return next(error);
  }

  if (!req.body.notifier.name) {
    const error = new Error('Missing notifier name');
    error.httpStatusCode = 403;
    return next(error);
  }

  if (!req.body.notifier.type) {
    const error = new Error('Missing notifier type');
    error.httpStatusCode = 403;
    return next(error);
  }

  if (!req.body.notifier.fields) {
    const error = new Error('Missing notifier fields');
    error.httpStatusCode = 403;
    return next(error);
  }

  if (!req.body.notifier.alerts) {
    const error = new Error('Missing notifier alerts');
    error.httpStatusCode = 403;
    return next(error);
  }

  req.body.notifier.name = req.body.notifier.name.replace(/[^-a-zA-Z0-9_: ]/g, '');

  if (req.body.notifier.name !== req.body.key &&
    parliament.settings.notifiers[req.body.notifier.name]) {
    const error = new Error(`${req.body.notifier.name} already exists. Notifier names must be unique`);
    error.httpStatusCode = 403;
    return next(error);
  }

  let foundNotifier;
  for (let n in internals.notifierTypes) {
    let notifier = internals.notifierTypes[n];
    if (notifier.type === req.body.notifier.type) {
      foundNotifier = notifier;
    }
  }

  if (!foundNotifier) {
    const error = new Error('Unknown notifier type');
    error.httpStatusCode = 403;
    return next(error);
  }

  // check that required notifier fields exist
  for (let f in foundNotifier.fields) {
    let field = foundNotifier.fields[f];
    for (let sf in req.body.notifier.fields) {
      let sentField = req.body.notifier.fields[sf];
      if (sentField.name === field.name && field.required && !sentField.value) {
        const error = new Error(`Missing a value for ${field.name}`);
        error.httpStatusCode = 403;
        return next(error);
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

  let successObj = {
    success: true,
    newKey: req.body.notifier.name,
    text: `Successfully updated ${req.params.name} notifier.`
  };
  let errorText = `Cannot update ${req.params.name} notifier`;
  writeParliament(req, res, next, successObj, errorText);
});

// Remove a notifier
router.delete('/notifiers/:name', verifyToken, (req, res, next) => {
  if (!parliament.settings.notifiers[req.params.name]) {
    const error = new Error(`Cannot find ${req.params.name} notifier to remove`);
    error.httpStatusCode = 403;
    return next(error);
  }

  parliament.settings.notifiers[req.params.name] = undefined;

  let successObj  = { success: true, text: `Successfully removed ${req.params.name} notifier.` };
  let errorText   = `Cannot remove ${req.params.name} notifier`;
  writeParliament(req, res, next, successObj, errorText);
});

// Create a new notifier
router.post('/notifiers', verifyToken, (req, res, next) => {
  if (!req.body.notifier) {
    const error = new Error('Missing notifier');
    error.httpStatusCode = 403;
    return next(error);
  }

  if (!req.body.notifier.name) {
    const error = new Error('Missing a unique notifier name');
    error.httpStatusCode = 403;
    return next(error);
  }

  if (!req.body.notifier.type) {
    const error = new Error('Missing notifier type');
    error.httpStatusCode = 403;
    return next(error);
  }

  if (!req.body.notifier.fields) {
    const error = new Error('Missing notifier fields');
    error.httpStatusCode = 403;
    return next(error);
  }

  req.body.notifier.name = req.body.notifier.name.replace(/[^-a-zA-Z0-9_: ]/g, '');

  if (parliament.settings.notifiers[req.body.notifier.name]) {
    const error = new Error(`${req.body.notifier.name} already exists. Notifier names must be unique`);
    error.httpStatusCode = 403;
    return next(error);
  }

  let foundNotifier;
  for (let n in internals.notifierTypes) {
    let notifier = internals.notifierTypes[n];
    if (notifier.type === req.body.notifier.type) {
      foundNotifier = notifier;
    }
  }

  if (!foundNotifier) {
    const error = new Error('Unknown notifier type');
    error.httpStatusCode = 403;
    return next(error);
  }

  // check that required notifier fields exist
  for (let f in foundNotifier.fields) {
    let field = foundNotifier.fields[f];
    for (let sf in req.body.notifier.fields) {
      let sentField = req.body.notifier.fields[sf];
      if (sentField.name === field.name && field.required && !sentField.value) {
        const error = new Error(`Missing a value for ${field.name}`);
        error.httpStatusCode = 403;
        return next(error);
      }
    }
  }

  parliament.settings.notifiers[req.body.notifier.name] = req.body.notifier;

  let successObj = {
    success: true,
    name: req.body.notifier.name,
    text: `Successfully added ${req.body.notifier.name} notifier.`
  };
  let errorText  = `Unable to add ${req.body.notifier.name} notifier.`;
  writeParliament(req, res, next, successObj, errorText);
});

// Update the parliament general settings object to the defaults
router.put('/settings/restoreDefaults', verifyToken, (req, res, next) => {
  let type = 'all'; // default
  if (req.body.type) {
    type = req.body.type;
  }

  if (type === 'general') {
    parliament.settings.general = JSON.parse(JSON.stringify(settingsDefault.general));
  } else {
    parliament.settings = JSON.parse(JSON.stringify(settingsDefault));
  }

  let settings = JSON.parse(JSON.stringify(parliament.settings));

  fs.writeFile(app.get('file'), JSON.stringify(parliament, null, 2), 'utf8',
    (err) => {
      if (err) {
        const errorMsg = `Unable to write parliament data: ${err.message || err}`;
        console.log(errorMsg);
        const error = new Error(errorMsg);
        error.httpStatusCode = 500;
        return next(error);
      }

      return res.json({
        settings: settings,
        text: `Successfully restored ${req.body.type} default settings.`
      });
    }
  );
});

// Get parliament with stats
router.get('/parliament', (req, res, next) => {
  let parliamentClone = JSON.parse(JSON.stringify(parliament));

  for (const group of parliamentClone.groups) {
    for (let cluster of group.clusters) {
      cluster.activeIssues = [];
      for (let issue of issues) {
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
router.put('/parliament', verifyToken, (req, res, next) => {
  if (!req.body.reorderedParliament) {
    const error = new Error('You must provide the new parliament order');
    error.httpStatusCode = 422;
    return next(error);
  }

  // remove any client only stuff
  for (const group of req.body.reorderedParliament.groups) {
    group.filteredClusters = undefined;
    for (const cluster of group.clusters) {
      cluster.issues = undefined;
      cluster.activeIssues = undefined;
    }
  }

  parliament = req.body.reorderedParliament;
  updateParliament();

  let successObj  = { success: true, text: 'Successfully reordered items in your parliament.' };
  let errorText   = 'Unable to update the order of items in your parliament.';
  writeParliament(req, res, next, successObj, errorText);
});

// Create a new group in the parliament
router.post('/groups', verifyToken, (req, res, next) => {
  if (!req.body.title) {
    const error = new Error('A group must have a title');
    error.httpStatusCode = 422;
    return next(error);
  }

  let newGroup = { title:req.body.title, id:groupId++, clusters:[] };
  if (req.body.description) { newGroup.description = req.body.description; }

  parliament.groups.push(newGroup);

  let successObj  = { success:true, group:newGroup, text: 'Successfully added new group.' };
  let errorText   = 'Unable to add that group to your parliament.';
  writeParliament(req, res, next, successObj, errorText);
});

// Delete a group in the parliament
router.delete('/groups/:id', verifyToken, (req, res, next) => {
  let index = 0;
  let foundGroup = false;
  for (let group of parliament.groups) {
    if (group.id === parseInt(req.params.id)) {
      parliament.groups.splice(index, 1);
      foundGroup = true;
      break;
    }
    ++index;
  }

  if (!foundGroup) {
    const error = new Error('Unable to find group to delete.');
    error.httpStatusCode = 500;
    return next(error);
  }

  let successObj  = { success:true, text:'Successfully removed the requested group.' };
  let errorText   = 'Unable to remove that group from the parliament.';
  writeParliament(req, res, next, successObj, errorText);
});

// Update a group in the parliament
router.put('/groups/:id', verifyToken, (req, res, next) => {
  if (!req.body.title) {
    const error = new Error('A group must have a title.');
    error.httpStatusCode = 422;
    return next(error);
  }

  let foundGroup = false;
  for (let group of parliament.groups) {
    if (group.id === parseInt(req.params.id)) {
      group.title = req.body.title;
      if (req.body.description) {
        group.description = req.body.description;
      }
      foundGroup = true;
      break;
    }
  }

  if (!foundGroup) {
    const error = new Error('Unable to find group to edit.');
    error.httpStatusCode = 500;
    return next(error);
  }

  let successObj  = { success:true, text:'Successfully updated the requested group.' };
  let errorText   = 'Unable to update that group in the parliament.';
  writeParliament(req, res, next, successObj, errorText);
});

// Create a new cluster within an existing group
router.post('/groups/:id/clusters', verifyToken, (req, res, next) => {
  if (!req.body.title || !req.body.url) {
    let message;
    if (!req.body.title) {
      message = 'A cluster must have a title.';
    } else if (!req.body.url) {
      message = 'A cluster must have a url.';
    }

    const error = new Error(message);
    error.httpStatusCode = 422;
    return next(error);
  }

  let newCluster = {
    title       : req.body.title,
    description : req.body.description,
    url         : req.body.url,
    localUrl    : req.body.localUrl,
    id          : clusterId++,
    type        : req.body.type || undefined
  };

  let foundGroup = false;
  for (let group of parliament.groups) {
    if (group.id === parseInt(req.params.id)) {
      group.clusters.push(newCluster);
      foundGroup = true;
      break;
    }
  }

  if (!foundGroup) {
    const error = new Error('Unable to find group to place cluster.');
    error.httpStatusCode = 500;
    return next(error);
  }

  let successObj  = {
    success   : true,
    cluster   : newCluster,
    parliament: parliament,
    text      : 'Successfully added the requested cluster.'
  };
  let errorText   = 'Unable to add that cluster to the parliament.';
  writeParliament(req, res, next, successObj, errorText, true);
});

// Delete a cluster
router.delete('/groups/:groupId/clusters/:clusterId', verifyToken, (req, res, next) => {
  let clusterIndex = 0;
  let foundCluster = false;
  for (let group of parliament.groups) {
    if (group.id === parseInt(req.params.groupId)) {
      for (let cluster of group.clusters) {
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
    const error = new Error('Unable to find cluster to delete.');
    error.httpStatusCode = 500;
    return next(error);
  }

  let successObj  = { success:true, text: 'Successfully removed the requested cluster.' };
  let errorText   = 'Unable to remove that cluster from your parliament.';
  writeParliament(req, res, next, successObj, errorText);
});

// Update a cluster
router.put('/groups/:groupId/clusters/:clusterId', verifyToken, (req, res, next) => {
  if (!req.body.title || !req.body.url) {
    let message;
    if (!req.body.title) {
      message = 'A cluster must have a title.';
    } else if (!req.body.url) {
      message = 'A cluster must have a url.';
    }

    const error = new Error(message);
    error.httpStatusCode = 422;
    return next(error);
  }

  let foundCluster = false;
  for (let group of parliament.groups) {
    if (group.id === parseInt(req.params.groupId)) {
      for (let cluster of group.clusters) {
        if (cluster.id === parseInt(req.params.clusterId)) {
          cluster.url             = req.body.url;
          cluster.title           = req.body.title;
          cluster.localUrl        = req.body.localUrl;
          cluster.description     = req.body.description;
          cluster.hideDeltaBPS    = req.body.hideDeltaBPS;
          cluster.hideDataNodes   = req.body.hideDataNodes;
          cluster.hideDeltaTDPS   = req.body.hideDeltaTDPS;
          cluster.hideTotalNodes  = req.body.hideTotalNodes;
          cluster.hideMonitoring  = req.body.hideMonitoring;
          cluster.hideMolochNodes = req.body.hideMolochNodes;
          cluster.type            = req.body.type || undefined;

          foundCluster = true;
          break;
        }
      }
    }
  }

  if (!foundCluster) {
    const error = new Error('Unable to find cluster to update.');
    error.httpStatusCode = 500;
    return next(error);
  }

  let successObj  = { success: true, text: 'Successfully updated the requested cluster.' };
  let errorText   = 'Unable to update that cluster in your parliament.';
  writeParliament(req, res, next, successObj, errorText);
});

// Get a list of issues
router.get('/issues', (req, res, next) => {
  let issuesClone = JSON.parse(JSON.stringify(issues));

  // filter out provisional issues
  issuesClone = issuesClone.filter((issue) => !issue.provisional);

  if (req.query.filter) { // simple search for issues
    let searchTerm = req.query.filter.toLowerCase();
    issuesClone = issuesClone.filter((issue) => {
      return issue.severity.toLowerCase().includes(searchTerm) ||
        (issue.node && issue.node.toLowerCase().includes(searchTerm)) ||
        issue.cluster.toLowerCase().includes(searchTerm) ||
        issue.message.toLowerCase().includes(searchTerm) ||
        issue.title.toLowerCase().includes(searchTerm) ||
        issue.text.toLowerCase().includes(searchTerm);
    });
  }

  let type = 'string';
  let sortBy = req.query.sort;
  if (sortBy === 'ignoreUntil' ||
    sortBy === 'firstNoticed' ||
    sortBy === 'lastNoticed' ||
    sortBy === 'acknowledged') {
    type = 'number';
  }

  if (sortBy) {
    let order = req.query.order || 'desc';
    issuesClone.sort((a, b) => {
      if (type === 'string') {
        let aVal = '';
        let bVal = '';

        if (b[sortBy] !== undefined) { bVal = b[sortBy]; }
        if (a[sortBy] !== undefined) { aVal = a[sortBy]; }

        return order === 'asc' ? bVal.localeCompare(aVal) : aVal.localeCompare(bVal);
      } else if (type === 'number') {
        let aVal = 0;
        let bVal = 0;

        if (b[sortBy] !== undefined) { bVal = b[sortBy]; }
        if (a[sortBy] !== undefined) { aVal = a[sortBy]; }

        return order === 'asc' ? bVal - aVal : aVal - bVal;
      }
    });
  }

  let recordsFiltered = issuesClone.length;

  if (req.query.length) { // paging
    let len = parseInt(req.query.length);
    let start = !req.query.start ? 0 : parseInt(req.query.start);

    issuesClone = issuesClone.slice(start, len + start);
  }

  return res.json({
    issues: issuesClone,
    recordsFiltered: recordsFiltered
  });
});

// acknowledge one or more issues
router.put('/acknowledgeIssues', verifyToken, (req, res, next) => {
  if (!req.body.issues || !req.body.issues.length) {
    let message = 'Must specify the issue(s) to acknowledge.';
    const error = new Error(message);
    error.httpStatusCode = 422;
    return next(error);
  }

  let now = Date.now();
  let count = 0;

  for (let i of req.body.issues) {
    let issue = findIssue(parseInt(i.clusterId), i.type, i.node);
    if (issue) {
      issue.acknowledged = now;
      count++;
    }
  }

  if (!count) {
    let errorText = 'Unable to acknowledge requested issue';
    if (req.body.issues.length > 1) { errorText += 's'; }
    const error = new Error(errorText);
    error.httpStatusCode = 500;
    return next(error);
  }

  let successText = `Successfully acknowledged ${count} requested issue`;
  let errorText = 'Unable to acknowledge the requested issue';
  if (count > 1) {
    successText += 's';
    errorText += 's';
  }

  let successObj = { success:true, text:successText, acknowledged:now };
  writeIssues(req, res, next, successObj, errorText);
});

// ignore one or more issues
router.put('/ignoreIssues', verifyToken, (req, res, next) => {
  if (!req.body.issues || !req.body.issues.length) {
    let message = 'Must specify the issue(s) to ignore.';
    const error = new Error(message);
    error.httpStatusCode = 422;
    return next(error);
  }

  let ms = req.body.ms || 3600000; // Default to 1 hour
  let ignoreUntil = Date.now() + ms;
  if (ms === -1) { ignoreUntil = -1; } // -1 means ignore it forever

  let count = 0;

  for (let i of req.body.issues) {
    let issue = findIssue(parseInt(i.clusterId), i.type, i.node);
    if (issue) {
      issue.ignoreUntil = ignoreUntil;
      count++;
    }
  }

  if (!count) {
    let errorText = 'Unable to ignore requested issue';
    if (req.body.issues.length > 1) { errorText += 's'; }
    const error = new Error(errorText);
    error.httpStatusCode = 500;
    return next(error);
  }

  let successText = `Successfully ignored ${count} requested issue`;
  let errorText = 'Unable to ignore the requested issue';
  if (count > 1) {
    successText += 's';
    errorText += 's';
  }

  let successObj = { success:true, text:successText, ignoreUntil:ignoreUntil };
  writeIssues(req, res, next, successObj, errorText);
});

// unignore one or more issues
router.put('/removeIgnoreIssues', verifyToken, (req, res, next) => {
  if (!req.body.issues || !req.body.issues.length) {
    let message = 'Must specify the issue(s) to unignore.';
    const error = new Error(message);
    error.httpStatusCode = 422;
    return next(error);
  }

  let count = 0;

  for (let i of req.body.issues) {
    let issue = findIssue(parseInt(i.clusterId), i.type, i.node);
    if (issue) {
      issue.ignoreUntil = undefined;
      issue.alerted     = undefined; // reset alert time so it can alert again
      count++;
    }
  }

  if (!count) {
    let errorText = 'Unable to unignore requested issue';
    if (req.body.issues.length > 1) { errorText += 's'; }
    const error = new Error(errorText);
    error.httpStatusCode = 500;
    return next(error);
  }

  let successText = `Successfully unignored ${count} requested issue`;
  let errorText = 'Unable to unignore the requested issue';
  if (count > 1) {
    successText += 's';
    errorText += 's';
  }

  let successObj = { success:true, text:successText };
  writeIssues(req, res, next, successObj, errorText);
});

// Remove an issue with a cluster
router.put('/groups/:groupId/clusters/:clusterId/removeIssue', verifyToken, (req, res, next) => {
  if (!req.body.type) {
    let message = 'Must specify the issue type to remove.';
    const error = new Error(message);
    error.httpStatusCode = 422;
    return next(error);
  }

  let foundIssue = removeIssue(req.body.type, req.params.clusterId, req.body.node);

  if (!foundIssue) {
    const error = new Error('Unable to find issue to remove. Maybe it was already removed.');
    error.httpStatusCode = 500;
    return next(error);
  }

  let successObj  = { success:true, text:'Successfully removed the requested issue.' };
  let errorText   = 'Unable to remove that issue.';
  writeIssues(req, res, next, successObj, errorText);
});

// Remove all acknowledged all issues
router.put('/issues/removeAllAcknowledgedIssues', verifyToken, (req, res, next) => {
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
    const error = new Error('There are no acknowledged issues to remove.');
    error.httpStatusCode = 400;
    return next(error);
  }

  let successObj  = { success:true, text:`Successfully removed ${count} acknowledged issues.` };
  let errorText   = 'Unable to remove acknowledged issues.';
  writeIssues(req, res, next, successObj, errorText, true);
});

// remove one or more acknowledged issues
router.put('/removeSelectedAcknowledgedIssues', verifyToken, (req, res, next) => {
  if (!req.body.issues || !req.body.issues.length) {
    let message = 'Must specify the acknowledged issue(s) to remove.';
    const error = new Error(message);
    error.httpStatusCode = 422;
    return next(error);
  }

  let count = 0;

  // mark issues to remove
  for (let i of req.body.issues) {
    let issue = findIssue(parseInt(i.clusterId), i.type, i.node);
    if (issue && issue.acknowledged) {
      count++;
      issue.remove = true;
    }
  }

  if (!count) {
    const error = new Error('There are no acknowledged issues to remove.');
    error.httpStatusCode = 400;
    return next(error);
  }

  count = 0;
  let len = issues.length;
  while (len--) {
    let issue = issues[len];
    if (issue.remove) {
      count++;
      issues.splice(len, 1);
    }
  }

  if (!count) {
    let errorText = 'Unable to remove requested issue';
    if (req.body.issues.length > 1) { errorText += 's'; }
    const error = new Error(errorText);
    error.httpStatusCode = 500;
    return next(error);
  }

  let successText = `Successfully removed ${count} requested issue`;
  let errorText = 'Unable to remove the requested issue';
  if (count > 1) {
    successText += 's';
    errorText += 's';
  }

  let successObj = { success:true, text:successText };
  writeIssues(req, res, next, successObj, errorText);
});

// issue a test alert to a specified notifier
router.post('/testAlert', (req, res, next) => {
  if (!req.body.notifier) {
    const error = new Error('Must specify the notifier.');
    error.httpStatusCode = 422;
    return next(error);
  }

  const notifier = parliament.settings.notifiers[req.body.notifier];

  if (!notifier) {
    let errorText = 'Unable to find the requested notifier';
    const error = new Error(errorText);
    error.httpStatusCode = 500;
    return next(error);
  }

  let config = {};

  for (let f in notifier.fields) {
    let field = notifier.fields[f];
    if (!field || (field.required && !field.value)) {
      // field doesn't exist, or field is required and doesn't have a value
      let message = `Missing the ${f} field for the ${notifier.name} notifier. Add it on the settings page.`;
      console.log(message);

      const error = new Error(message);
      error.httpStatusCode = 422;
      return next(error);
    }
    config[f] = field.value;
  }

  internals.notifierTypes[notifier.type].sendAlert(
    config,
    `Test alert from the ${notifier.name} notifier!`
  );

  let successObj = {
    success: true,
    text: `Successfully issued alert using the ${notifier.name} notifier.`
  };
  let errorText = `Unable to issue alert using the ${notifier.name} notifier.`;
  writeParliament(req, res, next, successObj, errorText);
});

/* SIGNALS! ----------------------------------------------------------------- */
// Explicit sigint handler for running under docker
// See https://github.com/nodejs/node/issues/4182
process.on('SIGINT', function () {
  process.exit();
});

/* LISTEN! ----------------------------------------------------------------- */
// vue index page
app.use((req, res, next) => {
  res.status(404).sendFile(`${__dirname}/vueapp/dist/index.html`);
});

let server;
if (app.get('keyFile') && app.get('certFile')) {
  const certOptions = {
    key : fs.readFileSync(app.get('keyFile')),
    cert: fs.readFileSync(app.get('certFile'))
  };
  server = https.createServer(certOptions, app);
} else {
  server = http.createServer(app);
}

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
        console.log(`ERROR - never mind, couldn't initialize Parliament\n`, err);
        process.exit(1);
      });

    setInterval(() => {
      updateParliament();
      processAlerts();
    }, 10000);
  });
