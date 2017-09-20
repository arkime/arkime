/******************************************************************************/
/* addUser.js -- Create a new user in the database
 *
 * addUser.js <user id> <user friendly name> <password> [-noweb] [-admin]
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
var Config = require("./config.js");
var Db = require ("./db.js");
var crypto = require('crypto');

var escInfo = Config.get("elasticsearch", "http://localhost:9200").split(",");
function help() {
  console.log("addUser.js [<config options>] <user id> <user friendly name> <password> [<options>]");
  console.log("");
  console.log("Options:");
  console.log("  --admin               Has admin privileges");
  console.log("  --apionly             Can only use api, not web pages");
  console.log("  --email               Can do email searches");
  console.log("  --expression  <expr>  Forced user expression");
  console.log("  --remove              Can remove data (scrub, delete tags)");
  console.log("  --webauth             Can auth using the web auth header or password");
  console.log("  --webauthonly         Can auth using the web auth header only, password ignored");
  console.log("");
  console.log("Config Options:");
  console.log("  -c <config file>      Config file to use");
  console.log("  -n <node name>        Node name section to use in config file");

  process.exit(0);
}

function main() {

  if (process.argv[2].length < 2) {
    console.log("userId must be set");
    process.exit(0);
  }

  var nuser = {
    userId: process.argv[2],
    userName: process.argv[3],
    passStore: Config.pass2store(process.argv[2], process.argv[4]),
    enabled: true,
    webEnabled: true,
    headerAuthEnabled: false,
    emailSearch: false,
    createEnabled: false,
    removeEnabled: false,
    settings: {}
  };

  var i;
  for (i = 5; i < process.argv.length; i++) {
    switch(process.argv[i]) {
    case "--admin":
    case "-admin":
      nuser.createEnabled = true;
      break;

    case "--remove":
    case "-remove":
      nuser.removeEnabled = true;
      break;

    case "--noweb":
    case "-noweb":
    case "--apionly":
      nuser.webEnabled = false;
      break;
      
    case "--webauthonly":
    case "-webauthonly":
      nuser.passStore = Config.pass2store(process.argv[2], crypto.randomBytes(48));
    case "--webauth":
    case "-webauth":
      nuser.headerAuthEnabled = true;
      break;

    case "--email":
    case "-email":
      nuser.emailSearch = true;
      break;

    case "--expression":
    case "-expression":
      nuser.expression = process.argv[i+1];
      i++;
      break;

    default:
      console.log("Unknown option", process.argv[i]);
      help();
    }
  }

  Db.setUser(process.argv[2], nuser, (err, info) => {
    if (err) {
      console.log("Elastic search error", err);
    } else {
      console.log("Added");
    }
    Db.close();
  });
}

if (process.argv.length < 5) {
  help();
}

Db.initialize({host : escInfo,
               prefix: Config.get("prefix", ""),
               usersHost: Config.get("usersElasticsearch"),
               usersPrefix: Config.get("usersPrefix")}, main);
