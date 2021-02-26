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
let prefix = Config.get('prefix', '');
if (prefix !== '' && prefix.charAt(prefix.length - 1) !== '_') {
  prefix += '_';
}

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

// GET calls we can match exactly
const getExact = {
  '/': 1,
  '/_cat/health': 1,
  '/_cluster/health': 1,
  '/_nodes/stats/jvm,process,fs,os,indices,thread_pool': 1
};
getExact[`/_template/${prefix}sessions2_template`] = 1;
getExact[`/${prefix}sessions2-*/_alias`] = 1;
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
  const length = 'content-length' in req.headers && req.headers['content-length'] !== '0';
  return encoding || length;
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

function doProxy (req, res, cb) {
  let result = '';
  const esUrl = elasticsearch + req.url;
  console.log('URL', esUrl);
  const url = new URL(esUrl);
  const options = { method: req.method };
  let client;
  if (url.match(/^https:/)) {
    options.agent = httpsAgent;
    client = https;
  } else {
    options.agent = httpAgent;
    client = http;
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
  } else if (path === `/${prefix}sequence/_doc/fn-${req.sensor.node}`) {
  } else if (path === `/${prefix}stats/_doc/${req.sensor.node}`) {
  } else if (path.startsWith(`/${prefix}files/_doc/${req.sensor.node}`)) {
  } else if (path.match(/^\/[^/]*sessions2-[^/]+\/_doc\/[^/]+$/)) {
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
    if (!index[i].includes('sessions2')) {
      console.log(`Invalid index ${index[i]} for bulk`);
      return false;
    }
  }

  return true;
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
  } else if (path.startsWith('/_bulk')) {
    if (!validateBulk(req)) {
      console.log(`POST failed node: ${req.sensor.node} path:${path}`);
      return res.status(400).send('Not authorized for API');
    }
  } else if (path.match(/^\/[^/]*history_v[^/]*\/_doc$/)) {
  } else {
    console.log(`POST failed node: ${req.sensor.node} path:${path}`);
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
  https.createServer({
    key: Config.keyFileData,
    cert: Config.certFileData,
    secureOptions: require('crypto').constants.SSL_OP_NO_TLSv1
  }, app).listen(Config.get('esProxyPort', '7200'), Config.get('esProxyHost', undefined));
} else {
  http.createServer(app).listen(Config.get('esProxyPort', '7200'), Config.get('esProxyHost', undefined));
}
