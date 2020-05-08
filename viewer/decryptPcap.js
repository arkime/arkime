/******************************************************************************/
/* decryptPcap.js -- decrypt an entire pcap file to stdout
 *
 * decryptPcap.js [options like -c/-n] <full path filename>
 *
 *
 * Copyright 2020 AOL Inc. All rights reserved.
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

/* jshint
  node: true, plusplus: false, curly: true, eqeqeq: true, immed: true, latedef: true, newcap: true, nonew: true, undef: true, strict: true, trailing: true
*/
'use strict';
var Config = require('./config.js');
var Db = require('./db.js');
var crypto = require('crypto');
var fs = require('fs');

var escInfo = Config.getArray('elasticsearch', ',', 'http://localhost:9200');

function main () {
  let query = { size: 100, query: { term: { name: process.argv[2] } }, sort: [{ num: { order: 'desc' } }] };
  Db.search('files', 'file', query, (err, data) => {
    if (err) {
      console.error('ES Error', err);
      process.exit();
    }
    if (data.hits.hits.length === 0) {
      console.error('No matches');
      process.exit();
    }
    let info = data.hits.hits[0]._source;
    if (!info.encoding || info.encoding === 'normal') {
      console.error('Not encrypted');
      process.exit();
    }

    // Get the kek
    let kek = Config.sectionGet('keks', info.kekId, undefined);
    if (kek === undefined) {
      console.error("ERROR - Couldn't find kek", info.kekId, 'in keks section');
      process.exit();
    }

    // Decrypt the dek
    let kdecipher = crypto.createDecipher('aes-192-cbc', kek);
    let encKey = Buffer.concat([kdecipher.update(Buffer.from(info.dek, 'hex')), kdecipher.final()]);

    // Setup IV
    let iv = Buffer.alloc(16);
    Buffer.from(info.iv, 'hex').copy(iv);

    // Setup streams
    let r = fs.createReadStream(process.argv[2]);
    let d = crypto.createDecipheriv(info.encoding, encKey, iv);
    d.on('end', function () {
      process.exit();
    });

    // Doit
    r.pipe(d).pipe(process.stdout);
  });
}

if (process.argv.length < 3) {
  console.log('Missing full path filename');
  process.exit();
}

Db.initialize({ host: escInfo,
  prefix: Config.get('prefix', ''),
  esClientKey: Config.get('esClientKey', null),
  esClientCert: Config.get('esClientCert', null),
  esClientKeyPass: Config.get('esClientKeyPass', null),
  insecure: Config.insecure,
  usersHost: Config.get('usersElasticsearch'),
  usersPrefix: Config.get('usersPrefix') }, main);
