/******************************************************************************/
/* config.js -- Code dealing with the config file, command line arguments,
 *              and dropping privileges
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

/// ///////////////////////////////////////////////////////////////////////////////
/// / Command Line Parsing
/// ///////////////////////////////////////////////////////////////////////////////
const ini = require('iniparser');
const os = require('os');
const fs = require('fs');
const version = require('../common/version');
const Auth = require('../common/auth');
const ArkimeUtil = require('../common/arkimeUtil');

exports.debug = 0;
exports.insecure = false;
exports.esProfile = false;
const internals = {
  configFile: `${version.config_prefix}/etc/config.ini`,
  hostName: os.hostname(),
  fields: [],
  fieldsMap: {},
  categories: {},
  options: new Map(),
  debugged: new Map()
};

function processArgs () {
  const args = [];
  for (let i = 0, ilen = process.argv.length; i < ilen; i++) {
    if (process.argv[i] === '-c') {
      i++;
      internals.configFile = process.argv[i];
    } else if (process.argv[i] === '--host') {
      i++;
      internals.hostName = process.argv[i];
    } else if (process.argv[i] === '-n') {
      i++;
      internals.nodeName = process.argv[i];
    } else if (process.argv[i] === '-o' || process.argv[i] === '--option') {
      i++;
      const equal = process.argv[i].indexOf('=');
      if (equal === -1) {
        console.log('Missing equal sign in', process.argv[i]);
        process.exit(1);
      }

      internals.options.set(process.argv[i].slice(0, equal), process.argv[i].slice(equal + 1));
    } else if (process.argv[i] === '--debug') {
      exports.debug++;
    } else if (process.argv[i] === '--insecure') {
      exports.insecure = true;
    } else if (process.argv[i] === '--esprofile') {
      exports.esProfile = true;
    } else {
      args.push(process.argv[i]);
    }
  }
  process.argv = args;

  if (!internals.nodeName) {
    internals.nodeName = internals.hostName.split('.')[0];
  }

  if (exports.debug > 0) {
    console.log('Debug Level', exports.debug);
  }
}
processArgs();

/// ///////////////////////////////////////////////////////////////////////////////
// Config File & Dropping Privileges
/// ///////////////////////////////////////////////////////////////////////////////

if (!fs.existsSync(internals.configFile)) {
  console.log("ERROR - Couldn't open config file '%s' maybe use the -c <configfile> option", internals.configFile);
  process.exit(1);
}
internals.config = ini.parseSync(internals.configFile);

if (internals.config.default === undefined) {
  if (internals.nodeName !== 'cont3xt') {
    console.log('ERROR - [default] section missing from', internals.configFile);
    process.exit(1);
  }
  internals.config.default = {};
}

exports.sectionGet = function (section, key, defaultValue) {
  let value;

  if (internals.config[section] && internals.config[section][key] !== undefined) {
    value = internals.config[section][key];
  } else {
    value = defaultValue;
  }

  if (value === 'false') {
    return false;
  }

  return value;
};

exports.getFull = function (node, key, defaultValue) {
  let value;
  if (internals.options.has(key) && (node === 'default' || node === internals.nodeName)) {
    value = internals.options.get(key);
  } else if (internals.config[node] && internals.config[node][key] !== undefined) {
    value = internals.config[node][key];
  } else if (internals.config[node] && internals.config[node].nodeClass && internals.config[internals.config[node].nodeClass] && internals.config[internals.config[node].nodeClass][key]) {
    value = internals.config[internals.config[node].nodeClass][key];
  } else if (internals.config.default[key] !== undefined) {
    value = internals.config.default[key];
  } else {
    value = defaultValue;
  }

  if (exports.debug > 0 && !internals.debugged.has(node + '::' + key)) {
    console.log(`CONFIG - ${key} on node ${node} is ${value}`);
    internals.debugged.set(node + '::' + key, true);
  }

  if (value === 'false') {
    return false;
  }
  return value;
};

exports.get = function (key, defaultValue) {
  return exports.getFull(internals.nodeName, key, defaultValue);
};

exports.getBoolFull = function (node, key, defaultValue) {
  const value = exports.getFull(node, key);
  if (value !== undefined) {
    if (value === 'true' || value === '1') {
      return true;
    } else if (value === 'false' || value === '0') {
      return false;
    } else {
      console.log('ERROR - invalid value for ', key);
    }
  }
  return defaultValue;
};

exports.getBool = function (key, defaultValue) {
  return exports.getBoolFull(internals.nodeName, key, defaultValue);
};

// Return an array split on separator, remove leading/trailing spaces, remove empty elements
exports.getArray = function (key, separator, defaultValue) {
  const value = exports.get(key, defaultValue);
  if (typeof value === 'string') {
    return value.split(separator).map(s => s.trim()).filter(s => s.match(/^\S+$/));
  } else {
    return value;
  }
};

exports.getObj = function (key, defaultValue) {
  const full = exports.getFull(internals.nodeName, key, defaultValue);
  if (!full) {
    return null;
  }

  const obj = {};
  full.split(';').forEach((element) => {
    const parts = element.split('=');
    if (parts && parts.length === 2) {
      if (parts[1] === 'true') {
        parts[1] = true;
      } else if (parts[1] === 'false') {
        parts[1] = false;
      }
      obj[parts[0]] = parts[1];
    }
  });
  return obj;
};

exports.getCaTrustCerts = function (node) {
  const caTrustFile = exports.getFull(node, 'caTrustFile');
  return ArkimeUtil.certificateFileToArray(caTrustFile);
};

function loadIncludes (includes) {
  if (!includes) {
    return;
  }
  includes.split(';').forEach((file) => {
    if (!fs.existsSync(file)) {
      console.log("ERROR - Couldn't open config includes file '%s'", file);
      process.exit(1);
    }
    const config = ini.parseSync(file);
    for (const group in config) {
      if (!internals.config[group]) {
        internals.config[group] = config[group];
      } else {
        for (const key in config[group]) {
          internals.config[group][key] = config[group][key];
        }
      }
    }
  });
}

loadIncludes(exports.get('includes', null));

function dropPrivileges () {
  if (process.getuid() !== 0) {
    return;
  }

  const group = exports.get('dropGroup', null);
  if (group !== null) {
    process.setgid(group);
  }

  const user = exports.get('dropUser', null);
  if (user !== null) {
    process.setuid(user);
  }
}

exports.getConfigFile = function () {
  return internals.configFile;
};

exports.isHTTPS = function (node) {
  return exports.getFull(node || internals.nodeName, 'keyFile') &&
         exports.getFull(node || internals.nodeName, 'certFile');
};

exports.basePath = function (node) {
  return exports.getFull(node || internals.nodeName, 'webBasePath', '/');
};

exports.nodeName = function () {
  return internals.nodeName;
};

exports.hostName = function () {
  return internals.hostName;
};

exports.arkimeWebURL = () => {
  let webUrl = exports.get('arkimeWebURL', `${exports.hostName()}${exports.basePath()}`);
  if (!webUrl.startsWith('http')) {
    webUrl = exports.isHTTPS() ? `https://${webUrl}` : `http://${webUrl}`;
  }
  return webUrl;
};

exports.keys = function (section) {
  if (internals.config[section] === undefined) { return []; }
  return Object.keys(internals.config[section]);
};

exports.headers = function (section) {
  if (internals.config[section] === undefined) { return []; }
  const keys = Object.keys(internals.config[section]);
  if (!keys) { return []; }
  const headers = Object.keys(internals.config[section]).map((key) => {
    const obj = { name: key };
    internals.config[section][key].split(';').forEach((element) => {
      const i = element.indexOf(':');
      if (i === -1) {
        return;
      }

      const parts = [element.slice(0, i), element.slice(i + 1)];
      if (parts[1] === 'true') {
        parts[1] = true;
      } else if (parts[1] === 'false') {
        parts[1] = false;
      }
      obj[parts[0]] = parts[1];
    });
    return obj;
  });

  return headers;
};

exports.configMap = function (section, dSection, d) {
  const data = internals.config[section] || internals.config[dSection] || d;
  if (data === undefined) { return {}; }
  const keys = Object.keys(data);
  if (!keys) { return {}; }
  const map = {};
  keys.forEach((key) => {
    const obj = {};
    data[key].split(';').forEach((element) => {
      const i = element.indexOf(':');
      if (i === -1) {
        return;
      }

      const parts = [element.slice(0, i), element.slice(i + 1)];
      if (parts[1] === 'true') {
        parts[1] = true;
      } else if (parts[1] === 'false') {
        parts[1] = false;
      }
      obj[parts[0]] = parts[1];
    });
    map[key] = obj;
  });

  return map;
};

let fsWait = null;
let httpsServer = null;
let httpsCryptoOption = null;
function watchFile (e, filename) {
  if (!httpsServer || !httpsCryptoOption) {
    return;
  }

  if (filename) { // 10s timeout for file changes (including file name changes)
    if (fsWait) { clearTimeout(fsWait); };

    fsWait = setTimeout(() => {
      fsWait = null;
      try { // try to get the new cert files
        loadCertData();
      } catch (err) { // don't continue if we can't read them
        console.log('Missing cert or key files. Cannot reload cert.');
        return;
      }

      console.log('Reloading cert...');

      const options = { // set new server cert options
        key: exports.keyFileData,
        cert: exports.certFileData,
        secureOptions: httpsCryptoOption
      };

      try {
        httpsServer.setSecureContext(options);
      } catch (err) {
        console.log('ERROR cert not reloaded: ', err.toString());
      }
    }, 10000);
  }
}

exports.setServerToReloadCerts = function (server, cryptoOption) {
  if (!exports.isHTTPS()) { return; } // only used in https mode

  if (exports.debug > 0) {
    console.log('Watching cert and key files. If either is changed, the server will be updated with the new files.');
  }

  httpsServer = server;
  httpsCryptoOption = cryptoOption;
};

function loadCertData () {
  exports.keyFileLocation = exports.get('keyFile');
  exports.certFileLocation = exports.get('certFile');
  exports.keyFileData = fs.readFileSync(exports.keyFileLocation);
  exports.certFileData = fs.readFileSync(exports.certFileLocation);
}

if (exports.debug === 0) {
  exports.debug = parseInt(exports.get('debug', 0));
  if (exports.debug) {
    console.log('Debug Level', exports.debug);
  }
}

if (exports.isHTTPS()) {
  try {
    loadCertData();

    // watch the cert and key files
    fs.watch(exports.certFileLocation, { persistent: false }, watchFile);
    fs.watch(exports.keyFileLocation, { persistent: false }, watchFile);
  } catch (err) {
    console.log('ERROR loading cert or key files:', err.toString());
  }
}

dropPrivileges();

/// ///////////////////////////////////////////////////////////////////////////////
// Fields
/// ///////////////////////////////////////////////////////////////////////////////
exports.getFields = function () {
  return internals.fields;
};

exports.getFieldsMap = function () {
  return internals.fieldsMap;
};

exports.getDBFieldsMap = function () {
  return internals.dbFieldsMap;
};

exports.getDBField = function (field, property) {
  if (internals.dbFieldsMap[field] === undefined) { return undefined; }
  if (property === undefined) { return internals.dbFieldsMap[field]; }
  return internals.dbFieldsMap[field][property];
};

exports.getCategories = function () {
  return internals.categories;
};

exports.loadFields = function (data) {
  internals.fields = [];
  internals.fieldsMap = {};
  internals.dbFieldsMap = {};
  internals.categories = {};
  data.forEach((field) => {
    const source = field._source;
    source.exp = field._id;

    // Add some transforms
    if (!source.transform) {
      if (source.exp === 'http.uri' || source.exp === 'http.uri.tokens') {
        source.transform = 'removeProtocol';
      }
      if (source.exp === 'host' || source.exp.startsWith('host.')) {
        source.transform = 'removeProtocolAndURI';
      }
    }

    internals.fieldsMap[field._id] = source;
    internals.dbFieldsMap[source.dbField] = source;
    if (source.dbField2 !== undefined) {
      internals.dbFieldsMap[source.dbField2] = source;
    }
    if (source.fieldECS !== undefined) {
      internals.dbFieldsMap[source.fieldECS] = source;
      internals.fieldsMap[source.fieldECS] = source;
    }
    internals.fields.push(source);
    if (!internals.categories[source.group]) {
      internals.categories[source.group] = [];
    }
    internals.categories[source.group].push(source);
    (source.aliases || []).forEach((alias) => {
      internals.fieldsMap[alias] = source;
    });
  });

  function sortFunc (a, b) {
    return a.exp.localeCompare(b.exp);
  }

  for (const cat in internals.categories) {
    internals.categories[cat] = internals.categories[cat].sort(sortFunc);
  }
};

/// ///////////////////////////////////////////////////////////////////////////////
// Initialize Auth
/// ///////////////////////////////////////////////////////////////////////////////

let mode = 'anonymousWithDB';
if (exports.get('passwordSecret')) {
  const userNameHeader = exports.get('userNameHeader');
  if (!userNameHeader || userNameHeader === 'digest') {
    mode = 'digest';
  } else if (userNameHeader === 'oidc') {
    mode = 'oidc';
  } else {
    mode = 'header';
  }
} else if (exports.get('regressionTests')) {
  mode = 'regressionTests';
}

Auth.initialize({
  mode,
  debug: exports.debug,
  basePath: exports.basePath(),
  passwordSecret: exports.getFull(internals.nodeName === 'cont3xt' ? 'cont3xt' : 'default', 'passwordSecret', 'password'),
  serverSecret: exports.getFull('default', 'serverSecret'),
  userNameHeader: exports.get('userNameHeader'),
  requiredAuthHeader: exports.get('requiredAuthHeader'),
  requiredAuthHeaderVal: exports.get('requiredAuthHeaderVal'),
  userAutoCreateTmpl: exports.get('userAutoCreateTmpl'),
  userAuthIps: exports.get('userAuthIps'),
  s2s: true,
  s2sRegressionTests: !!exports.get('s2sRegressionTests'),
  caTrustCerts: exports.getCaTrustCerts(internals.nodeName),
  authConfig: {
    httpRealm: exports.get('httpRealm', 'Moloch'),
    userIdField: exports.get('authUserIdField'),
    discoverURL: exports.get('authDiscoverURL'),
    clientId: exports.get('authClientId'),
    clientSecret: exports.get('authClientSecret'),
    redirectURIs: exports.get('authRedirectURIs')
  }
});
