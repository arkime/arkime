/******************************************************************************/
/* tsharkPdml.js -- Build the per-packet proto/field tree from tshark's PDML
 *                  SAX events, for the session tshark api. Kept free of
 *                  config and server state so it can be unit tested.
 *
 * Copyright Andy Wick
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

// Deepest a field may sit in the tree, protos are level 1. A crafted packet
// can make tshark nest hundreds of levels, which overflows the browser's
// stack when the recursive tree view renders it. Fields that would go deeper
// are added to their level MAX_TREE_DEPTH - 1 ancestor instead, so nothing
// is dropped and byte highlighting still works.
const MAX_TREE_DEPTH = 32;

/**
 * Returns sax opentag/closetag handlers that build one packet at a time as a
 * stack of proto/field nodes and call onPacket({ layers }) on </packet>.
 * Only one packet is ever buffered.
 * @param {boolean} includeHidden - keep fields tshark marks hide="yes"
 * @param {function} onPacket - called with each finished packet
 */
function pdmlTreeBuilder (includeHidden, onPacket) {
  let packet = null; // { layers: [...] } currently being built
  // { container, level } for each proto or field we're inside, top is current.
  // level is where container sits in the tree, not its stack position: a
  // proto nested in a field is still added to layers, so it's level 1
  let stack = null;

  function opentag (node) {
    if (!packet) {
      if (node.name === 'packet') { packet = { layers: [] }; stack = []; }
      return;
    }
    const a = node.attributes;
    if (node.name === 'proto') {
      const proto = { name: a.name || '', label: a.showname || a.name || '', fields: [] };
      if (a.pos !== undefined) { proto.pos = parseInt(a.pos); }
      if (a.size !== undefined) { proto.size = parseInt(a.size); }
      packet.layers.push(proto);
      stack.push({ container: proto, level: 1 });
    } else if (node.name === 'field') {
      const enclosing = stack[stack.length - 1];
      const level = (enclosing?.level ?? 0) + 1; // f's level
      if (!includeHidden && a.hide === 'yes') { stack.push({ container: null, level }); return; }
      const f = { name: a.name || '', label: a.showname || a.show || a.name || '' };
      if (a.show !== undefined && a.show !== '') { f.show = a.show; }
      if (a.value !== undefined && a.value !== '') { f.value = a.value; }
      if (a.pos !== undefined) { f.pos = parseInt(a.pos); }
      if (a.size !== undefined) { f.size = parseInt(a.size); }
      if (a.hide === 'yes') { f.hide = true; }
      const cur = enclosing?.container;
      if (cur) {
        if (!cur.fields) { cur.fields = []; }
        cur.fields.push(f);
      }
      // at the limit f's children go to cur instead
      stack.push(level >= MAX_TREE_DEPTH ? enclosing : { container: f, level });
    }
  }

  function closetag (tagName) {
    if (!packet) { return; }
    if (tagName === 'packet') {
      // Drop empty fields arrays for compactness.
      const stripEmpty = (n) => {
        if (n && n.fields) {
          if (n.fields.length === 0) { delete n.fields; } else { for (const c of n.fields) { stripEmpty(c); } }
        }
      };
      for (const l of packet.layers) { stripEmpty(l); }
      const done = packet;
      packet = null;
      stack = null;
      onPacket(done);
    } else if (tagName === 'proto' || tagName === 'field') {
      if (stack) { stack.pop(); }
    }
  }

  return { opentag, closetag };
}

module.exports = { pdmlTreeBuilder, MAX_TREE_DEPTH };
