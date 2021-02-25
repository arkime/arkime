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

'use strict';
const Config = require('./config.js');
const Db = require('./db.js');
const crypto = require('crypto');
const fs = require('fs');

const escInfo = Config.getArray('elasticsearch', ',', 'http://localhost:9200');

function main () {
  const query = { size: 100, query: { term: { name: process.argv[2] } }, sort: [{ num: { order: 'desc' } }] };
  Db.search('files', 'file', query, (err, data) => {
    if (err) {
      console.error('ES Error', err);
      process.exit();
    }
    if (data.hits.hits.length === 0) {
      console.error('No matches');
      process.exit();
    }
    const info = data.hits.hits[0]._source;
    if (!info.encoding || info.encoding === 'normal') {
      console.error('Not encrypted');
      process.exit();
    }

    // Get the kek
    const kek = Config.sectionGet('keks', info.kekId, undefined);
    if (kek === undefined) {
      console.error("ERROR - Couldn't find kek", info.kekId, 'in keks section');
      process.exit();
    }

    // Decrypt the dek
    // eslint-disable-next-line node/no-deprecated-api
    const kdecipher = crypto.createDecipher('aes-192-cbc', kek);
    const encKey = Buffer.concat([kdecipher.update(Buffer.from(info.dek, 'hex')), kdecipher.final()]);

    // Setup IV
    const iv = Buffer.alloc(16);
    Buffer.from(info.iv, 'hex').copy(iv);

    // Setup streams
    const r = fs.createReadStream(process.argv[2]);
    const d = crypto.createDecipheriv(info.encoding, encKey, iv);
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

Db.initialize({
  host: escInfo,
  prefix: Config.get('prefix', ''),
  esClientKey: Config.get('esClientKey', null),
  esClientCert: Config.get('esClientCert', null),
  esClientKeyPass: Config.get('esClientKeyPass', null),
  insecure: Config.insecure,
  usersHost: Config.get('usersElasticsearch'),
  usersPrefix: Config.get('usersPrefix')
}, main);
