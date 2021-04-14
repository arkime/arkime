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

  internals.elasticSearchClient.info((err, data) => {
    if (err) {
      console.log(err, data);
    }
    if (data.version.number.match(/^(7\.7\.0|7\.[0-6]\.|[0-6]|8)/)) {
      console.log('ERROR - ES', data.version.number, 'not supported, ES 7.7.1 or later required.');
      process.exit();
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
    exports.getAliasesCache('sessions2-*');
    setInterval(() => { exports.getAliasesCache('sessions2-*'); }, 2 * 60 * 1000);
  }
};

/// ///////////////////////////////////////////////////////////////////////////////
/// / Low level functions to convert from old style to new
/// ///////////////////////////////////////////////////////////////////////////////
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

exports.merge = (to, from) => {
  for (const key in from) {
    to[key] = from[key];
  }
};

exports.get = async (index, type, id) => {
  return internals.client7.get({ index: fixIndex(index), id: id });
};

exports.getWithOptions = async (index, type, id, options) => {
  const params = { index: fixIndex(index), id: id };
  exports.merge(params, options);
  return internals.client7.get(params);
};

// Get a session from ES and decode packetPos if requested
exports.getSession = async (id, options, cb) => {
  function fixPacketPos (session, fields) {
    if (!fields.packetPos || fields.packetPos.length === 0) {
      return cb(null, session);
    }
    exports.fileIdToFile(fields.node, -1 * fields.packetPos[0], (fileInfo) => {
      if (fileInfo && fileInfo.packetPosEncoding) {
        if (fileInfo.packetPosEncoding === 'gap0') {
          // Neg numbers aren't encoded, if pos is 0 same gap as last gap, otherwise last + pos
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
          // Neg numbers aren't encoded, use var length encoding, if pos is 0 same gap as last gap, otherwise last + pos
          exports.isLocalView(fields.node, () => {
            const newPacketPos = [];
            async.forEachOfSeries(fields.packetPos, (item, key, nextCb) => {
              if (key % 3 !== 0) { return nextCb(); } // Only look at every 3rd item

              exports.fileIdToFile(fields.node, -1 * item, (idToFileInfo) => {
                try {
                  const fd = fs.openSync(idToFileInfo.indexFilename, 'r');
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
          console.log('Unknown packetPosEncoding', fileInfo);
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
    try {
      const { body: session } = await exports.getWithOptions(exports.sid2Index(id), '_doc', exports.sid2Id(id), options);
      if (options && options._source && !options._source.includes('packetPos')) {
        return cb(null, session);
      }
      return fixPacketPos(session, session._source || session.fields);
    } catch (err) {
      return cb(err, {});
    }
  }
};

exports.index = async (index, type, id, doc) => {
  return internals.client7.index({ index: fixIndex(index), body: doc, id: id });
};

exports.indexNow = async (index, type, id, doc) => {
  return internals.client7.index({
    index: fixIndex(index), body: doc, id: id, refresh: true
  });
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

exports.cancelByOpaqueId = async (cancelId) => {
  const { body: results } = await internals.client7.tasks.list({
    detailed: false, group_by: 'parents'
  });

  let found = false;

  for (const resultKey in results.tasks) {
    const result = results.tasks[resultKey];
    if (result.headers &&
      result.headers['X-Opaque-Id'] &&
      result.headers['X-Opaque-Id'] === cancelId) {
      found = true;
      // don't need to wait for task to cancel, just break out and return
      internals.client7.tasks.cancel({ taskId: resultKey });
      break;
    }
  }

  if (!found) { // not found, return error
    throw new Error('Cancel ID not found, cannot cancel ES task(s)');
  }

  return 'ES task cancelled succesfully';
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
    async function getMoreUntilDone (error, response) {
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
        try {
          const { body: results } = await exports.scroll({
            scroll: '5m', body: { scroll_id: response._scroll_id }
          });
          getMoreUntilDone(null, results);
        } catch (err) {
          console.log('ERROR - issuing scroll', err);
          getMoreUntilDone(err, {});
        }
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

exports.msearch = async (index, type, queries, options) => {
  const body = [];

  for (let i = 0, ilen = queries.length; i < ilen; i++) {
    body.push({ index: fixIndex(index) });
    body.push(queries[i]);
  }

  const params = { body: body, rest_total_hits_as_int: true };

  let cancelId = null;
  if (options && options.cancelId) {
    // use opaqueId option so the task can be cancelled
    cancelId = { opaqueId: options.cancelId };
  }

  return internals.client7.msearch(params, cancelId);
};

exports.scroll = async (params) => {
  params.rest_total_hits_as_int = true;
  return internals.client7.scroll(params);
};

exports.clearScroll = async (params) => {
  return internals.client7.clearScroll(params);
};

exports.deleteDocument = async (index, type, id, options) => {
  const params = { index: fixIndex(index), id: id };
  exports.merge(params, options);
  return internals.client7.delete(params);
};

// This API does not call fixIndex
exports.deleteIndex = async (index, options) => {
  const params = { index: index };
  exports.merge(params, options);
  return internals.client7.indices.delete(params);
};

// This API does not call fixIndex
exports.optimizeIndex = async (index, options) => {
  const params = { index: index, maxNumSegments: 1 };
  exports.merge(params, options);
  return internals.client7.indices.forcemerge(params);
};

// This API does not call fixIndex
exports.closeIndex = async (index, options) => {
  const params = { index: index };
  exports.merge(params, options);
  return internals.client7.indices.close(params);
};

// This API does not call fixIndex
exports.openIndex = async (index, options) => {
  const params = { index: index };
  exports.merge(params, options);
  return internals.client7.indices.open(params);
};

// This API does not call fixIndex
exports.shrinkIndex = async (index, options) => {
  const params = { index: index, target: `${index}-shrink` };
  exports.merge(params, options);
  return internals.client7.indices.shrink(params);
};

exports.indexStats = async (index) => {
  return internals.client7.indices.stats({ index: fixIndex(index) });
};

exports.getAliases = async (index) => {
  return internals.client7.indices.getAlias({ index: fixIndex(index) });
};

exports.getAliasesCache = async (index) => {
  if (internals.aliasesCache && internals.aliasesCacheTimeStamp > Date.now() - 5000) {
    return internals.aliasesCache;
  }

  try {
    const { body: aliases } = await exports.getAliases(index);
    internals.aliasesCacheTimeStamp = Date.now();
    internals.aliasesCache = aliases;
    return aliases;
  } catch (err) {
    throw new Error(err);
  }
};

exports.health = async () => {
  try {
    const { body: data } = await internals.client7.info();
    const { body: result } = await internals.client7.cluster.health({});
    result.version = data.version.number;
    return result;
  } catch (err) {
    throw new Error(err);
  }
};

exports.indices = async (index) => {
  return internals.client7.cat.indices({
    format: 'json',
    index: fixIndex(index),
    bytes: 'b',
    h: 'health,status,index,uuid,pri,rep,docs.count,store.size,cd,segmentsCount,pri.search.query_current,memoryTotal'
  });
};

exports.indicesSettings = async (index) => {
  return internals.client7.indices.getSettings({ flatSettings: true, index: fixIndex(index) });
};

exports.setIndexSettings = async (index, options) => {
  try {
    const { body: response } = await internals.client7.indices.putSettings({
      index: index,
      body: options.body,
      timeout: '10m',
      masterTimeout: '10m'
    });
    return response;
  } catch (err) {
    internals.healthCache = {};
    throw new Error(err);
  }
};

exports.clearCache = async () => {
  return internals.client7.indices.clearCache({});
};

exports.shards = async () => {
  return internals.client7.cat.shards({
    format: 'json',
    bytes: 'b',
    h: 'index,shard,prirep,state,docs,store,ip,node,ur,uf,fm,sm'
  });
};

exports.allocation = async () => {
  return internals.client7.cat.allocation({ format: 'json', bytes: 'b' });
};

exports.recovery = async (sortField, activeOnly) => {
  return internals.client7.cat.recovery({
    format: 'json',
    bytes: 'b',
    s: sortField,
    active_only: activeOnly
  });
};

exports.master = async () => {
  return internals.client7.cat.master({ format: 'json' });
};

exports.getClusterSettings = async (options) => {
  return internals.client7.cluster.getSettings(options);
};

exports.putClusterSettings = async (options) => {
  options.timeout = '10m';
  options.masterTimeout = '10m';
  return internals.client7.cluster.putSettings(options);
};

exports.tasks = async () => {
  return internals.client7.tasks.list({ detailed: true, group_by: 'parents' });
};

exports.taskCancel = async (taskId) => {
  return internals.client7.tasks.cancel(taskId ? { taskId: taskId } : {});
};

exports.nodesStats = async (options) => {
  return internals.client7.nodes.stats(options);
};

exports.nodesInfo = async (options) => {
  return internals.client7.nodes.info(options);
};

exports.update = function (index, type, id, doc, options, cb) {
  if (!cb && typeof options === 'function') {
    cb = options;
    options = undefined;
  }

  const params = { index: fixIndex(index), body: doc, id: id, timeout: '10m' };
  exports.merge(params, options);
  return internals.elasticSearchClient.update(params, cb);
};

exports.updateSession = function (index, id, doc, cb) {
  const params = {
    retry_on_conflict: 3,
    index: fixIndex(index),
    body: doc,
    id: id,
    timeout: '10m'
  };

  internals.elasticSearchClient.update(params, async (err, data) => {
    // Did it fail with FORBIDDEN msg?
    if (err && err.message && err.message.match('FORBIDDEN')) {
      // Try clearing the index.blocks.write
      try {
        exports.setIndexSettings(fixIndex(index), { body: { 'index.blocks.write': null } });
        const { body: retryData } = await internals.client7.update(params);
        return cb(null, retryData);
      } catch (err) {
        return cb(err, {});
      }
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

exports.flush = async (index) => {
  if (index === 'users') {
    return internals.usersClient7.indices.flush({ index: fixIndex(index) });
  } else {
    return internals.client7.indices.flush({ index: fixIndex(index) });
  }
};

exports.refresh = async (index) => {
  if (index === 'users') {
    return internals.usersClient7.indices.refresh({ index: fixIndex(index) });
  } else {
    return internals.client7.indices.refresh({ index: fixIndex(index) });
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
exports.searchUsers = async (query) => {
  try {
    const { body: users } = await internals.usersClient7.search({
      index: internals.usersPrefix + 'users',
      body: query,
      rest_total_hits_as_int: true
    });
    return users;
  } catch (err) {
    throw new Error(err);
  }
};

// Return a user from DB, callback only
exports.getUser = (userId, cb) => {
  internals.usersClient7.get({ index: internals.usersPrefix + 'users', id: userId }, (err, result) => {
    cb(err, result.body || { found: false });
  });
};

// Return a user from cache, callback only
exports.getUserCache = (userId, cb) => {
  if (internals.usersCache[userId] && internals.usersCache[userId]._timeStamp > Date.now() - 5000) {
    return cb(null, internals.usersCache[userId]);
  }

  exports.getUser(userId, (err, suser) => {
    if (err) {
      return cb(err, suser);
    }

    suser._timeStamp = Date.now();
    internals.usersCache[userId] = suser;

    cb(null, suser);
  });
};

exports.numberOfUsers = async () => {
  try {
    const { body: count } = await internals.usersClient7.count({
      index: internals.usersPrefix + 'users',
      ignoreUnavailable: true,
      body: {
        query: { // exclude the shared user from results
          bool: { must_not: { term: { userId: '_moloch_shared' } } }
        }
      }
    });
    return count.count;
  } catch (err) {
    throw new Error(err);
  }
};

// Delete user, promise only
exports.deleteUser = async (userId) => {
  delete internals.usersCache[userId];
  try {
    await internals.usersClient7.delete({
      index: internals.usersPrefix + 'users',
      id: userId,
      refresh: true
    });
    delete internals.usersCache[userId]; // Delete again after db says its done refreshing
  } catch (err) {
    throw new Error(err);
  }
};

// Set user, callback only
exports.setUser = (userId, doc, cb) => {
  delete internals.usersCache[userId];
  const createOnly = !!doc._createOnly;
  delete doc._createOnly;
  internals.usersClient7.index({
    index: internals.usersPrefix + 'users',
    body: doc,
    id: userId,
    refresh: true,
    timeout: '10m',
    op_type: createOnly ? 'create' : 'index'
  }, (err) => {
    delete internals.usersCache[userId]; // Delete again after db says its done refreshing
    cb(err);
  });
};

exports.setLastUsed = async (userId, now) => {
  const params = {
    index: internals.usersPrefix + 'users',
    body: { doc: { lastUsed: now } },
    id: userId,
    retry_on_conflict: 3
  };

  return internals.usersClient7.update(params);
};

function twoDigitString (value) {
  return (value < 10) ? ('0' + value) : value.toString();
}

// History DB interactions
exports.historyIt = async (doc) => {
  const d = new Date(Date.now());
  const jan = new Date(d.getUTCFullYear(), 0, 0);
  const iname = internals.prefix + 'history_v1-' +
    twoDigitString(d.getUTCFullYear() % 100) + 'w' +
    twoDigitString(Math.floor((d - jan) / 604800000));

  return internals.client7.index({
    index: iname, body: doc, refresh: true, timeout: '10m'
  });
};
exports.searchHistory = async (query) => {
  return internals.client7.search({
    index: fixIndex('history_v1-*'), body: query, rest_total_hits_as_int: true
  });
};
exports.countHistory = async () => {
  return internals.client7.count({
    index: fixIndex('history_v1-*'), ignoreUnavailable: true
  });
};
exports.deleteHistory = async (id, index) => {
  return internals.client7.delete({
    index: index, id: id, refresh: true
  });
};

// Hunt DB interactions
exports.createHunt = async (doc) => {
  return internals.client7.index({
    index: fixIndex('hunts'), body: doc, refresh: 'wait_for', timeout: '10m'
  });
};
exports.searchHunt = async (query) => {
  return internals.client7.search({
    index: fixIndex('hunts'), body: query, rest_total_hits_as_int: true
  });
};
exports.countHunts = async () => {
  return internals.client7.count({ index: fixIndex('hunts') });
};
exports.deleteHunt = async (id) => {
  return internals.client7.delete({
    index: fixIndex('hunts'), id: id, refresh: true
  });
};
exports.setHunt = async (id, doc) => {
  return internals.client7.index({
    index: fixIndex('hunts'), body: doc, id: id, refresh: true, timeout: '10m'
  });
};
exports.getHunt = async (id) => {
  return internals.client7.get({ index: fixIndex('hunts'), id: id });
};

// Shortcut DB interactions
exports.searchShortcuts = async (query) => {
  return internals.client7.search({
    index: fixIndex('lookups'), body: query, rest_total_hits_as_int: true
  });
};
exports.createShortcut = async (doc) => {
  internals.shortcutsCache = {};
  return internals.client7.index({
    index: fixIndex('lookups'), body: doc, refresh: 'wait_for', timeout: '10m'
  });
};
exports.deleteShortcut = async (id) => {
  internals.shortcutsCache = {};
  return internals.client7.delete({
    index: fixIndex('lookups'), id: id, refresh: true
  });
};
exports.setShortcut = async (id, doc) => {
  internals.shortcutsCache = {};
  return internals.client7.index({
    index: fixIndex('lookups'), body: doc, id: id, refresh: true, timeout: '10m'
  });
};
exports.getShortcut = async (id) => {
  return internals.client7.get({ index: fixIndex('lookups'), id: id });
};
exports.getShortcutsCache = async (userId) => {
  if (internals.shortcutsCache[userId] && internals.shortcutsCache._timeStamp > Date.now() - 30000) {
    return internals.shortcutsCache[userId];
  }

  // only get shortcuts for this user or shared
  const query = {
    query: {
      bool: {
        should: [
          { term: { shared: true } },
          { term: { userId: userId } }
        ]
      }
    }
  };

  try {
    const { body: { hits: shortcuts } } = await exports.searchShortcuts(query);

    const shortcutsMap = {};
    for (const shortcut of shortcuts.hits) {
      // need the whole object to test for type mismatch
      shortcutsMap[shortcut._source.name] = shortcut;
    }

    internals.shortcutsCache[userId] = shortcutsMap;
    internals.shortcutsCache._timeStamp = Date.now();

    return shortcutsMap;
  } catch (err) {
    throw new Error(err);
  }
};

exports.molochNodeStats = async (nodeName, cb) => {
  try {
    const { body: stat } = await exports.get('stats', 'stat', nodeName);

    internals.molochNodeStatsCache[nodeName] = stat._source;
    internals.molochNodeStatsCache[nodeName]._timeStamp = Date.now();

    cb(null, stat._source);
  } catch (err) {
    if (internals.molochNodeStatsCache[nodeName]) {
      return cb(null, internals.molochNodeStatsCache[nodeName]);
    }
    return cb(err || 'Unknown node ' + nodeName, internals.molochNodeStatsCache[nodeName]);
  }
};

exports.molochNodeStatsCache = function (nodeName, cb) {
  if (internals.molochNodeStatsCache[nodeName] && internals.molochNodeStatsCache[nodeName]._timeStamp > Date.now() - 30000) {
    return cb(null, internals.molochNodeStatsCache[nodeName]);
  }

  return exports.molochNodeStats(nodeName, cb);
};

exports.healthCache = async () => {
  if (internals.healthCache._timeStamp !== undefined && internals.healthCache._timeStamp > Date.now() - 10000) {
    return internals.healthCache;
  }

  try {
    const health = await exports.health();
    try {
      const { body: doc } = await internals.client7.indices.getTemplate({
        name: fixIndex('sessions2_template'), filter_path: '**._meta'
      });
      health.molochDbVersion = doc[fixIndex('sessions2_template')].mappings._meta.molochDbVersion;
      internals.healthCache = health;
      internals.healthCache._timeStamp = Date.now();
      return health;
    } catch (err) {
      return health;
    }
  } catch (err) {
    // Even if an error, if we have a cache use it
    if (internals.healthCache._timeStamp !== undefined) {
      return internals.healthCache;
    }
    throw new Error(err);
  }
};

exports.nodesInfoCache = async () => {
  if (internals.nodesInfoCache._timeStamp !== undefined && internals.nodesInfoCache._timeStamp > Date.now() - 30000) {
    return internals.nodesInfoCache;
  }

  try {
    const { body: data } = await exports.nodesInfo();
    internals.nodesInfoCache = data;
    internals.nodesInfoCache._timeStamp = Date.now();
    return data;
  } catch (err) {
    throw new Error(err);
  }
};

exports.masterCache = async () => {
  if (internals.masterCache._timeStamp !== undefined && internals.masterCache._timeStamp > Date.now() - 60000) {
    return internals.masterCache;
  }

  try {
    const { body: data } = await exports.master();
    internals.masterCache = data;
    internals.masterCache._timeStamp = Date.now();
    return data;
  } catch (err) {
    throw new Error(err);
  }
};

exports.nodesStatsCache = async () => {
  if (internals.nodesStatsCache._timeStamp !== undefined && internals.nodesStatsCache._timeStamp > Date.now() - 2500) {
    return internals.nodesStatsCache;
  }

  try {
    const { body: data } = await exports.nodesStats({
      metric: 'jvm,process,fs,os,indices,thread_pool'
    });
    internals.nodesStatsCache = data;
    internals.nodesStatsCache._timeStamp = Date.now();
    return data;
  } catch (err) {
    throw new Error(err);
  }
};

exports.indicesCache = async () => {
  if (internals.indicesCache._timeStamp !== undefined &&
    internals.indicesCache._timeStamp > Date.now() - 10000) {
    return internals.indicesCache;
  }

  try {
    const { body: indices } = await exports.indices();
    internals.indicesCache = indices;
    internals.indicesCache._timeStamp = Date.now();
    return indices;
  } catch (err) {
    // Even if an error, if we have a cache use it
    if (internals.indicesCache._timeStamp !== undefined) {
      return internals.indicesCache;
    }
    throw new Error(err);
  }
};

exports.indicesSettingsCache = async () => {
  if (internals.indicesSettingsCache._timeStamp !== undefined &&
    internals.indicesSettingsCache._timeStamp > Date.now() - 10000) {
    return internals.indicesSettingsCache;
  }

  try {
    const { body: indicesSettings } = await exports.indicesSettings('_all');
    internals.indicesSettingsCache = indicesSettings;
    internals.indicesSettingsCache._timeStamp = Date.now();
    return indicesSettings;
  } catch (err) {
    if (internals.indicesSettingsCache._timeStamp !== undefined) {
      return internals.indicesSettingsCache;
    }
    throw new Error(err);
  }
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

exports.fileIdToFile = async (node, num, cb) => {
  const key = node + '!' + num;
  const info = internals.fileId2File[key];
  if (info !== undefined) {
    return setImmediate(() => {
      cb(info);
    });
  }

  try {
    const { body: fresult } = await exports.get('files', 'file', node + '-' + num);
    const file = fresult._source;
    internals.fileId2File[key] = file;
    internals.fileName2File[file.name] = file;
    return cb(file);
  } catch (err) { // Cache file is unknown
    internals.fileId2File[key] = null;
    return cb(null);
  }
};

exports.fileNameToFiles = function (fileName, cb) {
  let query;
  if (fileName[0] === '/' && fileName[fileName.length - 1] === '/') {
    query = { query: { regexp: { name: fileName.substring(1, fileName.length - 1) } }, sort: [{ num: { order: 'desc' } }] };
  } else if (fileName.indexOf('*') !== -1) {
    query = { query: { wildcard: { name: fileName } }, sort: [{ num: { order: 'desc' } }] };
  }

  // Not wildcard/regex check the cache
  if (!query) {
    if (internals.fileName2File[fileName]) {
      return cb([internals.fileName2File[fileName]]);
    }
    query = { size: 100, query: { term: { name: fileName } }, sort: [{ num: { order: 'desc' } }] };
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

exports.getSequenceNumber = async (sName) => {
  try {
    const { data: sinfo } = await exports.index('sequence', 'sequence', sName, {});
    return sinfo._version;
  } catch (err) {
    throw new Error(err);
  }
};

exports.numberOfDocuments = async (index, options) => {
  // count interface is slow for larget data sets, don't use for sessions unless multiES
  if (index !== 'sessions2-*' || internals.multiES) {
    const params = { index: fixIndex(index), ignoreUnavailable: true };
    exports.merge(params, options);
    try {
      const { body: total } = await internals.client7.count(params);
      return { count: total.count };
    } catch (err) {
      throw new Error(err);
    }
  }

  let count = 0;
  const str = `${internals.prefix}sessions2-`;

  try {
    const indices = await exports.indicesCache();

    for (let i = 0; i < indices.length; i++) {
      if (indices[i].index.includes(str)) {
        count += parseInt(indices[i]['docs.count']);
      }
    }

    return { count: count };
  } catch (err) {
    throw new Error(err);
  }
};

exports.updateFileSize = function (item, filesize) {
  exports.update('files', 'file', item.id, { doc: { filesize: filesize } });
};

exports.checkVersion = async function (minVersion, checkUsers) {
  const match = process.versions.node.match(/^(\d+)\.(\d+)\.(\d+)/);
  const nodeVersion = parseInt(match[1], 10) * 10000 + parseInt(match[2], 10) * 100 + parseInt(match[3], 10);
  if (nodeVersion < 81200) {
    console.log(`ERROR - Need at least node 8.12.0, currently using ${process.version}`);
    process.exit(1);
  }

  ['stats', 'dstats', 'sequence', 'files'].forEach(async (index) => {
    try {
      await exports.indexStats(index);
    } catch (err) {
      console.log(`ERROR - Issue with index ${index}. Make sure 'db/db.pl <eshost:esport> init' has been run`, err);
      process.exit(1);
    }
  });

  internals.elasticSearchClient.indices.getTemplate({ name: fixIndex('sessions2_template'), filter_path: '**._meta' }, (err, doc) => {
    if (err) {
      console.log("ERROR - Couldn't retrieve database version, is ES running?  Have you run ./db.pl host:port init?", err);
      process.exit(0);
    }
    try {
      const molochDbVersion = doc[fixIndex('sessions2_template')].mappings._meta.molochDbVersion;

      if (molochDbVersion < minVersion) {
        console.log(`ERROR - Current database version (${molochDbVersion}) is less then required version (${minVersion}) use 'db/db.pl <eshost:esport> upgrade' to upgrade`);
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
    const count = await exports.numberOfUsers();
    if (count === 0) {
      console.log('WARNING - No users are defined, use node viewer/addUser.js to add one, or turn off auth by unsetting passwordSecret');
    }
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
    exports.deleteDocument('files', 'file', id);
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

exports.getIndices = async (startTime, stopTime, bounding, rotateIndex) => {
  try {
    const aliases = await exports.getAliasesCache('sessions2-*');
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
      let year; let month; let day = 0; let hour = 0; let len;

      if (+index[0] >= 6) {
        year = 1900 + (+index[0]) * 10 + (+index[1]);
      } else {
        year = 2000 + (+index[0]) * 10 + (+index[1]);
      }

      if (index[2] === 'w') {
        len = 7 * 24 * 60 * 60;
        month = 1;
        day = (+index[3] * 10 + (+index[4])) * 7;
      } else if (index[2] === 'm') {
        month = (+index[3]) * 10 + (+index[4]);
        day = 1;
        len = 31 * 24 * 60 * 60;
      } else if (index.length === 6) {
        month = (+index[2]) * 10 + (+index[3]);
        day = (+index[4]) * 10 + (+index[5]);
        len = 24 * 60 * 60;
      } else {
        month = (+index[2]) * 10 + (+index[3]);
        day = (+index[4]) * 10 + (+index[5]);
        hour = (+index[7]) * 10 + (+index[8]);
        len = hlength;
      }

      const start = Date.UTC(year, month - 1, day, hour) / 1000;
      const stop = Date.UTC(year, month - 1, day, hour) / 1000 + len;

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
        if (stop >= (startTime - len) && start <= (stopTime + len)) {
          indices.push(iname);
        }
        break;
      }
    }

    if (indices.length === 0) {
      return internals.prefix + 'sessions2-*';
    }

    return indices.join();
  } catch {
    return '';
  }
};

exports.getMinValue = async (index, field) => {
  const params = {
    index: fixIndex(index),
    body: { size: 0, aggs: { min: { min: { field: field } } } }
  };
  return internals.client7.search(params);
};

exports.getClusterDetails = async () => {
  return internals.client7.get({ index: '_cluster', id: 'details' });
};

exports.getILMPolicy = async () => {
  try {
    const data = await internals.client7.ilm.getLifecycle({
      policy: `${internals.prefix}molochsessions,${internals.prefix}molochhistory`
    });
    return data.body;
  } catch {
    return {};
  }
};

exports.setILMPolicy = async (ilmName, policy) => {
  console.log('name', ilmName, 'policy', policy);
  try {
    const data = await internals.client7.ilm.putLifecycle({
      policy: ilmName, body: { policy: policy.policy }
    });
    return data.body;
  } catch (err) {
    console.log('ERROR - setting ILM Policy', err);
    throw new Error(err);
  }
};

exports.getTemplate = function (templateName) {
  return internals.elasticSearchClient.indices.getTemplate({ name: fixIndex(templateName), flat_settings: true });
};

exports.putTemplate = function (templateName, body) {
  return internals.elasticSearchClient.indices.putTemplate({ name: fixIndex(templateName), body: body });
};
