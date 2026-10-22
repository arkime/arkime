/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Unit tests for the dashboard and saved graph (spigraphConfig) helpers in the
viewer's vueapp. Run with `npm run test:unit`.

The vueapp modules are written for vite: extensionless relative imports and
the @common alias. A resolve hook adds the extension and swaps in stubs for
the modules that need the browser app (FieldService, utils, fetchWrapper).
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { register } from 'node:module';

const stub = (src) => `data:text/javascript,${encodeURIComponent(src)}`;
const STUBS = {
  '/search/FieldService': stub(`
    const find = (s) => (globalThis.testFields || []).find(f => f.exp === s || f.dbField === s);
    export default { getField: (s) => find(s) };`),
  '/utils/utils': stub(`
    let n = 0;
    export default { createRandomString: () => 'rand' + (++n) };`),
  '@common/fetchWrapper.js': stub('export const fetchWrapper = async () => ({});'),
  '@common/vueFilters.js': stub('export const commaString = String; export const humanReadableBytes = String;')
};

register(stub(`
const STUBS = ${JSON.stringify(STUBS)};
export async function resolve (specifier, context, next) {
  for (const [suffix, url] of Object.entries(STUBS)) {
    if (specifier.endsWith(suffix)) { return { url, shortCircuit: true }; }
  }
  if (specifier.startsWith('.') && !/\\.[a-z]+$/.test(specifier)) {
    return next(specifier + '.js', context);
  }
  return next(specifier, context);
}`));

globalThis.testFields = [
  { exp: 'ip.src', dbField: 'source.ip' },
  { exp: 'ip.dst', dbField: 'destination.ip' },
  { exp: 'port.dst', dbField: 'destination.port' },
  { exp: 'protocols', dbField: 'protocol' },
  { exp: 'bytes', dbField: 'totBytes' },
  { exp: 'packets', dbField: 'totPackets' }
];
const fieldsArr = globalThis.testFields;

const {
  GRAPH_KEYS, widgetsFromConfig, defaultWidgets, graphFromWidget, widgetFromGraph,
  graphFromSpigraphQuery, spigraphQueryFromGraph, graphOnSpigraph
} = await import('../viewer/vueapp/src/components/summary/dashboardConfig.js');
const { viewModeLabelKey } = await import('../viewer/vueapp/src/components/summary/widgets/viewModes.js');
const { widgetFullPageLink } = await import('../viewer/vueapp/src/components/summary/widgets/widgetData.js');

const graph = (overrides) => graphFromWidget(overrides);

describe('widgetsFromConfig', () => {
  test('keeps and normalizes v7 widgets', () => {
    const widgets = widgetsFromConfig({ widgets: [{ id: 'a', viewMode: 'bar', field: 'ip.src', length: '15', expression: { bad: 1 } }] });
    assert.equal(widgets.length, 1);
    assert.equal(widgets[0].id, 'a');
    assert.deepEqual(widgets[0].fields, ['ip.src']);
    assert.equal(widgets[0].length, 15);
    assert.equal(widgets[0].expression, '');
  });

  test('migrates a v6 dashboard (fields[] only)', () => {
    const widgets = widgetsFromConfig({
      fields: [{ field: 'ip.src', viewMode: 'pie', metricType: 'bytes' }, { field: 'protocols', viewMode: 'table' }],
      resultsLimit: 50,
      order: 'asc'
    });
    assert.deepEqual(widgets.map(w => [w.field, w.viewMode, w.metricType, w.length, w.order]), [
      ['ip.src', 'pie', 'bytes', 50, 'asc'],
      ['protocols', 'table', 'sessions', 50, 'asc']
    ]);
    assert.equal(new Set(widgets.map(w => w.id)).size, 2);
  });

  test('falls back to the starter dashboard', () => {
    const starter = defaultWidgets().map(w => w.viewMode);
    for (const cfg of [undefined, null, {}, { widgets: [] }, { widgets: [], fields: [] }, { widgets: 'x' }]) {
      assert.deepEqual(widgetsFromConfig(cfg).map(w => w.viewMode), starter);
    }
  });

  test('survives null entries and makes ids unique', () => {
    const widgets = widgetsFromConfig({ widgets: [null, { id: 'dup', viewMode: 'text' }, { id: 'dup', viewMode: 'stats' }] });
    assert.equal(widgets.length, 3);
    assert.equal(new Set(widgets.map(w => w.id)).size, 3);
    assert.equal(widgets[1].id, 'dup');
  });
});

describe('graphFromWidget', () => {
  test('keeps only the graph keys', () => {
    const g = graph({ id: 'w1', title: 'T', width: 4, height: 2, text: 'x', viewMode: 'pie', field: 'ip.src' });
    assert.deepEqual(Object.keys(g), GRAPH_KEYS);
    assert.deepEqual(g.fields, ['ip.src']);
  });

  test('normalizes shared data of the wrong type', () => {
    const g = graph({ viewMode: 'bar', fields: ['ip.src', 7, null], expression: ['x'], view: {}, length: 'lots', metrics: [1, 'bytes'], order: 'sideways' });
    assert.deepEqual(g.fields, ['ip.src']);
    assert.equal(g.expression, '');
    assert.equal(g.view, '');
    assert.equal(g.length, 20);
    assert.deepEqual(g.metrics, ['bytes']);
    assert.equal(g.order, 'desc');
  });

  test('widgetFromGraph gets a new id and the given layout', () => {
    const w = widgetFromGraph({ ...graph({ viewMode: 'pie', field: 'ip.src' }), id: 'stale' }, { width: 4, title: 'Mine' });
    assert.notEqual(w.id, 'stale');
    assert.equal(w.width, 4);
    assert.equal(w.title, 'Mine');
    assert.equal(w.viewMode, 'pie');
  });
});

describe('graphFromSpigraphQuery', () => {
  test('bar keeps one field plus its split', () => {
    const g = graphFromSpigraphQuery({ spiGraphType: 'bar', exp: 'ip.src', subFields: 'protocols', barMode: 'grouped', size: '15' }, { expression: 'port.dst == 53' });
    assert.deepEqual(g.fields, ['ip.src']);
    assert.equal(g.splitField, 'protocols');
    assert.equal(g.barMode, 'grouped');
    assert.equal(g.length, 15);
    assert.equal(g.expression, 'port.dst == 53');
  });

  test('heatmap intensity becomes the metric', () => {
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'heatmap', exp: 'ip.src', graphType: 'totBytesHisto' }, { fieldsArr }).metricType, 'bytes');
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'heatmap', exp: 'ip.src', graphType: 'sessionsHisto' }, { fieldsArr }).metricType, 'sessions');
  });

  test('connections map db fields back and keep their sample size', () => {
    const g = graphFromSpigraphQuery({ spiGraphType: 'connections', srcField: 'source.ip', dstField: 'ip.dst:port', connectionsLength: '250', size: '10', view: 'v1' }, { fieldsArr });
    assert.deepEqual(g.fields, ['ip.src', 'ip.dst:port']);
    assert.equal(g.length, 250);
    assert.equal(g.view, 'v1');
  });

  test('a nested table is an intersection', () => {
    const g = graphFromSpigraphQuery({ spiGraphType: 'table', exp: 'ip.src', subFields: 'ip.dst,port.dst' });
    assert.equal(g.viewMode, 'intersection');
    assert.deepEqual(g.fields, ['ip.src', 'ip.dst', 'port.dst']);
  });

  test('types with no graph give null', () => {
    assert.equal(graphFromSpigraphQuery({ exp: 'ip.src' }), null);
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'default', exp: 'ip.src' }), null);
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'pie' }), null);
  });
});

describe('spigraphQueryFromGraph', () => {
  test('connections carry their sample size and view', () => {
    assert.deepEqual(spigraphQueryFromGraph(graph({ viewMode: 'connections', fields: ['ip.src', 'ip.dst:port'], length: 250, view: 'v1' })), {
      spiGraphType: 'connections', srcField: 'source.ip', dstField: 'ip.dst:port', connectionsLength: 250, view: 'v1'
    });
  });

  test('a graph SPI Graph shows comes back the same', () => {
    const graphs = [
      graph({ viewMode: 'bar', field: 'ip.src', splitField: 'protocols', barMode: 'grouped', length: 15, view: 'v1', expression: 'port.dst == 53' }),
      graph({ viewMode: 'pie', fields: ['ip.src', 'port.dst'], length: 10 }),
      graph({ viewMode: 'treemap', fields: ['ip.src', 'ip.dst'] }),
      graph({ viewMode: 'sankey', fields: ['ip.src', 'ip.dst', 'port.dst'] }),
      graph({ viewMode: 'table', field: 'protocols', length: 50 }),
      graph({ viewMode: 'intersection', fields: ['ip.src', 'ip.dst'] }),
      graph({ viewMode: 'heatmap', field: 'ip.dst', metricType: 'packets', length: 30 }),
      graph({ viewMode: 'connections', fields: ['ip.src', 'ip.dst:port'], length: 500 })
    ];
    for (const g of graphs) {
      const query = spigraphQueryFromGraph(g);
      assert.deepEqual(graphFromSpigraphQuery(query, { expression: g.expression, fieldsArr }), g, g.viewMode);
    }
  });

  test('graphs SPI Graph has no type for give null', () => {
    assert.equal(spigraphQueryFromGraph({ viewMode: 'stats' }), null);
    assert.equal(spigraphQueryFromGraph({ viewMode: 'bar' }), null);
  });
});

describe('graphOnSpigraph', () => {
  const saved = graph({ viewMode: 'table', field: 'ip.src', metrics: ['sessions', 'bytes'], metricAgg: 'avg', sortMetric: 'bytes', order: 'asc', splitLength: 10, length: 20 });

  test('no page graph is no graph; no saved graph is the page', () => {
    assert.equal(graphOnSpigraph(saved, null), null);
    const page = graph({ viewMode: 'pie', field: 'ip.src' });
    assert.equal(graphOnSpigraph(null, page), page);
  });

  test('keeps the saved settings SPI Graph cannot show', () => {
    const page = graphFromSpigraphQuery({ spiGraphType: 'table', exp: 'ip.dst', size: '50' });
    const merged = graphOnSpigraph(saved, page);
    assert.equal(merged.field, 'ip.dst');
    assert.equal(merged.length, 50);
    assert.deepEqual(merged.metrics, ['sessions', 'bytes']);
    assert.equal(merged.metricAgg, 'avg');
    assert.equal(merged.sortMetric, 'bytes');
    assert.equal(merged.order, 'asc');
    assert.equal(merged.splitLength, 10);
  });

  test('an unchanged page is the saved graph', () => {
    const page = graphFromSpigraphQuery(spigraphQueryFromGraph(saved), { fieldsArr });
    assert.deepEqual(graphOnSpigraph(saved, page), saved);
  });

  test('a heatmap takes the page intensity, other types keep the saved metric', () => {
    const heat = graph({ viewMode: 'heatmap', field: 'ip.src', metricType: 'bytes' });
    const heatPage = graphFromSpigraphQuery({ spiGraphType: 'heatmap', exp: 'ip.src', graphType: 'totPacketsHisto' }, { fieldsArr });
    assert.equal(graphOnSpigraph(heat, heatPage).metricType, 'packets');

    const bar = graph({ viewMode: 'bar', field: 'ip.src', metricType: 'bytes' });
    const barPage = graphFromSpigraphQuery({ spiGraphType: 'bar', exp: 'ip.src' });
    assert.equal(graphOnSpigraph(bar, barPage).metricType, 'bytes');
  });
});

describe('widgetFullPageLink', () => {
  const route = { query: { expression: 'ip == 10.0.0.1', view: 'global' } };
  const store = { state: { timeRange: '24', views: [{ id: 'v1', name: 'web', expression: 'port.dst == 80' }] } };

  test('opens the saved graph SPI Graph url, keeping the global view', () => {
    const w = { ...graph({ viewMode: 'connections', fields: ['ip.src', 'ip.dst:port'], length: 250, view: 'v1' }), id: 'w1' };
    const link = widgetFullPageLink(route, store, w);
    assert.equal(link.path, '/spigraph');
    assert.deepEqual(link.query, {
      expression: '(ip == 10.0.0.1) && (port.dst == 80)',
      view: 'global',
      date: '24',
      spiGraphType: 'connections',
      srcField: 'source.ip',
      dstField: 'ip.dst:port',
      connectionsLength: 250
    });
  });

  test('a summed bar metric ranks SPI Graph', () => {
    const link = widgetFullPageLink({ query: {} }, store, { ...graph({ viewMode: 'bar', field: 'ip.src', metricType: 'bytes', splitField: 'protocols' }), id: 'w2' });
    assert.equal(link.query.graphType, 'totBytesHisto');
    assert.equal(link.query.subFields, 'protocols');
    assert.equal(link.query.view, undefined);
  });

  test('session-wide widgets open Sessions, the rest have no page', () => {
    assert.equal(widgetFullPageLink(route, store, { viewMode: 'timeline' }).path, '/sessions');
    assert.equal(widgetFullPageLink(route, store, { viewMode: 'stats' }), null);
    assert.equal(widgetFullPageLink(route, store, { viewMode: 'bar', field: 'nope' }), null);
  });
});

describe('viewModeLabelKey', () => {
  test('only own view modes have labels', () => {
    assert.equal(viewModeLabelKey('bar'), 'sessions.summary.barChart');
    assert.equal(viewModeLabelKey('constructor'), undefined);
    assert.equal(viewModeLabelKey('__proto__'), undefined);
  });
});
