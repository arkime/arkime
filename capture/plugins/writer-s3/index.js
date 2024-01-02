/******************************************************************************/
/* writer-s3.js -- Ability to read s3 files
 *
 * Copyright 2012-2015 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const { S3 } = require('@aws-sdk/client-s3');
const async = require('async');
const zlib = require('zlib');
const { decompressSync } = require('@xingrz/cppzst');
const S3s = {};
const LRU = require('lru-cache');
const CacheInProgress = {};
let Config;
let Db;
let Pcap;

const DEFAULT_COMPRESSED_BLOCK_SIZE = 100000;
const COMPRESSED_WITHIN_BLOCK_BITS = 20;
const COMPRESSED_GZIP = 1;
const COMPRESSED_ZSTD = 2;

const S3DEBUG = false;
// Store up to 100 items
const lru = new LRU({ max: 100 });

/// ///////////////////////////////////////////////////////////////////////////////
// https://coderwall.com/p/pq0usg/javascript-string-split-that-ll-return-the-remainder
function splitRemain (str, separator, limit) {
  str = str.split(separator);
  if (str.length <= limit) { return str; }

  const ret = str.splice(0, limit);
  ret.push(str.join(separator));

  return ret;
}
/// ///////////////////////////////////////////////////////////////////////////////
function makeS3 (node, region, bucket) {
  const key = Config.getFull(node, 's3AccessKeyId') ?? Config.get('s3AccessKeyId');

  const s3 = S3s[region + key];
  if (s3) {
    return s3;
  }

  const s3Params = { region };

  if (key) {
    const secret = Config.getFull(node, 's3SecretAccessKey') ?? Config.get('s3SecretAccessKey');
    if (!secret) {
      console.log('ERROR - No s3SecretAccessKey set for ', node);
    }

    s3Params.credentials = { accessKeyId: key, secretAccessKey: secret };
  }

  if (Config.getFull(node, 's3Host') !== undefined) {
    s3Params.endpoint = Config.getFull(node, 's3Host');
  }

  bucket ??= Config.getFull(node, 's3Bucket');
  const bucketHasDot = bucket.indexOf('.') >= 0;
  if (Config.getFull(node, 's3PathAccessStyle', bucketHasDot) === true) {
    s3Params.forcePathStyle = true;
  }

  if (Config.getFull(node, 's3UseHttp', false) === true) {
    s3Params.sslEnabled = false;
  }

  // Lets hope that we can find a credential provider elsewhere
  const rv = S3s[region + key] = new S3(s3Params);
  return rv;
}
/// ///////////////////////////////////////////////////////////////////////////////
function processSessionIdS3 (session, headerCb, packetCb, endCb, limit) {
  const fields = session._source || session.fields;

  // Get first pcap header
  let header, pcap, s3;
  Db.fileIdToFile(fields.node, fields.packetPos[0] * -1, function (info) {
    if (Config.debug) {
      console.log(`File Info for ${fields.node}-${fields.packetPos[0] * -1}`, info);
    }
    const parts = splitRemain(info.name, '/', 4);
    info.compressionBlockSize ??= DEFAULT_COMPRESSED_BLOCK_SIZE;

    // Make s3 for this request, all will be in same region
    s3 = makeS3(fields.node, parts[2], parts[3]);

    const params = {
      Bucket: parts[3],
      Key: parts[4],
      Range: 'bytes=0-128'
    };
    const cacheKey = 'pcap:' + fields.node + ':' + fields.packetPos[0];
    const headerData = lru.get(cacheKey);
    if (headerData) {
      header = headerData.header;
      pcap = headerData.pcap;
      if (headerCb) {
        headerCb(pcap, header);
      }
      readyToProcess();
    } else {
      if (S3DEBUG) {
        console.log('s3.getObject for header', params);
      }
      s3.getObject(params, async function (err, data) {
        if (err) {
          console.log(err, info);
          return endCb("Couldn't open s3 file, save might not be complete yet - " + info.name, fields);
        }
        const body = Buffer.from(await data.Body.transformToByteArray());
        if (params.Key.endsWith('.gz')) {
          header = zlib.gunzipSync(body, { finishFlush: zlib.constants.Z_SYNC_FLUSH });
        } else if (params.Key.endsWith('.zst')) {
          header = decompressSync(body);
        } else {
          header = body;
        }
        header = header.subarray(0, 24);
        pcap = Pcap.make(info.name, header);
        lru.set(cacheKey, { pcap, header });
        if (headerCb) {
          headerCb(pcap, header);
        }
        readyToProcess();
      });
    }
  });

  function readyToProcess () {
    let itemPos = 0;

    function doProcess (data, nextCb) {
      let haveAllCached = data.compressed;
      // See if we have all the required decompressed blocks
      for (let i = 0; i < data.subPackets.length; i++) {
        const sp = data.subPackets[i];
        const decompressedCacheKey = 'data:' + data.params.Bucket + ':' + data.params.Key + ':' + sp.rangeStart;
        const cachedDecompressed = lru.get(decompressedCacheKey);
        if (!cachedDecompressed) {
          haveAllCached = false;
          if (CacheInProgress[decompressedCacheKey]) {
            // We need to wait for this to complete
            if (CacheInProgress[decompressedCacheKey] === true) {
              CacheInProgress[decompressedCacheKey] = [];
            }
            CacheInProgress[decompressedCacheKey].push(function () { doProcess(data, nextCb); });
            return;
          }
          break;
        }
      }

      if (haveAllCached) {
        // We have all the data that we need. Note that this code assumes
        // that the data is compressed. Doing the caching for uncompressed
        // files is left as an exercise for the reader.
        async.each(data.subPackets, (sp, nextSubCb) => {
          const decompressedCacheKey = 'data:' + data.params.Bucket + ':' + data.params.Key + ':' + sp.rangeStart;
          const block = lru.get(decompressedCacheKey);
          const subPacketData = block.subarray(sp.packetStart, sp.packetEnd);
          const len = (pcap.bigEndian ? subPacketData.readUInt32BE(8) : subPacketData.readUInt32LE(8));

          packetCb(pcap, subPacketData.subarray(0, len + 16), nextSubCb, sp.itemPos);
        },
        nextCb);
      } else {
        for (let i = 0; i < data.subPackets.length; i++) {
          const sp = data.subPackets[i];
          const decompressedCacheKey = 'data:' + data.params.Bucket + ':' + data.params.Key + ':' + sp.rangeStart;
          if (!CacheInProgress[decompressedCacheKey]) {
            CacheInProgress[decompressedCacheKey] = true;
          }
        }
        data.params.Range = 'bytes=' + data.rangeStart + '-' + (data.rangeEnd - 1);
        if (S3DEBUG) {
          console.log('s3.getObject for pcap data', data.params);
        }
        s3.getObject(data.params, async function (err, s3data) {
          if (err) {
            console.log('WARNING - Only have SPI data, PCAP file no longer available', data.info.name, err);
            return nextCb('Only have SPI data, PCAP file no longer available for ' + data.info.name);
          }
          const body = Buffer.from(await s3data.Body.transformToByteArray());
          if (data.compressed) {
            // Need to decompress the block(s)
            const decompressed = {};
            // First build a map from rangeStart to the decompressed block
            for (let i = 0; i < data.subPackets.length; i++) {
              const sp = data.subPackets[i];
              if (!decompressed[sp.rangeStart]) {
                const offset = sp.rangeStart - data.rangeStart;
                if (data.compressed === COMPRESSED_GZIP) {
                  decompressed[sp.rangeStart] = zlib.inflateRawSync(body.subarray(offset, offset + data.info.compressionBlockSize),
                    { finishFlush: zlib.constants.Z_SYNC_FLUSH });
                } else if (data.compressed === COMPRESSED_ZSTD) {
                  decompressed[sp.rangeStart] = decompressSync(body.subarray(offset, offset + data.info.compressionBlockSize));
                }
                const decompressedCacheKey = 'data:' + data.params.Bucket + ':' + data.params.Key + ':' + sp.rangeStart;
                lru.set(decompressedCacheKey, decompressed[sp.rangeStart]);
                const cip = CacheInProgress[decompressedCacheKey];
                delete CacheInProgress[decompressedCacheKey];
                if (cip && cip !== true) {
                  for (let j = 0; j < cip.length; j++) {
                    cip[j]();
                  }
                }
              }
            }
            async.each(data.subPackets, (sp, nextSubCb) => {
              const block = decompressed[sp.rangeStart];
              const subPacketData = block.subarray(sp.packetStart, sp.packetEnd);
              const len = (pcap.bigEndian ? subPacketData.readUInt32BE(8) : subPacketData.readUInt32LE(8));

              packetCb(pcap, subPacketData.subarray(0, len + 16), nextSubCb, sp.itemPos);
            },
            nextCb);
          } else {
            async.each(data.subPackets, (sp, nextSubCb) => {
              const subPacketData = body.subarray(sp.packetStart - data.packetStart, sp.packetEnd - data.packetStart);
              const len = (pcap.bigEndian ? subPacketData.readUInt32BE(8) : subPacketData.readUInt32LE(8));

              packetCb(pcap, subPacketData.subarray(0, len + 16), nextSubCb, sp.itemPos);
            },
            nextCb);
          }
        });
      }
    }

    // FIrst pass, convert packetPos and packetLen (if we have it) into packetData
    const packetData = [];

    async.eachLimit(Object.keys(fields.packetPos), limit || 1, function (p, nextCb) {
      const pos = fields.packetPos[p];

      if (pos < 0) {
        Db.fileIdToFile(fields.node, pos * -1, function (info) {
          const parts = splitRemain(info.name, '/', 4);
          p = parseInt(p);
          let compressed = 0;
          if (info.name.endsWith('.gz')) {
            compressed = COMPRESSED_GZIP;
          } else if (info.name.endsWith('.zst')) {
            compressed = COMPRESSED_ZSTD;
          }
          for (let pp = p + 1; pp < fields.packetPos.length && fields.packetPos[pp] >= 0; pp++) {
            const packetPos = fields.packetPos[pp];
            let len = 65536;
            if (fields.packetLen) {
              len = fields.packetLen[pp];
            }
            const params = {
              Bucket: parts[3],
              Key: parts[4]
            };
            const pd = packetData[pp] = {
              params,
              info,
              compressed
            };
            if (compressed) {
              pd.rangeStart = Math.floor(packetPos / (1 << COMPRESSED_WITHIN_BLOCK_BITS));
              pd.rangeEnd = pd.rangeStart + info.compressionBlockSize;
              pd.packetStart = packetPos & ((1 << COMPRESSED_WITHIN_BLOCK_BITS) - 1);
              pd.packetEnd = pd.packetStart + len;
            } else {
              pd.rangeStart = packetPos;
              pd.rangeEnd = packetPos + len;
              pd.packetStart = packetPos;
              pd.packetEnd = packetPos + len;
            }
            packetData[pp].subPackets = [packetData[pp]];
          }
          return nextCb(null);
        });
        return;
      }
      return nextCb(null);
    },
    function (pcapErr, results) {
      // Now we have all the packetData objects. Set the itemPos correctly
      const packetDataOpt = [];
      let previousData = null;
      for (let i = 0; i < packetData.length; i++) {
        const data = packetData[i];
        if (data) {
          data.itemPos = itemPos++;
          // See if we should glue these two together
          if (previousData) {
            if (previousData.info.name === data.info.name) {
              // Referencing the same file
              if (data.rangeStart >= previousData.rangeStart &&
                  data.rangeStart < previousData.rangeEnd + 32768 &&
                  data.rangeEnd > previousData.rangeEnd) {
                // This is within 32k bytes -- just extend the fetch
                previousData.rangeEnd = data.rangeEnd;

                previousData.subPackets.push(data);
                continue;
              }
            }
          }
          packetDataOpt.push(data);
          previousData = data;
        }
      }
      async.eachLimit(packetDataOpt, limit || 1, (data, nextCb) => {
        doProcess(data, nextCb);
      }, (pcapEachErr) => {
        endCb(pcapEachErr, fields);
      });
    });
  }
}
/// ///////////////////////////////////////////////////////////////////////////////
function s3Expire () {
  const query = {
    _source: ['num', 'name', 'first', 'size', 'node'],
    from: '0',
    size: 1000,
    query: {
      bool: {
        must: [
          { range: { first: { lte: Math.floor(Date.now() / 1000 - (+Config.get('s3ExpireDays')) * 60 * 60 * 24) } } },
          { prefix: { name: 's3://' } }
        ]
      }
    },
    sort: { first: { order: 'asc' } }
  };
  Db.search('files', 'file', query, (err, data) => {
    if (err || !data.hits || !data.hits.hits) {
      return;
    }

    data.hits.hits.forEach((item) => {
      const parts = splitRemain(item._source.name, '/', 4);
      const s3 = makeS3(item._source.node, parts[2], parts[3]);
      s3.deleteObject({ Bucket: parts[3], Key: parts[4] }, (err) => {
        if (err) {
          console.log('Couldn\'t delete from S3', item._id, item._source);
        } else {
          Db.deleteDocument('files', 'file', item._id, (err) => {
            if (err) {
              console.log("Couldn't delete from ES", item._id, item._source);
            }
          });
        }
      });
    });
  });
}
/// ///////////////////////////////////////////////////////////////////////////////
exports.init = function (config, emitter, api) {
  api.registerWriter('s3', { localNode: false, processSessionId: processSessionIdS3 });
  Config = config;
  Db = api.getDb();
  Pcap = api.getPcap();

  if (Config.get('s3ExpireDays') !== undefined) {
    s3Expire();
    setInterval(s3Expire, 600 * 1000);
  }
};
