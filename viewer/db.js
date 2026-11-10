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
const util = require('util');
const async = require('async');
const { Client } = require('@elastic/elasticsearch');
const User = require('../common/user');
const ArkimeUtil = require('../common/arkimeUtil');
const ArkimeConfig = require('../common/arkimeConfig');
const { LRUCache } = require('lru-cache');
const DbESImpl = require('./db.es');
const DbUsersImpl = require('./db.users');
const DbClusterImpl = require('./db.cluster');
const ArkimeDb = require('../common/arkimeDb');
const DbCHImpl = require('./db.ch');
const EsQuery = require('./esQuery');
const { tokenFields, addAllTokens } = require('./sessionTokens');
const Config = require('./config.js');
DbCHImpl.register();

const cache10 = new LRUCache({ max: 1000, ttl: 1000 * 10 });
const cache60 = new LRUCache({ max: 1000, ttl: 1000 * 60 });
const esId2Info = new Map();
const esId2InfoLoadedCluster = new Map();

const Db = exports;

const internals = {
  fileId2File: new Map(),
  fileName2File: new Map(),
  shortcutsCache: new Map(),
  shortcutsCacheTS: new Map(),
  sessionIndices: ['partial-sessions3-*', 'sessions3-*'],
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
    for (const node of nodes) {
      if (!node.includes('://')) {
        console.log(`ERROR - endpoint url '${node}' must contain ://`);
        process.exit(1);
      }
    }
  } else if (!nodes.includes('://')) {
    console.log(`ERROR - endpoint url '${nodes}' must contain ://`);
    process.exit(1);
  }
}

// True if the index name (string, comma list, or array) refers to sessions
function isSessionsIndex (index) {
  if (!index) { return false; }
  if (Array.isArray(index)) { return index.some(isSessionsIndex); }
  return String(index).split(',').some((item) => {
    item = item.trim();
    if (item === 'sessions*') { return true; }
    if (item.startsWith('partial-')) { item = item.substring(8); }
    return /(^|_)sessions[23]($|[-*])/.test(item);
  });
}

Db.initialize = async (info) => {
  internals.multiES = info.multiES === 'true' || info.multiES === true || false;
  // with no OpenSearch/Elasticsearch db the viewer never talks to one
  internals.hasEs = internals.multiES || ArkimeConfig.hasEs();
  delete info.multiES;

  internals.debug = info.debug || 0;
  delete info.debug;

  internals.maxConcurrentShardRequests = info.maxConcurrentShardRequests;
  delete info.maxConcurrentShardRequests;

  internals.info = info;

  checkURLs(info.host);
  checkURLs(info.usersHost);

  internals.prefix = ArkimeUtil.formatPrefix(info.prefix);
  internals.clusterPrefix = ArkimeUtil.formatPrefix(info.clusterPrefix ?? info.prefix);
  internals.usersPrefix = ArkimeUtil.formatPrefix(info.usersPrefix ?? info.prefix);

  internals.nodeName = info.nodeName;
  delete info.nodeName;
  internals.hostName = info.hostName;
  delete info.hostName;

  internals.esProfile = info.esProfile || false;
  delete info.esProfile;

  internals.regressionTests = info.regressionTests ?? false;

  // Each connection's loopback rule comes from its own urls
  const insecure = (urls) => !!info.insecure || ArkimeConfig.isInsecure([urls]);

  const esSSLOptions = ArkimeUtil.tlsOptionsOrExit({
    insecure: insecure(info.host),
    caTrustFile: internals.info.caTrustFile,
    clientKey: info.esClientKey,
    clientCert: info.esClientCert,
    clientKeyPass: info.esClientKeyPass
  }, 'OpenSearch/Elasticsearch');

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
      insecure: insecure(info.usersHost),
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
      insecure: insecure(info.usersFallbackHost ?? info.host),
      caTrustFile: info.caTrustFile,
      requestTimeout: info.requestTimeout,
      node: info.usersFallbackHost ?? info.host,
      clientKey: info.esClientKey,
      clientCert: info.esClientCert,
      clientKeyPass: info.esClientKeyPass,
      apiKey: info.esApiKey,
      basicAuth: info.esBasicAuth,
      prefix: internals.multiES ? internals.prefix : internals.usersPrefix,
      readOnly: internals.multiES,
      getCurrentUserCB: info.getCurrentUserCB,
      noUsersCheck: info.noUsersCheck
    });
  }

  // views, shareables and shortcuts live in the users db, whatever backend it is
  internals.usersImpl = new DbUsersImpl(User.dbConnect(internals.usersPrefix));
  await internals.usersImpl.init();

  // Initialize the sessions DB backend. sessionsImpl serves sessions indices, the one the sessionsDbUrl
  // scheme registered or else OpenSearch/Elasticsearch; esImpl always exists and serves everything else.
  internals.esImpl = new DbESImpl({ Db, internals, fixIndex });

  const sessionsDbUrl = info.sessionsDbUrl || ArkimeConfig.sessionsDbUrls().join(',');
  const SessionsImpl = internals.multiES ? undefined : ArkimeDb.sessionsImpl(sessionsDbUrl);
  if (!internals.multiES && !SessionsImpl && !ArkimeConfig.isEsUrl(sessionsDbUrl)) {
    console.log(`ERROR - the sessions db can't be '${sessionsDbUrl.split(':')[0]}', set sessionsDbUrl to OpenSearch/Elasticsearch, ClickHouse or a dbPlugins sessions scheme`);
    process.exit(1);
  }
  if (SessionsImpl) {
    internals.sessionsImpl = new SessionsImpl({
      ...info, sessionsDbUrl, prefix: internals.prefix, caTrustFile: internals.info.caTrustFile, insecure: insecure(sessionsDbUrl), fieldsMap: () => Config.getDBFieldsMap(), EsQuery
    });
  } else {
    internals.sessionsImpl = internals.esImpl;
  }

  // Replace tag implementation
  if (internals.multiES) {
    Db.isLocalView = (node) => { return false; };
    internals.prefix = 'MULTIPREFIX_';
    internals.clusterPrefix = 'MULTIPREFIX_';
  }

  if (internals.debug) {
    console.log(`prefix:${internals.prefix} usersPrefix:${internals.usersPrefix}`);
  }

  // build regular expressions for the user-specified extra query index patterns
  if (Array.isArray(info.queryExtraIndices)) {
    internals.sessionIndices = [...new Set([...['partial-sessions3-*', 'sessions3-*'], ...info.queryExtraIndices])];
    delete internals.aliasesCache;
    for (const pattern of info.queryExtraIndices) {
      internals.queryExtraIndicesRegex.push(ArkimeUtil.wildcardToRegexp(pattern));
    }
    if (internals.debug > 2) {
      console.log(`defaultIndexPatterns: ${internals.sessionIndices}`);
    }
  }

  // Update aliases cache so -shrink/-reindex works
  if (internals.nodeName !== undefined && internals.hasEs) {
    Db.getAliasesCache();
    setInterval(() => { Db.getAliasesCache(); }, 2 * 60 * 1000);
  }

  internals.localShortcutsIndex = fixIndex('lookups');
  if (internals.info.usersHost && internals.usersPrefix !== undefined) {
    internals.remoteShortcutsIndex = `${internals.usersPrefix}lookups`;
  } else { // there is no remote shortcuts index, just set it to local
    internals.remoteShortcutsIndex = internals.localShortcutsIndex;
  }

  if (internals.debug > 1) {
    console.log(`remoteShortcutsIndex: ${internals.remoteShortcutsIndex} localShortcutsIndex: ${internals.localShortcutsIndex}`);
  }

  // What capture writes plus hunts, queries, history and featherprint live in the cluster db, dbUrl, which shares
  // the sessions ES connection when it is the same ES. Another ES uses the sessions ES credentials and needs db.pl init too.
  // Every backend names its tables with the cluster's prefix, like the ES indices, so clusters can share one db.
  // A multi viewer gets every cluster's cluster db through multies, see multiESNodes dbUrl
  let clusterDbUrl;
  if (!internals.multiES) {
    const urls = ArkimeConfig.dbUrls();
    const norm = (list) => [list].flat().map(u => (u.includes('://') ? u : 'http://' + u).replace(/\/+$/, '')).join(',');
    if (!ArkimeConfig.isEsUrl(urls[0]) || norm(urls) !== norm(internals.info.host)) {
      clusterDbUrl = urls.join(',');
    }
  }
  // capture has to write to it too, from every sensor
  if (clusterDbUrl && !ArkimeUtil.dbHoldsCluster(clusterDbUrl)) {
    console.log(`ERROR - dbUrl '${clusterDbUrl.split(':')[0]}' can't hold the cluster db, use Elasticsearch/OpenSearch, postgres:// or a dbPlugins cluster scheme, or set sessionsDbUrl and dbUrl`);
    process.exit(1);
  }
  const esConn = ArkimeUtil.dbConnect(undefined, { client: internals.client7, multiES: internals.multiES });
  // Each backend takes the options it understands
  const clusterConn = !clusterDbUrl
    ? esConn
    : ArkimeUtil.dbConnect(clusterDbUrl, {
      prefix: internals.clusterPrefix,
      insecure: insecure(clusterDbUrl),
      caTrustFile: info.caTrustFile,
      requestTimeout: info.requestTimeout,
      clientKey: info.esClientKey,
      clientCert: info.esClientCert,
      clientKeyPass: info.esClientKeyPass,
      apiKey: info.esApiKey,
      basicAuth: info.esBasicAuth
    });
  internals.clusterShared = clusterConn === esConn;
  internals.clusterConn = clusterConn;
  // The flush and refresh admin calls also reach a separate OpenSearch/Elasticsearch cluster db
  internals.clusterEsClient = !clusterDbUrl || ArkimeDb.isES(clusterDbUrl) ? clusterConn.client : undefined;
  internals.clusterImpl = new DbClusterImpl({
    conn: clusterConn,
    esConn,
    prefix: internals.clusterPrefix,
    fixIndex,
    multiES: internals.multiES
  });
  await internals.clusterImpl.init();

  // multies checks each cluster's version itself
  if (!internals.hasEs || internals.multiES) { return; }
  try {
    const { body: data } = await internals.client7.info();
    if (data.version.distribution === 'opensearch') {
      if (data.version.number.match(/^[0]/)) {
        console.log(`ERROR - OpenSearch ${data.version.number} not supported, OpenSearch 1.0.0 or later required.`);
        process.exit(1);
      }
    } else {
      if (data.version.number.match(/^([0-6]|7\.[0-9]\.)/)) {
        console.log(`ERROR - Elasticsearch ${data.version.number} not supported, Elasticsearch 7.10.0 or later required.`);
        process.exit(1);
      }
    }
  } catch (err) {
    ArkimeUtil.searchErrorMsg(err, internals.info.host, 'Getting OpenSearch/Elasticsearch info failed');
    process.exit(1);
  }
};

/// ///////////////////////////////////////////////////////////////////////////////
/// / Low level functions to convert from no prefix to having a prefix
/// ///////////////////////////////////////////////////////////////////////////////
const CLUSTER_INDEX_RE = new RegExp(`^(${DbClusterImpl.INDEX_NAMES.join('|')})(?![a-z])`);

function fixIndex (index) {
  if (index === undefined || index === '_all') { return index; }

  if (index === 'sessions*') {
    return `${internals.prefix}sessions3*`;
  }

  // Turn into array if comma separated
  if (index.includes(',')) {
    index = index.split(',');
  }

  if (Array.isArray(index)) {
    return index.map(val => fixIndex(val)).join(',');
  }

  // Don't fix extra user-specified indexes from the queryExtraIndices
  if (internals.queryExtraIndicesRegex.some(re => re.test(index))) {
    return index;
  }

  // Make sure partial-${prefix}index format
  if (index.startsWith('partial-')) {
    if (!index.substring(8).startsWith(internals.prefix)) {
      index = 'partial-' + internals.prefix + index.substring(8);
    }
    return index;
  }

  // The cluster db indices can have their own prefix, a name with the sessions one is moved to it
  if (internals.clusterPrefix && index.startsWith(internals.clusterPrefix) && CLUSTER_INDEX_RE.test(index.substring(internals.clusterPrefix.length))) {
    return index;
  }
  const bare = internals.prefix && index.startsWith(internals.prefix) ? index.substring(internals.prefix.length) : index;
  if (CLUSTER_INDEX_RE.test(bare)) {
    return internals.clusterPrefix + bare;
  }

  // If prefix isn't there, add it
  if (!index.startsWith(internals.prefix)) {
    index = internals.prefix + index;
  }

  if (internals.aliasesCache && !internals.aliasesCache[index]) {
    if (internals.aliasesCache['partial-' + index]) {
      index = 'partial-' + index;
    } else if (internals.aliasesCache[index + '-shrink']) {
      // If the index doesn't exist but the shrink version does exist, add -shrink
      index += '-shrink';
    } else if (internals.aliasesCache[index + '-reindex']) {
      // If the index doesn't exist but the reindex version does exist, add -reindex
      index += '-reindex';
    }
  }

  return index;
}
Db.fixIndex = fixIndex;

Db.merge = (to, from) => {
  for (const key in from) {
    if (Object.prototype.hasOwnProperty.call(from, key)) {
      to[key] = from[key];
    }
  }
};

Db.get = async (index, id) => {
  return internals.client7.get({ index: fixIndex(index), id });
};

Db.getWithOptions = async (index, id, options) => {
  const params = { index: fixIndex(index), id };
  Db.merge(params, options);
  return internals.client7.get(params);
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
  '@timestamp': true,
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
  '@timestamp': true,
  'cert.notBefore': true,
  'cert.notAfter': true
};

// Change foo.bar to foo: {bar:}
// Unarray singleton fields
// Change string dates to MS
function fixSessionFields (fields, unflatten) {
  if (!fields) { return; }
  if (unflatten) {
    fields.source = Object.create(null);
    fields.source.as = Object.create(null);
    fields.source.geo = Object.create(null);
    fields.destination = Object.create(null);
    fields.destination.as = Object.create(null);
    fields.destination.geo = Object.create(null);
    fields.client = Object.create(null);
    fields.server = Object.create(null);
  }
  for (const f in fields) {
    const path = f.split('.');
    if (ArkimeUtil.isPP(path)) { continue; }
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
        key[path[i]] = Object.create(null);
      }
      key = key[path[i]];
    }
  }
}

async function fixPacketPos (fields) {
  if (!fields.packetPos || fields.packetPos.length === 0) {
    return;
  }

  try {
    const fileInfo = await Db.fileIdToFile(fields.node, -1 * fields.packetPos[0]);
    if (internals.debug > 2) {
      console.log('GETSESSION - fixPacketPos', fileInfo);
    }

    if (!fileInfo || !fileInfo.packetPosEncoding) {
      return;
    }

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
      return;
    } else if (fileInfo.packetPosEncoding === 'localIndex') {
      // Neg numbers aren't encoded, use var length encoding, if pos is 0 same gap as last gap, otherwise last + pos
      if (await Db.isLocalView(fields.node)) {
        const newPacketPos = [];
        await async.forEachOfSeries(fields.packetPos, async (item, key) => {
          if (key % 3 !== 0) { return; } // Only look at every 3rd item

          let fd;
          try {
            const idToFileInfo = await Db.fileIdToFile(fields.node, -1 * item);
            fd = fs.openSync(idToFileInfo.indexFilename, 'r');
            if (!fd) { return; }
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
              if (x & 0x80) { // eslint-disable-line no-bitwise
                num = num + (x & 0x7f) * mult; // eslint-disable-line no-bitwise
                if (num !== 0) {
                  lastgap = num;
                }
                last += lastgap;
                newPacketPos.push(last);
                num = 0;
                mult = 1;
              } else {
                num = num + x * mult;
                mult *= 128; // Javascript can't shift large numbers, so mult
              }
            }
          } catch (e) {
            console.log(e);
          } finally {
            if (fd) {
              try {
                fs.closeSync(fd);
              } catch (closeErr) {
                console.log('Error closing file:', closeErr);
              }
            }
          }
          return;
        });
        fields.packetPos = newPacketPos;
        return;
      } else {
        delete fields.packetPos; // can't decode remote
        return;
      }
    } else {
      console.log('Unknown packetPosEncoding', fileInfo);
      return;
    }
  } catch (err) {
    return;
  }
}

async function fixFileIds (fields) {
  if (!fields || !fields.node) { return; }

  try {
    const stat = await Db.arkimeNodeStatsCache(fields.node);
    fields.nodehost = stat.hostname;
  } catch (err) {
    // Ignore error
  }

  if (!fields.fileId) {
    fields.fileId = [];
    return;
  }

  const files = [];
  for (const item of fields.fileId) {
    try {
      const file = await Db.fileIdToFile(fields.node, item);
      if (file && file.locked === 1) {
        files.push(file.name);
      }
    } catch (ferr) {
      // Ignore error
    }
  }
  fields.fileId = files;
}

// Get a session from OpenSearch/Elasticsearch and decode packetPos if requested.
// esProxy's searchIdsFilter only passes this exact query shape from sensor viewers.
Db.getSession = async (id, options, cb) => {
  if (internals.debug > 2) {
    console.log('GETSESSION -', id, options);
  }

  try {
    if (!options) {
      options = { _source: ['cert', 'dns', 'zeekintel'], fields: ['*'] };
    }
    const query = { query: { ids: { values: [Db.sid2Id(id)] } }, _source: options._source, fields: options.fields };

    const unflatten = options?.arkime_unflatten ?? true;
    const params = Object.create(null);
    Db.merge(params, options);
    delete params._source;
    delete params.fields;
    delete params.arkime_unflatten;
    delete params.final;

    const index = Db.sid2Index(id);

    let results = await internals.sessionsImpl.searchSessions(index, query, params);
    if (internals.debug > 2) {
      console.log('GETSESSION - search results', JSON.stringify(results, false, 2));
    }
    if (!results.hits || !results.hits.hits || results.hits.hits.length === 0) {
      if (options.final === true) {
        delete options.final;
        return cb ? cb('Not found') : Promise.reject('Not found');
      }
      options.final = true;
      if (!await internals.sessionsImpl.refreshForNotFoundRetry(index)) {
        return cb ? cb('Not found') : Promise.reject('Not found');
      }
      return Db.getSession(id, options, cb);
    }
    const session = results.hits.hits[0];
    session.found = true;
    if (session.fields && session._source && session._source.cert) {
      session.fields.cert = session._source.cert;
    }
    if (session.fields && session._source && session._source.dns) {
      session.fields.dns = session._source.dns;
    }
    if (session.fields && session._source && session._source.zeekintel) {
      session.fields.zeekintel = session._source.zeekintel;
    }
    delete session._source;
    fixSessionFields(session.fields, unflatten);
    if (session.fields.packetPos !== undefined) {
      await fixPacketPos(session.fields);
    }
    await fixFileIds(session.fields ?? session._source);
    return cb ? cb(null, session) : session;
  } catch (err) {
    return cb ? cb(err) : Promise.reject(err);
  }
};

Db.index = async (index, id, doc) => {
  return internals.client7.index({ index: fixIndex(index), body: doc, id });
};

// A sessions db that isn't OpenSearch/Elasticsearch writes the session like capture would, *Tokens included
Db.indexNow = async (index, id, doc) => {
  if (internals.sessionsImpl.writeSessions && /sessions[23]-/.test(index)) {
    const session = { ...doc, index: fixIndex(index), _id: id };
    addAllTokens(session, tokenFields(Object.keys(Config.getDBFieldsMap() ?? {})));
    return internals.sessionsImpl.writeSessions([session]);
  }
  return internals.client7.index({
    index: fixIndex(index), body: doc, id, refresh: true
  });
};

// Anything added to the body here must also be allowed by esProxy's sessions and files search validators
Db.searchBody = (query) => {
  if (internals.esProfile) { query.profile = true; }
  return query;
};

Db.search = async (index, query, options) => {
  const params = {
    index: fixIndex(index),
    body: Db.searchBody(query),
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

    return results;
  } catch (err) {
    const esError = err?.meta?.body?.error;
    if (esError?.type === 'index_not_found_exception') {
      console.log(`OpenSearch/Elasticsearch Search Error - index not found: ${esError.index ?? params.index}`);
    } else if (esError && internals.debug < 2) {
      console.log(`OpenSearch/Elasticsearch Search Error - ${esError.type}: ${esError.root_cause?.[0]?.reason ?? esError.reason}`);
    } else {
      console.trace(`OpenSearch/Elasticsearch Search Error - query: ${JSON.stringify(params, false, 2)} err:`, util.inspect(err, null, 20));
    }
    throw err;
  }
};

// Whether the sessions db cancels a search itself, without OpenSearch/Elasticsearch tasks
Db.cancelsSearches = () => typeof internals.sessionsImpl?.cancelByOpaqueId === 'function';

Db.cancelByOpaqueId = async (cancelId, cluster) => {
  // Searches run on the sessions db, a non ES one cancels them itself
  if (Db.cancelsSearches()) {
    if (!await internals.sessionsImpl.cancelByOpaqueId(cancelId)) {
      throw new Error('Cancel ID not found, cannot cancel the query');
    }
    return { body: 'Query cancelled successfully' };
  }

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

  return 'OpenSearch/Elasticsearch task cancelled successfully';
};

// The dbs whose running queries the Stats page can show and kill, not through the multi viewer
const ADMIN_DBS = ['sessions', 'cluster', 'users'];
Db.adminDb = (db) => {
  if (internals.multiES) { return undefined; }
  const impl = db === 'sessions' ? internals.sessionsImpl : db === 'cluster' ? internals.clusterConn : db === 'users' ? User.dbConnect() : undefined;
  return typeof impl?.adminQueries === 'function' ? impl : undefined;
};

// One entry per db server, a cluster and users db on the same one is just the cluster. The tab is the
// name camel cased, PG Users -> pgUsersQueries
Db.adminDbs = () => {
  const list = [];
  for (const db of ADMIN_DBS) {
    const impl = Db.adminDb(db);
    if (!impl || list.some(e => (e.impl.adminKey ?? e.impl) === (impl.adminKey ?? impl))) { continue; }
    list.push({ db, impl });
  }
  return list.map(({ db, impl }) => {
    const first = list.find(e => e.impl.adminName === impl.adminName).db === db;
    const label = first ? impl.adminName : `${impl.adminName} ${db[0].toUpperCase()}${db.slice(1)}`;
    const tab = label.split(' ').map((w, i) => i === 0 ? w.toLowerCase() : w[0].toUpperCase() + w.slice(1).toLowerCase()).join('') + 'Queries';
    const tablesName = impl.adminTablesName;
    return {
      db,
      name: label,
      tab,
      fields: impl.adminFields,
      tablesTab: tablesName ? tab.replace(/Queries$/, tablesName) : undefined,
      tablesName,
      tableFields: impl.adminTableFields,
      tableActions: impl.adminTableActions
    };
  });
};

Db.searchScroll = async (index, query, options, cb) => {
  const impl = isSessionsIndex(index) ? internals.sessionsImpl : internals.esImpl;
  return impl.searchScroll(index, query, options, cb);
};

Db.searchScrollIterator = async function* (index, query, options) {
  const impl = isSessionsIndex(index) ? internals.sessionsImpl : internals.esImpl;
  yield * impl.searchScrollIterator(index, query, options);
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
  Db.searchScroll(index, query, params, async (err, result) => {
    if (err || result.hits.hits.length === 0) { return cb(err, result); }

    for (const hit of result.hits.hits) {
      fixSessionFields(hit.fields, unflatten);
      if (hit.fields?.packetPos !== undefined) {
        await fixPacketPos(hit.fields);
      }
      await fixFileIds(hit.fields);
    }
    return cb(null, result);
  });
};

Db.searchSessionsIterator = async function* (index, query, options) {
  if (!options) { options = {}; }
  const unflatten = options.arkime_unflatten ?? true;
  const params = { preference: 'primaries', ignore_unavailable: 'true' };
  if (internals.maxConcurrentShardRequests) { params.maxConcurrentShardRequests = internals.maxConcurrentShardRequests; }
  Db.merge(params, options);
  delete params.arkime_unflatten;

  for await (const chunk of Db.searchScrollIterator(index, query, params)) {
    for (const hit of chunk.hits.hits) {
      fixSessionFields(hit.fields, unflatten);
      if (hit.fields?.packetPos !== undefined) {
        await fixPacketPos(hit.fields);
      }
      await fixFileIds(hit.fields);
    }
    yield chunk;
  }
};

Db.msearchSessions = async (index, queries, options) => {
  return internals.sessionsImpl.msearchSessions(index, queries, options);
};

// A sessions db with its own scrolls says which scroll ids are its
Db.scroll = async (params) => {
  const scrollId = params?.body?.scroll_id;
  if (scrollId === EsQuery.SCROLL_DONE) {
    return { body: { _scroll_id: EsQuery.SCROLL_DONE, hits: { total: 0, hits: [] } } };
  }
  if (internals.sessionsImpl?.ownsScroll?.(scrollId)) {
    return { body: await internals.sessionsImpl.scroll(scrollId) };
  }
  params.rest_total_hits_as_int = true;
  return internals.client7.scroll(params);
};

// Ignore errors when we try to clear a scroll that doesn't exist
Db.clearScroll = async (params) => {
  const scrollId = params?.body?.scroll_id;
  if (scrollId === EsQuery.SCROLL_DONE) { return; }
  try {
    if (internals.sessionsImpl?.ownsScroll?.(scrollId)) { return await internals.sessionsImpl.clearScroll(scrollId); }
    await internals.client7.clearScroll(params);
  } catch (err) {
  }
};

Db.deleteDocument = async (index, id, options) => {
  const impl = isSessionsIndex(index) ? internals.sessionsImpl : internals.esImpl;
  return impl.deleteDocument(index, id, options);
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

Db.getAliasesCache = async () => {
  if (internals.aliasesCache && internals.aliasesCacheTimeStamp > Date.now() - 5000) {
    return internals.aliasesCache;
  }

  const index = internals.sessionIndices;

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
    flat_settings: true,
    index: fixIndex(index),
    cluster
  });
};

Db.setIndexSettings = async (index, options) => {
  // Users might be on a different cluster
  // The ES index settings page also reaches a users db on its own OpenSearch/Elasticsearch
  const usersClient = User.dbConnect()?.client;
  if ((index === '*' || index.includes('users')) && internals.info.usersHost && usersClient) {
    try {
      await usersClient.indices.putSettings({
        index,
        body: options.body,
        timeout: '10m',
        master_timeout: '10m',
        cluster: options.cluster
      });
    } catch (err) {
      console.log('ERROR - updating users index settings', JSON.stringify(err, null, 2));
    }
  }

  try {
    const { body: response } = await internals.client7.indices.putSettings({
      index,
      body: options.body,
      timeout: '10m',
      master_timeout: '10m',
      cluster: options.cluster
    });
    return response;
  } catch (err) {
    console.log('ERROR - index settings', JSON.stringify(err, null, 2));
    cache10.clear();
    cache60.clear();
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
    h: 'index,shard,prirep,state,docs,store,ip,node,ur,uf,fm,sm,ud',
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

Db.getClusterSettingsCache = async (options) => {
  const key = 'clusterSettings-' + JSON.stringify(options);
  let value = cache60.get(key);
  if (value) {
    return value;
  }
  value = await internals.client7.cluster.getSettings(options);
  cache60.set(key, value);
  return value;
};

Db.putClusterSettings = async (options) => {
  cache60.keys().filter((v) => v.startsWith('clusterSettings-')).forEach((v) => cache60.delete(v));
  options.timeout = '10m';
  options.master_timeout = '10m';
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

Db.update = async (index, id, doc, options) => {
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

Db.updateSession = async (index, id, doc) => {
  return internals.sessionsImpl.updateSession(index, id, doc);
};

Db.close = async () => {
  User.close();
  internals.sessionsImpl.close();
  await ArkimeUtil.dbCloseAll();
  return internals.client7.close();
};

Db.reroute = async (cluster, commands) => {
  return internals.client7.cluster.reroute({
    timeout: '10m',
    master_timeout: '10m',
    retry_failed: true,
    cluster,
    body: { commands }
  });
};

Db.allocationExplain = async (cluster, index, shard, primary) => {
  const params = { cluster };

  // If specific shard info is provided, include it in the request body
  if (index != null && shard != null && primary != null) {
    params.body = {
      index,
      shard: parseInt(shard),
      primary: primary === 'true' || primary === true
    };
  }

  return internals.client7.cluster.allocationExplain(params);
};

// A flush or refresh of everything, or of a cluster db index, also has to reach a separate cluster db ES.
// With no OpenSearch/Elasticsearch there is nothing to do, the other dbs' writes are already visible.
function clusterDbIndices (op, index, cluster) {
  if (!internals.hasEs) { return; }
  const separate = internals.clusterEsClient && internals.clusterEsClient !== internals.client7;
  if (index !== undefined && index !== '*') {
    if (separate && CLUSTER_INDEX_RE.test(index)) {
      return internals.clusterEsClient.indices[op]({ index: fixIndex(index) });
    }
    return internals.client7.indices[op]({ index: fixIndex(index), cluster });
  }
  const sessions = internals.client7.indices[op]({ index: fixIndex(index), cluster });
  // No index is every index on the sessions ES, * is just the sessions prefix
  if (separate) {
    return Promise.all([sessions, internals.clusterEsClient.indices[op]({ index: `${internals.clusterPrefix}*` })]);
  }
  if (index === '*' && internals.clusterPrefix !== internals.prefix) {
    return Promise.all([sessions, internals.client7.indices[op]({ index: `${internals.clusterPrefix}*`, cluster })]);
  }
  return sessions;
}

Db.flush = async (index, cluster) => {
  if (index === 'users') {
    return User.flush(cluster);
  } else if (index === 'lookups' || index === 'views' || index === 'shareables') {
    return internals.usersImpl.flush(index, cluster);
  } else {
    return clusterDbIndices('flush', index, cluster);
  }
};

Db.refresh = async (index, cluster) => {
  if (index === 'users') {
    return User.flush(cluster);
  } else if (index === 'lookups' || index === 'views' || index === 'shareables') {
    return internals.usersImpl.refresh(index, cluster);
  } else {
    return clusterDbIndices('refresh', index, cluster);
  }
};

Db.addTagsToSession = async (index, id, tags, cluster) => {
  return internals.sessionsImpl.addTagsToSession(index, id, tags, cluster);
};

Db.removeTagsFromSession = async (index, id, tags, cluster) => {
  return internals.sessionsImpl.removeTagsFromSession(index, id, tags, cluster);
};

Db.addHuntToSession = async (index, id, huntId, huntName) => {
  return internals.sessionsImpl.addHuntToSession(index, id, huntId, huntName);
};

Db.removeHuntFromSession = async (index, id, huntId, huntName) => {
  return internals.sessionsImpl.removeHuntFromSession(index, id, huntId, huntName);
};

/// ///////////////////////////////////////////////////////////////////////////////
/// / High level functions
/// ///////////////////////////////////////////////////////////////////////////////
Db.flushCache = function () {
  internals.fileId2File.clear();
  internals.fileName2File.clear();
  User.flushCache();
  internals.shortcutsCache.clear();
  delete internals.aliasesCache;
  Db.getAliasesCache();
  cache10.clear();
  cache60.clear();
};

// History DB interactions
Db.historyIt = async (doc, cluster) => {
  return internals.clusterImpl.addHistory(doc, cluster);
};
// params are collection find params, returns { total, data: [{ id, doc, index }] }
Db.searchHistory = async (params, cluster) => {
  return internals.clusterImpl.searchHistory(params, cluster);
};
Db.countHistory = async (cluster) => {
  return internals.clusterImpl.countHistory(cluster);
};
Db.deleteHistory = async (id, cluster) => {
  return internals.clusterImpl.deleteHistory(id, cluster);
};
Db.deleteAllHistory = async () => {
  return internals.clusterImpl.deleteAllHistory();
};

// Hunt DB interactions
// { devices, history, alerts } collections
Db.featherprint = () => internals.clusterImpl.featherprint;

Db.createHunt = async (doc, cluster) => {
  return internals.clusterImpl.createHunt(doc, cluster);
};
// params are collection find params, returns { total, data: [{ id, doc, index }] }
Db.searchHunts = async (params, cluster) => {
  return internals.clusterImpl.searchHunts(params, cluster);
};
Db.countHunts = async (where, cluster) => {
  return internals.clusterImpl.countHunts(where, cluster);
};
Db.deleteHunt = async (id, cluster) => {
  return internals.clusterImpl.deleteHunt(id, cluster);
};
Db.updateHunt = async (id, fields, cluster) => {
  return internals.clusterImpl.updateHunt(id, fields, cluster);
};
// returns the hunt or null
Db.getHunt = async (id, cluster) => {
  return internals.clusterImpl.getHunt(id, cluster);
};

// fetches the version of the shortcuts from the users backend
async function getShortcutsVersion () {
  return internals.usersImpl.getShortcutsVersion();
}
// increments the shortcuts version so local ES nodes know to sync
async function setShortcutsVersion () {
  return internals.usersImpl.setShortcutsVersion();
}
// updates the shortcuts in the local ES db if they are out of sync with the users backend
// the users backend (ES or SQLite) is the source of truth, local ES nodes are replicas for session queries
Db.updateLocalShortcuts = async () => {
  if (!internals.info.usersHost ||
     !internals.info.isPrimaryViewer || // If no isPrimaryViewer then we aren't actually viewer, don't do this
    !internals.info.isPrimaryViewer() ||
    internals.info.host === internals.info.usersHost) {
    return;
  }

  if (internals.multiES) { return; } // don't sync shortcuts for multies
  if (!internals.usersImpl) { return; } // no users backend configured
  if (internals.sessionsImpl !== internals.esImpl) { return; } // only ES sessions queries read the local copy

  const msg = `updating local shortcuts (${internals.info.host}/${internals.localShortcutsIndex}) from users backend`;

  try {
    // fetch the version of the users backend shortcuts to check if the local shortcuts index
    // is up to date. if not, something has changed and we need to sync
    const version = await getShortcutsVersion();

    if (version === internals.localShortcutsVersion) { return; } // versions match, stop!

    console.log(msg);

    internals.shortcutsCache.clear(); // Clear cache when updating
    // fetch shortcuts from users backend and local ES index
    const [remoteShortcuts, { body: localResults }] = await Promise.all([
      internals.usersImpl.getAllShortcuts(),
      Db.searchShortcutsLocal({ size: 10000 })
    ]);

    // compare the local shortcuts to the remote shortcuts to determine
    // if any shortcuts have been deleted from the users backend
    for (const localShortcut of localResults.hits.hits) {
      let missing = true;
      for (const remoteShortcut of remoteShortcuts) {
        if (remoteShortcut.id === localShortcut._id) {
          missing = false;
          break;
        }
      }
      if (missing) {
        if (internals.debug > 1) {
          console.log(`SHORTCUT - deleting ${localShortcut._id} ${localShortcut._source.name} locally`);
        }
        await internals.client7.delete({
          index: internals.localShortcutsIndex,
          id: localShortcut._id,
          refresh: true
        });
      }
    }

    // compare remote shortcuts to local shortcuts to determine if any
    // shortcuts have been added or updated from the users backend
    let changed = false;
    for (const remoteShortcut of remoteShortcuts) {
      let missing = true;
      for (const localShortcut of localResults.hits.hits) {
        if (remoteShortcut.id === localShortcut._id) {
          missing = false;
          if (remoteShortcut.version !== localShortcut._version) {
            if (internals.debug > 1) {
              console.log(`SHORTCUT - update from remote ${remoteShortcut.id} ${remoteShortcut.source.name}`);
            }
            await internals.client7.index({
              id: remoteShortcut.id,
              index: internals.localShortcutsIndex,
              body: remoteShortcut.source,
              version_type: 'external',
              version: remoteShortcut.version
            });
            changed = true;
          }
        }
      }
      if (missing) {
        if (internals.debug > 1) {
          console.log(`SHORTCUT - add from remote ${remoteShortcut.id} ${remoteShortcut.source.name}`);
        }
        await internals.client7.index({
          id: remoteShortcut.id,
          index: internals.localShortcutsIndex,
          body: remoteShortcut.source,
          version_type: 'external',
          version: remoteShortcut.version
        });
        changed = true;
      }
    }

    if (changed) {
      await internals.client7.indices.refresh({ index: internals.localShortcutsIndex });
    }

    internals.localShortcutsVersion = version;
  } catch (err) {
    console.log(`ERROR - ${msg}:`, err);
  }
};
Db.searchShortcuts = async (params) => {
  return internals.usersImpl.searchShortcuts(params);
};
Db.searchShortcutsLocal = async (query) => {
  return internals.client7.search({
    index: internals.localShortcutsIndex, body: query, rest_total_hits_as_int: true, version: true
  });
};
Db.numberOfShortcuts = async (params) => {
  return internals.usersImpl.numberOfShortcuts(params);
};
Db.createShortcut = async (doc) => {
  internals.shortcutsCache.clear();
  await setShortcutsVersion();
  const id = await internals.usersImpl.createShortcut(doc);
  await Db.updateLocalShortcuts();
  return id;
};
Db.deleteShortcut = async (id) => {
  internals.shortcutsCache.clear();
  await setShortcutsVersion();
  await internals.usersImpl.deleteShortcut(id);
  await Db.updateLocalShortcuts();
};
Db.setShortcut = async (id, doc) => {
  internals.shortcutsCache.clear();
  await setShortcutsVersion();
  await internals.usersImpl.setShortcut(id, doc);
  await Db.updateLocalShortcuts();
};
Db.getShortcut = async (id) => {
  return internals.usersImpl.getShortcut(id);
};
Db.deleteAllShortcuts = async () => {
  internals.shortcutsCache.clear();
  await internals.usersImpl.deleteAllShortcuts();
  await setShortcutsVersion();
};
// The shortcuts a user can see, read from the users db, in the shape of the local ES index's hits
async function userShortcuts (userId, roles) {
  const hits = (await internals.usersImpl.getAllShortcuts())
    .filter(({ source: s }) => s.userId === userId || [].concat(s.users ?? []).includes(userId) || [].concat(s.roles ?? []).some(r => roles.includes(r)))
    .sort((a, b) => String(a.source.name).localeCompare(String(b.source.name)))
    .map(({ id, source, version }) => ({ _id: id, _source: source, _version: version }));
  return { total: hits.length, hits };
}

Db.getShortcutsCache = async (user) => {
  const cshortcuts = internals.shortcutsCache.get(user.userId);
  if (cshortcuts && internals.shortcutsCacheTS.get(user.userId) > Date.now() - 30000) {
    return cshortcuts;
  }

  const roles = [...await user.getRoles()]; // es requires an array for terms search

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
    sort: { name: { order: 'asc' } },
    size: 10000
  };

  // A multi viewer's local copy is behind multies, which only clusters with ES have, the users db is the source
  const shortcuts = internals.sessionsImpl === internals.esImpl && !internals.multiES
    ? (await Db.searchShortcutsLocal(query)).body.hits
    : await userShortcuts(user.userId, roles);

  const shortcutsMap = {};
  for (const shortcut of shortcuts.hits) {
    // need the whole object to test for type mismatch
    shortcutsMap[shortcut._source.name] = shortcut;
  }

  // Keep the ClickHouse lookups table current so terms-lookup translation
  // (IN subquery against the lookups table) sees these values. ES resolves
  // terms-lookup natively, so its impl has no syncShortcuts.
  if (internals.sessionsImpl.syncShortcuts) {
    try {
      await internals.sessionsImpl.syncShortcuts(shortcuts.hits);
    } catch (err) {
      console.log('ERROR - syncing shortcuts to ClickHouse', util.inspect(err, false, 50));
    }
  }

  internals.shortcutsCache.set(user.userId, shortcutsMap);
  internals.shortcutsCacheTS.set(user.userId, Date.now());

  return shortcutsMap;
};

Db.searchViews = async (params) => {
  return internals.usersImpl.searchViews(params);
};
Db.numberOfViews = async (params) => {
  return internals.usersImpl.numberOfViews(params);
};
Db.createView = async (doc) => {
  return internals.usersImpl.createView(doc);
};
Db.deleteView = async (id) => {
  return internals.usersImpl.deleteView(id);
};
Db.deleteAllViews = async () => {
  return internals.usersImpl.deleteAllViews();
};
Db.setView = async (id, doc) => {
  return internals.usersImpl.setView(id, doc);
};
Db.getView = async (id) => {
  return internals.usersImpl.getView(id);
};
Db.getViewByIdOrName = async (idOrName, user, roles) => {
  return internals.usersImpl.getViewByIdOrName(idOrName, user, roles);
};

Db.searchShareables = async (params) => {
  return internals.usersImpl.searchShareables(params);
};

Db.numberOfShareables = async (params) => {
  return internals.usersImpl.numberOfShareables(params);
};

Db.createShareable = async (doc) => {
  return internals.usersImpl.createShareable(doc);
};

Db.deleteShareable = async (id) => {
  return internals.usersImpl.deleteShareable(id);
};

Db.deleteAllShareables = async () => {
  return internals.usersImpl.deleteAllShareables();
};

// version, from getShareableVersioned, makes the write fail with a 409 when the
// item was changed or deleted since it was read
Db.setShareable = async (id, doc, version) => {
  return internals.usersImpl.setShareable(id, doc, version);
};

Db.getShareable = async (id) => {
  return internals.usersImpl.getShareable(id);
};

// { shareable, version }, version being opaque to callers
Db.getShareableVersioned = async (id) => {
  return internals.usersImpl.getShareableVersioned(id);
};

Db.hasEs = () => internals.hasEs;

// The db.pl schema version the cluster db matches, from the ES sessions template when it shares the sessions ES,
// else from the cluster db itself
Db.clusterSchemaVersion = async () => {
  if (internals.clusterShared && internals.clusterEsClient) { return (await Db.healthCache())?.molochDbVersion; }
  return internals.clusterConn.schemaVersion?.();
};

Db.arkimeNodeStats = async (nodeName) => {
  const stat = await internals.clusterImpl.stats.get(nodeName);
  if (!stat) { throw new Error('Unknown node'); }
  return stat;
};

Db.arkimeNodeStatsCache = async function (nodeName) {
  const key = `arkimeNodeStats-${nodeName}`;
  let stat = cache60.get(key);
  if (stat) {
    return stat;
  }

  stat = await Db.arkimeNodeStats(nodeName);
  cache60.set(key, stat);
  return stat;
};

// Without OpenSearch/Elasticsearch, health is whether each db answers, in the ES health shape Parliament reads
async function dbHealth () {
  const list = [];
  for (const [role, impl] of [['sessions', internals.sessionsImpl], ['cluster', internals.clusterConn], ['users', User.dbConnect()]]) {
    if (typeof impl?.serverVersion !== 'function' || list.some(e => (e.impl.adminKey ?? e.impl) === (impl.adminKey ?? impl))) { continue; }
    list.push({ role, impl });
  }
  let up = true;
  const versions = await Promise.all(list.map(async ({ role, impl }) => {
    try {
      return `${impl.dbName} ${await impl.serverVersion()}`;
    } catch (err) {
      up = false;
      console.log(`ERROR - ${role} db health check`, ArkimeUtil.sanitizeStr(err.message ?? err));
      return `${impl.dbName} down`;
    }
  }));
  return {
    status: up ? 'green' : 'red',
    db: [...new Set(list.map(e => e.impl.dbName))].join('+'),
    version: versions.join(' / '),
    number_of_nodes: 1,
    number_of_data_nodes: 1
  };
}

Db.healthCache = async (cluster) => {
  const key = `health-${cluster}`;
  const value = cache10.get(key);

  if (value !== undefined) {
    return value;
  }

  if (!internals.hasEs) {
    const dbs = await dbHealth();
    cache10.set(key, dbs);
    return dbs;
  }
  const health = await Db.health(cluster);
  if (cluster === undefined) {
    // a sessions db that isn't ES has no sessions template there
    try {
      const { body: doc } = await internals.client7.indices.getTemplate({
        name: fixIndex('sessions3_template'),
        filter_path: '**._meta'
      });
      health.molochDbVersion = doc[fixIndex('sessions3_template')]?.mappings?._meta?.molochDbVersion;
    } catch (err) {
      if (err.meta?.statusCode !== 404) { throw err; }
    }
  }
  cache10.set(key, health);
  return health;
};

Db.nodesInfoCache = async (cluster) => {
  const key = `nodesInfoCache-${cluster}`;
  const value = cache10.get(key);

  if (value !== undefined) {
    return value;
  }

  const { body: data } = await Db.nodesInfo({ cluster });
  cache10.set(key, data);
  return data;
};

Db.masterCache = async (cluster) => {
  const key = `master-${cluster}`;
  const value = cache10.get(key);

  if (value !== undefined) {
    return value;
  }

  const { body: data } = await Db.master(cluster);
  cache10.set(key, data);
  return data;
};

Db.loadESId2Info = async (cluster) => {
  if (esId2InfoLoadedCluster.has(cluster)) { return; }

  const { data } = await internals.clusterImpl.dstats.find({ where: { nodeName: { $prefix: 'es:' } }, size: 10000, cluster });
  for (const hit of data) {
    const id = hit.id.substring(3);
    const nodeName = hit.doc.nodeName.substring(3);
    const hostname = hit.doc.hostname.substring(3);
    esId2Info.set(`${cluster}-${id}`, { nodeName, hostname });
  }
  esId2InfoLoadedCluster.set(cluster, true);
};

Db.getESId2Node = (id, cluster) => {
  const node = esId2Info.get(`${cluster}-${id}`);
  if (!node) { return node; }
  return node.nodeName;
};

Db.updateESId2Info = async (id, nodeName, hostname, cluster) => {
  const key = `${cluster}-${id}`;
  if (esId2Info.has(key) && esId2Info.get(key).nodeName === nodeName && esId2Info.get(key).hostname === hostname) {
    return;
  }

  esId2Info.set(key, { nodeName, hostname });
  await internals.clusterImpl.dstats.put(`es:${id}`, { nodeName: `es:${nodeName}`, hostname: `es:${hostname}` }, { refresh: false });
};

Db.nodesStatsCache = async (cluster) => {
  const key = `nodesStats-${cluster}`;
  const value = cache10.get(key);

  if (value !== undefined) {
    return value;
  }

  const { body: data } = await Db.nodesStats({
    metric: 'jvm,process,fs,os,indices,thread_pool',
    cluster
  });

  const nodeKeys = Object.keys(data.nodes);

  for (let n = 0, nlen = nodeKeys.length; n < nlen; n++) {
    const node = data.nodes[nodeKeys[n]];
    Db.updateESId2Info(nodeKeys[n], node.name, node.host, cluster);
  }
  cache10.set(key, data);
  return data;
};

Db.indicesCache = async (cluster) => {
  const key = `indices-${cluster}`;
  const value = cache10.get(key);

  if (value !== undefined) {
    return value;
  }

  const { body: indices } = await Db.indices('_all', cluster);
  cache10.set(key, indices);
  return indices;
};

Db.indicesSettingsCache = async (cluster) => {
  const key = `indicesSettings-${cluster}`;
  const value = cache10.get(key);

  if (value !== undefined) {
    return value;
  }

  const { body: indicesSettings } = await Db.indicesSettings('_all', cluster);
  cache10.set(key, indicesSettings);
  return indicesSettings;
};

Db.hostnameToNodeids = async (hostname) => {
  const { data } = await internals.clusterImpl.stats.find({ where: { hostname }, fields: ['hostname'], size: 10000 });
  return data.map(hit => hit.id);
};

Db.fileIdToFile = async (node, num) => {
  const key = node + '!' + num;
  const info = internals.fileId2File.get(key);
  if (info !== undefined) {
    return info;
  }

  let file = null;
  try {
    file = await internals.clusterImpl.files.get(node + '-' + num);
  } catch (err) {
  }
  if (file) {
    internals.fileId2File.set(key, file);
    internals.fileName2File.set(file.name, file);
  } else { // Cache file is unknown
    internals.fileId2File.delete(key);
  }
  return file;
};

Db.fileNameToFiles = async (fileName) => {
  let where;
  let size = 10;
  if (fileName[0] === '/' && fileName[fileName.length - 1] === '/') {
    where = { name: { $regex: fileName.substring(1, fileName.length - 1) } };
  } else if (fileName.indexOf('*') !== -1) {
    where = { name: { $wildcard: fileName } };
  }

  // Not wildcard/regex check the cache
  if (!where) {
    if (internals.fileName2File.has(fileName)) {
      return [internals.fileName2File.get(fileName)];
    }
    where = { name: fileName };
    size = 100;
  }

  try {
    const { data } = await internals.clusterImpl.files.find({ where, sort: { num: 'desc' }, size });
    const files = [];
    for (const hit of data) {
      const file = hit.doc;
      const key = file.node + '!' + file.num;
      internals.fileId2File.set(key, file);
      internals.fileName2File.set(file.name, file);
      files.push(file);
    }
    return files;
  } catch (err) {
    return null;
  }
};

Db.getSequenceNumber = async (sName) => {
  return internals.clusterImpl.sequence.nextSequence(sName);
};

// Files, stats and dstats, params are collection find params
Db.searchFiles = async (params, cluster) => {
  return internals.clusterImpl.files.find({ ...params, cluster });
};
Db.filesSearchBody = (params) => {
  return internals.clusterImpl.files.searchBody?.(params);
};
Db.countFiles = async (cluster) => {
  return internals.clusterImpl.files.count(null, { cluster });
};
Db.putFile = async (id, doc) => {
  return internals.clusterImpl.files.put(id, doc);
};
// [{ key: node, count, first }] the oldest file per node
Db.filesRetention = async (cluster) => {
  return internals.clusterImpl.files.aggregate({ where: { locked: 0 }, groupBy: 'node', metrics: { first: { min: 'first' } }, cluster });
};
Db.searchStats = async (params, cluster) => {
  return internals.clusterImpl.stats.find({ ...params, cluster });
};
Db.countStats = async (cluster) => {
  return internals.clusterImpl.stats.count(null, { cluster });
};
Db.searchDStats = async (params, cluster) => {
  return internals.clusterImpl.dstats.find({ ...params, cluster });
};

Db.numberOfDocuments = async (index, options) => {
  const impl = isSessionsIndex(index) ? internals.sessionsImpl : internals.esImpl;
  return impl.numberOfDocuments(index, options);
};

Db.checkVersion = async function (minVersion) {
  const match = process.versions.node.match(/^(\d+)\.(\d+)\.(\d+)/);
  const nodeVersion = parseInt(match[1], 10) * 10000 + parseInt(match[2], 10) * 100 + parseInt(match[3], 10);
  if (nodeVersion < 240000) {
    console.log(`ERROR - Need node 24 or 26, currently using ${process.version}`);
    process.exit(1);
  } else if ((nodeVersion >= 250000 && nodeVersion < 260000) || nodeVersion >= 270000) {
    console.log(`ERROR - Node version ${process.version} is not supported, please use node 24 or 26`);
    process.exit(1);
  }

  try {
    await internals.clusterImpl.check();
  } catch (err) {
    // A separate cluster db needs its own init with its prefix
    const where = internals.clusterShared ? '' : `, for the cluster db use 'db/db.pl --prefix ${internals.clusterPrefix} <dbUrl> init'`;
    console.log(`ERROR - ${err.message}${where}\n`, err.cause?.message ?? '');
    process.exit(1);
  }

  // the version lives in the ES sessions template, other sessions dbs have their own schema
  if (internals.sessionsImpl === internals.esImpl) {
    await ArkimeUtil.checkArkimeSchemaVersion(internals.client7, internals.prefix, minVersion);
  }
  // a separate cluster db was init'd on its own, an ES one has its own template version
  if (!internals.clusterShared) {
    await internals.clusterConn.schemaVersion?.(minVersion);
  }
};

Db.isLocalView = async function (node) {
  if (node === internals.nodeName) {
    if (internals.debug > 1) {
      console.log(`DEBUG: node:${node} is local view because equals ${internals.nodeName}`);
    }
    return true;
  }

  try {
    const stat = await Db.arkimeNodeStatsCache(node);
    if (stat.hostname !== os.hostname() && stat.hostname !== internals.hostName) {
      if (internals.debug > 1) {
        console.log(`DEBUG: node:${node} is NOT local view because ${stat.hostname} != ${os.hostname()} or --host ${internals.hostName}`);
      }
      return false;
    } else {
      if (internals.debug > 1) {
        console.log(`DEBUG: node:${node} is local view because ${stat.hostname} == ${os.hostname()} or --host ${internals.hostName}`);
      }
      return true;
    }
  } catch (err) {
    if (internals.debug > 1) {
      console.log(`DEBUG: node:${node} is NOT local view because error ${err} ${os.hostname()} ${internals.hostName}`);
    }
    return false;
  }
};

Db.deleteFile = async (node, id, path) => {
  try {
    await fs.promises.unlink(path);
  } catch (err) {
    console.log('EXPIRE - error deleting file', node, id, path, err);
  }
  await Db.deleteFileDoc(id);
};

// Only the files doc, not the pcap
Db.deleteFileDoc = async (id) => {
  return internals.clusterImpl.files.delete(id);
};

Db.session2Sid = function (item) {
  // ver can be 3@ (sessions3) or x@ (user-specified queryExtraIndices)
  const ver = internals.queryExtraIndicesRegex.some(re => re.test(item._index)) ? 'x@' : '3@';
  if (ver === 'x@') {
    // document from queryExtraIndices, format Sid as x@_index:_id
    return ver + item._index + ':' + item._id;
  } else if (item._id.length < 31) {
    if (item._index.startsWith('partial-')) {
      // partial-tests_sessions3-191021 191021-abcd => 3@191021:191021-abcd
      return ver + item._index.substring(internals.prefix.length + 18) + ':' + item._id;
    } else {
      // tests_sessions3-191021 191021-abcd => 3@191021:191021-abcd
      return ver + item._index.substring(internals.prefix.length + 10) + ':' + item._id;
    }
  } else {
    // tests_sessions3-191021 191021-abcdefghijklmnopqrstuvwxyz => 3@191021-abcdefghijklmnopqrstuvwxyz
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

Db.sid2Index = function (id) {
  const colon = id.indexOf(':');

  if (id[1] === '@') {
    if (id[0] === 'x') {
      // ver is x@, which indicates user-specified queryExtraIndices,
      //   so the id will be formatted x@_index:_id
      const index = id.substr(2, colon - 2);
      if (colon > 2 && /^[\w.+-]+$/.test(index) && internals.queryExtraIndicesRegex.some(re => re.test(index))) {
        return index;
      } else {
        throw new Error('Db.sid2Index: ERROR - queryExtraIndices regex did not match');
      }
    }

    const indexDate = (colon > 0) ? id.substr(2, colon - 2) : id.substr(2, id.indexOf('-') - 2);
    if (id[0] !== '3' || !/^[\w-]+$/.test(indexDate)) {
      throw new Error('Db.sid2Index: ERROR - invalid session id');
    }
    return 'sessions' + id[0] + '-' + indexDate;
  }

  const date = (colon > 0) ? id.substr(0, colon) : id.substr(0, id.indexOf('-'));
  if (!/^[\w-]+$/.test(date)) {
    throw new Error('Db.sid2Index: ERROR - invalid session id');
  }

  const s3 = 'sessions3-' + date;
  return internals.aliasesCache ? fixIndex(s3) : s3;
};

// regression tests read and write the collections capture writes
Db.clusterCollection = (collName) => {
  if (['files', 'stats', 'dstats', 'fields', 'sequence'].includes(collName)) { return internals.clusterImpl[collName]; }
  return internals.regressionTests ? internals.clusterImpl.testCollection(collName) : undefined;
};

// [{ id, doc }] every field definition
Db.loadFields = async () => {
  return (await internals.clusterImpl.fields.find({ size: 10000 })).data;
};

Db.getSessionIndices = function (excludeExtra) {
  return internals.sessionsImpl.getSessionIndices(excludeExtra);
};

Db.getIndices = async (startTime, stopTime, bounding, rotateIndex, extraIndices) => {
  return internals.sessionsImpl.getIndices(startTime, stopTime, bounding, rotateIndex, extraIndices);
};

Db.getMinValue = async (index, field) => {
  const impl = isSessionsIndex(index) ? internals.sessionsImpl : internals.esImpl;
  return impl.getMinValue(index, field);
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

// Queries (crons) DB interactions
Db.setQueriesNode = async (node, force) => {
  return internals.clusterImpl.setQueriesNode(node, force);
};

Db.getQueriesNode = async () => {
  return internals.clusterImpl.getQueriesNode();
};

// returns the query or null
Db.getQuery = async (id) => {
  return internals.clusterImpl.getQuery(id);
};
Db.createQuery = async (doc) => {
  return internals.clusterImpl.createQuery(doc);
};
Db.updateQuery = async (id, fields) => {
  return internals.clusterImpl.updateQuery(id, fields);
};
Db.deleteQuery = async (id) => {
  return internals.clusterImpl.deleteQuery(id);
};
// params are collection find params, returns { total, data: [{ id, doc }] }
Db.searchQueries = async (params) => {
  return internals.clusterImpl.searchQueries(params);
};
