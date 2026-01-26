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
  console.log('  --admin                   Has admin privileges');
  console.log('  --apionly                 Can only use api, not web pages');
  console.log('  --createOnly              Only create the user if it doesn\'t exist');
  console.log('  --disablePcapDownload     The user can see the pcap but not download it');
  console.log('  --no-disablePcapDownload  The user can download the pcap');
  console.log('  --email                   Can do email searches');
  console.log('  --no-email                Cannot do email searches');
  console.log('  --expression <expr>       Forced user expression');
  console.log('  --hideFiles               Hide the files page from this user');
  console.log('  --no-hideFiles            Show the files page to this user');
  console.log('  --hidePcap                Hide the pcap from this user, only metadata is shown');
  console.log('  --no-hidePcap             Show the pcap to this user');
  console.log('  --hideStats               Hide the stats page from this user');
  console.log('  --no-hideStats            Show the stats page to this user');
  console.log('  --packetSearch            Can create a packet search job (hunt)');
  console.log('  --no-packetSearch         Cannot create a packet search job (hunt)');
  console.log('  --remove                  Can remove data (scrub, delete tags)');
  console.log('  --no-remove               Cannot remove data (scrub, delete tags)');
  console.log('  --roles <roles>           Comma separated list of roles');
  console.log('  --timeLimit               Max time limit for searches in hours');
  console.log('  --webauth                 Can auth using the web auth header or password');
  console.log('  --webauthonly             Can auth using the web auth header only, password ignored');
  console.log();
  console.log('Config Options:');
  console.log('  -c, --config <file|url>   Where to fetch the config file from');
  console.log('  --insecure                Disable certificate verification for https calls');
  console.log('  -n <node name>            Node name section to use in config file');

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
      roles.add('superAdmin');
      break;

    case '--removeEnabled':
    case '--remove':
      nuser.removeEnabled = true;
      break;

    case '--no-remove':
      nuser.removeEnabled = false;
      break;

    case '--noweb':
    case '--apionly':
      nuser.webEnabled = false;
      break;

    case '--webauthonly':
      nuser.headerAuthEnabled = true;
      nuser.passStore = Auth.pass2store(process.argv[2], cryptoLib.randomBytes(48));
      break;

    case '--webauth':
      nuser.headerAuthEnabled = true;
      break;

    case '--email':
      nuser.emailSearch = true;
      break;

    case '--no-email':
      nuser.emailSearch = false;
      break;

    case '--expression':
      nuser.expression = process.argv[i + 1];
      i++;
      break;

    case '--packetSearch':
      nuser.packetSearch = true;
      break;

    case '--no-packetSearch':
      nuser.packetSearch = false;
      break;

    case '--createOnly':
      nuser._createOnly = true;
      break;

    case '--roles':
      process.argv[i + 1].split(',').forEach(r => roles.add(r));
      i++;
      break;

    case '--hideFiles':
      nuser.hideFiles = true;
      break;

    case '--no-hideFiles':
      nuser.hideFiles = false;
      break;

    case '--hidePcap':
      nuser.hidePcap = true;
      break;

    case '--no-hidePcap':
      nuser.hidePcap = false;
      break;

    case '--hideStats':
      nuser.hideStats = true;
      break;

    case '--no-hideStats':
      nuser.hideStats = false;
      break;

    case '--disablePcapDownload':
      nuser.disablePcapDownload = true;
      break;

    case '--no-disablePcapDownload':
      nuser.disablePcapDownload = false;
      break;

    case '--timeLimit':
      nuser.timeLimit = parseInt(process.argv[i + 1], 10);
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
  await Config.initialize({ initAuth: true });

  if (Config.nodeName() === 'cont3xt') {
    const usersUrl = Config.get('usersUrl');
    const usersEs = Config.getArray('usersElasticsearch', Config.get('elasticsearch', 'http://localhost:9200'));
    User.initialize({
      insecure: ArkimeConfig.isInsecure([usersUrl, usersEs]),
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
    await Db.initialize({
      host: escInfo,
      prefix: Config.get('prefix', 'arkime_'),
      esClientKey: Config.get('esClientKey', null),
      esClientCert: Config.get('esClientCert', null),
      esClientKeyPass: Config.get('esClientKeyPass', null),
      insecure: ArkimeConfig.isInsecure([escInfo, Config.getArray('usersElasticsearch')]),
      caTrustFile: Config.getFull(Config.nodeName(), 'caTrustFile'),
      usersHost: Config.getArray('usersElasticsearch'),
      usersPrefix: Config.get('usersPrefix'),
      esApiKey: Config.get('elasticsearchAPIKey', null),
      usersEsApiKey: Config.get('usersElasticsearchAPIKey', null),
      esBasicAuth: Config.get('elasticsearchBasicAuth', null),
      usersEsBasicAuth: Config.get('usersElasticsearchBasicAuth', null),
      noUsersCheck: true
    });
    main();
  }
}
premain();
