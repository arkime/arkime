/******************************************************************************/
/* notifier.syslog.js  -- syslog notifier implementation
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const dgram = require('dgram');
const net = require('net');
const tls = require('tls');

// RFC 5424 facility codes
const FACILITIES = {
  kern: 0, user: 1, mail: 2, daemon: 3, auth: 4, syslog: 5,
  lpr: 6, news: 7, uucp: 8, cron: 9, authpriv: 10, ftp: 11,
  local0: 16, local1: 17, local2: 18, local3: 19,
  local4: 20, local5: 21, local6: 22, local7: 23
};

// RFC 5424 severity codes
const SEVERITIES = {
  emerg: 0, alert: 1, crit: 2, err: 3,
  warning: 4, notice: 5, info: 6, debug: 7
};

exports.init = function (api) {
  api.register('syslog', {
    name: 'Syslog',
    type: 'syslog',
    fields: [{
      name: 'host',
      required: true,
      description: 'Syslog server hostname or IP address'
    }, {
      name: 'port',
      required: true,
      description: 'Syslog server port (typically 514 for UDP/TCP or 6514 for TLS)'
    }, {
      name: 'protocol',
      required: true,
      description: 'Transport protocol: udp, tcp, or tls'
    }, {
      name: 'facility',
      description: 'Syslog facility (default: local0). Options: kern, user, mail, daemon, auth, syslog, lpr, news, uucp, cron, authpriv, ftp, local0-local7'
    }, {
      name: 'severity',
      description: 'Syslog severity (default: warning). Options: emerg, alert, crit, err, warning, notice, info, debug'
    }, {
      name: 'tag',
      description: 'Syslog app-name / tag (default: arkime)'
    }, {
      name: 'insecure',
      type: 'checkbox',
      description: 'Disable TLS certificate verification (not recommended)'
    }],
    sendAlert: exports.sendSyslogAlert
  });
};

/**
 * Build an RFC 5424 syslog message
 */
exports.buildSyslogMessage = function (config, message) {
  const facility = FACILITIES[config.facility] ?? FACILITIES.local0;
  const severity = SEVERITIES[config.severity] ?? SEVERITIES.warning;
  const pri = (facility * 8) + severity;
  const tag = config.tag || 'arkime';
  const timestamp = new Date().toISOString();
  const hostname = require('os').hostname();

  // RFC 5424: <PRI>VERSION TIMESTAMP HOSTNAME APP-NAME PROCID MSGID STRUCTURED-DATA MSG
  return `<${pri}>1 ${timestamp} ${hostname} ${tag} ${process.pid} - - ${message}`;
};

exports.sendSyslogAlert = function (config, message, links, cb) {
  if (!config.host || !config.port || !config.protocol) {
    console.error('Please fill out the required fields for Syslog notifications on the Settings page.');
    if (cb) { cb({ errors: { syslog: 'Missing required Syslog fields' } }); }
    return;
  }

  const protocol = config.protocol.toLowerCase();
  if (protocol !== 'udp' && protocol !== 'tcp' && protocol !== 'tls') {
    if (cb) { cb({ errors: { syslog: 'Protocol must be udp, tcp, or tls' } }); }
    return;
  }

  const port = parseInt(config.port, 10);
  if (!Number.isInteger(port) || port < 1 || port > 65535) {
    if (cb) { cb({ errors: { syslog: 'Invalid port (must be 1-65535)' } }); }
    return;
  }

  if (links && links.length) {
    for (const link of links) {
      message += ` ${link.text}: ${link.url}`;
    }
  }

  const syslogMsg = exports.buildSyslogMessage(config, message);

  if (protocol === 'udp') {
    exports.sendUdp(config, syslogMsg, cb);
  } else {
    exports.sendTcp(config, syslogMsg, protocol === 'tls', cb);
  }
};

exports.sendUdp = function (config, message, cb) {
  const client = dgram.createSocket('udp4');
  const buf = Buffer.from(message);
  const port = parseInt(config.port, 10);

  client.send(buf, 0, buf.length, port, config.host, (err) => {
    client.close();
    if (err) {
      console.error('Syslog UDP error:', err.message);
      if (cb) { cb({ errors: { syslog: err.message } }); }
    } else {
      if (cb) { cb({}); }
    }
  });
};

exports.sendTcp = function (config, message, useTls, cb) {
  const connectFn = useTls ? tls.connect : net.connect;
  const options = { host: config.host, port: parseInt(config.port, 10) };
  if (useTls) {
    options.rejectUnauthorized = !config.insecure;
  }

  const client = connectFn(options, () => {
    // RFC 6587: octet-counting framing for TCP syslog
    const msgBuf = Buffer.from(message);
    const frame = `${msgBuf.length} ${message}`;
    client.end(frame);
  });

  client.on('error', (err) => {
    console.error(`Syslog ${useTls ? 'TLS' : 'TCP'} error:`, err.message);
    if (cb) { cb({ errors: { syslog: err.message } }); }
    cb = null; // prevent double callback
  });

  client.on('close', () => {
    if (cb) { cb({}); }
    cb = null;
  });

  client.setTimeout(10000, () => {
    client.destroy();
    if (cb) { cb({ errors: { syslog: 'Connection timed out' } }); }
    cb = null;
  });
};
