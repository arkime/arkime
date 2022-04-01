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

const os = require('os');
const fs = require('fs');
const async = require('async');
const { Client } = require('@elastic/elasticsearch');
const User = require('../common/user');

const internals = {
  fileId2File: {},
  fileName2File: {},
  molochNodeStatsCache: {},
  healthCache: {},
  indicesCache: {},
  indicesSettingsCache: {},
  shortcutsCache: {},
  nodesStatsCache: {},
  nodesInfoCache: {},
  masterCache: {},
  qInProgress: 0,
  q: [],
  doShortcutsUpdates: false,
  remoteShortcutsIndex: undefined,
  localShortcutsIndex: undefined,
  localShortcutsVersion: -1 // always start with -1 so there's an initial sync of shortcuts from user's es db
};

function checkURLs (nodes) {
  if (nodes === undefined) {
    return;
  }

  if (Array.isArray(nodes)) {
    nodes.forEach(node => {
      if (!node.startsWith('http')) {
        console.log(`ERROR - Elasticsearch endpoint url '${node}' must start with http:// or https://`);
        process.exit();
      }
    });
  } else if (!nodes.startsWith('http')) {
    console.log(`ERROR - Elasticsearch endpoint url '${nodes}' must start with http:// or https://`);
    process.exit();
  }
}

exports.initialize = async (info, cb) => {
  internals.multiES = info.multiES === 'true' || info.multiES === true || false;
  internals.debug = info.debug || 0;

  delete info.multiES;
  delete info.debug;

  internals.info = info;

  checkURLs(info.host);
  checkURLs(info.usersHost);

  if (info.prefix === '') {
    internals.prefix = '';
  } else if (info.prefix && info.prefix.charAt(info.prefix.length - 1) !== '_') {
    internals.prefix = info.prefix + '_';
  } else {
    internals.prefix = info.prefix || '';
  }

  if (info.usersPrefix === '') {
    internals.usersPrefix = '';
  } else if (info.usersPrefix && info.usersPrefix.charAt(info.usersPrefix.length - 1) !== '_') {
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

  const esClientOptions = {
    node: internals.info.host,
    maxRetries: 2,
    requestTimeout: (parseInt(info.requestTimeout, 10) + 30) * 1000 || 330000,
    ssl: esSSLOptions
  };

  if (info.esApiKey) {
    esClientOptions.auth = {
      apiKey: info.esApiKey
    };
  } else if (info.esBasicAuth) {
    let basicAuth = info.esBasicAuth;
    if (!basicAuth.includes(':')) {
      basicAuth = Buffer.from(basicAuth, 'base64').toString();
    }
    basicAuth = basicAuth.split(':');
    esClientOptions.auth = {
      username: basicAuth[0],
      password: basicAuth[1]
    };
  }

  internals.client7 = new Client(esClientOptions);

  if (info.usersHost) {
    User.initialize({
      insecure: info.insecure,
      ca: info.ca,
      requestTimeout: info.requestTimeout,
      node: info.usersHost,
      clientKey: info.esClientKey,
      clientCert: info.esClientCert,
      clientKeyPass: info.esClientKeyPass,
      apiKey: info.usersEsApiKey,
      basicAuth: info.usersEsBasicAuth,
      prefix: internals.usersPrefix,
      debug: internals.debug,
      getCurrentUserCB: info.getCurrentUserCB
    });
  } else {
    User.initialize({
      insecure: info.insecure,
      ca: info.ca,
      requestTimeout: info.requestTimeout,
      node: info.host,
      clientKey: info.esClientKey,
      clientCert: info.esClientCert,
      clientKeyPass: info.esClientKeyPass,
      apiKey: info.esApiKey,
      basicAuth: info.esBasicAuth,
      prefix: internals.prefix,
      debug: internals.debug,
      readOnly: internals.multiES,
      getCurrentUserCB: info.getCurrentUserCB
    });
  }

  internals.usersClient7 = User.getClient();

  // Replace tag implementation
  if (internals.multiES) {
    exports.isLocalView = (node, yesCB, noCB) => { return noCB(); };
    internals.prefix = 'MULTIPREFIX_';
  }

  if (internals.debug) {
    console.log(`prefix:${internals.prefix} usersPrefix:${internals.usersPrefix}`);
  }

  // Update aliases cache so -shrink/-reindex works
  if (internals.nodeName !== undefined) {
    exports.getAliasesCache(['sessions2-*', 'sessions3-*']);
    setInterval(() => { exports.getAliasesCache(['sessions2-*', 'sessions3-*']); }, 2 * 60 * 1000);
  }

  // if there's a user's db and cronQueries is set, sync shortcuts from user's
  // to local db so they can be used for sessions search
  // (can't use remote db for searching via shortcuts)
  internals.localShortcutsIndex = fixIndex('lookups');
  if (internals.info.usersHost && internals.info.cronQueries) {
    internals.remoteShortcutsIndex = `${internals.usersPrefix}lookups`;
    // make sure the remote es is not the same as local es
    if (info.host !== info.usersHost && info.prefix !== info.usersPrefix) {
      // only need to sync and update if the es' are different
      internals.doShortcutsUpdates = true; // for updating if editing shortcuts locally
      exports.updateLocalShortcuts(); // immediately update shortcuts
      setInterval(() => { exports.updateLocalShortcuts(); }, 60000); // and every minute
    }
  } else if (internals.info.usersHost && internals.usersPrefix !== undefined) {
    internals.remoteShortcutsIndex = `${internals.usersPrefix}lookups`;
  } else { // there is no remote shorcuts index, just set it to local
    internals.remoteShortcutsIndex = internals.localShortcutsIndex;
  }

  if (internals.debug > 1) {
    console.log(`remoteShortcutsIndex: ${internals.remoteShortcutsIndex} localShortcutsIndex: ${internals.localShortcutsIndex}`);
  }

  try {
    const { body: data } = await internals.client7.info();
    if (data.version.distribution === 'opensearch') {
      if (data.version.number.match(/^[0]/)) {
        console.log(`ERROR - Opensearch ${data.version.number} not supported, Opensearch 1.0.0 or later required.`);
        process.exit();
      }
    } else {
      if (data.version.number.match(/^([0-6]|7\.[0-9]\.)/)) {
        console.log(`ERROR - ES ${data.version.number} not supported, ES 7.10.0 or later required.`);
        process.exit();
      }
    }
    return cb();
  } catch (err) {
    console.log('ERROR - getting ES client info, is ES running?', err);
    process.exit(1);
  }
};

/// ///////////////////////////////////////////////////////////////////////////////
/// / Low level functions to convert from old style to new
/// ///////////////////////////////////////////////////////////////////////////////
function fixIndex (index) {
  if (index === undefined || index === '_all') { return index; }

  if (index === 'sessions*') {
    // Didn't used to have a prefix, don't use one for sessions2
    if (internals.prefix === 'arkime_') {
      return `sessions2*,${internals.prefix}sessions3*`;
    }
    return `${internals.prefix}sessions2*,${internals.prefix}sessions3*`;
  }

  if (Array.isArray(index)) {
    return index.map((val) => {
      if (val.startsWith(internals.prefix)) {
        return val;
      } else {
        return fixIndex(val);
      }
    }).join(',');
  }

  // If prefix isn't there, add it. But don't add it for sessions2 unless really set.
  if (!index.startsWith(internals.prefix) && (!index.startsWith('sessions2') || internals.prefix !== 'arkime_')) {
    index = internals.prefix + index;
  }

  if (internals.aliasesCache && !internals.aliasesCache[index]) {
    if (internals.aliasesCache[index + '-shrink']) {
      // If the index doesn't exist but the shrink version does exist, add -shrink
      index += '-shrink';
    } else if (internals.aliasesCache[index + '-reindex']) {
      // If the index doesn't exist but the reindex version does exist, add -reindex
      index += '-reindex';
    }
  }

  return index;
}
exports.fixIndex = fixIndex;

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

exports.getSessionPromise = (id, options) => {
  return new Promise((resolve, reject) => {
    exports.getSession(id, options, (err, session) => {
      err ? reject(err) : resolve(session);
    });
  });
};

// Fields too hard to leave as arrays for now
const singletonFields = {
  'destination.ip': true,
  'destination.port': true,
  'destination.packets': true,
  'destination.bytes': true,
  dstPayload8: true,
  'server.bytes': true,
  'server.packets': true,
  'destination.geo.country_iso_code': true,
  'destination.as.full': true,
  dstRIR: true,

  'source.ip': true,
  'source.port': true,
  'source.packets': true,
  'source.bytes': true,
  srcPayload8: true,
  'client.bytes': true,
  'client.packets': true,
  'source.geo.country_iso_code': true,
  'source.as.full': true,
  srcRIR: true,

  firstPacket: true,
  lastPacket: true,
  ipProtocol: true,
  node: true,
  srcNode: true,
  'tcpflags.rst': true,
  'tcpflags.syn': true,
  'tcpflags.srcZero': true,
  'tcpflags.psh': true,
  'tcpflags.dstZero': true,
  'tcpflags.syn-ack': true,
  'tcpflags.fin': true,
  'tcpflags.ack': true,
  'tcpflags.urg': true,
  'network.community_id': true,
  totDataBytes: true,
  'network.bytes': true,
  'network.packets': true,
  rootId: true
};

const dateFields = {
  firstPacket: true,
  lastPacket: true
};

// Change foo.bar to foo: {bar:}
// Unarray singleton fields
// Change string dates to MS
function fixSessionFields (fields, unflatten) {
  if (!fields) { return; }
  if (unflatten) {
    fields.source = { as: {}, geo: {} };
    fields.destination = { as: {}, geo: {} };
    fields.client = {};
    fields.server = {};
  }
  for (const f in fields) {
    const path = f.split('.');
    let key = fields;

    // No dot in name, maybe no change
    if (path.length === 1) {
      if (fields[f].length > 0 && (singletonFields[f] || f.endsWith('Cnt') || f.endsWith('-cnt'))) {
        fields[f] = fields[f][0];
      }
      if (dateFields[f]) {
        fields[f] = Date.parse(fields[f]);
      }
      continue;
    }

    // Dot in name, will be moving
    let value = fields[f];
    if (singletonFields[f] || f.endsWith('Cnt') || f.endsWith('-cnt')) {
      value = value[0];
    }
    if (dateFields[f]) {
      value = Date.parse(value);
    }
    if (!unflatten) {
      fields[f] = value;
      continue;
    }
    delete fields[f];
    for (let i = 0; i < path.length; i++) {
      if (i === path.length - 1) {
        key[path[i]] = value;
        break;
      } else if (key[path[i]] === undefined) {
        key[path[i]] = {};
      }
      key = key[path[i]];
    }
  }
}

// Get a session from ES and decode packetPos if requested
exports.getSession = async (id, options, cb) => {
  if (internals.debug > 2) {
    console.log('GETSESSION -', id, options);
  }
  function fixPacketPos (session, fields) {
    if (!fields.packetPos || fields.packetPos.length === 0) {
      return cb(null, session);
    }
    exports.fileIdToFile(fields.node, -1 * fields.packetPos[0], (fileInfo) => {
      if (internals.debug > 2) {
        console.log('GETSESSION - fixPackPos', fileInfo);
      }
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

  const optionsReplaced = options === undefined;
  if (!options) {
    options = { _source: 'cert', fields: ['*'] };
  }
  const query = { query: { ids: { values: [exports.sid2Id(id)] } }, _source: options._source, fields: options.fields };

  const unflatten = options?.arkime_unflatten ?? true;
  const params = { };
  exports.merge(params, options);
  delete params._source;
  delete params.fields;
  delete params.arkime_unflatten;
  delete params.final;

  const index = exports.sid2Index(id, { multiple: true });
  exports.search(index, '_doc', query, params, (err, results) => {
    if (internals.debug > 2) {
      console.log('GETSESSION - search results', err, JSON.stringify(results, false, 2));
    }
    if (err) { return cb(err); }
    if (!results.hits || !results.hits.hits || results.hits.hits.length === 0) {
      if (options.final === true) {
        delete options.final;
        return cb('Not found');
      }
      options.final = true;
      internals.client7.indices.refresh({ index: fixIndex(index) }, () => {
        exports.getSession(id, options, cb);
      });
      return;
    }
    const session = results.hits.hits[0];
    session.found = true;
    if (session.fields && session._source && session._source.cert) {
      session.fields.cert = session._source.cert;
    }
    delete session._source;
    fixSessionFields(session.fields, unflatten);
    if (!optionsReplaced && options.fields && !options.fields.includes('packetPos')) {
      return cb(null, session);
    }
    return fixPacketPos(session, session.fields);
  });
};

exports.index = async (index, type, id, doc) => {
  return internals.client7.index({ index: fixIndex(index), body: doc, id: id });
};

exports.indexNow = async (index, type, id, doc) => {
  return internals.client7.index({
    index: fixIndex(index), body: doc, id: id, refresh: true
  });
};

exports.search = async (index, type, query, options, cb) => {
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

  let cancelId = null;
  if (options && options.cancelId) {
    // use opaqueId option so the task can be cancelled
    cancelId = { opaqueId: options.cancelId };
    delete options.cancelId;
  }

  exports.merge(params, options);

  try {
    const { body: results } = await internals.client7.search(params, cancelId);
    return cb ? cb(null, results) : results;
  } catch (err) {
    console.trace(`ES Search Error - query: ${JSON.stringify(params, false, 2)} err:`, err);
    if (cb) { return cb(err, null); }
    throw err;
  }
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

exports.searchScroll = function (index, type, query, options, cb) {
  // external scrolling, or multiesES or lesseq 10000, do a normal search which does its own Promise conversion
  if (query.scroll !== undefined || internals.multiES || (query.size ?? 0) + (parseInt(query.from ?? 0, 10)) <= 10000) {
    return exports.search(index, type, query, options, cb);
  }

  // Convert promise to cb by calling ourselves
  if (!cb) {
    return new Promise((resolve, reject) => {
      exports.searchScroll(index, query, type, options, (err, data) => {
        if (err) {
          reject(err);
        } else {
          resolve(data);
        }
      });
    });
  }

  // Now actually do the search scroll
  const from = +query.from || 0;
  const size = +query.size || 0;

  const querySize = from + size;
  delete query.from;

  let totalResults;
  const params = { scroll: '2m' };
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
            scroll: '2m', body: { scroll_id: response._scroll_id }
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
};

exports.searchSessions = function (index, query, options, cb) {
  if (cb === undefined) {
    return new Promise((resolve, reject) => {
      exports.searchSessions(index, query, options, (err, result) => {
        err ? reject(err) : resolve(result);
      });
    });
  }

  if (!options) { options = {}; }
  const unflatten = options.arkime_unflatten ?? true;
  const params = { preference: 'primaries', ignore_unavailable: 'true' };
  exports.merge(params, options);
  delete params.arkime_unflatten;
  exports.searchScroll(index, 'session', query, params, (err, result) => {
    if (err || result.hits.hits.length === 0) { return cb(err, result); }

    for (let i = 0; i < result.hits.hits.length; i++) {
      fixSessionFields(result.hits.hits[i].fields, unflatten);
    }
    return cb(null, result);
  });
};

exports.msearchSessions = async (index, queries, options) => {
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
    console.log('ERROR - fetching aliases', err.toString());
  }
};

exports.health = async () => {
  const { body: data } = await internals.client7.info();
  const { body: result } = await internals.client7.cluster.health();
  result.version = data.version.number;
  return result;
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
    throw err;
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

exports.update = async (index, type, id, doc, options) => {
  const params = {
    id: id,
    body: doc,
    timeout: '10m',
    retry_on_conflict: 3,
    index: fixIndex(index)
  };
  exports.merge(params, options);
  return internals.client7.update(params);
};

exports.updateSession = async (index, id, doc, cb) => {
  const params = {
    retry_on_conflict: 3,
    index: fixIndex(index),
    body: doc,
    id: id,
    timeout: '10m'
  };

  try {
    const { body: data } = await internals.client7.update(params);
    return cb(null, data);
  } catch (err) {
    if (err.statusCode !== 403) { return cb(err, {}); }
    try { // try clearing the index.blocks.write if we got a forbidden response
      exports.setIndexSettings(fixIndex(index), { body: { 'index.blocks.write': null } });
      const { body: retryData } = await internals.client7.update(params);
      return cb(null, retryData);
    } catch (err) {
      return cb(err, {});
    }
  }
};

exports.close = async () => {
  User.close();
  return internals.client7.close();
};

exports.reroute = async () => {
  return internals.client7.cluster.reroute({
    timeout: '10m',
    masterTimeout: '10m',
    retryFailed: true
  });
};

exports.flush = async (index) => {
  if (index === 'users') {
    return User.flush();
  } else if (index === 'lookups') {
    return internals.usersClient7.indices.flush({ index: fixIndex(index) });
  } else {
    return internals.client7.indices.flush({ index: fixIndex(index) });
  }
};

exports.refresh = async (index) => {
  if (index === 'users') {
    User.flush();
  } else if (index === 'lookups') {
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

exports.removeHuntFromSession = function (index, id, huntId, huntName, cb) {
  const script = `
    if (ctx._source.huntId != null) {
      int index = ctx._source.huntId.indexOf(params.huntId);
      if (index > -1) { ctx._source.huntId.remove(index); }
    }
    if (ctx._source.huntName != null) {
      int index = ctx._source.huntName.indexOf(params.huntName);
      if (index > -1) { ctx._source.huntName.remove(index); }
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
  User.flushCache();
  internals.shortcutsCache = {};
  delete internals.aliasesCache;
  exports.getAliasesCache();
};

function twoDigitString (value) {
  return (value < 10) ? ('0' + value) : value.toString();
}

// History DB interactions
exports.historyIt = async function (doc) {
  const d = new Date(Date.now());
  const jan = new Date(d.getUTCFullYear(), 0, 0);
  const iname = internals.prefix + 'history_v1-' +
    twoDigitString(d.getUTCFullYear() % 100) + 'w' +
    twoDigitString(Math.floor((d - jan) / 604800000));

  if (internals?.healthCache?.molochDbVersion < 72) {
    delete doc.esQuery;
    delete doc.esQueryIndices;
  }
  return internals.client7.index({
    index: iname, body: doc, refresh: true, timeout: '10m'
  });
};
exports.searchHistory = async (query) => {
  return internals.client7.search({
    index: internals.prefix === 'arkime_' ? 'history_v1-*,arkime_history_v1-*' : fixIndex('history_v1-*'),
    body: query,
    rest_total_hits_as_int: true
  });
};
exports.countHistory = async () => {
  return internals.client7.count({
    index: internals.prefix === 'arkime_' ? 'history_v1-*,arkime_history_v1-*' : fixIndex('history_v1-*'),
    ignoreUnavailable: true
  });
};
exports.deleteHistory = async (id, index) => {
  return internals.client7.delete({
    index: index, id: id, refresh: true
  });
};

// Hunt DB interactions
exports.createHunt = async (doc) => {
  if (internals?.healthCache?.molochDbVersion < 72) {
    delete doc.description;
  }
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
  exports.refresh('sessions*');
  return internals.client7.index({
    index: fixIndex('hunts'), body: doc, id: id, refresh: true, timeout: '10m'
  });
};
exports.getHunt = async (id) => {
  return internals.client7.get({ index: fixIndex('hunts'), id: id });
};

// fetches the version of the remote shortcuts index (remote db = user's es)
async function getShortcutsVersion () {
  const { body: doc } = await internals.usersClient7.indices.getMapping({
    index: internals.remoteShortcutsIndex
  });

  // get version of the first index (always want the first and only index returned)
  return doc[Object.keys(doc)[0]]?.mappings?._meta?.version || 0;
}
// updates the shortcuts index version in the remote db so that the local
// db knows to sync the shortcuts (remote db = user's es)
async function setShortcutsVersion () {
  // fetch the remote shortcuts index version so it can be incremented
  const version = await getShortcutsVersion();
  return internals.usersClient7.indices.putMapping({
    index: internals.remoteShortcutsIndex,
    body: { _meta: { version: version + 1, initSync: true } }
  });
}
// updates the shortcuts in the local db if they are out of sync with the remote db (remote db = user's es)
// if there's a users es set, then the shortcuts are saved in the remote db
// so they need to be periodically updated in the local db for searching by shortcuts to work
exports.updateLocalShortcuts = async () => {
  if (internals.multiES) { return; } // don't sync shortcuts for multies

  const msg = `updating local shortcuts (${internals.info.host}/${internals.localShortcutsIndex}) from remote (${internals.info.usersHost}/${internals.remoteShortcutsIndex})`;

  try {
    // fetch the version of the remote shortcuts index to check if the local shortcuts index
    // is up to date. if not, something has changed in the remote index and we need to sync
    const version = await getShortcutsVersion();

    if (version === internals.localShortcutsVersion) { return; } // version's match, stop!

    console.log(msg);

    internals.shortcutsCache = {}; // Clear cache when updating
    // fetch shortcuts from remote and local indexes
    const [{ body: remoteResults }, { body: localResults }] = await Promise.all([
      exports.searchShortcuts({ size: 10000 }), exports.searchShortcutsLocal({ size: 10000 })
    ]);

    // compare the local shortcuts to the remote shortcuts to determine
    // if any shortcuts have been deleted from the remote db
    for (const localShortcut of localResults.hits.hits) {
      let missing = true;
      for (const remoteShortcut of remoteResults.hits.hits) {
        if (remoteShortcut._id === localShortcut._id) {
          missing = false; // we found it, it's not missing
          break;
        }
      }
      // if we get here without the missing flag set to false
      if (missing) { // it's missing from the remote db
        if (internals.debug > 1) {
          console.log(`SHORTCUT - deleting ${localShortcut._id} ${localShortcut._source.name} locally`);
        }
        internals.client7.delete({ // remove the shortcut from the local db
          index: internals.localShortcutsIndex,
          id: localShortcut._id,
          refresh: true
        });
      }
    }

    // compare remote shortcuts to local shortcuts to determine if any
    // shortcuts have been added or updated from the remote db
    for (const remoteShortcut of remoteResults.hits.hits) {
      let missing = true;
      for (const localShortcut of localResults.hits.hits) {
        if (remoteShortcut._id === localShortcut._id) {
          missing = false; // found it, check if we need to update it
          if (remoteShortcut._version !== localShortcut._version) {
            if (internals.debug > 1) {
              console.log(`SHORTCUT - update from remote ${remoteShortcut._id} ${remoteShortcut._source.name}`);
            }
            // the versions don't match, this shortcut has been updated in the remote db
            internals.client7.index({ // update the shortcut in the local db
              id: remoteShortcut._id,
              index: internals.localShortcutsIndex,
              body: remoteShortcut._source,
              version_type: 'external',
              // use remote shortcut version since that is where it was edited
              version: remoteShortcut._version // (should have highest version)
            });
          }
        }
      }
      // if we get here without the missing flag set to false
      if (missing) { // it's missing from the local db
        if (internals.debug > 1) {
          console.log(`SHORTCUT - add from remote ${remoteShortcut._id} ${remoteShortcut._source.name}`);
        }
        internals.client7.index({ // add the shortcut in the local db
          id: remoteShortcut._id,
          index: internals.localShortcutsIndex,
          body: remoteShortcut._source,
          version_type: 'external',
          // don't need to increment version because this is the first time the
          version: remoteShortcut._version // local db has seen this shortcut
        });
      }
    }

    internals.localShortcutsVersion = version;
  } catch (err) {
    console.log(`ERROR - ${msg}:`, err);
  }
};
exports.searchShortcuts = async (query) => {
  return internals.usersClient7.search({
    index: internals.remoteShortcutsIndex, body: query, rest_total_hits_as_int: true, version: true
  });
};
exports.searchShortcutsLocal = async (query) => {
  return internals.client7.search({
    index: internals.localShortcutsIndex, body: query, rest_total_hits_as_int: true, version: true
  });
};
exports.numberOfShortcuts = async () => {
  return internals.usersClient7.count({
    index: internals.remoteShortcutsIndex
  });
};
exports.createShortcut = async (doc) => {
  internals.shortcutsCache = {};
  await setShortcutsVersion();
  const response = await internals.usersClient7.index({
    index: internals.remoteShortcutsIndex, body: doc, refresh: 'wait_for', timeout: '10m'
  });
  if (internals.doShortcutsUpdates) { exports.updateLocalShortcuts(); }
  return response;
};
exports.deleteShortcut = async (id) => {
  internals.shortcutsCache = {};
  await setShortcutsVersion();
  const response = await internals.usersClient7.delete({
    index: internals.remoteShortcutsIndex, id: id, refresh: true
  });
  if (internals.doShortcutsUpdates) { exports.updateLocalShortcuts(); }
  return response;
};
exports.setShortcut = async (id, doc) => {
  internals.shortcutsCache = {};
  await setShortcutsVersion();
  const response = await internals.usersClient7.index({
    index: internals.remoteShortcutsIndex, body: doc, id: id, refresh: true, timeout: '10m'
  });
  if (internals.doShortcutsUpdates) { exports.updateLocalShortcuts(); }
  return response;
};
exports.getShortcut = async (id) => {
  return internals.usersClient7.get({ index: internals.remoteShortcutsIndex, id: id });
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
    },
    size: 10000
  };

  const { body: { hits: shortcuts } } = await exports.searchShortcutsLocal(query);

  const shortcutsMap = {};
  for (const shortcut of shortcuts.hits) {
    // need the whole object to test for type mismatch
    shortcutsMap[shortcut._source.name] = shortcut;
  }

  internals.shortcutsCache[userId] = shortcutsMap;
  internals.shortcutsCache._timeStamp = Date.now();

  return shortcutsMap;
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
        name: fixIndex('sessions3_template'), filter_path: '**._meta'
      });
      health.molochDbVersion = doc[fixIndex('sessions3_template')].mappings._meta.molochDbVersion;
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
    throw err;
  }
};

exports.nodesInfoCache = async () => {
  if (internals.nodesInfoCache._timeStamp !== undefined && internals.nodesInfoCache._timeStamp > Date.now() - 30000) {
    return internals.nodesInfoCache;
  }

  const { body: data } = await exports.nodesInfo();
  internals.nodesInfoCache = data;
  internals.nodesInfoCache._timeStamp = Date.now();
  return data;
};

exports.masterCache = async () => {
  if (internals.masterCache._timeStamp !== undefined && internals.masterCache._timeStamp > Date.now() - 60000) {
    return internals.masterCache;
  }

  const { body: data } = await exports.master();
  internals.masterCache = data;
  internals.masterCache._timeStamp = Date.now();
  return data;
};

exports.nodesStatsCache = async () => {
  if (internals.nodesStatsCache._timeStamp !== undefined && internals.nodesStatsCache._timeStamp > Date.now() - 2500) {
    return internals.nodesStatsCache;
  }

  const { body: data } = await exports.nodesStats({
    metric: 'jvm,process,fs,os,indices,thread_pool'
  });
  internals.nodesStatsCache = data;
  internals.nodesStatsCache._timeStamp = Date.now();
  return data;
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
    throw err;
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
    throw err;
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
    if (cb) {
      return setImmediate(() => { cb(info); });
    }
    return info;
  }

  let file = null;
  try {
    const { body: fresult } = await exports.get('files', 'file', node + '-' + num);
    file = fresult._source;
    internals.fileId2File[key] = file;
    internals.fileName2File[file.name] = file;
  } catch (err) { // Cache file is unknown
    internals.fileId2File[key] = null;
  }
  return cb ? cb(file) : file;
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
  const { body: sinfo } = await exports.index('sequence', 'sequence', sName, {});
  return sinfo._version;
};

exports.numberOfDocuments = async (index, options) => {
  // count interface is slow for larget data sets, don't use for sessions unless multiES
  if (index !== 'sessions2-*' || internals.multiES) {
    const params = { index: fixIndex(index), ignoreUnavailable: true };
    exports.merge(params, options);
    const { body: total } = await internals.client7.count(params);
    return { count: total.count };
  }

  let count = 0;
  const str = `${internals.prefix}sessions2-`;

  const indices = await exports.indicesCache();

  for (let i = 0; i < indices.length; i++) {
    if (indices[i].index.includes(str)) {
      count += parseInt(indices[i]['docs.count']);
    }
  }

  return { count: count };
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

  try {
    const { body: doc } = await internals.client7.indices.getTemplate({
      name: fixIndex('sessions3_template'),
      filter_path: '**._meta'
    });

    try {
      const molochDbVersion = doc[fixIndex('sessions3_template')].mappings._meta.molochDbVersion;

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
  } catch (err) {
    console.log("ERROR - Couldn't retrieve database version, is ES running?  Have you run ./db.pl host:port init?", err);
    process.exit(0);
  }

  if (checkUsers) {
    const count = await User.numberOfUsers();
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
    cb();
  });
};

exports.session2Sid = function (item) {
  const ver = item._index.includes('sessions2') ? '2@' : '3@';
  if (item._id.length < 31) {
    // sessions2 didn't have new arkime_ prefix
    if (ver === '2@' && internals.prefix === 'arkime_') {
      return ver + item._index.substring(10) + ':' + item._id;
    } else {
      return ver + item._index.substring(internals.prefix.length + 10) + ':' + item._id;
    }
  }

  return ver + item._id;
};

exports.sid2Id = function (id) {
  if (id[1] === '@') {
    id = id.substr(2);
  }

  const colon = id.indexOf(':');
  if (colon > 0) {
    return id.substr(colon + 1);
  }

  return id;
};

exports.sid2Index = function (id, options) {
  const colon = id.indexOf(':');

  if (id[1] === '@') {
    if (colon > 0) {
      return 'sessions' + id[0] + '-' + id.substr(2, colon - 2);
    }
    return 'sessions' + id[0] + '-' + id.substr(2, id.indexOf('-') - 2);
  }

  const s3 = 'sessions3-' + ((colon > 0) ? id.substr(0, colon) : id.substr(0, id.indexOf('-')));
  const s2 = 'sessions2-' + ((colon > 0) ? id.substr(0, colon) : id.substr(0, id.indexOf('-')));

  if (!internals.aliasesCache) {
    return s3;
  }

  const fs2 = fixIndex(s2);

  const results = [];

  if (internals.aliasesCache[fs2] || internals.aliasesCache[fs2 + '-reindex'] || internals.aliasesCache[fs2 + '-shrink']) {
    results.push(fs2);
  }

  const fs3 = fixIndex(s3);
  if (internals.aliasesCache[fs3] || internals.aliasesCache[fs3 + '-reindex'] || internals.aliasesCache[fs3 + '-shrink']) {
    results.push(fs3);
  }

  if (results.length > 1 && options?.multiple) {
    return results;
  }

  return results[0];
};

exports.loadFields = async () => {
  return exports.search('fields', 'field', { size: 3000 });
};

exports.getIndices = async (startTime, stopTime, bounding, rotateIndex) => {
  try {
    const aliases = await exports.getAliasesCache(['sessions2-*', 'sessions3-*']);
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
      if (index.startsWith('sessions2-')) { // sessions2 might not have prefix
        index = index.substring(10);
      } else {
        index = index.substring(internals.prefix.length + 10);
      }
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
      return fixIndex(['sessions2-*', 'sessions3-*']);
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
    throw err;
  }
};

exports.getTemplate = async (templateName) => {
  return internals.client7.indices.getTemplate({ name: fixIndex(templateName), flat_settings: true });
};

exports.putTemplate = async (templateName, body) => {
  return internals.client7.indices.putTemplate({ name: fixIndex(templateName), body: body });
};

exports.setQueriesNode = async (node) => {
  internals.client7.indices.putMapping({
    index: fixIndex('queries'),
    body: { _meta: { node: node, updateTime: Date.now() } }
  });
};

exports.getQueriesNode = async () => {
  const { body: doc } = await internals.client7.indices.getMapping({
    index: fixIndex('queries')
  });

  // Since queries is an alias we dont't know the real index name here
  const meta = doc[Object.keys(doc)[0]].mappings._meta;

  return {
    node: meta?.node,
    updateTime: meta?.updateTime
  };
};
