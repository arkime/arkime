/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Connection tree: the src -> dst sample /api/connections returns, as a rooted
tree. The dashboard tree widget and the SPI Graph tree type both build theirs
here, so they fan out the same way.
*/

// a node fans out to at most this many branches, the heaviest first, and the
// whole tree stops growing past MAX_TREE_NODES so a dense sample stays readable
export const MAX_TREE_CHILDREN = 25;
export const MAX_TREE_NODES = 400;

/**
 * The tree's root: the asked for value when it is in the sample (null when it
 * isn't), else the source with the most sessions (type 1 = source only, 3 =
 * seen at both ends), null when the sample has no source.
 * @param {object[]} nodes - the sample's nodes ({ id, type, sessions })
 * @param {string} [rootId] - the asked for root value
 * @returns {object|null} the root node
 */
export function findTreeRoot (nodes, rootId) {
  const list = nodes || [];
  if (rootId) { return list.find(n => n.id === rootId) || null; }
  let best = null;
  for (const n of list) {
    if ((n.type === 1 || n.type === 3) && (n.sessions || 0) > (best?.sessions || 0)) { best = n; }
  }
  return best;
}

/**
 * The sample as a tree from rootNode: { id, node, link, children }, node the
 * sample's node, link the sample's link into it (undefined for the root) with
 * source/target resolved to nodes. Breadth first, each host once at the first
 * hop it is reached on (so a cycle never loops), the heaviest branches first.
 * @param {object} data - { nodes, links }, links referencing node positions
 * @param {object} rootNode - from findTreeRoot
 * @param {number} depth - hops to draw
 * @returns {object} the root of the tree
 */
export function buildConnectionTree (data, rootNode, depth) {
  const nodes = data?.nodes || [];
  // outgoing branches per node id
  const out = new Map();
  for (const l of data?.links || []) {
    const s = nodes[l.source];
    const t = nodes[l.target];
    if (!s || !t || s === t) { continue; }
    if (!out.has(s.id)) { out.set(s.id, []); }
    out.get(s.id).push({ node: t, link: { ...l, source: s, target: t } });
  }
  const seen = new Set([rootNode.id]);
  const root = { id: rootNode.id, node: rootNode, link: undefined, children: [] };
  let count = 1;
  let level = [root];
  for (let hop = 0; hop < depth && level.length && count < MAX_TREE_NODES; hop++) {
    const next = [];
    for (const from of level) {
      const branches = (out.get(from.id) || [])
        .filter(b => !seen.has(b.node.id))
        .sort((a, b) => (b.link.value || 0) - (a.link.value || 0))
        .slice(0, MAX_TREE_CHILDREN);
      for (const b of branches) {
        if (count >= MAX_TREE_NODES) { break; }
        seen.add(b.node.id);
        const child = { id: b.node.id, node: b.node, link: b.link, children: [] };
        from.children.push(child);
        next.push(child);
        count++;
      }
    }
    level = next;
  }
  return root;
}
