/******************************************************************************/
/* esProxy.js  -- A security ES proxy that sensor nodes can be pointed to that
 *                only allows required ES calls for a sensor only machine. It can
 *                also check ips and passwords. You must be using central viewers
 *                that do NOT talk to the proxy, and talk to ES directly.
 *
 * Copyright 2020 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Config = require('./config.js');
const express = require('express');
const cryptoLib = require('crypto');
const http = require('http');
const https = require('https');
const fs = require('fs');
const basicAuth = require('basic-auth');
const zlib = require('zlib');
const ArkimeUtil = require('../common/arkimeUtil');
const ArkimeConfig = require('../common/arkimeConfig');
const aws4 = require('aws4');
const EsProxyCredentials = require('./esProxyCredentials');

// express app
const app = express();

// ============================================================================
// Config
// ============================================================================

let elasticsearch;
let sensors;
let oldprefix;
let prefix;
let sessionsIndexRe;
const esSSLOptions = { rejectUnauthorized: !ArkimeConfig.insecure };
let authHeader;
let sigV4Signer = null;
let sigV4Credentials = null;
let sigV4SignerTee = null;
let sigV4CredentialsTee = null;

ArkimeConfig.loaded(() => {
  elasticsearch = Config.get('elasticsearch');
  sensors = Config.configMap('esproxy-sensors');
  prefix = ArkimeUtil.formatPrefix(Config.get('prefix', 'arkime_'));
  oldprefix = prefix === 'arkime_' ? '' : prefix;
  // Sessions indices only, with our configured prefix
  const esc = str => str.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  sessionsIndexRe = new RegExp(`^(?:${esc(oldprefix)}sessions2-|(?:partial-)?${esc(prefix)}sessions3-)[A-Za-z0-9_*-]+$`);

  for (const sensor in sensors) {
    const { pass, ip } = sensors[sensor];
    // A blank or missing pass/ip turns that check off, refuse to run open
    if ((pass === undefined && ip === undefined) ||
        (pass !== undefined && !ArkimeUtil.isString(pass)) ||
        (ip !== undefined && !ArkimeUtil.isString(ip))) {
      console.log(`ERROR - esproxy-sensors '${sensor}' must set a non empty 'pass' and/or 'ip'`);
      process.exit(1);
    }
    sensors[sensor].node = sensor;
    if (ip) {
      sensors[sensor].ip = ip.split(',');
    }
  }
  console.log('ESPROXY sensors configured: %d', Object.keys(sensors).length);
  console.log(`PREFIX: ${prefix} OLDPREFIX: ${oldprefix}`);

  const esClientKey = Config.get('esClientKey');
  const esClientCert = Config.get('esClientCert');
  const caTrustFile = Config.getFull(Config.nodeName(), 'caTrustFile');
  if (caTrustFile) { esSSLOptions.ca = ArkimeUtil.certificateFileToArray(caTrustFile); }
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

  if (esAPIKey) {
    authHeader = `ApiKey ${esAPIKey}`;
  } else if (esBasicAuth) {
    if (!esBasicAuth.includes(':')) {
      authHeader = `Basic ${esBasicAuth}`;
    } else {
      authHeader = `Basic ${Buffer.from(esBasicAuth).toString('base64')}`;
    }
  }

  const sigV4Enabled = Config.get('esProxySigV4', false) === 'true' || Config.get('esProxySigV4', false) === true;
  if (sigV4Enabled) {
    const region = Config.get('esProxySigV4Region');
    const service = Config.get('esProxySigV4Service', 'es');

    sigV4Credentials = new EsProxyCredentials({
      credentialUrl: Config.get('esProxySigV4CredentialUrl'),
      credentialCert: Config.get('esProxySigV4CredentialCert'),
      credentialKey: Config.get('esProxySigV4CredentialKey'),
      roleArn: Config.get('esProxySigV4RoleArn'),
      accessKeyId: Config.get('esProxySigV4AccessKeyId'),
      secretAccessKey: Config.get('esProxySigV4SecretAccessKey'),
      sessionToken: Config.get('esProxySigV4SessionToken')
    });

    sigV4Signer = { region, service };
    authHeader = null;
    console.log('SigV4 signing enabled for region:', region);
  }
});

// ============================================================================
// Optional tee stuff
let elasticsearchTee;
let authHeaderTee;

ArkimeConfig.loaded(() => {
  elasticsearchTee = Config.sectionGet('tee', 'elasticsearch');
  const esAPIKeyTee = Config.sectionGet('tee', 'elasticsearchAPIKey');
  const esBasicAuthTee = Config.sectionGet('tee', 'elasticsearchBasicAuth');

  if (esAPIKeyTee) {
    authHeaderTee = `ApiKey ${esAPIKeyTee}`;
  } else if (esBasicAuthTee) {
    if (!esBasicAuthTee.includes(':')) {
      authHeaderTee = `Basic ${esBasicAuthTee}`;
    } else {
      authHeaderTee = `Basic ${Buffer.from(esBasicAuthTee).toString('base64')}`;
    }
  }

  const teeSigV4Enabled = Config.sectionGet('tee', 'esProxySigV4', false) === 'true' || Config.sectionGet('tee', 'esProxySigV4', false) === true;
  if (teeSigV4Enabled) {
    const teeRegion = Config.sectionGet('tee', 'esProxySigV4Region');
    const teeService = Config.sectionGet('tee', 'esProxySigV4Service', 'es');

    sigV4CredentialsTee = new EsProxyCredentials({
      credentialUrl: Config.sectionGet('tee', 'esProxySigV4CredentialUrl'),
      credentialCert: Config.sectionGet('tee', 'esProxySigV4CredentialCert'),
      credentialKey: Config.sectionGet('tee', 'esProxySigV4CredentialKey'),
      roleArn: Config.sectionGet('tee', 'esProxySigV4RoleArn'),
      accessKeyId: Config.sectionGet('tee', 'esProxySigV4AccessKeyId'),
      secretAccessKey: Config.sectionGet('tee', 'esProxySigV4SecretAccessKey'),
      sessionToken: Config.sectionGet('tee', 'esProxySigV4SessionToken')
    });

    sigV4SignerTee = { region: teeRegion, service: teeService };
    authHeaderTee = null;
    console.log('SigV4 signing enabled for tee, region:', teeRegion);
  }
});

// ============================================================================
// GET calls we can match exactly
const getExact = {
  '/': 1,
  '/_cat/health': 1,
  '/_cluster/health': 1,
  '/_refresh': 1,
  '/_nodes/stats/jvm,process,fs,os,indices,thread_pool': 1
};

// POST calls we can match exactly
const postExact = {
};

// PUT calls we can match exactly
const putExact = {
};

ArkimeConfig.loaded(() => {
  getExact[`/_template/${prefix}sessions3_template`] = 1;
  getExact[`/_template/${oldprefix}sessions2_template`] = 1;
  getExact[`/${oldprefix}sessions2-*/_alias`] = 1;
  getExact[`/${prefix}sessions3-*/_alias`] = 1;
  getExact[`/${oldprefix}sessions2-*,${prefix}sessions3-*/_alias`] = 1;
  getExact[`/${oldprefix}sessions2-*,partial-${prefix}sessions3-*,${prefix}sessions3-*/_alias`] = 1;
  getExact[`/${prefix}*/_refresh`] = 1;
  getExact[`/${prefix}stats/_stats`] = 1;
  getExact[`/${prefix}users/_stats`] = 1;
  getExact[`/${prefix}users/_count`] = 1;
  getExact[`/${prefix}sequence/_stats`] = 1;
  getExact[`/${prefix}dstats/_stats`] = 1;
  getExact[`/${prefix}files/_stats`] = 1;
  getExact[`/${prefix}fields/_search`] = 1;
  getExact[`/${prefix}queries/_mapping`] = 1;
  getExact[`/${prefix}queries/_doc/primary-viewer`] = 1;
  getExact[`/${prefix}files/_mapping`] = 1;

  postExact[`/${prefix}stats/_search`] = 1;
  postExact[`/${prefix}fields/_search`] = 1;
  postExact[`/${prefix}lookups/_search`] = 1;
});

// ============================================================================
// RegressionTests
// ===========================================================================
if (ArkimeConfig.regressionTests) {
  app.post('/regressionTests/shutdown', function (req, res) {
    process.exit(0);
  });
}

// ============================================================================
// Auth
// ===========================================================================

app.use((req, res, next) => {
  const credentials = basicAuth(req);
  if (!credentials) {
    return res.set('WWW-Authenticate', 'Basic').status(401).send();
  }

  // Own properties only. The name is whatever the client put in Basic auth, and
  // a plain [] lookup with __proto__ or constructor returns something truthy off
  // Object.prototype that has no pass and no ip -- authenticating a sensor that
  // was never configured. configMap now hands back a null prototype map too.
  // isPP is the codebase's usual guard but is not sufficient on its own here:
  // toString, valueOf and hasOwnProperty are not on its list yet resolve just
  // the same, which is what the own property check covers.
  const sensor = !ArkimeUtil.isPP(credentials.name) && Object.hasOwn(sensors, credentials.name)
    ? sensors[credentials.name]
    : undefined;
  if (!sensor) {
    console.log(`Unknown sensor ${ArkimeUtil.sanitizeStr(credentials.name)}`);
    return res.set('WWW-Authenticate', 'Basic').status(401).send();
  }

  if (sensor.pass !== undefined &&
      !cryptoLib.timingSafeEqual(
        cryptoLib.createHmac('sha256', 'compare').update(sensor.pass).digest(),
        cryptoLib.createHmac('sha256', 'compare').update(credentials.pass).digest())) {
    console.log(`Incorrect password for ${ArkimeUtil.sanitizeStr(credentials.name)}`);
    return res.set('WWW-Authenticate', 'Basic').status(401).send();
  }

  req.sensor = sensor;
  if (!sensor.ip) {
    return next();
  }

  let ip = req.socket.remoteAddress;
  if (ip.startsWith('::ffff:')) {
    ip = ip.substring(7);
  }

  if (!req.sensor.ip.includes(ip)) {
    console.log(`Incorrect source ip ${ip} for ${credentials.name}`);
    return res.set('WWW-Authenticate', 'Basic').status(401).send();
  }

  return next();
});

// Params like q= and source= override a validated body, only allow what capture and the sensor viewer send
const allowedQueryParams = new Set([
  '_source', 'filter_path', 'format', 'ignore_unavailable', 'max_concurrent_shard_requests', 'preference',
  'refresh', 'rest_total_hits_as_int', 'retry_on_conflict', 'size', 'timeout', 'version', 'version_type'
]);

// Parse the raw url once, the guard checks req.esPath and doProxyFull forwards
// req.esPath + req.esSearch, so both see the same thing. Don't use req.params,
// Express decodes it and an encoded ?/# then splits it differently.
// The es client encodes the commas in index lists and es decodes them, so do the same.
app.use((req, res, next) => {
  const url = new URL(req.url, 'http://0.0.0.0/');
  req.esPath = url.pathname.replace(/%2C/gi, ',');
  req.esSearch = url.search;
  for (const key of url.searchParams.keys()) {
    if (!allowedQueryParams.has(key)) {
      console.log(`Query param failed node: ${req.sensor.node} param:>%s<:`, ArkimeUtil.sanitizeStr(key));
      return res.status(400).send('Not authorized for API');
    }
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
  let contentLength = parseInt(req.headers['content-length'] || '1024');
  if (contentLength > 11000000) {
    contentLength = 11000000;
  }

  const buf = Buffer.alloc(contentLength);
  let pos = 0;
  req.on('data', (chunk) => { chunk.copy(buf, pos); pos += chunk.length; });
  req.on('end', () => {
    req.body = buf;
    next();
  });
}

// Proxy

async function doProxyFull (config, req, res) {
  let result = '';
  const esUrl = config.elasticsearch + req.esPath + req.esSearch;
  console.log(`URL ${req.method} "%s"`, ArkimeUtil.sanitizeStr(esUrl));
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

  // For SigV4: determine the body bytes to sign and send.
  // Decompress gzip so we sign the same bytes the server will hash.
  let bodyToSend = req._body ? req.body : null;
  if (config.sigV4Signer && bodyToSend && req.headers['content-encoding'] === 'gzip') {
    try {
      bodyToSend = zlib.gunzipSync(bodyToSend);
    } catch (err) {
      console.log('SigV4: failed to decompress gzip body, sending as-is');
    }
  }

  if (config.sigV4Signer && config.sigV4Credentials) {
    const creds = await config.sigV4Credentials.getCredentials();

    const signOpts = aws4.sign({
      host: url.hostname,
      path: url.pathname + (url.search || ''),
      method: req.method,
      service: config.sigV4Signer.service,
      region: config.sigV4Signer.region,
      headers: {
        'content-type': req.headers['content-type'] || 'application/json'
      },
      body: bodyToSend || ''
    }, {
      accessKeyId: creds.accessKeyId,
      secretAccessKey: creds.secretAccessKey,
      sessionToken: creds.sessionToken
    });

    options.headers = signOpts.headers;
  } else if (config.authHeader) {
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

  if (!config.sigV4Signer) {
    preq.setHeader('content-type', req.headers['content-type'] || 'application/json');
    // Copy these headers from original request to new request
    for (const header of ['x-opaque-id', 'content-encoding', 'content-length']) {
      if (req.headers[header] !== undefined) {
        preq.setHeader(header, req.headers[header]);
      }
    }
  }

  preq.on('error', (e) => {
    console.log('Request error "%s"', ArkimeUtil.sanitizeStr(url), e);
    try { res.status(502).send('ES proxy error'); } catch (err) { }
  });
  if (bodyToSend) {
    preq.end(bodyToSend);
  } else {
    preq.end('');
  }
}

async function doProxy (req, res) {
  await doProxyFull({ elasticsearch, authHeader, sigV4Signer, sigV4Credentials }, req, res);
  if (elasticsearchTee) {
    doProxyFull({ elasticsearch: elasticsearchTee, authHeader: authHeaderTee, sigV4Signer: sigV4SignerTee, sigV4Credentials: sigV4CredentialsTee }, req, {
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
  const path = req.esPath;

  // Empty IFs since those are allowed requests and will run code at end
  if (getExact[path]) {
  } else if (path === '/tagger/_search' || path.startsWith('/tagger/_source/')) {
  } else if (path.startsWith(`/${prefix}users/_doc/`)) {
  } else if (path.startsWith(`/${prefix}hunts/_doc/`)) {
  } else if (path === `/${prefix}sequence/_doc/fn-${req.sensor.node}`) {
  } else if (path === `/${prefix}stats/_doc/${req.sensor.node}`) {
  } else if (isOwnFilesDoc(path, req.sensor.node)) {
  } else if (isSessionsDocPath(path, '_doc')) {
  } else if (isSessionsIndicesPath(path, '_refresh')) {
  } else {
    console.log(`GET failed node: ${req.sensor.node} path:>%s<:`, ArkimeUtil.sanitizeStr(path));
    return res.status(400).send('Not authorized for API');
  }
  doProxy(req, res).catch(e => {
    console.log('Proxy error', e);
    res.status(500).send('Internal proxy error');
  });
});

// Validate Bulk

// Wildcards only for search, es would also resolve one in a bulk _index or _doc path
function isSessionsIndex (_index, wildcard = false) {
  return ArkimeUtil.isString(_index) && sessionsIndexRe.test(_index) && (wildcard || !_index.includes('*'));
}

function isFieldsIndex (_index) {
  return _index.startsWith(`${prefix}fields`);
}

// Only the sensor's own <node>-<num> doc, so node "test" can't match "test2" or "test-x"
function isOwnFilesDoc (path, node, action = '_doc') {
  const docPrefix = `/${prefix}files/${action}/${node}-`;
  const remainder = path.slice(docPrefix.length);
  return path.startsWith(docPrefix) && /^\d+$/.test(remainder);
}

function isOwnDstatsDoc (path, node) {
  const docPrefix = `/${prefix}dstats/_doc/${node}-`;
  const remainder = path.slice(docPrefix.length);
  return path.startsWith(docPrefix) && /^\d+-\d+$/.test(remainder);
}

// /<sessions index>/<action>/<id>
function isSessionsDocPath (path, action) {
  const m = path.match(/^\/([^/]+)\/([^/]+)\/[^/]+$/);
  return m !== null && m[2] === action && isSessionsIndex(m[1]);
}

function validateBulk (req) {
  if (!req._body) {
    return true;
  }

  let body;
  if (req.headers['content-encoding'] === undefined) {
    body = req.body;
  } else if (req.headers['content-encoding'] === 'gzip') {
    try {
      body = zlib.gunzipSync(req.body);
    } catch (err) {
      console.log('Error decoding gzip for bulk');
      return false;
    }
  } else {
    console.log('Invalid content-encoding "%s" for bulk', ArkimeUtil.sanitizeStr(req.headers['content-encoding']));
    return false;
  }

  const lines = body.toString('utf8').split('\n');
  for (let i = 0; i < lines.length; i++) {
    // ES allows blank lines between pairs of meta/object
    if (lines[i].trim() === '') { continue; }
    let json;
    try {
      json = JSON.parse(lines[i]);
      if (Object.keys(json).length !== 1) { throw new Error('More than 1 bulk operation in object'); }
      if (typeof json.index === 'object') {
        if (typeof json.index._index !== 'string') { throw new Error('Missing index _index string'); }

        // Eventually this should only allow fields
        const _index = json.index._index;
        if (!isSessionsIndex(_index) && !isFieldsIndex(_index)) { throw new Error(`Bad index ${_index}`); }
      } else if (typeof json.create === 'object') {
        const _index = json.create._index;
        if (!isSessionsIndex(_index)) { throw new Error(`Bad index ${_index}`); }
      } else if (typeof json.update === 'object') {
        const _index = json.update._index;
        if (!isFieldsIndex(_index)) { throw new Error(`Bad index ${_index}`); }
      } else if (typeof json.delete === 'object') {
        const _index = json.delete._index;
        if (!isFieldsIndex(_index)) { throw new Error(`Bad index ${_index}`); }
      } else {
        throw new Error('Missing create, update, delete or index operation');
      }
    } catch (err) {
      // Parser errors can contain body snippets. Log only validation metadata.
      console.log('Bulk validation failed at line %d (body bytes: %d)', i + 1, body.length);
      return false;
    }
    // delete has no body line, all others do
    if (!json.delete) {
      i++;
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

// Db.getSession looks up a single session by id
const searchIdsKeys = ['query', '_source', 'fields', 'profile'];
function validateSearchIds (req) {
  try {
    const json = JSON.parse(req.body.toString('utf8'));
    if (!Object.keys(json).every(key => searchIdsKeys.includes(key))) { return false; }
    if (Object.keys(json.query).length !== 1 || Object.keys(json.query.ids).length !== 1) { return false; }
    const values = json.query.ids.values;
    return Array.isArray(values) && values.length === 1 && ArkimeUtil.isString(values[0]);
  } catch (e) {
    return false;
  }
}

// /<sessions index>,<sessions index>/<action>
function isSessionsIndicesPath (path, action, wildcard = false) {
  const m = path.match(/^\/([^/]+)\/([^/]+)$/);
  return m !== null && m[2] === action && m[1].split(',').every(index => isSessionsIndex(index, wildcard));
}

// getEntirePCAP looks up every session sharing a rootId
const searchRootIdKeys = ['size', '_source', 'sort', 'query', 'profile'];
function validateSearchRootId (req) {
  try {
    const json = JSON.parse(req.body.toString('utf8'));
    if (!Object.keys(json).every(key => searchRootIdKeys.includes(key))) { return false; }
    if (Object.keys(json.query).length !== 1) { return false; }

    // pre 6.7.1 viewers sent the bare term
    if (json.query.term !== undefined) {
      return Object.keys(json.query.term).length === 1 &&
        ArkimeUtil.isString(json.query.term.rootId);
    }

    // filter only, so the user's forced expression is ANDed with the rootId
    // term and can only narrow what the bare term already allowed
    const bool = json.query.bool;
    return Object.keys(bool).length === 1 &&
      Array.isArray(bool.filter) &&
      Object.keys(bool.filter[0].term).length === 1 &&
      ArkimeUtil.isString(bool.filter[0].term.rootId);
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

// Partial doc update only, and it can't move the file to another node
function validateFilesUpdate (req) {
  try {
    const json = JSON.parse(req.body.toString('utf8'));
    return Object.keys(json).length === 1 && typeof json.doc === 'object' && json.doc !== null &&
      (json.doc.node === undefined || json.doc.node === req.sensor.node);
  } catch (e) {
    return false;
  }
}

// Post requests
app.post('*', saveBody, (req, res) => {
  const path = req.esPath;

  // Empty IFs since those are allowed requests and will run code at end
  if (postExact[path]) {
  } else if (path.startsWith(`/${prefix}fields/_doc/`)) {
  } else if (path === `/${prefix}sequence/_doc/fn-${req.sensor.node}`) {
  } else if (path === `/${prefix}stats/_doc/${req.sensor.node}`) {
  } else if (isOwnDstatsDoc(path, req.sensor.node)) {
  } else if (isOwnFilesDoc(path, req.sensor.node)) {
  } else if (isOwnFilesDoc(path, req.sensor.node, '_update') && validateFilesUpdate(req)) {
  } else if (path.startsWith('/_bulk') && validateBulk(req)) {
  } else if (path.startsWith(`/${prefix}files/_search`) && validateFilesSearch(req)) {
  } else if (isSessionsIndicesPath(path, '_search', true) && (validateSearchIds(req) || validateSearchRootId(req))) {
  } else if (path.match(/^\/[^/]*history_v[^/]*\/_doc$/)) {
  } else if (isSessionsDocPath(path, '_update') && validateUpdate(req)) {
    console.log(`UPDATE : ${req.sensor.node} path:>%s<:`, ArkimeUtil.sanitizeStr(path));
    if (Config.debug) {
      console.log('UPDATE body bytes: %d', req.body.length);
    }
  } else {
    console.log(`POST failed node: ${req.sensor.node} path:>%s<: body bytes: %d`, ArkimeUtil.sanitizeStr(path), Buffer.isBuffer(req.body) ? req.body.length : 0);
    return res.status(400).send('Not authorized for API');
  }
  doProxy(req, res).catch(e => {
    console.log('Proxy error', e);
    res.status(500).send('Internal proxy error');
  });
});

// Delete requests
app.delete('*', (req, res) => {
  const path = req.esPath;

  // Empty IFs since those are allowed requests and will run code at end
  if (isOwnFilesDoc(path, req.sensor.node)) {
  } else {
    console.log(`DELETE failed node: ${req.sensor.node} path:>%s<:`, ArkimeUtil.sanitizeStr(path));
    return res.status(400).send('Not authorized for API');
  }
  doProxy(req, res).catch(e => {
    console.log('Proxy error', e);
    res.status(500).send('Internal proxy error');
  });
});

// Put requests
app.put('*', (req, res) => {
  const path = req.esPath;

  // Empty IFs since those are allowed requests and will run code at end
  if (putExact[path]) {
  } else {
    console.log(`PUT failed node: ${req.sensor.node} path:>%s<:`, ArkimeUtil.sanitizeStr(path));
    return res.status(400).send('Not authorized for API');
  }
  doProxy(req, res).catch(e => {
    console.log('Proxy error', e);
    res.status(500).send('Internal proxy error');
  });
});

// Replace the default express error handler
app.use(ArkimeUtil.expressErrorHandler);

// ============================================================================
// MAIN
// ===========================================================================
let httpAgent;
let httpsAgent;

async function main () {
  await Config.initialize();

  httpAgent = new http.Agent({ keepAlive: true, keepAliveMsecs: 5000, maxSockets: 100 });
  httpsAgent = new https.Agent(Object.assign({ keepAlive: true, keepAliveMsecs: 5000, maxSockets: 100 }, esSSLOptions));

  if (sigV4Credentials) {
    await sigV4Credentials.initialize();
  }
  if (sigV4CredentialsTee) {
    await sigV4CredentialsTee.initialize();
  }

  ArkimeUtil.createHttpServer(app, Config.get('esProxyHost'), Config.get('esProxyPort', '7200'));
}

main();
