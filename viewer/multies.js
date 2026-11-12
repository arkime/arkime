/******************************************************************************/
/* multies.js  -- Make multiple ES servers look like one by merging results
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

/// / Modules
/// ///////////////////////////////////////////////////////////////////////////////
const Config = require('./config.js');
const express = require('express');
const async = require('async');
const { Client } = require('@elastic/elasticsearch');
const http = require('http');
const https = require('https');
const path = require('path');
const cryptoLib = require('crypto');
const ArkimeUtil = require('../common/arkimeUtil');
const ArkimeConfig = require('../common/arkimeConfig');
const { newAggregations, aggConvert2Obj, aggAdd, aggConvert2Arr } = require('./multiesAggs');
const MultiesClusterDb = require('./multiesClusterDb');
const DbClusterImpl = require('./db.cluster.js');
const DbESImpl = require('./db.es.js');
const ESConnection = require('../common/arkimeDb.es.js');
const ArkimeDb = require('../common/arkimeDb');
require('./db.ch.js').register();

process.on('unhandledRejection', (reason, promise) => {
  console.error('=== UNHANDLED REJECTION ===');
  console.error('Promise:', promise);
  if (reason instanceof Error) {
    console.error('Reason:', reason.message);
    console.error('Stack:', reason.stack);
    if (reason.meta) { try { console.error('Meta:', JSON.stringify(reason.meta, null, 2)); } catch (e) {} }
  } else {
    console.error('Reason:', reason);
  }
  console.error('===========================');
});
process.on('uncaughtException', (err, o) => {
  console.error('=== UNCAUGHT EXCEPTION ===');
  console.error('Origin:', o);
  console.error('Error:', err && err.message);
  console.error('Stack:', err && err.stack);
  console.error('==========================');
});
process.on('warning', (w) => {
  console.error('=== NODE WARNING ===');
  console.error(w.name, w.message);
  console.error(w.stack);
  console.error('====================');
});

let esSSLOptions; // set once the config is loaded
let httpAgent;
let httpsAgent;
let multiESBasicAuthHmac;

// How long a backend may stay silent before we give up on it (ms)
let backendTimeout = 300 * 1000;

ArkimeConfig.loaded(() => {
  esSSLOptions = ArkimeUtil.tlsOptionsOrExit({
    insecure: ArkimeConfig.insecure,
    caTrustFile: Config.get('caTrustFile'),
    clientKey: Config.get('esClientKey'),
    clientCert: Config.get('esClientCert'),
    clientKeyPass: Config.get('esClientKeyPass')
  }, 'OpenSearch/Elasticsearch');
  httpAgent = new http.Agent({ keepAlive: true, keepAliveMsecs: 5000, maxSockets: 100 });
  httpsAgent = new https.Agent(Object.assign({ keepAlive: true, keepAliveMsecs: 5000, maxSockets: 100 }, esSSLOptions));

  const expected = Config.get('multiESBasicAuth');
  if (expected) {
    multiESBasicAuthHmac = cryptoLib.createHmac('sha256', 'compare').update(expected).digest();
  }

  backendTimeout = parseInt(Config.get('elasticsearchTimeout', 300), 10) * 1000;
  if (isNaN(backendTimeout) || backendTimeout <= 0) { backendTimeout = 300 * 1000; }
});

const clients = {};
const sessionsImpls = {}; // a cluster's sessions backend when its sessionsDbUrl isn't OpenSearch/Elasticsearch
let sessionsFieldsMap = {}; // field definitions by db name for sessionsImpls, from the viewer's fields search
const clusterDbs = {}; // optional per-node cluster db, Postgres { conn } or OpenSearch/Elasticsearch { esUrl, client }
let nodes = [];
const clusters = {};
const clusterList = [];
const authHeader = {};
let activeESNodes = [];

function hasBody (req) {
  const encoding = 'transfer-encoding' in req.headers;
  const len = 'content-length' in req.headers && req.headers['content-length'] !== '0';
  return encoding || len;
}

function saveBody (req, res, next) {
  if (req._body) { return next(); }
  req.body = req.body || {};

  if (!hasBody(req)) { return next(); }

  // flag as parsed
  req._body = true;

  // parse
  let buf = '';
  req.setEncoding('utf8');
  req.on('data', (chunk) => { buf += chunk; });
  req.on('end', () => {
    req.body = buf;
    next();
  });
}

// app.configure
const favicon = require('serve-favicon');
const compression = require('compression');

const app = express();
// app.use ((req, res, next) => {console.log(req.url); next();});

app.enable('jsonp callback');
app.use(favicon(path.join(__dirname, '/public/favicon.ico')));
ArkimeUtil.logger(app);
app.use(saveBody);
app.use(compression());
app.use(function (req, res, next) {
  if (res.setTimeout) {
    res.setTimeout(10 * 60 * 1000); // Increase default from 2 min to 10 min
    return next();
  }
});

// ============================================================================
// Optional basic auth: if multiESBasicAuth ("user:password") is set in config,
// require it on every request. Compared in constant time.
// ============================================================================
app.use((req, res, next) => {
  if (!multiESBasicAuthHmac) { return next(); }

  const header = req.headers.authorization;
  if (!header || !header.startsWith('Basic ')) {
    return res.set('WWW-Authenticate', 'Basic').status(401).send();
  }

  const supplied = Buffer.from(header.slice(6).trim(), 'base64').toString('utf8');
  const b = cryptoLib.createHmac('sha256', 'compare').update(supplied).digest();
  if (!cryptoLib.timingSafeEqual(multiESBasicAuthHmac, b)) {
    return res.set('WWW-Authenticate', 'Basic').status(401).send();
  }

  return next();
});

// Routing looks at the raw path, so refuse one that new URL() would rewrite (dot segments, \) before forwarding
app.use((req, res, next) => {
  let pathname;
  try {
    pathname = new URL('http://localhost' + req.url).pathname;
  } catch (e) { }
  if (pathname !== req.path) {
    return res.status(400).send({ error: 'Invalid path' });
  }
  return next();
});

function node2EntryUrl (node) {
  const url = node.split(',')[0].trim();
  if (url.match(/^http/)) { return url; }
  return 'http://' + url;
}

// The cluster's sessions db, sessionsDbUrl else dbUrl else the entry's url
function node2SessionsDbUrl (node) {
  return node2DbUrl(node, 'sessionsDbUrl') ?? node2DbUrl(node, 'dbUrl') ?? node2EntryUrl(node);
}

// The cluster's ES for the sessions and everything else ES only: the sessions db when it is one, else dbUrl when it
// is one, else the entry's url
function node2Url (node) {
  for (const url of [node2SessionsDbUrl(node), node2DbUrl(node, 'dbUrl')]) {
    if (url && ArkimeConfig.isEsUrl(url)) { return ArkimeConfig.esUrl(url).replace(/\/+$/, ''); }
  }
  return node2EntryUrl(node);
}

// Does the cluster have an OpenSearch/Elasticsearch, not when both the sessions and the cluster db are elsewhere
function node2HasEs (node) {
  const dbUrl = node2DbUrl(node, 'dbUrl');
  return ArkimeConfig.isEsUrl(node2SessionsDbUrl(node)) || !dbUrl || ArkimeConfig.isEsUrl(dbUrl);
}

// dbPrefix, else the entry's prefix
function node2ClusterPrefix (node) {
  const prefix = node2DbUrl(node, 'dbPrefix');
  return prefix === null ? node2Prefix(node) : ArkimeUtil.formatPrefix(prefix);
}

// sessionsDbPrefix, then the entry's prefix with a sessionsDbUrl or the cluster one without
function node2SessionsPrefix (node) {
  const prefix = node2DbUrl(node, 'sessionsDbPrefix');
  if (prefix !== null) { return ArkimeUtil.formatPrefix(prefix); }
  return node2DbUrl(node, 'sessionsDbUrl') ? node2Prefix(node) : node2ClusterPrefix(node);
}

const MULTI_CLUSTER_RE = new RegExp(`MULTIPREFIX_(?=(${DbClusterImpl.INDEX_NAMES.join('|')})(?![a-z]))`, 'g');

// A cluster's prefixes and the regexes that turn them back into MULTIPREFIX_, they never change
const nodePrefixes = new Map();
function node2Prefixes (node) {
  let p = nodePrefixes.get(node);
  if (!p) {
    p = { sessions: node2SessionsPrefix(node), cluster: node2ClusterPrefix(node) };
    p.res = [
      [new RegExp('(index":\\s*|[,{]|  )"' + p.cluster + '(stats|dstats|sequence|files|history)', 'g'), '$1"MULTIPREFIX_$2'],
      [new RegExp('(index":\\s*)"' + p.cluster + '(fields_v[1-4][0-9]?)"', 'g'), '$1"MULTIPREFIX_$2"'],
      [new RegExp('(index":\\s*|[,{]|  )"' + p.sessions + '(sessions3|users)', 'g'), '$1"MULTIPREFIX_$2']
    ];
    nodePrefixes.set(node, p);
  }
  return p;
}

// MULTIPREFIX_ to the cluster's prefix for that index
function toNodePrefix (str, node) {
  const p = node2Prefixes(node);
  return str.replace(MULTI_CLUSTER_RE, p.cluster).replace(/MULTIPREFIX_/g, p.sessions);
}

// The cluster's prefixes in an ES response back to MULTIPREFIX_
function fromNodePrefix (str, node) {
  for (const [re, to] of node2Prefixes(node).res) {
    str = str.replace(re, to);
  }
  return str;
}

// Requests for the cluster db indices, which a cluster may keep in another db
function isClusterDbPath (reqPath) {
  return !!MultiesClusterDb.collectionName(reqPath);
}

// A flush or refresh of everything also goes to each cluster's separate OpenSearch/Elasticsearch cluster db
function clusterDbsToo (op) {
  return async (req, res, next) => {
    await Promise.all(Object.entries(clusterDbs).filter(([, c]) => c.client).map(([node, c]) =>
      c.client.indices[op]({ index: `${node2ClusterPrefix(node)}*` }).catch(() => {})
    ));
    next();
  };
}

function node2Prefix (node) {
  const parts = node.split(',');
  for (let p = 1; p < parts.length; p++) {
    const kv = parts[p].split(':');
    if (kv[0] === 'prefix') {
      if (kv[1] === '') {
        return '';
      } else if (kv[1].charAt(kv[1].length - 1) !== '_') {
        return kv[1] + '_';
      }
      return kv[1];
    }
  }
  return 'arkime_';
}

function node2Name (node) {
  const parts = node.split(',');
  for (let p = 1; p < parts.length; p++) {
    const kv = parts[p].trim().split(':');
    if (kv[0] === 'name') {
      return kv[1]?.trim();
    }
  }
  return null;
}

function node2ESAPIKey (node) {
  const parts = node.split(',');
  for (let p = 1; p < parts.length; p++) {
    const kv = parts[p].trim().split(':');
    if (kv[0] === 'elasticsearchAPIKey') {
      return kv[1]?.trim();
    }
  }
  return null;
}

function node2ESBasicAuth (node) {
  const parts = node.split(',');
  for (let p = 1; p < parts.length; p++) {
    const kv = parts[p].trim().split(':');
    if (kv[0] === 'elasticsearchBasicAuth') {
      return kv.slice(1).join(':'); // value may be user:pass, keep everything after the key
    }
  }
  return null;
}

// Parse a `<key>:<full-url>` like sessionsDbUrl from a multiESNodes entry. Unlike the
// other node2* parsers, the value can itself contain `:` (e.g. scheme +
// port), so split on the first `:` only.
function node2DbUrl (node, key) {
  const parts = node.split(',');
  for (let p = 1; p < parts.length; p++) {
    const idx = parts[p].indexOf(':');
    if (idx < 0) { continue; }
    if (parts[p].substring(0, idx) === key) {
      return parts[p].substring(idx + 1);
    }
  }
  return null;
}

// A multiESNodes entry with url credentials and the ES api key or basic auth hidden, safe to log
function node2Safe (node) {
  return node.split(',').map((part, p) => {
    if (/^\s*(elasticsearchAPIKey|elasticsearchBasicAuth):/.test(part)) { return part.replace(/:.*/, ':***'); }
    return part.replace(p === 0 ? /^([a-z][a-z0-9+.-]*:\/\/)?[^/@]*@/i : /([a-z][a-z0-9+.-]*:\/\/)[^/@]*@/i, '$1***@');
  }).join(',');
}

// True if the `:index` route segment refers to a sessions index.
function isSessionsIndex (idx) {
  return idx && /sessions3/.test(idx);
}

function getActiveNodes (clusterin) {
  if (clusterin) {
    if (!Array.isArray(clusterin)) {
      clusterin = [clusterin];
    }
    const tmpNodes = [];
    for (let i = 0; i < clusterin.length; i++) {
      if (clusters[clusterin[i]]) { // cluster -> node
        tmpNodes.push(clusters[clusterin[i]]);
      }
    }
    const esNodes = activeESNodes.slice().filter(node => tmpNodes.includes(node));
    return esNodes;
  } else {
    return activeESNodes.slice();
  }
}

function makeRequest (url, options, cb) {
  let result = '';
  let called = false;
  const done = (err, res) => {
    if (called) { return; }
    called = true;
    if (cb) { cb(err, res); }
  };

  const preq = options.arkime_client.request(url, options, (pres) => {
    pres.on('data', (chunk) => {
      result += chunk.toString();
    });
    pres.on('end', () => {
      if (result.length) {
        if (!options.skipReplace) {
          result = fromNodePrefix(result, options.arkime_node);
        }
        try {
          result = JSON.parse(result);
        } catch (e) {
          console.log('ERROR - could not parse response from ES', e);
          return done(e, {});
        }
      } else {
        result = {};
      }
      // HTTP error statuses count as failures; keep the body, it carries the error details
      if (pres.statusCode >= 400) {
        return done(new Error(`backend status ${pres.statusCode}`), result);
      }
      done(null, result);
    });
    pres.on('error', (e) => {
      if (!called) { console.log('Response error', e); }
      done(e, {});
    });
  });
  preq.setHeader('content-type', 'application/json');

  // A backend that never answers would otherwise hang the request forever
  // and eventually exhaust the agent's sockets
  preq.setTimeout(backendTimeout, () => {
    console.log('ERROR - backend timeout', url.host, url.pathname);
    done(new Error('backend timeout'), {});
    preq.destroy();
  });
  if (options.arkime_opaque !== undefined) {
    preq.setHeader('X-Opaque-Id', options.arkime_opaque);
  }
  if (options.arkime_body !== undefined && ArkimeUtil.isString(options.arkime_body, 0)) {
    if (options.method === 'DELETE') {
      preq.setHeader('content-length', options.arkime_body.length);
    }
    preq.end(options.arkime_body);
  }
  preq.on('error', (e) => {
    if (!called) { console.log('Request error', e); }
    done(e, {});
  });
  preq.end();
}

function simpleGather (req, res, bodies, doneCb, preFetched) {
  // A throw while merging would otherwise leave the request open until the
  // caller's own timeout, holding its socket
  const finish = (err, results) => {
    try {
      return doneCb(err, results);
    } catch (e) {
      console.log('ERROR - %s %s merging cluster results', req.method, req.url, e);
      if (!res.headersSent) { res.status(500).send({ error: 'Failed to merge cluster results' }); }
    }
  };

  let cluster = null;
  if (req.query.cluster) {
    cluster = Array.isArray(req.query.cluster) ? req.query.cluster : req.query.cluster.split(',');
    req.url = req.url.replace(/cluster=[^&]*(&|$)/g, ''); // remove cluster from URL
    delete req.query.cluster;
  }
  const activeNodes = getActiveNodes(cluster);
  if (activeNodes.length === 0) { // Empty nodes. Either all clusters are down or invalid clusters
    return finish(true, [{}]);
  }
  async.map(activeNodes, (node, asyncCb) => {
    if (preFetched && Object.prototype.hasOwnProperty.call(preFetched, node)) {
      let result = preFetched[node];
      // Apply the same prefix→MULTIPREFIX_ rewrite the HTTP path does
      // (see makeRequest), so multi-cluster viewers see consistent
      // _index values regardless of whether a backend is ES or CH.
      if (node2Prefixes(node).sessions && !req._skipReplace) {
        try {
          result = JSON.parse(fromNodePrefix(JSON.stringify(result), node));
        } catch (e) { /* leave result untouched on error */ }
      }
      result.cluster = clusters[node];
      return asyncCb(null, result);
    }
    const collName = clusterDbs[node]?.conn && MultiesClusterDb.collectionName(req.path);
    if (collName) {
      const body = req._body ? (bodies?.[node] ?? req.body) : undefined;
      return MultiesClusterDb.answer(clusterDbs[node].conn.collection(collName, DbClusterImpl.collectionOptions(collName)), req._clusterDbOp, req.params.id, req.query, body)
        .then((result) => {
          result.cluster = clusters[node];
          asyncCb(null, result);
        })
        .catch((err) => {
          console.log("ERROR - multies cluster db %s %s for cluster '%s':", req.method, ArkimeUtil.sanitizeStr(req.path), node2Name(node), ArkimeUtil.sanitizeLine(err.message));
          asyncCb(null, { cluster: clusters[node], arkime_failed: true, error: err.message });
        });
    }
    // Only the sessions and cluster db calls above have somewhere to go
    if (!clients[node]) {
      return asyncCb(null, { cluster: clusters[node], arkime_failed: true });
    }
    const nodeName = node2Name(node);
    let nodeUrl = (clusterDbs[node]?.esUrl && isClusterDbPath(req.path) ? clusterDbs[node].esUrl : node2Url(node)) + req.url;

    const options = {
      method: req.method,
      arkime_opaque: req.headers['x-opaque-id'],
      skipReplace: req._skipReplace
    };
    options.arkime_node = node;
    nodeUrl = toNodePrefix(nodeUrl, node);
    const url = new URL(nodeUrl);
    if (url.protocol === 'https:') {
      options.agent = httpsAgent;
      options.arkime_client = https;
    } else {
      options.agent = httpAgent;
      options.arkime_client = http;
    }
    if (authHeader[nodeName]) {
      options.headers = {
        Authorization: authHeader[nodeName]
      };
    }
    if (req._body) {
      if (bodies && bodies[node]) {
        options.arkime_body = bodies[node];
      } else {
        options.arkime_body = req.body;
      }
    }

    if (req.arkime_need_to_scroll) {
      // Save size and reset to 1000 per scroll call
      let body;
      try {
        body = JSON.parse(options.arkime_body);
      } catch (e) {
        console.log('ERROR - could not parse arkime_body for scroll', e);
        return res.status(400).send('Invalid body');
      }
      const totSize = body.size;
      body.size = 1000;
      options.arkime_body = JSON.stringify(body);

      // Add scroll parameter to url to turn on scrolling
      url.href += '&scroll=2m';

      let fullResults;
      makeRequest(url, options, function doResponse (err, result) {
        // On failure return what was gathered so far, marked failed if nothing was
        if (err || !result?.hits?.hits) {
          if (fullResults === undefined) {
            fullResults = { cluster: clusters[node], arkime_failed: true };
          }
          return asyncCb(null, fullResults);
        }

        // First response just save, after append
        if (fullResults === undefined) {
          fullResults = result;
          fullResults.cluster = clusters[node];
        } else {
          fullResults.hits.hits = fullResults.hits.hits.concat(result.hits.hits);
        }

        // We are done, return the results
        if (result.hits.hits.length === 0 || fullResults.hits.hits.length >= totSize) {
          // Clear the scroll if we have a scroll id
          if (result._scroll_id !== undefined) {
            url.pathname = '/_search/scroll';
            url.search = '';
            body = { scroll_id: result._scroll_id };
            options.arkime_body = JSON.stringify(body);
            options.method = 'DELETE';
            makeRequest(url, options);
          }

          return asyncCb(null, fullResults);
        }

        // Not done, but just keep doing a scroll, clearing any search parameters
        url.pathname = '/_search/scroll';
        url.search = '';
        body = { scroll: '2m', scroll_id: result._scroll_id };
        options.arkime_body = JSON.stringify(body);
        makeRequest(url, options, doResponse);
      });
    } else {
      // Not a scroll
      makeRequest(url, options, (err, result) => {
        result.cluster = clusters[node];
        if (err) { result.arkime_failed = true; }
        asyncCb(null, result);
      });
    }
  }, (err, results) => {
    // Drop failed backends so one bad cluster can't break a query the rest can
    // answer. If all failed, pass through for the per-endpoint error handling.
    if (Array.isArray(results)) {
      const good = results.filter(r => r && !r.arkime_failed);
      if (good.length > 0) { results = good; }
      for (const r of results) { if (r) { delete r.arkime_failed; } }
    }
    return finish(err, results);
  });
}

function shallowCopy (obj1, obj2) {
  for (const attrname in obj2) {
    obj1[attrname] = obj2[attrname];
  }
}

// Merge all the top level nodes fields
function simpleGatherNodes (req, res) {
  simpleGather(req, res, null, (err, results) => {
    const obj = results[0];
    for (let i = 1; i < results.length; i++) {
      shallowCopy(obj.nodes, results[i].nodes);
    }
    res.send(obj);
  });
}

// Merge all the top level tasks fields
function simpleGatherTasks (req, res) {
  simpleGather(req, res, null, (err, results) => {
    const obj = results[0];
    for (let i = 1; i < results.length; i++) {
      shallowCopy(obj.tasks, results[i].tasks);
    }
    res.send(obj);
  });
}

function shallowAdd (obj1, obj2) {
  for (const attrname in obj2) {
    if (typeof obj2[attrname] === 'number') {
      obj1[attrname] += obj2[attrname];
    }
  }
}
// For any top level number field add them together
function simpleGatherAdd (req, res) {
  simpleGather(req, res, null, (err, results) => {
    const obj = results[0];
    for (let i = 1; i < results.length; i++) {
      shallowAdd(obj, results[i]);
    }
    obj.cluster_name = 'COMBINED';
    res.send(obj);
  });
}

function simpleGatherFirst (req, res) {
  simpleGather(req, res, null, (err, results) => {
    res.send(results[0]);
  });
}

// Count for sessions indices on CH-backed clusters: dispatch to db.ch.js
// numberOfDocuments per node and let simpleGatherAdd sum the counts.
function sessionsCount (req, res) {
  const indexParam = req.params.index;
  if (!isSessionsIndex(indexParam)) { return simpleGatherAdd(req, res); }
  const cluster = req.query.cluster
    ? (Array.isArray(req.query.cluster) ? req.query.cluster : req.query.cluster.split(','))
    : null;
  const activeNodes = getActiveNodes(cluster);
  const preFetched = {};
  async.each(activeNodes, (node, cb) => {
    if (!sessionsImpls[node]) { return cb(); }
    sessionsImpls[node].numberOfDocuments(toNodePrefix(indexParam, node), {})
      .then((r) => { preFetched[node] = { count: Number(r.count || 0) }; cb(); })
      .catch((err) => {
        console.log(`multies sessions _count failed for cluster '${node2Name(node)}':`, ArkimeUtil.sanitizeLine(err.message));
        preFetched[node] = { count: 0 };
        cb();
      });
  }, () => {
    simpleGather(req, res, null, (err, results) => {
      const obj = results[0];
      for (let i = 1; i < results.length; i++) {
        shallowAdd(obj, results[i]);
      }
      obj.cluster_name = 'COMBINED';
      res.send(obj);
    }, preFetched);
  });
}

function simpleGather1Cluster (req, res) {
  req._skipReplace = true;
  if (req.query.cluster === undefined) {
    console.log('Missing cluster', ArkimeUtil.sanitizeStr(req.url));
    return res.send({ error: 'Missing cluster' });
  }
  if (req.query.cluster.split(',').length !== 1) {
    console.log('Expecting 1 cluster', ArkimeUtil.sanitizeStr(req.url));
    return res.send({ error: 'Expecting 1 cluster' });
  }

  // Remove cluster from body if there
  if (req._body) {
    let body;
    try {
      body = JSON.parse(req.body);
    } catch (e) {
      return res.status(400).send({ error: 'Invalid JSON body' });
    }
    delete body.cluster;
    req.body = JSON.stringify(body);
  }

  simpleGather(req, res, null, (err, results) => {
    res.send(results[0]);
  });
}

// Tag a route with the cluster db operation it performs, simpleGather never takes one from the request path
function clusterDbOp (op) {
  return (req, res, next) => {
    req._clusterDbOp = op;
    next();
  };
}

app.get('/_tasks', simpleGatherTasks);
app.post('/_tasks/:taskId/_cancel', simpleGather1Cluster);
app.post('/_tasks/_cancel', simpleGather1Cluster);

app.get('/_cluster/nodes/stats', simpleGather1Cluster);
app.get('/_nodes', simpleGatherNodes);
app.get('/_nodes/stats', simpleGatherNodes);
app.get('/_nodes/stats/:kinds', simpleGatherNodes);
app.get('/_cluster/health', simpleGatherAdd);

// A sessions db with per period indices answers for its own, like ES with no aliases
app.get(['/:index/_aliases', '/:index/_alias'], (req, res) => {
  if (!isSessionsIndex(req.params.index)) { return simpleGatherNodes(req, res); }
  sessionsPreFetch(async (impl, index) => {
    if (!impl.listIndices) { return undefined; }
    return Object.fromEntries((await impl.listIndices(index)).map(indexName => [indexName, { aliases: {} }]));
  }, req.params.index, (preFetched) => {
    simpleGather(req, res, null, (err, results) => {
      const obj = {};
      for (const result of results) {
        for (const [key, value] of Object.entries(result)) { if (key !== 'cluster' && key !== 'error') { obj[key] = value; } }
      }
      res.send(obj);
    }, preFetched);
  });
});

app.post('/:index/_close', simpleGather1Cluster);
app.post('/:index/_open', simpleGather1Cluster);
app.post('/:index/_forcemerge', simpleGather1Cluster);

app.delete('/:index', simpleGather1Cluster);
app.delete('/MULTIPREFIX_hunts/_doc/:id', clusterDbOp('delete'), simpleGather1Cluster);
app.delete('/MULTIPREFIX_history*/_doc/:id', clusterDbOp('delete'), simpleGather1Cluster);

app.get('/MULTIPREFIX_sessions*/_refresh', (req, res) => {
  // Keep the MULTIPREFIX_ placeholder so each node's real prefix is
  // substituted in simpleGather; collapse the index list to one pattern
  req.url = '/MULTIPREFIX_sessions*/_refresh';
  return simpleGatherFirst(req, res);
});

app.get('/:index/_settings', simpleGatherFirst);
app.put('/:index/_settings', simpleGather1Cluster);

app.put('/:index1/_shrink/:index2', simpleGather1Cluster);

app.get('/_ilm/policy/*', simpleGather1Cluster);
app.put('/_ilm/policy/*', simpleGather1Cluster);

app.get('/_cluster/:type/details', function (req, res) {
  const result = { available: [], active: [], inactive: [], prefix: {} };
  const activeNodes = getActiveNodes();
  for (let i = 0; i < clusterList.length; i++) {
    result.available.push(clusterList[i]);
    if (activeNodes.includes(clusters[clusterList[i]])) {
      result.active.push(clusterList[i]);
    } else {
      result.inactive.push(clusterList[i]);
    }
    result.prefix[clusterList[i]] = node2SessionsPrefix(clusters[clusterList[i]]);
  }
  res.send(result);
});

app.get('/:index/_status', (req, res) => {
  simpleGather(req, res, null, (err, results) => {
    const obj = results[0];
    for (let i = 1; i < results.length; i++) {
      for (const index in results[i].indices) {
        if (obj.indices[index]) {
          obj.indices[index].docs.num_docs += results[i].indices[index].docs.num_docs;
          obj.indices[index].docs.max_doc += results[i].indices[index].docs.max_doc;
        } else {
          obj.indices[index] = results[i].indices[index];
        }
      }
    }
    res.send(obj);
  });
});

app.get('/:index/_stats', (req, res) => {
  simpleGather(req, res, null, (err, results) => {
    // console.log("DEBUG - _stats results", JSON.stringify(results, null, 2));
    const obj = results[0];
    for (let i = 1; i < results.length; i++) {
      for (const index in results[i].indices) {
        if (obj.indices[index]) {
          obj.indices[index].total.docs.count += results[i].indices[index].total.docs.count;
          obj.indices[index].total.docs.deleted += results[i].indices[index].total.docs.deleted;
        } else {
          obj.indices[index] = results[i].indices[index];
        }
      }
    }
    res.send(obj);
  });
});

app.get('/_template/MULTIPREFIX_sessions3_template', (req, res) => {
  req._skipReplace = req.query.cluster !== undefined;
  simpleGather(req, res, null, (err, results) => {
    const version = (r) => r.MULTIPREFIX_sessions3_template?.mappings?._meta?.molochDbVersion;
    const versioned = results.filter(r => version(r) !== undefined);
    if (versioned.length === 0) {
      // No cluster has OpenSearch/Elasticsearch, their sessions dbs keep their own schema like a single viewer's
      if (!Object.keys(clients).length) {
        return res.send({ MULTIPREFIX_sessions3_template: { mappings: { _meta: { molochDbVersion: ArkimeDb.SCHEMA_VERSION } } } });
      }
      return res.send(results[0]);
    }
    res.send(versioned.reduce((min, r) => version(r) < version(min) ? r : min));
  });
});
app.put('/_template/MULTIPREFIX_sessions3_template', simpleGather1Cluster);

app.put('/arkime_sids_v50', (req, res) => {
  console.log('ERROR - Please set usersDbUrl and usersDbPrefix in config.ini for multi-viewer. Maybe something like');
  console.log('usersDbUrl=http://localhost:9200');
  console.log('usersDbPrefix=arkime');
  process.exit(1);
});

app.get(['/users/user/:user', '/users/_doc/:user'], async (req, res) => {
  try {
    const { body: user } = await clients[nodes[0]].get({
      index: 'users', type: '_doc', id: req.params.user
    });
    return res.send(user);
  } catch (err) {
    console.log('ERROR - GET /users/user/%s', ArkimeUtil.sanitizeStr(req.params.user), err);
    return res.send({});
  }
});

app.post(['/users/user/:user', '/users/_doc/:user'], async (req, res) => {
  try {
    const { body: result } = await clients[nodes[0]].index({
      index: 'users', type: '_doc', id: req.params.user, body: req.body, refresh: true
    });
    return res.send(result);
  } catch (err) {
    console.log('ERROR - POST /users/user/%s', ArkimeUtil.sanitizeStr(req.params.user), err);
    return res.send({});
  }
});

app.get('/_cat/master', async (req, res) => {
  try {
    const { body: result } = await clients[nodes[0]].cat.master({ format: 'json' });
    return res.send(result);
  } catch (err) {
    console.log('ERROR - GET /_cat/master', err);
    return res.send({});
  }
});

app.get('/_cat/*', (req, res) => {
  simpleGather(req, res, null, (err, results) => {
    let obj = results[0];
    for (let i = 1; i < results.length; i++) {
      if (results[i].error) {
        console.log('ERROR - GET', req.url, req.query.index, req.query.type, results[i].error);
      }
      obj = obj.concat(results[i]);
    }
    res.send(obj);
  });
});

// Runs fn(impl, the node's index) for each active cluster with a sessionsImpl, an undefined answer goes to the cluster's ES
function sessionsPreFetch (fn, index, cb) {
  const preFetched = {};
  async.each(getActiveNodes(), (node, nodeCb) => {
    if (!sessionsImpls[node]) { return nodeCb(); }
    fn(sessionsImpls[node], toNodePrefix(index, node)).then((result) => {
      if (result !== undefined) { preFetched[node] = result; }
    }).catch((err) => {
      console.log(`multies sessions request failed for cluster '${node2Name(node)}':`, ArkimeUtil.sanitizeLine(err.message));
      preFetched[node] = { error: err.message };
    }).then(() => nodeCb());
  }, () => cb(preFetched));
}

app.get(['/:index/:type/_search', '/:index/_search'], clusterDbOp('search'), (req, res) => {
  if (!isSessionsIndex(req.params.index)) { return getSearch(req, res, {}); }
  sessionsPreFetch((impl, index) => impl.searchSessions(index, { query: { match_all: {} }, size: Number(req.query.size ?? 10) }, {}), req.params.index, (preFetched) => getSearch(req, res, preFetched));
});

function getSearch (req, res, preFetched) {
  simpleGather(req, res, null, (err, results) => {
    const obj = results[0];
    obj.hits ??= { total: 0, hits: [] };
    for (let i = 1; i < results.length; i++) {
      if (results[i].error || !results[i].hits) {
        console.log('ERROR - GET _search', req.query.index, req.query.type, results[i].error);
        continue;
      }
      obj.hits.total += results[i].hits.total;
      obj.hits.hits = obj.hits.hits.concat(results[i].hits.hits);
    }
    res.send(obj);
  }, preFetched);
}

app.get('/:index/:type/:id', clusterDbOp('get'), function (req, res) {
  if (isSessionsIndex(req.params.index)) {
    const cluster = req.query.cluster
      ? (Array.isArray(req.query.cluster) ? req.query.cluster : req.query.cluster.split(','))
      : null;
    const activeNodes = getActiveNodes(cluster);
    const preFetched = {};
    return async.each(activeNodes, (node, cb) => {
      if (!sessionsImpls[node]) { return cb(); }
      sessionsImpls[node].getSession(req.params.id, {})
        .then((hit) => {
          if (hit) {
            preFetched[node] = { _id: hit._id, _index: hit._index, _source: hit._source, fields: hit.fields, found: true };
          } else {
            preFetched[node] = { _id: req.params.id, _index: req.params.index, found: false };
          }
          cb();
        })
        .catch((err) => {
          console.log(`multies sessions get failed for cluster '${node2Name(node)}':`, ArkimeUtil.sanitizeLine(err.message));
          preFetched[node] = { _id: req.params.id, found: false, error: err.message };
          cb();
        });
    }, () => {
      simpleGather(req, res, null, (err, results) => {
        for (let i = 0; i < results.length; i++) {
          if (results[i].found) {
            if (results[i]._source) { results[i]._source.cluster = results[i].cluster; }
            if (results[i].fields) { results[i].fields.cluster = results[i].cluster; }
            return res.send(results[i]);
          }
        }
        res.send(results[0]);
      }, preFetched);
    });
  }

  simpleGather(req, res, null, (err, results) => {
    for (let i = 0; i < results.length; i++) {
      if (results[i].found) {
        if (results[i]._source) {
          results[i]._source.cluster = results[i].cluster;
        }
        if (results[i].fields) {
          results[i].fields.cluster = results[i].cluster;
        }
        return res.send(results[i]);
      }
    }
    res.send(results[0]);
  });
});

app.get('/_cluster/settings', function (req, res) {
  let cluster = null;
  if (req.query.cluster) {
    cluster = Array.isArray(req.query.cluster) ? req.query.cluster : req.query.cluster.split(',');
    req.url = req.url.replace(/cluster=[^&]*(&|$)/g, ''); // remove cluster from URL
    req._skipReplace = true;
  }
  if (!cluster || cluster.length > 1) {
    res.send({ persistent: {}, transient: {} });
  } else {
    simpleGather(req, res, null, (err, results) => {
      res.send(results[0]);
    });
  }
});

app.put('/_cluster/settings', simpleGather1Cluster);

app.head(/^\/$/, function (req, res) {
  res.send('');
});

app.get(/^\/$/, function (req, res) {
  simpleGather(req, res, null, (err, results) => {
    res.send(results[0]);
  });
});

// Facets are a pain, we convert from array to map to back to array
/// ///////////////////////////////////////////////////////////////////////////////

function facet2Obj (field, facet) {
  const obj = {};
  for (let i = 0; i < facet.length; i++) {
    obj[facet[i][field]] = facet[i];
  }
  return obj;
}

function facet2Arr (facet, type) {
  let arr = [];
  for (const attrname in facet) {
    arr.push(facet[attrname]);
  }

  if (type === 'histogram') {
    arr = arr.sort((a, b) => { return a.key - b.key; });
  } else {
    arr = arr.sort((a, b) => { return b.count - a.count; });
  }
  return arr;
}

function facetConvert2Obj (facets) {
  for (const facetname in facets) {
    if (facets[facetname]._type === 'histogram') {
      facets[facetname].entries = facet2Obj('key', facets[facetname].entries);
    } else if (facets[facetname]._type === 'terms') {
      facets[facetname].terms = facet2Obj('term', facets[facetname].terms);
    } else {
      console.log('Unknown facet type', facets[facetname]._type);
    }
  }
}

function facetConvert2Arr (facets) {
  for (const facetname in facets) {
    const facetarray = facets[facetname]._type === 'histogram' ? 'entries' : 'terms';
    facets[facetname][facetarray] = facet2Arr(facets[facetname][facetarray], facets[facetname]._type);
  }
}

function facetAdd (obj1, obj2) {
  for (const facetname in obj2) {
    const facetarray = obj1[facetname]._type === 'histogram' ? 'entries' : 'terms';

    obj1[facetname].total += obj2[facetname].total;
    obj1[facetname].missing += obj2[facetname].missing;
    obj1[facetname].other += obj2[facetname].other;

    for (const entry in obj2[facetname][facetarray]) {
      if (!obj1[facetname][facetarray][entry]) {
        obj1[facetname][facetarray][entry] = obj2[facetname][facetarray][entry];
      } else {
        const o1 = obj1[facetname][facetarray][entry];
        const o2 = obj2[facetname][facetarray][entry];

        o1.count += o2.count;
        if (o1.total) {
          o1.total += o2.total;
        }
      }
    }
  }
}

function fixQuery (node, body, doneCb) {
  try {
    body = JSON.parse(body);
  } catch (e) {
    console.log('ERROR - could not parse body in fixQuery', e);
    return doneCb(null, body);
  }

  // Reset from & size since we do aggregation
  if (body.size) {
    body.size = (+body.size) + ((+body.from) || 0);
  }

  delete body.from;
  // an ES cluster ranks the buckets itself before they are merged
  if (!sessionsImpls[node]) { DbESImpl.rankEmptyMetricsLast(body.aggregations ?? body.aggs); }

  let outstanding = 0;
  let finished = 0;
  let outerErr = null;

  function convert (qParent, obj) {
    for (const item in obj) {
      doProcess(qParent, obj, item);
    }
  }

  function doProcess (qParent, obj, item) {
    let query;

    if (item === 'index' && obj[item] === 'arkime_lookups') {
      obj[item] = `${node2Prefix(node)}lookups`;
    } else if (item === 'fileand' && typeof obj[item] === 'string') {
      const qName = obj.fileand;
      delete obj.fileand;
      outstanding++;

      if (qName[0] === '/' && qName[qName.length - 1] === '/') {
        query = { query: { regexp: { name: qName.substring(1, qName.length - 1) } } };
      } else if (qName.indexOf('*') !== -1) {
        query = { query: { wildcard: { name: qName } } };
      } else {
        query = { query: { term: { name: qName } } };
      }
      // A failed lookup fails the cluster, no files inside a negated file term would match everything
      const filesDone = (err, files) => {
        outstanding--;
        outerErr ??= err;
        obj.bool = { should: [] };
        for (const file of files) {
          obj.bool.should.push({ bool: { filter: [{ term: { node: file.node } }, { term: { fileId: file.num } }] } });
        }
        if (obj.bool.should.length === 0) {
          // An empty should matches everything, no files must match nothing
          obj.bool = { must_not: [{ match_all: {} }] };
        }
        if (finished && outstanding === 0) {
          doneCb(outerErr, body);
        }
      };
      if (clusterDbs[node]?.conn) {
        clusterDbs[node].conn.collection('files', DbClusterImpl.collectionOptions('files')).find({ where: ESConnection.decompile(query.query), size: 500 })
          .then(({ data }) => filesDone(null, data.map(h => h.doc)))
          .catch((err) => {
            console.log(`ERROR - multies cluster db files lookup for cluster '${node2Name(node)}':`, ArkimeUtil.sanitizeLine(err.message));
            filesDone(err, []);
          });
      } else {
        (clusterDbs[node]?.client ?? clients[node]).search({ index: node2ClusterPrefix(node) + 'files', size: 500, body: query }, (err, result) => {
          if (err) {
            console.log(`ERROR - multies files lookup for cluster '${node2Name(node)}':`, ArkimeUtil.sanitizeLine(err.message));
            return filesDone(err, []);
          }
          filesDone(null, result.body.hits.hits.map(h => h._source));
        });
      }
    } else if (typeof obj[item] === 'object') {
      convert(obj, obj[item]);
    }
  }

  convert(null, body);
  if (outstanding === 0) {
    return doneCb(outerErr, body);
  }

  finished = 1;
}

function combineResults (obj, result) {
  if (!result.hits) {
    console.log('NO RESULTS', result);
    return;
  }

  obj.hits.total += result.hits.total;
  obj.hits.missing += result.hits.missing;
  obj.hits.other += result.hits.other;
  if (result.hits.hits) {
    const hits = result.hits.hits;
    for (let i = 0; i < hits.length; i++) {
      hits[i].cluster = result.cluster;
      if (hits[i]._source) {
        hits[i]._source.cluster = result.cluster;
      }
      if (hits[i].fields) {
        hits[i].fields.cluster = result.cluster;
      }
    }
    obj.hits.hits = obj.hits.hits.concat(result.hits.hits);
  }
  if (obj.facets) {
    facetConvert2Obj(result.facets);
    facetAdd(obj.facets, result.facets);
  }

  if (obj.aggregations && result.aggregations) {
    aggConvert2Obj(result.aggregations);
    aggAdd(obj.aggregations, result.aggregations);
  }
}

function sortResultsAndTruncate (search, obj) {
  // Resort items
  if (search.sort && search.sort.length > 0) {
    const sortorder = [];
    for (let i = 0; i < search.sort.length; i++) {
      sortorder[i] = search.sort[i][Object.keys(search.sort[i])[0]].order === 'asc' ? 1 : -1;
    }

    obj.hits.hits = obj.hits.hits.sort((a, b) => {
      for (let i = 0; i < a.sort.length; i++) {
        if (a.sort[i] === b.sort[i]) {
          continue;
        }
        if (typeof a.sort[i] === 'string') {
          return sortorder[i] * a.sort[i].localeCompare(b.sort[i]);
        } else {
          return sortorder[i] * (a.sort[i] - b.sort[i]);
        }
      }
      return 0;
    });
  }

  if (search.size) {
    const from = +search.from || 0;
    obj.hits.hits = obj.hits.hits.slice(from, from + (+search.size));
  }
}
function newResult (search) {
  const result = { hits: { hits: [], total: 0 } };
  if (search.facets) {
    result.facets = {};
    for (const facet in search.facets) {
      if (search.facets[facet].histogram) {
        result.facets[facet] = { entries: [], _type: 'histogram', total: 0, other: 0, missing: 0 };
      } else {
        result.facets[facet] = { terms: [], _type: 'terms', total: 0, other: 0, missing: 0 };
      }
    }
  }
  if (search.aggregations) {
    result.aggregations = newAggregations(search.aggregations);
  }
  return result;
}

app.post(['/MULTIPREFIX_fields/field/_search', '/MULTIPREFIX_fields/_search'], clusterDbOp('search'), function (req, res) {
  simpleGather(req, res, null, (err, results) => {
    const obj = {
      hits: {
        total: 0,
        hits: []
      }
    };

    const unique = {};
    for (let i = 0; i < results.length; i++) {
      const result = results[i];

      if (result.error) {
        console.log('ERROR - POST /fields/field/_search', result.error);
        return res.send(obj);
      }

      for (let h = 0; h < result.hits.hits.length; h++) {
        const hit = result.hits.hits[h];
        if (!unique[hit._id]) {
          unique[hit._id] = 1;
          obj.hits.total++;
          obj.hits.hits.push(hit);
        }
      }
    }

    const fieldsMap = {};
    for (const { _source: f } of obj.hits.hits) {
      for (const dbName of [f?.fieldECS, f?.dbField2, f?.dbField]) { if (dbName) { fieldsMap[dbName] = f; } }
    }
    sessionsFieldsMap = fieldsMap;

    // Add the cluster field definition for multi-viewer
    if (!unique.cluster) {
      obj.hits.total++;
      obj.hits.hits.push({
        _index: 'fields_v30',
        _id: 'cluster',
        _source: {
          friendlyName: 'Cluster',
          group: 'general',
          help: 'Cluster name the session was recorded on (multi-viewer only)',
          type: 'termfield',
          dbField2: 'cluster'
        }
      });
    }

    res.send(obj);
  });
});

app.post(['/:index/:type/_search', '/:index/_search'], clusterDbOp('search'), function (req, res) {
  const bodies = {};
  const preFetched = {};
  let search;
  try {
    search = JSON.parse(req.body);
  } catch (e) {
    return res.status(400).send({ error: 'Invalid JSON body' });
  }

  if (+search.size + (+search.from || 0) > 10000) {
    req.arkime_need_to_scroll = true;
  }
  // console.log("DEBUG - INCOMING SEARCH", JSON.stringify(search, null, 2));
  let cluster = null;
  if (search.cluster) {
    req.query.cluster = search.cluster;
    delete search.cluster;
    req.body = JSON.stringify(search);
  }

  if (req.query.cluster) {
    cluster = Array.isArray(req.query.cluster) ? req.query.cluster : req.query.cluster.split(',');
  }

  const indexParam = req.params.index;
  const sessionsRoute = isSessionsIndex(indexParam);
  const activeNodes = getActiveNodes(cluster);
  async.each(activeNodes, (node, asyncCb) => {
    fixQuery(node, req.body, (err, body) => {
      // console.log("DEBUG - OUTGOING SEARCH", node, JSON.stringify(body, null, 2));
      if (err) {
        preFetched[node] = { arkime_failed: true, error: err.message };
        return asyncCb(null);
      }
      if (sessionsImpls[node] && sessionsRoute) {
        sessionsImpls[node].searchSessions(toNodePrefix(indexParam, node), body, {})
          .then((result) => {
            preFetched[node] = result;
            asyncCb(null);
          })
          .catch((err) => {
            console.log(`multies sessions search failed for cluster '${node2Name(node)}':`, ArkimeUtil.sanitizeLine(err.message));
            preFetched[node] = { hits: { total: 0, hits: [] }, error: err.message };
            asyncCb(null);
          });
        return;
      }
      bodies[node] = JSON.stringify(body);
      asyncCb(null);
    });
  }, (err) => {
    simpleGather(req, res, bodies, (err, results) => {
      const obj = newResult(search);

      for (let i = 0; i < results.length; i++) {
        combineResults(obj, results[i]);
      }

      if (obj.facets) {
        facetConvert2Arr(obj.facets);
      }

      if (obj.aggregations) {
        aggConvert2Arr(obj.aggregations);
      }

      sortResultsAndTruncate(search, obj);

      res.send(obj);
    }, preFetched);
  });
});

function msearch (req, res) {
  const lines = req.body.split(/[\r\n]/);
  const bodies = {};
  const preFetched = {};
  const activeNodes = getActiveNodes();
  async.each(activeNodes, (node, nodeCb) => {
    if (sessionsImpls[node]) {
      const pairs = []; // {header, query}
      let allSessions = true;
      for (let i = 0; i + 1 < lines.length; i += 2) {
        if (lines[i] === '' && lines[i + 1] === '') { continue; }
        let header;
        try { header = lines[i] === '{}' || lines[i] === '' ? {} : JSON.parse(lines[i]); } catch (e) { allSessions = false; break; }
        const idx = Array.isArray(header.index) ? header.index[0] : header.index;
        if (!isSessionsIndex(idx)) { allSessions = false; break; }
        pairs.push({ header, query: lines[i + 1] });
      }
      if (allSessions && pairs.length) {
        return Promise.all(pairs.map((pair) => new Promise((resolve, reject) => {
          fixQuery(node, pair.query, (err, fixed) => {
            if (err) { return reject(err); }
            const idx = Array.isArray(pair.header.index) ? pair.header.index[0] : pair.header.index;
            sessionsImpls[node].searchSessions(toNodePrefix(idx, node), fixed, {})
              .then(resolve)
              .catch((err2) => {
                console.log(`multies sessions msearch failed for cluster '${node2Name(node)}':`, ArkimeUtil.sanitizeLine(err2.message));
                resolve({ hits: { total: 0, hits: [] }, error: err2.message });
              });
          });
        }))).then((responses) => {
          preFetched[node] = { responses };
        }).catch((err) => {
          preFetched[node] = { arkime_failed: true, error: err.message };
        }).then(() => nodeCb());
      }
    }
    const nlines = [];
    async.eachSeries(lines, (line, lineCb) => {
      if (line === '{}' || line === '') {
        nlines.push('{}');
        return lineCb();
      }
      fixQuery(node, line, (err, body) => {
        nlines.push(JSON.stringify(body));
        lineCb(err);
      });
    }, (err) => {
      if (err) {
        preFetched[node] = { arkime_failed: true, error: err.message };
        return nodeCb();
      }
      bodies[node] = nlines.join('\n') + '\n';
      bodies[node] = toNodePrefix(bodies[node], node);
      nodeCb();
    });
  }, (err) => {
    simpleGather(req, res, bodies, (err, results) => {
      // no cluster answered (unknown cluster, or all of them down)
      if (err || !Array.isArray(results[0]?.responses)) {
        return res.status(500).send({ error: 'No cluster answered the msearch' });
      }
      const obj = { responses: [] };
      for (let h = 0; h < results[0].responses.length; h++) {
        obj.responses[h] = newResult(JSON.parse(lines[h * 2 + 1]));

        for (let i = 0; i < results.length; i++) {
          combineResults(obj.responses[h], results[i].responses[h]);
        }

        if (obj.responses[h].facets) {
          facetConvert2Arr(obj.responses[h].facets);
        }

        if (obj.responses[h].aggregations) {
          aggConvert2Arr(obj.responses[h].aggregations);
        }

        sortResultsAndTruncate(JSON.parse(lines[h * 2 + 1]), obj.responses[h]);
      }

      res.send(obj);
    }, preFetched);
  });
}

app.post(['/:index/:type/:id/_update', '/:index/_update/:id'], async (req, res) => {
  let body;
  try {
    body = JSON.parse(req.body);
  } catch (e) {
    return res.status(400).send({ error: 'Invalid JSON body' });
  }
  const cluster = req.query.cluster ?? body.cluster;
  if (cluster && clusters[cluster]) {
    const node = clusters[cluster];
    delete body.cluster;

    const index = toNodePrefix(req.params.index, node);
    const id = req.params.id;

    if (sessionsImpls[node] && isSessionsIndex(req.params.index)) {
      try {
        const result = await sessionsImpls[node].updateSession(index, id, body);
        return res.send(result);
      } catch (err) {
        console.log('ERROR - CH /%s/%s/%s/_update', ArkimeUtil.sanitizeStr(req.params.index), ArkimeUtil.sanitizeStr(req.params.type), ArkimeUtil.sanitizeStr(req.params.id), err);
        return res.send({});
      }
    }

    const collName = clusterDbs[node]?.conn && MultiesClusterDb.collectionName(req.path);
    if (collName) {
      try {
        const result = await MultiesClusterDb.answer(clusterDbs[node].conn.collection(collName, DbClusterImpl.collectionOptions(collName)), 'update', id, req.query, JSON.stringify(body));
        return res.status(result.status ?? 200).send(result);
      } catch (err) {
        console.log('ERROR - cluster db /%s/_update/%s', ArkimeUtil.sanitizeStr(req.params.index), ArkimeUtil.sanitizeStr(req.params.id), ArkimeUtil.sanitizeLine(err.message));
        return res.status(400).send({ error: err.message });
      }
    }

    const params = {
      retry_on_conflict: 3,
      index,
      body,
      id,
      timeout: '10m'
    };

    try {
      const client = clusterDbs[node]?.client && isClusterDbPath(req.path) ? clusterDbs[node].client : clients[node];
      const { body: result } = await client.update(params);
      return res.send(result);
    } catch (err) {
      console.log('ERROR - /%s/%s/%s/_update', ArkimeUtil.sanitizeStr(req.params.index), ArkimeUtil.sanitizeStr(req.params.type), ArkimeUtil.sanitizeStr(req.params.id), err);
      return res.send({});
    }
  } else {
    console.log('ERROR - body of the request does not contain cluster field', req.method, req.url, req.body);
    return res.end();
  }
});

app.post(['/:index/history', '/*history*/_doc'], clusterDbOp('history'), simpleGatherFirst);

app.post('/:index/:type/_msearch', msearch);
app.post('/_msearch', msearch);

app.get('/:index/_count', clusterDbOp('count'), sessionsCount);
app.post('/:index/_count', clusterDbOp('count'), sessionsCount);
app.get('/:index/:type/_count', clusterDbOp('count'), sessionsCount);
app.post('/:index/:type/_count', clusterDbOp('count'), sessionsCount);

app.post('/_cache/clear', simpleGather1Cluster);
app.post('/_cluster/reroute', simpleGather1Cluster);
app.get('/MULTIPREFIX_*/_flush', clusterDbsToo('flush'), clusterDbOp('refresh'), simpleGather1Cluster);
app.get('/MULTIPREFIX_*/_refresh', clusterDbsToo('refresh'), clusterDbOp('refresh'), simpleGather1Cluster);

app.post('/MULTIPREFIX_hunts/_doc', clusterDbOp('index'), simpleGather1Cluster);
app.put('/MULTIPREFIX_hunts/_doc/:id', clusterDbOp('index'), simpleGather1Cluster);

if (ArkimeConfig.regressionTests) {
  app.post('/regressionTests/shutdown', function (req, res) {
    process.exit(0);
  });
}

app.get(/./, function (req, res) {
  simpleGather(req, res, null, (err, results) => {
    console.log('UNKNOWN', req.method, req.url, results);
  });
});

app.post(/./, function (req, res) {
  console.log('UNKNOWN', req.method, req.url, req.body);
});

app.put(/./, function (req, res) {
  console.log('UNKNOWN', req.method, req.url, req.body);
});

// Replace the default express error handler
app.use(ArkimeUtil.expressErrorHandler);

/// ///////////////////////////////////////////////////////////////////////////////
/// / Main
/// ///////////////////////////////////////////////////////////////////////////////

// Ping (HEAD /) periodically to maintain a list of active ES nodes
function pingESNode (client, node) {
  return new Promise((resolve, reject) => {
    if (!client) { return resolve({ isActive: true, node }); }
    client.ping({}, {
      requestTimeout: 3 * 1000 // ping usually has a 3000ms timeout
    }, function (error, { body: response }) {
      resolve({ isActive: !error, node });
    });
  });
}

function enumerateActiveNodes () {
  const pingTasks = [];
  for (let i = 0; i < nodes.length; i++) {
    pingTasks.push(pingESNode(clients[nodes[i]], nodes[i]));
  }
  Promise.all(pingTasks).then(function (values) {
    const activeNodes = [];
    for (let i = 0; i < values.length; i++) {
      if (values[i].isActive) { // true -> node is active
        activeNodes.push(values[i].node);
      } else { // false -> node is down
        // remove credential from the url
        let host = values[i].node.split('://');
        host = host[host.length > 1 ? 1 : 0].split('@'); // user:pass@elastic.com:9200
        host = host[host.length > 1 ? host.length - 1 : 0];
        console.log('OpenSearch/Elasticsearch is down at ', host);
      }
    }
    activeESNodes = activeNodes.slice();
  });
}

setInterval(enumerateActiveNodes, 1 * 60 * 1000); // 1*60*1000 ms

async function premain () {
  await Config.initialize();

  nodes = Config.getArray('multiESNodes', '', ';');
  if (nodes.length === 0 || nodes[0] === '') {
    console.log('ERROR - Empty multiESNodes');
    process.exit(1);
  }

  for (let i = 0; i < nodes.length; i++) {
    const nodeName = node2Name(nodes[i]);
    if (!nodeName) {
      console.log('ERROR - name is missing in multiESNodes for', node2Safe(nodes[i]), 'Set node name as multiESNodes=http://example1:9200,name:<friendly-name-1>;http://example2:9200,name:<friendly-name-2>');
      process.exit(1);
    }
    clusterList[i] = nodeName; // name

    // Maintain a mapping of node to cluster and cluster to node
    clusters[nodes[i]] = clusterList[i]; // node -> cluster
    clusters[clusterList[i]] = nodes[i]; // cluster -> node
  }

  // First connect
  for (const node of nodes) {
    if (node.toLowerCase().includes(',http')) {
      console.log('WARNING - multiESNodes may be using a comma as a host delimiter, change to semicolon');
    }

    const esNode = node2Url(node);

    const esClientOptions = {
      node: esNode,
      requestTimeout: 300000,
      maxRetries: 2,
      ssl: esSSLOptions
    };

    const nodeName = node2Name(node);
    const esAPIKey = node2ESAPIKey(node);
    let esBasicAuth = node2ESBasicAuth(node);
    if (esAPIKey) {
      esClientOptions.auth = {
        apiKey: esAPIKey
      };
      authHeader[nodeName] = `ApiKey ${esAPIKey}`;
    } else if (esBasicAuth) {
      if (!esBasicAuth.includes(':')) {
        esBasicAuth = Buffer.from(esBasicAuth, 'base64').toString();
      }
      esBasicAuth = ArkimeUtil.splitRemain(esBasicAuth, ':', 1);
      esClientOptions.auth = {
        username: esBasicAuth[0],
        password: esBasicAuth[1]
      };
      const b64 = Buffer.from(esBasicAuth.join(':')).toString('base64');
      authHeader[nodeName] = `Basic ${b64}`;
    }

    if (node2HasEs(node)) {
      clients[node] = new Client(esClientOptions);
    }

    const sessionsUrl = node2SessionsDbUrl(node);
    const SessionsImpl = ArkimeDb.sessionsImpl(sessionsUrl);
    if (!SessionsImpl && !ArkimeConfig.isEsUrl(sessionsUrl)) {
      console.log(`ERROR - multiESNodes sessions db for cluster '${nodeName}' can't be '${sessionsUrl.split(':')[0]}', set sessionsDbUrl to OpenSearch/Elasticsearch, ClickHouse or a dbPlugins sessions scheme`);
      process.exit(1);
    }
    if (SessionsImpl) {
      try {
        sessionsImpls[node] = new SessionsImpl({ sessionsDbUrl: sessionsUrl, prefix: ArkimeUtil.formatPrefix(node2SessionsPrefix(node)), insecure: ArkimeConfig.isInsecure([sessionsUrl]), caTrustFile: ArkimeConfig.get('caTrustFile'), fieldsMap: () => sessionsFieldsMap, EsQuery: require('./esQuery') });
        console.log(`multies: ${sessionsUrl.split(':')[0]} sessions backend for cluster '${nodeName}'`);
      } catch (err) {
        console.log(`multies: failed to create the sessions backend for cluster '${nodeName}':`, ArkimeUtil.sanitizeLine(err.message));
        process.exit(1);
      }
    }

    // The cluster's own ES is used unless its dbUrl keeps capture's metadata, hunts and history elsewhere
    let clusterDbUrl = node2DbUrl(node, 'dbUrl');
    if (clusterDbUrl && ArkimeConfig.isEsUrl(clusterDbUrl) && ArkimeConfig.esUrl(clusterDbUrl).replace(/\/+$/, '') === node2Url(node)) {
      clusterDbUrl = null;
    }
    if (clusterDbUrl) {
      if (ArkimeConfig.isEsUrl(clusterDbUrl)) {
        // Another OpenSearch/Elasticsearch with the cluster's credentials, its requests pass straight through
        const esUrl = ArkimeConfig.esUrl(clusterDbUrl).replace(/\/+$/, '');
        clusterDbs[node] = { esUrl, client: new Client({ ...esClientOptions, node: esUrl }) };
      } else if (ArkimeUtil.dbHoldsCluster(clusterDbUrl)) {
        clusterDbs[node] = { conn: ArkimeUtil.dbConnect(clusterDbUrl, { prefix: node2ClusterPrefix(node) }) };
        await DbClusterImpl.loadBaseFields(clusterDbs[node].conn.collection('fields'));
      } else {
        console.log(`ERROR - multiESNodes dbUrl for cluster '${nodeName}' must be OpenSearch/Elasticsearch, postgres:// or a dbPlugins cluster scheme, set sessionsDbUrl too for a sessions only db like ClickHouse`);
        process.exit(1);
      }
      console.log(`multies: ${clusterDbUrl.split(':')[0]} cluster db for cluster '${nodeName}'`);
    }
  }

  // Now check version numbers
  nodes.forEach(async (node) => {
    if (!clients[node]) { return; }
    try {
      const { body: data } = await clients[node].info();

      if (data.version.distribution === 'opensearch') {
        if (data.version.number.match(/^[0]/)) {
          console.log(`ERROR - OpenSearch ${data.version.number} not supported, OpenSearch 1.0.0 or later required.`);
          process.exit(1);
        }
      } else {
        if (data.version.number.match(/^([0-6]|7\.[0-9]\.)/)) {
          console.log(`ERROR - ES ${data.version.number} not supported, ES 7.10.0 or later required.`);
          process.exit(1);
        }
      }
    } catch (err) {
      console.log(err);
    }
  });

  // list of active nodes
  activeESNodes = nodes.slice();
  console.log(nodes.map(node2Safe));

  const multiESHost = Config.get('multiESHost', '127.0.0.1');
  if (!/^(127\.\d+\.\d+\.\d+|::1|localhost)$/.test(multiESHost)) {
    console.log(`WARNING - multies is listening on ${multiESHost}, not localhost${multiESBasicAuthHmac ? '' : ', and multiESBasicAuth is not set, so anything that can reach it can read and change every cluster'}`);
  }
  ArkimeUtil.createHttpServer(app, multiESHost, Config.get('multiESPort', 8200));
}

premain();
