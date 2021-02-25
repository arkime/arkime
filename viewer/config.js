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
const crypto = require('crypto');
const version = require('./version');

exports.debug = 0;
exports.insecure = false;
exports.esProfile = false;
const internals = {
  configFile: `${version.config_prefix}/etc/config.ini`,
  hostName: os.hostname(),
  fields: [],
  fieldsMap: {},
  categories: {},
  options: {},
  debugged: {}
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

      internals.options[process.argv[i].slice(0, equal)] = process.argv[i].slice(equal + 1);
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
// Encryption stuff
/// ///////////////////////////////////////////////////////////////////////////////
exports.md5 = function (str, encoding) {
  return crypto
    .createHash('md5')
    .update(str)
    .digest(encoding || 'hex');
};

// Hash (MD5) and encrypt the password before storing.
// Encryption is used because ES is insecure by default and we don't want others adding accounts.
exports.pass2store = function (userid, password) {
  // md5 is required because of http digest
  const m = exports.md5(userid + ':' + exports.getFull('default', 'httpRealm', 'Moloch') + ':' + password);

  if (internals.aes256Encryption) {
    // New style with IV: IV.E
    const iv = crypto.randomBytes(16);
    const c = crypto.createCipheriv('aes-256-cbc', internals.passwordSecret256, iv);
    let e = c.update(m, 'binary', 'hex');
    e += c.final('hex');
    return iv.toString('hex') + '.' + e;
  } else {
    // Old style without IV: E
    // eslint-disable-next-line node/no-deprecated-api
    const c = crypto.createCipher('aes192', internals.passwordSecret);
    let e = c.update(m, 'binary', 'hex');
    e += c.final('hex');
    return e;
  }
};

// Decrypt the encrypted hashed password, it is still hashed
exports.store2ha1 = function (passstore) {
  try {
    const parts = passstore.split('.');
    if (parts.length === 2) {
      // New style with IV: IV.E
      const c = crypto.createDecipheriv('aes-256-cbc', internals.passwordSecret256, Buffer.from(parts[0], 'hex'));
      let d = c.update(parts[1], 'hex', 'binary');
      d += c.final('binary');
      return d;
    } else {
      // Old style without IV: E
      // eslint-disable-next-line node/no-deprecated-api
      const c = crypto.createDecipher('aes192', internals.passwordSecret);
      let d = c.update(passstore, 'hex', 'binary');
      d += c.final('binary');
      return d;
    }
  } catch (e) {
    console.log("passwordSecret set in the [default] section can not decrypt information.  You may need to re-add users if you've changed the secret.", e);
    process.exit(1);
  }
};

// Encrypt an object into an auth string
exports.obj2auth = function (obj, c2s, secret) {
  if (internals.aes256Encryption) {
    // New style with IV: IV.E.H
    if (secret) {
      secret = crypto.createHash('sha256').update(secret).digest();
    } else {
      secret = internals.serverSecret256;
    }

    const iv = crypto.randomBytes(16);
    const c = crypto.createCipheriv('aes-256-cbc', secret, iv);
    let e = c.update(JSON.stringify(obj), 'binary', 'hex');
    e += c.final('hex');
    e = iv.toString('hex') + '.' + e;
    const h = crypto.createHmac('sha256', secret).update(e).digest('hex');
    return e + '.' + h;
  } else {
    // Old style without IV: E or E.H
    secret = secret || internals.serverSecret;

    // eslint-disable-next-line node/no-deprecated-api
    const c = crypto.createCipher('aes192', secret);
    let e = c.update(JSON.stringify(obj), 'binary', 'hex');
    e += c.final('hex');

    const h = crypto.createHmac('sha256', secret).update(e, 'hex').digest('hex');

    // include sig if c2s or s2sSignedAuth
    if (c2s || internals.s2sSignedAuth) {
      return e + '.' + h;
    }

    return e;
  }
};

// Decrypt the auth string into an object
exports.auth2obj = function (auth, c2s, secret) {
  const parts = auth.split('.');

  if (parts.length === 3) {
    // New style with IV: IV.E.H
    if (secret) {
      secret = crypto.createHash('sha256').update(secret).digest();
    } else {
      secret = internals.serverSecret256;
    }

    const signature = Buffer.from(parts[2], 'hex');
    const h = crypto.createHmac('sha256', secret).update(parts[0] + '.' + parts[1]).digest();

    if (!crypto.timingSafeEqual(signature, h)) {
      throw new Error('Incorrect signature');
    }

    try {
      const c = crypto.createDecipheriv('aes-256-cbc', secret, Buffer.from(parts[0], 'hex'));
      let d = c.update(parts[1], 'hex', 'binary');
      d += c.final('binary');
      return JSON.parse(d);
    } catch (error) {
      console.log(error);
      throw new Error('Incorrect auth supplied');
    }
  } else {
    // Old style without IV: E or E.H

    secret = secret || internals.serverSecret;

    // if sig missing error if c2s or s2sSignedAuth
    if (parts.length === 1 && (c2s || internals.s2sSignedAuth)) {
      throw new Error('Missing signature');
    }

    if (parts.length > 1) {
      const signature = Buffer.from(parts[1], 'hex');
      const h = crypto.createHmac('sha256', secret).update(parts[0], 'hex').digest();

      if (!crypto.timingSafeEqual(signature, h)) {
        throw new Error('Incorrect signature');
      }
    }

    try {
      // eslint-disable-next-line node/no-deprecated-api
      const c = crypto.createDecipher('aes192', secret);
      let d = c.update(parts[0], 'hex', 'binary');
      d += c.final('binary');
      return JSON.parse(d);
    } catch (error) {
      console.log(error);
      throw new Error('Incorrect auth supplied');
    }
  }
};

/// ///////////////////////////////////////////////////////////////////////////////
// Config File & Dropping Privileges
/// ///////////////////////////////////////////////////////////////////////////////

if (!fs.existsSync(internals.configFile)) {
  console.log("ERROR - Couldn't open config file '" + internals.configFile + "' maybe use the -c <configfile> option");
  process.exit(1);
}
internals.config = ini.parseSync(internals.configFile);

if (internals.config.default === undefined) {
  console.log('ERROR - [default] section missing from', internals.configFile);
  process.exit(1);
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
  if (internals.options[key] !== undefined && (node === 'default' || node === internals.nodeName)) {
    value = internals.options[key];
  } else if (internals.config[node] && internals.config[node][key] !== undefined) {
    value = internals.config[node][key];
  } else if (internals.config[node] && internals.config[node].nodeClass && internals.config[internals.config[node].nodeClass] && internals.config[internals.config[node].nodeClass][key]) {
    value = internals.config[internals.config[node].nodeClass][key];
  } else if (internals.config.default[key]) {
    value = internals.config.default[key];
  } else {
    value = defaultValue;
  }

  if (exports.debug > 0 && internals.debugged[node + '::' + key] === undefined) {
    console.log(`CONFIG - ${key} on node ${node} is ${value}`);
    internals.debugged[node + '::' + key] = 1;
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
  return exports.get(key, defaultValue).split(separator).map(s => s.trim()).filter(s => s.match(/^\S+$/));
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

  if (caTrustFile && caTrustFile.length > 0) {
    const certs = [];

    const caTrustFileLines = fs.readFileSync(caTrustFile, 'utf8').split('\n');

    let foundCert = [];

    for (let i = 0, ilen = caTrustFileLines.length; i < ilen; i++) {
      const line = caTrustFileLines[i];
      if (line.length === 0) {
        continue;
      }
      foundCert.push(line);
      if (line.match(/-END CERTIFICATE-/)) {
        certs.push(foundCert.join('\n'));
        foundCert = [];
      }
    }

    if (certs.length > 0) {
      return certs;
    }
  }

  return undefined;
};

function loadIncludes (includes) {
  if (!includes) {
    return;
  }
  includes.split(';').forEach((file) => {
    if (!fs.existsSync(file)) {
      console.log("ERROR - Couldn't open config includes file '" + file + "'");
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

exports.configMap = function (section, name, d) {
  const data = internals.config[section] || d;
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

if (exports.isHTTPS()) {
  exports.keyFileData = fs.readFileSync(exports.get('keyFile'));
  exports.certFileData = fs.readFileSync(exports.get('certFile'));
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
// Globals
/// ///////////////////////////////////////////////////////////////////////////////
internals.s2sSignedAuth = exports.getFull('default', 's2sSignedAuth', true);
internals.aes256Encryption = exports.getFull('default', 'aes256Encryption', true);

// If passwordSecret isn't set, viewer will treat accounts as anonymous
internals.passwordSecret = exports.getFull('default', 'passwordSecret', 'password');
internals.passwordSecret256 = crypto.createHash('sha256').update(internals.passwordSecret).digest();

if (exports.getFull('default', 'serverSecret')) {
  internals.serverSecret = exports.getFull('default', 'serverSecret');
  internals.serverSecret256 = crypto.createHash('sha256').update(internals.serverSecret).digest();
} else {
  internals.serverSecret = internals.passwordSecret;
  internals.serverSecret256 = internals.passwordSecret256;
}
