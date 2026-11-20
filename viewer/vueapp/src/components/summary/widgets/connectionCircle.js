/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Connection circle: the src -> dst sample /api/connections returns, with the
hosts on a ring and each pair an arc across it. The dashboard circle widget and
the SPI Graph circle type both lay theirs out here.
*/
import { OTHER_COLOR } from './chartColors.js';

// how many hosts the ring can be set to show, the heaviest are kept
export const CIRCLE_MAX_NODES = [10, 20, 50, 100, 200];
export const DEFAULT_CIRCLE_MAX_NODES = 100;

const TYPE_ORDER = { 1: 0, 3: 1, 2: 2 }; // sources, both, destinations

// the database field the arcs are colored by unless another is picked (protocols)
export const DEFAULT_CIRCLE_COLOR_FIELD = 'protocol';
// the values that get a color of their own, the rest are "other"
export const MAX_CIRCLE_COLORS = 9;
// a pair is named by what it carried, not by its transport, when it has both
const TRANSPORTS = new Set(['tcp', 'udp', 'sctp']);

/**
 * Gives each link the value it is colored by (link.colorValue, '' = other): of
 * the field's values over the pair's sessions, the one most links share, so
 * the colors stay few.
 * @param {object[]} links - links carrying the field's value or values
 * @param {string} [field] - the database field
 * @returns {object} { colorValues, other } the colored values, most links
 *   first, and whether any link fell outside them
 */
export function colorCircleLinks (links, field = DEFAULT_CIRCLE_COLOR_FIELD) {
  const lists = links.map((l) => {
    const all = [].concat(l[field] ?? []).map(String);
    if (field !== DEFAULT_CIRCLE_COLOR_FIELD) { return all; }
    const carried = all.filter(p => !TRANSPORTS.has(p));
    return carried.length ? carried : all;
  });
  const seen = new Map();
  for (const list of lists) {
    for (const v of list) { seen.set(v, (seen.get(v) || 0) + 1); }
  }
  const counts = new Map();
  links.forEach((l, i) => {
    let best = '';
    for (const v of lists[i]) {
      if (!best || seen.get(v) > seen.get(best) || (seen.get(v) === seen.get(best) && v < best)) { best = v; }
    }
    l.colorValue = best;
    if (best) { counts.set(best, (counts.get(best) || 0) + 1); }
  });
  const colorValues = [...counts.entries()]
    .sort((x, y) => y[1] - x[1] || (x[0] < y[0] ? -1 : 1))
    .slice(0, MAX_CIRCLE_COLORS)
    .map(([v]) => v);
  const colored = new Set(colorValues);
  let other = false;
  for (const l of links) {
    if (!colored.has(l.colorValue)) { l.colorValue = ''; other = true; }
  }
  return { colorValues, other };
}

/**
 * The sample on a ring: nodes { data: { id, node }, index, angle } and links
 * (the sample's link with source/target resolved to those nodes). Sources come
 * first, then hosts seen at both ends, then destinations, the heaviest first.
 * @param {object} data - { nodes, links }, links referencing node positions
 * @param {string} [weight] - the node property the ring is ordered and cut by
 * @param {string} [colorField] - the database field the arcs are colored by
 * @param {number} [maxNodes] - how many hosts the ring shows
 * @returns {object} { nodes, links, dropped, colorValues, other } dropped the
 *   hosts past the cap, colorValues/other from colorCircleLinks
 */
export function buildConnectionCircle (data, weight = 'sessions', colorField = DEFAULT_CIRCLE_COLOR_FIELD, maxNodes = DEFAULT_CIRCLE_MAX_NODES) {
  const all = data?.nodes || [];
  const w = n => n[weight] ?? n.sessions ?? 0;
  const kept = all.map((node, pos) => ({ node, pos }))
    .sort((a, b) => w(b.node) - w(a.node))
    .slice(0, CIRCLE_MAX_NODES.includes(maxNodes) ? maxNodes : DEFAULT_CIRCLE_MAX_NODES)
    .sort((a, b) => (TYPE_ORDER[a.node.type] ?? 1) - (TYPE_ORDER[b.node.type] ?? 1) || w(b.node) - w(a.node));

  const step = (2 * Math.PI) / Math.max(1, kept.length);
  const byPos = new Map();
  const nodes = kept.map((k, index) => {
    const n = { data: { id: k.node.id, node: k.node }, index, angle: index * step };
    byPos.set(k.pos, n);
    return n;
  });

  const links = [];
  for (const l of data?.links || []) {
    const source = byPos.get(l.source);
    const target = byPos.get(l.target);
    if (!source || !target || source === target) { continue; }
    links.push({ ...l, source, target });
  }
  // a pair seen both ways bends its two arcs apart
  const pairs = new Set(links.map(l => `${l.source.index},${l.target.index}`));
  for (const l of links) { l.twoWay = pairs.has(`${l.target.index},${l.source.index}`); }
  return { nodes, links, dropped: all.length - kept.length, ...colorCircleLinks(links, colorField) };
}

/** [x, y] at an angle (0 = top, clockwise) on a ring of radius r. */
export function circlePoint (angle, r) {
  return [r * Math.sin(angle), -r * Math.cos(angle)];
}

/** The arc of a link: a curve between its ends pulled toward the center. */
export function circleLinkPath (l, r) {
  const [sx, sy] = circlePoint(l.source.angle, r);
  const [tx, ty] = circlePoint(l.target.angle, r);
  let cx = 0;
  let cy = 0;
  if (l.twoWay) {
    // off to the arc's own side, which flips with its direction
    const len = Math.hypot(tx - sx, ty - sy) || 1;
    cx = ((sy - ty) / len) * r * 0.15;
    cy = ((tx - sx) / len) * r * 0.15;
  }
  return `M${sx},${sy}Q${cx},${cy} ${tx},${ty}`;
}

/** A label outside the ring reading outward, flipped on the left half. */
export function circleLabel (angle, r) {
  const deg = (angle * 180) / Math.PI;
  const flip = deg > 180;
  return {
    transform: `rotate(${deg - 90}) translate(${r},0)${flip ? ' rotate(180)' : ''}`,
    anchor: flip ? 'end' : 'start'
  };
}

/** True when a link touches the ring node. */
export function circleTouches (l, n) {
  return l.source === n || l.target === n;
}

/** The ring nodes a node shares a link with, itself included. */
export function circlePeers (links, n) {
  const peers = new Set([n]);
  for (const l of links) {
    if (circleTouches(l, n)) { peers.add(l.source); peers.add(l.target); }
  }
  return peers;
}

/**
 * The legend: { name, color } per colored value, then other (name '') when an
 * arc fell outside them.
 * @param {object} circle - buildConnectionCircle's result
 * @param {function} color - a value's color
 */
export function circleLegend (circle, color) {
  const legend = circle.colorValues.map(v => ({ name: v, color: color(v) }));
  if (circle.other && circle.colorValues.length) { legend.push({ name: '', color: OTHER_COLOR }); }
  return legend;
}
