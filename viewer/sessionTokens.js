/* sessionTokens.js -- the *Tokens fields of a session, for sessions dbs that don't analyze text like ES
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const ArkimeUtil = require('../common/arkimeUtil');

// The *Tokens fields capture defines, used when the fields db doesn't say
const DEFAULT_TOKENS = ['dhcp.hostTokens', 'dns.hostTokens', 'email.hostTokens', 'http.hostTokens', 'http.uriTokens',
  'http.useragentTokens', 'oracle.hostTokens', 'quic.hostTokens', 'smb.hostTokens', 'socks.hostTokens'];

// Sensors can define fields, so bound how much work they add to every doc
const MAX_TOKENS = 256;
const MAX_TOKENS_DEPTH = 8;
const MAX_TOKENS_LEN = 256;
// Every field walks the doc's arrays on its path, so a doc gets this many values walked in all
const MAX_TOKENS_WORK = 1000000;

// ES's wordSplit analyzer: lowercased, split on anything outside [A-Za-z0-9_]
function wordSplit (str) {
  return (str.match(/[A-Za-z0-9_]+/g) ?? []).map(t => t.toLowerCase());
}

/**
 * The *Tokens dbFields, the defaults included, as the { segs, key } addTokens takes
 */
function tokenFields (dbFields = []) {
  return [...new Set([...DEFAULT_TOKENS, ...dbFields])].filter(dbField => dbField.endsWith('Tokens') && dbField.length <= MAX_TOKENS_LEN).map(dbField => {
    const segs = dbField.slice(0, -'Tokens'.length).split('.');
    return { segs, key: `${segs.at(-1)}Tokens` };
  }).filter(({ segs }) => segs.length <= MAX_TOKENS_DEPTH && segs.every(seg => seg !== '' && !ArkimeUtil.isProtoKey(seg)))
    .slice(0, MAX_TOKENS);
}

// Sets the tokens of the field at segs beside it, in every object of an array on the way.
// Iterative, a sensor's doc can nest arrays deeper than the stack.
function addTokens (doc, segs, tokensKey, work) {
  const spend = (n) => {
    work.left -= n;
    if (work.left < 0) { throw new Error(`More than ${MAX_TOKENS_WORK} values to tokenize`); }
  };
  const stack = [[doc, 0]];
  while (stack.length > 0) {
    const [obj, depth] = stack.pop();
    if (Array.isArray(obj)) {
      spend(obj.length);
      for (const o of obj) { stack.push([o, depth]); }
      continue;
    }
    if (obj === null || typeof obj !== 'object') { continue; }
    if (depth < segs.length - 1) {
      stack.push([obj[segs[depth]], depth + 1]);
      continue;
    }
    const value = obj[segs[depth]];
    if (value === undefined || value === null) { continue; }
    if (Array.isArray(value)) { spend(value.length); }
    obj[tokensKey] = [].concat(value).flatMap(v => typeof v === 'string' ? wordSplit(v) : []);
  }
}

/**
 * Add every one of the tokenFields to doc, throws when the doc's arrays would take too long
 */
function addAllTokens (doc, fields) {
  const work = { left: MAX_TOKENS_WORK };
  for (const { segs, key } of fields) { addTokens(doc, segs, key, work); }
}

module.exports = { tokenFields, addAllTokens };
