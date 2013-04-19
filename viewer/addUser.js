/******************************************************************************/
/* addUser.js -- Create a new user in the database
 *
 * addUser.js <user id> <user friendly name> <password> [-noweb] [-admin]
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
var Config = require("./config.js");
var Db = require ("./db.js");
var crypto = require('crypto');

var escInfo = Config.get("elasticsearch", "localhost:9200").split(':');
Db.initialize({host : escInfo[0], port: escInfo[1]});

if (process.argv.length < 4) {
  console.log("addUser.js <user id> <user friendly name> <password> [-noweb] [-admin] [-email]");
  process.exit(0);
}

var nuser = {
  userId: process.argv[2],
  userName: process.argv[3],
  passStore: Config.pass2store(process.argv[2], process.argv[4]),
  enabled: true,
  webEnabled: true,
  emailSearch: false,
  createEnabled: false
};

var i;
for (i = 0; i < process.argv.length; i++) {
  if (process.argv[i] === "-admin") {
    nuser.createEnabled = true;
  }
  if (process.argv[i] === "-noweb") {
    nuser.webEnabled = false;
  }
  if (process.argv[i] === "-email") {
    nuser.emailSearch = true;
  }
}

console.log(nuser);
Db.indexNow("users", "user", process.argv[2], nuser, function(err, info) {
  console.log("add user", err, info);
});
