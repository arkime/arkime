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
var util = require('util');
var S3s = {};
var Config;
var Db;
var Pcap;

//////////////////////////////////////////////////////////////////////////////////
//https://coderwall.com/p/pq0usg/javascript-string-split-that-ll-return-the-remainder
function splitRemain(str, separator, limit) {
    str = str.split(separator);
    if(str.length <= limit) {return str;}

    var ret = str.splice(0, limit);
    ret.push(str.join(separator));

    return ret;
}
//////////////////////////////////////////////////////////////////////////////////
function makeS3(node, region)
{
  var key = Config.getFull(node, "s3AccessKeyId");
  if (!key) {
    console.log("ERROR - No s3AccessKeyId set for ", node);
    return undefined;
  }

  var s3 = S3s[region + key];
  if (s3) {
    return s3;
  }

  var secret = Config.getFull(node, "s3SecretAccessKey");
  if (!secret) {
    console.log("ERROR - No s3SecretAccessKey set for ", node);
  }
  var s3Params = {region: region,
                  accessKeyId: key, 
                  secretAccessKey: secret};
  return S3s[region + key] = new AWS.S3(s3Params);
}
//////////////////////////////////////////////////////////////////////////////////
function processSessionIdS3(session, headerCb, packetCb, endCb, limit) {
  var fields = session._source || session.fields;

  // Get first pcap header
  var header, pcap, s3;
  Db.fileIdToFile(fields.node, fields.packetPos[0] * -1, function(info) {
    var parts = splitRemain(info.name,'/', 4);

    // Make s3 for this request, all will be in same region
    s3 = makeS3(fields.node, parts[2]);

    var params = {
      Bucket: parts[3],
      Key: parts[4],
      Range: 'bytes=0-23'
    };
    //console.log("HEADER", params);
    s3.getObject(params, function (err, data) {
      if (err) {
        console.log(err, info);
        return endCb("Couldn't open s3 file, save might not be complete yet - " + info.name, fields);
      }
      header = data.Body;
      pcap = Pcap.make(info.name, header);
      if (headerCb) {
        headerCb(pcap, header);
      }
      readyToProcess();
    });
  });

  function readyToProcess () {
    var params;
    var itemPos = 0;
    var saveInfo;

    function process(ipos, nextCb) {
      //console.log("NEXT", params);
      s3.getObject(params, function (err, data) {
        if (err) {
          console.log("WARNING - Only have SPI data, PCAP file no longer available", saveInfo.name, err);
          return nextCb("Only have SPI data, PCAP file no longer available for " + saveInfo.name);
        }
        packetCb(pcap, data.Body, nextCb, ipos);
      });
    }

    async.eachLimit(Object.keys(fields.packetPos), limit || 1, function(p, nextCb) {
      var pos = fields.packetPos[p];

      if (pos < 0) {
        Db.fileIdToFile(fields.node, pos * -1, function(info) {
          saveInfo = info;
          var parts = splitRemain(info.name,'/', 4);
          params = {
            Bucket: parts[3],
            Key: parts[4]
          };
          return nextCb(null);
        });
        return;
      }

      var len = fields.packetLen[p];
      params.Range = "bytes=" + pos + "-" + (pos+len-1);
      process(itemPos++, nextCb);
    },
    function (pcapErr, results) {
      endCb(pcapErr, fields);
    });
  }
}
//////////////////////////////////////////////////////////////////////////////////
function s3Expire()
{
  var query = { _source: [ 'num', 'name', 'first', 'size', 'node' ],
                  from: '0',
                  size: 1000,
                 query: { bool: {
                    must: [
                          {range: {first: {lte: Math.floor(Date.now()/1000 - (+Config.get("s3ExpireDays"))*60*60*24)}}},
                          {prefix: {name: "s3://"}}
                        ]
                 }},
                 sort: { first: { order: 'asc' } } };
  Db.search('files', 'file', query, function(err, data) {
    if (!data.hits || !data.hits.hits) {
      return;
    }
    //console.log("HITS", data.hits.hits);

    data.hits.hits.forEach(function(item) {
      var parts = splitRemain(item._source.name,'/', 4);
      var s3 = makeS3(item._source.node, parts[2]);
      s3.deleteObject({Bucket: parts[3], Key: parts[4]}, function (err, data) {
        if (err) {
          console.log("Couldn't delete from S3", item._id, item._source);
        } else {
          Db.deleteDocument('files', 'file', item._id, function(err, data) {
            if (err) {
              console.log("Couldn't delete from ES", item._id, item._source);
            }
          });
        }
      });
    });
  });
}
//////////////////////////////////////////////////////////////////////////////////
exports.init = function (config, emitter, api) {
  api.registerWriter("s3", {localNode: false, processSessionId: processSessionIdS3 });
  Config = config;
  Db = api.getDb();
  Pcap = api.getPcap();

  if (Config.get("s3ExpireDays") !== undefined) {
    s3Expire();
    setInterval(s3Expire, 600*1000);
  }
}
