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

var internals = {tagId2Name: {},
                 tagName2Id: {},
                 fileId2File: {},
                 fileName2File: {},
                 molochNodeStatsCache: {},
                 healthCache: {},
                 usersCache: {},
                 qInProgress: 0,
                 apiVersion: "2.x",
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

  internals.elasticSearchClient.info(function(err,data) {
    if (err) {
      console.log(err, data);
    }
    if (data.version.number.match(/^(2.[0-3]|1|0)/)) {
      console.log("ERROR - ES", data.version.number, "not supported, ES 2.4.x or later required.");
      process.exit();
      throw new Error("Exiting");
    }
    exports.isES5 = data.version.number.match(/^5/);

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
    exports.tagIdToName = function (id, cb) { return cb(id); };
    exports.tagNameToId = function (name, cb) { return cb(name); };
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
  if (Array.isArray(index)) {
    return index.map(function(val) {
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
  internals.elasticSearchClient.get({index: fixIndex(index), type: type, id: id}, cb);
};

exports.getWithOptions = function (index, type, id, options, cb) {
  var params = {index: fixIndex(index), type:type, id: id};
  exports.merge(params, options);
  internals.elasticSearchClient.get(params, cb);
};

exports.index = function (index, type, id, document, cb) {
  internals.elasticSearchClient.index({index: fixIndex(index), type: type, body: document, id: id}, cb);
};

exports.indexNow = function (index, type, id, document, cb) {
  internals.elasticSearchClient.index({index: fixIndex(index), type: type, body: document, id: id, refresh: 1}, cb);
};

exports.search = function (index, type, query, options, cb) {
  if (!cb) {
    cb = options;
    options = undefined;
  }
  var params = {index: fixIndex(index), type: type, body: query};
  exports.merge(params, options);
  internals.elasticSearchClient.search(params, cb);
};

exports.searchScroll = function (index, type, query, options, cb) {
  if (!cb) {
    cb = options;
    options = {};
  }

  if ((query.size || 0) + (parseInt(query.from,10) || 0) >= 10000) {
    var totalResults;
    var params = {scroll: '1m'};
    exports.merge(params, options);
    var querySize = query.size;
    delete query.size
    exports.search(index, type, query, params,
      function getMoreUntilDone(error, response) {
        if (totalResults === undefined) {
          totalResults = response;
        } else {
          Array.prototype.push.apply(totalResults.hits.hits, response.hits.hits);
        }

        if (!error && totalResults.hits.total > 0 && totalResults.hits.hits.length < Math.min(response.hits.total, querySize)) {
          exports.scroll({
            scrollId: response._scroll_id,
            scroll: '1m',
            body: { // ALW - Remove someday - https://github.com/elastic/elasticsearch-php/issues/564
              scroll_id: response._scroll_id,
            }
          }, getMoreUntilDone);
        } else {
            cb(error, totalResults);
        }
      });
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

  internals.elasticSearchClient.msearch({body: body}, cb);
};

exports.scroll = function (params, callback) {
  internals.elasticSearchClient.scroll(params, callback);
};

exports.deleteByQuery = function (index, type, query, cb) {
  internals.elasticSearchClient.deleteByQuery({index: fixIndex(index), type: type, body: query}, cb);
};

exports.deleteDocument = function (index, type, id, options, cb) {
  if (!cb) {
    cb = options;
    options = undefined;
  }
  var params = {index: fixIndex(index), type: type, id: id};
  exports.merge(params, options);
  internals.elasticSearchClient.delete(params, cb);
};

exports.indexStats = function(index, cb) {
  internals.elasticSearchClient.indices.stats({index: fixIndex(index)}, cb);
};

exports.getAliases = function(index, cb) {
  internals.elasticSearchClient.indices.getAliases({index: fixIndex(index)}, cb);
};

exports.getAliasesCache = function (index, cb) {
  if (internals.aliasesCache && internals.aliasesCacheTimeStamp > Date.now() - 5000) {
    return cb(null, internals.aliasesCache);
  }

  exports.getAliases(index, function(err, aliases) {
    if (err) {
      return cb(err, aliases);
    }

    internals.aliasesCacheTimeStamp = Date.now();
    internals.aliasesCache = aliases;

    cb(null, aliases);
  });
};

exports.health = function(cb) {
  internals.elasticSearchClient.info(function(err,data) {
    internals.elasticSearchClient.cluster.health({}, function(err, result) {
      if (data && result) {
        result.version = data.version.number;
      }
      return cb(err, result);
    });
  });
};

exports.nodesStats = function (options, cb) {
  return internals.elasticSearchClient.nodes.stats(options, function (err, data, status) {cb(err,data);});
};

exports.update = function (index, type, id, document, options, cb) {
  if (!cb) {
    cb = options;
    options = undefined;
  }

  var params = {index: fixIndex(index), type: type, body: document, id: id};
  exports.merge(params, options);
  internals.elasticSearchClient.update(params, cb);
};

exports.close = function () {
  internals.elasticSearchClient.close();
};

//////////////////////////////////////////////////////////////////////////////////
//// High level functions
//////////////////////////////////////////////////////////////////////////////////
exports.flushCache = function () {
  internals.tagId2Name = {};
  internals.tagName2Id = {};
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
  internals.usersElasticSearchClient.get({index: internals.usersPrefix + 'users', type: 'user', id: name}, cb);
};

exports.getUserCache = function (name, cb) {
  if (internals.usersCache[name] && internals.usersCache[name]._timeStamp > Date.now() - 5000) {
    return cb(null, internals.usersCache[name]);
  }

  exports.getUser(name, function(err, suser) {
    if (err) {
      return cb(err, suser);
    }

    suser._timeStamp = Date.now();
    internals.usersCache[name] = suser;

    cb(null, suser);
  });
};

exports.numberOfUsers = function(cb) {
  internals.usersElasticSearchClient.count({index: internals.usersPrefix + 'users', ignoreUnavailable:true}, function(err, result) {
    if (err || result.error) {
      return cb(null, 0);
    }

    return cb(null, result.count);
  });
};

exports.deleteUser = function (name, cb) {
  delete internals.usersCache[name];
  return internals.usersElasticSearchClient.delete({index: internals.usersPrefix + 'users', type: 'user', id: name, refresh: 1}, cb);
};

exports.setUser = function(name, doc, cb) {
  delete internals.usersCache[name];
  return internals.usersElasticSearchClient.index({index: internals.usersPrefix + 'users', type: 'user', body: doc, id: name, refresh: 1}, cb);
};

exports.molochNodeStats = function (name, cb) {
  exports.get('stats', 'stat', name, function(err, stat) {
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

  return exports.health(function(err, health) {
      if (err) {
        // Even if an error, if we have a cache use it
        if (internals.healthCache._timeStamp !== undefined) {
          return cb(null, internals.healthCache);
        }
        return cb(err, null);
      }

      exports.get("dstats", "version", "version", function(err, doc) {
        if (doc !== undefined && doc._source !== undefined) {
          health.molochDbVersion = doc._source.version;
        }
        internals.healthCache = health;
        internals.healthCache._timeStamp = Date.now();
        cb(null, health);
      });
  });
};

exports.hostnameToNodeids = function (hostname, cb) {
  var query = {query: {match: {hostname:hostname}}};
  exports.search('stats', 'stat', query, function(err, sdata) {
    var nodes = [];
    if (sdata && sdata.hits && sdata.hits.hits) {
      for (var i = 0, ilen = sdata.hits.hits.length; i < ilen; i++) {
        nodes.push(sdata.hits.hits[i]._id);
      }
    }
    cb(nodes);
  });
};

function tagWorker(task, callback) {
  if (task.type === "tagIdToName") {
    if (internals.tagId2Name[task.id]) {
      return setImmediate(callback, null, internals.tagId2Name[task.id]);
    }
    var query = {query: {term: {n:task.id}}};
    exports.search('tags', 'tag', query, function(err, tdata) {
      if (!err && tdata.hits.hits[0]) {
        internals.tagId2Name[task.id] = tdata.hits.hits[0]._id;
        internals.tagName2Id[tdata.hits.hits[0]._id] = task.id;
        return callback(null, internals.tagId2Name[task.id]);
      }
      console.log("LOOKUPERROR", query, err, tdata.hits);
      return callback(null, "<lookuperror>");
    });
  } else {
    if (internals.tagName2Id[task.name]) {
      return setImmediate(callback, null, internals.tagName2Id[task.name]);
    }

    exports.get('tags', 'tag', task.name, function(err, tdata) {
      if (!err && tdata.found) {
        internals.tagName2Id[task.name] = tdata._source.n;
        internals.tagId2Name[tdata._source.n] = task.name;
        return callback(null, internals.tagName2Id[task.name]);
      }
      return callback(null, -1);
    });
  }
}

internals.tagQueue = async.queue(tagWorker, 5);

exports.tagIdToName = function (id, cb) {
  internals.tagQueue.push({id: id, type: "tagIdToName"}, function (err, data) {
    return cb(data);
  });
};

exports.tagNameToId = function (name, cb) {
  internals.tagQueue.push({name: name, type: "tagNameToId"}, function (err, data) {
    return cb(data);
  });
};

exports.fileIdToFile = function (node, num, cb) {
  var key = node + "!" + num;
  if (internals.fileId2File[key] !== undefined) {
    return cb(internals.fileId2File[key]);
  }

  exports.get('files', 'file', node + '-' + num, function (err, fresult) {
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

  exports.search('files', 'file', query, function(err, data) {
    var files = [];
    if (err || !data.hits) {
      return cb(null);
    }
    data.hits.hits.forEach(function(hit) {
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
  exports.index("sequence", "sequence", name, {}, function (err, sinfo) {
    cb(err, sinfo._version);
  });
};

exports.createTag = function (name, cb) {
  // Only allow 1 create at a time, the lazy way
  if (exports.createTag.inProgress === 1) {
    setTimeout(exports.createTag, 50, name, cb);
    return;
  }

  // Was already created while waiting
  if (internals.tagName2Id[name]) {
    return cb(internals.tagName2Id[name]);
  }

  // Do a create
  exports.createTag.inProgress = 1;
  exports.getSequenceNumber("tags", function (err, num) {
    exports.index("tags", "tag", name, {n: num}, function (err, tinfo) {
      exports.createTag.inProgress = 0;
      internals.tagId2Name[num] = name;
      internals.tagName2Id[name] = num;
      cb(num);
    });
  });
};

exports.numberOfDocuments = function (index, cb) {
  internals.elasticSearchClient.count({index: fixIndex(index), ignoreUnavailable:true}, function(err, result) {
    if (err || result.error) {
      return cb(null, 0);
    }

    return cb(null, result.count);
  });
};

exports.updateFileSize = function (item, filesize) {
  exports.update("files", "file", item.id, {doc: {filesize: filesize}});
};

exports.checkVersion = function(minVersion, checkUsers) {
  var match = process.versions.node.match(/^(\d+)\.(\d+)\.(\d+)/);
  var version = parseInt(match[1], 10)*10000 + parseInt(match[2], 10) * 100 + parseInt(match[3], 10);
  if (version < 40600) {
    console.log("ERROR - Need at least node 4.6.0, currently using", process.version);
    process.exit(1);
    throw new Error("Exiting");
  }

  var index;

  ["stats", "dstats", "tags", "sequence", "files"].forEach(function(index) {
    exports.indexStats(index, function(err, status) {
      if (err || status.error) {
        console.log("ERROR - Issue with index '" + index + "' make sure 'db/db.pl <eshost:esport> init' has been run", err, status);
        process.exit(1);
        throw new Error("Exiting");
      }
    });
  });

  exports.get("dstats", "version", "version", function(err, doc) {
    var version;
    if (err) {
      console.log("ERROR - Couldn't retrieve database version, is ES running?  Have you run ./db.pl host:port init?", err);
      process.exit(0);
      throw new Error("Exiting");
    }
    if (!doc.found) {
      version = 0;
    } else {
      version = doc._source.version;
    }

    if (version < minVersion) {
        console.log("ERROR - Current database version (" + version + ") is less then required version (" + minVersion + ") use 'db/db.pl <eshost:esport> upgrade' to upgrade");
        process.exit(1);
        throw new Error("Exiting");
    }
  });

  if (checkUsers) {
    exports.numberOfUsers(function(err, num) {
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

  exports.molochNodeStatsCache(node, function(err, stat) {
    if (err || stat.hostname !== os.hostname()) {
      noCB();
    } else {
      yesCB();
    }
  });
};

exports.deleteFile = function(node, id, path, cb) {
  fs.unlink(path, function() {
    exports.deleteDocument('files', 'file', id, function(err, data) {
      cb(null);
    });
  });
};

exports.id2Index = function (id) {
  return 'sessions-' + id.substr(0,id.indexOf('-'));
};

exports.loadFields = function(cb) {
  exports.search("fields", "field", {size:1000}, function (err, data) {
    if (err) {
      return cb([]);
    }
    cb(data.hits.hits);
  });
};

function twoDigitString(value) {
  return (value < 10) ? ("0" + value) : value.toString();
}

exports.getIndices = function(startTime, stopTime, rotateIndex, cb) {
  var indices = [];
  exports.getAliasesCache("sessions-*", function(err, aliases) {

    if (err || aliases.error) {
      return cb("");
    }

    var offset = 86400;
    if (rotateIndex === "hourly") {
      offset = 3600;
    }

    startTime = Math.floor(startTime/offset)*offset;

    while (startTime < stopTime) {
      var iname;
      var d = new Date(startTime*1000);
      switch (rotateIndex) {
      case "monthly":
        iname = internals.prefix + "sessions-" +
          twoDigitString(d.getUTCFullYear()%100) + 'm' +
          twoDigitString(d.getUTCMonth()+1);
        break;
      case "weekly":
        var jan = new Date(d.getUTCFullYear(), 0, 0);
        iname = internals.prefix + "sessions-" +
          twoDigitString(d.getUTCFullYear()%100) + 'w' +
          twoDigitString(Math.floor((d - jan) / 604800000));
        break;
      case "hourly":
        iname = internals.prefix + "sessions-" +
          twoDigitString(d.getUTCFullYear()%100) +
          twoDigitString(d.getUTCMonth()+1) +
          twoDigitString(d.getUTCDate()) + 'h' +
          twoDigitString(d.getUTCHours());
        break;
      default:
        iname = internals.prefix + "sessions-" +
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
      return cb("sessions-*");
    }

    return cb(indices.join());
  });
};
