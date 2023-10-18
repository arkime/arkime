/******************************************************************************/
/* addUser.js -- Create a new user in the database
 *
 * addUser.js <user id> <user friendly name> <password> [-noweb] [-admin]
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';
const Config = require('./config.js');
const Db = require('./db.js');
const cryptoLib = require('crypto');
const Auth = require('../common/auth');
const User = require('../common/user');
const ArkimeConfig = require('../common/arkimeConfig');

function help () {
  console.log('addUser.js [<config options>] <user id> <user friendly name> <password> [<options>]');
  console.log('');
  console.log('Options:');
  console.log('  --admin                 Has admin privileges');
  console.log('  --apionly               Can only use api, not web pages');
  console.log('  --email                 Can do email searches');
  console.log('  --expression  <expr>    Forced user expression');
  console.log('  --remove                Can remove data (scrub, delete tags)');
  console.log('  --webauth               Can auth using the web auth header or password');
  console.log('  --webauthonly           Can auth using the web auth header only, password ignored');
  console.log('  --packetSearch          Can create a packet search job (hunt)');
  console.log('  --createOnly            Only create the user if it doesn\'t exist');
  console.log('  --roles                 Comma seperated list of roles');
  console.log('');
  console.log('Config Options:');
  console.log('  -c, --config <file|url> Where to fetch the config file from');
  console.log('  -n <node name>          Node name section to use in config file');
  console.log('  --insecure              Disable certificate verification for https calls');

  process.exit(0);
}

function main () {
  if (process.argv[2].length < 2) {
    console.log('userId must be set');
    process.exit(0);
  }

  const nuser = {
    userId: process.argv[2],
    userName: process.argv[3],
    passStore: Auth.pass2store(process.argv[2], process.argv[4]),
    enabled: true,
    webEnabled: true,
    headerAuthEnabled: false,
    emailSearch: false,
    removeEnabled: false,
    packetSearch: false,
    welcomeMsgNum: 0,
    settings: {}
  };

  const roles = new Set();
  for (let i = 5; i < process.argv.length; i++) {
    switch (process.argv[i]) {
    case '--admin':
    case '-admin':
      roles.add('superAdmin');
      break;

    case '--remove':
    case '-remove':
      nuser.removeEnabled = true;
      break;

    case '--noweb':
    case '-noweb':
    case '--apionly':
      nuser.webEnabled = false;
      break;

    case '--webauthonly':
    case '-webauthonly':
      nuser.headerAuthEnabled = true;
      nuser.passStore = Auth.pass2store(process.argv[2], cryptoLib.randomBytes(48));
      break;

    case '--webauth':
    case '-webauth':
      nuser.headerAuthEnabled = true;
      break;

    case '--email':
    case '-email':
      nuser.emailSearch = true;
      break;

    case '--expression':
    case '-expression':
      nuser.expression = process.argv[i + 1];
      i++;
      break;

    case '--packetSearch':
    case '-packetSearch':
      nuser.packetSearch = true;
      break;

    case '--createOnly':
    case '-createOnly':
      nuser._createOnly = true;
      break;

    case '--roles':
    case '-roles':
      process.argv[i + 1].split(',').forEach(r => roles.add(r));
      i++;
      break;

    default:
      console.log('Unknown option', process.argv[i]);
      help();
    }
  }

  if (roles.size === 0) {
    roles.add('arkimeUser');
    roles.add('cont3xtUser');
    roles.add('parliamentUser');
    roles.add('wiseUser');
  }

  nuser.roles = [...roles];

  User.setUser(process.argv[2], nuser, (err, info) => {
    if (err) {
      if (err.meta.body.error.type === 'version_conflict_engine_exception') {
        console.log('User already exists');
      } else {
        console.log('OpenSearch/Elasticsearch error', JSON.stringify(err, false, 2));
      }
    } else {
      console.log('Added');
    }
    if (Config.nodeName() !== 'cont3xt') {
      Db.close();
    }
  });
}

if (process.argv.length < 5) {
  help();
}

async function premain () {
  await Config.initialize();

  if (Config.nodeName() === 'cont3xt') {
    const usersUrl = Config.get('usersUrl');
    const usersEs = Config.getArray('usersElasticsearch', Config.get('elasticsearch', 'http://localhost:9200'));
    User.initialize({
      insecure: ArkimeConfig.insecure,
      requestTimeout: Config.get('elasticsearchTimeout', 300),
      url: usersUrl,
      node: usersEs,
      caTrustFile: Config.get('caTrustFile'),
      clientKey: Config.get('esClientKey'),
      clientCert: Config.get('esClientCert'),
      clientKeyPass: Config.get('esClientKeyPass'),
      prefix: Config.get('usersPrefix'),
      apiKey: Config.get('usersElasticsearchAPIKey'),
      basicAuth: Config.get('usersElasticsearchBasicAuth', Config.get('elasticsearchBasicAuth')),
      noUsersCheck: true
    });
    main();
  } else {
    const escInfo = Config.getArray('elasticsearch', 'http://localhost:9200');
    Db.initialize({
      host: escInfo,
      prefix: Config.get('prefix', 'arkime_'),
      esClientKey: Config.get('esClientKey', null),
      esClientCert: Config.get('esClientCert', null),
      esClientKeyPass: Config.get('esClientKeyPass', null),
      insecure: ArkimeConfig.insecure,
      caTrustFile: Config.getFull(Config.nodeName(), 'caTrustFile'),
      usersHost: Config.getArray('usersElasticsearch'),
      usersPrefix: Config.get('usersPrefix'),
      esApiKey: Config.get('elasticsearchAPIKey', null),
      usersEsApiKey: Config.get('usersElasticsearchAPIKey', null),
      esBasicAuth: Config.get('elasticsearchBasicAuth', null),
      usersEsBasicAuth: Config.get('usersElasticsearchBasicAuth', null),
      noUsersCheck: true
    }, main);
  }
}
premain();
