/******************************************************************************/
/* notifier.snmp.js  -- SNMP notifier implementation
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const snmp = require('net-snmp');

// Use maps for security
const VERSIONS = new Map([
  ['1', snmp.Version1],
  ['2c', snmp.Version2c],
  ['3', snmp.Version3]
]);

function getVersion (config) {
  return VERSIONS.get(String(config.version || '2c'));
}

const AUTH_PROTOCOLS = new Map([
  ['none', snmp.AuthProtocols.none],
  ['md5', snmp.AuthProtocols.md5],
  ['sha', snmp.AuthProtocols.sha],
  ['sha224', snmp.AuthProtocols.sha224],
  ['sha256', snmp.AuthProtocols.sha256],
  ['sha384', snmp.AuthProtocols.sha384],
  ['sha512', snmp.AuthProtocols.sha512]
]);

const PRIV_PROTOCOLS = new Map([
  ['none', snmp.PrivProtocols.none],
  ['des', snmp.PrivProtocols.des],
  ['aes', snmp.PrivProtocols.aes],
  ['aes256b', snmp.PrivProtocols.aes256b],
  ['aes256r', snmp.PrivProtocols.aes256r]
]);

// Map keys are lowercase strings, config values can be mixed case or missing
function protoName (value) {
  return String(value || 'none').toLowerCase();
}

// SNMPv3 settings that don't add up used to quietly send noAuthNoPriv traps, refuse them instead.
// Returns an error string, or undefined when ok.
function checkV3 (config) {
  if (!config.v3User) { return 'Security username is required for SNMPv3'; }
  const auth = protoName(config.v3AuthProtocol);
  const priv = protoName(config.v3PrivProtocol);
  if (!AUTH_PROTOCOLS.has(auth)) { return 'Unknown SNMPv3 auth protocol'; }
  if (!PRIV_PROTOCOLS.has(priv)) { return 'Unknown SNMPv3 privacy protocol'; }
  if (auth === 'none' && config.v3AuthKey) { return 'SNMPv3 auth key is set but auth protocol is none'; }
  if (auth !== 'none' && !config.v3AuthKey) { return 'SNMPv3 auth protocol needs an auth key'; }
  if (priv === 'none' && config.v3PrivKey) { return 'SNMPv3 privacy key is set but privacy protocol is none'; }
  if (priv !== 'none' && !config.v3PrivKey) { return 'SNMPv3 privacy protocol needs a privacy key'; }
  if (priv !== 'none' && auth === 'none') { return 'SNMPv3 privacy needs an auth protocol'; }
}

// Everything sendSnmpAlert would refuse, so the settings page refuses it first
exports.validate = function (config) {
  const version = getVersion(config);
  if (version === undefined) { return 'Version must be 1, 2c, or 3'; }
  if (version === snmp.Version3) { return checkV3(config); }
  if (!config.community) { return 'Community string is required for SNMPv1/v2c'; }
};

// Default enterprise OID for Arkime Parliament alerts
const DEFAULT_TRAP_OID = '1.3.6.1.4.1.79054.1.1';

// OID for the alert message varbind
const ALERT_MESSAGE_OID = '1.3.6.1.4.1.79054.1.1.1';

exports.init = function (api) {
  api.register('snmp', {
    name: 'SNMP',
    type: 'snmp',
    fields: [{
      name: 'host',
      required: true,
      description: 'SNMP trap receiver hostname or IP address'
    }, {
      name: 'port',
      description: 'SNMP trap receiver port (default: 162)'
    }, {
      name: 'version',
      description: 'SNMP version: 1, 2c, or 3 (default: 2c)'
    }, {
      name: 'community',
      type: 'secret',
      description: 'SNMP community string for v1/v2c (e.g. public)'
    }, {
      name: 'v3User',
      description: 'SNMPv3 security username (required for v3)'
    }, {
      name: 'v3AuthProtocol',
      description: 'SNMPv3 auth protocol: none, md5, sha, sha224, sha256, sha384, sha512 (default: none)'
    }, {
      name: 'v3AuthKey',
      type: 'secret',
      description: 'SNMPv3 authentication key/passphrase'
    }, {
      name: 'v3PrivProtocol',
      description: 'SNMPv3 privacy protocol: none, des, aes, aes256b, aes256r (default: none)'
    }, {
      name: 'v3PrivKey',
      type: 'secret',
      description: 'SNMPv3 privacy/encryption key/passphrase'
    }, {
      name: 'trapOid',
      description: `Enterprise-specific trap OID (default: ${DEFAULT_TRAP_OID})`
    }],
    sendAlert: exports.sendSnmpAlert,
    validate: exports.validate
  });
};

exports.sendSnmpAlert = function (config, message, links, cb) {
  if (!config.host) {
    console.error('Please fill out the required fields for SNMP notifications on the Settings page.');
    if (cb) { cb({ errors: { snmp: 'Missing required SNMP fields' } }); }
    return;
  }

  // Notifiers saved before validate existed could still have a bad combination
  const error = exports.validate(config);
  if (error) {
    if (cb) { cb({ errors: { snmp: error } }); }
    return;
  }
  const version = getVersion(config);

  if (links && links.length) {
    for (const link of links) {
      message += ` ${link.text}: ${link.url}`;
    }
  }

  let port = 162;
  if (config.port !== undefined && config.port !== null && config.port !== '') {
    port = parseInt(config.port, 10);
    if (!Number.isInteger(port) || port < 1 || port > 65535) {
      if (cb) { cb({ errors: { snmp: 'Invalid port (must be 1-65535)' } }); }
      return;
    }
  }
  const trapOid = config.trapOid || DEFAULT_TRAP_OID;

  let session;
  if (version === snmp.Version3) {
    const authProto = AUTH_PROTOCOLS.get(protoName(config.v3AuthProtocol));
    const privProto = PRIV_PROTOCOLS.get(protoName(config.v3PrivProtocol));

    let level = snmp.SecurityLevel.noAuthNoPriv;
    if (authProto !== snmp.AuthProtocols.none && privProto !== snmp.PrivProtocols.none) {
      level = snmp.SecurityLevel.authPriv;
    } else if (authProto !== snmp.AuthProtocols.none) {
      level = snmp.SecurityLevel.authNoPriv;
    }

    const user = {
      name: config.v3User,
      level,
      authProtocol: authProto,
      authKey: config.v3AuthKey || '',
      privProtocol: privProto,
      privKey: config.v3PrivKey || ''
    };

    session = snmp.createV3Session(config.host, user, {
      port,
      trapPort: port
    });
  } else {
    session = snmp.createSession(config.host, config.community, {
      version,
      port,
      trapPort: port
    });
  }

  const varbinds = [{
    oid: ALERT_MESSAGE_OID,
    type: snmp.ObjectType.OctetString,
    value: message
  }];

  session.trap(trapOid, varbinds, (err) => {
    session.close();
    if (err) {
      console.error('SNMP trap error:', err.message);
      if (cb) { cb({ errors: { snmp: err.message } }); }
    } else {
      if (cb) { cb({}); }
    }
  });
};
