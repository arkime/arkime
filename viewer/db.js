/******************************************************************************/
/* db.js -- Lowlevel and highlevel functions dealing with the database
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

const os = require('os');
const fs = require('fs');
const async = require('async');
const { Client } = require('@elastic/elasticsearch');
const User = require('../common/user');
const ArkimeUtil = require('../common/arkimeUtil');
const LRU = require('lru-cache');

const cache = new LRU({ max: 100, maxAge: 1000 * 10 });
const Db = exports;

const internals = {
  fileId2File: new Map(),
  fileName2File: new Map(),
  arkimeNodeStatsCache: new Map(),
  shortcutsCache: new Map(),
  shortcutsCacheTS: new Map(),
  sessionIndices: ['sessions2-*', 'sessions3-*'],
  queryExtraIndicesRegex: [],
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
        console.log(`ERROR - OpenSearch/Elasticsearch endpoint url '${node}' must start with http:// or https://`);
        process.exit();
      }
    });
  } else if (!nodes.startsWith('http')) {
    console.log(`ERROR - OpenSearch/Elasticsearch endpoint url '${nodes}' must start with http:// or https://`);
    process.exit();
  }
}

Db.initialize = async (info, cb) => {
  internals.multiES = info.multiES === 'true' || info.multiES === true || false;
  delete info.multiES;

  internals.debug = info.debug || 0;
  delete info.debug;

  internals.maxConcurrentShardRequests = info.maxConcurrentShardRequests;
  delete info.maxConcurrentShardRequests;

  internals.info = info;

  checkURLs(info.host);
  checkURLs(info.usersHost);

  internals.prefix = ArkimeUtil.formatPrefix(info.prefix);
  internals.usersPrefix = ArkimeUtil.formatPrefix(info.usersPrefix ?? info.prefix);

  internals.nodeName = info.nodeName;
  delete info.nodeName;
  internals.hostName = info.hostName;
  delete info.hostName;

  internals.esProfile = info.esProfile || false;
  delete info.esProfile;

  const esSSLOptions = { rejectUnauthorized: !internals.info.insecure };
  if (internals.info.caTrustFile) { esSSLOptions.ca = ArkimeUtil.certificateFileToArray(internals.info.caTrustFile); };
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
    basicAuth = ArkimeUtil.splitRemain(basicAuth, ':', 1);
    esClientOptions.auth = {
      username: basicAuth[0],
      password: basicAuth[1]
    };
  }

  internals.client7 = new Client(esClientOptions);

  if (info.usersHost) {
    User.initialize({
      insecure: info.insecure,
      caTrustFile: info.caTrustFile,
      requestTimeout: info.requestTimeout,
      node: info.usersHost,
      clientKey: info.esClientKey,
      clientCert: info.esClientCert,
      clientKeyPass: info.esClientKeyPass,
      apiKey: info.usersEsApiKey,
      basicAuth: info.usersEsBasicAuth,
      prefix: internals.usersPrefix,
      getCurrentUserCB: info.getCurrentUserCB,
      noUsersCheck: info.noUsersCheck
    });
  } else {
    User.initialize({
      insecure: info.insecure,
      caTrustFile: info.caTrustFile,
      requestTimeout: info.requestTimeout,
      node: info.host,
      clientKey: info.esClientKey,
      clientCert: info.esClientCert,
      clientKeyPass: info.esClientKeyPass,
      apiKey: info.esApiKey,
      basicAuth: info.esBasicAuth,
      prefix: internals.prefix,
      readOnly: internals.multiES,
      getCurrentUserCB: info.getCurrentUserCB,
      noUsersCheck: info.noUsersCheck
    });
  }

  internals.usersClient7 = User.getClient();

  // Replace tag implementation
  if (internals.multiES) {
    Db.isLocalView = (node, yesCB, noCB) => { return noCB(); };
    internals.prefix = 'MULTIPREFIX_';
  }

  if (internals.debug) {
    console.log(`prefix:${internals.prefix} usersPrefix:${internals.usersPrefix}`);
  }

  // build regular expressions for the user-specified extra query index patterns
  if (Array.isArray(info.queryExtraIndices)) {
    internals.sessionIndices = [...new Set([...['sessions2-*', 'sessions3-*'], ...info.queryExtraIndices])];
    for (const pattern in info.queryExtraIndices) {
      internals.queryExtraIndicesRegex.push(ArkimeUtil.wildcardToRegexp(info.queryExtraIndices[pattern]));
    }
    if (internals.debug > 2) {
      console.log(`defaultIndexPatterns: ${internals.sessionIndices}`);
    }
  }

  // Update aliases cache so -shrink/-reindex works
  if (internals.nodeName !== undefined) {
    Db.getAliasesCache(internals.sessionIndices);
    setInterval(() => { Db.getAliasesCache(internals.sessionIndices); }, 2 * 60 * 1000);
  }

  internals.localShortcutsIndex = fixIndex('lookups');
  if (internals.info.usersHost && internals.usersPrefix !== undefined) {
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
        console.log(`ERROR - OpenSearch ${data.version.number} not supported, OpenSearch 1.0.0 or later required.`);
        process.exit();
      }
    } else {
      if (data.version.number.match(/^([0-6]|7\.[0-9]\.)/)) {
        console.log(`ERROR - Elasticsearch ${data.version.number} not supported, Elasticsearch 7.10.0 or later required.`);
        process.exit();
      }
    }
    return cb();
  } catch (err) {
    ArkimeUtil.searchErrorMsg(err, internals.info.host, 'Getting OpenSearch/Elasticsearch info failed');
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

  // Don't fix extra user-specified indexes from the queryExtraIndices
  if (!internals.queryExtraIndicesRegex.some(re => re.test(index))) {
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
  }

  return index;
}
Db.fixIndex = fixIndex;

Db.merge = (to, from) => {
  for (const key in from) {
    to[key] = from[key];
  }
};

Db.get = async (index, type, id) => {
  return internals.client7.get({ index: fixIndex(index), id });
};

Db.getWithOptions = async (index, type, id, options) => {
  const params = { index: fixIndex(index), id };
  Db.merge(params, options);
  return internals.client7.get(params);
};

Db.getSessionPromise = (id, options) => {
  return new Promise((resolve, reject) => {
    Db.getSession(id, options, (err, session) => {
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

const dedupFields = {
  'dns.host': true,
  'dns.mailserverHost': true,
  'dns.opcode': true,
  'dns.status': true,
  'dns.qt': true,
  'dns.qc': true
};

const dateFields = {
  firstPacket: true,
  lastPacket: true,
  'cert.notBefore': true,
  'cert.notAfter': true
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
    if (dateFields[f]) {
      for (let v = 0; v < value.length; v++) {
        value[v] = Date.parse(value[v]);
      }
    }
    if (singletonFields[f] || f.endsWith('Cnt') || f.endsWith('-cnt')) {
      value = value[0];
    }
    if (dedupFields[f]) {
      value = [...new Set(value)].sort();
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

// Get a session from OpenSearch/Elasticsearch and decode packetPos if requested
Db.getSession = async (id, options, cb) => {
  if (internals.debug > 2) {
    console.log('GETSESSION -', id, options);
  }
  function fixPacketPos (session, fields) {
    if (!fields.packetPos || fields.packetPos.length === 0) {
      return cb(null, session);
    }
    Db.fileIdToFile(fields.node, -1 * fields.packetPos[0], (fileInfo) => {
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
          Db.isLocalView(fields.node, () => {
            const newPacketPos = [];
            async.forEachOfSeries(fields.packetPos, (item, key, nextCb) => {
              if (key % 3 !== 0) { return nextCb(); } // Only look at every 3rd item

              Db.fileIdToFile(fields.node, -1 * item, (idToFileInfo) => {
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
    options = { _source: ['cert', 'dns'], fields: ['*'] };
  }
  const query = { query: { ids: { values: [Db.sid2Id(id)] } }, _source: options._source, fields: options.fields };

  const unflatten = options?.arkime_unflatten ?? true;
  const params = { };
  Db.merge(params, options);
  delete params._source;
  delete params.fields;
  delete params.arkime_unflatten;
  delete params.final;

  const index = Db.sid2Index(id, { multiple: true });
  Db.search(index, '_doc', query, params, (err, results) => {
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
        Db.getSession(id, options, cb);
      });
      return;
    }
    const session = results.hits.hits[0];
    session.found = true;
    if (session.fields && session._source && session._source.cert) {
      session.fields.cert = session._source.cert;
    }
    if (session.fields && session._source && session._source.dns) {
      session.fields.dns = session._source.dns;
    }
    delete session._source;
    fixSessionFields(session.fields, unflatten);
    if (!optionsReplaced && options.fields && !options.fields.includes('packetPos')) {
      return cb(null, session);
    }
    return fixPacketPos(session, session.fields);
  });
};

Db.index = async (index, type, id, doc) => {
  return internals.client7.index({ index: fixIndex(index), body: doc, id });
};

Db.indexNow = async (index, type, id, doc) => {
  return internals.client7.index({
    index: fixIndex(index), body: doc, id, refresh: true
  });
};

Db.search = async (index, type, query, options, cb) => {
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

  Db.merge(params, options);

  try {
    const { body: results } = await internals.client7.search(params, cancelId);
    if (!internals.debug && internals.esProfile) {
      console.log('QUERY:', JSON.stringify(query, false, 2));
      console.log('RESPONSE:', JSON.stringify(results, false, 2));
    }

    return cb ? cb(null, results) : results;
  } catch (err) {
    console.trace(`OpenSearch/Elasticsearch Search Error - query: ${JSON.stringify(params, false, 2)} err:`, err);
    if (cb) { return cb(err, null); }
    throw err;
  }
};

Db.cancelByOpaqueId = async (cancelId, cluster) => {
  const { body: results } = await internals.client7.tasks.list({
    detailed: false,
    group_by: 'parents',
    cluster
  });

  let found = false;

  for (const resultKey in results.tasks) {
    const result = results.tasks[resultKey];
    if (result.headers &&
      result.headers['X-Opaque-Id'] &&
      result.headers['X-Opaque-Id'] === cancelId) {
      found = true;
      // don't need to wait for task to cancel, just break out and return
      internals.client7.tasks.cancel({ taskId: resultKey, cluster });
      break;
    }
  }

  if (!found) { // not found, return error
    throw new Error('Cancel ID not found, cannot cancel OpenSearch/Elasticsearch task(s)');
  }

  return 'OpenSearch/Elasticsearch task cancelled succesfully';
};

Db.searchScroll = function (index, type, query, options, cb) {
  // external scrolling, or multiesES or lesseq 10000, do a normal search which does its own Promise conversion
  if (query.scroll !== undefined || internals.multiES || (query.size ?? 0) + (parseInt(query.from ?? 0, 10)) <= 10000) {
    return Db.search(index, type, query, options, cb);
  }

  // Convert promise to cb by calling ourselves
  if (!cb) {
    return new Promise((resolve, reject) => {
      Db.searchScroll(index, query, type, options, (err, data) => {
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
  Db.merge(params, options);
  query.size = 1000; // Get 1000 items per scroll call
  query.profile = internals.esProfile;
  Db.search(index, type, query, params,
    async function getMoreUntilDone (error, response) {
      if (error) {
        if (totalResults && from > 0) {
          totalResults.hits.hits = totalResults.hits.hits.slice(from);
        }
        if (response && response._scroll_id) {
          Db.clearScroll({ body: { scroll_id: response._scroll_id } });
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
          const { body: results } = await Db.scroll({
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
          Db.clearScroll({ body: { scroll_id: response._scroll_id } });
        }
        return cb(null, totalResults);
      }
    });
};

Db.searchSessions = function (index, query, options, cb) {
  if (cb === undefined) {
    return new Promise((resolve, reject) => {
      Db.searchSessions(index, query, options, (err, result) => {
        err ? reject(err) : resolve(result);
      });
    });
  }

  if (!options) { options = {}; }
  const unflatten = options.arkime_unflatten ?? true;
  const params = { preference: 'primaries', ignore_unavailable: 'true' };
  if (internals.maxConcurrentShardRequests) { params.maxConcurrentShardRequests = internals.maxConcurrentShardRequests; }
  Db.merge(params, options);
  delete params.arkime_unflatten;
  Db.searchScroll(index, 'session', query, params, (err, result) => {
    if (err || result.hits.hits.length === 0) { return cb(err, result); }

    for (let i = 0; i < result.hits.hits.length; i++) {
      fixSessionFields(result.hits.hits[i].fields, unflatten);
    }
    return cb(null, result);
  });
};

Db.msearchSessions = async (index, queries, options) => {
  const body = [];

  for (let i = 0, ilen = queries.length; i < ilen; i++) {
    body.push({ index: fixIndex(index) });
    body.push(queries[i]);
  }

  const params = { body, rest_total_hits_as_int: true };

  let cancelId = null;
  if (options && options.cancelId) {
    // use opaqueId option so the task can be cancelled
    cancelId = { opaqueId: options.cancelId };
  }

  return internals.client7.msearch(params, cancelId);
};

Db.scroll = async (params) => {
  params.rest_total_hits_as_int = true;
  return internals.client7.scroll(params);
};

Db.clearScroll = async (params) => {
  return internals.client7.clearScroll(params);
};

Db.deleteDocument = async (index, type, id, options) => {
  const params = { index: fixIndex(index), id };
  Db.merge(params, options);
  return internals.client7.delete(params);
};

// This API does not call fixIndex
Db.deleteIndex = async (index, options) => {
  const params = { index };
  Db.merge(params, options);
  return internals.client7.indices.delete(params);
};

// This API does not call fixIndex
Db.optimizeIndex = async (index, options) => {
  const params = { index, maxNumSegments: 1 };
  Db.merge(params, options);
  return internals.client7.indices.forcemerge(params);
};

// This API does not call fixIndex
Db.closeIndex = async (index, options) => {
  const params = { index };
  Db.merge(params, options);
  return internals.client7.indices.close(params);
};

// This API does not call fixIndex
Db.openIndex = async (index, options) => {
  const params = { index };
  Db.merge(params, options);
  return internals.client7.indices.open(params);
};

// This API does not call fixIndex
Db.shrinkIndex = async (index, options) => {
  const params = { index, target: `${index}-shrink` };
  Db.merge(params, options);
  return internals.client7.indices.shrink(params);
};

Db.indexStats = async (index) => {
  return internals.client7.indices.stats({ index: fixIndex(index) });
};

Db.getAliases = async (index) => {
  return internals.client7.indices.getAlias({ index: fixIndex(index) });
};

Db.getAliasesCache = async (index) => {
  if (internals.aliasesCache && internals.aliasesCacheTimeStamp > Date.now() - 5000) {
    return internals.aliasesCache;
  }

  try {
    const { body: aliases } = await Db.getAliases(index);
    internals.aliasesCacheTimeStamp = Date.now();
    internals.aliasesCache = aliases;
    return aliases;
  } catch (err) {
    console.log('ERROR - fetching aliases', err.toString());
  }
};

Db.health = async (cluster) => {
  const { body: data } = await internals.client7.info({ cluster });
  const { body: result } = await internals.client7.cluster.health({ cluster });
  result.version = data.version.number;
  return result;
};

Db.indices = async (index, cluster) => {
  return internals.client7.cat.indices({
    format: 'json',
    index,
    bytes: 'b',
    h: 'health,status,index,uuid,pri,rep,docs.count,store.size,cd,segmentsCount,pri.search.query_current,memoryTotal',
    cluster
  });
};

Db.indicesSettings = async (index, cluster) => {
  return internals.client7.indices.getSettings({
    flatSettings: true,
    index: fixIndex(index),
    cluster
  });
};

Db.setIndexSettings = async (index, options) => {
  // Users might be on a different cluster
  if ((index === '*' || index.includes('users')) && internals.info.usersHost) {
    try {
      await internals.usersClient7.indices.putSettings({
        index,
        body: options.body,
        timeout: '10m',
        masterTimeout: '10m',
        cluster: options.cluster
      });
    } catch (err) {
    }
  }

  try {
    const { body: response } = await internals.client7.indices.putSettings({
      index,
      body: options.body,
      timeout: '10m',
      masterTimeout: '10m',
      cluster: options.cluster
    });
    return response;
  } catch (err) {
    cache.reset();
    throw err;
  }
};

Db.clearCache = async (cluster) => {
  return internals.client7.indices.clearCache({ cluster });
};

Db.shards = async (options) => {
  return internals.client7.cat.shards({
    format: 'json',
    bytes: 'b',
    h: 'index,shard,prirep,state,docs,store,ip,node,ur,uf,fm,sm',
    cluster: options?.cluster
  });
};

Db.allocation = async (cluster) => {
  return internals.client7.cat.allocation({ format: 'json', bytes: 'b', cluster });
};

Db.recovery = async (sortField, activeOnly, cluster) => {
  return internals.client7.cat.recovery({
    format: 'json',
    bytes: 'b',
    s: sortField,
    active_only: activeOnly,
    cluster
  });
};

Db.master = async (cluster) => {
  return internals.client7.cat.master({ format: 'json', cluster });
};

Db.getClusterSettings = async (options) => {
  return internals.client7.cluster.getSettings(options);
};

Db.putClusterSettings = async (options) => {
  options.timeout = '10m';
  options.masterTimeout = '10m';
  return internals.client7.cluster.putSettings(options);
};

Db.tasks = async (options) => {
  return internals.client7.tasks.list({ detailed: true, group_by: 'parents', cluster: options.cluster });
};

Db.taskCancel = async (taskId, cluster) => {
  return internals.client7.tasks.cancel({ taskId, cluster });
};

Db.nodesStats = async (options) => {
  return internals.client7.nodes.stats(options);
};

Db.nodesInfo = async (options) => {
  return internals.client7.nodes.info(options);
};

Db.update = async (index, type, id, doc, options) => {
  const params = {
    id,
    body: doc,
    timeout: '10m',
    retry_on_conflict: 3,
    index: fixIndex(index)
  };
  Db.merge(params, options);
  return internals.client7.update(params);
};

Db.updateSession = async (index, id, doc, cb) => {
  const params = {
    retry_on_conflict: 3,
    index: fixIndex(index),
    body: doc,
    id,
    timeout: '10m'
  };

  try {
    const { body: data } = await internals.client7.update(params);
    return cb(null, data);
  } catch (err) {
    if (err.statusCode !== 403) { return cb(err, {}); }
    try { // try clearing the index.blocks.write if we got a forbidden response
      Db.setIndexSettings(fixIndex(index), { body: { 'index.blocks.write': null } });
      const { body: retryData } = await internals.client7.update(params);
      return cb(null, retryData);
    } catch (err) {
      return cb(err, {});
    }
  }
};

Db.close = async () => {
  User.close();
  return internals.client7.close();
};

Db.reroute = async (cluster) => {
  return internals.client7.cluster.reroute({
    timeout: '10m',
    masterTimeout: '10m',
    retryFailed: true,
    cluster
  });
};

Db.flush = async (index, cluster) => {
  if (index === 'users') {
    return User.flush(cluster);
  } else if (index === 'lookups') {
    return internals.usersClient7.indices.flush({ index: `${internals.usersPrefix}${index}`, cluster });
  } else {
    return internals.client7.indices.flush({ index: fixIndex(index), cluster });
  }
};

Db.refresh = async (index, cluster) => {
  if (index === 'users') {
    User.flush(cluster);
  } else if (index === 'lookups') {
    return internals.usersClient7.indices.refresh({ index: `${internals.usersPrefix}${index}`, cluster });
  } else {
    return internals.client7.indices.refresh({ index: fixIndex(index), cluster });
  }
};

Db.addTagsToSession = function (index, id, tags, cluster, cb) {
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
        tags
      }
    }
  };

  if (cluster) { body.cluster = cluster; }

  Db.updateSession(index, id, body, cb);
};

Db.removeTagsFromSession = function (index, id, tags, cluster, cb) {
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
        tags
      }
    }
  };

  if (cluster) { body.cluster = cluster; }

  Db.updateSession(index, id, body, cb);
};

Db.addHuntToSession = function (index, id, huntId, huntName, cb) {
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
        huntId,
        huntName
      }
    }
  };

  Db.updateSession(index, id, body, cb);
};

Db.removeHuntFromSession = function (index, id, huntId, huntName, cb) {
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
        huntId,
        huntName
      }
    }
  };

  Db.updateSession(index, id, body, cb);
};

/// ///////////////////////////////////////////////////////////////////////////////
/// / High level functions
/// ///////////////////////////////////////////////////////////////////////////////
Db.flushCache = function () {
  internals.fileId2File.clear();
  internals.fileName2File.clear();
  internals.arkimeNodeStatsCache.clear();
  User.flushCache();
  internals.shortcutsCache.clear();
  delete internals.aliasesCache;
  Db.getAliasesCache();
  cache.reset();
};

function twoDigitString (value) {
  return (value < 10) ? ('0' + value) : value.toString();
}

// History DB interactions
Db.historyIt = async function (doc) {
  const d = new Date(Date.now());
  const jan = new Date(d.getUTCFullYear(), 0, 0);
  const iname = internals.prefix + 'history_v1-' +
    twoDigitString(d.getUTCFullYear() % 100) + 'w' +
    twoDigitString(Math.floor((d - jan) / 604800000));

  return internals.client7.index({
    index: iname, body: doc, refresh: true, timeout: '10m'
  });
};
Db.searchHistory = async (query) => {
  return internals.client7.search({
    index: internals.prefix === 'arkime_' ? 'history_v1-*,arkime_history_v1-*' : fixIndex('history_v1-*'),
    body: query,
    rest_total_hits_as_int: true
  });
};
Db.countHistory = async (cluster) => {
  return internals.client7.count({
    index: internals.prefix === 'arkime_' ? 'history_v1-*,arkime_history_v1-*' : fixIndex('history_v1-*'),
    ignoreUnavailable: true,
    cluster
  });
};
Db.deleteHistory = async (id, index, cluster) => {
  return internals.client7.delete({
    index, id, refresh: true, cluster
  });
};

// Hunt DB interactions
Db.createHunt = async (doc) => {
  return internals.client7.index({
    index: fixIndex('hunts'), body: doc, refresh: 'wait_for', timeout: '10m'
  });
};
Db.searchHunt = async (query) => {
  return internals.client7.search({
    index: fixIndex('hunts'), body: query, rest_total_hits_as_int: true
  });
};
Db.countHunts = async () => {
  return internals.client7.count({ index: fixIndex('hunts') });
};
Db.deleteHunt = async (id) => {
  return internals.client7.delete({
    index: fixIndex('hunts'), id, refresh: true
  });
};
Db.setHunt = async (id, doc) => {
  await Db.refresh('sessions*');
  return internals.client7.index({
    index: fixIndex('hunts'), body: doc, id, refresh: true, timeout: '10m'
  });
};
Db.updateHunt = async (id, doc) => {
  const params = {
    refresh: true,
    retry_on_conflict: 3,
    index: fixIndex('hunts'),
    body: { doc },
    id,
    timeout: '10m'
  };

  return internals.client7.update(params);
};
Db.getHunt = async (id) => {
  return internals.client7.get({ index: fixIndex('hunts'), id });
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
Db.updateLocalShortcuts = async () => {
  if (!internals.info.usersHost ||
     !internals.info.isPrimaryViewer || // If no isPrimaryViewer then we aren't actually viewer, dont do this
    !internals.info.isPrimaryViewer() ||
    internals.info.host === internals.info.usersHost) {
    return;
  }

  if (internals.multiES) { return; } // don't sync shortcuts for multies

  const msg = `updating local shortcuts (${internals.info.host}/${internals.localShortcutsIndex}) from remote (${internals.info.usersHost}/${internals.remoteShortcutsIndex})`;

  try {
    // fetch the version of the remote shortcuts index to check if the local shortcuts index
    // is up to date. if not, something has changed in the remote index and we need to sync
    const version = await getShortcutsVersion();

    if (version === internals.localShortcutsVersion) { return; } // version's match, stop!

    console.log(msg);

    internals.shortcutsCache.clear(); // Clear cache when updating
    // fetch shortcuts from remote and local indexes
    const [{ body: remoteResults }, { body: localResults }] = await Promise.all([
      Db.searchShortcuts({ size: 10000 }), Db.searchShortcutsLocal({ size: 10000 })
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
Db.searchShortcuts = async (query) => {
  return internals.usersClient7.search({
    index: internals.remoteShortcutsIndex, body: query, rest_total_hits_as_int: true, version: true
  });
};
Db.searchShortcutsLocal = async (query) => {
  return internals.client7.search({
    index: internals.localShortcutsIndex, body: query, rest_total_hits_as_int: true, version: true
  });
};
Db.numberOfShortcuts = async (query) => {
  return internals.usersClient7.count({
    index: internals.remoteShortcutsIndex, body: query
  });
};
Db.createShortcut = async (doc) => {
  internals.shortcutsCache.clear();
  await setShortcutsVersion();
  const response = await internals.usersClient7.index({
    index: internals.remoteShortcutsIndex, body: doc, refresh: 'wait_for', timeout: '10m'
  });
  Db.updateLocalShortcuts();
  return response;
};
Db.deleteShortcut = async (id) => {
  internals.shortcutsCache.clear();
  await setShortcutsVersion();
  const response = await internals.usersClient7.delete({
    index: internals.remoteShortcutsIndex, id, refresh: true
  });
  Db.updateLocalShortcuts();
  return response;
};
Db.setShortcut = async (id, doc) => {
  internals.shortcutsCache.clear();
  await setShortcutsVersion();
  const response = await internals.usersClient7.index({
    index: internals.remoteShortcutsIndex, body: doc, id, refresh: true, timeout: '10m'
  });
  Db.updateLocalShortcuts();
  return response;
};
Db.getShortcut = async (id) => {
  return internals.usersClient7.get({ index: internals.remoteShortcutsIndex, id });
};
Db.getShortcutsCache = async (user) => {
  const cshortcuts = internals.shortcutsCache.get(user.userId);
  if (cshortcuts && internals.shortcutsCacheTS.get(user.userId) > Date.now() - 30000) {
    return cshortcuts;
  }

  const roles = [...await user.getRoles()]; // es requries an array for terms search

  // only get shortcuts for this user or shared
  const query = {
    query: {
      bool: {
        should: [
          { terms: { roles } }, // shared via user role
          { term: { users: user.userId } }, // shared via userId
          { term: { userId: user.userId } } // created by this user
        ]
      }
    },
    size: 10000
  };

  const { body: { hits: shortcuts } } = await Db.searchShortcutsLocal(query);

  const shortcutsMap = {};
  for (const shortcut of shortcuts.hits) {
    // need the whole object to test for type mismatch
    shortcutsMap[shortcut._source.name] = shortcut;
  }

  internals.shortcutsCache.set(user.userId, shortcutsMap);
  internals.shortcutsCacheTS.set(user.userId, Date.now());

  return shortcutsMap;
};

Db.searchViews = async (query) => {
  return internals.usersClient7.search({
    index: `${internals.usersPrefix}views`, body: query, rest_total_hits_as_int: true, version: true
  });
};
Db.numberOfViews = async (query) => {
  return internals.usersClient7.count({
    index: `${internals.usersPrefix}views`, body: query
  });
};
Db.createView = async (doc) => {
  return await internals.usersClient7.index({
    index: `${internals.usersPrefix}views`, body: doc, refresh: 'wait_for', timeout: '10m'
  });
};
Db.deleteView = async (id) => {
  return await internals.usersClient7.delete({
    index: `${internals.usersPrefix}views`, id, refresh: true
  });
};
Db.setView = async (id, doc) => {
  return await internals.usersClient7.index({
    index: `${internals.usersPrefix}views`, body: doc, id, refresh: true, timeout: '10m'
  });
};
Db.getView = async (id) => {
  return internals.usersClient7.get({ index: `${internals.usersPrefix}views`, id });
};

Db.arkimeNodeStats = async (nodeName, cb) => {
  try {
    const { body: stat } = await Db.get('stats', 'stat', nodeName);

    stat._source._timeStamp = Date.now();
    internals.arkimeNodeStatsCache.set(nodeName, stat._source);

    cb(null, stat._source);
  } catch (err) {
    if (internals.arkimeNodeStatsCache.has(nodeName)) {
      return cb(null, internals.arkimeNodeStatsCache.get(nodeName));
    }
    return cb(err || 'Unknown node ' + nodeName);
  }
};

Db.arkimeNodeStatsCache = function (nodeName, cb) {
  const stat = internals.arkimeNodeStatsCache.get(nodeName);
  if (stat && stat._timeStamp > Date.now() - 30000) {
    return cb(null, stat);
  }

  return Db.arkimeNodeStats(nodeName, cb);
};

Db.healthCache = async (cluster) => {
  const key = `health-${cluster}`;
  const value = cache.get(key);

  if (value !== undefined) {
    return value;
  }

  const health = await Db.health(cluster);
  const { body: doc } = await internals.client7.indices.getTemplate({
    name: fixIndex('sessions3_template'),
    filter_path: '**._meta',
    cluster
  });
  if (cluster === undefined) {
    health.molochDbVersion = doc[fixIndex('sessions3_template')].mappings._meta.molochDbVersion;
  }
  cache.set(key, health);
  return health;
};

Db.nodesInfoCache = async (cluster) => {
  const key = `nodesInfoCache-${cluster}`;
  const value = cache.get(key);

  if (value !== undefined) {
    return value;
  }

  const { body: data } = await Db.nodesInfo({ cluster });
  cache.set(key, data);
  return data;
};

Db.masterCache = async (cluster) => {
  const key = `master-${cluster}`;
  const value = cache.get(key);

  if (value !== undefined) {
    return value;
  }

  const { body: data } = await Db.master();
  cache.set(key, data);
  return data;
};

Db.nodesStatsCache = async (cluster) => {
  const key = `nodesStats-${cluster}`;
  const value = cache.get(key);

  if (value !== undefined) {
    return value;
  }

  const { body: data } = await Db.nodesStats({
    metric: 'jvm,process,fs,os,indices,thread_pool',
    cluster
  });
  cache.set(key, data);
  return data;
};

Db.indicesCache = async (cluster) => {
  const key = `indices-${cluster}`;
  const value = cache.get(key);

  if (value !== undefined) {
    return value;
  }

  const { body: indices } = await Db.indices('_all', cluster);
  cache.set(key, indices);
  return indices;
};

Db.indicesSettingsCache = async (cluster) => {
  const key = `indicesSettings-${cluster}`;
  const value = cache.get(key);

  if (value !== undefined) {
    return value;
  }

  const { body: indicesSettings } = await Db.indicesSettings('_all', cluster);
  cache.set(key, indicesSettings);
  return indicesSettings;
};

Db.hostnameToNodeids = function (hostname, cb) {
  const query = { query: { match: { hostname } } };
  Db.search('stats', 'stat', query, (err, sdata) => {
    const nodes = [];
    if (sdata && sdata.hits && sdata.hits.hits) {
      for (let i = 0, ilen = sdata.hits.hits.length; i < ilen; i++) {
        nodes.push(sdata.hits.hits[i]._id);
      }
    }
    cb(nodes);
  });
};

Db.fileIdToFile = async (node, num, cb) => {
  const key = node + '!' + num;
  const info = internals.fileId2File.get(key);
  if (info !== undefined) {
    if (cb) {
      return setImmediate(() => { cb(info); });
    }
    return info;
  }

  let file = null;
  try {
    const { body: fresult } = await Db.get('files', 'file', node + '-' + num);
    file = fresult._source;
    internals.fileId2File.set(key, file);
    internals.fileName2File.set(file.name, file);
  } catch (err) { // Cache file is unknown
    internals.fileId2File.delete(key);
  }
  return cb ? cb(file) : file;
};

Db.fileNameToFiles = function (fileName, cb) {
  let query;
  if (fileName[0] === '/' && fileName[fileName.length - 1] === '/') {
    query = { query: { regexp: { name: fileName.substring(1, fileName.length - 1) } }, sort: [{ num: { order: 'desc' } }] };
  } else if (fileName.indexOf('*') !== -1) {
    query = { query: { wildcard: { name: fileName } }, sort: [{ num: { order: 'desc' } }] };
  }

  // Not wildcard/regex check the cache
  if (!query) {
    if (internals.fileName2File.has(fileName)) {
      return cb([internals.fileName2File.get(fileName)]);
    }
    query = { size: 100, query: { term: { name: fileName } }, sort: [{ num: { order: 'desc' } }] };
  }

  Db.search('files', 'file', query, (err, data) => {
    const files = [];
    if (err || !data.hits) {
      return cb(null);
    }
    data.hits.hits.forEach((hit) => {
      const file = hit._source;
      const key = file.node + '!' + file.num;
      internals.fileId2File.set(key, file);
      internals.fileName2File.set(file.name, file);
      files.push(file);
    });
    return cb(files);
  });
};

Db.getSequenceNumber = async (sName) => {
  const { body: sinfo } = await Db.index('sequence', 'sequence', sName, {});
  return sinfo._version;
};

Db.numberOfDocuments = async (index, options) => {
  // count interface is slow for larget data sets, don't use for sessions unless multiES
  if (index !== 'sessions2-*' || internals.multiES) {
    const params = { index: fixIndex(index), ignoreUnavailable: true };
    Db.merge(params, options);
    const { body: total } = await internals.client7.count(params);
    return { count: total.count };
  }

  let count = 0;
  const str = `${internals.prefix}sessions2-`;

  const indices = await Db.indicesCache(options.cluster);

  for (let i = 0; i < indices.length; i++) {
    if (indices[i].index.includes(str)) {
      count += parseInt(indices[i]['docs.count']);
    }
  }

  return { count };
};

Db.checkVersion = async function (minVersion) {
  const match = process.versions.node.match(/^(\d+)\.(\d+)\.(\d+)/);
  const nodeVersion = parseInt(match[1], 10) * 10000 + parseInt(match[2], 10) * 100 + parseInt(match[3], 10);
  if (nodeVersion < 181500) {
    console.log(`ERROR - Need node 18 (18.15 or higher) or node 20, currently using ${process.version}`);
    process.exit(1);
  } else if (nodeVersion >= 210000) {
    console.log(`ERROR - Node version ${process.version} is not supported, please use node 18 (18.15 or higher) or node 20`);
    process.exit(1);
  }

  ['stats', 'dstats', 'sequence', 'files'].forEach(async (index) => {
    try {
      await Db.indexStats(index);
    } catch (err) {
      console.log(`ERROR - Issue with '${fixIndex(index)}' index, make sure 'db/db.pl <host:port> init' has been run.\n`, err);
      process.exit(1);
    }
  });

  ArkimeUtil.checkArkimeSchemaVersion(internals.client7, internals.prefix, minVersion);
};

Db.isLocalView = function (node, yesCB, noCB) {
  if (node === internals.nodeName) {
    if (internals.debug > 1) {
      console.log(`DEBUG: node:${node} is local view because equals ${internals.nodeName}`);
    }
    return yesCB();
  }

  Db.arkimeNodeStatsCache(node, (err, stat) => {
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

Db.deleteFile = function (node, id, path, cb) {
  fs.unlink(path, () => {
    Db.deleteDocument('files', 'file', id);
    cb();
  });
};

Db.session2Sid = function (item) {
  // ver can be 2@ (sessions2), 3@ (sessions3), or x@ (user-specified queryExtraIndices)
  const ver = internals.queryExtraIndicesRegex.some(re => re.test(item._index)) ? 'x@' : item._index.includes('sessions2') ? '2@' : '3@';
  if (ver === 'x@') {
    // document from queryExtraIndices, format Sid as x@_index:_id
    return ver + item._index + ':' + item._id;
  } else if (item._id.length < 31) {
    // sessions2 didn't have new arkime_ prefix
    if (ver === '2@' && internals.prefix === 'arkime_') {
      return ver + item._index.substring(10) + ':' + item._id;
    } else {
      return ver + item._index.substring(internals.prefix.length + 10) + ':' + item._id;
    }
  } else {
    return ver + item._id;
  }
};

Db.sid2Id = function (id) {
  if (id[1] === '@') {
    id = id.substr(2);
  }

  const colon = id.indexOf(':');
  if (colon > 0) {
    return id.substr(colon + 1);
  }

  return id;
};

Db.sid2Index = function (id, options) {
  const colon = id.indexOf(':');

  if (id[1] === '@') {
    if (id[0] === 'x') {
      // ver is x@, which indicates user-specified queryExtraIndices,
      //   so the id will be formatted x@_index:_id
      // console.log(`Db.sid2Index: ${id.substr(2, colon - 2)}`);
      return id.substr(2, colon - 2);
    } else {
      if (colon > 0) {
        return 'sessions' + id[0] + '-' + id.substr(2, colon - 2);
      }
      return 'sessions' + id[0] + '-' + id.substr(2, id.indexOf('-') - 2);
    }
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

Db.loadFields = async () => {
  return Db.search('fields', 'field', { size: 10000 });
};

Db.getSessionIndices = function (excludeExtra) {
  if (excludeExtra) {
    return ['sessions2-*', 'sessions3-*'];
  }
  return internals.sessionIndices;
};

Db.getIndices = async (startTime, stopTime, bounding, rotateIndex, extraIndices) => {
  try {
    const aliases = await Db.getAliasesCache(internals.sessionIndices);
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
    // For hourly and month indices (and user-specified queryExtraIndices) we may search extra
    for (const iname in aliases) {
      let index = iname;
      let isQueryExtraIndex = false;
      if (index.endsWith('-shrink')) {
        index = index.substring(0, index.length - 7);
      }
      if (index.endsWith('-reindex')) {
        index = index.substring(0, index.length - 8);
      }
      if (index.startsWith('sessions2-')) { // sessions2 might not have prefix
        index = index.substring(10);
      } else if (internals.queryExtraIndicesRegex.some(re => re.test(index))) {
        // extra user-specified indexes from the queryExtraIndices don't have the prefix
        isQueryExtraIndex = true;
      } else {
        index = index.substring(internals.prefix.length + 10);
      }

      let year; let month; let day = 0; let hour = 0; let len;
      let queryExtraIndexTimeMatched = false; let queryExtraIndexTimeMatch;

      if (isQueryExtraIndex) {
        // the user-specified queryExtraIndices are less under our control, so we
        //   are going to take some regex-based best guesses to figure out if it's hourly, daily, etc.

        // daily 240311                         v year      v month        v day
        queryExtraIndexTimeMatch = iname.match(/([0-9][0-9])(0[1-9]|1[0-2])(0[1-9]|[12][0-9]|3[01])$/);
        if (queryExtraIndexTimeMatch) {
          queryExtraIndexTimeMatched = true;
          index = queryExtraIndexTimeMatch[0];
        }

        if (!queryExtraIndexTimeMatched) {
          // hourly 240311h19                     v year      v month        v day                    h  v hour
          queryExtraIndexTimeMatch = iname.match(/([0-9][0-9])(0[1-9]|1[0-2])(0[1-9]|[12][0-9]|3[01])[Hh]([01][0-9]|2[0-3])$/);
          if (queryExtraIndexTimeMatch) {
            queryExtraIndexTimeMatched = true;
            index = queryExtraIndexTimeMatch[0];
          }
        }

        if (!queryExtraIndexTimeMatched) {
          // weekly 24w10                         v year     w  v week
          queryExtraIndexTimeMatch = iname.match(/([0-9][0-9])[Ww]([0-4][0-9]|5[0-3])$/);
          if (queryExtraIndexTimeMatch) {
            queryExtraIndexTimeMatched = true;
            index = queryExtraIndexTimeMatch[0];
          }
        }

        if (!queryExtraIndexTimeMatched) {
          // monthly 24m10                        v year     w  v month
          queryExtraIndexTimeMatch = iname.match(/([0-9][0-9])[Mm](0[1-9]|1[0-2])$/);
          if (queryExtraIndexTimeMatch) {
            queryExtraIndexTimeMatched = true;
            index = queryExtraIndexTimeMatch[0];
          }
        }
      } // if (isQueryExtraIndex)

      if (!isQueryExtraIndex || queryExtraIndexTimeMatched) {
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
          // queryExtraIndices don't really have any way to specify (hourly[23468]|hourly12),
          //   so for those hourly just means "hourly" with regards to length calculation
          len = isQueryExtraIndex ? (60 * 60) : hlength;
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
      } else if (isQueryExtraIndex) {
        // this is a extra user-specified index pattetern from queryExtraIndices, and
        //   we couldn't grok it, so just query the whole thing
        indices.push(iname);
      }
    } // for (const iname in aliases)

    if (indices.length === 0) {
      return fixIndex(internals.sessionIndices);
    }

    if (internals.debug > 2) {
      console.log(`getIndices: ${indices}`);
    }
    return indices.join();
  } catch {
    return '';
  }
};

Db.getMinValue = async (index, field) => {
  const params = {
    index: fixIndex(index),
    body: { size: 0, aggs: { min: { min: { field } } } }
  };
  return internals.client7.search(params);
};

Db.getClusterDetails = async () => {
  return internals.client7.get({ index: '_cluster', id: 'details' });
};

Db.getILMPolicy = async (cluster) => {
  try {
    const data = await internals.client7.ilm.getLifecycle({
      policy: `${internals.prefix}molochsessions,${internals.prefix}molochhistory`,
      cluster
    });
    return data.body;
  } catch {
    return {};
  }
};

Db.setILMPolicy = async (ilmName, policy, cluster) => {
  console.log('name', ilmName, 'policy', policy);
  try {
    const data = await internals.client7.ilm.putLifecycle({
      policy: ilmName, body: { policy: policy.policy }, cluster
    });
    return data.body;
  } catch (err) {
    console.log('ERROR - setting ILM Policy', err);
    throw err;
  }
};

Db.getTemplate = async (templateName, cluster) => {
  return internals.client7.indices.getTemplate({ name: fixIndex(templateName), flat_settings: true, cluster });
};

Db.putTemplate = async (templateName, body, cluster) => {
  return internals.client7.indices.putTemplate({ name: fixIndex(templateName), body, cluster });
};

Db.setQueriesNode = async (node, force) => {
  const namePid = `node-${process.pid}`;

  // force is true we just rewrite the primary-viewer entry everytime
  if (force) {
    internals.client7.index({
      id: 'primary-viewer',
      index: fixIndex('queries'),
      body: { name: namePid, lastRun: Date.now(), enabled: false }
    });
    return true;
  }

  // force is false. Try and update the previous record. If our record
  // just update the time stamp. If not our record then take the record if
  // timestamp is 1 min old. Otherwise noop.

  const script = `
    if (ctx._source.name == params.name) {
      ctx._source.lastRun = ctx['_now'];
    } else if (ctx['_now'] - ctx._source.lastRun >= 60000) {
      ctx._source.lastRun = ctx['_now'];
      ctx._source.name = params.name;
    } else {
      ctx.op = "none";
    }
  `;

  const body = {
    script: {
      source: script,
      lang: 'painless',
      params: {
        name: namePid
      }
    }
  };
  try {
    const result = await internals.client7.update({
      id: 'primary-viewer',
      index: fixIndex('queries'),
      body
    });
    return result.body.result === 'updated';
  } catch (e) {
    // There was no entry to update, just try and create a new record as ourself
    try {
      const result = await internals.client7.index({
        id: 'primary-viewer',
        index: fixIndex('queries'),
        body: { name: namePid, lastRun: Date.now(), enabled: false },
        op_type: 'create'
      });
      return result.body.result === 'created';
    } catch (e2) {
      return false;
    }
  }
};

Db.getQueriesNode = async () => {
  try {
    const { body: doc } = await internals.client7.get({
      id: 'primary-viewer',
      index: fixIndex('queries')
    });
    return {
      node: doc._source?.name,
      updateTime: doc._source?.lastRun
    };
  } catch (e) {
    const { body: doc } = await internals.client7.indices.getMapping({
      index: fixIndex('queries')
    });
    // Since queries is an alias we don't know the real index name here
    const meta = doc[Object.keys(doc)[0]].mappings._meta;

    return {
      node: meta?.node,
      updateTime: meta?.updateTime
    };
  }
};

Db.getQuery = async (id) => {
  return internals.client7.get({ index: fixIndex('queries'), id });
};
