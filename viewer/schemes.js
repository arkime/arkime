/******************************************************************************/
/* schemes.js -- schemes for file loading
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const internals = require('./internals.js');
const http = require('http');
const axios = require('axios');
const LRU = require('lru-cache');
// const fs = require('fs');
const Config = require('./config.js');
const { GetObjectCommand, S3 } = require('@aws-sdk/client-s3');

const blocklru = new LRU({ max: 100 });
const S3s = new Map();

const httpAgent = new http.Agent({ family: 4 });

// --------------------------------------------------------------------------
async function getBlockHTTP (info, pos) {
  async function getBlockHttpInternal () {
    const result = await axios.get(info.name, { responseType: 'arraybuffer', httpAgent, headers: { range: `bytes=${blockStart}-${blockStart + blockSize}` } });
    block = result.data;
  }

  const blockSize = 0x10000;
  const blockStart = Math.floor(pos / blockSize) * blockSize;
  const key = `${info.name}:${blockStart}`;

  let block = blocklru.get(key);
  if (!block) {
    block = getBlockHttpInternal();
    blocklru.set(key, block);
  }
  block = await block;
  return block.slice(pos - blockStart);
}

// --------------------------------------------------------------------------
function makeS3 (info) {
  const key = Config.getFull(info.node, 's3AccessKeyId') ?? Config.get('s3AccessKeyId');

  const cacheKey = `${info.extra.endpoint}:${info.extra.bucket}:${key}`;

  let s3 = S3s.get(cacheKey);
  if (s3) {
    return s3;
  }

  const s3Params = { region: info.extra.region, endpoint: info.extra.endpoint };

  if (key) {
    const secret = Config.getFull(info.node, 's3SecretAccessKey') ?? Config.get('s3SecretAccessKey');
    if (!secret) {
      console.log('ERROR - No s3SecretAccessKey set for ', info.node);
    }

    s3Params.credentials = { accessKeyId: key, secretAccessKey: secret };
  }

  s3Params.forcePathStyle = info.extra.pathStyle;
  s3Params.sslEnabled = info.extra.endpoint.startsWith('https://');

  // Lets hope that we can find a credential provider elsewhere

  s3 = new S3(s3Params);
  S3s.set(cacheKey, s3);
  return s3;
}

// --------------------------------------------------------------------------
// https://coderwall.com/p/pq0usg/javascript-string-split-that-ll-return-the-remainder
function splitRemain (str, separator, limit) {
  str = str.split(separator);
  if (str.length <= limit) { return str; }

  const ret = str.splice(0, limit);
  ret.push(str.join(separator));

  return ret;
}

// --------------------------------------------------------------------------
async function getBlockS3Internal (info, params) {
  const s3 = makeS3(info);
  const command = new GetObjectCommand(params);
  const response = await s3.send(command);
  return Buffer.from(await response.Body.transformToByteArray());
}

// --------------------------------------------------------------------------
// s3://bucket/path
async function getBlockS3 (info, pos) {
  const blockSize = 0x10000;
  const blockStart = Math.floor(pos / blockSize) * blockSize;
  const key = `${info.name}:${blockStart}`;
  let block = blocklru.get(key);
  if (!block) {
    const parts = splitRemain(info.name, '/', 4);

    const params = {
      Bucket: parts[3],
      Key: parts[4],
      Range: `bytes=${blockStart}-${blockStart + blockSize}`
    };

    block = getBlockS3Internal(info, params);
    blocklru.set(key, block);
  }

  block = await block;
  // Return a new buffer starting at pos
  return block.slice(pos - blockStart);
}

// --------------------------------------------------------------------------
// s3http://host:port/bucket/path
// s3https://host:port/bucket/path
async function getBlockS3HTTP (info, pos) {
  const blockSize = 0x10000;
  const blockStart = Math.floor(pos / blockSize) * blockSize;
  const key = `${info.extra.host}:${info.extra.bucket}:${blockStart}`;
  let block = blocklru.get(key);
  if (!block) {
    try {
      const params = {
        Bucket: info.extra.bucket,
        Key: info.extra.path.replace(/^\//, ''),
        Range: `bytes=${blockStart}-${blockStart + blockSize}`
      };

      block = getBlockS3Internal(info, params);
      blocklru.set(key, block);
    } catch (err) {
      console.error(err);
    }
  }

  // Return a new buffer starting at pos
  block = await block;
  return block.slice(pos - blockStart);
}

// --------------------------------------------------------------------------
/*
async function getBlockDisk (info, pos) {
  const blockSize = 0x10000;
  const blockStart = Math.floor(pos / blockSize) * blockSize;
  const key = `${info.name}:${blockStart}`;
  let block = blocklru.get(key);
  if (!block) {
    const fd = await fs.promises.open(info.name, 'r');
    const result = await fd.promises.read(Buffer.alloc(blockSize), 0, blockSize, blockStart);
    block = result.buffer.slice(0, result.bytesRead);
    blocklru.set(key, block);
    fs.close(fd);
  }

  // Return a new buffer starting at pos
  return block.slice(pos - blockStart);
}
internals.schemes.set('file', { getBlock: getBlockDisk });
*/

internals.schemes.set('http', { getBlock: getBlockHTTP });
internals.schemes.set('https', { getBlock: getBlockHTTP });
internals.schemes.set('s3', { getBlock: getBlockS3 });
internals.schemes.set('s3http', { getBlock: getBlockS3HTTP });
internals.schemes.set('s3https', { getBlock: getBlockS3HTTP });
