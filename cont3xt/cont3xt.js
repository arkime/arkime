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
const dayMs = 60000 * 60 * 24;

const internals = {
  configFile: `${version.config_prefix}/etc/cont3xt.ini`,
  debug: 0,
  insecure: false,
  regressionTests: false
};

// ----------------------------------------------------------------------------
// Routes
// ----------------------------------------------------------------------------

app.post('/shutdown', (req, res) => {
  if (internals.regressionTests) {
    console.log('Shutting down');
    process.exit(0);
  }
});

app.use(Auth.doAuth);

app.get('/api/linkGroup/getViewable', LinkGroup.apiGetViewable);
app.get('/api/linkGroup/getEditable', LinkGroup.apiGetEditable);
app.put('/api/linkGroup', [jsonParser], LinkGroup.apiCreate);
app.put('/api/linkGroup/:id', [jsonParser], LinkGroup.apiUpdate);
app.delete('/api/linkGroup/:id', [jsonParser], LinkGroup.apiDelete);

app.get('/api/roles', User.apiRoles);

app.get('/api/integration/search/:query', Integration.apiSearch);

app.get('/test', (req, res) => {
  for (let i = 0; i < 100; i++) {
    setTimeout(() => { res.write(JSON.stringify({ num: i }) + '\n'); }, 100 * i);
  }
  setTimeout(() => { res.end(); }, 100 * 100);
  console.log('/test');
});

// using fallthrough: false because there is no 404 endpoint (client router
// handles 404s) and sending index.html is confusing
app.use('/cont3xt/font-awesome', express.static(
  path.join(__dirname, '/../node_modules/font-awesome'),
  { maxAge: dayMs, fallthrough: false }
));
app.use('/cont3xt/assets', express.static(
  path.join(__dirname, '/../assets'),
  { maxAge: dayMs, fallthrough: false }
));

/* LISTEN! ----------------------------------------------------------------- */
// using fallthrough: false because there is no 404 endpoint (client router
// handles 404s) and sending index.html is confusing
// expose vue bundles (prod)
app.use(['/static', '/cont3xt/static'], express.static(
  path.join(__dirname, '/vueapp/dist/static'),
  { maxAge: dayMs, fallthrough: false }
));
// expose vue bundle (dev)
app.use(['/app.js', '/cont3xt/app.js'], express.static(
  path.join(__dirname, '/vueapp/dist/app.js'),
  { fallthrough: false }
));
app.use(['/app.js.map', '/cont3xt/app.js.map'], express.static(
  path.join(__dirname, '/vueapp/dist/app.js.map'),
  { fallthrough: false }
));

// vue index page
app.use((req, res, next) => {
  res.sendFile(path.join(__dirname, '/vueapp/dist/index.html'));
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

function getConfig (section, sectionKey, d) {
  if (!internals.config[section]) {
    return d;
  }
  return internals.config[section][sectionKey] || d;
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

  const es = getConfig('cont3xt', 'elasticsearch', 'http://localhost:9200').split(',');
  Db.initialize({
    debug: internals.debug,
    node: es,
    apiKey: getConfig('cont3xt', 'elasticsearchAPIKey'),
    basicAuth: getConfig('cont3xt', 'elasticsearchBasicAuth')
  });

  let usersEs = getConfig('cont3xt', 'usersElasticsearch');
  if (usersEs) {
    usersEs = usersEs.split(',');
  } else {
    usersEs = es;
  }

  User.initialize({
    debug: internals.debug,
    node: usersEs,
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
    cache: cache
  });
}

async function main () {
  processArgs(process.argv);
  try {
    internals.config = ini.parseSync(internals.configFile);
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

  /* setTimeout(() => { Integration.search('ip', '10.10.10.10'); }, 1000);
  setTimeout(() => { Integration.search('ip', '8.8.8.8'); }, 1000); */
}

main();
