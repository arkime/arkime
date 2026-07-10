#!/usr/bin/env node
/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Mini Splunk + Databricks server for testing the WISE source.splunk.js and
 * source.databricks.js sources. Everything is in memory and all auth is
 * ignored.
 *
 * Both SDKs insist on talking HTTPS to their host, so this server serves TLS
 * with an embedded self-signed cert (CN=localhost). The splunk-sdk sets
 * rejectUnauthorized:false itself; the databricks SDK does not, so the wise
 * test process is launched with NODE_TLS_REJECT_UNAUTHORIZED=0 (see tests.pl).
 *
 * Splunk  - plain REST/JSON: /services/auth/login, /services/server/info,
 *           and a oneshot search POST to .../search/jobs.
 * Databricks - Thrift TBinaryProtocol over HTTP POST (Content-Type
 *           application/x-thrift), the Hive TCLIService API. We reuse the
 *           real thrift library + the generated TCLIService types shipped with
 *           @databricks/sql so the wire encoding is always correct. Responses
 *           use "direct results" so no separate fetch round-trip is needed.
 *
 * Usage: node mini-wise-source.js [--debug] <port>
 *
 * To add fake data, edit SPLUNK_ROWS / DATABRICKS_ROWS below. Each row is a
 * plain object whose properties become columns; `ip` is the key column and the
 * other properties are matched by the source's `fields`/`shortcut` config.
 */
'use strict';

const https = require('https');
const path = require('path');
const thrift = require('thrift');

// The databricks generated thrift types live outside the package `exports`
// map, so resolve them relative to the installed package.
const dbsqlDir = path.dirname(require.resolve('@databricks/sql'));
const TCLIService = require(path.join(dbsqlDir, '..', 'thrift', 'TCLIService.js'));
const ttypes = require(path.join(dbsqlDir, '..', 'thrift', 'TCLIService_types.js'));

let debug = 0;
if (process.argv[2] === '--debug') {
  debug = 1;
  process.argv.splice(2, 1);
}
const port = parseInt(process.argv[2], 10) || 9998;

// ----------------------------------------------------------------------------
// Fake data. Edit these to add/change what the sources return.
// ----------------------------------------------------------------------------
const SPLUNK_ROWS = [
  { ip: '66.66.66.66', threat: 'splunk-malware' },
  { ip: '66.66.66.67', threat: 'splunk-botnet' }
];

const DATABRICKS_ROWS = [
  { ip: '77.77.77.77', threat: 'databricks-c2' },
  { ip: '77.77.77.78', threat: 'databricks-phish' }
];

// ----------------------------------------------------------------------------
// Embedded self-signed cert for localhost (valid ~100 years).
// ----------------------------------------------------------------------------
const KEY = `-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDGW+gGgl2gI/mB
Ng4vrriBuT6rf5jHZWm8M9mXHsrlR6pwIELNuaFl6xKCMEN3zMqmIEFUVKvWG7lJ
PFDN4ljfNRLJRaFfbRFeYOmSWmaw6+pSfeUqJqsLZXjFQh10f0QESkS45r5LPIx1
V6fCKPMnZ5DeIZJ+fhE9BcVJ3CHnmqjE4GJqXx8CXsbpgdwubZNwT/lFvv28miiS
9LfjgCePRaPkinqUlqHU8guJXrFlCVhq+M90CJjtukwu++4dZ316Lx5f6nzBQzSz
uyn202ENDZkI/b4p20og+XhEgUh/abVVskXiCkPv3wgDCa7KN6kab+LFFD7qWhU9
A6n2/HGDAgMBAAECggEBAJBZ39+kzZe1tmQ2v1op73HQKnBJ2hf5kFn67bVRHlx+
q+UPRS5Lkc4GpSCDGQY8zZjZzlEdkTOuV0eZkkBSIVTGXdaFSquURtiE9FWiXisQ
dCT2I2hqXX1cqef7dk4KM6hfl+VrXj6IYLsgQCBHBrS9ZKqSifZtrgPXiDx461+M
YDB7TBUOLgzLRAVSlICmazNM1SvyuA6xf59BT2fvdLCHYova2i6DejfkZX6svoVI
ClWQpH+SPh7yJPwILcpa96hN2HAW4X58uSH0g76QMrl4YkuU79CseByGWWkGuRKx
rjF/niQAlVJGRh+cm8rb/sXZPsMrnbNh9TY6b/frIlkCgYEA7QkMSbFDIjCMiBpC
Jff+8IaoO/RhSTGhb0vQmYPzCBpZ3lB6DSV5SFAAkmaN2/eN9gsCsqkImxW/qX9a
e/3P07CTDzM6z7zVNyCOPpNTKxT3r4gfRf5uTX6FocSULwBhdEak+jfMr78IYaJs
J+TOB3w+iLfdCech7tKAIOaVivcCgYEA1jqwzRiewQji7dXPb/F8JciCfaMVfRkZ
VgJ3sesUPV7shHpiFIcHiBTxkrZovM+5KwwN3pHiUL8IudoZRkRIHqf+nchy4ki1
5dXgPngNzYmftOlE0sGkxi3+QRKaLIyXoAMGJdSTqIpUkVu18piG//HaZG+SGTKF
YdcpnuBYPtUCgYBohbvgZwUmd2gQwBt5KLFHmOlofqvDndoE/NaAS1oIsa39RVl8
oJCpnXWTGRvm6nO0EkjfRYBg+qcoc9sPn+1b+JnwcvO1FRykEXwIBej/r2BFC+5W
bApxq5/7pHZ/f1h58IjhOWfN+5wTiY7NzKw5SsU8fm8+5afl6vbLC0LYIwKBgBzN
OTXpyHY1ZqUJKOFo+wLtaTXQ9jOiaziYDlWaQFdb6rqI5aTS3p1aC3xpD73Kw59Y
+Ihi3qVyeY7bFqjOx09v0JiP+XoYwnPLBGIBrAFlLlaZQgp/xFJsnpFLGfVBVaVK
osn1QYDYUDRWuyiJfyTr9CuqoF7I3wvfbJYSnWqdAoGAOa/UHk1QotM96TNsfM5j
2rXzIEtF0JbuBqKHjFetnWV1AdQAvbepfHd5V5oN2FKLX+pam4BPNDkCDMFAUe5V
rOgXIqepvAWHl/1QREsisltRgHZSheNSF1k3n+XB7IiK7c87Mlf++enmWWUVxw63
qEBsMPdlCLQbx2l4nG47p1k=
-----END PRIVATE KEY-----`;

const CERT = `-----BEGIN CERTIFICATE-----
MIICyzCCAbOgAwIBAgIJAP+sa1l20MjhMA0GCSqGSIb3DQEBCwUAMBQxEjAQBgNV
BAMMCWxvY2FsaG9zdDAgFw0yNjA3MTAxNTQwNDJaGA8yMTI2MDYxNjE1NDA0Mlow
FDESMBAGA1UEAwwJbG9jYWxob3N0MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB
CgKCAQEAxlvoBoJdoCP5gTYOL664gbk+q3+Yx2VpvDPZlx7K5UeqcCBCzbmhZesS
gjBDd8zKpiBBVFSr1hu5STxQzeJY3zUSyUWhX20RXmDpklpmsOvqUn3lKiarC2V4
xUIddH9EBEpEuOa+SzyMdVenwijzJ2eQ3iGSfn4RPQXFSdwh55qoxOBial8fAl7G
6YHcLm2TcE/5Rb79vJookvS344Anj0Wj5Ip6lJah1PILiV6xZQlYavjPdAiY7bpM
LvvuHWd9ei8eX+p8wUM0s7sp9tNhDQ2ZCP2+KdtKIPl4RIFIf2m1VbJF4gpD798I
AwmuyjepGm/ixRQ+6loVPQOp9vxxgwIDAQABox4wHDAaBgNVHREEEzARgglsb2Nh
bGhvc3SHBH8AAAEwDQYJKoZIhvcNAQELBQADggEBAKHLvK7xVDmzzoaqAMwxPfyF
BCzEXHcLWHTHhGXGgV45roMoBVvjCiuXVq0mNcfuKN69RlbaaoAmyaPMZFEsTBRG
3JSmN6a0uHX+2yohQWj2avvzpPKzCl4DxpS02X/E4HiVl2ymAydkC3KHe7ZUnss8
Rx3jRiE/TC3lj4NBqerjEoB9ddptmzaef30+yOXffmZborIKe60gsaCO+LrTvN1i
7YlCLrkGF65NZal7Xn89JbIruxVauOlqeKcl/dmJM/0K4f6R4+Q9Mzlg/anCvn2g
JAX0pBPenRVmpFPfupy4B7ezQBytmQf9JWpwzlREuKznN4ivMMDDxZ5sQSK+3gQ=
-----END CERTIFICATE-----`;

function log (...args) {
  if (debug) { console.log(...args); }
}

// ----------------------------------------------------------------------------
// Splunk REST emulation
// ----------------------------------------------------------------------------
function splunkResults (rows) {
  // Shape matches what oneshotSearch({output_mode:'json'}) returns.
  return JSON.stringify({
    preview: false,
    init_offset: 0,
    fields: [{ name: 'ip' }, { name: 'threat' }],
    results: rows
  });
}

function handleSplunk (req, res, body) {
  const url = req.url.split('?')[0];

  if (url === '/services/auth/login') {
    // The SDK reads response.data.sessionKey
    return sendJSON(res, { sessionKey: 'mini-wise-session' });
  }

  if (url === '/services/server/info') {
    // The SDK reads response.data.generator.version
    return sendJSON(res, {
      generator: { version: '9.0.0', instance_type: 'mini' },
      entry: [{ name: 'mini', content: { version: '9.0.0' } }]
    });
  }

  if (url.endsWith('/search/jobs')) {
    // The source has already substituted %%SEARCHTERM%% into the search, so
    // match any known key that appears in the query string.
    const search = decodeURIComponent((body.match(/(?:^|&)search=([^&]*)/) || [])[1] || '');
    const rows = SPLUNK_ROWS.filter((r) => search.includes(r.ip));
    log(`SPLUNK oneshot search=${search} -> ${rows.length} row(s)`);
    return send(res, 200, 'application/json', splunkResults(rows));
  }

  log(`SPLUNK 404 ${req.method} ${req.url}`);
  return send(res, 404, 'application/json', '{}');
}

// ----------------------------------------------------------------------------
// Databricks Thrift (TCLIService) emulation
// ----------------------------------------------------------------------------
// Must be a valid v4 UUID (byte 6 high nibble = 4, byte 8 high nibble = 8..b):
// the SDK runs guid through uuid.stringify() which validates it.
const GUID = Buffer.from([0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0x4c, 0xde, 0x8f, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd]);
const SECRET = Buffer.from('fedcba9876543210', 'utf8');  // 16 bytes, opaque

function okStatus () {
  return new ttypes.TStatus({ statusCode: ttypes.TStatusCode.SUCCESS_STATUS });
}

function handleIdentifier () {
  return new ttypes.THandleIdentifier({ guid: GUID, secret: SECRET });
}

// Build a COLUMN_BASED result set + metadata for the given rows.
function buildResultSetMetadata () {
  const columns = ['ip', 'threat'].map((colName, i) => new ttypes.TColumnDesc({
    columnName: colName,
    typeDesc: new ttypes.TTypeDesc({
      types: [new ttypes.TTypeEntry({
        primitiveEntry: new ttypes.TPrimitiveTypeEntry({ type: ttypes.TTypeId.STRING_TYPE })
      })]
    }),
    position: i + 1
  }));

  return new ttypes.TGetResultSetMetadataResp({
    status: okStatus(),
    resultFormat: ttypes.TSparkRowSetType.COLUMN_BASED_SET,
    schema: new ttypes.TTableSchema({ columns })
  });
}

function stringColumn (values) {
  return new ttypes.TColumn({
    stringVal: new ttypes.TStringColumn({ values, nulls: Buffer.alloc(0) })
  });
}

function buildRowSet (rows) {
  return new ttypes.TRowSet({
    startRowOffset: 0,
    rows: [],
    columns: [
      stringColumn(rows.map((r) => r.ip)),
      stringColumn(rows.map((r) => r.threat))
    ]
  });
}

const databricksHandler = {
  OpenSession (req, cb) {
    log('DATABRICKS OpenSession');
    cb(null, new ttypes.TOpenSessionResp({
      status: okStatus(),
      // Advertise the newest protocol so the client will send named
      // parameters (needs >= V8). Result format is still forced to
      // COLUMN_BASED_SET below, so no Arrow/LZ4 handling is needed.
      serverProtocolVersion: ttypes.TProtocolVersion.SPARK_CLI_SERVICE_PROTOCOL_V9,
      sessionHandle: new ttypes.TSessionHandle({ sessionId: handleIdentifier() })
    }));
  },

  ExecuteStatement (req, cb) {
    log(`DATABRICKS ExecuteStatement: ${req.statement}`);
    // Non-periodic queries carry a SEARCHTERM named parameter; periodic
    // queries carry none and want the whole table.
    let rows = DATABRICKS_ROWS;
    const term = searchTermParam(req);
    if (term !== undefined) {
      rows = DATABRICKS_ROWS.filter((r) => r.ip === term);
    }

    const operationHandle = new ttypes.TOperationHandle({
      operationId: handleIdentifier(),
      operationType: ttypes.TOperationType.EXECUTE_STATEMENT,
      hasResultSet: true
    });

    cb(null, new ttypes.TExecuteStatementResp({
      status: okStatus(),
      operationHandle,
      directResults: new ttypes.TSparkDirectResults({
        operationStatus: new ttypes.TGetOperationStatusResp({
          status: okStatus(),
          operationState: ttypes.TOperationState.FINISHED_STATE
        }),
        resultSetMetadata: buildResultSetMetadata(),
        resultSet: new ttypes.TFetchResultsResp({
          status: okStatus(),
          hasMoreRows: false,
          results: buildRowSet(rows)
        }),
        closeOperation: new ttypes.TCloseOperationResp({ status: okStatus() })
      })
    }));
  },

  // Defensive fallbacks in case the client does not use direct results.
  GetOperationStatus (req, cb) {
    cb(null, new ttypes.TGetOperationStatusResp({
      status: okStatus(),
      operationState: ttypes.TOperationState.FINISHED_STATE
    }));
  },
  GetResultSetMetadata (req, cb) {
    cb(null, buildResultSetMetadata());
  },
  FetchResults (req, cb) {
    cb(null, new ttypes.TFetchResultsResp({
      status: okStatus(),
      hasMoreRows: false,
      results: buildRowSet(DATABRICKS_ROWS)
    }));
  },
  CloseOperation (req, cb) {
    cb(null, new ttypes.TCloseOperationResp({ status: okStatus() }));
  },
  CloseSession (req, cb) {
    cb(null, new ttypes.TCloseSessionResp({ status: okStatus() }));
  }
};

// Pull the SEARCHTERM named-parameter string value out of an ExecuteStatement
// request, or undefined if there isn't one.
function searchTermParam (req) {
  if (!req.parameters) { return undefined; }
  for (const p of req.parameters) {
    if (p.name === 'SEARCHTERM' && p.value && p.value.stringValue !== undefined && p.value.stringValue !== null) {
      return p.value.stringValue;
    }
  }
  return undefined;
}

function handleThrift (req, res, bodyBuf) {
  const processor = new TCLIService.Processor(databricksHandler);
  const outChunks = [];
  const outTransport = new thrift.TBufferedTransport(null, (buf) => outChunks.push(buf));
  const outProtocol = new thrift.TBinaryProtocol(outTransport);

  const receiver = thrift.TBufferedTransport.receiver((inTransport) => {
    const inProtocol = new thrift.TBinaryProtocol(inTransport);
    try {
      processor.process(inProtocol, outProtocol);
      // Our handlers reply synchronously, so the response is fully flushed now.
      send(res, 200, 'application/x-thrift', Buffer.concat(outChunks));
    } catch (err) {
      log('DATABRICKS thrift error', err);
      send(res, 500, 'text/plain', 'thrift error');
    }
  });
  receiver(bodyBuf);
}

// ----------------------------------------------------------------------------
// HTTP plumbing
// ----------------------------------------------------------------------------
function send (res, code, type, body) {
  res.writeHead(code, { 'Content-Type': type, 'Content-Length': Buffer.byteLength(body) });
  res.end(body);
}

function sendJSON (res, obj) {
  send(res, 200, 'application/json', JSON.stringify(obj));
}

const server = https.createServer({ key: KEY, cert: CERT }, (req, res) => {
  const chunks = [];
  req.on('data', (c) => chunks.push(c));
  req.on('end', () => {
    const bodyBuf = Buffer.concat(chunks);

    if (req.url.split('?')[0] === '/_shutdown') {
      send(res, 200, 'text/plain', 'bye');
      return server.close(() => process.exit(0));
    }

    if ((req.headers['content-type'] || '').includes('application/x-thrift')) {
      return handleThrift(req, res, bodyBuf);
    }

    return handleSplunk(req, res, bodyBuf.toString('utf8'));
  });
});

server.listen(port, '127.0.0.1', () => {
  console.log(`mini-wise-source listening on port ${port}`);
});

process.on('SIGINT', () => process.exit(0));
