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
    os             = require('os'),
    fs             = require('fs');

var internals = {fileId2File: {},
                 fileName2File: {},
                 molochNodeStatsCache: {},
                 healthCache: {},
                 indicesCache: {},
                 indicesSettingsCache: {},
                 usersCache: {},
                 lookupsCache: {},
                 nodesStatsCache: {},
                 nodesInfoCache: {},
                 masterCache: {},
                 qInProgress: 0,
                 apiVersion: "6.7",
                 q: []};

exports.initialize = function (info, cb) {
  internals.dontMapTags = info.dontMapTags === 'true' || info.dontMapTags === true || false;
  internals.debug = info.debug || 0;
  delete info.dontMapTags;
  delete info.debug;

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

  var esSSLOptions =  {rejectUnauthorized: !internals.info.insecure, ca: internals.info.ca};
  if(info.esClientKey) {
    esSSLOptions.key = fs.readFileSync(info.esClientKey);
    esSSLOptions.cert = fs.readFileSync(info.esClientCert);
    if(info.esClientKeyPass) {
       esSSLOptions.passphrase = info.esClientKeyPass;
    }
  }

  internals.elasticSearchClient = new ESC.Client({
    host: internals.info.host,
    apiVersion: internals.apiVersion,
    requestTimeout: (parseInt(info.requestTimeout, 10) + 30) * 1000 || 330000,
    keepAlive: true,
    minSockets: 20,
    maxSockets: 51,
    ssl: esSSLOptions
  });

  internals.elasticSearchClient.info((err,data) => {
    if (err) {
      console.log(err, data);
    }
    if (data.version.number.match(/^(6.[0-6]|[0-5]|8)/)) {
      console.log("ERROR - ES", data.version.number, "not supported, ES 6.7.x or later required.");
      process.exit();
      throw new Error("Exiting");
    }

    if (info.usersHost) {
      internals.usersElasticSearchClient = new ESC.Client({
        host: internals.info.usersHost,
        apiVersion: internals.apiVersion,
        requestTimeout: info.requestTimeout*1000 || 300000,
        keepAlive: true,
        minSockets: 5,
        maxSockets: 6,
        ssl: esSSLOptions
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
  if (!cb && typeof options === 'function') {
    cb = options;
    options = undefined;
  }

  let params = {
    index: fixIndex(index),
    type: type,
    body: query, rest_total_hits_as_int: true
  };

  exports.merge(params, options);

  return internals.elasticSearchClient.search(params, cb);
};

exports.cancelByOpaqueId = function(cancelId, cb) {
  internals.elasticSearchClient.tasks.list({detailed: "false", group_by: "parents"})
    .then((results) => {
      let found = false;

      for (let resultKey in results.tasks) {
        let result = results.tasks[resultKey];
        if (result.headers &&
          result.headers['X-Opaque-Id'] &&
          result.headers['X-Opaque-Id'] === cancelId) {
          found = true;
          internals.elasticSearchClient.tasks.cancel({ taskId: resultKey }, () => {});
        }
      }

      // not found, return error
      if (!found) { return cb('cancel id not found, cannot cancel es task(s)'); }

      return cb();
    })
    .catch((error) => {
      return cb(error);
    });
};

function searchScrollInternal(index, type, query, options, cb) {
  let from = +query.from || 0;
  let size = +query.size || 0;

  let querySize = from + size;
  delete query.from;

  var totalResults;
  var params = {scroll: '5m'};
  exports.merge(params, options);
  query.size = 1000; // Get 1000 items per scroll call
  exports.search(index, type, query, params,
    function getMoreUntilDone(error, response) {
      if (error) {
        if (totalResults && from > 0) {
          totalResults.hits.hits = totalResults.hits.hits.slice(from);
        }
        if (response && response._scroll_id) {
          exports.clearScroll({body:{scroll_id: response._scroll_id}});
        }
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
        if (totalResults && from > 0) {
          totalResults.hits.hits = totalResults.hits.hits.slice(from);
        }
        exports.clearScroll({body:{scroll_id: response._scroll_id}});
        return cb(null, totalResults);
      }
    });
}

exports.searchScroll = function (index, type, query, options, cb) {
  if ((query.size || 0) + (parseInt(query.from, 10) || 0) >= 10000) {
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
  // ALW - FIXME - 6.1+ has removed primary_first :(
  let params = { preference: 'primaries', ignore_unavailable: 'true' };

  if (options && options.cancelId) {
    // set X-Opaque-Id header on the params so the task can be canceled
    params.headers = { 'X-Opaque-Id': options.cancelId };
  }

  exports.merge(params, options);
  delete params.cancelId;
  return exports.searchScroll(index, type, query, params, cb);
};

exports.msearch = function (index, type, queries, options, cb) {
  var body = [];

  for(var i = 0, ilen = queries.length; i < ilen; i++){
    body.push({index: fixIndex(index), type: type});
    body.push(queries[i]);
  }

  let params = {body: body, rest_total_hits_as_int: true};

  if (options && options.cancelId) {
    // set X-Opaque-Id header on the params so the task can be canceled
    params.headers = { 'X-Opaque-Id': options.cancelId };
  }

  return internals.elasticSearchClient.msearch(params, cb);
};

exports.scroll = function (params, callback) {
  return internals.elasticSearchClient.scroll(params, callback);
};

exports.clearScroll = function (params, callback) {
  return internals.elasticSearchClient.clearScroll(params, callback);
};

exports.bulk = function (params, callback) {
  return internals.elasticSearchClient.bulk(params, callback);
};

exports.deleteByQuery = function (index, type, query, cb) {
  return internals.elasticSearchClient.deleteByQuery({index: fixIndex(index), type: type, body: query}, cb);
};

exports.deleteDocument = function (index, type, id, options, cb) {
  if (!cb && typeof options === 'function') {
    cb = options;
    options = undefined;
  }
  var params = {index: fixIndex(index), type: type, id: id};
  exports.merge(params, options);
  return internals.elasticSearchClient.delete(params, cb);
};

// This API does not call fixIndex
exports.deleteIndex = function (index, options, cb) {
  if (!cb && typeof options === 'function') {
    cb = options;
    options = undefined;
  }
  var params = {index: index};
  exports.merge(params, options);
  return internals.elasticSearchClient.indices.delete(params, cb);
};

// This API does not call fixIndex
exports.optimizeIndex = function (index, options, cb) {
  if (!cb && typeof options === 'function') {
    cb = options;
    options = undefined;
  }
  var params = {index: index, maxNumSegments: 1};
  exports.merge(params, options);
  return internals.elasticSearchClient.indices.forcemerge(params, cb);
};

// This API does not call fixIndex
exports.closeIndex = function (index, options, cb) {
  if (!cb && typeof options === 'function') {
    cb = options;
    options = undefined;
  }
  var params = {index: index};
  exports.merge(params, options);
  return internals.elasticSearchClient.indices.close(params, cb);
};

// This API does not call fixIndex
exports.openIndex = function (index, options, cb) {
  if (!cb && typeof options === 'function') {
    cb = options;
    options = undefined;
  }
  var params = {index: index};
  exports.merge(params, options);
  return internals.elasticSearchClient.indices.open(params, cb);
};

exports.shrinkIndex = function (index, options, cb) {
  let params = { index: index, target: `${index}-shrink` };
  exports.merge(params, options);
  return internals.elasticSearchClient.indices.shrink(params, cb);
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

exports.indicesSettings = function(cb, index) {
  return internals.elasticSearchClient.indices.getSettings({flatSettings: true, index: fixIndex(index)}, cb);
};

exports.setIndexSettings = (index, options, cb) => {
  return internals.elasticSearchClient.indices.putSettings(
    {
      index: index,
      body: options.body
    },
    () => {
      internals.healthCache = {};
      cb();
    }
  );
};

exports.shards = function(cb) {
  return internals.elasticSearchClient.cat.shards({format: "json", bytes: "b", h: "index,shard,prirep,state,docs,store,ip,node,ur,uf,fm,sm"}, cb);
};

exports.recovery = function(sortField, cb) {
  return internals.elasticSearchClient.cat.recovery({format: "json", bytes: "b", s: sortField}, cb);
};

exports.master = function(cb) {
  return internals.elasticSearchClient.cat.master({format: "json"}, cb);
};

exports.getClusterSettings = function(options, cb) {
  return internals.elasticSearchClient.cluster.getSettings(options, cb);
};

exports.putClusterSettings = function(options, cb) {
  return internals.elasticSearchClient.cluster.putSettings(options, cb);
};

exports.tasks = function(cb) {
  return internals.elasticSearchClient.tasks.list({detailed: "false", group_by: "parents"}, cb);
};

exports.taskCancel = function(taskId, cb) {
  return internals.elasticSearchClient.tasks.cancel({taskId: taskId}, cb);
};

exports.nodesStats = function (options, cb) {
  return internals.elasticSearchClient.nodes.stats(options, cb);
};

exports.nodesInfo = function (options, cb) {
  return internals.elasticSearchClient.nodes.info(options, cb);
};

exports.update = function (index, type, id, document, options, cb) {
  if (!cb && typeof options === 'function') {
    cb = options;
    options = undefined;
  }

  var params = {index: fixIndex(index), type: type, body: document, id: id, timeout: '10m'};
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

exports.addTagsToSession = function (index, id, tags, node, cb) {
  let params = {
    retry_on_conflict: 3,
    index: fixIndex(index),
    type: 'session',
    id: id,
    timeout: '10m'
  };

  let script = `
    if (ctx._source.tags != null) {
      for (int i = 0; i < params.tags.length; i++) {
        if (ctx._source.tags.indexOf(params.tags[i]) == -1) {
          ctx._source.tags.add(params.tags[i]);
        }
      }
      ctx._source.tagsCnt = ctx._source.tags.length;
    } else {
      ctx._source.tags = params.tags;
      ctx._source.tagsCnt = params.tags.length;
    }
  `;

  params.body = {
    script: {
      inline: script,
      lang: 'painless',
      params: {
        tags: tags
      }
    }
  };

  if (node) { params.body._node = node; }

  return internals.elasticSearchClient.update(params, cb);
};

exports.removeTagsFromSession = function (index, id, tags, node, cb) {
  let params = {
    retry_on_conflict: 3,
    index: fixIndex(index),
    type: 'session',
    id: id,
    timeout: '10m'
  };

  let script = `
    if (ctx._source.tags != null) {
      for (int i = 0; i < params.tags.length; i++) {
        int index = ctx._source.tags.indexOf(params.tags[i]);
        if (index > -1) { ctx._source.tags.remove(index); }
      }
      ctx._source.tagsCnt = ctx._source.tags.length;
      if (ctx._source.tagsCnt == 0) {
        ctx._source.remove("tags");
        ctx._source.remove("tagsCnt");
      }
    }
  `;

  params.body = {
    script: {
      inline: script,
      lang: 'painless',
      params: {
        tags: tags
      }
    }
  };

  if (node) { params.body._node = node; }

  return internals.elasticSearchClient.update(params, cb);
};

exports.addHuntToSession = function (index, id, huntId, huntName, cb) {
  let params = {
    retry_on_conflict: 3,
    index: fixIndex(index),
    type: 'session',
    id: id,
    timeout: '10m'
  };

  let script = `
    if (ctx._source.huntId != null) {
      ctx._source.huntId.add(params.huntId);
    } else {
      ctx._source.huntId = [ params.huntId ];
    }
    if (ctx._source.huntName != null) {
      ctx._source.huntName.add(params.huntName);
    } else {
      ctx._source.huntName = [ params.huntName ];
    }
  `;

  params.body = {
    script: {
      inline: script,
      lang: 'painless',
      params: {
        huntId: huntId,
        huntName: huntName
      }
    }
  };

  return internals.elasticSearchClient.update(params, cb);
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
  internals.lookupsCache = {};
  delete internals.aliasesCache;
};
exports.searchUsers = function(query, cb) {
  return internals.usersElasticSearchClient.search({index: internals.usersPrefix + 'users', type: 'user', body: query, rest_total_hits_as_int: true}, cb);
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
  return internals.usersElasticSearchClient.delete({index: internals.usersPrefix + 'users', type: 'user', id: name, refresh: true}, (err) => {
    delete internals.usersCache[name]; // Delete again after db says its done refreshing
    cb(err);
  });
};

exports.setUser = function(name, doc, cb) {
  delete internals.usersCache[name];
  return internals.usersElasticSearchClient.index({index: internals.usersPrefix + 'users', type: 'user', body: doc, id: name, refresh: true}, (err) => {
    delete internals.usersCache[name]; // Delete again after db says its done refreshing
    cb(err);
  });
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
  return internals.elasticSearchClient.search({index:fixIndex('history_v1-*'), type:"history", body:query, rest_total_hits_as_int: true}, cb);
};
exports.numberOfLogs = function(cb) {
  return internals.elasticSearchClient.count({index:fixIndex('history_v1-*'), type:"history", ignoreUnavailable:true}, cb);
};
exports.deleteHistoryItem = function (id, index, cb) {
  return internals.elasticSearchClient.delete({index:index, type: 'history', id: id, refresh: true}, cb);
};

exports.createHunt = function (doc, cb) {
  return internals.elasticSearchClient.index({index:fixIndex('hunts'), type:'hunt', body:doc, refresh: "wait_for"}, cb);
};
exports.searchHunt = function (query, cb) {
  return internals.elasticSearchClient.search({index:fixIndex('hunts'), type:'hunt', body:query, rest_total_hits_as_int: true}, cb);
};
exports.numberOfHunts = function(cb) {
  return internals.elasticSearchClient.count({index:fixIndex('hunts'), type:'hunt'}, cb);
};
exports.deleteHuntItem = function (id, cb) {
  return internals.elasticSearchClient.delete({index:fixIndex('hunts'), type:'hunt', id:id, refresh:true}, cb);
};
exports.setHunt = function (id, doc, cb) {
  return internals.elasticSearchClient.index({index:fixIndex('hunts'), type: 'hunt', body:doc, id: id, refresh:true}, cb);
};

exports.searchLookups = function (query, cb) {
  return internals.elasticSearchClient.search({index:fixIndex('lookups'), type:'lookup', body:query, rest_total_hits_as_int: true}, cb);
};
exports.createLookup = function (doc, username, cb) {
  internals.lookupsCache = {};
  return internals.elasticSearchClient.index({index:fixIndex('lookups'), type:'lookup', body:doc, refresh: "wait_for"}, cb);
};
exports.deleteLookup = function (id, username, cb) {
  internals.lookupsCache = {};
  return internals.elasticSearchClient.delete({index:fixIndex('lookups'), type:'lookup', id:id, refresh:true}, cb);
};
exports.setLookup = function (id, username, doc, cb) {
  internals.lookupsCache = {};
  return internals.elasticSearchClient.index({index:fixIndex('lookups'), type: 'lookup', body:doc, id: id, refresh:true}, cb);
};
exports.getLookup = function (id, cb) {
  return internals.elasticSearchClient.get({index:fixIndex('lookups'), type:'lookup', id:id}, cb);
};
exports.getLookupsCache = function (name, cb) {
  if (internals.lookupsCache[name] && internals.lookupsCache._timeStamp > Date.now() - 30000) {
    return cb(null, internals.lookupsCache[name]);
  }

  // only get lookups for this user or shared
  const query = {
    query: {
      bool: {
        should: [
          { term: { shared: true } },
          { term: { userId: name } }
        ]
      }
    }
  };

  exports.searchLookups(query, (err, lookups) => {
    if (err) { return cb(err, lookups); }

    let lookupsMap = {};
    for (let lookup of lookups.hits.hits) {
      // need the whole object to test for type mismatch
      lookupsMap[lookup._source.name] = lookup;
    }

    internals.lookupsCache[name] = lookupsMap;
    internals.lookupsCache._timeStamp = Date.now();

    cb(null, lookupsMap);
  });
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

    internals.elasticSearchClient.indices.getTemplate({name: fixIndex("sessions2_template"), filter_path: "**._meta", include_type_name: true}, (err, doc) => {
      if (err) {
        return cb(null, health);
      }
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

exports.nodesInfoCache = function () {
  if (internals.nodesInfoCache._timeStamp !== undefined && internals.nodesInfoCache._timeStamp > Date.now() - 30000) {
    return new Promise((resolve, reject) => {resolve(internals.nodesInfoCache);});
  }

  return new Promise((resolve, reject) => {
    exports.nodesInfo((err, data) => {
      if (err) {
        reject(err);
      } else {
        internals.nodesInfoCache = data;
        internals.nodesInfoCache._timeStamp = Date.now();
        resolve(data);
      }
    });
  });
};

exports.masterCache = function () {
  if (internals.masterCache._timeStamp !== undefined && internals.masterCache._timeStamp > Date.now() - 60000) {
    return new Promise((resolve, reject) => {resolve(internals.masterCache);});
  }

  return new Promise((resolve, reject) => {
    exports.master((err, data) => {
      if (err) {
        reject(err);
      } else {
        internals.masterCache = data;
        internals.masterCache._timeStamp = Date.now();
        resolve(data);
      }
    });
  });
};

exports.nodesStatsCache = function () {
  if (internals.nodesStatsCache._timeStamp !== undefined && internals.nodesStatsCache._timeStamp > Date.now() - 2500) {
    return new Promise((resolve, reject) => {resolve(internals.nodesStatsCache);});
  }

  return new Promise((resolve, reject) => {
    exports.nodesStats({metric: 'jvm,process,fs,os,indices,thread_pool'}, (err, data) => {
      if (err) {
        reject(err);
      } else {
        internals.nodesStatsCache = data;
        internals.nodesStatsCache._timeStamp = Date.now();
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

exports.indicesSettingsCache = function (cb) {
  if (!cb) {
    return internals.indicesSettingsCache;
  }

  if (internals.indicesSettingsCache._timeStamp !== undefined && internals.indicesSettingsCache._timeStamp > Date.now() - 10000) {
    return cb(null, internals.indicesSettingsCache);
  }

  return exports.indicesSettings((err, indicesSettings) => {
    if (err) {
      // Even if an error, if we have a cache use it
      if (internals.indicesSettingsCache._timeStamp !== undefined) {
        return cb(null, internals.indicesSettingsCache);
      }
      return cb(err, null);
    }

    internals.indicesSettingsCache = indicesSettings;
    internals.indicesSettingsCache._timeStamp = Date.now();
    cb(null, indicesSettings);
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
    query = {size: 100, query: {term: {name: name}}, sort: [{num: {order: "desc"}}]};
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
  if (version < 81200) {
    console.log(`ERROR - Need at least node 8.12.0, currently using ${process.version}`);
    process.exit(1);
    throw new Error("Exiting");
  }

  ["stats", "dstats", "sequence", "files"].forEach((index) => {
    exports.indexStats(index, (err, status) => {
      if (err || status.error) {
        console.log("ERROR - Issue with index '" + index + "' make sure 'db/db.pl <eshost:esport> init' has been run", err, status);
        process.exit(1);
        throw new Error("Exiting");
      }
    });
  });

  internals.elasticSearchClient.indices.getTemplate({name: fixIndex("sessions2_template"), filter_path: "**._meta", include_type_name: true}, (err, doc) => {
    if (err) {
      console.log("ERROR - Couldn't retrieve database version, is ES running?  Have you run ./db.pl host:port init?", err);
      process.exit(0);
    }
    try {
      var version = doc[fixIndex("sessions2_template")].mappings.session._meta.molochDbVersion;

      if (version < minVersion) {
        console.log(`ERROR - Current database version (${version}) is less then required version (${minVersion}) use 'db/db.pl <eshost:esport> upgrade' to upgrade`);
        if (doc._node) {
          console.log(`On node ${doc._node}`);
        }
        process.exit(1);
      }
    } catch (e) {
      console.log("ERROR - Couldn't find database version.  Have you run ./db.pl host:port upgrade?", e);
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
    if (internals.debug > 1) {
      console.log(`DEBUG: node:${node} is local view because equals ${internals.nodeName}`);
    }
    return yesCB();
  }

  exports.molochNodeStatsCache(node, (err, stat) => {
    if (err || stat.hostname !== os.hostname()) {
      if (internals.debug > 1) {
        console.log(`DEBUG: node:${node} is NOT local view because ${stat.hostname} != ${os.hostname()}`);
      }
      noCB();
    } else {
      if (internals.debug > 1) {
        console.log(`DEBUG: node:${node} is local view because ${stat.hostname} == ${os.hostname()}`);
      }
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

exports.session2Sid = function (item) {
  if (item._id.length < 31) {
    return item._index.substring(internals.prefix.length + 10) + ':' + item._id;
  }

  return item._id;
};

exports.sid2Id = function (id) {
  let colon = id.indexOf(':');
  if (colon > 0) {
    return id.substr(colon+1);
  }

  return id;
};

exports.sid2Index = function (id) {
  let colon = id.indexOf(':');
  if (colon > 0) {
    return 'sessions2-' + id.substr(0, colon);
  }
  return 'sessions2-' + id.substr(0, id.indexOf('-'));
};

exports.loadFields = function(cb) {
  return exports.search("fields", "field", {size:1000}, cb);
};

exports.getIndices = function(startTime, stopTime, rotateIndex, cb) {
  exports.getAliasesCache("sessions2-*", (err, aliases) => {

    if (err || aliases.error) {
      return cb('');
    }

    let indices = [];

    // Guess how long hour indices we find are
    let hlength = 0;
    if (rotateIndex === 'hourly') {
      hlength = 60*60;
    } else if (rotateIndex.startsWith('hourly')) {
      hlength = +rotateIndex.substring(6)*60*60;
    } else {
      hlength = 12*60*60; // Max hourly can be is 12 hours
    }

    // Go thru each index, convert to start/stop range and see if our time range overlaps
    // For hourly and month indices we may search extra
    for (let iname in aliases) {
      let index = iname;
      if (index.endsWith('-shrink')) {
        index = index.substring(0,index.length-7);
      }
      index = index.substring(internals.prefix.length + 10);
      let year, month, day = 0, hour = 0, length;

      if (+index[0] >= 6) {
        year = 1900 + (+index[0])*10 + (+index[1]);
      } else {
        year = 2000 + (+index[0])*10 + (+index[1]);
      }

      if (index[2] === 'w') {
        length = 7*24*60*60;
        month = 1;
        day = (+index[3]*10 + (+index[4]))*7;
      } else if (index[2] === 'm') {
        month = (+index[3])*10 + (+index[4]);
        day = 1;
        length = 31*24*60*60;
      } else if (index.length === 6) {
        month = (+index[2])*10 + (+index[3]);
        day = (+index[4])*10 + (+index[5]);
        length = 24*60*60;
      } else {
        month = (+index[2])*10 + (+index[3]);
        day = (+index[4])*10 + (+index[5]);
        hour = (+index[7])*10 + (+index[8]);
        length = hlength;
      }

      let start = Date.UTC(year, month-1, day, hour)/1000;
      let stop = Date.UTC(year, month-1, day, hour)/1000+length;

      if (stop >= startTime && start <= stopTime) {
        indices.push(iname);
      }
    }

    if (indices.length === 0) {
      return cb(internals.prefix + 'sessions2-*');
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
