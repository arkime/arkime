/******************************************************************************/
/* multies.js  -- Make multiple ES servers look like one but merging results
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

/// / Modules
/// ///////////////////////////////////////////////////////////////////////////////
const Config = require('./config.js');
const express = require('express');
const async = require('async');
const { Client } = require('@elastic/elasticsearch');
const http = require('http');
const https = require('https');
const fs = require('fs');
const path = require('path');
const ArkimeUtil = require('../common/arkimeUtil');

const esSSLOptions = { rejectUnauthorized: !Config.insecure, ca: Config.getCaTrustCerts(Config.nodeName()) };
const esClientKey = Config.get('esClientKey');
const esClientCert = Config.get('esClientCert');
if (esClientKey) {
  esSSLOptions.key = fs.readFileSync(esClientKey);
  esSSLOptions.cert = fs.readFileSync(esClientCert);
  const esClientKeyPass = Config.get('esClientKeyPass');
  if (esClientKeyPass) {
    esSSLOptions.passphrase = esClientKeyPass;
  }
}

const clients = {};
let nodes = [];
const clusters = {};
const clusterList = [];
const authHeader = {};
let activeESNodes = [];
const httpAgent = new http.Agent({ keepAlive: true, keepAliveMsecs: 5000, maxSockets: 100 });
const httpsAgent = new https.Agent(Object.assign({ keepAlive: true, keepAliveMsecs: 5000, maxSockets: 100 }, esSSLOptions));

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
const logger = require('morgan');
const favicon = require('serve-favicon');
const compression = require('compression');

const app = express();
app.enable('jsonp callback');
app.use(favicon(path.join(__dirname, '/public/favicon.ico')));
app.use(logger(':date \x1b[1m:method\x1b[0m \x1b[33m:url\x1b[0m :res[content-length] bytes :response-time ms'));
app.use(saveBody);
app.use(compression());
app.use(function (req, res, next) {
  if (res.setTimeout) {
    res.setTimeout(10 * 60 * 1000); // Increase default from 2 min to 10 min
    return next();
  }
});

function node2Url (node) {
  const url = node.split(',')[0];
  if (url.match(/^http/)) { return url; }
  return 'http://' + url;
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
    const kv = parts[p].split(':');
    if (kv[0] === 'name') {
      return kv[1];
    }
  }
  return null;
}

function node2ESAPIKey (node) {
  const parts = node.split(',');
  for (let p = 1; p < parts.length; p++) {
    const kv = parts[p].split(':');
    if (kv[0] === 'elasticsearchAPIKey') {
      return kv[1];
    }
  }
  return null;
}

function node2ESBasicAuth (node) {
  const parts = node.split(',');
  for (let p = 1; p < parts.length; p++) {
    const kv = parts[p].split(':');
    if (kv[0] === 'elasticsearchBasicAuth') {
      return kv[1];
    }
  }
  return null;
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
    const esNodes = [];
    activeESNodes.slice().forEach((node) => {
      if (tmpNodes.includes(node)) {
        esNodes.push(node);
      }
    });
    return esNodes;
  } else {
    return activeESNodes.slice();
  }
}

function makeRequest (url, options, cb) {
  let result = '';

  const preq = options.arkime_client.request(url, options, (pres) => {
    pres.on('data', (chunk) => {
      result += chunk.toString();
    });
    pres.on('end', () => {
      if (result.length) {
        result = result.replace(new RegExp('(index":\\s*|[,{]|  )"' + options.arkime_prefix + '(sessions3|sessions2|stats|dstats|sequence|files|users|history)', 'g'), '$1"MULTIPREFIX_$2');
        result = result.replace(new RegExp('(index":\\s*)"' + options.arkime_prefix + '(fields_v[1-4][0-9]?)"', 'g'), '$1"MULTIPREFIX_$2"');
        result = JSON.parse(result);
      } else {
        result = {};
      }
      if (cb) {
        cb(null, result);
      }
    });
  });
  preq.setHeader('content-type', 'application/json');
  if (options.arkime_opaque !== undefined) {
    preq.setHeader('X-Opaque-Id', options.arkime_opaque);
  }
  if (options.arkime_body !== undefined) {
    if (options.method === 'DELETE') {
      preq.setHeader('content-length', options.arkime_body.length);
    }
    preq.end(options.arkime_body);
  }
  preq.on('error', (e) => {
    console.log('Request error', e);
  });
  preq.end();
}

function simpleGather (req, res, bodies, doneCb) {
  let cluster = null;
  if (req.query.cluster) {
    cluster = Array.isArray(req.query.cluster) ? req.query.cluster : req.query.cluster.split(',');
    req.url = req.url.replace(/cluster=[^&]*(&|$)/g, ''); // remove cluster from URL
    delete req.query.cluster;
  }
  const activeNodes = getActiveNodes(cluster);
  if (activeNodes.length === 0) { // Empty nodes. Either all clusters are down or invalid clusters
    return doneCb(true, [{}]);
  }
  async.map(activeNodes, (node, asyncCb) => {
    const nodeName = node2Name(node);
    let nodeUrl = node2Url(node) + req.url;

    const options = { method: req.method, arkime_opaque: req.headers['x-opaque-id'] };
    options.arkime_prefix = node2Prefix(node);
    nodeUrl = nodeUrl.replace(/MULTIPREFIX_/g, options.arkime_prefix).replace(/arkime_sessions2/g, 'sessions2');
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
      let body = JSON.parse(options.arkime_body);
      const totSize = body.size;
      body.size = 1000;
      options.arkime_body = JSON.stringify(body);

      // Add scroll parameter to url to turn on scrolling
      url.href += '&scroll=2m';

      let fullResults;
      makeRequest(url, options, function doResponse (err, result) {
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
        asyncCb(null, result);
      });
    }
  }, doneCb);
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

app.get('/_tasks', simpleGatherTasks);
app.post('/_tasks/:taskId/_cancel', simpleGatherFirst);

app.get('/_cluster/nodes/stats', simpleGatherNodes);
app.get('/_nodes', simpleGatherNodes);
app.get('/_nodes/stats', simpleGatherNodes);
app.get('/_nodes/stats/:kinds', simpleGatherNodes);
app.get('/_cluster/health', simpleGatherAdd);

app.get('/:index/_aliases', simpleGatherNodes);
app.get('/:index/_alias', simpleGatherNodes);

app.get('/MULTIPREFIX_sessions*/_refresh', (req, res) => {
  req.url = '/sessions*/_refresh';
  return simpleGatherFirst(req, res);
});

app.get('/_cluster/:type/details', function (req, res) {
  const result = { available: [], active: [], inactive: [] };
  const activeNodes = getActiveNodes();
  for (let i = 0; i < clusterList.length; i++) {
    result.available.push(clusterList[i]);
    if (activeNodes.includes(clusters[clusterList[i]])) {
      result.active.push(clusterList[i]);
    } else {
      result.inactive.push(clusterList[i]);
    }
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

app.get('/_template/MULTIPREFIX_sessions2_template', (req, res) => {
  simpleGather(req, res, null, (err, results) => {
    // console.log("DEBUG -", JSON.stringify(results, null, 2));

    let obj = results[0];
    for (let i = 1; i < results.length; i++) {
      if (results[i].MULTIPREFIX_sessions2_template &&
          results[i].MULTIPREFIX_sessions2_template.mappings._meta.molochDbVersion < obj.MULTIPREFIX_sessions2_template.mappings._meta.molochDbVersion) {
        obj = results[i];
      }
    }
    res.send(obj);
  });
});

app.get('/_template/MULTIPREFIX_sessions3_template', (req, res) => {
  simpleGather(req, res, null, (err, results) => {
    // console.log("DEBUG -", JSON.stringify(results, null, 2));

    let obj = results[0];
    for (let i = 1; i < results.length; i++) {
      if (results[i].MULTIPREFIX_sessions3_template.mappings._meta.molochDbVersion < obj.MULTIPREFIX_sessions3_template.mappings._meta.molochDbVersion) {
        obj = results[i];
      }
    }
    res.send(obj);
  });
});

app.get(['/users/user/:user', '/users/_doc/:user'], async (req, res) => {
  try {
    const { body: user } = await clients[nodes[0]].get({
      index: 'users', type: '_doc', id: req.params.user
    });
    return res.send(user);
  } catch (err) {
    console.log(`ERROR - GET /users/user/${req.params.user}`, err);
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
    console.log(`ERROR - POST /users/user/${req.params.user}`, err);
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

app.get(['/:index/:type/_search', '/:index/_search'], (req, res) => {
  simpleGather(req, res, null, (err, results) => {
    const obj = results[0];
    for (let i = 1; i < results.length; i++) {
      if (results[i].error) {
        console.log('ERROR - GET _search', req.query.index, req.query.type, results[i].error);
      }
      obj.hits.total += results[i].hits.total;
      obj.hits.hits = obj.hits.hits.concat(results[i].hits.hits);
    }
    res.send(obj);
  });
});

app.get('/:index/:type/:id', function (req, res) {
  simpleGather(req, res, null, (err, results) => {
    for (let i = 0; i < results.length; i++) {
      if (results[i].found) {
        if (results[i]._source) {
          results[i]._source.cluster = results[i].cluster;
        }
        return res.send(results[i]);
      }
    }
    res.send(results[0]);
  });
});

app.get('/_cluster/settings', function (req, res) {
  res.send({ persistent: {}, transient: {} });
});

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

// Aggregations are a pain, we convert from array to map to back to array
/// ///////////////////////////////////////////////////////////////////////////////

function agg2Obj (field, agg) {
  const obj = {};
  for (let i = 0; i < agg.length; i++) {
    obj[agg[i][field]] = agg[i];
  }
  return obj;
}

function agg2Arr (agg, type) {
  let arr = [];
  for (const attrname in agg) {
    arr.push(agg[attrname]);
  }

  if (type === 'histogram') {
    arr = arr.sort((a, b) => { return a.key - b.key; });
  } else {
    arr = arr.sort((a, b) => {
      if (b.doc_count !== a.doc_count) {
        return b.doc_count - a.doc_count;
      }
      return a.key - b.key;
    });
  }
  return arr;
}

function aggConvert2Obj (aggs) {
  for (const aggname in aggs) {
    aggs[aggname].buckets = agg2Obj('key', aggs[aggname].buckets);
  }
}

function aggConvert2Arr (aggs) {
  for (const aggname in aggs) {
    aggs[aggname].buckets = agg2Arr(aggs[aggname].buckets, aggs[aggname]._type);
    delete aggs[aggname]._type;
  }
}

function aggAdd (obj1, obj2) {
  for (const aggname in obj2) {
    obj1[aggname].doc_count_error_upper_bound += obj2[aggname].doc_count_error_upper_bound;
    obj1[aggname].sum_other_doc_count += obj2[aggname].sum_other_doc_count;
    for (const entry in obj2[aggname].buckets) {
      if (!obj1[aggname].buckets[entry]) {
        obj1[aggname].buckets[entry] = obj2[aggname].buckets[entry];
      } else {
        const o1 = obj1[aggname].buckets[entry];
        const o2 = obj2[aggname].buckets[entry];

        o1.doc_count += o2.doc_count;
        if (o1.db) {
          o1.db.value += o2.db.value;
          o1.pa.value += o2.pa.value;
        }
      }
    }
  }
}

function fixQuery (node, body, doneCb) {
  body = JSON.parse(body);

  // Reset from & size since we do aggregation
  if (body.size) {
    body.size = (+body.size) + ((+body.from) || 0);
  }

  delete body.from;

  let outstanding = 0;
  let finished = 0;
  const err = null;

  function convert (qParent, obj) {
    for (const item in obj) {
      doProcess(qParent, obj, item);
    }
  };

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
      clients[node].search({ index: node2Prefix(node) + 'files', size: 500, body: query }, (err, { body: result }) => {
        outstanding--;
        obj.bool = { should: [] };
        result.hits.hits.forEach((file) => {
          obj.bool.should.push({ bool: { filter: [{ term: { node: file._source.node } }, { term: { fileId: file._source.num } }] } });
        });
        if (obj.bool.should.length === 0) {
          err = 'No matching files found';
        }
        if (finished && outstanding === 0) {
          doneCb(err, body);
        }
      });
    } else if (typeof obj[item] === 'object') {
      convert(obj, obj[item]);
    }
  }

  convert(null, body);
  if (outstanding === 0) {
    return doneCb(err, body);
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

  if (obj.aggregations) {
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
    result.aggregations = {};
    for (const agg in search.aggregations) {
      if (search.aggregations[agg].histogram) {
        result.aggregations[agg] = { buckets: {}, _type: 'histogram', doc_count_error_upper_bound: 0, sum_other_doc_count: 0 };
      } else {
        result.aggregations[agg] = { buckets: {}, _type: 'terms', doc_count_error_upper_bound: 0, sum_other_doc_count: 0 };
      }
    }
  }
  return result;
}

app.post(['/MULTIPREFIX_fields/field/_search', '/MULTIPREFIX_fields/_search'], function (req, res) {
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
        console.log('ERROR - GET /fields/field/_search', result.error);
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
    res.send(obj);
  });
});

app.post(['/:index/:type/_search', '/:index/_search'], function (req, res) {
  const bodies = {};
  const search = JSON.parse(req.body);

  if (+search.size + (+search.from || 0) > 10000) {
    req.arkime_need_to_scroll = true;
  }
  // console.log("DEBUG - INCOMING SEARCH", JSON.stringify(search, null, 2));
  const activeNodes = getActiveNodes();
  async.each(activeNodes, (node, asyncCb) => {
    fixQuery(node, req.body, (err, body) => {
      // console.log("DEBUG - OUTGOING SEARCH", node, JSON.stringify(body, null, 2));
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
    });
  });
});

function msearch (req, res) {
  const lines = req.body.split(/[\r\n]/);
  const bodies = {};
  const activeNodes = getActiveNodes();
  async.each(activeNodes, (node, nodeCb) => {
    const nlines = [];
    async.eachSeries(lines, (line, lineCb) => {
      if (line === '{}' || line === '') {
        nlines.push('{}');
        return lineCb();
      }
      fixQuery(node, line, (err, body) => {
        nlines.push(JSON.stringify(body));
        lineCb();
      });
    }, (err) => {
      bodies[node] = nlines.join('\n') + '\n';
      const prefix = node2Prefix(node);
      bodies[node] = bodies[node].replace(/MULTIPREFIX_/g, prefix).replace(/arkime_sessions2/g, 'sessions2');
      nodeCb();
    });
  }, (err) => {
    simpleGather(req, res, bodies, (err, results) => {
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
    });
  });
}

app.post(['/:index/:type/:id/_update', '/:index/_update/:id'], async (req, res) => {
  const body = JSON.parse(req.body);
  if (body.cluster && clusters[body.cluster]) {
    const node = clusters[body.cluster];
    delete body.cluster;

    const prefix = node2Prefix(node);
    const index = req.params.index.replace(/MULTIPREFIX_/g, prefix).replace(/arkime_sessions2/g, 'sessions2');
    const id = req.params.id;
    const params = {
      retry_on_conflict: 3,
      index,
      body,
      id,
      timeout: '10m'
    };

    try {
      const { body: result } = await clients[node].update(params);
      return res.send(result);
    } catch (err) {
      console.log(`ERROR - /${req.params.index}/${req.params.type}/${req.params.id}/_update`, err);
      return res.send({});
    }
  } else {
    console.log('ERROR - body of the request does not contain cluster field', req.method, req.url, req.body);
    return res.end();
  }
});

app.post(['/:index/history', '/*history*/_doc'], simpleGatherFirst);

app.post('/:index/:type/_msearch', msearch);
app.post('/_msearch', msearch);

app.get('/:index/_count', simpleGatherAdd);
app.post('/:index/_count', simpleGatherAdd);
app.get('/:index/:type/_count', simpleGatherAdd);
app.post('/:index/:type/_count', simpleGatherAdd);

if (Config.get('regressionTests')) {
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

// Replace the default express error handler
app.use(ArkimeUtil.expressErrorHandler);

/// ///////////////////////////////////////////////////////////////////////////////
/// / Main
/// ///////////////////////////////////////////////////////////////////////////////

nodes = Config.get('multiESNodes', '').split(';');
if (nodes.length === 0 || nodes[0] === '') {
  console.log('ERROR - Empty multiESNodes');
  process.exit(1);
}

for (let i = 0; i < nodes.length; i++) {
  const nodeName = node2Name(nodes[i]);
  if (!nodeName) {
    console.log('ERROR - name is missing in multiESNodes for', nodes[i], 'Set node name as multiESNodes=http://example1:9200,name:<friendly-name-11>;http://example2:9200,name:<friendly-name-2>');
    process.exit(1);
  }
  clusterList[i] = nodeName; // name

  // Maintain a mapping of node to cluster and cluster to node
  clusters[nodes[i]] = clusterList[i]; // node -> cluster
  clusters[clusterList[i]] = nodes[i]; // cluster -> node
}

// First connect
nodes.forEach((node) => {
  if (node.toLowerCase().includes(',http')) {
    console.log('WARNING - multiESNodes may be using a comma as a host delimiter, change to semicolon');
  }

  let esNode = node.split(',')[0];
  esNode = esNode.startsWith('http') ? esNode : `http://${esNode}`;

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
    esBasicAuth = esBasicAuth.split(':');
    esClientOptions.auth = {
      username: esBasicAuth[0],
      password: esBasicAuth[1]
    };
    const b64 = Buffer.from(esBasicAuth.join(':')).toString('base64');
    authHeader[nodeName] = `Basic ${b64}`;
  }

  clients[node] = new Client(esClientOptions);
});

// Now check version numbers
nodes.forEach(async (node) => {
  try {
    const { body: data } = await clients[node].info();

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
  } catch (err) {
    console.log(err);
  }
});

// list of active nodes
activeESNodes = nodes.slice();

// Ping (HEAD /) periodically to maintian a list of active ES nodes
function pingESNode (client, node) {
  return new Promise((resolve, reject) => {
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

console.log(nodes);

console.log('Listen on ', Config.get('multiESPort', '8200'));

if (Config.isHTTPS()) {
  const cryptoOption = require('crypto').constants.SSL_OP_NO_TLSv1;
  const server = https.createServer({
    key: Config.keyFileData,
    cert: Config.certFileData,
    secureOptions: cryptoOption
  }, app).listen(Config.get('multiESPort', '8200'), Config.get('multiESHost', undefined));
  Config.setServerToReloadCerts(server, cryptoOption);
} else {
  http.createServer(app).listen(Config.get('multiESPort', '8200'), Config.get('multiESHost', undefined));
}
