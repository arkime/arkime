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
"use strict";

var ESC            = require('elasticsearch'),
    async          = require('async'),
    os             = require('os'),
    fs             = require('fs'),
    util           = require('util');

var internals = {tagId2Name: {},
                 tagName2Id: {},
                 fileId2File: {},
                 fileName2File: {},
                 qInProgress: 0,
                 q: []};

exports.initialize = function (info) {
  internals.dontMapTags = info.dontMapTags || false;
  delete info.dontMapTags;

  internals.nodeName = info.nodeName;
  delete info.nodeName;

  internals.elasticSearchClient = new ESC.Client({
    host: info.host + ":" + info.port,
    apiVersion: "0.90",
    requestTimeout: 300000
  });

  // Replace tag implementation
  if (internals.dontMapTags) {
    exports.tagIdToName = function (id, cb) { return cb(id); };
    exports.tagNameToId = function (name, cb) { return cb(name); };
  }
};

//////////////////////////////////////////////////////////////////////////////////
//// Very simple throttling Q
//////////////////////////////////////////////////////////////////////////////////
function canDo(fn, that, args) {
  if (internals.qInProgress < 5) {
    internals.qInProgress++;
    return true;
  }
  internals.q.push([fn, that, args]);
  return false;
}

function didIt() {
  internals.qInProgress--;
  if (internals.q.length > 0) {
    var item = internals.q.shift();
    async.setImmediate(function () {item[0].apply(item[1], item[2]);});
  }
}

//////////////////////////////////////////////////////////////////////////////////
//// Low level functions to convert from old style to new
//////////////////////////////////////////////////////////////////////////////////

function merge(to, from) {
  for (var key in from) {
    to[key] = from[key];
  }
}


exports.get = function (index, type, id, cb) {
  internals.elasticSearchClient.get({index: index, type: type, id: id}, cb);
};

exports.getWithOptions = function (index, type, id, options, cb) {
  var params = {index: index, type:type, id: id};
  merge(params, options);
  internals.elasticSearchClient.get(params, cb);
};

/* Work around a breaking change where document.id is nolonger used for the id */
exports.index = function (index, type, id, document, cb) {
  internals.elasticSearchClient.index({index: index, type: type, body: document, id: id}, cb);
};

exports.indexNow = function (index, type, id, document, cb) {
  internals.elasticSearchClient.index({index: index, type: type, body: document, id: id, refresh: 1}, cb);
};

exports.search = function (index, type, query, cb) {
  internals.elasticSearchClient.search({index: index, type: type, body: query}, cb);
};

exports.searchPrimary = function (index, type, query, cb) {
  internals.elasticSearchClient.search({index: index, type: type, body: query, preference: "_primary_first", ignoreIndices: "missing"}, cb);
};

exports.msearch = function (index, type, queries, cb) {
  var body = [];

  for(var i = 0, ilen = queries.length; i < ilen; i++){
    body.push({index: index, type: type});
    body.push(queries[i]);
  }

  internals.elasticSearchClient.msearch({body: body}, cb);
};

exports.deleteByQuery = function (index, type, query, cb) {
  internals.elasticSearchClient.deleteByQuery({index: index, type: type, body: query}, cb);
};

exports.deleteDocument = function (index, type, id, cb) {
  internals.elasticSearchClient.delete({index: index, type: type, id: id}, cb);
};

exports.status = function(index, cb) {
  internals.elasticSearchClient.indices.status({index: index}, cb);
};

exports.health = function(cb) {
  internals.elasticSearchClient.cluster.health({}, cb);
};

exports.nodesStats = function (options, cb) {
  internals.elasticSearchClient.cluster.nodeStats(options, function (err, data, status) {cb(err,data);});
};

exports.update = function (index, type, id, document, cb) {
  internals.elasticSearchClient.update({index: index, type: type, body: document, id: id}, cb);
};

exports.close = function () {
  internals.elasticSearchClient.close();
}

//////////////////////////////////////////////////////////////////////////////////
//// High level functions
//////////////////////////////////////////////////////////////////////////////////
internals.molochNodeStatsCache = {};

exports.molochNodeStats = function (name, cb) {
  exports.get('stats', 'stat', name, function(err, stat) {
    if (err || !stat.exists) {

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


internals.healthCache = {};
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
  var query = {query: {text: {hostname:hostname}}};
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

exports.tagIdToName = function (id, cb) {
  if (!canDo(exports.tagIdToName, this, arguments)) {
    return;
  }

  if (internals.tagId2Name[id]) {
    cb(internals.tagId2Name[id]);
    return didIt();
  }

  var query = {query: {term: {n:id}}};
  exports.search('tags', 'tag', query, function(err, tdata) {
    didIt();

    if (!err && tdata.hits.hits[0]) {
      internals.tagId2Name[id] = tdata.hits.hits[0]._id;
      internals.tagName2Id[tdata.hits.hits[0]._id] = id;
      return cb(internals.tagId2Name[id]);
    }

    return cb(null);
  });
};

exports.tagNameToId = function (name, cb) {
  if (!canDo(exports.tagNameToId, this, arguments)) {
    return;
  }

  if (internals.tagName2Id[name]) {
    cb(internals.tagName2Id[name]);
    return didIt();
  }

  exports.get('tags', 'tag', name, function(err, tdata) {
    console.log("taglookup", name, err, tdata);
    didIt();
    if (!err && tdata.exists) {
      internals.tagName2Id[name] = tdata._source.n;
      internals.tagId2Name[tdata._source.n] = name;
      return cb(internals.tagName2Id[name]);
    }
    return cb(-1);
  });
};

exports.fileIdToFile = function (node, num, cb) {
  var key = node + "!" + num;
  if (internals.fileId2File[key]) {
    return cb(internals.fileId2File[key]);
  }

  exports.get('files', 'file', node + '-' + num, function (err, fresult) {
    if (!err && fresult.exists) {
      var file = fresult._source;
      internals.fileId2File[key] = file;
      internals.fileName2File[file.name] = file;
      return cb(file);
    }

    return cb(null);
  });
};

exports.fileNameToFile = function (name, cb) {
  if (internals.fileName2File[name]) {
    return cb(internals.fileName2File[name]);
  }

  var query = {query: {term: {name: name}}};
  exports.search('files', 'file', query, function(err, data) {
    if (!err && data.hits.hits[0]) {
      var file = data.hits.hits[0]._source;
      var key = file.node + "!" + file.num;
      internals.fileId2File[key] = file;
      internals.fileName2File[file.name] = file;
      return cb(file);
    }

    return cb(null);
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
  }

  var index;

  ["stats", "dstats", "tags", "sequence", "files", "users"].forEach(function(index) {
    exports.status(index, function(err, status) {
      if (err || status.error) {
        console.log("ERROR - Issue with index '" + index + "' make sure 'db/db.pl <eshost:esport> init' has been run", err, status);
        process.exit(1);
      }
    });
  });

  exports.get("dstats", "version", "version", function(err, doc) {
    var version;
    if (err) {
      console.log(err);
      process.exit(0);
    }
    if (!doc.exists) {
      version = 0;
    } else {
      version = doc._source.version;
    }

    if (version < minVersion) {
        console.log("ERROR - Current database version (" + version + ") is less then required version (" + minVersion + ") use 'db/db.pl <eshost:esport> upgrade' to upgrade");
        process.exit(1);
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
    cb(data.hits.hits);
  });
};
