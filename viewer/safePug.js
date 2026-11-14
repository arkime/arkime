/* safePug.js -- check that pug from a remote source, like WISE views, is a subset that can't run code
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const pug = require('pug');
const acorn = require('acorn');

const TAGS = new Set(['div', 'span', 'dl', 'dt', 'dd', 'b', 'strong', 'em', 'i', 'br', 'h4', 'ul', 'li']);
const ATTRS = new Set(['class', 'title', 'suffix']);
const BADPROPS = new Set(['constructor', '__proto__', 'prototype']);
// arrayList's later args are raw html and settings, so only the first 4
const MIXINS = { arrayList: 4 };
// The detail html is compiled as a Vue template, so no {{ }} either
const BADTEXT = /[<>{}]/;

// Only session member reads, literals, and for if tests ! && || ??
function safeNode (n, logic) {
  switch (n?.type) {
  case 'Identifier':
    return n.name === 'session';
  case 'MemberExpression':
    return !n.computed && !n.optional && !BADPROPS.has(n.property.name) && safeNode(n.object, false);
  case 'Literal':
    if (typeof n.value === 'string') {
      return !BADTEXT.test(n.value) && !/["'`\\]/.test(n.raw.slice(1, -1)) && !BADPROPS.has(n.value);
    }
    return typeof n.value === 'number' || typeof n.value === 'boolean';
  case 'LogicalExpression':
    return logic && safeNode(n.left, logic) && safeNode(n.right, logic);
  case 'UnaryExpression':
    return logic && n.operator === '!' && safeNode(n.argument, logic);
  default:
    return false;
  }
}

function parseExpr (src) {
  try {
    let comment = false;
    const prog = acorn.parse('(' + src + ')', { ecmaVersion: 2022, onComment: () => { comment = true; } });
    if (comment || prog.body.length !== 1 || prog.body[0].type !== 'ExpressionStatement') {
      return undefined;
    }
    return prog.body[0].expression;
  } catch {
    return undefined;
  }
}

function checkNode (n) {
  switch (n.type) {
  case 'Block':
    for (const c of n.nodes) {
      const err = checkNode(c);
      if (err) { return err; }
    }
    return undefined;
  case 'Text':
    return BADTEXT.test(n.val) ? `text ${n.val}` : undefined;
  case 'Comment':
    return BADTEXT.test(n.val) ? `comment ${n.val}` : undefined;
  case 'BlockComment':
    return BADTEXT.test(n.val) ? `comment ${n.val}` : checkNode(n.block);
  case 'Tag':
    if (!TAGS.has(n.name)) { return `tag ${n.name}`; }
    if (n.attributeBlocks.length) { return '&attributes'; }
    for (const a of n.attrs) {
      if (!ATTRS.has(a.name)) { return `attribute ${a.name}`; }
      if (a.val === true) { continue; }
      const expr = parseExpr(a.val);
      // .class shorthand isn't escaped, it can't have anything to escape
      if ((!a.mustEscape && a.name !== 'class') || expr?.type !== 'Literal' || typeof expr.value !== 'string' || !safeNode(expr, false)) {
        return `attribute ${a.name} value ${a.val}`;
      }
    }
    return checkNode(n.block);
  case 'Conditional':
    if (!safeNode(parseExpr(n.test), true)) { return `if ${n.test}`; }
    return checkNode(n.consequent) || (n.alternate ? checkNode(n.alternate) : undefined);
  case 'Mixin': {
    if (!n.call || !Object.hasOwn(MIXINS, n.name) || n.block || n.attrs.length || n.attributeBlocks.length) {
      return `mixin ${n.name}`;
    }
    const args = parseExpr('[' + (n.args ?? '') + ']');
    if (args?.type !== 'ArrayExpression' || args.elements.length > MIXINS[n.name] || !args.elements.every(e => safeNode(e, false))) {
      return `mixin ${n.name} args ${n.args}`;
    }
    return undefined;
  }
  default:
    return `${n.type}`;
  }
}

/**
 * Check that a view is safe pug: allowed tags and attributes with literal values, text without
 * html or Vue interpolation, if tests on session fields, and +arrayList calls. No code, interpolation,
 * includes, filters, each or other mixins.
 *
 * @param {string} view - The pug, indented as it will be in the session detail template
 * @returns {string|undefined} Why the view isn't safe, or undefined if it is
 */
function checkView (view) {
  let err;
  try {
    // Parse it nested the same way it will be, the plugin runs before includes are loaded
    pug.compile('div\n' + view, {
      plugins: [{
        postParse (ast) {
          const root = ast.nodes[0];
          if (ast.nodes.length !== 1 || root.type !== 'Tag' || root.name !== 'div' || root.attrs.length) {
            err = 'bad indentation';
          } else {
            err = checkNode(root.block);
          }
          if (err) { throw new Error(err); }
          return ast;
        }
      }]
    });
  } catch (e) {
    return err ?? e.message.split('\n').pop();
  }
  return undefined;
}

module.exports = { checkView };
