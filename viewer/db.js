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

'use strict';

const ESC = require('elasticsearch');
const os = require('os');
const fs = require('fs');
const async = require('async');
const { Client } = require('@elastic/elasticsearch');

const internals = {
  fileId2File: {},
  fileName2File: {},
  molochNodeStatsCache: {},
  healthCache: {},
  indicesCache: {},
  indicesSettingsCache: {},
  usersCache: {},
  shortcutsCache: {},
  nodesStatsCache: {},
  nodesInfoCache: {},
  masterCache: {},
  qInProgress: 0,
  apiVersion: '7.7',
  q: []
};

exports.initialize = function (info, cb) {
  internals.multiES = info.multiES === 'true' || info.multiES === true || false;
  internals.debug = info.debug || 0;
  internals.getSessionBySearch = info.getSessionBySearch || false;

  delete info.multiES;
  delete info.debug;

  internals.info = info;

  if (info.prefix && info.prefix.charAt(info.prefix.length - 1) !== '_') {
    internals.prefix = info.prefix + '_';
  } else {
    internals.prefix = info.prefix || '';
  }

  if (info.usersPrefix && info.usersPrefix.charAt(info.usersPrefix.length - 1) !== '_') {
    internals.usersPrefix = info.usersPrefix + '_';
  } else {
    internals.usersPrefix = info.usersPrefix || internals.prefix;
  }

  internals.nodeName = info.nodeName;
  delete info.nodeName;
  internals.hostName = info.hostName;
  delete info.hostName;

  internals.esProfile = info.esProfile || false;
  delete info.esProfile;

  const esSSLOptions = { rejectUnauthorized: !internals.info.insecure, ca: internals.info.ca };
  if (info.esClientKey) {
    esSSLOptions.key = fs.readFileSync(info.esClientKey);
    esSSLOptions.cert = fs.readFileSync(info.esClientCert);
    if (info.esClientKeyPass) {
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

  internals.elasticSearchClient.info((err, data) => {
    if (err) {
      console.log(err, data);
    }
    if (data.version.number.match(/^(7\.7\.0|7\.[0-6]\.|[0-6]|8)/)) {
      console.log('ERROR - ES', data.version.number, 'not supported, ES 7.7.1 or later required.');
      process.exit();
    }

    internals.client7 = new Client({
      node: internals.info.host,
      maxRetries: 2,
      requestTimeout: (parseInt(info.requestTimeout, 10) + 30) * 1000 || 330000,
      ssl: esSSLOptions
    });

    if (info.usersHost) {
      internals.usersClient7 = new Client({
        node: internals.info.usersHost,
        maxRetries: 2,
        requestTimeout: (parseInt(info.requestTimeout, 10) + 30) * 1000 || 330000,
        ssl: esSSLOptions
      });
    } else {
      internals.usersClient7 = internals.client7;
    }
    return cb();
  });

  // Replace tag implementation
  if (internals.multiES) {
    exports.isLocalView = function (node, yesCB, noCB) { return noCB(); };
    internals.prefix = 'MULTIPREFIX_';
  }

  // Update aliases cache so -shrink/-reindex works
  if (internals.nodeName !== undefined) {
    exports.getAliasesCache('sessions2-*', () => {});
    setInterval(() => { exports.getAliasesCache('sessions2-*', () => {}); }, 2 * 60 * 1000);
  }
};

/// ///////////////////////////////////////////////////////////////////////////////
/// / Low level functions to convert from old style to new
/// ///////////////////////////////////////////////////////////////////////////////
//
//
function fixIndex (index) {
  if (index === undefined || index === '_all') { return index; }

  if (Array.isArray(index)) {
    return index.map((val) => {
      if (val.lastIndexOf(internals.prefix, 0) === 0) {
        return val;
      } else {
        return internals.prefix + val;
      }
    });
  }

  // If prefix isn't there, add it
  if (index.lastIndexOf(internals.prefix, 0) !== 0) {
    index = internals.prefix + index;
  }

  // If the index doesn't exist but the shrink version does exist, add -shrink
  if (internals.aliasesCache && !internals.aliasesCache[index] && internals.aliasesCache[index + '-shrink']) {
    index += '-shrink';
  }

  // If the index doesn't exist but the reindex version does exist, add -reindex
  if (internals.aliasesCache && !internals.aliasesCache[index] && internals.aliasesCache[index + '-reindex']) {
    index += '-reindex';
  }

  return index;
}

exports.merge = function (to, from) {
  for (const key in from) {
    to[key] = from[key];
  }
};

exports.get = function (index, type, id, cb) {
  return internals.elasticSearchClient.get({ index: fixIndex(index), id: id }, cb);
};

exports.getWithOptions = function (index, type, id, options, cb) {
  const params = { index: fixIndex(index), id: id };
  exports.merge(params, options);
  return internals.elasticSearchClient.get(params, cb);
};

// Get a session from ES and decode packetPos if requested
exports.getSession = function (id, options, cb) {
  function fixPacketPos (session, fields) {
    if (!fields.packetPos || fields.packetPos.length === 0) {
      return cb(null, session);
    }
    exports.fileIdToFile(fields.node, -1 * fields.packetPos[0], (fileInfo) => {
      // Neg numbers aren't encoded, if pos is 0 same gap as last gap, otherwise last + pos
      if (fileInfo) {
        if (fileInfo.packetPosEncoding === 'gap0') {
          let last = 0;
          let lastgap = 0;
          for (let i = 0, ilen = fields.packetPos.length; i < ilen; i++) {
            if (fields.packetPos[i] < 0) {
              last = 0;
            } else {
              if (fields.packetPos[i] === 0) {
                fields.packetPos[i] = last + lastgap;
              } else {
                lastgap = fields.packetPos[i];
                fields.packetPos[i] += last;
              }
              last = fields.packetPos[i];
            }
          }
          return cb(null, session);
        } else if (fileInfo.packetPosEncoding === 'localIndex') {
          exports.isLocalView(fields.node, () => {
            const newPacketPos = [];
            async.forEachOfSeries(fields.packetPos, (item, key, nextCb) => {
              if (key % 3 !== 0) { return nextCb(); } // Only look at every 3rd item

              exports.fileIdToFile(fields.node, -1 * item, (fileInfo) => {
                try {
                  const fd = fs.openSync(fileInfo.indexFilename, 'r');
                  if (!fd) { return nextCb(); }
                  const buffer = Buffer.alloc(fields.packetPos[key + 2]);
                  fs.readSync(fd, buffer, 0, buffer.length, fields.packetPos[key + 1]);
                  let last = 0;
                  let lastgap = 0;
                  let num = 0;
                  let mult = 1;
                  newPacketPos.push(item);
                  for (let i = 0; i < buffer.length; i++) {
                    const x = buffer.readUInt8(i);
                    // high bit set when last
                    if (x & 0x80) {
                      num = num + (x & 0x7f) * mult;
                      if (num !== 0) {
                        lastgap = num;
                      }
                      last += lastgap;
                      newPacketPos.push(last);
                      num = 0;
                      mult = 1;
                    } else {
                      num = num + x * mult;
                      mult *= 128; // Javscript can't shift large numbers, so mult
                    }
                  }
                  fs.closeSync(fd);
                } catch (e) {
                  console.log(e);
                }
                return nextCb();
              });
            }, () => {
              fields.packetPos = newPacketPos;
              return cb(null, session);
            });
          }, () => {
            return cb(null, session);
          });
        } else {
          console.log('Unsupported packetPosEncoding', fileInfo);
          return cb(null, session);
        }
      } else {
        return cb(null, session);
      }
    });
  }

  if (internals.getSessionBySearch) {
    exports.search(exports.sid2Index(id), '_doc', { query: { ids: { values: [exports.sid2Id(id)] } } }, options, (err, results) => {
      if (err) { return cb(err); }
      if (!results.hits || !results.hits.hits || results.hits.hits.length === 0) { return cb('Not found'); }
      const session = results.hits.hits[0];
      session.found = true;
      if (options && options._source && !options._source.includes('packetPos')) {
        return cb(null, session);
      }
      return fixPacketPos(session, session._source || session.fields);
    });
  } else {
    exports.getWithOptions(exports.sid2Index(id), '_doc', exports.sid2Id(id), options, (err, session) => {
      if (err || (options && options._source && !options._source.includes('packetPos'))) {
        return cb(err, session);
      }
      return fixPacketPos(session, session._source || session.fields);
    });
  }
};

exports.index = function (index, type, id, document, cb) {
  return internals.elasticSearchClient.index({ index: fixIndex(index), body: document, id: id }, cb);
};

exports.indexNow = function (index, type, id, document, cb) {
  return internals.elasticSearchClient.index({ index: fixIndex(index), body: document, id: id, refresh: true }, cb);
};

exports.search = function (index, type, query, options, cb) {
  if (!cb && typeof options === 'function') {
    cb = options;
    options = undefined;
  }
  query.profile = internals.esProfile;

  const params = {
    index: fixIndex(index),
    body: query,
    rest_total_hits_as_int: true
  };

  exports.merge(params, options);

  return internals.elasticSearchClient.search(params, cb);
};

exports.cancelByOpaqueId = function (cancelId, cb) {
  internals.elasticSearchClient.tasks.list({ detailed: false, group_by: 'parents' })
    .then((results) => {
      let found = false;

      for (const resultKey in results.tasks) {
        const result = results.tasks[resultKey];
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

function searchScrollInternal (index, type, query, options, cb) {
  const from = +query.from || 0;
  const size = +query.size || 0;

  const querySize = from + size;
  delete query.from;

  let totalResults;
  const params = { scroll: '5m' };
  exports.merge(params, options);
  query.size = 1000; // Get 1000 items per scroll call
  query.profile = internals.esProfile;
  exports.search(index, type, query, params,
    function getMoreUntilDone (error, response) {
      if (error) {
        if (totalResults && from > 0) {
          totalResults.hits.hits = totalResults.hits.hits.slice(from);
        }
        if (response && response._scroll_id) {
          exports.clearScroll({ body: { scroll_id: response._scroll_id } });
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
            scroll_id: response._scroll_id
          }
        }, getMoreUntilDone);
      } else {
        if (totalResults && from > 0) {
          totalResults.hits.hits = totalResults.hits.hits.slice(from);
        }
        if (response._scroll_id) {
          exports.clearScroll({ body: { scroll_id: response._scroll_id } });
        }
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
  const params = { preference: 'primaries', ignore_unavailable: 'true' };

  if (options && options.cancelId) {
    // set X-Opaque-Id header on the params so the task can be canceled
    params.headers = { 'X-Opaque-Id': options.cancelId };
  }

  exports.merge(params, options);
  delete params.cancelId;
  return exports.searchScroll(index, type, query, params, cb);
};

exports.msearch = function (index, type, queries, options, cb) {
  const body = [];

  for (let i = 0, ilen = queries.length; i < ilen; i++) {
    body.push({ index: fixIndex(index) });
    body.push(queries[i]);
  }

  const params = { body: body, rest_total_hits_as_int: true };

  if (options && options.cancelId) {
    // set X-Opaque-Id header on the params so the task can be canceled
    params.headers = { 'X-Opaque-Id': options.cancelId };
  }

  return internals.elasticSearchClient.msearch(params, cb);
};

exports.scroll = function (params, callback) {
  params.rest_total_hits_as_int = true;
  return internals.elasticSearchClient.scroll(params, callback);
};

exports.clearScroll = function (params, callback) {
  return internals.elasticSearchClient.clearScroll(params, callback);
};

exports.bulk = function (params, callback) {
  return internals.elasticSearchClient.bulk(params, callback);
};

exports.deleteByQuery = function (index, type, query, cb) {
  return internals.elasticSearchClient.deleteByQuery({ index: fixIndex(index), body: query }, cb);
};

exports.deleteDocument = function (index, type, id, options, cb) {
  if (!cb && typeof options === 'function') {
    cb = options;
    options = undefined;
  }
  const params = { index: fixIndex(index), id: id };
  exports.merge(params, options);
  return internals.elasticSearchClient.delete(params, cb);
};

// This API does not call fixIndex
exports.deleteIndex = function (index, options, cb) {
  if (!cb && typeof options === 'function') {
    cb = options;
    options = undefined;
  }
  const params = { index: index };
  exports.merge(params, options);
  return internals.elasticSearchClient.indices.delete(params, cb);
};

// This API does not call fixIndex
exports.optimizeIndex = function (index, options, cb) {
  if (!cb && typeof options === 'function') {
    cb = options;
    options = undefined;
  }
  const params = { index: index, maxNumSegments: 1 };
  exports.merge(params, options);
  return internals.elasticSearchClient.indices.forcemerge(params, cb);
};

// This API does not call fixIndex
exports.closeIndex = function (index, options, cb) {
  if (!cb && typeof options === 'function') {
    cb = options;
    options = undefined;
  }
  const params = { index: index };
  exports.merge(params, options);
  return internals.elasticSearchClient.indices.close(params, cb);
};

// This API does not call fixIndex
exports.openIndex = function (index, options, cb) {
  if (!cb && typeof options === 'function') {
    cb = options;
    options = undefined;
  }
  const params = { index: index };
  exports.merge(params, options);
  return internals.elasticSearchClient.indices.open(params, cb);
};

exports.shrinkIndex = function (index, options, cb) {
  const params = { index: index, target: `${index}-shrink` };
  exports.merge(params, options);
  return internals.elasticSearchClient.indices.shrink(params, cb);
};

exports.indexStats = function (index, cb) {
  return internals.elasticSearchClient.indices.stats({ index: fixIndex(index) }, cb);
};

exports.getAliases = function (index, cb) {
  return internals.elasticSearchClient.indices.getAlias({ index: fixIndex(index) }, cb);
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

exports.health = function (cb) {
  return internals.elasticSearchClient.info((err, data) => {
    internals.elasticSearchClient.cluster.health({}, (err, result) => {
      if (data && result) {
        result.version = data.version.number;
      }
      return cb(err, result);
    });
  });
};

exports.indices = function (cb, index) {
  return internals.elasticSearchClient.cat.indices({ format: 'json', index: fixIndex(index), bytes: 'b', h: 'health,status,index,uuid,pri,rep,docs.count,store.size,cd,segmentsCount,pri.search.query_current,memoryTotal' }, cb);
};

exports.indicesSettings = function (cb, index) {
  return internals.elasticSearchClient.indices.getSettings({ flatSettings: true, index: fixIndex(index) }, cb);
};

exports.setIndexSettings = (index, options, cb) => {
  return internals.elasticSearchClient.indices.putSettings(
    {
      index: index,
      body: options.body,
      timeout: '10m',
      masterTimeout: '10m'
    },
    () => {
      internals.healthCache = {};
      if (cb) { cb(); }
    }
  );
};

exports.clearCache = function (cb) {
  return internals.elasticSearchClient.indices.clearCache({}, cb);
};

exports.shards = function (cb) {
  return internals.elasticSearchClient.cat.shards({ format: 'json', bytes: 'b', h: 'index,shard,prirep,state,docs,store,ip,node,ur,uf,fm,sm' }, cb);
};

exports.allocation = function (cb) {
  return internals.elasticSearchClient.cat.allocation({ format: 'json', bytes: 'b' }, cb);
};

exports.recovery = function (sortField, activeOnly, cb) {
  return internals.elasticSearchClient.cat.recovery({ format: 'json', bytes: 'b', s: sortField, active_only: activeOnly }, cb);
};

exports.master = function (cb) {
  return internals.elasticSearchClient.cat.master({ format: 'json' }, cb);
};

exports.getClusterSettings = function (options, cb) {
  return internals.elasticSearchClient.cluster.getSettings(options, cb);
};

exports.putClusterSettings = function (options, cb) {
  options.timeout = '10m';
  options.masterTimeout = '10m';
  return internals.elasticSearchClient.cluster.putSettings(options, cb);
};

exports.tasks = function (cb) {
  return internals.elasticSearchClient.tasks.list({ detailed: true, group_by: 'parents' }, cb);
};

exports.taskCancel = function (taskId, cb) {
  const params = {};
  if (taskId) { params.taskId = taskId; }
  return internals.elasticSearchClient.tasks.cancel(params, cb);
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

  const params = { index: fixIndex(index), body: document, id: id, timeout: '10m' };
  exports.merge(params, options);
  return internals.elasticSearchClient.update(params, cb);
};

exports.updateSession = function (index, id, document, cb) {
  const params = {
    retry_on_conflict: 3,
    index: fixIndex(index),
    body: document,
    id: id,
    timeout: '10m'
  };

  internals.elasticSearchClient.update(params, (err, data) => {
    // Did it fail with FORBIDDEN msg?
    if (err && err.message && err.message.match('FORBIDDEN')) {
      // Try clearing the index.blocks.write
      exports.setIndexSettings(fixIndex(index), { body: { 'index.blocks.write': null } }, (err, data) => {
        // Try doing the update again
        internals.elasticSearchClient.update(params, (err, data) => {
          return cb(err, data);
        });
      });
      return;
    }
    return cb(err, data);
  });
};

exports.close = function () {
  return internals.elasticSearchClient.close();
};

exports.reroute = function (cb) {
  return internals.elasticSearchClient.cluster.reroute({
    timeout: '10m',
    masterTimeout: '10m',
    retryFailed: true
  }, cb);
};

exports.flush = function (index, cb) {
  if (index === 'users') {
    return internals.usersClient7.indices.flush({ index: fixIndex(index) }, cb);
  } else {
    return internals.client7.indices.flush({ index: fixIndex(index) }, cb);
  }
};

exports.refresh = function (index, cb) {
  if (index === 'users') {
    return internals.usersClient7.indices.refresh({ index: fixIndex(index) }, cb);
  } else {
    return internals.client7.indices.refresh({ index: fixIndex(index) }, cb);
  }
};

exports.addTagsToSession = function (index, id, tags, cluster, cb) {
  const script = `
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

  const body = {
    script: {
      source: script,
      lang: 'painless',
      params: {
        tags: tags
      }
    }
  };

  if (cluster) { body.cluster = cluster; }

  exports.updateSession(index, id, body, cb);
};

exports.removeTagsFromSession = function (index, id, tags, cluster, cb) {
  const script = `
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

  const body = {
    script: {
      source: script,
      lang: 'painless',
      params: {
        tags: tags
      }
    }
  };

  if (cluster) { body.cluster = cluster; }

  exports.updateSession(index, id, body, cb);
};

exports.addHuntToSession = function (index, id, huntId, huntName, cb) {
  const script = `
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

  const body = {
    script: {
      source: script,
      lang: 'painless',
      params: {
        huntId: huntId,
        huntName: huntName
      }
    }
  };

  exports.updateSession(index, id, body, cb);
};

/// ///////////////////////////////////////////////////////////////////////////////
/// / High level functions
/// ///////////////////////////////////////////////////////////////////////////////
exports.flushCache = function () {
  internals.fileId2File = {};
  internals.fileName2File = {};
  internals.molochNodeStatsCache = {};
  internals.healthCache = {};
  internals.usersCache = {};
  internals.shortcutsCache = {};
  delete internals.aliasesCache;
};
// search against user index, promise only
exports.searchUsers = function (query, cb) {
  return new Promise((resolve, reject) => {
    internals.usersClient7.search({ index: internals.usersPrefix + 'users', body: query, rest_total_hits_as_int: true })
      .then((results) => { resolve(results.body); })
      .catch((error) => { reject(error); });
  });
};

// Return a user from DB, callback only
exports.getUser = function (name, cb) {
  internals.usersClient7.get({ index: internals.usersPrefix + 'users', id: name }, (err, result) => {
    cb(err, result.body || { found: false });
  });
};

// Return a user from cache, callback only
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

// Return a user from cache, promise only
exports.numberOfUsers = function () {
  return new Promise((resolve, reject) => {
    internals.usersClient7.count({ index: internals.usersPrefix + 'users', ignoreUnavailable: true })
      .then((results) => { resolve({ count: results.body.count }); })
      .catch((error) => { reject(error); });
  });
};

// Delete user, callback only
exports.deleteUser = function (name, cb) {
  delete internals.usersCache[name];
  internals.usersClient7.delete({ index: internals.usersPrefix + 'users', id: name, refresh: true }, (err) => {
    delete internals.usersCache[name]; // Delete again after db says its done refreshing
    cb(err);
  });
};

// Set user, callback only
exports.setUser = function (name, doc, cb) {
  delete internals.usersCache[name];
  const createOnly = !!doc._createOnly;
  delete doc._createOnly;
  internals.usersClient7.index({
    index: internals.usersPrefix + 'users',
    body: doc,
    id: name,
    refresh: true,
    timeout: '10m',
    op_type: createOnly ? 'create' : 'index'
  }, (err) => {
    delete internals.usersCache[name]; // Delete again after db says its done refreshing
    cb(err);
  });
};

exports.setLastUsed = function (name, now, cb) {
  const params = { index: internals.usersPrefix + 'users', body: { doc: { lastUsed: now } }, id: name, retry_on_conflict: 3 };

  return internals.usersClient7.update(params, cb);
};

function twoDigitString (value) {
  return (value < 10) ? ('0' + value) : value.toString();
}

exports.historyIt = function (doc, cb) {
  const d = new Date(Date.now());
  const jan = new Date(d.getUTCFullYear(), 0, 0);
  const iname = internals.prefix + 'history_v1-' +
    twoDigitString(d.getUTCFullYear() % 100) + 'w' +
    twoDigitString(Math.floor((d - jan) / 604800000));

  return internals.elasticSearchClient.index({ index: iname, body: doc, refresh: true, timeout: '10m' }, cb);
};
exports.searchHistory = function (query, cb) {
  return internals.elasticSearchClient.search({ index: fixIndex('history_v1-*'), body: query, rest_total_hits_as_int: true }, cb);
};
exports.numberOfLogs = function (cb) {
  return internals.elasticSearchClient.count({ index: fixIndex('history_v1-*'), ignoreUnavailable: true }, cb);
};
exports.deleteHistoryItem = function (id, index, cb) {
  return internals.elasticSearchClient.delete({ index: index, id: id, refresh: true }, cb);
};

exports.createHunt = function (doc, cb) {
  return internals.elasticSearchClient.index({ index: fixIndex('hunts'), body: doc, refresh: 'wait_for', timeout: '10m' }, cb);
};
exports.searchHunt = function (query, cb) {
  return internals.elasticSearchClient.search({ index: fixIndex('hunts'), body: query, rest_total_hits_as_int: true }, cb);
};
exports.numberOfHunts = function (cb) {
  return internals.elasticSearchClient.count({ index: fixIndex('hunts') }, cb);
};
exports.deleteHuntItem = function (id, cb) {
  return internals.elasticSearchClient.delete({ index: fixIndex('hunts'), id: id, refresh: true }, cb);
};
exports.setHunt = function (id, doc, cb) {
  return internals.elasticSearchClient.index({ index: fixIndex('hunts'), body: doc, id: id, refresh: true, timeout: '10m' }, cb);
};
exports.getHunt = function (id, cb) {
  return internals.elasticSearchClient.get({ index: fixIndex('hunts'), id: id }, cb);
};

exports.searchShortcuts = function (query, cb) {
  return internals.elasticSearchClient.search({ index: fixIndex('lookups'), body: query, rest_total_hits_as_int: true }, cb);
};
exports.createShortcut = function (doc, username, cb) {
  internals.shortcutsCache = {};
  return internals.elasticSearchClient.index({ index: fixIndex('lookups'), body: doc, refresh: 'wait_for', timeout: '10m' }, cb);
};
exports.deleteShortcut = function (id, username, cb) {
  internals.shortcutsCache = {};
  return internals.elasticSearchClient.delete({ index: fixIndex('lookups'), id: id, refresh: true }, cb);
};
exports.setShortcut = function (id, username, doc, cb) {
  internals.shortcutsCache = {};
  return internals.elasticSearchClient.index({ index: fixIndex('lookups'), body: doc, id: id, refresh: true, timeout: '10m' }, cb);
};
exports.getShortcut = function (id, cb) {
  return internals.elasticSearchClient.get({ index: fixIndex('lookups'), id: id }, cb);
};
exports.getShortcutsCache = function (name, cb) {
  if (internals.shortcutsCache[name] && internals.shortcutsCache._timeStamp > Date.now() - 30000) {
    return cb(null, internals.shortcutsCache[name]);
  }

  // only get shortcuts for this user or shared
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

  exports.searchShortcuts(query, (err, shortcuts) => {
    if (err) { return cb(err, shortcuts); }

    const shortcutsMap = {};
    for (const shortcut of shortcuts.hits.hits) {
      // need the whole object to test for type mismatch
      shortcutsMap[shortcut._source.name] = shortcut;
    }

    internals.shortcutsCache[name] = shortcutsMap;
    internals.shortcutsCache._timeStamp = Date.now();

    cb(null, shortcutsMap);
  });
};

exports.molochNodeStats = function (name, cb) {
  exports.get('stats', 'stat', name, (err, stat) => {
    if (err || !stat.found) {
      // Even if an error, if we have a cached value use it
      if (err && internals.molochNodeStatsCache[name]) {
        return cb(null, internals.molochNodeStatsCache[name]);
      }

      cb(err || 'Unknown node ' + name, internals.molochNodeStatsCache[name]);
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

    internals.elasticSearchClient.indices.getTemplate({ name: fixIndex('sessions2_template'), filter_path: '**._meta' }, (err, doc) => {
      if (err) {
        return cb(null, health);
      }
      health.molochDbVersion = doc[fixIndex('sessions2_template')].mappings._meta.molochDbVersion;
      internals.healthCache = health;
      internals.healthCache._timeStamp = Date.now();
      cb(null, health);
    });
  });
};

exports.healthCachePromise = function () {
  return new Promise(function (resolve, reject) {
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
    return new Promise((resolve, reject) => { resolve(internals.nodesInfoCache); });
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
    return new Promise((resolve, reject) => { resolve(internals.masterCache); });
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
    return new Promise((resolve, reject) => { resolve(internals.nodesStatsCache); });
  }

  return new Promise((resolve, reject) => {
    exports.nodesStats({ metric: 'jvm,process,fs,os,indices,thread_pool' }, (err, data) => {
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
  }, '_all');
};

exports.hostnameToNodeids = function (hostname, cb) {
  const query = { query: { match: { hostname: hostname } } };
  exports.search('stats', 'stat', query, (err, sdata) => {
    const nodes = [];
    if (sdata && sdata.hits && sdata.hits.hits) {
      for (let i = 0, ilen = sdata.hits.hits.length; i < ilen; i++) {
        nodes.push(sdata.hits.hits[i]._id);
      }
    }
    cb(nodes);
  });
};

exports.fileIdToFile = function (node, num, cb) {
  const key = node + '!' + num;
  const info = internals.fileId2File[key];
  if (info !== undefined) {
    return setImmediate(() => {
      cb(info);
    });
  }

  exports.get('files', 'file', node + '-' + num, (err, fresult) => {
    if (!err && fresult.found) {
      const file = fresult._source;
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
  let query;
  if (name[0] === '/' && name[name.length - 1] === '/') {
    query = { query: { regexp: { name: name.substring(1, name.length - 1) } }, sort: [{ num: { order: 'desc' } }] };
  } else if (name.indexOf('*') !== -1) {
    query = { query: { wildcard: { name: name } }, sort: [{ num: { order: 'desc' } }] };
  }

  // Not wildcard/regex check the cache
  if (!query) {
    if (internals.fileName2File[name]) {
      return cb([internals.fileName2File[name]]);
    }
    query = { size: 100, query: { term: { name: name } }, sort: [{ num: { order: 'desc' } }] };
  }

  exports.search('files', 'file', query, (err, data) => {
    const files = [];
    if (err || !data.hits) {
      return cb(null);
    }
    data.hits.hits.forEach((hit) => {
      const file = hit._source;
      const key = file.node + '!' + file.num;
      internals.fileId2File[key] = file;
      internals.fileName2File[file.name] = file;
      files.push(file);
    });
    return cb(files);
  });
};

exports.getSequenceNumber = function (name, cb) {
  exports.index('sequence', 'sequence', name, {}, (err, sinfo) => {
    cb(err, sinfo._version);
  });
};

exports.numberOfDocuments = function (index, options) {
  // count interface is slow for larget data sets, don't use for sessions unless multiES
  if (index !== 'sessions2-*' || internals.multiES) {
    const params = { index: fixIndex(index), ignoreUnavailable: true };
    exports.merge(params, options);
    return internals.elasticSearchClient.count(params);
  }

  return new Promise((resolve, reject) => {
    let count = 0;
    const str = internals.prefix + 'sessions2-';
    exports.indicesCache((err, indices) => {
      for (let i = 0; i < indices.length; i++) {
        if (indices[i].index.includes(str)) {
          count += parseInt(indices[i]['docs.count']);
        }
      }
      resolve({ count: count });
    });
  });
};

exports.updateFileSize = function (item, filesize) {
  exports.update('files', 'file', item.id, { doc: { filesize: filesize } });
};

exports.checkVersion = function (minVersion, checkUsers) {
  const match = process.versions.node.match(/^(\d+)\.(\d+)\.(\d+)/);
  const version = parseInt(match[1], 10) * 10000 + parseInt(match[2], 10) * 100 + parseInt(match[3], 10);
  if (version < 81200) {
    console.log(`ERROR - Need at least node 8.12.0, currently using ${process.version}`);
    process.exit(1);
  }

  ['stats', 'dstats', 'sequence', 'files'].forEach((index) => {
    exports.indexStats(index, (err, status) => {
      if (err || status.error) {
        console.log("ERROR - Issue with index '" + index + "' make sure 'db/db.pl <eshost:esport> init' has been run", err, status);
        process.exit(1);
      }
    });
  });

  internals.elasticSearchClient.indices.getTemplate({ name: fixIndex('sessions2_template'), filter_path: '**._meta' }, (err, doc) => {
    if (err) {
      console.log("ERROR - Couldn't retrieve database version, is ES running?  Have you run ./db.pl host:port init?", err);
      process.exit(0);
    }
    try {
      const version = doc[fixIndex('sessions2_template')].mappings._meta.molochDbVersion;

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
        console.log('WARNING - No users are defined, use node viewer/addUser.js to add one, or turn off auth by unsetting passwordSecret');
      }
    });
  }
};

exports.isLocalView = function (node, yesCB, noCB) {
  if (node === internals.nodeName) {
    if (internals.debug > 1) {
      console.log(`DEBUG: node:${node} is local view because equals ${internals.nodeName}`);
    }
    return yesCB();
  }

  exports.molochNodeStatsCache(node, (err, stat) => {
    if (err || (stat.hostname !== os.hostname() && stat.hostname !== internals.hostName)) {
      if (internals.debug > 1) {
        console.log(`DEBUG: node:${node} is NOT local view because ${stat.hostname} != ${os.hostname()} or --host ${internals.hostName}`);
      }
      noCB();
    } else {
      if (internals.debug > 1) {
        console.log(`DEBUG: node:${node} is local view because ${stat.hostname} == ${os.hostname()} or --host ${internals.hostName}`);
      }
      yesCB();
    }
  });
};

exports.deleteFile = function (node, id, path, cb) {
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
  const colon = id.indexOf(':');
  if (colon > 0) {
    return id.substr(colon + 1);
  }

  return id;
};

exports.sid2Index = function (id) {
  const colon = id.indexOf(':');
  if (colon > 0) {
    return 'sessions2-' + id.substr(0, colon);
  }
  return 'sessions2-' + id.substr(0, id.indexOf('-'));
};

exports.loadFields = function (cb) {
  return exports.search('fields', 'field', { size: 1000 }, cb);
};

exports.getIndices = function (startTime, stopTime, bounding, rotateIndex, cb) {
  exports.getAliasesCache('sessions2-*', (err, aliases) => {
    if (err || aliases.error) {
      return cb('');
    }

    const indices = [];

    // Guess how long hour indices we find are
    let hlength = 0;
    if (rotateIndex === 'hourly') {
      hlength = 60 * 60;
    } else if (rotateIndex.startsWith('hourly')) {
      hlength = +rotateIndex.substring(6) * 60 * 60;
    } else {
      hlength = 12 * 60 * 60; // Max hourly can be is 12 hours
    }

    // Go thru each index, convert to start/stop range and see if our time range overlaps
    // For hourly and month indices we may search extra
    for (const iname in aliases) {
      let index = iname;
      if (index.endsWith('-shrink')) {
        index = index.substring(0, index.length - 7);
      }
      if (index.endsWith('-reindex')) {
        index = index.substring(0, index.length - 8);
      }
      index = index.substring(internals.prefix.length + 10);
      let year; let month; let day = 0; let hour = 0; let length;

      if (+index[0] >= 6) {
        year = 1900 + (+index[0]) * 10 + (+index[1]);
      } else {
        year = 2000 + (+index[0]) * 10 + (+index[1]);
      }

      if (index[2] === 'w') {
        length = 7 * 24 * 60 * 60;
        month = 1;
        day = (+index[3] * 10 + (+index[4])) * 7;
      } else if (index[2] === 'm') {
        month = (+index[3]) * 10 + (+index[4]);
        day = 1;
        length = 31 * 24 * 60 * 60;
      } else if (index.length === 6) {
        month = (+index[2]) * 10 + (+index[3]);
        day = (+index[4]) * 10 + (+index[5]);
        length = 24 * 60 * 60;
      } else {
        month = (+index[2]) * 10 + (+index[3]);
        day = (+index[4]) * 10 + (+index[5]);
        hour = (+index[7]) * 10 + (+index[8]);
        length = hlength;
      }

      const start = Date.UTC(year, month - 1, day, hour) / 1000;
      const stop = Date.UTC(year, month - 1, day, hour) / 1000 + length;

      switch (bounding) {
      default:
      case 'last':
        if (stop >= startTime && start <= stopTime) {
          indices.push(iname);
        }
        break;
      case 'first':
      case 'both':
      case 'either':
      case 'database':
        if (stop >= (startTime - length) && start <= (stopTime + length)) {
          indices.push(iname);
        }
        break;
      }
    }

    if (indices.length === 0) {
      return cb(internals.prefix + 'sessions2-*');
    }

    return cb(indices.join());
  });
};

exports.getMinValue = function (index, field, cb) {
  const params = { index: fixIndex(index), body: { size: 0, aggs: { min: { min: { field: field } } } } };
  return internals.elasticSearchClient.search(params, (err, data) => {
    if (err) { return cb(err, 0); }
    return cb(null, data.aggregations.min.value);
  });
};

exports.getClusterDetails = function (cb) {
  return internals.elasticSearchClient.get({ index: '_cluster', id: 'details' }, cb);
};

exports.getILMPolicy = function () {
  return new Promise((resolve, reject) => {
    internals.client7.ilm.getLifecycle({ policy: `${internals.prefix}molochsessions,${internals.prefix}molochhistory` }, (err, data) => {
      if (err) {
        resolve({});
      } else {
        resolve(data.body);
      }
    });
  });
};

exports.setILMPolicy = function (name, policy) {
  console.log('name', name, 'policy', policy);
  return new Promise((resolve, reject) => {
    internals.client7.ilm.putLifecycle({ policy: name, body: { policy: policy.policy } }, (err, data) => {
      if (err) {
        console.log('ERROR', err, 'data', data);
        reject(err);
      } else {
        resolve(data.body);
      }
    });
  });
};

exports.getTemplate = function (name) {
  return internals.elasticSearchClient.indices.getTemplate({ name: fixIndex(name), flat_settings: true });
};

exports.putTemplate = function (name, body) {
  return internals.elasticSearchClient.indices.putTemplate({ name: fixIndex(name), body: body });
};
