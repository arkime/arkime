/******************************************************************************/
/* config.js -- Code dealing with the config file, command line arguments, 
 *              and dropping privileges
 *
 * Copyright 2012-2013 AOL Inc. All rights reserved.
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
"use strict";

//////////////////////////////////////////////////////////////////////////////////
//// Command Line Parsing
//////////////////////////////////////////////////////////////////////////////////
var ini    = require('iniparser'),
    os     = require('os'),
    crypto = require('crypto');

var internals = {
    configFile: "/data/moloch/etc/config.ini",
    nodeName: os.hostname().substring(0, os.hostname().indexOf('.'))
  };

function processArgs() {
  var i;
  var args = [];
  for (i = 0; i < process.argv.length; i++) {
    if (process.argv[i] === "-c") {
      i++;
      internals.configFile = process.argv[i];
    } else if (process.argv[i] === "-n") {
      i++;
      internals.nodeName = process.argv[i];
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
  var c = crypto.createDecipher('aes192', exports.getFull("default", "passwordSecret", "password"));
  var d = c.update(passstore, "hex", "binary");
  d += c.final("binary");
  return d;
};

exports.obj2auth = function(obj) {
  var c = crypto.createCipher('aes192', exports.getFull("default", "passwordSecret", "password"));
  var e = c.update(JSON.stringify(obj), "binary", "hex");
  e += c.final("hex");
  return e;
};

exports.auth2obj = function(auth) {
  var c = crypto.createDecipher('aes192', exports.getFull("default", "passwordSecret", "password"));
  var d = c.update(auth, "hex", "binary");
  d += c.final("binary");
  return JSON.parse(d);
};

//////////////////////////////////////////////////////////////////////////////////
// Config File & Dropping Privileges
//////////////////////////////////////////////////////////////////////////////////

internals.config = ini.parseSync(internals.configFile);


if (internals.config["default"] === undefined) {
  console.log("ERROR - [default] section missing from", internals.configFile);
  process.exit(1);
}

exports.getFull = function(node, key, defaultValue) {
  if (internals.config[node] && internals.config[node][key] !== undefined ) {
    return internals.config[node][key];
  }

  if (internals.config[node] && internals.config[node].nodeClass && internals.config[internals.config[node].nodeClass] && internals.config[internals.config[node].nodeClass][key]) {
    return internals.config[internals.config[node].nodeClass][key];
  }

  if (internals.config["default"][key]) {
    return internals.config["default"][key];
  }

  return defaultValue;
};

exports.get = function(key, defaultValue) {
    return exports.getFull(internals.nodeName, key, defaultValue);
};

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

dropPrivileges();
