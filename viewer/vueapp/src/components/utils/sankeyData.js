/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Shared transform from /api/spigraphhierarchy's nested `hierarchicalResults` into
the { nodes, links } shape d3-sankey wants. Used by both sankey renderers — the
SPI Graph page's Hierarchy.vue and the dashboard's HierarchyWidget.vue — so the
two always describe the same data the same way.
*/

/**
 * Flatten nested hierarchy results into d3-sankey nodes and links.
 *
 * Node values are cumulative: a parent counts at least the sum of its children,
 * but keeps its own count when that is larger (the endpoint returns only the
 * top N children per level, so a parent's own total can exceed their sum — it
 * reports that total as `sizeValue` and leaves `size` unset). Node ids are
 * name + depth, so the same value appearing under two parents becomes one node
 * with two inbound links.
 *
 * With a single level there are no flows between fields, so the root is kept as
 * the source column (root → each value); with more, the root is dropped and the
 * first field becomes the leftmost column.
 *
 * @param {object} root - hierarchicalResults ({ name, children: [...] })
 * @returns {{nodes: object[], links: object[], rootKept: boolean}} nodes carry
 *          { id, name, value, depth }; links carry { source, target, value }
 *          as node ids; rootKept says whether depth 0 is the synthetic root
 *          rather than the first field's values.
 */
export function hierarchyToSankey (root) {
  const nodes = [];
  const links = [];
  const nodeMap = new Map();

  if (!root || !root.children) { return { nodes, links, rootKept: false }; }

  // memoized: cumulative() is asked for every node twice (once for the node,
  // once for its inbound link) and a 3 field sankey can carry thousands of them
  const totals = new Map();
  const cumulative = (n) => {
    if (totals.has(n)) { return totals.get(n); }
    let total;
    if (!n.children?.length) {
      total = n.size || 0;
    } else {
      const sum = n.children.reduce((s, c) => s + cumulative(c), 0);
      total = Math.max(n.sizeValue || n.size || 0, sum);
    }
    totals.set(n, total);
    return total;
  };

  const multiLevel = root.children.some(c => c.children?.length);

  const traverse = (n, depth, parentId) => {
    const id = `${n.name}_${depth}`;
    const seen = nodeMap.has(id);
    if (!seen) {
      nodeMap.set(id, { id, name: n.name, value: cumulative(n), depth });
      nodes.push(nodeMap.get(id));
    }
    if (parentId && parentId !== id) {
      links.push({ source: parentId, target: id, value: cumulative(n) });
    }
    // a node reached again through another parent already contributed its
    // subtree, so don't walk it a second time
    if (seen) { return; }
    for (const c of (n.children || [])) { traverse(c, depth + 1, id); }
  };

  if (multiLevel) {
    for (const c of root.children) { traverse(c, 0, null); }
  } else {
    traverse(root, 0, null);
  }

  return { nodes, links, rootKept: !multiLevel };
}
