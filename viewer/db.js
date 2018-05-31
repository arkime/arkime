/******************************************************************************/
/* db.js -- Lowlevel and highlevel functions dealing with the database
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
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
'use strict';

var ESC            = require('elasticsearch'),
    async          = require('async'),
    os             = require('os'),
    fs             = require('fs'),
    util           = require('util');

var internals = {fileId2File: {},
                 fileName2File: {},
                 molochNodeStatsCache: {},
                 healthCache: {},
                 indicesCache: {},
                 usersCache: {},
                 qInProgress: 0,
                 apiVersion: "5.5",
                 q: []};

exports.initialize = function (info, cb) {
  internals.dontMapTags = info.dontMapTags === 'true' || info.dontMapTags === true || false;
  delete info.dontMapTags;
  internals.info = info;

  if (info.prefix && info.prefix.charAt(info.prefix.length-1) !== "_") {
    internals.prefix = info.prefix + "_";
  } else {
    internals.prefix = info.prefix || "";
  }

  if (info.usersPrefix && info.usersPrefix.charAt(info.usersPrefix.length-1) !== "_") {
    internals.usersPrefix = info.usersPrefix + "_";
  } else {
    internals.usersPrefix = info.usersPrefix || internals.prefix;
  }

  internals.nodeName = info.nodeName;
  delete info.nodeName;

  internals.elasticSearchClient = new ESC.Client({
    host: internals.info.host,
    apiVersion: internals.apiVersion,
    requestTimeout: 300000,
    keepAlive: true,
    minSockets: 20,
    maxSockets: 51
  });

  internals.elasticSearchClient.info((err,data) => {
    if (err) {
      console.log(err, data);
    }
    if (data.version.number.match(/^(2.[0-3]|1|0)/)) {
      console.log("ERROR - ES", data.version.number, "not supported, ES 2.4.x or later required.");
      process.exit();
      throw new Error("Exiting");
    }

    if (info.usersHost) {
      internals.usersElasticSearchClient = new ESC.Client({
        host: internals.info.usersHost,
        apiVersion: internals.apiVersion,
        requestTimeout: 300000,
        keepAlive: true,
        minSockets: 5,
        maxSockets: 6
      });
    } else {
      internals.usersElasticSearchClient = internals.elasticSearchClient;
    }
    return cb();
  });

  // Replace tag implementation
  if (internals.dontMapTags) {
    exports.isLocalView = function(node, yesCB, noCB) {return noCB(); };
    internals.prefix = "MULTIPREFIX_";
  }
};

//////////////////////////////////////////////////////////////////////////////////
//// Low level functions to convert from old style to new
//////////////////////////////////////////////////////////////////////////////////
//
//
function fixIndex(index) {
  if (index === undefined) {return undefined;}

  if (Array.isArray(index)) {
    return index.map((val) => {
      if (val.lastIndexOf(internals.prefix, 0) === 0) {
        return val;
      } else {
        return internals.prefix + val;
      }
    });
  }

  if (index.lastIndexOf(internals.prefix, 0) === 0) {
    return index;
  } else {
    return internals.prefix + index;
  }
}

exports.merge = function(to, from) {
  for (var key in from) {
    to[key] = from[key];
  }
};

exports.get = function (index, type, id, cb) {
  return internals.elasticSearchClient.get({index: fixIndex(index), type: type, id: id}, cb);
};

exports.getWithOptions = function (index, type, id, options, cb) {
  var params = {index: fixIndex(index), type:type, id: id};
  exports.merge(params, options);
  return internals.elasticSearchClient.get(params, cb);
};

exports.index = function (index, type, id, document, cb) {
  return internals.elasticSearchClient.index({index: fixIndex(index), type: type, body: document, id: id}, cb);
};

exports.indexNow = function (index, type, id, document, cb) {
  return internals.elasticSearchClient.index({index: fixIndex(index), type: type, body: document, id: id, refresh: true}, cb);
};

exports.search = function (index, type, query, options, cb) {
  if (!cb) {
    cb = options;
    options = undefined;
  }
  var params = {index: fixIndex(index), type: type, body: query};
  exports.merge(params, options);
  return internals.elasticSearchClient.search(params, cb);
};

function searchScrollInternal(index, type, query, options, cb) {
  var totalResults;
  var params = {scroll: '5m'};
  exports.merge(params, options);
  var querySize = query.size;
  query.size = 1000; // Get 1000 items per scroll call
  exports.search(index, type, query, params,
    function getMoreUntilDone(error, response) {
      if (error) {
          return cb(error, totalResults);
      }

      if (totalResults === undefined) {
        totalResults = response;
      } else {
        Array.prototype.push.apply(totalResults.hits.hits, response.hits.hits);
      }

      if (totalResults.hits.total > 0 && totalResults.hits.hits.length < Math.min(response.hits.total, querySize)) {
        exports.scroll({
          scroll: '5m',
          body: {
            scroll_id: response._scroll_id,
          }
        }, getMoreUntilDone);
      } else {
          cb(error, totalResults);
      }
    });
}

exports.searchScroll = function (index, type, query, options, cb) {
  if ((query.size || 0) + (parseInt(query.from,10) || 0) >= 10000) {
    if (cb) {
      return searchScrollInternal(index, type, query, options, cb);
    } else {
      return new Promise((resolve, reject) => {
        searchScrollInternal(index, type, query, options, (err, data) => {
          if (err) {
            reject(err);
          } else {
            resolve(data);
          }
        });
      });
    }
  } else {
    return exports.search(index, type, query, options, cb);
  }
};

exports.searchPrimary = function (index, type, query, options, cb) {
  if (!cb) {
    cb = options;
    options = undefined;
  }

  var params = {preference: "_primary_first", ignore_unavailable: "true"};
  exports.merge(params, options);
  return exports.searchScroll(index, type, query, params, cb);
};

exports.msearch = function (index, type, queries, cb) {
  var body = [];

  for(var i = 0, ilen = queries.length; i < ilen; i++){
    body.push({index: fixIndex(index), type: type});
    body.push(queries[i]);
  }

  return internals.elasticSearchClient.msearch({body: body}, cb);
};

exports.scroll = function (params, callback) {
  return internals.elasticSearchClient.scroll(params, callback);
};

exports.bulk = function (params, callback) {
  return internals.elasticSearchClient.bulk(params, callback);
};

exports.deleteByQuery = function (index, type, query, cb) {
  return internals.elasticSearchClient.deleteByQuery({index: fixIndex(index), type: type, body: query}, cb);
};

exports.deleteDocument = function (index, type, id, options, cb) {
  if (!cb) {
    cb = options;
    options = undefined;
  }
  var params = {index: fixIndex(index), type: type, id: id};
  exports.merge(params, options);
  return internals.elasticSearchClient.delete(params, cb);
};

exports.deleteIndex = function (index, options, cb) {
  if (!cb) {
    cb = options;
    options = undefined;
  }
  var params = {index: fixIndex(index)};
  exports.merge(params, options);
  return internals.elasticSearchClient.indices.delete(params, cb);
};

exports.indexStats = function(index, cb) {
  return internals.elasticSearchClient.indices.stats({index: fixIndex(index)}, cb);
};

exports.getAliases = function(index, cb) {
  return internals.elasticSearchClient.indices.getAlias({index: fixIndex(index)}, cb);
};

exports.getAliasesCache = function (index, cb) {
  if (internals.aliasesCache && internals.aliasesCacheTimeStamp > Date.now() - 5000) {
    return cb(null, internals.aliasesCache);
  }

  exports.getAliases(index, (err, aliases) => {
    if (err) {
      return cb(err, aliases);
    }

    internals.aliasesCacheTimeStamp = Date.now();
    internals.aliasesCache = aliases;

    cb(null, aliases);
  });
};

exports.health = function(cb) {
  return internals.elasticSearchClient.info((err,data) => {
    internals.elasticSearchClient.cluster.health({}, (err, result) => {
      if (data && result) {
        result.version = data.version.number;
      }
      return cb(err, result);
    });
  });
};

exports.indices = function(cb, index) {
  return internals.elasticSearchClient.cat.indices({format: "json", index: fixIndex(index), bytes: "b", h: "health,status,index,uuid,pri,rep,docs.count,store.size,cd,segmentsCount,pri.search.query_current,memoryTotal"}, cb);
};

exports.shards = function(cb) {
  return internals.elasticSearchClient.cat.shards({format: "json", bytes: "b", h: "index,shard,prirep,state,docs,store,ip,node,ur,uf,fm,sm"}, cb);
};

exports.getClusterSettings = function(options, cb) {
  return internals.elasticSearchClient.cluster.getSettings(options, cb);
};

exports.putClusterSettings = function(options, cb) {
  return internals.elasticSearchClient.cluster.putSettings(options, cb);
};

exports.tasks = function(cb) {
  return internals.elasticSearchClient.tasks.list({detailed: "true", group_by: "parents"}, cb);
};

exports.taskCancel = function(taskId, cb) {
  return internals.elasticSearchClient.tasks.cancel({taskId: taskId}, cb);
};

exports.nodesStats = function (options, cb) {
  return internals.elasticSearchClient.nodes.stats(options, cb);
};

exports.update = function (index, type, id, document, options, cb) {
  if (!cb) {
    cb = options;
    options = undefined;
  }

  var params = {index: fixIndex(index), type: type, body: document, id: id};
  exports.merge(params, options);
  return internals.elasticSearchClient.update(params, cb);
};

exports.close = function () {
  return internals.elasticSearchClient.close();
};

exports.flush = function (index, cb) {
  return internals.usersElasticSearchClient.indices.flush({index: fixIndex(index)}, cb);
};

exports.refresh = function (index, cb) {
  return internals.usersElasticSearchClient.indices.refresh({index: fixIndex(index)}, cb);
};

//////////////////////////////////////////////////////////////////////////////////
//// High level functions
//////////////////////////////////////////////////////////////////////////////////
exports.flushCache = function () {
  internals.fileId2File = {};
  internals.fileName2File = {};
  internals.molochNodeStatsCache = {};
  internals.healthCache = {};
  internals.usersCache = {};
  delete internals.aliasesCache;
};


exports.searchUsers = function(query, cb) {
  return internals.usersElasticSearchClient.search({index: internals.usersPrefix + 'users', type: 'user', body: query}, cb);
};

exports.getUser = function (name, cb) {
  return internals.usersElasticSearchClient.get({index: internals.usersPrefix + 'users', type: 'user', id: name}, cb);
};

exports.getUserCache = function (name, cb) {
  if (internals.usersCache[name] && internals.usersCache[name]._timeStamp > Date.now() - 5000) {
    return cb(null, internals.usersCache[name]);
  }

  exports.getUser(name, (err, suser) => {
    if (err) {
      return cb(err, suser);
    }

    suser._timeStamp = Date.now();
    internals.usersCache[name] = suser;

    cb(null, suser);
  });
};

exports.numberOfUsers = function(cb) {
  return internals.usersElasticSearchClient.count({index: internals.usersPrefix + 'users', ignoreUnavailable:true}, cb);
};

exports.deleteUser = function (name, cb) {
  delete internals.usersCache[name];
  return internals.usersElasticSearchClient.delete({index: internals.usersPrefix + 'users', type: 'user', id: name, refresh: true}, cb);
};

exports.setUser = function(name, doc, cb) {
  delete internals.usersCache[name];
  return internals.usersElasticSearchClient.index({index: internals.usersPrefix + 'users', type: 'user', body: doc, id: name, refresh: true}, cb);
};

function twoDigitString(value) {
  return (value < 10) ? ("0" + value) : value.toString();
}

exports.historyIt = function(doc, cb) {
  var d     = new Date(Date.now());
  var jan   = new Date(d.getUTCFullYear(), 0, 0);
  var iname = internals.prefix + 'history_v1-' +
    twoDigitString(d.getUTCFullYear()%100) + 'w' +
    twoDigitString(Math.floor((d - jan) / 604800000));

  return internals.elasticSearchClient.index({index:iname, type:'history', body:doc, refresh: true}, cb);
};
exports.searchHistory = function(query, cb) {
  return internals.elasticSearchClient.search({index:fixIndex('history_v1-*'), type:"history", body:query}, cb);
};
exports.numberOfLogs = function(cb) {
  return internals.elasticSearchClient.count({index:fixIndex('history_v1-*'), type:"history", ignoreUnavailable:true}, cb);
};

exports.deleteHistoryItem = function (id, index, cb) {
  return internals.elasticSearchClient.delete({index:index, type: 'history', id: id, refresh: true}, cb);
};

exports.molochNodeStats = function (name, cb) {
  exports.get('stats', 'stat', name, (err, stat) => {
    if (err || !stat.found) {

      // Even if an error, if we have a cached value use it
      if (err && internals.molochNodeStatsCache[name]) {
        return cb(null, internals.molochNodeStatsCache[name]);
      }

      cb(err || "Unknown node " + name, internals.molochNodeStatsCache[name]);
    } else {
      internals.molochNodeStatsCache[name] = stat._source;
      internals.molochNodeStatsCache[name]._timeStamp = Date.now();

      cb(null, stat._source);
    }
  });
};

exports.molochNodeStatsCache = function (name, cb) {
  if (internals.molochNodeStatsCache[name] && internals.molochNodeStatsCache[name]._timeStamp > Date.now() - 30000) {
    return cb(null, internals.molochNodeStatsCache[name]);
  }

  return exports.molochNodeStats(name, cb);
};


exports.healthCache = function (cb) {
  if (!cb) {
    return internals.healthCache;
  }

  if (internals.healthCache._timeStamp !== undefined && internals.healthCache._timeStamp > Date.now() - 10000) {
    return cb(null, internals.healthCache);
  }

  return exports.health((err, health) => {
    if (err) {
      // Even if an error, if we have a cache use it
      if (internals.healthCache._timeStamp !== undefined) {
        return cb(null, internals.healthCache);
      }
      return cb(err, null);
    }

    internals.elasticSearchClient.indices.getTemplate({name: fixIndex("sessions2_template"), filter_path: "**._meta"}, (err, doc) => {
      health.molochDbVersion = doc[fixIndex("sessions2_template")].mappings.session._meta.molochDbVersion;
      internals.healthCache = health;
      internals.healthCache._timeStamp = Date.now();
      cb(null, health);
    });
  });
};

exports.healthCachePromise = function () {
  return new Promise(function(resolve, reject) {
    exports.healthCache((err, data) => {
      if (err) {
        reject(err);
      } else {
        resolve(data);
      }
    });
  });
};

exports.indicesCache = function (cb) {
  if (!cb) {
    return internals.indicesCache;
  }

  if (internals.indicesCache._timeStamp !== undefined && internals.indicesCache._timeStamp > Date.now() - 10000) {
    return cb(null, internals.indicesCache);
  }

  return exports.indices((err, indices) => {
    if (err) {
      // Even if an error, if we have a cache use it
      if (internals.indicesCache._timeStamp !== undefined) {
        return cb(null, internals.indicesCache);
      }
      return cb(err, null);
    }

    internals.indicesCache = indices;
    internals.indicesCache._timeStamp = Date.now();
    cb(null, indices);
  });
};

exports.hostnameToNodeids = function (hostname, cb) {
  var query = {query: {match: {hostname:hostname}}};
  exports.search('stats', 'stat', query, (err, sdata) => {
    var nodes = [];
    if (sdata && sdata.hits && sdata.hits.hits) {
      for (var i = 0, ilen = sdata.hits.hits.length; i < ilen; i++) {
        nodes.push(sdata.hits.hits[i]._id);
      }
    }
    cb(nodes);
  });
};

exports.fileIdToFile = function (node, num, cb) {
  var key = node + '!' + num;
  let info = internals.fileId2File[key];
  if (info !== undefined) {
    return setImmediate(() => {
      cb(info);
    });
  }

  exports.get('files', 'file', node + '-' + num, (err, fresult) => {
    if (!err && fresult.found) {
      var file = fresult._source;
      internals.fileId2File[key] = file;
      internals.fileName2File[file.name] = file;
      return cb(file);
    }

    // Cache file is unknown
    internals.fileId2File[key] = null;
    return cb(null);
  });
};

exports.fileNameToFiles = function (name, cb) {
  var query;
  if (name[0] === "/" && name[name.length - 1] === "/") {
    query = {query: {regexp: {name: name.substring(1, name.length-1)}}, sort: [{num: {order: "desc"}}]};
  } else if (name.indexOf("*") !== -1) {
    query = {query: {wildcard: {name: name}}, sort: [{num: {order: "desc"}}]};
  }

  // Not wildcard/regex check the cache
  if (!query) {
    if (internals.fileName2File[name]) {
      return cb([internals.fileName2File[name]]);
    }
    query = {query: {term: {name: name}}, sort: [{num: {order: "desc"}}]};
  }

  exports.search('files', 'file', query, (err, data) => {
    var files = [];
    if (err || !data.hits) {
      return cb(null);
    }
    data.hits.hits.forEach((hit) => {
      var file = hit._source;
      var key = file.node + "!" + file.num;
      internals.fileId2File[key] = file;
      internals.fileName2File[file.name] = file;
      files.push(file);
    });
    return cb(files);
  });
};

exports.getSequenceNumber = function (name, cb) {
  exports.index("sequence", "sequence", name, {}, (err, sinfo) => {
    cb(err, sinfo._version);
  });
};

exports.numberOfDocuments = function (index, cb) {
  if (cb === undefined) {
    // Promise version
    return internals.elasticSearchClient.count({index: fixIndex(index), ignoreUnavailable:true});
  } else {
    // cb version - remove in future
    internals.elasticSearchClient.count({index: fixIndex(index), ignoreUnavailable:true}, (err, result) => {
      if (err || result.error) {
        return cb(null, 0);
      }

      return cb(null, result.count);
    });
  }
};

exports.updateFileSize = function (item, filesize) {
  exports.update("files", "file", item.id, {doc: {filesize: filesize}});
};

exports.checkVersion = function(minVersion, checkUsers) {
  var match = process.versions.node.match(/^(\d+)\.(\d+)\.(\d+)/);
  var version = parseInt(match[1], 10)*10000 + parseInt(match[2], 10) * 100 + parseInt(match[3], 10);
  if (version < 60000) {
    console.log("ERROR - Need at least node 6.0.0, currently using", process.version);
    process.exit(1);
    throw new Error("Exiting");
  }

  var index;

  ["stats", "dstats", "sequence", "files"].forEach((index) => {
    exports.indexStats(index, (err, status) => {
      if (err || status.error) {
        console.log("ERROR - Issue with index '" + index + "' make sure 'db/db.pl <eshost:esport> init' has been run", err, status);
        process.exit(1);
        throw new Error("Exiting");
      }
    });
  });

  internals.elasticSearchClient.indices.getTemplate({name: fixIndex("sessions2_template"), filter_path: "**._meta"}, (err, doc) => {
    if (err) {
      console.log("ERROR - Couldn't retrieve database version, is ES running?  Have you run ./db.pl host:port init?", err);
      process.exit(0);
    }
    try {
      var version = doc[fixIndex("sessions2_template")].mappings.session._meta.molochDbVersion;

      if (version < minVersion) {
          console.log("ERROR - Current database version (" + version + ") is less then required version (" + minVersion + ") use 'db/db.pl <eshost:esport> upgrade' to upgrade");
          process.exit(1);
      }
    } catch (e) {
      console.log("ERROR - Couldn't find database version.  Have you run ./db.pl host:port upgrade?");
      process.exit(0);
    }
  });

  if (checkUsers) {
    exports.numberOfUsers((err, num) => {
      if (num === 0) {
        console.log("WARNING - No users are defined, use node viewer/addUser.js to add one, or turn off auth by unsetting passwordSecret");
      }
    });
  }
};

exports.isLocalView = function(node, yesCB, noCB) {
  if (node === internals.nodeName) {
    return yesCB();
  }

  exports.molochNodeStatsCache(node, (err, stat) => {
    if (err || stat.hostname !== os.hostname()) {
      noCB();
    } else {
      yesCB();
    }
  });
};

exports.deleteFile = function(node, id, path, cb) {
  fs.unlink(path, () => {
    exports.deleteDocument('files', 'file', id, (err, data) => {
      cb(null);
    });
  });
};

exports.id2Index = function (id) {
  return 'sessions2-' + id.substr(0,id.indexOf('-'));
};

exports.loadFields = function(cb) {
  return exports.search("fields", "field", {size:1000}, cb);
};

exports.getIndices = function(startTime, stopTime, rotateIndex, cb) {
  var indices = [];
  exports.getAliasesCache("sessions2-*", (err, aliases) => {

    if (err || aliases.error) {
      return cb("");
    }

    var offset = 86400;
    if (rotateIndex === "hourly") {
      offset = 3600;
    } else if (rotateIndex === "hourly6") {
      offset = 3600*6;
    }

    startTime = Math.floor(startTime/offset)*offset;

    while (startTime < stopTime) {
      var iname;
      var d = new Date(startTime*1000);
      switch (rotateIndex) {
      case "monthly":
        iname = internals.prefix + "sessions2-" +
          twoDigitString(d.getUTCFullYear()%100) + 'm' +
          twoDigitString(d.getUTCMonth()+1);
        break;
      case "weekly":
        var jan = new Date(d.getUTCFullYear(), 0, 0);
        iname = internals.prefix + "sessions2-" +
          twoDigitString(d.getUTCFullYear()%100) + 'w' +
          twoDigitString(Math.floor((d - jan) / 604800000));
        break;
      case "hourly6":
        iname = internals.prefix + "sessions2-" +
          twoDigitString(d.getUTCFullYear()%100) +
          twoDigitString(d.getUTCMonth()+1) +
          twoDigitString(d.getUTCDate()) + 'h' +
          twoDigitString((d.getUTCHours()/6)*6);
        break;
      case "hourly":
        iname = internals.prefix + "sessions2-" +
          twoDigitString(d.getUTCFullYear()%100) +
          twoDigitString(d.getUTCMonth()+1) +
          twoDigitString(d.getUTCDate()) + 'h' +
          twoDigitString(d.getUTCHours());
        break;
      default:
        iname = internals.prefix + "sessions2-" +
          twoDigitString(d.getUTCFullYear()%100) +
          twoDigitString(d.getUTCMonth()+1) +
          twoDigitString(d.getUTCDate());
        break;
      }

      startTime += offset;

      if (aliases[iname] && (indices.length === 0 || iname !== indices[indices.length-1])) {
        indices.push(iname);
      }
    }

    if (indices.length === 0) {
      return cb("sessions2-*");
    }

    return cb(indices.join());
  });
};

exports.getMinValue = function(index, field, cb) {
  var params = {index: fixIndex(index), body: {size: 0, aggs: {min: {min: {field: field}}}}};
  return internals.elasticSearchClient.search(params, (err, data) => {
    if (err) { return cb(err, 0); }
    return cb(null, data.aggregations.min.value);
  });
};
