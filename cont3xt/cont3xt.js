const express = require('express');
const ini = require('iniparser');
const app = express();
const http = require('http');
const path = require('path');
const version = require('../viewer/version');
const User = require('../common/user');
const Auth = require('../common/auth');
const Links = require('./links');
const Db = require('./db');

const internals = {
  configFile: `${version.config_prefix}/etc/cont3xt.ini`,
  debug: 0,
  insecure: false
};

// ----------------------------------------------------------------------------
// Routes
// ----------------------------------------------------------------------------

app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, '/index.html'));
});

app.get('/test', (req, res) => {
  for (let i = 0; i < 100; i++) {
    setTimeout(() => { res.write(JSON.stringify({ num: i }) + '\n'); }, 100 * i);
  }
  setTimeout(() => { res.end(); }, 100 * 100);
  console.log('/test');
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
  if (userNameHeader === 'anonymous' || userNameHeader === 'digest') {
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

  const es = getConfig('cont3xt', 'elasticsearch', 'http://localhost:9200');
  Db.initialize({
    debug: internals.debug,
    node: es,
    apiKey: getConfig('cont3xt', 'elasticsearchAPIKey'),
    basicAuth: getConfig('cont3xt', 'elasticsearchBasicAuth')
  });

  if (mode === 'anonymous') {
    return;
  }

  const usersEs = getConfig('cont3xt', 'usersElasticsearch', 'http://localhost:9200');

  User.initialize({
    debug: internals.debug,
    node: usersEs,
    prefix: getConfig('cont3xt', 'usersPrefix', ''),
    apiKey: getConfig('cont3xt', 'usersElasticsearchAPIKey'),
    basicAuth: getConfig('cont3xt', 'usersElasticsearchBasicAuth')
  });
}

function main () {
  processArgs(process.argv);
  internals.config = ini.parseSync(internals.configFile);
  setupAuth();

  const server = http.createServer(app);
  server.listen(3218);

  const links = Links.get({size: 5});
  console.log('LINKS', links);
}

main();
