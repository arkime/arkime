/******************************************************************************/
/* writer-s3.js -- Ability to read s3 files
 *
 * Copyright 2012-2015 AOL Inc. All rights reserved.
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

var AWS = require('aws-sdk');
var async = require('async');
var zlib = require('zlib');
var S3s = {};
var Config;
var Db;
var Pcap;

var COMPRESSED_BLOCK_SIZE = 100000;
var COMPRESSED_WITHIN_BLOCK_BITS = 20;

/// ///////////////////////////////////////////////////////////////////////////////
// https://coderwall.com/p/pq0usg/javascript-string-split-that-ll-return-the-remainder
function splitRemain (str, separator, limit) {
  str = str.split(separator);
  if (str.length <= limit) { return str; }

  var ret = str.splice(0, limit);
  ret.push(str.join(separator));

  return ret;
}
/// ///////////////////////////////////////////////////////////////////////////////
function makeS3 (node, region) {
  var s3 = S3s[region + key];
  if (s3) {
    return s3;
  }

  var s3Params = {region: region};

  var key = Config.getFull(node, 's3AccessKeyId');
  if (key) {
    var secret = Config.getFull(node, 's3SecretAccessKey');
    if (!secret) {
      console.log('ERROR - No s3SecretAccessKey set for ', node);
    }

    s3Params.accessKeyId = key;
    s3Params.secretAccessKey = secret;
  }

  // Lets hope that we can find a credential provider elsewhere
  var rv = S3s[region + key] = new AWS.S3(s3Params);
  return rv;
}
/// ///////////////////////////////////////////////////////////////////////////////
function processSessionIdS3 (session, headerCb, packetCb, endCb, limit) {
  var fields = session._source || session.fields;

  // Get first pcap header
  var header, pcap, s3;
  Db.fileIdToFile(fields.node, fields.packetPos[0] * -1, function (info) {
    var parts = splitRemain(info.name, '/', 4);

    // Make s3 for this request, all will be in same region
    s3 = makeS3(fields.node, parts[2]);

    var params = {
      Bucket: parts[3],
      Key: parts[4],
      Range: 'bytes=0-128'
    };
    // console.log("HEADER", params);
    s3.getObject(params, function (err, data) {
      if (err) {
        console.log(err, info);
        return endCb("Couldn't open s3 file, save might not be complete yet - " + info.name, fields);
      }
      if (params.Key.endsWith('.gz')) {
        header = zlib.gunzipSync(data.Body, { finishFlush: zlib.constants.Z_SYNC_FLUSH });
      } else {
        header = data.Body;
      }
      header = header.subarray(0, 24);
      pcap = Pcap.make(info.name, header);
      if (headerCb) {
        headerCb(pcap, header);
      }
      readyToProcess();
    });
  });

  function readyToProcess () {
    var itemPos = 0;

    function process (data, nextCb) {
      // console.log("NEXT", data);
      data.params.Range = 'bytes=' + data.rangeStart + '-' + (data.rangeEnd - 1);
      s3.getObject(data.params, function (err, s3data) {
        if (err) {
          console.log('WARNING - Only have SPI data, PCAP file no longer available', data.info.name, err);
          return nextCb('Only have SPI data, PCAP file no longer available for ' + data.info.name);
        }
        if (data.compressed) {
          // Need to decompress the block(s)
          var decompressed = {};
          // First build a map from rangeStart to the decompressed block
          for (var i = 0; i < data.subPackets.length; i++) {
            var sp = data.subPackets[i];
            if (!decompressed[sp.rangeStart]) {
              var offset = sp.rangeStart - data.rangeStart;
              decompressed[sp.rangeStart] = zlib.inflateRawSync(s3data.Body.subarray(offset, offset + COMPRESSED_BLOCK_SIZE),
                  { finishFlush: zlib.constants.Z_SYNC_FLUSH });
            }
          }
          async.each(data.subPackets, function (sp, nextCb) {
            var block = decompressed[sp.rangeStart];
            var packetData = block.subarray(sp.packetStart, sp.packetEnd);
            var len = (pcap.bigEndian?packetData.readUInt32BE(8):packetData.readUInt32LE(8));

            packetCb(pcap, packetData.subarray(0, len + 16), nextCb, sp.itemPos);
          },
          nextCb);
        } else {
          async.each(data.subPackets, function (sp, nextCb) {
            var packetData = s3data.Body.subarray(sp.packetStart - data.packetStart, sp.packetEnd - data.packetStart);
            var len = (pcap.bigEndian?packetData.readUInt32BE(8):packetData.readUInt32LE(8));

            packetCb(pcap, packetData.subarray(0, len + 16), nextCb, sp.itemPos);
          },
          nextCb);
        }
      });
    }

    // FIrst pass, convert packetPos and packetLen (if we have it) into packetData
    var packetData = [];

    async.eachLimit(Object.keys(fields.packetPos), limit || 1, function (p, nextCb) {
      var pos = fields.packetPos[p];

      if (pos < 0) {
        Db.fileIdToFile(fields.node, pos * -1, function (info) {
          var parts = splitRemain(info.name, '/', 4);
          p = parseInt(p);
          var compressed = info.name.endsWith('.gz');
          for (var pp = p + 1; pp < fields.packetPos.length && fields.packetPos[pp] >= 0; pp++) {
            var pos = fields.packetPos[pp];
            var len = 65536;
            if (fields.packetLen) {
              len = fields.packetLen[pp];
            }
            var params = {
              Bucket: parts[3],
              Key: parts[4]
            };
            var pd = packetData[pp] = {
              params: params,
              info: info,
              compressed: compressed
            };
            if (compressed) {
              pd.rangeStart = Math.floor(pos / (1 << COMPRESSED_WITHIN_BLOCK_BITS));
              pd.rangeEnd = pd.rangeStart + COMPRESSED_BLOCK_SIZE;
              pd.packetStart = pos & ((1 << COMPRESSED_WITHIN_BLOCK_BITS) - 1);
              pd.packetEnd = pd.packetStart + len;
            } else {
              pd.rangeStart = pos;
              pd.rangeEnd = pos + len;
              pd.packetStart = pos;
              pd.packetEnd = pos + len;
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
      var packetDataOpt = [];
      var previousData = null;
      for (var i = 0; i < packetData.length; i++) {
        var data = packetData[i];
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
      async.eachLimit(packetDataOpt, limit || 1, function (data, nextCb) {
        process(data, nextCb);
      },
      function (pcapErr, results) {
        endCb(pcapErr, fields);
      });
    });
  }
}
/// ///////////////////////////////////////////////////////////////////////////////
function s3Expire () {
  var query = { _source: [ 'num', 'name', 'first', 'size', 'node' ],
    from: '0',
    size: 1000,
    query: { bool: {
      must: [
        {range: {first: {lte: Math.floor(Date.now() / 1000 - (+Config.get('s3ExpireDays')) * 60 * 60 * 24)}}},
        {prefix: {name: 's3://'}}
      ]
    }},
    sort: { first: { order: 'asc' } } };
  Db.search('files', 'file', query, function (err, data) {
    if (!data.hits || !data.hits.hits) {
      return;
    }
    // console.log("HITS", data.hits.hits);

    data.hits.hits.forEach(function (item) {
      var parts = splitRemain(item._source.name, '/', 4);
      var s3 = makeS3(item._source.node, parts[2]);
      s3.deleteObject({Bucket: parts[3], Key: parts[4]}, function (err, data) {
        if (err) {
          console.log("Couldn't delete from S3", item._id, item._source);
        } else {
          Db.deleteDocument('files', 'file', item._id, function (err, data) {
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
  api.registerWriter('s3', {localNode: false, processSessionId: processSessionIdS3});
  Config = config;
  Db = api.getDb();
  Pcap = api.getPcap();

  if (Config.get('s3ExpireDays') !== undefined) {
    s3Expire();
    setInterval(s3Expire, 600 * 1000);
  }
};
