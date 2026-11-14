// Copyright Andy Wick
// SPDX-License-Identifier: Apache-2.0
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { buildConnectionCircle, colorCircleLinks, circleLinkPath, circlePeers, circleLegend, DEFAULT_CIRCLE_MAX_NODES, MAX_CIRCLE_COLORS } from '../viewer/vueapp/src/components/summary/widgets/connectionCircle.js';
import { OTHER_COLOR } from '../viewer/vueapp/src/components/summary/widgets/chartColors.js';

const sample = () => ({
  nodes: [
    { id: 'a', type: 1, sessions: 5 },
    { id: 'b', type: 2, sessions: 9 },
    { id: 'c', type: 3, sessions: 2 },
    { id: 'd', type: 1, sessions: 7 }
  ],
  links: [
    { source: 0, target: 1, value: 3, protocol: ['tcp', 'tls'] },
    { source: 3, target: 1, value: 4, protocol: ['tcp', 'tls', 'http'] },
    { source: 2, target: 1, value: 1, protocol: ['udp'] },
    { source: 2, target: 2, value: 1, protocol: ['dns'] }
  ]
});

describe('connection circle', () => {
  test('sources, then both, then destinations, the heaviest first', () => {
    const c = buildConnectionCircle(sample());
    assert.deepEqual(c.nodes.map(n => n.data.id), ['d', 'a', 'c', 'b']);
    assert.equal(c.dropped, 0);
    assert.equal(c.links.length, 3); // the self link is left out
    assert.equal(c.links[0].source.data.id, 'a');
  });
  test('only the heaviest fit on the ring', () => {
    const nodes = Array.from({ length: DEFAULT_CIRCLE_MAX_NODES + 5 }, (_, i) => ({ id: `n${i}`, type: 1, sessions: i }));
    const data = { nodes, links: [{ source: 0, target: 50, value: 1 }, { source: 104, target: 103, value: 1 }] };
    const c = buildConnectionCircle(data);
    assert.equal(c.nodes.length, DEFAULT_CIRCLE_MAX_NODES);
    assert.equal(c.dropped, 5);
    assert.equal(c.links.length, 1); // an end of the other fell off the ring
    const ten = buildConnectionCircle(data, 'sessions', undefined, 10);
    assert.deepEqual([ten.nodes.length, ten.dropped, ten.nodes[0].data.id], [10, 95, 'n104']);
    // a size that isn't offered is the default
    assert.equal(buildConnectionCircle(data, 'sessions', undefined, 7).nodes.length, DEFAULT_CIRCLE_MAX_NODES);
  });
  test('a weight of 0 is a weight, not a missing one', () => {
    const data = {
      nodes: [
        { id: 'scanner', type: 1, sessions: 500, totDataBytes: 0 },
        { id: 'talker', type: 1, sessions: 2, totDataBytes: 300 },
        { id: 'unknown', type: 1, sessions: 7 }
      ],
      links: []
    };
    const c = buildConnectionCircle(data, 'totDataBytes');
    assert.deepEqual(c.nodes.map(n => n.data.id), ['talker', 'unknown', 'scanner']);
  });
  test('a pair seen both ways gets two arcs bent apart', () => {
    const data = {
      nodes: [{ id: 'a', type: 3, sessions: 2 }, { id: 'b', type: 3, sessions: 2 }, { id: 'c', type: 2, sessions: 1 }],
      links: [{ source: 0, target: 1, value: 1 }, { source: 1, target: 0, value: 1 }, { source: 0, target: 2, value: 1 }]
    };
    const c = buildConnectionCircle(data);
    assert.deepEqual(c.links.map(l => l.twoWay), [true, true, false]);
    const [ab, ba, ac] = c.links.map(l => circleLinkPath(l, 100));
    const control = p => p.match(/Q([^ ]+) /)[1];
    assert.notEqual(control(ab), control(ba));
    assert.equal(control(ac), '0,0');
  });
  test('a host lights itself and its peers, the legend ends with other', () => {
    const c = buildConnectionCircle(sample());
    const b = c.nodes.find(n => n.data.id === 'b');
    assert.deepEqual([...circlePeers(c.links, b)].map(n => n.data.id).sort(), ['a', 'b', 'c', 'd']);
    const a = c.nodes.find(n => n.data.id === 'a');
    assert.deepEqual([...circlePeers(c.links, a)].map(n => n.data.id).sort(), ['a', 'b']);
    assert.deepEqual(circleLegend(c, v => `#${v}`), [{ name: 'tls', color: '#tls' }, { name: 'udp', color: '#udp' }]);
    assert.deepEqual(circleLegend({ colorValues: ['tls'], other: true }, () => '#1').at(-1), { name: '', color: OTHER_COLOR });
    assert.deepEqual(circleLegend({ colorValues: [], other: true }, () => '#1'), []);
  });
  test('protocols name what a pair carried, a transport only when alone', () => {
    const c = buildConnectionCircle(sample());
    assert.deepEqual(c.links.map(l => l.colorValue), ['tls', 'tls', 'udp']);
    assert.deepEqual(c.colorValues, ['tls', 'udp']);
    assert.equal(c.other, false);
  });
  test('any field can color, values past the cap are other', () => {
    const links = Array.from({ length: MAX_CIRCLE_COLORS + 2 }, (_, i) => ({ port: i < 2 ? 443 : i }));
    links.push({});
    const { colorValues, other } = colorCircleLinks(links, 'port');
    assert.equal(colorValues.length, MAX_CIRCLE_COLORS);
    assert.equal(colorValues[0], '443');
    assert.equal(other, true);
    assert.equal(links.at(-1).colorValue, '');
  });
});
