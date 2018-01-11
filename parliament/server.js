#!/usr/bin/env node
'use strict';


/* dependencies ------------------------------------------------------------- */
const express = require('express');
const path    = require('path');
const http    = require('http');
const https   = require('https');
const fs      = require('fs');
const favicon = require('serve-favicon');
const request = require('request');
const rp      = require('request-promise');
const bp      = require('body-parser');
const logger  = require('morgan');
const jwt     = require('jsonwebtoken');
const bcrypt  = require('bcrypt');


/* app setup --------------------------------------------------------------- */
const app     = express();
const router  = express.Router();

const version = 1;

(function() { // parse arguments
  let appArgs = process.argv.slice(2);
  let file, port;

  function setPasswordHash(err, hash) {
    if (err) {
      console.error(`Error hashing password: ${err}`);
      return;
    }

    app.set('password', hash);
  }

  function help() {
    console.log('server.js [<config options>]\n');
    console.log('Config Options:');
    console.log('  -c, --config   Parliament config file to use');
    console.log('  --pass         Password for updating the parliament');
    console.log('  --port         Port for the web app to listen on');
    console.log('  --cert         Public certificate to use for https');
    console.log('  --key          Private certificate to use for https');

    process.exit(0);
  }

  for (let i = 0, len = appArgs.length; i < len; i++) {
    switch(appArgs[i]) {
      case '-c':
      case '--config':
        file = appArgs[i+1];
        i++;
        break;

      case '--pass':
        bcrypt.hash(appArgs[i+1], 10, setPasswordHash);
        i++;
        break;

      case '--port':
        port = appArgs[i+1];
        i++;
        break;

      case '--cert':
        app.set('certFile', appArgs[i+1]);
        i++;
        break;

      case '--key':
        app.set('keyFile', appArgs[i+1]);
        i++;
        break;

      case '-h':
      case '--help':
        help();
        break;

      default:
        help();
        break;
    }
  }

  if (!appArgs.length) {
    console.log('WARNING: No config options were set, starting Parliament in view only mode with defaults.\n');
  }

  // set optional config options that reqiure defaults
  app.set('port', port || 8008);
  app.set('file', file || './parliament.json');
}());

// get the parliament file or create it if it doesn't exist
let parliament;
try {
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
} catch (err) {
  parliament = { version:version, groups:[] };
}

// define ids for groups and clusters
let groupId = 0, clusterId = 0;
// create timeout for updating the parliament data on an interval
let timeout;

app.disable('x-powered-by');

// parliament app pages
app.use('/parliament', express.static(`${__dirname}/dist/index.html`, { maxAge:600*1000 }));
app.use('/parliament/issues', express.static(`${__dirname}/dist/index.html`, { maxAge:600*1000 }));

// log requests
app.use(logger('dev'));

app.use(favicon(`${__dirname}/public/favicon.ico`));

// serve public files
app.use('/parliament/public', express.static(`${__dirname}/public`, { maxAge:600*1000 }));

// serve app bundles
app.use('/parliament', express.static(path.join(__dirname, 'dist')));

// define router to mount api related functions
app.use('/parliament/api', router);
router.use(bp.json());
router.use(bp.urlencoded({ extended: true }));


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
  console.error(err.stack);
  res.status(err.httpStatusCode || 500).json({
    success : false,
    text    : err.message || 'Error'
  });
});

// 404 page
app.use((req, res, next) => {
  res.status(404).sendFile(`${__dirname}/dist/404.html`);
});

// Verify token
function verifyToken(req, res, next) {
  function tokenError(req, res, errorText) {
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
// TODO alert stuffs!
function issueAlert(cluster, issue) {
  issue.alerted = Date.now();
  console.log(`Alert issued for: ${cluster.title} - ${issue.type}`);
}

// Finds an issue in a cluster
function findIssue(groupId, clusterId, issueType, node) {
  for(let group of parliament.groups) {
    if (group.id === groupId) {
      for (let cluster of group.clusters) {
        if (cluster.id === clusterId) {
          if (cluster.issues) {
            for (let issue of cluster.issues) {
              if (issue.type === issueType && issue.node === node) {
                return issue;
              }
            }
          }
        }
      }
    }
  }

  return;
}

// Updates an existing issue or pushes a new issue onto the issue array
function setIssue(cluster, newIssue) {
  if (!cluster.issues) { cluster.issues = []; }

  for (let issue of cluster.issues) {
    if (issue.type === newIssue.type && issue.node === newIssue.node) {
      if (issue.dismissed && (Date.now() - issue.lastNoticed > 86400000)) {
        // issue was dismissed, but exceeded the threshold (1 day),
        // so it's really a new issue
        issue.alerted       = undefined;
        issue.dismissed     = undefined;
        issue.firstNoticed  = Date.now();
      }


      if (Date.now() > issue.ignoreUntil && issue.ignoreUntil !== -1) {
        // the ignore has expired, so alert!
        issue.ignoreUntil = undefined;
        issue.alerted     = undefined;
      }

      issue.lastNoticed = Date.now();

      if (!issue.dismissed && !issue.ignoreUntil && !issue.alerted) {
        issueAlert(cluster, issue);
      }

      return;
    }
  }

  newIssue.firstNoticed = Date.now();
  cluster.issues.push(newIssue);

  issueAlert(cluster, cluster.issues[cluster.issues.length-1]);

  return;
}

// Retrieves the health of each cluster and updates the cluster with that info
function getHealth(cluster) {
  return new Promise((resolve, reject) => {

  let options = {
    url: `${cluster.localUrl || cluster.url}/eshealth.json`,
    method: 'GET',
    rejectUnauthorized: false
  };

  rp(options)
    .then((response) => {
      cluster.healthError = undefined;

      let health;
      try { health = JSON.parse(response); }
      catch (e) {
        cluster.healthError = 'ES health parse failure';
        console.error('Bad response for es health', cluster.localUrl || cluster.url);
        return resolve();
      }

      if (health) {
        cluster.status      = health.status;
        cluster.totalNodes  = health.number_of_nodes;
        cluster.dataNodes   = health.number_of_data_nodes;

        if (cluster.status === 'red') { // alert on red es status
          setIssue(cluster, {
            type    : 'esRed',
            title   : 'ES is red',
            severity: 'red'
          });
        }
      }

      return resolve();
    })
    .catch((error) => {
      let message = error.message || error;

      setIssue(cluster, {
        type    : 'esDown',
        title   : 'ES is unreachable',
        value   : message,
        severity: 'red'
      });

      cluster.healthError = message;

      console.error('HEALTH ERROR:', message);
      return resolve();
    });

  });
}

// Retrieves, then calculates stats for each cluster and updates the cluster with that info
function getStats(cluster) {
  return new Promise((resolve, reject) => {

  let options = {
    url: `${cluster.localUrl || cluster.url}/stats.json`,
    method: 'GET',
    rejectUnauthorized: false
  };

  rp(options)
    .then((response) => {
      cluster.statsError = undefined;

      if (response.bsqErr) {
        cluster.statsError = response.bsqErr;
        console.error('Get stats error', response.bsqErr);
        return resolve();
      }

      let stats;
      try { stats = JSON.parse(response); }
      catch (e) {
        cluster.statsError = 'ES stats parse failure';
        console.error('Bad response for stats', cluster.localUrl || cluster.url);
        return resolve();
      }

      if (!stats || !stats.data) { return resolve(); }

      cluster.deltaBPS = 0;
      // sum delta bytes per second
      for (let stat of stats.data) {
        if (stat.deltaBytesPerSec) {
          cluster.deltaBPS += stat.deltaBytesPerSec;
        }
      }

      cluster.deltaTDPS = 0;
      // sum delta total dropped per second
      for (let stat of stats.data) {
        if (stat.deltaTotalDroppedPerSec) {
          cluster.deltaTDPS += stat.deltaTotalDroppedPerSec;
        }
      }

      // Look for issues
      for (let stat of stats.data) {
        if ((Date.now()/1000 - stat.currentTime) > 30) {
          setIssue(cluster, {
            type    : 'outOfDate',
            title   : 'Out of date',
            node    : stat.nodeName,
            value   : Date.now() - stat.currentTime,
            severity: 'red'
          });
        }

        if (stat.deltaPacketsPerSec === 0) {
          setIssue(cluster, {
            type    : 'noPackets',
            title   : 'No packets',
            node    : stat.nodeName,
            value   : 0,
            severity: 'yellow'
          });
        }

        if (stat.deltaESDroppedPerSec > 0) {
          setIssue(cluster, {
            type    : 'esDropped',
            title   : 'ES dropped',
            node    : stat.nodeName,
            value   : stat.deltaESDroppedPerSec,
            severity: 'yellow'
          });
        }
      }

      return resolve();
    })
    .catch((error) => {
      let message = error.message || error;
      console.error('STATS ERROR:', message);

      setIssue(cluster, {
        type    : 'esDown',
        title   : 'ES is unreachable',
        value   : message,
        severity: 'red'
      });

      cluster.statsError = message;
      return resolve();
    });
  });
}

// Initializes the parliament with ids for each group and cluster
function initalizeParliament() {
  return new Promise((resolve, reject) => {
    if (!parliament.groups) { parliament.groups = []; }

    for (let group of parliament.groups) {
      group.id = groupId++;
      if (group.clusters) {
        for (let cluster of group.clusters) {
          cluster.id = clusterId++;
        }
      }
    }

    fs.writeFile(app.get('file'), JSON.stringify(parliament, null, 2), 'utf8',
      (err) => {
        if (err) {
          console.error('Parliament initialization error:', err.message || err);
          return reject();
        }

        return resolve();
      }
    );
  });
}

// Chains all promises for requests for health and stats to update each cluster
// in the parliament
function updateParliament() {
  return new Promise((resolve, reject) => {
    let promises = [];
    for (let group of parliament.groups) {
      if (group.clusters) {
        for (let cluster of group.clusters) {
          // only get health for online clusters
          if (!cluster.disabled) {
            promises.push(getHealth(cluster));
          }
          // don't get stats for multiviewers or offline clusters
          if (!cluster.multiviewer && !cluster.disabled) {
            promises.push(getStats(cluster));
          }
        }
      }
    }

    Promise.all(promises)
      .then(() => {
        // save the data created after updating the parliament
        fs.writeFile(app.get('file'), JSON.stringify(parliament, null, 2), 'utf8');
        return resolve();
      })
      .catch((error) => {
        console.error('Parliament update error:', error.messge || error);
        return resolve();
      });

  });
}

// Writes the parliament to the parliament json file, updates the parliament
// with health and stats, then sends success or error
function writeParliament(req, res, next, successObj, errorText, sendParliament) {
  fs.writeFile(app.get('file'), JSON.stringify(parliament, null, 2), 'utf8',
    (err) => {
      if (err) {
        const errorMsg = `Unable to write parliament data: ${err.message || err}`;
        console.error(errorMsg);
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


/* APIs -------------------------------------------------------------------- */
// Authenticate user
router.post('/auth', (req, res, next) => {
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
    expiresIn: 60*60*24 // expires in 24 hours
  });

  res.json({ // return the information including token as JSON
    success : true,
    text    : 'Here\'s your token!',
    token   : token
  });
});

// Get whether authentication is set
router.get('/auth', (req, res, next) => {
  let hasAuth = !!app.get('password');
  return res.json({ hasAuth:hasAuth });
});

// Get whether the user is logged in
// If it passes the verifyToken middleware, the user is logged in
router.get('/auth/loggedin', verifyToken, (req, res, next) => {
  return res.json({ loggedin:true });
});

// Update (or create) a password for the parliament
router.put('/auth/update', (req, res, next) => {
  if (!req.body.password) {
    const error = new Error('You must provide a password');
    error.httpStatusCode = 422;
    return next(error);
  }

  bcrypt.hash(req.body.password, 10, (err, hash) => {
    if (err) {
      console.error(`Error hashing password: ${err}`);
      const error = new Error('Hashing password failed.');
      error.httpStatusCode = 401;
      return next(error);
    }

    app.set('password', hash);

    parliament.password = hash;

    const payload = { admin:true };

    let token = jwt.sign(payload, hash, {
      expiresIn: 60*60*24 // expires in 24 hours
    });

    // return the information including token as JSON
    let successObj  = { success: true, text: 'Here\'s your new token!', token: token };
    let errorText   = 'Unable to update your password.';
    writeParliament(req, res, next, successObj, errorText);
  });
});

// Get parliament with stats
router.get('/parliament', (req, res, next) => {
  let parliamentClone = JSON.parse(JSON.stringify(parliament));
  if (parliamentClone.password) { parliamentClone.password = undefined; }
  return res.json(parliamentClone);
});

// Updates the parliament order of clusters and groups
router.put('/parliament', verifyToken, (req, res, next) => {
  if (!req.body.reorderedParliament) {
    const error = new Error('You must provide the new parliament order');
    error.httpStatusCode = 422;
    return next(error);
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
  let foundGroup = false, index = 0;
  for(let group of parliament.groups) {
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
  for(let group of parliament.groups) {
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
    if (!req.body.title) { message = 'A cluster must have a title.'; }
    else if (!req.body.url) { message = 'A cluster must have a url.'; }
    const error = new Error(message);
    error.httpStatusCode = 422;
    return next(error);
  }

  let newCluster = {
    title       : req.body.title,
    description : req.body.description,
    url         : req.body.url,
    localUrl    : req.body.localUrl,
    multiviewer : req.body.multiviewer,
    disabled    : req.body.disabled,
    id          : clusterId++
  };

  let foundGroup = false;
  for(let group of parliament.groups) {
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
  let foundCluster = false, clusterIndex = 0;
  for(let group of parliament.groups) {
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
    if (!req.body.title) { message = 'A cluster must have a title.'; }
    else if (!req.body.url) { message = 'A cluster must have a url.'; }
    const error = new Error(message);
    error.httpStatusCode = 422;
    return next(error);
  }

  let foundCluster = false;
  for(let group of parliament.groups) {
    if (group.id === parseInt(req.params.groupId)) {
      for (let cluster of group.clusters) {
        if (cluster.id === parseInt(req.params.clusterId)) {
          cluster.title         = req.body.title;
          cluster.description   = req.body.description;
          cluster.url           = req.body.url;
          cluster.localUrl      = req.body.localUrl;
          cluster.multiviewer   = req.body.multiviewer;
          cluster.disabled      = req.body.disabled;
          cluster.hideDeltaBPS  = req.body.hideDeltaBPS;
          cluster.hideDataNodes = req.body.hideDataNodes;
          cluster.hideDeltaTDPS = req.body.hideDeltaTDPS;
          cluster.hideTotalNodes= req.body.hideTotalNodes;
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

  let successObj  = { success:true, text: 'Successfully updated the requested cluster.' };
  let errorText   = 'Unable to update that cluster in your parliament.';
  writeParliament(req, res, next, successObj, errorText);
});

// Get a list of issues
router.get('/issues', (req, res, next) => {
  let issues = [];

  for(let group of parliament.groups) {
    for (let cluster of group.clusters) {
      if (cluster.issues) {
        for (let issue of cluster.issues) {
          if (issue && !issue.dismissed) {
            let issueClone = JSON.parse(JSON.stringify(issue));
            issueClone.groupId = group.id;
            issueClone.clusterId = cluster.id;
            issueClone.cluster = cluster.title;
            issues.push(issueClone);
          }
        }
      }
    }
  }

  return res.json({ issues:issues });
});

// Dismiss an issue with a cluster
router.put('/groups/:groupId/clusters/:clusterId/dismissIssue', verifyToken, (req, res, next) => {
  if (!req.body.type) {
    let message = 'Must specify the issue type to dismiss.';
    const error = new Error(message);
    error.httpStatusCode = 422;
    return next(error);
  }

  let now = Date.now();

  let issue = findIssue(parseInt(req.params.groupId), parseInt(req.params.clusterId), req.body.type, req.body.node);

  if (!issue) {
    const error = new Error('Unable to find issue to dismiss.');
    error.httpStatusCode = 500;
    return next(error);
  }

  issue.dismissed = now;

  let successObj  = { success:true, text:'Successfully dismissed the requested issue.', dismissed:now };
  let errorText   = 'Unable to dismiss that issue.';
  writeParliament(req, res, next, successObj, errorText);
});

// Ignore an issue with a cluster
router.put('/groups/:groupId/clusters/:clusterId/ignoreIssue', verifyToken, (req, res, next) => {
  if (!req.body.type) {
    let message = 'Must specify the issue type to ignore.';
    const error = new Error(message);
    error.httpStatusCode = 422;
    return next(error);
  }

  let ms = req.body.ms || 3600000; // Default to 1 hour

  let ignoreUntil = Date.now() + ms;
  if (ms === -1) { ignoreUntil = -1; } // -1 means ignore it forever

  let issue = findIssue(parseInt(req.params.groupId), parseInt(req.params.clusterId), req.body.type, req.body.node);

  if (!issue) {
    const error = new Error('Unable to find issue to ignore.');
    error.httpStatusCode = 500;
    return next(error);
  }

  issue.ignoreUntil = ignoreUntil;

  let successObj  = { success:true, text:'Successfully ignored the requested issue.', ignoreUntil:ignoreUntil };
  let errorText   = 'Unable to ignore that issue.';
  writeParliament(req, res, next, successObj, errorText);
});

// Allow an issue with a cluster to alert by removing ignoreUntil
router.put('/groups/:groupId/clusters/:clusterId/removeIgnoreIssue', verifyToken, (req, res, next) => {
  if (!req.body.type) {
    let message = 'Must specify the issue type to remove the ignore.';
    const error = new Error(message);
    error.httpStatusCode = 422;
    return next(error);
  }

  let issue = findIssue(parseInt(req.params.groupId), parseInt(req.params.clusterId), req.body.type, req.body.node);

  if (!issue) {
    const error = new Error('Unable to find issue to remove the ignore.');
    error.httpStatusCode = 500;
    return next(error);
  }

  issue.ignoreUntil = undefined;
  issue.alerted     = undefined; // reset alert time so it can alert again

  let successObj  = { success:true, text:'Successfully removed the ignore for the requested issue.' };
  let errorText   = 'Unable to remove the ignore for that issue.';
  writeParliament(req, res, next, successObj, errorText);
});


/* LISTEN! ----------------------------------------------------------------- */
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
    console.error(`ERROR - couldn't listen on port ${app.get('port')}, is Parliament already running?`);
    process.exit(1);
    throw new Error('Exiting');
  })
  .on('listening', function (e) {
    console.log(`Express server listening on port ${server.address().port} in ${app.settings.env} mode`);
  })
  .listen(app.get('port'), () => {
    initalizeParliament()
      .then(() => {
        updateParliament();
      })
      .catch(() => {
        process.exit(1);
        throw new Error('Exiting');
      });

    timeout = setInterval(() => {
      updateParliament();
    }, 10000);
  });
