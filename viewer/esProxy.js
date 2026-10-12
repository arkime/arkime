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

// Largest request body, sent or after gunzip. 2x capture's max dbBulkSize (15MB), a
// bulk can also go a session past it, or be uncompressed when compressES is off.
const MAX_BODY = 32 * 1024 * 1024;
const MAX_INFLATE = MAX_BODY;
// Never let a stuck es request hold one of the shared agent sockets forever
const ES_TIMEOUT_MS = 5 * 60 * 1000;

function gunzipBounded (buf) {
  return zlib.gunzipSync(buf, { maxOutputLength: MAX_INFLATE });
}

// ============================================================================
// Config
// ============================================================================

let elasticsearch;
let sensors;
let oldprefix;
let prefix;
let sessionsIndexRe;
let historyDocRe;
let usersDocRe;
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
  // The viewer's weekly history index, only with our prefix
  historyDocRe = new RegExp(`^/${esc(prefix)}history_v1-\\d+w\\d+/_doc$`);
  usersDocRe = new RegExp(`^/${esc(prefix)}users/_doc/[^/]+$`);
  // has the sensor passwords
  if (Config.debug) {
    console.log('sensors', sensors);
  }

  for (const sensor in sensors) {
    const { pass, ip, host } = sensors[sensor];
    // A blank or missing pass/ip turns that check off, refuse to run open
    if ((pass === undefined && ip === undefined) ||
        (pass !== undefined && !ArkimeUtil.isString(pass)) ||
        (ip !== undefined && !ArkimeUtil.isString(ip))) {
      console.log(`ERROR - esproxy-sensors '${sensor}' must set a non empty 'pass' and/or 'ip'`);
      process.exit(1);
    }
    // A blank host would turn the stats hostname check off
    if (host !== undefined && !ArkimeUtil.isString(host)) {
      console.log(`ERROR - esproxy-sensors '${sensor}' must set a non empty 'host' when set`);
      process.exit(1);
    }
    sensors[sensor].node = sensor;
    if (ip) {
      sensors[sensor].ip = ip.split(',');
    }
  }
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

  // getFull turns 'true'/'false' into booleans, anything else stays a string
  const sigV4Enabled = Config.get('esProxySigV4', false) === true;
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

  const teeSigV4Enabled = Config.sectionGet('tee', 'esProxySigV4', false) === true;
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
// Forward the re-serialized params, not the raw search, so es can't split the
// raw string differently (e.g. on ;) and see a param that wasn't checked.
app.use((req, res, next) => {
  const url = new URL(req.url, 'http://0.0.0.0/');
  req.esPath = url.pathname.replace(/%2C/gi, ',');
  for (const key of url.searchParams.keys()) {
    if (!allowedQueryParams.has(key)) {
      console.log(`Query param failed node: ${req.sensor.node} param:>%s<:`, ArkimeUtil.sanitizeStr(key));
      return res.status(400).send('Not authorized for API');
    }
  }
  const search = url.searchParams.toString();
  req.esSearch = search ? '?' + search : '';
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

  // Keep only what was really sent, a too large body is refused rather than truncated
  const chunks = [];
  let size = 0;
  req.on('data', (chunk) => {
    size += chunk.length;
    if (size <= MAX_BODY) { chunks.push(chunk); }
  });
  req.on('end', () => {
    if (size > MAX_BODY) {
      return res.status(413).send('Request body too large');
    }
    req.body = Buffer.concat(chunks, size);
    next();
  });
}

// Proxy

async function doProxyFull (config, req, res) {
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
      bodyToSend = gunzipBounded(bodyToSend);
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

  // Stream the response, buffering a large one could exhaust memory. Still always a
  // 200, which is what capture and the sensor viewers have always gotten from esProxy.
  const preq = client.request(url, options, (pres) => {
    if (!res.pipeable) {
      pres.resume();
      return;
    }
    res.setHeader('content-type', 'application/json');
    pres.pipe(res);
  });

  if (!config.sigV4Signer) {
    preq.setHeader('content-type', req.headers['content-type'] || 'application/json');
    // Copy these headers from original request to new request. Not content-length,
    // node sets it from the body actually sent.
    for (const header of ['x-opaque-id', 'content-encoding']) {
      if (req.headers[header] !== undefined) {
        preq.setHeader(header, req.headers[header]);
      }
    }
  }

  preq.setTimeout(ES_TIMEOUT_MS, () => {
    preq.destroy(new Error('timeout'));
  });
  // Don't keep an es socket busy for a client that went away
  res.on?.('close', () => {
    if (!res.writableFinished) { preq.destroy(); }
  });

  preq.on('error', (e) => {
    console.log('Request error "%s"', ArkimeUtil.sanitizeStr(url), e);
    try { if (!res.headersSent) { res.status(502).send('ES proxy error'); } else { res.destroy(); } } catch (err) { }
  });
  if (bodyToSend) {
    preq.end(bodyToSend);
  } else {
    preq.end('');
  }
}

async function doProxy (req, res) {
  res.pipeable = true;
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
  } else if (usersDocRe.test(path)) {
    // The sensor viewer needs the user for s2s auth, never its secrets
    req.esSearch = '?_source_excludes=passStore,totpSecret,cont3xt';
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

// The fields alias or a versioned fields index, not just anything starting with fields
function isFieldsIndex (_index) {
  return ArkimeUtil.isString(_index) && (_index === `${prefix}fields` || (_index.startsWith(`${prefix}fields_v`) && /^\d+$/.test(_index.slice(`${prefix}fields_v`.length))));
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

// Field definitions control how every viewer compiles queries, including users'
// forced expressions, so a sensor may add new fields but not redefine existing ones:
// index becomes create, and an update may only change these display keys.
const fieldsUpdateKeys = ['category', 'friendlyName', 'help', 'aliases'];

function validateBulk (req) {
  if (!req._body) {
    return true;
  }

  let body;
  if (req.headers['content-encoding'] === undefined) {
    body = req.body;
  } else if (req.headers['content-encoding'] === 'gzip') {
    try {
      body = gunzipBounded(req.body);
    } catch (err) {
      console.log('Error decoding gzip for bulk');
      return false;
    }
  } else {
    console.log('Invalid content-encoding "%s" for bulk', ArkimeUtil.sanitizeStr(req.headers['content-encoding']));
    return false;
  }

  const lines = body.toString('utf8').split('\n');
  const out = [];
  let rewrote = false;
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
        if (isFieldsIndex(_index)) {
          if (lines[i + 1] === undefined) { throw new Error('Missing index body'); }
          out.push(JSON.stringify({ create: json.index }), lines[i + 1]);
          rewrote = true;
          i++;
          continue;
        }
        if (!isSessionsIndex(_index)) { throw new Error(`Bad index ${_index}`); }
      } else if (typeof json.create === 'object') {
        const _index = json.create._index;
        if (!isSessionsIndex(_index)) { throw new Error(`Bad index ${_index}`); }
      } else if (typeof json.update === 'object') {
        const _index = json.update._index;
        if (!isFieldsIndex(_index)) { throw new Error(`Bad index ${_index}`); }
        const update = JSON.parse(lines[i + 1]);
        if (Object.keys(update).length !== 1 || typeof update.doc !== 'object' || update.doc === null) { throw new Error('Fields update must be a partial doc'); }
        const doc = Object.fromEntries(Object.entries(update.doc).filter(([key]) => fieldsUpdateKeys.includes(key)));
        if (Object.keys(doc).length !== Object.keys(update.doc).length) {
          console.log(`Bulk dropping field update keys node: ${req.sensor.node}`, ArkimeUtil.sanitizeStr(lines[i + 1]));
          rewrote = true;
        }
        if (Object.keys(doc).length > 0) {
          out.push(lines[i], JSON.stringify({ doc }));
        }
        i++;
        continue;
      } else if (typeof json.delete === 'object') {
        const _index = json.delete._index;
        if (!isFieldsIndex(_index)) { throw new Error(`Bad index ${_index}`); }
      } else {
        console.log('Failed bulk', JSON.stringify(json, null, 2));
        throw new Error('Missing create, update, delete or index operation');
      }
    } catch (err) {
      console.log('Bulk error', err, ArkimeUtil.sanitizeStr(lines[i]));
      return false;
    }
    // delete has no body line, all others do
    if (json.delete) {
      out.push(lines[i]);
    } else {
      out.push(lines[i], lines[i + 1]);
      i++;
    }
  }

  if (rewrote) {
    // Everything was dropped, answer success without asking es, capture logs any error
    if (out.length === 0) {
      req.bulkEmpty = true;
      return true;
    }
    req.body = Buffer.from(out.join('\n') + '\n');
    delete req.headers['content-encoding'];
  }

  return true;
}

// Validate Files Search, the shapes capture and the sensor viewer's expiry send.
// Only top level keys that can't widen the node filter, aggs global ignores the query.
const filesSearchKeys = ['size', 'sort', 'query', '_source'];
function validateFilesSearch (req) {
  try {
    const json = parseDocBody(req);
    if (!Object.keys(json).every(key => filesSearchKeys.includes(key))) { return false; }
    if (Object.keys(json.query).length !== 1) { return false; }
    const bool = json.query.bool;
    if (!Object.keys(bool).every(key => ['filter', 'must', 'must_not'].includes(key))) { return false; }
    const first = (bool.filter ?? bool.must)[0];
    if (Object.keys(first).length !== 1 || Object.keys(first.terms).length !== 1) { return false; }
    const nodes = first.terms.node;
    if (!ArkimeUtil.isStringArray(nodes) || !nodes.includes(req.sensor.node)) { return false; }
    // The expiry query lists every node on the sensor's host, with host set only
    // allow nodes of sensors configured on that same host
    if (req.sensor.host !== undefined) {
      return nodes.every(node => node === req.sensor.node || (Object.hasOwn(sensors, node) && sensors[node].host === req.sensor.host));
    }
    return true;
  } catch (e) {
    return false;
  }
}

// Db.getSession looks up a single session by id
const searchIdsKeys = ['query', '_source', 'fields', 'profile'];
function validateSearchIds (req) {
  try {
    const json = parseDocBody(req);
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
    const json = parseDocBody(req);
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
    const json = parseDocBody(req);
    return json.script !== undefined && json.doc === undefined;
  } catch (e) {
    return false;
  }
}

// Parse a json body, capture gzips bodies over 860 bytes when compressES is set
function parseDocBody (req) {
  let body = req.body;
  if (req.headers['content-encoding'] === 'gzip') {
    body = gunzipBounded(body);
  } else if (req.headers['content-encoding'] !== undefined) {
    throw new Error('Bad content-encoding');
  }
  return JSON.parse(body.toString('utf8'));
}

// Viewers use the stats hostname to decide which nodes are on their host, so
// when the sensor has a host set it can't claim another one
function validateStatsDoc (req) {
  if (req.sensor.host === undefined) { return true; }
  try {
    const json = parseDocBody(req);
    return typeof json === 'object' && json !== null && json.hostname === req.sensor.host;
  } catch (e) {
    return false;
  }
}

// The doc body has to describe the file in the path, the viewer expiring files
// trusts node to decide which host's disk the file is on
function validateFilesDoc (req) {
  try {
    const json = parseDocBody(req);
    const num = req.esPath.slice(`/${prefix}files/_doc/${req.sensor.node}-`.length);
    return typeof json === 'object' && json !== null && json.node === req.sensor.node && json.num === parseInt(num, 10);
  } catch (e) {
    return false;
  }
}

// Partial doc update only, and it can't move the file to another node
function validateFilesUpdate (req) {
  try {
    const json = parseDocBody(req);
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
  } else if (path === `/${prefix}sequence/_doc/fn-${req.sensor.node}`) {
  } else if (path === `/${prefix}stats/_doc/${req.sensor.node}` && validateStatsDoc(req)) {
  } else if (isOwnDstatsDoc(path, req.sensor.node)) {
  } else if (isOwnFilesDoc(path, req.sensor.node) && validateFilesDoc(req)) {
  } else if (isOwnFilesDoc(path, req.sensor.node, '_update') && validateFilesUpdate(req)) {
  } else if (path.startsWith('/_bulk') && validateBulk(req)) {
  } else if (path === `/${prefix}files/_search` && validateFilesSearch(req)) {
  } else if (isSessionsIndicesPath(path, '_search', true) && (validateSearchIds(req) || validateSearchRootId(req))) {
  } else if (historyDocRe.test(path)) {
  } else if (isSessionsDocPath(path, '_update') && validateUpdate(req)) {
    console.log(`UPDATE : ${req.sensor.node} path:>%s<:`, ArkimeUtil.sanitizeStr(path));
    if (Config.debug) {
      console.log(req.body.toString('utf8'));
    }
  } else {
    console.log(`POST failed node: ${req.sensor.node} path:>%s<:`, ArkimeUtil.sanitizeStr(path));
    // Log failed body for debugging hacking
    console.log(req.body.toString('utf8'));
    return res.status(400).send('Not authorized for API');
  }
  if (req.bulkEmpty) {
    return res.send({ took: 0, errors: false, items: [] });
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
