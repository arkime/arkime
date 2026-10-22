/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { isFieldMode, DEFAULT_VIEW_MODES, defaultLength, METRIC_AGGS, SPLIT_LENGTHS, BAR_MODES } from './widgets/viewModes';
import FieldService from '../search/FieldService';
import Utils from '../utils/utils';

// Saved and shared dashboards can hold any JSON (a shared one is written by
// another user), so each widget value is only kept when it has the right type;
// an object there would throw wherever it is turned into a string.
const str = (v, def = '') => (typeof v === 'string' ? v : def);
const int = (v) => ((typeof v === 'number' || typeof v === 'string') ? parseInt(v, 10) : NaN);

// widgets span 1-4 columns and 1-8 rows; anything missing/invalid falls back to
// `def` (rows allow a larger max since the grid uses a fine 160px row unit)
const spanOrDefault = (v, def, max = 4) => {
  const n = int(v);
  return n >= 1 && n <= max ? n : def;
};

/**
 * Builds a default widget definition for a field expression
 */
export const makeWidgetDef = (fieldExp, overrides = {}) => {
  const viewMode = overrides.viewMode || DEFAULT_VIEW_MODES[fieldExp] || 'bar';
  return {
    id: Utils.createRandomString(),
    field: fieldExp,
    fields: fieldExp ? [fieldExp] : [], // 1-3 fields for multi-field widgets
    viewMode,
    metricType: 'sessions',
    metrics: [], // table multi-metric columns (falls back to metricType)
    metricAgg: 'sum', // sum/avg/min/max/cardinality of the metric field(s)
    sortMetric: 'sessions',
    // bar: optional second field each value is split by
    splitField: '',
    splitLength: 5,
    barMode: 'stacked',
    text: '', // text widget content
    // top-N cap, or a session sample size for connections
    length: defaultLength(viewMode),
    order: 'desc',
    expression: '',
    view: '',
    height: 3, // 3 × 160px row unit = 480px (chart default)
    width: 2,
    title: '',
    ...overrides
  };
};

/**
 * Ensures a saved widget has an id and all expected fields
 */
export const normalizeWidget = (w) => {
  // fields[] is the source of truth (1-3); field stays = fields[0] for back-compat
  const fields = Array.isArray(w.fields) && w.fields.length
    ? w.fields.filter(f => typeof f === 'string').slice(0, 3)
    : (str(w.field) ? [w.field] : []);
  const viewMode = str(w.viewMode) || DEFAULT_VIEW_MODES[str(w.field)] || 'bar';
  const limit = int(w.length);
  return makeWidgetDef(fields[0] || '', {
    id: str(w.id) || Utils.createRandomString(),
    fields,
    viewMode,
    metricType: str(w.metricType) || 'sessions',
    metrics: Array.isArray(w.metrics) ? w.metrics.filter(m => typeof m === 'string').slice(0, 4) : [],
    metricAgg: METRIC_AGGS.includes(w.metricAgg) ? w.metricAgg : 'sum',
    sortMetric: str(w.sortMetric) || str(w.metricType) || 'sessions',
    splitField: str(w.splitField),
    splitLength: SPLIT_LENGTHS.includes(w.splitLength) ? w.splitLength : 5,
    barMode: BAR_MODES.includes(w.barMode) ? w.barMode : 'stacked',
    text: str(w.text),
    length: limit > 0 ? limit : defaultLength(viewMode),
    order: w.order === 'asc' ? 'asc' : 'desc',
    expression: str(w.expression),
    view: str(w.view),
    height: spanOrDefault(w.height, 3, 8),
    width: spanOrDefault(w.width, 2),
    title: str(w.title)
  });
};

/**
 * Curated starter dashboard (no saved config): the capture-wide band
 * (stats/time/timeline), a geo map + top-talkers bar, then one of each
 * aggregating visualization spanning sessions / bytes / packets metrics.
 */
export const defaultWidgets = () => [
  // capture-wide summary band
  makeWidgetDef('', { viewMode: 'stats', width: 2, height: 1 }),
  makeWidgetDef('', { viewMode: 'time', width: 2, height: 1 }),
  makeWidgetDef('', { viewMode: 'timeline', width: 4, height: 2 }),
  // geo distribution + all-IP top talkers (bar)
  makeWidgetDef('country.src', { viewMode: 'map', width: 2, height: 3 }),
  makeWidgetDef('ip', { viewMode: 'bar', width: 2, height: 3, length: 15 }),
  // protocols breakdown (pie)
  makeWidgetDef('protocols', { viewMode: 'pie', width: 2, height: 3, length: 8 }),
  // multi-metric table: Dst IP:Port ranked by bytes, with session/packet columns
  makeWidgetDef('ip.dst:port', {
    viewMode: 'table',
    width: 2,
    height: 3,
    metrics: ['sessions', 'bytes', 'packets'],
    sortMetric: 'bytes',
    metricType: 'bytes'
  }),
  // Dst IP heatmap by packets over time
  makeWidgetDef('ip.dst', { viewMode: 'heatmap', width: 2, height: 3, metricType: 'packets' }),
  // Src IP treemap sized by bytes
  makeWidgetDef('ip.src', { viewMode: 'treemap', width: 2, height: 3, metricType: 'bytes' }),
  // Src → Dst IP combinations (intersection)
  makeWidgetDef('ip.src', { viewMode: 'intersection', width: 4, height: 3, fields: ['ip.src', 'ip.dst'] })
];

/**
 * The widgets a saved dashboard config (a summaryConfig shareable's data or the
 * per-user state) shows: its widgets[], else its legacy v6 fields[] migrated to
 * widgets, else the starter dashboard. Each widget is normalized and ids are
 * unique.
 */
export const widgetsFromConfig = (cfg) => {
  let widgets;
  if (Array.isArray(cfg?.widgets) && cfg.widgets.length) {
    widgets = cfg.widgets.map(w => normalizeWidget(w || {}));
  } else if (Array.isArray(cfg?.fields) && cfg.fields.length) {
    widgets = cfg.fields.map(f => normalizeWidget({
      field: f?.field,
      viewMode: f?.viewMode,
      metricType: f?.metricType,
      length: cfg.resultsLimit,
      order: cfg.order
    }));
  } else {
    widgets = defaultWidgets();
  }
  // hand-edited imports may collide, which would leave a widget stuck loading
  // (stream chunks are matched by id)
  const seen = new Set();
  for (const w of widgets) {
    if (!w.id || seen.has(w.id)) { w.id = Utils.createRandomString(); }
    seen.add(w.id);
  }
  return widgets;
};

/* Saved graphs (spigraphConfig shareables) ---------------------------------
   A saved graph is a widget definition without its identity and layout, so
   SPI Graph, the graphs library and the dashboard all speak one shape. A
   dashboard copies a graph in; it never references one. See SHAREABLES.md. */

// the widget keys a saved graph carries (everything but id/title/width/height/text)
export const GRAPH_KEYS = ['field', 'fields', 'viewMode', 'metricType', 'metrics', 'metricAgg', 'sortMetric', 'splitField', 'splitLength', 'barMode', 'length', 'order', 'expression', 'view'];

// view modes a graph can have: the field-bound dashboard types SPI Graph draws
export const GRAPH_VIEW_MODES = ['bar', 'pie', 'table', 'intersection', 'treemap', 'sankey', 'heatmap', 'connections'];

/** True when a widget can be saved as a graph. */
export const widgetIsGraph = (w) => GRAPH_VIEW_MODES.includes(w?.viewMode) && (Array.isArray(w.fields) ? w.fields.length > 0 : !!w.field);

/** The saved-graph data of a widget (normalized, layout and title dropped). */
export const graphFromWidget = (w) => {
  const n = normalizeWidget(w || {});
  const graph = {};
  for (const key of GRAPH_KEYS) { graph[key] = n[key]; }
  return graph;
};

/** A new widget built from a saved graph plus layout overrides (title, width, height). */
export const widgetFromGraph = (graph, overrides = {}) => normalizeWidget({ ...(graph || {}), id: undefined, ...overrides });

// SPI Graph's types that have a graph (its Timeline has none: one histogram per value)
const SPIGRAPH_TO_VIEW_MODE = { bar: 'bar', pie: 'pie', treemap: 'treemap', sankey: 'sankey', heatmap: 'heatmap', connections: 'connections', table: 'table' };

/**
 * The graph the SPI Graph page is showing, from its url query and the current
 * search. The search expression becomes the graph's local filter, so a widget
 * made from it keeps showing the same slice under any dashboard search.
 * @param {object} query - the SPI Graph route query, with the page's own state
 *   filled in where the url leaves it out (exp, size, the heatmap intensity as
 *   graphType, connections' src/dst db fields)
 * @param {object} opts - { expression (the current search), fieldsArr (to map db fields back to exps) }
 * @returns {object|null} graph data, or null when the page's type has no graph
 */
export const graphFromSpigraphQuery = (query, { expression, fieldsArr } = {}) => {
  const type = query.spiGraphType || 'default';
  const viewMode = SPIGRAPH_TO_VIEW_MODE[type];
  if (!viewMode) { return null; }
  const subFields = str(query.subFields).split(',').filter(Boolean);
  const exp = str(query.exp) || str(query.field);
  let fields = exp ? [exp, ...subFields] : [];
  let overrides = {};
  if (type === 'table' && subFields.length) {
    // SPI Graph's nested table is the dashboard's intersection view
    overrides.viewMode = 'intersection';
  } else if (type === 'bar') {
    fields = exp ? [exp] : [];
    overrides = { splitField: subFields[0] || '', barMode: query.barMode === 'grouped' ? 'grouped' : 'stacked' };
  } else if (type === 'connections') {
    // the connections url carries database field names
    const byDb = (db) => (fieldsArr || []).find(f => f.dbField === db)?.exp || (db === 'ip.dst:port' ? db : '');
    fields = [byDb(str(query.srcField) || 'source.ip'), byDb(str(query.dstField) || 'destination.ip')];
  } else if (type === 'heatmap' && str(query.graphType) && query.graphType !== 'sessionsHisto') {
    // the intensity metric is a <dbField>Histo key
    const db = query.graphType.replace(/Histo$/, '');
    const metric = (fieldsArr || []).find(f => f.dbField === db)?.exp;
    if (metric) { overrides.metricType = metric; }
  }
  if (!fields.length || !fields[0]) { return null; }
  // connections' limit is its session sample size, a url param of its own
  const limit = int(type === 'connections' ? query.connectionsLength : query.size);
  const graph = normalizeWidget({
    field: fields[0],
    fields,
    viewMode,
    length: limit > 0 ? limit : defaultLength(viewMode),
    expression: str(expression),
    view: str(query.view),
    ...overrides
  });
  const data = {};
  for (const key of GRAPH_KEYS) { data[key] = graph[key]; }
  return data;
};

/**
 * The SPI Graph url query that shows a saved graph (no time: the page keeps
 * its own). Null for a graph SPI Graph can't draw.
 */
export const spigraphQueryFromGraph = (graph) => {
  const g = normalizeWidget(graph || {});
  const fields = g.fields.length ? g.fields : (g.field ? [g.field] : []);
  if (!fields.length) { return null; }
  const base = { exp: fields[0], size: g.length, view: g.view || undefined };
  switch (g.viewMode) {
  case 'bar':
    return { ...base, spiGraphType: 'bar', subFields: g.splitField || undefined, barMode: g.barMode };
  case 'pie':
  case 'treemap':
  case 'sankey':
    return { ...base, spiGraphType: g.viewMode, subFields: fields.slice(1).join(',') || undefined };
  case 'table':
    return { ...base, spiGraphType: 'table', subFields: undefined };
  case 'intersection':
    return { ...base, spiGraphType: 'table', subFields: fields.slice(1).join(',') || undefined };
  case 'heatmap': {
    const db = g.metricType && g.metricType !== 'sessions' ? FieldService.getField(g.metricType, true)?.dbField : undefined;
    return { ...base, spiGraphType: 'heatmap', graphType: db ? `${db}Histo` : undefined };
  }
  case 'connections': {
    const toDb = (e) => ((e === 'ip.dst:port' || e === 'destination.ip:port') ? e : FieldService.getField(e, true)?.dbField);
    return { spiGraphType: 'connections', srcField: toDb(fields[0]), dstField: toDb(fields[1]), connectionsLength: g.length, view: base.view };
  }
  default:
    return null;
  }
};

// the graph keys SPI Graph shows and lets the user change (a heatmap's
// metricType is its intensity); it can't show the others (metric columns,
// aggregation, sort metric, order, split length)
const SPIGRAPH_KEYS = ['field', 'fields', 'viewMode', 'splitField', 'barMode', 'length', 'expression', 'view'];

/**
 * The graph SPI Graph shows, as saved-graph data: what the page shows on top of
 * the saved graph it loaded, if any, so the settings it can't show survive a
 * save over that graph.
 * @param {object|null} saved - the loaded saved graph's data
 * @param {object|null} page - graphFromSpigraphQuery for the page (null: no graph)
 */
export const graphOnSpigraph = (saved, page) => {
  if (!page) { return null; }
  if (!saved) { return page; }
  const keys = page.viewMode === 'heatmap' ? [...SPIGRAPH_KEYS, 'metricType'] : SPIGRAPH_KEYS;
  const merged = { ...saved };
  for (const key of keys) { merged[key] = page[key]; }
  return graphFromWidget(merged);
};

// map v7-only field view modes to the closest mode a v6 viewer can render
// (v6 knows only bar/pie/table) for the dual-written legacy fields[] shape
const V6_VIEW_MODES = { bar: 'bar', pie: 'pie', table: 'table', intersection: 'table', heatmap: 'bar', treemap: 'bar', sankey: 'table', connections: 'table' };

/**
 * Projects a widget list onto the v6 summaryConfig shape that is dual-written
 * alongside widgets[], so a v6 viewer in a mixed-version cluster can still read
 * a dashboard saved by v7. Session-wide widgets (timeline/map/stats/time) have
 * no v6 equivalent and are left out. See SHAREABLES.md.
 *
 * It follows widget order, so anything that reorders widgets has to rebuild
 * this too or the two shapes drift apart.
 *
 * @param {object[]} widgets - the dashboard's widgets, in display order
 * @returns {object} { fields, resultsLimit, order }
 */
export const toV6Shape = (widgets) => {
  const fieldWidgets = (widgets || []).filter(w => w.field && isFieldMode(w.viewMode));

  return {
    fields: fieldWidgets.map(w => ({
      field: w.field,
      viewMode: V6_VIEW_MODES[w.viewMode] || 'bar',
      metricType: w.metricType || 'sessions'
    })),
    resultsLimit: fieldWidgets[0]?.length || 20,
    order: fieldWidgets[0]?.order || 'desc'
  };
};
