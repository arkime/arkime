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
const DbESImpl = require('./db.es.js');

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
let huntsDocRe;
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
  usersDocRe = new RegExp(`^/${esc(prefix)}users/_doc/([^/]+)$`);
  huntsDocRe = new RegExp(`^/${esc(prefix)}hunts/_doc/([^/]+)$`);
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
// Forward the re-serialized params, not the raw search, so es can't split the
// raw string differently (e.g. on ;) and see a param that wasn't checked.
app.use((req, res, next) => {
  const url = new URL(req.url, 'http://0.0.0.0/');
  req.esPath = url.pathname.replace(/%2C/gi, ',');
  for (const [key, value] of url.searchParams) {
    // docker.sh --wait-for-db, only on cluster health where it can't do anything else
    if (key === 'wait_for_status' && req.esPath === '/_cluster/health' && ['green', 'yellow', 'red'].includes(value)) {
      continue;
    }
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

// SigV4 signs the exact bytes es will hash
async function esAuthHeaders (config, url, method, contentType, body) {
  if (config.sigV4Signer && config.sigV4Credentials) {
    const creds = await config.sigV4Credentials.getCredentials();

    const signOpts = aws4.sign({
      host: url.hostname,
      path: url.pathname + (url.search || ''),
      method,
      service: config.sigV4Signer.service,
      region: config.sigV4Signer.region,
      headers: {
        'content-type': contentType
      },
      body
    }, {
      accessKeyId: creds.accessKeyId,
      secretAccessKey: creds.secretAccessKey,
      sessionToken: creds.sessionToken
    });

    return signOpts.headers;
  } else if (config.authHeader) {
    return { Authorization: config.authHeader };
  }
  return undefined;
}

// esProxy's own lookups, always against the primary es
async function esRequestJson (method, path, body) {
  const url = new URL(elasticsearch + path);
  const client = url.protocol === 'https:' ? https : http;
  const data = body ? JSON.stringify(body) : '';
  const headers = Object.assign({ 'content-type': 'application/json' },
    await esAuthHeaders({ authHeader, sigV4Signer, sigV4Credentials }, url, method, 'application/json', data));

  return new Promise((resolve, reject) => {
    const preq = client.request(url, { method, headers, agent: client === https ? httpsAgent : httpAgent }, (pres) => {
      const chunks = [];
      pres.on('data', chunk => chunks.push(chunk));
      pres.on('end', () => {
        let json;
        try { json = JSON.parse(Buffer.concat(chunks).toString('utf8')); } catch (e) { }
        resolve({ statusCode: pres.statusCode, body: json });
      });
    });
    preq.setTimeout(ES_TIMEOUT_MS, () => { preq.destroy(new Error('timeout')); });
    preq.on('error', reject);
    preq.end(data);
  });
}

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

  options.headers = await esAuthHeaders(config, url, req.method, req.headers['content-type'] || 'application/json', bodyToSend || '');

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
    // Never let a tee failure become an unhandled rejection, that exits for every sensor
    doProxyFull({ elasticsearch: elasticsearchTee, authHeader: authHeaderTee, sigV4Signer: sigV4SignerTee, sigV4Credentials: sigV4CredentialsTee }, req, {
      setHeader: () => {},
      send: () => {}
    }).catch(e => {
      console.log('Tee error', ArkimeUtil.sanitizeStr(e?.message ?? String(e)));
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
  } else if (isDocId(path.match(usersDocRe)?.[1])) {
    // The sensor viewer needs the user for s2s auth, never its secrets
    req.esSearch = '?_source_excludes=passStore,totpSecret,cont3xt';
  } else if (isDocId(path.match(huntsDocRe)?.[1])) {
  } else if (path === `/${prefix}sequence/_doc/fn-${req.sensor.node}`) {
  } else if (path === `/${prefix}stats/_doc/${req.sensor.node}`) {
  } else if (isOwnFilesDoc(path, req.sensor.node)) {
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

// On ES 7 and OpenSearch 1 typed routes /<index>/_doc/<x> is an endpoint, not a doc,
// when x is one of these. Those versions are frozen, so the list is complete. Ids are
// otherwise free, user ids can start with _. es matches routes on the raw path, these
// and / are refused decoded too in case one ever doesn't. No real id has a / in it.
const typedEndpoints = new Set([
  '_bulk', '_count', '_create', '_delete_by_query', '_doc', '_explain', '_field_caps', '_graph',
  '_mapping', '_mappings', '_mget', '_msearch', '_mtermvectors', '_rank_eval', '_rollup_search',
  '_search', '_search_shards', '_source', '_termvectors', '_update', '_update_by_query', '_validate', '_xpack'
]);
function isDocId (segment) {
  if (!ArkimeUtil.isString(segment)) { return false; }
  let id;
  try {
    id = decodeURIComponent(segment);
  } catch (e) {
    return false;
  }
  return !id.includes('/') && !typedEndpoints.has(id);
}

// /<sessions index>/<action>/<id>
function isSessionsDocPath (path, action) {
  const m = path.match(/^\/([^/]+)\/([^/]+)\/([^/]+)$/);
  return m !== null && m[2] === action && isSessionsIndex(m[1]) && isDocId(m[3]);
}

// Field definitions control how every viewer compiles queries, including users'
// forced expressions, so a sensor may add new fields but not redefine existing ones:
// index becomes create, and an update may only change these display keys.
const fieldsUpdateKeys = ['category', 'friendlyName', 'help', 'aliases'];

// Viewers look fields up by these too, a new field can't claim an existing field's
// db name or it changes how that db field is typed, displayed and compiled with db:.
// db.pl defines the fields that legitimately share one and writes to es directly.
const fieldNameProps = ['dbField', 'dbField2', 'fieldECS'];
function fieldNames (source) {
  return fieldNameProps.map(prop => source?.[prop]).filter(dbName => ArkimeUtil.isString(dbName));
}

// Drop the creates whose db names an existing field with another id already has
async function dropFieldCollisions (creates) {
  const names = [...new Set(creates.flatMap(create => create.names))];
  if (names.length === 0) { return; }
  const { statusCode, body } = await esRequestJson('POST', `/${prefix}fields/_search`, {
    size: 10000,
    _source: fieldNameProps,
    query: { bool: { should: fieldNameProps.map(prop => ({ terms: { [prop]: names } })), minimum_should_match: 1 } }
  });
  if (statusCode !== 200 || !Array.isArray(body?.hits?.hits)) { throw new Error(`fields search status ${statusCode}`); }
  const takenBy = new Map();
  for (const hit of body.hits.hits) {
    for (const dbName of fieldNames(hit._source)) {
      (takenBy.get(dbName) ?? takenBy.set(dbName, new Set()).get(dbName)).add(hit._id);
    }
  }
  for (const create of creates) {
    create.dropped = create.names.some(dbName => [...(takenBy.get(dbName) ?? [])].some(id => id !== create.id));
  }
}

async function validateBulk (req) {
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
  // Strings, or a field create object that may still be dropped
  const out = [];
  const fieldCreates = [];
  const deletedFields = new Set();
  let rewrote = false;
  for (let i = 0; i < lines.length; i++) {
    // ES allows blank lines between pairs of meta/object
    if (lines[i].trim() === '') { continue; }
    let json;
    // The line an error is about, the body line once the command parsed
    let errLine = i;
    try {
      json = JSON.parse(lines[i]);
      if (Object.keys(json).length !== 1) { throw new Error('More than 1 bulk operation in object'); }
      if (typeof json.index === 'object') {
        if (typeof json.index._index !== 'string') { throw new Error('Missing index _index string'); }

        // Eventually this should only allow fields
        const _index = json.index._index;
        if (isFieldsIndex(_index)) {
          if (lines[i + 1] === undefined) { throw new Error('Missing index body'); }
          const id = json.index._id;
          errLine = i + 1;
          const source = JSON.parse(lines[i + 1]);
          rewrote = true;
          i++;
          // Viewers skip a bad definition, drop it here so it never gets stored but
          // the rest of capture's batch of new fields still is
          if (!Config.isValidFieldDoc({ _id: id, _source: source })) {
            console.log(`Bulk dropping invalid field definition node: ${req.sensor.node}`, ArkimeUtil.sanitizeStr(lines[i - 1]));
            continue;
          }
          // A delete then create of the same id would redefine it
          if (deletedFields.has(id)) { continue; }
          const create = { id, names: fieldNames(source), lines: [JSON.stringify({ create: json.index }), lines[i]] };
          fieldCreates.push(create);
          out.push(create);
          continue;
        }
        if (!isSessionsIndex(_index)) { throw new Error(`Bad index ${_index}`); }
      } else if (typeof json.create === 'object') {
        const _index = json.create._index;
        if (!isSessionsIndex(_index)) { throw new Error(`Bad index ${_index}`); }
      } else if (typeof json.update === 'object') {
        const _index = json.update._index;
        if (!isFieldsIndex(_index)) { throw new Error(`Bad index ${_index}`); }
        if (lines[i + 1] === undefined) { throw new Error('Missing update body'); }
        errLine = i + 1;
        const update = JSON.parse(lines[i + 1]);
        if (Object.keys(update).length !== 1 || typeof update.doc !== 'object' || update.doc === null) { throw new Error('Fields update must be a partial doc'); }
        const doc = Object.fromEntries(Object.entries(update.doc).filter(([key]) => fieldsUpdateKeys.includes(key)));
        if (Object.keys(doc).length !== Object.keys(update.doc).length) {
          const dropped = Object.keys(update.doc).filter(key => !fieldsUpdateKeys.includes(key));
          console.log('Bulk dropping field update keys node: %s keys: %s', req.sensor.node, ArkimeUtil.sanitizeStr(dropped.join(',')));
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
        // A delete then create redefines the field, capture only deletes obsolete
        // http.request-/http.response- fields so dropping it just leaves them.
        // Capture resends those on every start, so only log once per sensor.
        if (!req.sensor.loggedFieldDelete) {
          req.sensor.loggedFieldDelete = true;
          console.log(`Bulk dropping field deletes node: ${req.sensor.node}`, ArkimeUtil.sanitizeStr(lines[i]));
        }
        if (ArkimeUtil.isString(json.delete._id)) { deletedFields.add(json.delete._id); }
        rewrote = true;
        continue;
      } else {
        throw new Error('Missing create, update, delete or index operation');
      }
    } catch (err) {
      // Only the failing command's operation, _index and _id, the POST failed debug log has the full body
      let command = 'unparsable';
      if (json !== null && typeof json === 'object' && !Array.isArray(json) && Object.keys(json).length > 0) {
        const operation = Object.keys(json)[0];
        const { _index, _id } = json[operation] ?? {};
        const scalar = (v) => (typeof v === 'string' || typeof v === 'number') ? v : undefined;
        command = JSON.stringify({ [operation]: { _index: scalar(_index), _id: scalar(_id) } });
      }
      // A JSON.parse message quotes the input, which is the body
      const reason = err instanceof SyntaxError ? 'invalid JSON' : err.message;
      console.log('Bulk failed node: %s line: %d %s - %s', req.sensor.node, errLine + 1, ArkimeUtil.sanitizeStr(command), ArkimeUtil.sanitizeStr(reason));
      if (Config.debug) {
        console.log(err);
      }
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

  if (fieldCreates.length > 0) {
    try {
      await dropFieldCollisions(fieldCreates);
      for (const create of fieldCreates) {
        if (create.dropped) { console.log(`Bulk dropping field with a db name in use node: ${req.sensor.node}`, ArkimeUtil.sanitizeStr(create.lines[0])); }
      }
    } catch (err) {
      // Can't tell which are safe, drop the new fields but keep everything else
      console.log(`Bulk dropping new fields, error checking them node: ${req.sensor.node}`, ArkimeUtil.sanitizeStr(err?.message ?? String(err)));
      for (const create of fieldCreates) { create.dropped = true; }
    }
  }

  if (rewrote) {
    const final = out.flatMap(item => typeof item === 'string' ? [item] : (item.dropped ? [] : item.lines));
    // Everything was dropped, answer success without asking es, capture logs any error
    if (final.length === 0) {
      req.bulkEmpty = true;
      return true;
    }
    req.body = Buffer.from(final.join('\n') + '\n');
    delete req.headers['content-encoding'];
  }

  return true;
}

// Validate Files Search, the shapes capture and the sensor viewer's expiry send.
// Only top level keys that can't widen the node filter, aggs global ignores the query.
const filesSearchKeys = ['size', 'sort', 'query', '_source'];
async function validateFilesSearch (req) {
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
    // The expiry query lists every node on the sensor's host, so only allow those
    const allowed = await hostNodes(req.sensor);
    return nodes.every(node => allowed.includes(node));
  } catch (e) {
    return false;
  }
}

// Db.getSession looks up a single session by id. The filter clauses to keep, or undefined
const searchIdsKeys = ['query', '_source', 'fields', 'profile'];
function searchIdsFilter (json) {
  try {
    if (!Object.keys(json).every(key => searchIdsKeys.includes(key))) { return undefined; }
    if (Object.keys(json.query).length !== 1 || Object.keys(json.query.ids).length !== 1) { return undefined; }
    const values = json.query.ids.values;
    if (!Array.isArray(values) || values.length !== 1 || !ArkimeUtil.isString(values[0])) { return undefined; }
    return [json.query];
  } catch (e) {
    return undefined;
  }
}

// /<sessions index>,<sessions index>/<action>
function isSessionsIndicesPath (path, action, wildcard = false) {
  const m = path.match(/^\/([^/]+)\/([^/]+)$/);
  return m !== null && m[2] === action && m[1].split(',').every(index => isSessionsIndex(index, wildcard));
}

// getEntirePCAP looks up every session sharing a rootId
const searchRootIdKeys = ['size', '_source', 'sort', 'query', 'profile'];
function searchRootIdFilter (json) {
  try {
    if (!Object.keys(json).every(key => searchRootIdKeys.includes(key))) { return undefined; }
    if (Object.keys(json.query).length !== 1) { return undefined; }

    // pre 6.7.1 viewers sent the bare term
    if (json.query.term !== undefined) {
      return Object.keys(json.query.term).length === 1 && ArkimeUtil.isString(json.query.term.rootId) ? [json.query] : undefined;
    }

    // filter only, so the user's forced expression is ANDed with the rootId
    // term and can only narrow what the bare term already allowed
    const bool = json.query.bool;
    return Object.keys(bool).length === 1 &&
      Array.isArray(bool.filter) &&
      Object.keys(bool.filter[0].term).length === 1 &&
      ArkimeUtil.isString(bool.filter[0].term.rootId) ? bool.filter : undefined;
  } catch (e) {
    return undefined;
  }
}

// Both are narrowed to the nodes on the sensor's host, so a sensor viewer can't read
// other hosts' sessions
async function validateSessionsSearch (req) {
  try {
    const json = parseDocBody(req);
    // The forced expression clauses and sort pass through, no scripts or doc lookups there
    if (hasForbiddenSearchKey(json)) { return false; }
    const filter = searchIdsFilter(json) ?? searchRootIdFilter(json);
    if (filter === undefined) { return false; }
    const nodes = await hostNodes(req.sensor, true);
    json.query = { bool: { filter: [...filter, { terms: { node: nodes } }] } };
    req.body = Buffer.from(JSON.stringify(json));
    delete req.headers['content-encoding'];
    return true;
  } catch (e) {
    return false;
  }
}
// Only the hunt script a sensor viewer runs when its remote hunt matches, tagging
// and hunt removal run on central viewers. Any other script could rewrite or delete
// any session. Compared ignoring whitespace since older viewers indent it differently.
// es runs the proxy's copy, which leaves a session from another host alone.
const normScript = (source) => source.replace(/\s+/g, ' ').trim();
const addHuntScript = normScript(DbESImpl.SESSION_SCRIPTS.addHuntToSession);
const addHuntHostScript = `if (params.esproxyNodes.contains(ctx._source.node)) { ${addHuntScript} } else { ctx.op = 'none'; }`;
async function validateUpdate (req) {
  try {
    // es returns the doc when _source is asked for, even when the script makes the
    // update a noop, which would read any session by id. The viewer never asks for it.
    if (new URLSearchParams(req.esSearch).has('_source')) { return false; }
    const json = parseDocBody(req);
    if (Object.keys(json).length !== 1 || typeof json.script !== 'object' || json.script === null) { return false; }
    const { source, lang, params } = json.script;
    if (!Object.keys(json.script).every(key => ['source', 'lang', 'params'].includes(key))) { return false; }
    if ((lang !== undefined && lang !== 'painless') || !ArkimeUtil.isString(source)) { return false; }
    if (normScript(source) !== addHuntScript || typeof params !== 'object' || params === null) { return false; }
    if (Object.keys(params).length !== 2 || !ArkimeUtil.isString(params.huntId) || !ArkimeUtil.isString(params.huntName)) { return false; }
    const esproxyNodes = await hostNodes(req.sensor, true);
    const script = { lang: 'painless', source: addHuntHostScript, params: { huntId: params.huntId, huntName: params.huntName, esproxyNodes } };
    req.body = Buffer.from(JSON.stringify({ script }));
    delete req.headers['content-encoding'];
    return true;
  } catch (e) {
    return false;
  }
}

// What the viewer's history logging always sends, so a sensor can't write just any
// audit row. The index mapping is strict, which bounds the other keys.
function validateHistoryDoc (req) {
  try {
    const json = parseDocBody(req);
    return typeof json === 'object' && json !== null && !Array.isArray(json) &&
      ArkimeUtil.isString(json.userId) && ArkimeUtil.isString(json.api) && ArkimeUtil.isString(json.method) &&
      typeof json.timestamp === 'number';
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

// Stats, fields and lookups searches from viewers are plain queries. A script runs
// with es's credential, and naming another index's doc (index or _index next to id
// or _id: terms lookup, more_like_this, percolate, geo_shape) reads it. A wrapper
// query (base64) or a suggester collate (string template) hides any of those from
// this check. Exact keys, so fields named description or index are fine.
const forbiddenSearchKeys = new Set([
  'script', 'scripts', 'script_fields', 'script_score', 'scripted_metric', '_script', 'bucket_script',
  'bucket_selector', 'moving_fn', 'minimum_should_match_script', 'init_script', 'map_script', 'combine_script',
  'reduce_script', 'runtime_mappings', 'wrapper', 'collate'
]);
function hasForbiddenSearchKey (value) {
  if (Array.isArray(value)) { return value.some(hasForbiddenSearchKey); }
  if (typeof value !== 'object' || value === null) { return false; }
  if ((Object.hasOwn(value, 'index') || Object.hasOwn(value, '_index')) && (Object.hasOwn(value, 'id') || Object.hasOwn(value, '_id'))) {
    // Except a $shortcut in a forced expression, a terms lookup in the lookups
    // index, which sensors can search anyway. Its other keys are still checked.
    if (value.index !== `${prefix}lookups` || Object.hasOwn(value, '_index')) { return true; }
  }
  return Object.keys(value).some(key => forbiddenSearchKeys.has(key) || hasForbiddenSearchKey(value[key]));
}

function validateSearchBody (req) {
  if (!req._body) { return true; }
  try {
    return !hasForbiddenSearchKey(parseDocBody(req));
  } catch (e) {
    return false;
  }
}

// The configured host, else the hostname pinned from the sensor's first stats doc
function knownHost (sensor) {
  return sensor.host ?? sensor.pinnedHost;
}

// Another esProxy may have pinned the sensor since this one started, so a missing pin
// is looked up from its stats doc (a get by id is realtime) before trusting a new one
async function sensorHost (sensor) {
  const host = knownHost(sensor);
  if (host !== undefined) { return host; }
  sensor.pinLookup ??= lookupPinnedHost(sensor).finally(() => { delete sensor.pinLookup; });
  return sensor.pinLookup;
}

async function lookupPinnedHost (sensor) {
  const { statusCode, body } = await esRequestJson('GET', `/${prefix}stats/_doc/${encodeURIComponent(sensor.node)}?_source=hostname`);
  if (statusCode === 404) { return undefined; }
  if (statusCode !== 200) { throw new Error(`status ${statusCode}`); }
  // A stats doc that exists without a hostname isn't a missing pin, fail closed
  if (!ArkimeUtil.isString(body?._source?.hostname)) {
    if (!sensor.loggedNoHostname) {
      sensor.loggedNoHostname = true;
      console.log(`ERROR - stats doc for sensor ${sensor.node} has no hostname, set 'host' in esproxy-sensors or delete the doc`);
    }
    throw new Error('stats doc without hostname');
  }
  sensor.pinnedHost = body._source.hostname;
  console.log(`Pinning sensor ${sensor.node} to hostname %s`, ArkimeUtil.sanitizeStr(sensor.pinnedHost));
  return sensor.pinnedHost;
}

// The nodes of the sensors on this sensor's host, which is all a sensor viewer handles:
// the central viewer sends it hunts and pcap requests by the session's node. Without a
// stats doc yet a sensor has no host, so only its own node. Only sensors that never
// sent a stats doc are looked up here, in one mget, the rest are pinned.
async function hostNodes (sensor, sessions = false) {
  const host = await sensorHost(sensor);
  if (host === undefined) {
    // A viewer whose node name never sends stats must set host in esproxy-sensors.
    // Capture searches files before its first stats doc, so only warn for sessions.
    if (sessions && !sensor.loggedNoHost) {
      sensor.loggedNoHost = true;
      console.log(`WARNING - sensor ${sensor.node} has no stats doc or 'host' in esproxy-sensors, it only sees its own node's sessions`);
    }
    return [sensor.node];
  }
  const others = Object.values(sensors).filter(other => other !== sensor);
  // Another sensor's stats doc without a hostname only leaves that sensor out
  await pinHostsFromStats(others.filter(other => knownHost(other) === undefined)).catch(() => {});
  return [sensor.node, ...others.filter(other => knownHost(other) === host).map(other => other.node)];
}

// Pin sensors from the hostnames in their stats docs, a mget is realtime
async function pinHostsFromStats (unpinned) {
  if (unpinned.length === 0) { return; }
  const { statusCode, body } = await esRequestJson('POST', `/${prefix}stats/_mget?_source=hostname`, { ids: unpinned.map(sensor => sensor.node) });
  if (statusCode !== 200 || !Array.isArray(body?.docs)) { return; }
  pinFromStatsDocs(body.docs);
}

function pinFromStatsDocs (docs) {
  for (const doc of docs) {
    if (doc.found && ArkimeUtil.isString(doc._source?.hostname) && Object.hasOwn(sensors, doc._id)) {
      sensors[doc._id].pinnedHost = doc._source.hostname;
      console.log(`Pinning sensor ${doc._id} to hostname %s`, ArkimeUtil.sanitizeStr(doc._source.hostname));
    }
  }
}

// Viewers use the stats hostname to decide which nodes are on their host, so
// a sensor can't claim another one. Without a host set the first hostname seen sticks.
async function validateStatsDoc (req) {
  try {
    const json = parseDocBody(req);
    if (typeof json !== 'object' || json === null || !ArkimeUtil.isString(json.hostname)) { return false; }
    const host = await sensorHost(req.sensor);
    // No stats doc yet, this one sets the hostname. Nothing is pinned here since the
    // write can still fail, the next request pins from the doc es has (a get is realtime)
    // which also keeps several esProxy in agreement.
    if (host === undefined) {
      console.log(`First stats doc for sensor ${req.sensor.node} hostname %s`, ArkimeUtil.sanitizeStr(json.hostname));
      return true;
    }
    return json.hostname === host;
  } catch (e) {
    return false;
  }
}

// Pin sensors without a host to the hostname already in their stats doc, so a
// restart doesn't let the next stats doc pick a new one
async function loadPinnedHosts () {
  const ids = Object.keys(sensors).filter(sensor => sensors[sensor].host === undefined);
  if (ids.length === 0) { return; }

  // A bad url or credentials won't fix themselves, exit instead of never listening
  const fatal = (msg) => {
    console.log(`ERROR - loading stats hostnames to pin esproxy-sensors, ${msg}`);
    process.exit(1);
  };
  if (!ArkimeUtil.isString(elasticsearch) || !URL.canParse(elasticsearch)) { fatal('invalid elasticsearch url'); }

  while (true) {
    let res;
    try {
      res = await esRequestJson('POST', `/${prefix}stats/_mget?_source=hostname`, { ids });
    } catch (err) {
      // es not up yet or not reachable
      console.log('ERROR - loading stats hostnames to pin esproxy-sensors, retrying', ArkimeUtil.sanitizeStr(err?.message ?? String(err)));
      await new Promise(resolve => setTimeout(resolve, 5000));
      continue;
    }
    const { statusCode, body } = res;
    // No stats index yet means no stats docs to pin from
    if (statusCode === 404) { return; }
    if (statusCode === 200 && Array.isArray(body?.docs)) {
      pinFromStatsDocs(body.docs);
      return;
    }
    if (statusCode < 500 && statusCode !== 429) { fatal(`status ${statusCode}${body === undefined ? ' with a non JSON response' : ''}`); }
    console.log(`ERROR - loading stats hostnames to pin esproxy-sensors, status ${statusCode}, retrying`);
    await new Promise(resolve => setTimeout(resolve, 5000));
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

// Viewers key dstats by nodeName, so it must be the sensor's own
function validateDstatsDoc (req) {
  try {
    const json = parseDocBody(req);
    return typeof json === 'object' && json !== null && json.nodeName === req.sensor.node;
  } catch (e) {
    return false;
  }
}

// Partial doc update only, and it can't move the file to another node or change
// the path or lock that viewers trust when reading and expiring the file
function validateFilesUpdate (req) {
  try {
    const json = parseDocBody(req);
    return Object.keys(json).length === 1 && typeof json.doc === 'object' && json.doc !== null &&
      (json.doc.node === undefined || json.doc.node === req.sensor.node) &&
      !Object.hasOwn(json.doc, 'name') && !Object.hasOwn(json.doc, 'indexFilename') && !Object.hasOwn(json.doc, 'locked');
  } catch (e) {
    return false;
  }
}

// Post requests
// Express 4 doesn't catch a rejected async handler, and an unhandled rejection
// would exit esProxy for every sensor
app.post('*', saveBody, (req, res) => {
  handlePost(req, res).catch(err => {
    console.log('POST error', ArkimeUtil.sanitizeStr(err?.message ?? String(err)));
    if (!res.headersSent) { res.status(500).send('Internal proxy error'); }
  });
});

// The sensor viewer's startup "no users" check. Drop the body, a query would let
// a sensor count users matching guesses about their secrets.
function dropUsersCountBody (req) {
  req.body = {};
  req._body = false;
  delete req.headers['content-encoding'];
  return true;
}

async function handlePost (req, res) {
  const path = req.esPath;

  // These may look up a sensor's pinned host or existing fields in es
  const statsOk = path === `/${prefix}stats/_doc/${req.sensor.node}` && await validateStatsDoc(req);
  const filesSearchOk = path === `/${prefix}files/_search` && await validateFilesSearch(req);
  const bulkOk = path.startsWith('/_bulk') && await validateBulk(req);
  const sessionsSearchOk = isSessionsIndicesPath(path, '_search', true) && await validateSessionsSearch(req);
  const updateOk = isSessionsDocPath(path, '_update') && await validateUpdate(req);

  // Empty IFs since those are allowed requests and will run code at end
  if (postExact[path] && validateSearchBody(req)) {
  } else if (path === `/${prefix}users/_count` && dropUsersCountBody(req)) {
  } else if (path === `/${prefix}sequence/_doc/fn-${req.sensor.node}`) {
  } else if (statsOk) {
  } else if (isOwnDstatsDoc(path, req.sensor.node) && validateDstatsDoc(req)) {
  } else if (isOwnFilesDoc(path, req.sensor.node) && validateFilesDoc(req)) {
  } else if (isOwnFilesDoc(path, req.sensor.node, '_update') && validateFilesUpdate(req)) {
  } else if (bulkOk) {
  } else if (filesSearchOk) {
  } else if (sessionsSearchOk) {
  } else if (historyDocRe.test(path) && validateHistoryDoc(req)) {
  } else if (updateOk) {
    console.log(`UPDATE : ${req.sensor.node} path:>%s<:`, ArkimeUtil.sanitizeStr(path));
    if (Config.debug) {
      console.log(req.body.toString('utf8'));
    }
  } else {
    console.log(`POST failed node: ${req.sensor.node} path:>%s<:`, ArkimeUtil.sanitizeStr(path));
    if (Config.debug && Buffer.isBuffer(req.body)) {
      console.log(req.body.toString('utf8'));
    }
    return res.status(400).send('Not authorized for API');
  }
  if (req.bulkEmpty) {
    return res.send({ took: 0, errors: false, items: [] });
  }
  doProxy(req, res).catch(e => {
    console.log('Proxy error', e);
    res.status(500).send('Internal proxy error');
  });
}

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

  // Before listening, otherwise a stats doc could beat the pin
  await loadPinnedHosts();
  const unpinned = Object.values(sensors).filter(sensor => knownHost(sensor) === undefined);
  if (unpinned.length > 0) {
    console.log(`WARNING - ${unpinned.length} esproxy-sensors have no 'host' and no stats doc yet, their first stats doc will set the hostname viewers trust for them and whose sessions they can read through esProxy. Set 'host' to choose it instead.`);
  }

  ArkimeUtil.createHttpServer(app, Config.get('esProxyHost'), Config.get('esProxyPort', '7200'));
}

main();
