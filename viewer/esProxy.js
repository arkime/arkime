/******************************************************************************/
/* esProxy.js  -- A security ES proxy that sensor nodes can be pointed to that
 *                only allows required ES calls for a sensor only machine. It can
 *                also check ips and passwords. You must be using central viewers
 *                that do NOT talk to the proxy, and talk to ES directly.
 *
 * Copyright 2020 AOL Inc. All rights reserved.
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
const Config = require('./config.js');
const express = require('express');
const http = require('http');
const https = require('https');
const fs = require('fs');
const basicAuth = require('basic-auth');

// express app
const app = express();

// ============================================================================
// Config
// ============================================================================

const elasticsearch = Config.get('elasticsearch');
const sensors = Config.configMap('esproxy-sensors');
console.log('sensors', sensors);

for (const sensor in sensors) {
  sensors[sensor].node = sensor;
  if (sensors[sensor].ip) {
    sensors[sensor].ip = sensors[sensor].ip.split(',');
  }
}
let prefix = Config.get('prefix', 'arkime_');
if (prefix !== '' && prefix.charAt(prefix.length - 1) !== '_') {
  prefix += '_';
}

const oldprefix = prefix === 'arkime_' ? '' : prefix;

console.log(`PREFIX: ${prefix} OLDPREFIX: ${oldprefix}`);

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

const esAPIKey = Config.get('elasticsearchAPIKey');
const esBasicAuth = Config.get('elasticsearchBasicAuth');

let authHeader;
if (esAPIKey) {
  authHeader = `ApiKey ${esAPIKey}`;
} else if (esBasicAuth) {
  if (!esBasicAuth.includes(':')) {
    authHeader = `Basic ${esBasicAuth}`;
  } else {
    authHeader = `Basic ${Buffer.from(esBasicAuth, 'base64').toString()}`;
  }
}

// ============================================================================
// Optional tee stuff
const elasticsearchTee = Config.sectionGet('tee', 'elasticsearch');
const esAPIKeyTee = Config.sectionGet('tee', 'elasticsearchAPIKey');
const esBasicAuthTee = Config.sectionGet('tee', 'elasticsearchBasicAuth');

let authHeaderTee;
if (esAPIKeyTee) {
  authHeaderTee = `ApiKey ${esAPIKeyTee}`;
} else if (esBasicAuthTee) {
  if (!esBasicAuthTee.includes(':')) {
    authHeaderTee = `Basic ${esBasicAuthTee}`;
  } else {
    authHeaderTee = `Basic ${Buffer.from(esBasicAuthTee, 'base64').toString()}`;
  }
}

// ============================================================================
// GET calls we can match exactly
const getExact = {
  '/': 1,
  '/_cat/health': 1,
  '/_cluster/health': 1,
  '/_refresh': 1,
  '/_nodes/stats/jvm,process,fs,os,indices,thread_pool': 1
};
getExact[`/_template/${prefix}sessions3_template`] = 1;
getExact[`/_template/${oldprefix}sessions2_template`] = 1;
getExact[`/${oldprefix}sessions2-*/_alias`] = 1;
getExact[`/${prefix}sessions3-*/_alias`] = 1;
getExact[`/${oldprefix}sessions2-*,${prefix}sessions3-*/_alias`] = 1;
getExact[`/${prefix}stats/_stats`] = 1;
getExact[`/${prefix}users/_stats`] = 1;
getExact[`/${prefix}users/_count`] = 1;
getExact[`/${prefix}sequence/_stats`] = 1;
getExact[`/${prefix}dstats/_stats`] = 1;
getExact[`/${prefix}files/_stats`] = 1;
getExact[`/${prefix}fields/_search`] = 1;

// POST calls we can match exactly
const postExact = {
};
postExact[`/${prefix}stats/_search`] = 1;
postExact[`/${prefix}fields/_search`] = 1;
postExact[`/${prefix}lookups/_search`] = 1;

// PUT calls we can match exactly
const putExact = {
};

// ============================================================================
// Auth
// ===========================================================================

app.use((req, res, next) => {
  const credentials = basicAuth(req);
  if (!credentials) {
    return res.set('WWW-Authenticate', 'Basic').status(401).send();
  }

  if (!sensors[credentials.name]) {
    console.log(`Unknown sensor ${credentials.name}`);
    return res.set('WWW-Authenticate', 'Basic').status(401).send();
  }

  if (sensors[credentials.name].pass !== undefined && sensors[credentials.name].pass !== credentials.pass) {
    console.log(`Incorrect password for ${credentials.name}`);
    return res.set('WWW-Authenticate', 'Basic').status(401).send();
  }

  req.sensor = sensors[credentials.name];
  if (!sensors[credentials.name].ip) {
    return next();
  }

  let ip = req.connection.remoteAddress;
  if (ip.startsWith('::ffff:')) {
    ip = ip.substring(7);
  }

  if (!req.sensor.ip.includes(ip)) {
    console.log(`Incorrect source ip ${ip} for ${credentials.name}`);
    return res.set('WWW-Authenticate', 'Basic').status(401).send();
  }

  return next();
});

// ============================================================================
// Proxy code to real ES
// ===========================================================================

// Save the post body

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
  const buf = Buffer.alloc(parseInt(req.headers['content-length'] || '1024'));
  let pos = 0;
  req.on('data', (chunk) => { chunk.copy(buf, pos); pos += chunk.length; });
  req.on('end', () => {
    req.body = buf;
    next();
  });
}

// Proxy

function doProxyFull (config, req, res) {
  let result = '';
  const esUrl = config.elasticsearch + req.url;
  console.log(`URL ${req.method} ${esUrl}`);
  const url = new URL(esUrl);
  const options = { method: req.method };
  let client;
  if (esUrl.match(/^https:/)) {
    options.agent = httpsAgent;
    client = https;
  } else {
    options.agent = httpAgent;
    client = http;
  }

  if (config.authHeader) {
    options.headers = {
      Authorization: config.authHeader
    };
  }

  const preq = client.request(url, options, (pres) => {
    pres.on('data', (chunk) => {
      result += chunk.toString();
    });
    pres.on('end', () => {
      res.setHeader('content-type', 'application/json');
      res.send(result);
    });
  });
  preq.setHeader('content-type', req.headers['content-type'] || 'application/json');

  // Copy these headers from original request to new request
  for (const header of ['x-opaque-id', 'content-encoding', 'content-length']) {
    if (req.headers[header] !== undefined) {
      preq.setHeader(header, req.headers[header]);
    }
  }

  preq.on('error', (e) => {
    console.log(`Request error ${url}`, e);
  });
  if (req._body) {
    preq.end(req.body);
  } else {
    preq.end('');
  }
}

function doProxy (req, res) {
  doProxyFull({ elasticsearch: elasticsearch, authHeader: authHeader }, req, res);
  if (elasticsearchTee) {
    doProxyFull({ elasticsearch: elasticsearchTee, authHeader: authHeaderTee }, req, {
      setHeader: () => {},
      send: () => {}
    });
  }
}

// ============================================================================
// handle the requests from the capture/viewer
// ===========================================================================

// Get requests
app.get('*', (req, res) => {
  const path = req.params['0'];

  // Empty IFs since those are allowed requests and will run code at end
  if (getExact[path]) {
  } else if (path.startsWith('/tagger')) {
  } else if (path.startsWith(`/${prefix}users/_doc/`)) {
  } else if (path.startsWith(`/${prefix}hunts/_doc/`)) {
  } else if (path === `/${prefix}sequence/_doc/fn-${req.sensor.node}`) {
  } else if (path === `/${prefix}stats/_doc/${req.sensor.node}`) {
  } else if (path.startsWith(`/${prefix}files/_doc/${req.sensor.node}`)) {
  } else if (path.match(/^\/[^/]*sessions2-[^/]+\/_doc\/[^/]+$/)) {
  } else if (path.match(/^\/[^/]*sessions3-[^/]+\/_doc\/[^/]+$/)) {
  } else {
    console.log(`GET failed node: ${req.sensor.node} path:${path}`);
    return res.status(400).send('Not authorized for API');
  }
  doProxy(req, res);
});

// Validate Bulk
// TODO - work with inflate/defate
function validateBulk (req) {
  if (!req._body) {
    return true;
  }

  const index = req.body.toString('utf8').match(/{"_index": *"[^"]*"}/g);
  for (const i in index) {
    if (!index[i].includes('sessions2') && !index[i].includes('sessions2')) {
      console.log(`Invalid index ${index[i]} for bulk`);
      return false;
    }
  }

  return true;
}

// Validate Files Search
function validateFilesSearch (req) {
  try {
    const json = JSON.parse(req.body.toString('utf8'));
    if (json.query.bool.filter) {
      return json.query.bool.filter[0].terms.node.includes(req.sensor.node);
    } else {
      return json.query.bool.must[0].terms.node.includes(req.sensor.node);
    }
  } catch (e) {
    return false;
  }
}

function validateSearchIds (req) {
  try {
    const json = JSON.parse(req.body.toString('utf8'));
    return json.query.ids.values.length === 1;
  } catch (e) {
    return false;
  }
}
function validateUpdate (req) {
  try {
    const json = JSON.parse(req.body.toString('utf8'));
    return json.script !== undefined && json.doc === undefined;
  } catch (e) {
    return false;
  }
}

// Post requests
app.post('*', saveBody, (req, res) => {
  const path = req.params['0'];

  // Empty IFs since those are allowed requests and will run code at end
  if (postExact[path]) {
  } else if (path.startsWith(`/${prefix}fields/_doc/`)) {
  } else if (path.startsWith('/tagger')) {
  } else if (path === `/${prefix}sequence/_doc/fn-${req.sensor.node}`) {
  } else if (path === `/${prefix}stats/_doc/${req.sensor.node}`) {
  } else if (path.startsWith(`/${prefix}dstats/_doc/${req.sensor.node}`)) {
  } else if (path.startsWith(`/${prefix}files/_doc/${req.sensor.node}`)) {
  } else if (path.startsWith('/_bulk') && validateBulk(req)) {
  } else if (path.startsWith(`/${prefix}files/_search`) && validateFilesSearch(req)) {
  } else if ((path.startsWith(`/${oldprefix}sessions2`) || path.startsWith(`/${prefix}sessions3`)) && path.endsWith('/_search') && validateSearchIds(req)) {
  } else if (path.match(/^\/[^/]*history_v[^/]*\/_doc$/)) {
  } else if (path.match(/^\/[^/]*sessions[23]-[^/]+\/_update\/[^/]+$/) && validateUpdate(req)) {
    console.log(`UPDATE : ${req.sensor.node} path:>${path}<:`);
    console.log(req.body.toString('utf8'));
  } else {
    console.log(`POST failed node: ${req.sensor.node} path:>${path}<:`);
    console.log(req.body.toString('utf8'));
    return res.status(400).send('Not authorized for API');
  }
  doProxy(req, res);
});

// Delete requests
app.delete('*', (req, res) => {
  const path = req.params['0'];

  // Empty IFs since those are allowed requests and will run code at end
  if (path.startsWith(`/${prefix}files/_doc/${req.sensor.node}-`)) {
  } else {
    console.log(`DELETE failed node: ${req.sensor.node} path:${path}`);
    return res.status(400).send('Not authorized for API');
  }
  doProxy(req, res);
});

// Put requests
app.put('*', (req, res) => {
  const path = req.params['0'];

  // Empty IFs since those are allowed requests and will run code at end
  if (putExact[path]) {
  } else {
    console.log(`PUT failed node: ${req.sensor.node} path:${path}`);
    return res.status(400).send('Not authorized for API');
  }
  doProxy(req, res);
});

// ============================================================================
// MAIN
// ===========================================================================
const httpAgent = new http.Agent({ keepAlive: true, keepAliveMsecs: 5000, maxSockets: 100 });
const httpsAgent = new https.Agent(Object.assign({ keepAlive: true, keepAliveMsecs: 5000, maxSockets: 100 }, esSSLOptions));

console.log('Listen on ', Config.get('esProxyPort', '7200'));
if (Config.isHTTPS()) {
  const cryptoOption = require('crypto').constants.SSL_OP_NO_TLSv1;
  const server = https.createServer({
    key: Config.keyFileData,
    cert: Config.certFileData,
    secureOptions: cryptoOption
  }, app).listen(Config.get('esProxyPort', '7200'), Config.get('esProxyHost', undefined));
  Config.setServerToReloadCerts(server, cryptoOption);
} else {
  http.createServer(app).listen(Config.get('esProxyPort', '7200'), Config.get('esProxyHost', undefined));
}
