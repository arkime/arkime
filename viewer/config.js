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
/*jshint
  node: true, plusplus: false, curly: true, eqeqeq: true, immed: true, latedef: true, newcap: true, nonew: true, undef: true, strict: true, trailing: true
*/
'use strict';

//////////////////////////////////////////////////////////////////////////////////
//// Command Line Parsing
//////////////////////////////////////////////////////////////////////////////////
var ini    = require('iniparser'),
    os     = require('os'),
    fs     = require('fs'),
    crypto = require('crypto');

exports.debug = 0;
var internals = {
    configFile: "/data/moloch/etc/config.ini",
    nodeName: os.hostname().split(".")[0],
    fields: [],
    fieldsMap: {},
    categories: {}
  };

function processArgs() {
  var args = [];
  for (var i = 0, ilen = process.argv.length; i < ilen; i++) {
    if (process.argv[i] === "-c") {
      i++;
      internals.configFile = process.argv[i];
    } else if (process.argv[i] === "-n") {
      i++;
      internals.nodeName = process.argv[i];
    } else if (process.argv[i] === "--debug") {
      exports.debug++;
    } else {
      args.push(process.argv[i]);
    }
  }
  process.argv = args;
}
processArgs();

//////////////////////////////////////////////////////////////////////////////////
// Encryption stuff
//////////////////////////////////////////////////////////////////////////////////
exports.md5 = function (str, encoding){
  return crypto
    .createHash('md5')
    .update(str)
    .digest(encoding || 'hex');
};

exports.pass2store = function(userid, password) {
  var m = exports.md5(userid + ":" + exports.getFull("default", "httpRealm", "Moloch") + ":" + password);
  var c = crypto.createCipher('aes192', exports.getFull("default", "passwordSecret", "password"));
  var e = c.update(m, "binary", "hex");
  e += c.final("hex");
  return e;
};

exports.store2ha1 = function(passstore) {
  try {
    var c = crypto.createDecipher('aes192', exports.getFull("default", "passwordSecret", "password"));
    var d = c.update(passstore, "hex", "binary");
    d += c.final("binary");
    return d;
  } catch (e) {
    console.log("passwordSecret set in the [default] section can not decrypt information.  You may need to re-add users if you've changed the secret.", e);
    process.exit(1);
  }
};

exports.obj2auth = function(obj, secret) {
  secret = secret || exports.getFull("default", "serverSecret") || exports.getFull("default", "passwordSecret", "password");
  var c = crypto.createCipher('aes192', secret);
  var e = c.update(JSON.stringify(obj), "binary", "hex");
  e += c.final("hex");
  return e;
};

exports.auth2obj = function(auth, secret) {
  secret = secret || exports.getFull("default", "serverSecret") || exports.getFull("default", "passwordSecret", "password");
  var c = crypto.createDecipher('aes192', secret);
  var d = c.update(auth, "hex", "binary");
  d += c.final("binary");
  return JSON.parse(d);
};

//////////////////////////////////////////////////////////////////////////////////
// Config File & Dropping Privileges
//////////////////////////////////////////////////////////////////////////////////

if (!fs.existsSync(internals.configFile)) {
  console.log("ERROR - Couldn't open config file '" + internals.configFile + "' maybe use the -c <configfile> option");
  process.exit(1);
}
internals.config = ini.parseSync(internals.configFile);


if (internals.config["default"] === undefined) {
  console.log("ERROR - [default] section missing from", internals.configFile);
  process.exit(1);
}

exports.sectionGet = function(section, key, defaultValue) {
  var value;

  if (internals.config[section] && internals.config[section][key] !== undefined ) {
    value = internals.config[section][key];
  } else {
    value = defaultValue;
  }

  if (value === "false") {
    return false;
  }

  return value;
}

exports.getFull = function(node, key, defaultValue) {
  var value;
  if (internals.config[node] && internals.config[node][key] !== undefined ) {
    value = internals.config[node][key];
  } else if (internals.config[node] && internals.config[node].nodeClass && internals.config[internals.config[node].nodeClass] && internals.config[internals.config[node].nodeClass][key]) {
    value = internals.config[internals.config[node].nodeClass][key];
  } else if (internals.config["default"][key]) {
    value = internals.config["default"][key];
  } else {
    value = defaultValue;
  }
  if (value === "false") {
    return false;
  }
  return value;
};

exports.get = function(key, defaultValue) {
  return exports.getFull(internals.nodeName, key, defaultValue);
};

exports.getObj = function(key, defaultValue) {
  var full = exports.getFull(internals.nodeName, key, defaultValue);
  if (!full) {
    return null;
  }

  var obj = {};
  full.split(';').forEach(function(element) {
    var parts = element.split("=");
    if (parts && parts.length === 2) {
      if (parts[1] === "true") {
        parts[1] = true;
      } else if (parts[1] === "false") {
        parts[1] = false;
      }
      obj[parts[0]] = parts[1];
    }
  });
  return obj;
};

function loadIncludes(includes) {
  if (!includes) {
    return;
  }
  includes.split(';').forEach(function(file) {
    if (!fs.existsSync(file)) {
      console.log("ERROR - Couldn't open config includes file '" + file + "'");
      process.exit(1);
    }
    var config = ini.parseSync(file);
    for (var group in config) {
      if (!internals.config[group]) {
        internals.config[group] = config[group];
      } else {
        for (var key in config[group]) {
          internals.config[group][key] = config[group][key];
        }
      }
    }
  });
}

loadIncludes(exports.get("includes", null));

function dropPrivileges() {
  if (process.getuid() !== 0) {
    return;
  }

  var group = exports.get("dropGroup", null);
  if (group !== null) {
    process.setgid(group);
  }

  var user = exports.get("dropUser", null);
  if (user !== null) {
    process.setuid(user);
  }
}

exports.getConfigFile = function() {
  return internals.configFile;
};

exports.isHTTPS = function(node) {
  return exports.getFull(node || internals.nodeName, "keyFile") &&
         exports.getFull(node || internals.nodeName, "certFile");
};

exports.basePath = function(node) {
  return exports.getFull(node || internals.nodeName, "webBasePath", "/");
};

exports.nodeName = function() {
  return internals.nodeName;
};

exports.keys = function(section) {
  if (internals.config[section] === undefined) {return [];}
  return Object.keys(internals.config[section]);
};

exports.headers = function(section) {
  if (internals.config[section] === undefined) {return [];}
  var keys = Object.keys(internals.config[section]);
  if (!keys) {return [];}
  var headers = Object.keys(internals.config[section]).map(function(key) {
    var obj = {name: key};
    internals.config[section][key].split(';').forEach(function(element) {
      var i = element.indexOf(':');
      if (i === -1) {
        return;
      }

      var parts = [element.slice(0, i), element.slice(i+1)];
      if (parts[1] === "true") {
        parts[1] = true;
      } else if (parts[1] === "false") {
        parts[1] = false;
      }
      obj[parts[0]] = parts[1];
    });
    return obj;
  });

  return headers;
};

exports.configMap = function(section, name, d) {
  var data = internals.config[section] || d;
  if (data === undefined) {return {};}
  var keys = Object.keys(data);
  if (!keys) {return {};}
  var map = {};
  keys.forEach(function(key) {
    var obj = {};
    data[key].split(';').forEach(function(element) {
      var i = element.indexOf(':');
      if (i === -1) {
        return;
      }

      var parts = [element.slice(0, i), element.slice(i+1)];
      if (parts[1] === "true") {
        parts[1] = true;
      } else if (parts[1] === "false") {
        parts[1] = false;
      }
      obj[parts[0]] = parts[1];
    });
    map[key] = obj;
  });

  return map;
};

if (exports.isHTTPS()) {
    exports.keyFileData = fs.readFileSync(exports.get("keyFile"));
    exports.certFileData = fs.readFileSync(exports.get("certFile"));
}
dropPrivileges();

//////////////////////////////////////////////////////////////////////////////////
// Fields
//////////////////////////////////////////////////////////////////////////////////
exports.getFields = function() {
  return internals.fields;
};

exports.getFieldsMap = function() {
  return internals.fieldsMap;
};

exports.getDBFieldsMap = function() {
  return internals.dbFieldsMap;
};

exports.getCategories = function() {
  return internals.categories;
};

exports.loadFields = function(data) {
  internals.fields = [];
  internals.fieldsMap = {};
  internals.dbFieldsMap = {};
  internals.categories =  {};
  data.forEach(function(field) {
    var source = field._source;
    source.exp = field._id;
    internals.fieldsMap[field._id] = source;
    internals.dbFieldsMap[source.dbField] = source;
    internals.fields.push(source);
    if (!internals.categories[source.group]) {
      internals.categories[source.group] = [];
    }
    internals.categories[source.group].push(source);
    (source.aliases || []).forEach(function(alias) {
      internals.fieldsMap[alias] = source;
    });
  });

  function sortFunc(a,b) {
    return a.exp.localeCompare(b.exp);
  }

  for (var cat in internals.categories) {
    internals.categories[cat] = internals.categories[cat].sort(sortFunc);
  }
};
