/******************************************************************************/
/* decryptPcap.js -- decrypt an entire pcap file to stdout
 *
 * decryptPcap.js [options like -c/-n] <full path filename>
 *
 * Copyright 2020 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';
const Config = require('./config.js');
const Db = require('./db.js');
const cryptoLib = require('crypto');
const fs = require('fs');
const ArkimeConfig = require('../common/arkimeConfig');

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
    // eslint-disable-next-line n/no-deprecated-api
    const kdecipher = cryptoLib.createDecipher('aes-192-cbc', kek);
    const encKey = Buffer.concat([kdecipher.update(Buffer.from(info.dek, 'hex')), kdecipher.final()]);

    const r = fs.createReadStream(process.argv[2]);

    if (info.encoding === 'aes-256-ctr') {
      // Setup IV
      const iv = Buffer.alloc(16);
      Buffer.from(info.iv, 'hex').copy(iv);

      // Setup streams
      const d = cryptoLib.createDecipheriv(info.encoding, encKey, iv);
      d.on('end', () => {
        process.exit();
      });

      // Doit
      r.pipe(d).pipe(process.stdout);
    } else if (info.encoding === 'xor-2048') {
      let pos = 0;
      r.on('data', (chunk) => {
        for (let i = 0; i < chunk.length; i++, pos++) {
          chunk[i] ^= encKey[pos % 256];
        }
        process.stdout.write(chunk);
      });
      r.on('end', () => {
        process.exit();
      });
    } else {
      console.log('Unknown encoding', info.encoding);
    }
  });
}

if (process.argv.length < 3) {
  console.log('Missing full path filename');
  process.exit();
}

async function premain () {
  await Config.initialize();

  const escInfo = Config.getArray('elasticsearch', 'http://localhost:9200');
  Db.initialize({
    host: escInfo,
    prefix: Config.get('prefix', 'arkime_'),
    queryExtraIndices: Config.getArray('queryExtraIndices', ''),
    esClientKey: Config.get('esClientKey', null),
    esClientCert: Config.get('esClientCert', null),
    esClientKeyPass: Config.get('esClientKeyPass', null),
    insecure: ArkimeConfig.insecure,
    usersHost: Config.getArray('usersElasticsearch'),
    usersPrefix: Config.get('usersPrefix'),
    esApiKey: Config.get('elasticsearchAPIKey', null),
    usersEsApiKey: Config.get('usersElasticsearchAPIKey', null),
    esBasicAuth: Config.get('elasticsearchBasicAuth', null),
    usersEsBasicAuth: Config.get('usersElasticsearchBasicAuth', null),
    noUsersCheck: true
  }, main);
}

premain();
