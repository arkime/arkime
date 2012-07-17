/******************************************************************************/
/* addUser.js -- Create a new user in the database
 *
 * addUser.js <user id> <user friendly name> <password> [-noweb] [-admin]
 *
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
  console.log("addUser.js <user id> <user friendly name> <password> [-noweb] [-admin]");
  process.exit(0);
}

var nuser = {
  userId: process.argv[2],
  userName: process.argv[3],
  passStore: Config.pass2store(process.argv[2], process.argv[4]),
  enabled: true,
  webEnabled: true,
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
}

console.log(nuser);
Db.indexNow("users", "user", process.argv[2], nuser, function(err, info) {
  console.log("add user", err, info);
});
