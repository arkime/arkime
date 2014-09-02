/******************************************************************************/
/* db.js -- Lowlevel and highlevel functions dealing with the database
 *
 * Copyright 2012-2014 AOL Inc. All rights reserved.
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
(function () {'use strict';} ());

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
                 apiVersion: "0.90",
                 q: []};

exports.initialize = function (info) {
  internals.dontMapTags = info.dontMapTags || false;
  delete info.dontMapTags;
  internals.info = info;

  internals.nodeName = info.nodeName;
  delete info.nodeName;

  internals.elasticSearchClient = new ESC.Client({
    host: internals.info.host,
    apiVersion: internals.apiVersion,
    requestTimeout: 300000,
    keepAlive: true,
    minSockets: 20,
    maxSockets: 21
  });

  // See if this is 1.1
  internals.elasticSearchClient.info(function(err,data) {
    if (data.version.number.match(/^1.0/)) {
      console.log("ES 1.0 is not supported");
      process.exit();
      throw new Error("Exiting");
    }

    var vmatch = data.version.number.match(/^1\.[1-3]/);
    if (vmatch) {
      if (data.version.number.match(/^1\.[1-3]/)) {
        internals.apiVersion = vmatch[0];
      } else {
        internals.apiVersion = "1.x";
      }
      var oldes = internals.elasticSearchClient;
      setTimeout(function() {oldes.close();}, 2000);
      internals.elasticSearchClient = new ESC.Client({
        host: internals.info.host,
        apiVersion: internals.apiVersion,
        requestTimeout: 300000,
        keepAlive: true,
        minSockets: 20,
        maxSockets: 21
      });
    }
  });

  // Replace tag implementation
  if (internals.dontMapTags) {
    exports.tagIdToName = function (id, cb) { return cb(id); };
    exports.tagNameToId = function (name, cb) { return cb(name); };
  }
};

//////////////////////////////////////////////////////////////////////////////////
//// Low level functions to convert from old style to new
//////////////////////////////////////////////////////////////////////////////////

exports.merge = function(to, from) {
  for (var key in from) {
    to[key] = from[key];
  }
};

exports.get = function (index, type, id, cb) {
  internals.elasticSearchClient.get({index: index, type: type, id: id}, function(err, data) {
    if (data && data.exists !== undefined) {
      data.found = data.exists;
      delete data.exists;
    }
    cb(err, data);
  });
};

exports.getWithOptions = function (index, type, id, options, cb) {
  var params = {index: index, type:type, id: id};
  exports.merge(params, options);
  internals.elasticSearchClient.get(params, function(err, data) {
    if (data && data.exists !== undefined) {
      data.found = data.exists;
      delete data.exists;
    }
    cb(err, data);
  });
};

exports.index = function (index, type, id, document, cb) {
  internals.elasticSearchClient.index({index: index, type: type, body: document, id: id}, cb);
};

exports.indexNow = function (index, type, id, document, cb) {
  internals.elasticSearchClient.index({index: index, type: type, body: document, id: id, refresh: 1}, cb);
};

exports.search = function (index, type, query, options, cb) {
  if (!cb) {
    cb = options;
    options = undefined;
  }
  var params = {index: index, type: type, body: query};
  exports.merge(params, options);

  if (query._source && internals.apiVersion === "0.90") {
    query.fields = query._source;
    delete query._source;
  }
  internals.elasticSearchClient.search(params, cb);
};

exports.searchPrimary = function (index, type, query, cb) {
  return exports.search(index, type, query, {preference: "_primary_first", ignoreIndices: "missing"}, cb);
};

exports.msearch = function (index, type, queries, cb) {
  var body = [];

  for(var i = 0, ilen = queries.length; i < ilen; i++){
    body.push({index: index, type: type});
    body.push(queries[i]);
  }

  internals.elasticSearchClient.msearch({body: body}, cb);
};

exports.scroll = function (params, callback) {
  internals.elasticSearchClient.scroll(params, callback);
};

exports.deleteByQuery = function (index, type, query, cb) {
  internals.elasticSearchClient.deleteByQuery({index: index, type: type, body: query}, cb);
};

exports.deleteDocument = function (index, type, id, options, cb) {
  if (!cb) {
    cb = options;
    options = undefined;
  }
  var params = {index: index, type: type, id: id};
  exports.merge(params, options);
  internals.elasticSearchClient.delete(params, cb);
};

exports.status = function(index, cb) {
  internals.elasticSearchClient.indices.status({index: index}, cb);
};

exports.health = function(cb) {
  internals.elasticSearchClient.cluster.health({}, cb);
};

exports.nodesStats = function (options, cb) {
  if (internals.apiVersion === "0.90") {
    internals.elasticSearchClient.cluster.nodeStats(options, function (err, data, status) {cb(err,data);});
  } else {
    internals.elasticSearchClient.nodes.stats(options, function (err, data, status) {cb(err,data);});
  }
};

exports.update = function (index, type, id, document, options, cb) {
  if (!cb) {
    cb = options;
    options = undefined;
  }

  var params = {index: index, type: type, body: document, id: id};
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
};

exports.getUser = function (name, cb) {
  if (internals.usersCache[name] && internals.usersCache[name]._timeStamp > Date.now() - 5000) {
    return cb(null, internals.usersCache[name]);
  }

  exports.get('users', 'user', name, function(err, suser) {
    if (err) {
      return cb(err, suser);
    }

    suser._timeStamp = Date.now();
    internals.usersCache[name] = suser;

    cb(null, suser);
  });
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

      internals.healthCache = health;
      internals.healthCache._timeStamp = Date.now();

      cb(null, health);
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
      return callback(null, internals.tagId2Name[task.id]);
    }
    var query = {query: {term: {n:task.id}}};
    exports.search('tags', 'tag', query, function(err, tdata) {
      if (!err && tdata.hits.hits[0]) {
        internals.tagId2Name[task.id] = tdata.hits.hits[0]._id;
        internals.tagName2Id[tdata.hits.hits[0]._id] = task.id;
        return callback(null, internals.tagId2Name[task.id]);
      }
      return callback(null, "<lookuperror>");
    });
  } else {
    if (internals.tagName2Id[task.name]) {
      return callback(null, internals.tagName2Id[task.name]);
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
  if (internals.fileId2File[key]) {
    return cb(internals.fileId2File[key]);
  }

  exports.get('files', 'file', node + '-' + num, function (err, fresult) {
    if (!err && fresult.found) {
      var file = fresult._source;
      internals.fileId2File[key] = file;
      internals.fileName2File[file.name] = file;
      return cb(file);
    }

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
  exports.status(index, function(err, result) {
    if (err || result.error) {
      return cb(null, 0);
    }

    var i;
    var num = 0;
    for (i in result.indices) {
      if (typeof result.indices[i] === "object" && result.indices[i].docs) {
        num += result.indices[i].docs.num_docs;
      }
    }
    cb(null, num);
  });
};

exports.updateFileSize = function (item, filesize) {
  exports.update("files", "file", item.id, {doc: {filesize: filesize}});
};

exports.checkVersion = function(minVersion, checkUsers) {
  var match = process.versions.node.match(/^(\d+)\.(\d+)\.(\d+)/);
  var version = parseInt(match[1], 10)*10000 + parseInt(match[2], 10) * 100 + parseInt(match[3], 10);
  if (version < 1020) {
    console.log("ERROR - Need at least node 0.10.20, currently using", process.version);
    process.exit(1);
    throw new Error("Exiting");
  }

  var index;

  ["stats", "dstats", "tags", "sequence", "files", "users"].forEach(function(index) {
    exports.status(index, function(err, status) {
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
    exports.numberOfDocuments("users", function(err, num) {
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
  exports.status("sessions-*", function(err, status) {

    if (err || status.error) {
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
        iname = "sessions-" +
          twoDigitString(d.getUTCFullYear()%100) + 'm' +
          twoDigitString(d.getUTCMonth()+1);
        break;
      case "weekly":
        var jan = new Date(d.getUTCFullYear(), 0, 0);
        iname = "sessions-" +
          twoDigitString(d.getUTCFullYear()%100) + 'w' +
          twoDigitString(Math.floor((d - jan) / 604800000));
        break;
      case "hourly":
        iname = "sessions-" +
          twoDigitString(d.getUTCFullYear()%100) +
          twoDigitString(d.getUTCMonth()+1) +
          twoDigitString(d.getUTCDate()) + 'h' +
          twoDigitString(d.getUTCHours());
        break;
      default:
        iname = "sessions-" +
          twoDigitString(d.getUTCFullYear()%100) +
          twoDigitString(d.getUTCMonth()+1) +
          twoDigitString(d.getUTCDate());
        break;
      }

      startTime += offset;

      if (status.indices[iname] && (indices.length === 0 || iname !== indices[indices.length-1])) {
        indices.push(iname);
      }
    }

    if (indices.length === 0) {
      return cb("sessions-*");
    }

    return cb(indices.join());
  });
};
