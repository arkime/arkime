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
  GRAPH_KEYS, widgetsFromConfig, defaultWidgets, graphFromWidget, widgetFromGraph, normalizeWidget, widgetIsGraph,
  graphFromSpigraphQuery, spigraphQueryFromGraph, graphOnSpigraph, migrateDashboard, DASHBOARD_VERSION
} = await import('../viewer/vueapp/src/components/summary/dashboardConfig.js');
const { viewModeLabelKey, fieldCountLimits, MAX_FIELDS } = await import('../viewer/vueapp/src/components/summary/widgets/viewModes.js');
const { widgetFullPageLink, widgetFields, hierarchyFields, pageExpression, combinedExpression } = await import('../viewer/vueapp/src/components/summary/widgets/widgetData.js');
const { findTreeRoot, buildConnectionTree, MAX_TREE_CHILDREN, MAX_TREE_NODES } = await import('../viewer/vueapp/src/components/summary/widgets/connectionTree.js');

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

  test('keeps up to MAX_FIELDS fields and an Other flag that is really true', () => {
    const fields = ['f1', 'f2', 'f3', 'f4', 'f5', 'f6', 'f7', 'f8', 'f9'];
    const [w] = widgetsFromConfig({ widgets: [{ viewMode: 'intersection', fields, other: 'yes' }] });
    assert.deepEqual(w.fields, fields.slice(0, MAX_FIELDS));
    assert.equal(w.other, false);
    assert.equal(widgetsFromConfig({ widgets: [{ viewMode: 'pie', field: 'ip.src', other: true }] })[0].other, true);
  });

  test('a text widget saved before the markdown widget is a markdown widget', () => {
    const [w] = widgetsFromConfig({ widgets: [{ id: 't', viewMode: 'text', text: '# notes' }] });
    assert.equal(w.viewMode, 'markdown');
    assert.equal(w.text, '# notes');
  });

  test('survives null entries and makes ids unique', () => {
    const widgets = widgetsFromConfig({ widgets: [null, { id: 'dup', viewMode: 'markdown' }, { id: 'dup', viewMode: 'stats' }] });
    assert.equal(widgets.length, 3);
    assert.equal(new Set(widgets.map(w => w.id)).size, 3);
    assert.equal(widgets[1].id, 'dup');
  });
});

describe('normalizeWidget', () => {
  test('keeps bar orientation and timeline style, defaults the rest', () => {
    const bar = normalizeWidget({ id: 'b', viewMode: 'bar', field: 'ip.src', orientation: 'horizontal' });
    assert.equal(bar.orientation, 'horizontal');
    const line = normalizeWidget({ id: 't', viewMode: 'timeline', seriesType: 'lines', yScale: 'log' });
    assert.equal(line.seriesType, 'lines');
    assert.equal(line.yScale, 'log');
    const odd = normalizeWidget({ id: 'x', viewMode: 'timeline', orientation: 'sideways', seriesType: 'dots', yScale: 'sqrt' });
    assert.equal(odd.orientation, 'vertical');
    assert.equal(odd.seriesType, 'bars');
    assert.equal(odd.yScale, 'linear');
  });

  // the dashboard saves its widgets through normalizeWidget after a resize,
  // move or remove, so a render entry must come back with every setting
  test('a render entry round trips every saved setting', () => {
    const saved = {
      id: 'w', viewMode: 'bar', field: 'port.dst', fields: ['port.dst'], orderBy: 'key', bucketInterval: 10,
      orientation: 'horizontal', treeRoot: '10.0.0.1', treeDepth: 4, gaugeMax: 100, gaugeWarn: 50, gaugeCrit: 90,
      seriesType: 'lines', yScale: 'log', width: 3, height: 2, title: 'Ports'
    };
    const entry = { ...normalizeWidget(saved), data: [{ item: 80 }], loading: false, error: null, otherData: null };
    const again = normalizeWidget(entry);
    for (const [k, v] of Object.entries(saved)) { assert.deepEqual(again[k], v, k); }
    for (const k of ['data', 'loading', 'error', 'otherData']) { assert.equal(k in again, false, k); }
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

  test('connections map the ip:port destination either way it is spelled', () => {
    for (const dstField of ['ip.dst:port', 'destination.ip:port']) {
      assert.deepEqual(graphFromSpigraphQuery({ spiGraphType: 'connections', srcField: 'source.ip', dstField }, { fieldsArr }).fields, ['ip.src', 'ip.dst:port']);
    }
  });

  test('an intersection table is an intersection, a unique one a table', () => {
    const g = graphFromSpigraphQuery({ spiGraphType: 'table', exp: 'ip.src', subFields: 'ip.dst,port.dst' });
    assert.equal(g.viewMode, 'intersection');
    assert.deepEqual(g.fields, ['ip.src', 'ip.dst', 'port.dst']);
    const unique = graphFromSpigraphQuery({ spiGraphType: 'table', exp: 'ip.src', subFields: 'ip.dst,port.dst', tableMode: 'unique' });
    assert.equal(unique.viewMode, 'table');
    assert.deepEqual(unique.fields, ['ip.src', 'ip.dst', 'port.dst']);
    // one field is the same either way: a plain table
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'table', exp: 'ip.src', tableMode: 'unique' }).viewMode, 'table');
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'table', exp: 'ip.src' }).viewMode, 'table');
  });

  test('SPI Graph\'s Timeline is a timeline split by its field', () => {
    for (const q of [{ exp: 'ip.src' }, { spiGraphType: 'default', exp: 'ip.src' }]) {
      const g = graphFromSpigraphQuery(q);
      assert.equal(g.viewMode, 'timeline');
      assert.equal(g.splitField, 'ip.src');
    }
  });

  test('SPI Graph\'s percent layout is the timeline\'s percent y axis', () => {
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'default', exp: 'ip.src', timelineMode: 'percent' }).yScale, 'percent');
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'default', exp: 'ip.src', timelineMode: 'stacked' }).yScale, 'linear');
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'default', exp: 'ip.src' }).yScale, 'linear');
  });

  test('SPI Graph\'s Time of day is a time of day grid split by its field', () => {
    const g = graphFromSpigraphQuery({ spiGraphType: 'timeofday', exp: 'ip.src', size: '5', graphType: 'totBytesHisto' }, { fieldsArr });
    assert.equal(g.viewMode, 'timeofday');
    assert.equal(g.splitField, 'ip.src');
    assert.equal(g.splitLength, 5);
    assert.equal(g.metricType, 'bytes');
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'timeofday' }), null);
  });

  test('SPI Graph\'s matrix is the first field by the second', () => {
    const g = graphFromSpigraphQuery({ spiGraphType: 'matrix', exp: 'ip.src', subFields: 'port.dst,protocols', size: '10' });
    assert.equal(g.viewMode, 'matrix');
    assert.deepEqual(g.fields, ['ip.src', 'port.dst']);
    assert.equal(g.length, 10);
    assert.deepEqual(graphFromSpigraphQuery({ spiGraphType: 'matrix', exp: 'ip.src' }).fields, ['ip.src']);
  });

  test('no field gives no graph', () => {
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
      graph({ viewMode: 'bar', field: 'ip.src', orientation: 'horizontal', length: 20 }),
      graph({ viewMode: 'timeline', splitField: 'protocols', splitLength: 10 }),
      graph({ viewMode: 'timeline', splitField: 'ip.dst', splitLength: 5, metricType: 'bytes', view: 'v1' }),
      graph({ viewMode: 'timeline', splitField: 'protocols', splitLength: 5, yScale: 'percent' }),
      graph({ viewMode: 'timeofday', splitField: 'protocols', splitLength: 10 }),
      graph({ viewMode: 'timeofday', splitField: 'ip.dst', splitLength: 5, metricType: 'bytes', view: 'v1' }),
      graph({ viewMode: 'matrix', fields: ['ip.src', 'port.dst'], length: 10 }),
      graph({ viewMode: 'pie', fields: ['ip.src', 'port.dst'], length: 10 }),
      graph({ viewMode: 'pie', fields: ['protocols', 'port.dst'], splitField: 'ip.dst', length: 10 }),
      graph({ viewMode: 'pie', fields: ['protocols'], other: true }),
      graph({ viewMode: 'pie', fields: ['protocols'], splitField: 'ip.dst', other: true }),
      graph({ viewMode: 'pie', fields: ['protocols'], splitField: 'ip.dst', splitLength: 10 }),
      graph({ viewMode: 'intersection', fields: ['ip.src', 'ip.dst', 'port.dst', 'protocols', 'bytes', 'packets', 'ip.src'] }),
      graph({ viewMode: 'treemap', fields: ['ip.src', 'ip.dst'] }),
      graph({ viewMode: 'sankey', fields: ['ip.src', 'ip.dst', 'port.dst'] }),
      graph({ viewMode: 'table', field: 'protocols', length: 50 }),
      graph({ viewMode: 'table', fields: ['ip.src', 'ip.dst', 'port.dst', 'protocols', 'bytes', 'packets', 'ip.src'], length: 50 }),
      graph({ viewMode: 'intersection', fields: ['ip.src', 'ip.dst'] }),
      graph({ viewMode: 'heatmap', field: 'ip.dst', metricType: 'packets', length: 30 }),
      graph({ viewMode: 'connections', fields: ['ip.src', 'ip.dst:port'], length: 500 })
    ];
    for (const g of graphs) {
      const query = spigraphQueryFromGraph(g);
      assert.deepEqual(graphFromSpigraphQuery(query, { expression: g.expression, fieldsArr }), g, g.viewMode);
    }
  });

  test('a summed metric ranks the graph; a heatmap always names its intensity', () => {
    assert.equal(spigraphQueryFromGraph(graph({ viewMode: 'bar', field: 'ip.src', metricType: 'bytes' })).graphType, 'totBytesHisto');
    assert.equal(spigraphQueryFromGraph(graph({ viewMode: 'bar', field: 'ip.src', metricType: 'bytes', metricAgg: 'avg' })).graphType, undefined);
    assert.equal(spigraphQueryFromGraph(graph({ viewMode: 'pie', field: 'ip.src' })).graphType, undefined);
    assert.equal(spigraphQueryFromGraph(graph({ viewMode: 'heatmap', field: 'ip.src' })).graphType, 'sessionsHisto');
  });

  test('a split pie opens in SPI Graph\'s split pie mode', () => {
    const query = spigraphQueryFromGraph(graph({ viewMode: 'pie', fields: ['protocols', 'port.dst'], splitField: 'ip.dst', length: 10 }));
    assert.equal(query.spiGraphType, 'pie');
    assert.equal(query.pieMode, 'split');
    assert.equal(query.exp, 'ip.dst');
    assert.equal(query.subFields, 'protocols,port.dst');
    // its pie count is the widget's split length; a count off the scale is the default
    assert.equal(query.pieCount, 5);
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'pie', exp: 'ip.dst', subFields: 'protocols', pieMode: 'split', pieCount: '10' }).splitLength, 10);
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'pie', exp: 'ip.dst', subFields: 'protocols', pieMode: 'split', pieCount: '7' }).splitLength, 5);
    // a split mode with nothing to split is a plain pie
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'pie', exp: 'ip.dst', pieMode: 'split' }).splitField, '');
    // only a pie shows Other
    assert.equal(spigraphQueryFromGraph(graph({ viewMode: 'pie', field: 'ip.dst', other: true })).other, 'true');
    assert.equal(spigraphQueryFromGraph(graph({ viewMode: 'pie', field: 'ip.dst' })).other, undefined);
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'bar', exp: 'ip.dst', other: 'true' }).other, false);
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

  test('a split pie\'s pie count comes from the page', () => {
    const page = graphFromSpigraphQuery({ spiGraphType: 'pie', exp: 'ip.dst', subFields: 'protocols', pieMode: 'split', pieCount: '3' });
    assert.equal(graphOnSpigraph(saved, page).splitLength, 3);
  });

  test('SPI Graph\'s layout sets or clears a timeline\'s percent axis and leaves a log one alone', () => {
    const log = graph({ viewMode: 'timeline', splitField: 'protocols', yScale: 'log' });
    const percent = graph({ viewMode: 'timeline', splitField: 'protocols', yScale: 'percent' });
    const stackedPage = graphFromSpigraphQuery({ spiGraphType: 'default', exp: 'protocols', timelineMode: 'stacked' });
    const percentPage = graphFromSpigraphQuery({ spiGraphType: 'default', exp: 'protocols', timelineMode: 'percent' });
    assert.equal(graphOnSpigraph(log, stackedPage).yScale, 'log');
    assert.equal(graphOnSpigraph(log, percentPage).yScale, 'percent');
    assert.equal(graphOnSpigraph(percent, stackedPage).yScale, 'linear');
    assert.equal(graphOnSpigraph(percent, percentPage).yScale, 'percent');
  });

  test('an unchanged page is the saved graph', () => {
    const page = graphFromSpigraphQuery(spigraphQueryFromGraph(saved), { fieldsArr });
    assert.deepEqual(graphOnSpigraph(saved, page), saved);
  });

  test('a table\'s fields are the ones SPI Graph\'s unique table shows', () => {
    const table = graph({ viewMode: 'table', fields: ['ip.src', 'ip.dst', 'port.dst'], metrics: ['bytes'] });
    const query = spigraphQueryFromGraph(table);
    assert.equal(query.tableMode, 'unique');
    assert.equal(query.subFields, 'ip.dst,port.dst');
    const page = graphFromSpigraphQuery(query, { fieldsArr });
    assert.deepEqual(graphOnSpigraph(table, page), table);
    // a field taken off on the page is gone from the saved graph; the metric
    // columns SPI Graph can't show stay
    const changed = graphFromSpigraphQuery({ spiGraphType: 'table', exp: 'protocols', subFields: 'ip.dst', tableMode: 'unique' });
    assert.deepEqual(graphOnSpigraph(table, changed).fields, ['protocols', 'ip.dst']);
    assert.deepEqual(graphOnSpigraph(table, changed).metrics, ['bytes']);
    // switched to the intersection layout on the page, it saves as an intersection
    assert.equal(graphOnSpigraph(table, graphFromSpigraphQuery({ ...query, tableMode: undefined })).viewMode, 'intersection');
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

  test('a split timeline opens SPI Graph\'s timeline of its split field', () => {
    const link = widgetFullPageLink({ query: {} }, store, { viewMode: 'timeline', splitField: 'protocols', splitLength: 10, metricType: 'bytes' });
    assert.equal(link.path, '/spigraph');
    assert.equal(link.query.spiGraphType, 'default');
    assert.equal(link.query.exp, 'protocols');
    assert.equal(link.query.size, 10);
    assert.equal(link.query.graphType, 'totBytesHisto');
    assert.equal(widgetFullPageLink({ query: {} }, store, { viewMode: 'timeline', splitField: 'protocols', splitLength: 3 }).query.size, 5);
    // a split field SPI Graph can't aggregate falls back to Sessions
    assert.equal(widgetFullPageLink({ query: {} }, store, { viewMode: 'timeline', splitField: 'nope' }).path, '/sessions');
  });

  test('a split timeline opens SPI Graph stacked, a percent one as shares', () => {
    assert.equal(spigraphQueryFromGraph(graph({ viewMode: 'timeline', splitField: 'protocols' })).timelineMode, 'stacked');
    assert.equal(spigraphQueryFromGraph(graph({ viewMode: 'timeline', splitField: 'protocols', yScale: 'percent' })).timelineMode, 'percent');
  });

  test('a split time of day opens SPI Graph\'s Time of day, an unsplit one has no full page', () => {
    const link = widgetFullPageLink({ query: {} }, store, { viewMode: 'timeofday', splitField: 'protocols', splitLength: 5 });
    assert.equal(link.path, '/spigraph');
    assert.equal(link.query.spiGraphType, 'timeofday');
    assert.equal(link.query.exp, 'protocols');
    assert.equal(widgetFullPageLink({ query: {} }, store, { viewMode: 'timeofday' }), null);
    assert.equal(widgetIsGraph({ viewMode: 'timeofday', splitField: 'protocols' }), true);
    assert.equal(widgetIsGraph({ viewMode: 'timeofday' }), false);
  });

  test('a matrix opens SPI Graph\'s matrix of its two fields', () => {
    const link = widgetFullPageLink({ query: {} }, store, { ...graph({ viewMode: 'matrix', fields: ['ip.src', 'port.dst'], length: 10 }), id: 'w9' });
    assert.equal(link.query.spiGraphType, 'matrix');
    assert.equal(link.query.exp, 'ip.src');
    assert.equal(link.query.subFields, 'port.dst');
  });

  test('a split timeline is a graph, an unsplit one is not', () => {
    assert.equal(widgetIsGraph({ viewMode: 'timeline', splitField: 'protocols' }), true);
    assert.equal(widgetIsGraph({ viewMode: 'timeline' }), false);
    // SPI Graph's Timeline at 20 values is saved as the most a split timeline draws
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'default', exp: 'protocols', size: '20' }, { fieldsArr }).splitLength, 10);
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'default' }, { fieldsArr }), null);
  });

  test('a horizontal bar opens SPI Graph\'s bar horizontal', () => {
    const link = widgetFullPageLink({ query: {} }, store, { ...graph({ viewMode: 'bar', field: 'ip.src', orientation: 'horizontal' }), id: 'w3' });
    assert.equal(link.query.barOrientation, 'horizontal');
    assert.equal(widgetFullPageLink({ query: {} }, store, { ...graph({ viewMode: 'bar', field: 'ip.src' }), id: 'w4' }).query.barOrientation, undefined);
  });

  test('session-wide widgets open Sessions, the rest have no page', () => {
    assert.equal(widgetFullPageLink(route, store, { viewMode: 'timeline' }).path, '/sessions');
    assert.equal(widgetFullPageLink(route, store, { viewMode: 'stats' }), null);
    assert.equal(widgetFullPageLink(route, store, { viewMode: 'bar', field: 'nope' }), null);
  });
});

describe('widget fields', () => {
  const fields = ['f1', 'f2', 'f3', 'f4', 'f5', 'f6', 'f7'];

  test('each view mode takes up to its own limit', () => {
    assert.equal(fieldCountLimits('intersection')[1], 7);
    assert.equal(fieldCountLimits('table')[1], 7);
    for (const mode of ['pie', 'treemap', 'sankey']) { assert.equal(fieldCountLimits(mode)[1], 7, mode); }
    assert.deepEqual(widgetFields({ viewMode: 'intersection', fields }), fields);
    assert.deepEqual(widgetFields({ viewMode: 'pie', fields: [...fields, 'f8'] }), fields);
    assert.deepEqual(widgetFields({ viewMode: 'sessions', fields: [...fields, 'f8'] }), [...fields, 'f8']);
    assert.deepEqual(widgetFields({ viewMode: 'bar', fields }), ['f1']);
  });

  test('a split pie nests its fields under the split field', () => {
    assert.deepEqual(hierarchyFields({ viewMode: 'pie', fields: ['f1', 'f2'], splitField: 's' }), ['s', 'f1', 'f2']);
    assert.deepEqual(hierarchyFields({ viewMode: 'pie', fields: ['f1'] }), ['f1']);
    // only a pie draws small multiples
    assert.deepEqual(hierarchyFields({ viewMode: 'treemap', fields: ['f1'], splitField: 's' }), ['f1']);
  });
});

describe('viewModeLabelKey', () => {
  test('only own view modes have labels', () => {
    assert.equal(viewModeLabelKey('bar'), 'sessions.summary.barChart');
    assert.equal(viewModeLabelKey('constructor'), undefined);
    assert.equal(viewModeLabelKey('__proto__'), undefined);
  });
});

describe('tree graphs', () => {
  test('a tree widget opens the SPI Graph tree with its root and depth', () => {
    const q = spigraphQueryFromGraph({ viewMode: 'tree', fields: ['ip.src', 'ip.dst'], length: 500, treeRoot: '10.0.0.1', treeDepth: 4 });
    assert.equal(q.spiGraphType, 'tree');
    assert.equal(q.srcField, 'source.ip');
    assert.equal(q.dstField, 'destination.ip');
    assert.equal(q.connectionsLength, 500);
    assert.equal(q.treeRoot, '10.0.0.1');
    assert.equal(q.treeDepth, 4);
  });
  test('the SPI Graph tree page is a tree graph', () => {
    const g = graphFromSpigraphQuery({ spiGraphType: 'tree', srcField: 'source.ip', dstField: 'destination.ip', connectionsLength: 100, treeRoot: '10.0.0.1', treeDepth: 2 }, { fieldsArr });
    assert.equal(g.viewMode, 'tree');
    assert.deepEqual(g.fields, ['ip.src', 'ip.dst']);
    assert.equal(g.treeRoot, '10.0.0.1');
    assert.equal(g.treeDepth, 2);
    assert.equal(graphFromSpigraphQuery({ spiGraphType: 'tree', srcField: 'source.ip', dstField: 'destination.ip' }, { fieldsArr }).treeDepth, 3);
  });
});

describe('normalizeWidget orderBy', () => {
  test('defaults to value and only accepts key', () => {
    assert.equal(widgetsFromConfig({ widgets: [{ viewMode: 'bar', field: 'port.dst' }] })[0].orderBy, 'value');
    assert.equal(widgetsFromConfig({ widgets: [{ viewMode: 'bar', field: 'port.dst', orderBy: 'key' }] })[0].orderBy, 'key');
    assert.equal(widgetsFromConfig({ widgets: [{ viewMode: 'bar', field: 'port.dst', orderBy: 'nope' }] })[0].orderBy, 'value');
  });
  test('a saved graph keeps its orderBy', () => {
    assert.equal(graphFromWidget({ viewMode: 'bar', field: 'port.dst', orderBy: 'key' }).orderBy, 'key');
    assert.ok(GRAPH_KEYS.includes('orderBy'));
  });
});

describe('migrateDashboard', () => {
  test('a current config is returned as is', () => {
    const cfg = { version: DASHBOARD_VERSION, widgets: [{ id: 'a', viewMode: 'stats' }] };
    assert.equal(migrateDashboard(cfg), cfg);
  });
  test('a config without a version is version 1 and comes out current', () => {
    const out = migrateDashboard({ widgets: [{ id: 'a', viewMode: 'stats' }] });
    assert.equal(out.version, DASHBOARD_VERSION);
    assert.equal(out.widgets.length, 1);
  });
  test('a newer config is left alone so a mixed-version cluster still loads it', () => {
    const cfg = { version: DASHBOARD_VERSION + 5, widgets: [] };
    assert.equal(migrateDashboard(cfg), cfg);
  });
  test('garbage in gives an empty current config', () => {
    assert.deepEqual(migrateDashboard(null), { version: DASHBOARD_VERSION });
    assert.deepEqual(migrateDashboard('x'), { version: DASHBOARD_VERSION });
    assert.equal(migrateDashboard({ version: 'old' }).version, DASHBOARD_VERSION);
  });
});

describe('dashboard filter', () => {
  const views = [{ id: 'v1', name: 'web', expression: 'port.dst == 80' }];

  test('pageExpression ANDs the global search and the dashboard filter', () => {
    assert.equal(pageExpression({ query: {} }, { state: {} }), undefined);
    assert.equal(pageExpression({ query: { expression: 'ip == 1.1.1.1' } }, { state: { dashboardExpression: '' } }), 'ip == 1.1.1.1');
    assert.equal(pageExpression({ query: {} }, { state: { dashboardExpression: 'protocols == dns' } }), 'protocols == dns');
    assert.equal(pageExpression({ query: { expression: 'ip == 1.1.1.1' } }, { state: { dashboardExpression: 'protocols == dns' } }), '(ip == 1.1.1.1) && (protocols == dns)');
  });

  test('combinedExpression puts the widget filter on top of the page search', () => {
    const store = { state: { dashboardExpression: 'protocols == dns', views } };
    assert.equal(combinedExpression({ query: {} }, store, { expression: 'dns.status == NXDOMAIN' }), '(protocols == dns) && (dns.status == NXDOMAIN)');
    assert.equal(combinedExpression({ query: { expression: 'node == a' } }, store, { view: 'v1' }), '((node == a) && (protocols == dns)) && (port.dst == 80)');
    assert.equal(combinedExpression({ query: {} }, store, {}), 'protocols == dns');
    assert.equal(combinedExpression({ query: {} }, { state: { views } }, {}), undefined);
  });

  test('the full page link carries the dashboard filter', () => {
    const store = { state: { timeRange: '1', dashboardExpression: 'protocols == dns', views: [] } };
    assert.equal(widgetFullPageLink({ query: {} }, store, { viewMode: 'timeline' }).query.expression, 'protocols == dns');
  });
});

describe('connection tree', () => {
  // a -> b (3), a -> c (7), b -> d (1), b -> a (a cycle), e -> e (a self link)
  const nodes = [
    { id: 'a', type: 1, sessions: 10 }, { id: 'b', type: 3, sessions: 5 }, { id: 'c', type: 2, sessions: 1 },
    { id: 'd', type: 2, sessions: 1 }, { id: 'e', type: 1, sessions: 20 }
  ];
  const links = [
    { source: 0, target: 1, value: 3 }, { source: 0, target: 2, value: 7 }, { source: 1, target: 3, value: 1 },
    { source: 1, target: 0, value: 1 }, { source: 4, target: 4, value: 9 }
  ];
  const count = (t) => 1 + t.children.reduce((sum, c) => sum + count(c), 0);

  test('findTreeRoot: the asked for id when present, else the busiest source', () => {
    assert.equal(findTreeRoot(nodes, 'b').id, 'b');
    assert.equal(findTreeRoot(nodes, 'nope'), null);
    assert.equal(findTreeRoot(nodes, '').id, 'e');
    assert.equal(findTreeRoot([], ''), null);
    assert.equal(findTreeRoot([{ id: 'x', type: 2, sessions: 9 }], ''), null);
  });

  test('buildConnectionTree: heaviest first, each host once, no cycles, depth honored', () => {
    const t = buildConnectionTree({ nodes, links }, nodes[0], 3);
    assert.equal(t.id, 'a');
    assert.equal(t.link, undefined);
    assert.deepEqual(t.children.map(c => c.id), ['c', 'b']);
    assert.deepEqual(t.children[1].children.map(c => c.id), ['d']);
    assert.equal(t.children[1].link.value, 3);
    assert.equal(t.children[1].link.source.id, 'a');
    assert.equal(t.children[1].link.target.id, 'b');
    assert.equal(buildConnectionTree({ nodes, links }, nodes[0], 1).children[1].children.length, 0);
    assert.equal(count(buildConnectionTree({ nodes, links }, nodes[4], 3)), 1);
  });

  test('a node fans out to MAX_TREE_CHILDREN and the tree stops at MAX_TREE_NODES', () => {
    const wide = { nodes: [{ id: 'r', type: 1, sessions: 1 }], links: [] };
    for (let i = 0; i < 40; i++) { wide.nodes.push({ id: `n${i}`, type: 2, sessions: 1 }); wide.links.push({ source: 0, target: i + 1, value: i }); }
    const t = buildConnectionTree(wide, wide.nodes[0], 1);
    assert.equal(t.children.length, MAX_TREE_CHILDREN);
    assert.equal(t.children[0].id, 'n39');

    const deep = { nodes: [{ id: 'r', type: 1, sessions: 1 }], links: [] };
    for (let i = 0; i < MAX_TREE_CHILDREN; i++) {
      deep.nodes.push({ id: `c${i}`, type: 3, sessions: 1 }); const ci = deep.nodes.length - 1;
      deep.links.push({ source: 0, target: ci, value: 1 });
      for (let j = 0; j < MAX_TREE_CHILDREN; j++) {
        deep.nodes.push({ id: `c${i}_${j}`, type: 2, sessions: 1 });
        deep.links.push({ source: ci, target: deep.nodes.length - 1, value: 1 });
      }
    }
    assert.equal(count(buildConnectionTree(deep, deep.nodes[0], 2)), MAX_TREE_NODES);
  });
});
