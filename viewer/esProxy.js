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
  console.log('sensors', sensors);

  for (const sensor in sensors) {
    sensors[sensor].node = sensor;
    if (sensors[sensor].ip) {
      sensors[sensor].ip = sensors[sensor].ip.split(',');
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
  getExact[`/${prefix}stats/_stats`] = 1;
  getExact[`/${prefix}users/_stats`] = 1;
  getExact[`/${prefix}users/_count`] = 1;
  getExact[`/${prefix}sequence/_stats`] = 1;
  getExact[`/${prefix}dstats/_stats`] = 1;
  getExact[`/${prefix}files/_stats`] = 1;
  getExact[`/${prefix}fields/_search`] = 1;
  getExact[`/${prefix}queries/_mapping`] = 1;
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

  if (!sensors[credentials.name]) {
    console.log(`Unknown sensor ${credentials.name}`);
    return res.set('WWW-Authenticate', 'Basic').status(401).send();
  }

  if (sensors[credentials.name].pass !== undefined &&
      !cryptoLib.timingSafeEqual(
        cryptoLib.createHmac('sha256', 'compare').update(sensors[credentials.name].pass).digest(),
        cryptoLib.createHmac('sha256', 'compare').update(credentials.pass).digest())) {
    console.log(`Incorrect password for ${credentials.name}`);
    return res.set('WWW-Authenticate', 'Basic').status(401).send();
  }

  req.sensor = sensors[credentials.name];
  if (!sensors[credentials.name].ip) {
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

// ============================================================================
// Proxy code to real ES
// ===========================================================================

function normalizeUrlPath (path) {
  const normalizedUrl = new URL(path, 'https://0.0.0.0/');
  return normalizedUrl.pathname;
}

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
  const esUrl = config.elasticsearch + req.url;
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
  const path = normalizeUrlPath(req.params['0']);

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
    console.log(`GET failed node: ${req.sensor.node} path:>%s<:`, ArkimeUtil.sanitizeStr(path));
    return res.status(400).send('Not authorized for API');
  }
  doProxy(req, res).catch(e => {
    console.log('Proxy error', e);
    res.status(500).send('Internal proxy error');
  });
});

// Validate Bulk

function isSessionsIndex (_index) {
  return _index.startsWith(`${oldprefix}sessions2-`) || _index.startsWith(`${prefix}sessions3-`);
}

function isFieldsIndex (_index) {
  return _index.startsWith(`${prefix}fields`);
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
        console.log('Failed bulk', JSON.stringify(json, null, 2));
        throw new Error('Missing create, update or index operation');
      }
    } catch (err) {
      console.log('Bulk error', err, ArkimeUtil.sanitizeStr(lines[i]));
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
  const path = normalizeUrlPath(req.params['0']);

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
  doProxy(req, res).catch(e => {
    console.log('Proxy error', e);
    res.status(500).send('Internal proxy error');
  });
});

// Delete requests
app.delete('*', (req, res) => {
  const path = normalizeUrlPath(req.params['0']);

  // Empty IFs since those are allowed requests and will run code at end
  if (path.startsWith(`/${prefix}files/_doc/${req.sensor.node}-`)) {
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
  const path = normalizeUrlPath(req.params['0']);

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
