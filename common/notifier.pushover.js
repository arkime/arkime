/******************************************************************************/
/* notifier.pushover.js  -- Pushover notifier implementation
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const cryptoLib = require('crypto');
const zlib = require('zlib');
const NotifierHttp = require('./notifierHttp');

const PUSHOVER_URL = 'https://api.pushover.net/1/messages.json';

// Pushover limits, https://pushover.net/api#limits
const MAX_MESSAGE = 1024;
const MAX_TITLE = 250;
const MAX_URL = 512;
const MAX_URL_TITLE = 100;

// Emergency (priority 2) alerts repeat every retry seconds until acknowledged,
// until expire seconds pass, or until Pushover has repeated it MAX_REPEATS times
const MIN_RETRY = 30;
const MAX_EXPIRE = 10800;
const MAX_REPEATS = 50;
const DEFAULT_RETRY = 60;

const PRIORITIES = new Set(['-2', '-1', '0', '1', '2']);

// What alert senders escape with ArkimeUtil.safeStr
const ENTITIES = { '&amp;': '&', '&lt;': '<', '&gt;': '>', '&quot;': '"', '&#39;': '\'', '&#47;': '/' };

const FIELDS = [{
  name: 'appToken',
  required: true,
  type: 'secret',
  description: 'Pushover application API token, from an application created at https://pushover.net/apps/build'
}, {
  name: 'userKey',
  required: true,
  type: 'secret',
  description: 'Pushover user or group key to send the alert to'
}, {
  name: 'device',
  description: 'Comma separated device names to send to (default: all of the user\'s devices)'
}, {
  name: 'priority',
  description: 'Priority: -2 (no notification), -1 (quiet), 0 (normal), 1 (high, bypasses quiet hours), or 2 (emergency, repeats until acknowledged) (default: 0)'
}, {
  name: 'retry',
  description: `Emergency priority only, seconds between repeats, at least ${MIN_RETRY} (default: ${DEFAULT_RETRY})`
}, {
  name: 'expire',
  description: `Emergency priority only, seconds to keep repeating, at most ${MAX_EXPIRE} and at most ${MAX_REPEATS} times retry since Pushover stops after ${MAX_REPEATS} repeats (default: the longest allowed)`
}, {
  name: 'sound',
  description: 'Sound name, such as pushover, siren, persistent, or none, or a custom sound (default: the user\'s default sound)'
}, {
  name: 'title',
  description: 'Title of the alert (default: the Pushover application name)'
}, {
  name: 'encryptionKey',
  type: 'secret',
  description: 'End-to-end encryption key, the 64 character hex key from the Pushover app. The message and title are encrypted so only your devices can read them, links are sent in the message'
}];

exports.init = function (api) {
  api.register('pushover', {
    name: 'Pushover',
    type: 'pushover',
    fields: FIELDS,
    sendAlert: exports.sendPushoverAlert,
    validate: exports.validate
  });
};

// Field values are strings, but trim them and treat blank as unset
function value (config, field) {
  const v = config[field];
  if (typeof v !== 'string') { return undefined; }
  const str = v.trim();
  return str.length ? str : undefined;
}

// Whole number strings only, parseInt would take '30s' or '1e3' as a number
function wholeNumber (str) {
  return /^\d+$/.test(str) ? parseInt(str, 10) : NaN;
}

// Pushover counts characters, not UTF-16 units or bytes
function charCount (str) {
  return Array.from(str).length;
}

// Alert senders HTML escape what they put in the message (ArkimeUtil.safeStr)
// and mark *emphasis* for Slack and Teams. Pushover is sent plain text, which
// can't carry markup either way, so undo both rather than show &#39; and
// asterisks. One pass, so &amp;lt; becomes &lt; and not <. Emphasis is only
// stripped when it opens after whitespace and wraps text that starts and ends
// with a non-space, so a * in a url or a lone * in a name stays.
exports.plainText = function (message) {
  return String(message)
    .replace(/&(?:amp|lt|gt|quot|#39|#47);/g, (entity) => ENTITIES[entity])
    .replace(/(^|\s)\*([^*\s](?:[^*\n]*[^*\s])?)\*(?=[\s.,:;!?]|$)/gm, '$1$2')
    .trim();
};

/**
 * Cut body, keeping suffix whole, until encode(body + suffix) is at most max
 * by size. Cuts on code points so a character is never split, and marks the
 * cut with an ellipsis. If the suffix alone doesn't fit, it gets cut as well.
 * @ignore
 */
function fit (body, suffix, max, encode, size) {
  const whole = encode(body + suffix);
  if (size(whole) <= max) { return whole; }

  // Binary search for the most characters of body that still fit
  const chars = Array.from(body);
  let best;
  let lo = 0;
  let hi = chars.length - 1;
  while (lo <= hi) {
    const mid = (lo + hi) >> 1;
    const out = encode(chars.slice(0, mid).join('') + '…' + suffix);
    if (size(out) <= max) {
      best = out;
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }

  if (best !== undefined) { return best; }
  if (suffix) { return fit(body + suffix, '', max, encode, size); }
  return encode('…'); // can't happen with the Pushover limits, but never recurse forever
}

// Everything sendPushoverAlert would refuse, so the settings page refuses it first.
// Returns an error string, or undefined when ok.
exports.validate = function (config) {
  for (const field of FIELDS) {
    if (config[field.name] !== undefined && typeof config[field.name] !== 'string') {
      return `${field.name} must be text`;
    }
    if (field.required && value(config, field.name) === undefined) {
      return `Missing a value for ${field.name}`;
    }
  }

  const priority = value(config, 'priority');
  if (priority !== undefined && !PRIORITIES.has(priority)) {
    return 'Priority must be -2, -1, 0, 1, or 2';
  }

  const retryStr = value(config, 'retry');
  const retry = retryStr === undefined ? DEFAULT_RETRY : wholeNumber(retryStr);
  if (!(retry >= MIN_RETRY)) {
    return `Retry must be a number of seconds, at least ${MIN_RETRY}`;
  }

  const expireStr = value(config, 'expire');
  if (expireStr !== undefined) {
    const expire = wholeNumber(expireStr);
    if (!(expire >= 1 && expire <= MAX_EXPIRE)) {
      return `Expire must be a number of seconds, from 1 to ${MAX_EXPIRE}`;
    }
    if (priority === '2' && expire > retry * MAX_REPEATS) {
      return `Pushover stops an emergency alert after ${MAX_REPEATS} repeats, so expire can be at most ${MAX_REPEATS} times retry (${retry * MAX_REPEATS})`;
    }
  }

  const key = value(config, 'encryptionKey');
  if (key !== undefined && !/^[0-9a-fA-F]{64}$/.test(key)) {
    return 'Encryption key must be 64 hex characters';
  }
};

/**
 * Encrypt one field with the Pushover end-to-end encryption scheme,
 * https://pushover.net/api#e2ee
 * base64(IV + AES-256-CBC(gzip(text)) + HMAC-SHA256(IV + ciphertext)), same key for both
 * @ignore
 */
exports.encryptField = function (keyHex, text) {
  const key = Buffer.from(keyHex, 'hex');
  const iv = cryptoLib.randomBytes(16);
  const cipher = cryptoLib.createCipheriv('aes-256-cbc', key, iv);
  const ciphertext = Buffer.concat([cipher.update(zlib.gzipSync(Buffer.from(text, 'utf8'), { level: 9 })), cipher.final()]);
  const hmac = cryptoLib.createHmac('sha256', key).update(iv).update(ciphertext).digest();
  return Buffer.concat([iv, ciphertext, hmac]).toString('base64');
};

/**
 * Build the Pushover API parameters for an alert, call validate first
 * @ignore
 */
exports.buildPushoverParams = function (config, message, links) {
  // Pushover can't read encrypted fields and doesn't say what it limits them
  // to, so keep each encrypted value within the plain text limit, which also
  // keeps what the device decrypts within it. The ciphertext length only
  // depends on the gzip length, so the random IV doesn't change what fits.
  const key = value(config, 'encryptionKey');
  const encode = key ? (str) => exports.encryptField(key, str) : (str) => str;
  const size = key ? (str) => str.length : charCount;

  // Pushover takes one link, the rest go in the message like the sms notifier
  // does. An encrypted url_title is at least 108 characters, over its limit of
  // 100 even when empty, so when encrypting every link goes in the message.
  let url;
  let suffix = '';
  for (const link of links ?? []) {
    if (!key && !url && charCount(String(link.url)) <= MAX_URL) {
      url = { url: String(link.url), url_title: fit(String(link.text), '', MAX_URL_TITLE, (str) => str, charCount) };
    } else {
      suffix += `\n${link.text}: ${link.url}`;
    }
  }

  const params = {
    token: value(config, 'appToken'),
    user: value(config, 'userKey').replace(/\s/g, ''),
    message: fit(exports.plainText(message), suffix, MAX_MESSAGE, encode, size)
  };

  const title = value(config, 'title');
  if (title) { params.title = fit(title, '', MAX_TITLE, encode, size); }
  if (url) { Object.assign(params, url); }

  const device = value(config, 'device');
  if (device) { params.device = device.replace(/\s/g, ''); }

  const sound = value(config, 'sound');
  if (sound) { params.sound = sound; }

  const priority = value(config, 'priority');
  if (priority) {
    params.priority = priority;
    if (priority === '2') {
      const retry = value(config, 'retry') ?? String(DEFAULT_RETRY);
      params.retry = retry;
      params.expire = value(config, 'expire') ?? String(Math.min(parseInt(retry, 10) * MAX_REPEATS, MAX_EXPIRE));
    }
  }

  if (key) { params.encrypted = '1'; }

  return params;
};

// A rejected alert comes back as a 4xx with {status: 0, errors: [...]}
async function checkResponse (response) {
  const json = await response.json().catch(() => ({}));
  if (response.ok && json.status === 1) { return; }
  const reason = Array.isArray(json.errors) ? json.errors.join(', ') : '';
  return `Pushover returned ${response.status}${reason ? `: ${reason.slice(0, 200)}` : ''}`;
}

exports.sendPushoverAlert = function (config, message, links, cb) {
  const invalid = exports.validate(config);
  if (invalid) {
    console.error(`Pushover notifier: ${invalid}, fix it on the Settings page to enable Pushover notifications`);
    if (cb) { cb({ errors: { pushover: invalid } }); }
    return;
  }

  return NotifierHttp.postAlert('pushover', 'Pushover', PUSHOVER_URL, {
    body: new URLSearchParams(exports.buildPushoverParams(config, message, links))
  }, checkResponse, cb);
};
