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
const ArkimeUtil = require('../common/arkimeUtil');

async function main () {
  const query = { size: 100, query: { term: { name: process.argv[2] } }, sort: [{ num: { order: 'desc' } }] };
  try {
    const data = await Db.search('files', query);
    if (data.hits.hits.length === 0) {
      console.error('No matches');
      process.exit(1);
    }
    const info = data.hits.hits[0]._source;
    if (!info.encoding || info.encoding === 'normal') {
      console.error('Not encrypted');
      process.exit(1);
    }

    // Get the kek
    const kek = Config.sectionGet('keks', info.kekId, undefined);
    if (kek === undefined) {
      console.error("ERROR - Couldn't find kek", info.kekId, 'in keks section');
      process.exit(1);
    }

    // Decrypt the dek
    let encKey;
    if (info.dekEncoding === 'aes-256-gcm') {
      encKey = ArkimeUtil.decryptDEKWithGCM(kek, Buffer.from(info.dek, 'hex'),
        Buffer.from(info.dekSalt, 'hex'), Buffer.from(info.dekIv, 'hex'), Buffer.from(info.dekTag, 'hex'));
    } else {
      const kdecipher = ArkimeUtil.createDecipherAES192NoIV(kek);
      encKey = Buffer.concat([kdecipher.update(Buffer.from(info.dek, 'hex')), kdecipher.final()]);
    }

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

      // Do it
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
      process.exit(1);
    }
  } catch (err) {
    console.error('ES Error', err);
    process.exit(1);
  }
}

if (process.argv.length < 3) {
  console.log('Missing full path filename');
  process.exit(1);
}

async function premain () {
  await Config.initialize();

  const escInfo = Config.getArray('elasticsearch', 'http://localhost:9200');
  await Db.initialize({
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
    esSigV4: Config.get('elasticsearchSigV4', false),
    esSigV4Region: Config.get('elasticsearchSigV4Region', null),
    esSigV4Service: Config.get('elasticsearchSigV4Service', null),
    esSigV4CredentialUrl: Config.get('elasticsearchSigV4CredentialUrl', null),
    esSigV4CredentialCert: Config.get('elasticsearchSigV4CredentialCert', null),
    esSigV4CredentialKey: Config.get('elasticsearchSigV4CredentialKey', null),
    esSigV4RoleArn: Config.get('elasticsearchSigV4RoleArn', null),
    esSigV4AccessKeyId: Config.get('elasticsearchSigV4AccessKeyId', null),
    esSigV4SecretAccessKey: Config.get('elasticsearchSigV4SecretAccessKey', null),
    esSigV4SessionToken: Config.get('elasticsearchSigV4SessionToken', null),
    usersEsSigV4: Config.get('usersElasticsearchSigV4', false),
    usersEsSigV4Region: Config.get('usersElasticsearchSigV4Region', null),
    usersEsSigV4Service: Config.get('usersElasticsearchSigV4Service', null),
    usersEsSigV4CredentialUrl: Config.get('usersElasticsearchSigV4CredentialUrl', null),
    usersEsSigV4CredentialCert: Config.get('usersElasticsearchSigV4CredentialCert', null),
    usersEsSigV4CredentialKey: Config.get('usersElasticsearchSigV4CredentialKey', null),
    usersEsSigV4RoleArn: Config.get('usersElasticsearchSigV4RoleArn', null),
    usersEsSigV4AccessKeyId: Config.get('usersElasticsearchSigV4AccessKeyId', null),
    usersEsSigV4SecretAccessKey: Config.get('usersElasticsearchSigV4SecretAccessKey', null),
    usersEsSigV4SessionToken: Config.get('usersElasticsearchSigV4SessionToken', null),
    noUsersCheck: true
  });
  main();
}

premain();
