/******************************************************************************/
/* db.js -- Lowlevel and highlevel functions dealing with the database
 *
 * Copyright 2012-2013 AOL Inc. All rights reserved.
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

var ESC            = require('elasticsearchclient'),
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

  internals.elasticSearchClient = new ESC(info);
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
    setTimeout(function () {item[0].apply(item[1], item[2]);}, 0);
  }
}

//////////////////////////////////////////////////////////////////////////////////
//// Low level functions to undo the data/error seperate callbacks
//////////////////////////////////////////////////////////////////////////////////

exports.get = function (index, type, query, cb) {
  internals.elasticSearchClient.get(index, type, query)
    .on('data', function(data) {
      cb(null, JSON.parse(data));
    })
    .on('error', function(error) {
      cb(error, null);
    })
    .exec();
};

exports.getWithOptions = function (index, type, query, options, cb) {
  internals.elasticSearchClient.get(index, type, query, options)
    .on('data', function(data) {
      cb(null, JSON.parse(data));
    })
    .on('error', function(error) {
      cb(error, null);
    })
    .exec();
};

/* Work around a breaking change where document.id is nolonger used for the id */
if (typeof ESC.prototype.multisearch === "function") {
  exports.index = function (index, type, id, document, cb) {
    internals.elasticSearchClient.index(index, type, document, id)
      .on('data', function(data) {
        cb(null, JSON.parse(data));
      })
      .on('error', function(error) {
        cb(error, null);
      })
      .exec();
  };

  exports.indexNow = function (index, type, id, document, cb) {
    internals.elasticSearchClient.index(index, type, document, id, {refresh: 1})
      .on('data', function(data) {
        cb(null, JSON.parse(data));
      })
      .on('error', function(error) {
        cb(error, null);
      })
      .exec();
  };
} else {
  exports.index = function (index, type, id, document, cb) {
    document.id = id;

    internals.elasticSearchClient.index(index, type, document)
      .on('data', function(data) {
        cb(null, JSON.parse(data));
      })
      .on('error', function(error) {
        cb(error, null);
      })
      .exec();
  };

  exports.indexNow = function (index, type, id, document, cb) {
    document.id = id;

    internals.elasticSearchClient.index(index, type, document, {refresh: 1})
      .on('data', function(data) {
        cb(null, JSON.parse(data));
      })
      .on('error', function(error) {
        cb(error, null);
      })
      .exec();
  };
}

exports.search = function (index, type, query, cb) {
  internals.elasticSearchClient.search(index, type, query)
    .on('data', function(data) {
      cb(null, JSON.parse(data));
    })
    .on('error', function(error) {
      cb(error, null);
    })
    .exec();
};

exports.searchPrimary = function (index, type, query, cb) {
  internals.elasticSearchClient.search(index, type, query, {preference: "_primary_first"})
    .on('data', function(data) {
      cb(null, JSON.parse(data));
    })
    .on('error', function(error) {
      cb(error, null);
    })
    .exec();
};

exports.msearch = function (index, type, queries, cb) {
  var path = '/' + index + "/" + type + "/_msearch";

  var buf='';
  for(var i=0; i<queries.length;i++){
    buf += "{}\n";
    buf += queries[i] + "\n";
  }

  internals.elasticSearchClient.createCall({data:buf, path:path, method: "POST"}, internals.elasticSearchClient.clientOptions)
    .on('data', function(data) {
      cb(null, JSON.parse(data));
    })
    .on('error', function(error) {
      cb(error, null);
    })
    .exec();
};

exports.deleteByQuery = function (index, type, query, cb) {
  internals.elasticSearchClient.deleteByQuery(index, type, query)
    .on('data', function(data) {
      cb(null, JSON.parse(data));
    })
    .on('error', function(error) {
      cb(error, null);
    })
    .exec();
};

exports.deleteDocument = function (index, type, id, cb) {
  internals.elasticSearchClient.deleteDocument(index, type, id, {refresh:true})
    .on('data', function(data) {
      cb(null, JSON.parse(data));
    })
    .on('error', function(error) {
      cb(error, null);
    })
    .exec();
};

exports.status = function(index, cb) {
  internals.elasticSearchClient.status(index)
    .on('data', function(data) {
      cb(null, JSON.parse(data));
    })
    .on('error', function(error) {
      cb(error, null);
    })
    .exec();
};

exports.health = function(cb) {
  internals.elasticSearchClient.health()
    .on('data', function(data) {
      cb(null, JSON.parse(data));
    })
    .on('error', function(error) {
      cb(error, null);
    })
    .exec();
};

exports.nodesStats = function (options, cb) {
  internals.elasticSearchClient.nodesStats(null, options)
    .on('data', function(data) {
      cb(null, JSON.parse(data));
    })
    .on('error', function(error) {
      cb(error, null);
    })
    .exec();
};

exports.update = function (indexName, typeName, documentId, document, cb) {
    var path = '/' + indexName + '/' + typeName + '/' + documentId + '/_update';
    return internals.elasticSearchClient.createCall({data: JSON.stringify(document),
                                                     path: path,
                                                     method: 'POST'}, internals.elasticSearchClient.clientOptions)
    .on('data', function(data) {
      cb(null, JSON.parse(data));
    })
    .on('error', function(error) {
      cb(error, null);
    })
    .exec();
};

exports.esVersion = function (cb) {
  internals.elasticSearchClient.createCall({data:"",path:"/",method: "GET"}, internals.elasticSearchClient.clientOptions)
    .on('data', function(data) {
      data = JSON.parse(data);
      var matches = data.version.number.match(/^(\d+).(\d+).(\d+)/);
      cb(((+matches[1]) << 16) | ((+matches[2]) << 8) | (+matches[3]));
    })
    .exec();
};

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
      var i;
      for (i = 0; i < sdata.hits.hits.length; i++) {
        nodes.push(sdata.hits.hits[i]._id);
      }
    }
    cb(nodes);
  });
};

exports.tagIdToName = function (id, cb) {
  if (internals.dontMapTags) {
    return cb(id);
  }

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

exports.tagNameToId = function (name, cb) {
  if (internals.dontMapTags) {
    return cb(name);
  }

  if (!canDo(exports.tagNameToId, this, arguments)) {
    return;
  }

  if (internals.tagName2Id[name]) {
    cb(internals.tagName2Id[name]);
    return didIt();
  }

  exports.get('tags', 'tag', encodeURIComponent(name), function(err, tdata) {
    didIt();
    if (!err && tdata.exists) {
      internals.tagName2Id[name] = tdata._source.n;
      internals.tagId2Name[tdata._source.n] = name;
      return cb(internals.tagName2Id[name]);
    }
    return cb(-1);
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
    setTimeout(function () {exports.createTag(name, cb);}, 50);
    return;
  }

  // Was already created while waiting
  if (internals.tagName2Id[name]) {
    return cb(internals.tagName2Id[name]);
  }

  // Do a create
  exports.createTag.inProgress = 1;
  exports.getSequenceNumber("tags", function (err, num) {
    exports.index("tags", "tag", encodeURIComponent(name), {n: num}, function (err, tinfo) {
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
      if (typeof result.indices[i] === "object") {
        num += result.indices[i].docs.num_docs;
      }
    }
    cb(null, num);
  });
};

exports.updateFileSize = function (item, filesize) {
  // _update has bug so do ourselves
  internals.elasticSearchClient.createCall({data:JSON.stringify({script: "ctx._source.filesize = " + filesize}),
                                            path:"/files/file/" + item.id + "/_update",
                                            method: "POST"}, internals.elasticSearchClient.clientOptions)
    .on('error', function(error) {
      console.log("ERROR - updateFileSize", error);
    })
    .exec();
};

exports.checkVersion = function(minVersion, checkUsers) {
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
    if (!doc.exists) {
      version = 0;
    } else {
      version = doc._source.version;
    }

    if (version < minVersion) {
        console.log("ERROR - Current database version (" + version + ") is less then required version (" + MIN_DB_VERSION + ") use 'db/db.pl <eshost:esport> upgrade' to upgrade");
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
