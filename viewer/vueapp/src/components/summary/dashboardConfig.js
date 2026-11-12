/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { isFieldMode, DEFAULT_VIEW_MODES, defaultLength, METRIC_AGGS, isPercentileAgg, metricAggsFor, hasOrder, hasLevels, allowsBucketInterval, COMPARES, SPLIT_LENGTHS, BAR_MODES, BAR_ORIENTATIONS, SERIES_TYPES, Y_SCALES, MAX_FIELDS, TREE_DEPTHS, DEFAULT_TREE_DEPTH, slotMetrics } from './widgets/viewModes';
import { CIRCLE_MAX_NODES, DEFAULT_CIRCLE_MAX_NODES } from './widgets/connectionCircle';
import FieldService from '../search/FieldService';
import { VARIABLE_NAME_RE, CONTROL_TYPES } from './widgets/dashboardVariables';
import Utils from '../utils/utils';

// Saved and shared dashboards can hold any JSON (a shared one is written by
// another user), so each widget value is only kept when it has the right type;
// an object there would throw wherever it is turned into a string.
const str = (v, def = '') => (typeof v === 'string' ? v : def);
const int = (v) => ((typeof v === 'number' || typeof v === 'string') ? parseInt(v, 10) : NaN);
// a non-negative whole number, 0 when missing or invalid (0 = unset)
export const nonNeg = (v) => { const n = int(v); return n > 0 ? n : 0; };

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
    fields: fieldExp ? [fieldExp] : [], // up to MAX_FIELDS for multi-field widgets
    viewMode,
    metricType: 'sessions',
    metrics: [], // table multi-metric columns (falls back to metricType)
    metricAgg: 'sum', // sum/avg/min/max/cardinality of the metric field(s)
    sortMetric: 'sessions',
    // bar: optional second field each value is split by
    splitField: '',
    splitLength: 5,
    barMode: 'stacked',
    orientation: 'vertical', // bar: 'horizontal' lays the bars along the axis
    seriesType: 'bars', // timeline: 'bars' or 'lines'
    yScale: 'linear', // timeline: 'linear' or 'log'
    other: false, // pie: an "Other" slice for what falls outside the top N
    bucketInterval: 0, // numeric field: range width per bucket (0 = one bucket per value)
    gaugeMax: 0, // gauge: the arc's full value (0 = the value itself)
    gaugeWarn: 0, // gauge/metric/table/bar/timeline: the warning level, amber at or above it (0 = none)
    gaugeCrit: 0, // the critical level, red at or above it (0 = none)
    compare: '', // timeline/area/metric: also the same window a 'day' or a 'week' earlier ('' = no)
    text: '', // markdown widget content
    treeRoot: '', // connection tree: the value the tree starts from ('' = the busiest source)
    treeDepth: DEFAULT_TREE_DEPTH, // connection tree: hops drawn from the root
    colorField: '', // connection circle: the field its arcs are colored by ('' = protocols)
    maxNodes: DEFAULT_CIRCLE_MAX_NODES, // connection circle: how many hosts the ring shows
    // top-N cap, or a session sample size for connections
    length: defaultLength(viewMode),
    order: 'desc',
    orderBy: 'value', // 'value': Top/Bottom N by the metric; 'key': by the field's own value
    sortField: '', // sessions list: the column it sorts by ('' = start time)
    variable: '', // control: the variable its picks set ('' = named after its field)
    applyAll: true, // control: its picks filter every widget, not just ${variable} uses
    controlType: 'list', // control: 'list' of top values, 'range' slider or 'text' box
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
  // fields[] is the source of truth; field stays = fields[0] for back-compat
  const fields = Array.isArray(w.fields) && w.fields.length
    ? w.fields.filter(f => typeof f === 'string').slice(0, MAX_FIELDS)
    : (str(w.field) ? [w.field] : []);
  // the text widget of earlier 7.0 builds is the markdown widget
  const savedMode = str(w.viewMode) === 'text' ? 'markdown' : str(w.viewMode);
  const viewMode = savedMode || DEFAULT_VIEW_MODES[str(w.field)] || 'bar';
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
    orientation: BAR_ORIENTATIONS.includes(w.orientation) ? w.orientation : 'vertical',
    seriesType: SERIES_TYPES.includes(w.seriesType) ? w.seriesType : 'bars',
    yScale: Y_SCALES.includes(w.yScale) ? w.yScale : 'linear',
    sparkline: w.sparkline === true,
    other: w.other === true,
    bucketInterval: nonNeg(w.bucketInterval),
    gaugeMax: nonNeg(w.gaugeMax),
    gaugeWarn: nonNeg(w.gaugeWarn),
    gaugeCrit: nonNeg(w.gaugeCrit),
    compare: COMPARES.includes(w.compare) ? w.compare : '',
    text: str(w.text),
    treeRoot: str(w.treeRoot),
    colorField: str(w.colorField),
    maxNodes: CIRCLE_MAX_NODES.includes(w.maxNodes) ? w.maxNodes : DEFAULT_CIRCLE_MAX_NODES,
    treeDepth: TREE_DEPTHS.includes(w.treeDepth) ? w.treeDepth : DEFAULT_TREE_DEPTH,
    length: limit > 0 ? limit : defaultLength(viewMode),
    order: w.order === 'asc' ? 'asc' : 'desc',
    orderBy: w.orderBy === 'key' ? 'key' : 'value',
    sortField: str(w.sortField),
    variable: VARIABLE_NAME_RE.test(str(w.variable)) ? w.variable : '',
    applyAll: w.applyAll !== false,
    controlType: CONTROL_TYPES.includes(w.controlType) ? w.controlType : 'list',
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
  // links to the system dashboards (one per protocol) and the saved ones
  makeWidgetDef('', { viewMode: 'navigation', width: 4, height: 1 }),
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
 * The dashboard config format version this code writes. A saved dashboard
 * (shareable data, per-user state or a system dashboard file) carries it as
 * data.version; one without it is version 1. Bump it when the shape changes
 * and add the upgrade to migrateDashboard, which runs on every load.
 */
export const DASHBOARD_VERSION = 1;

// most widgets a dashboard shows (MAX_WIDGETS in apiShareables.js and the
// /api/sessions/summary limit)
export const MAX_WIDGETS = 100;

/**
 * Upgrades a dashboard config written by an older version to the current
 * shape. Returns the config as is (plus version) when it is current, or newer
 * than this code knows, so a mixed-version cluster still loads it.
 * @param {object} cfg - { version?, colorScheme?, widgets? | fields? }
 * @returns {object} the config at DASHBOARD_VERSION
 */
export const migrateDashboard = (cfg) => {
  if (!cfg || typeof cfg !== 'object') { return { version: DASHBOARD_VERSION }; }
  const from = Number.isInteger(cfg.version) && cfg.version > 0 ? cfg.version : 1;
  if (cfg.version === from && from >= DASHBOARD_VERSION) { return cfg; }
  const out = { ...cfg };
  // version 1 -> 2: (none yet) each step upgrades one version:
  // if (out.version < 2) { ...; out.version = 2; }
  out.version = DASHBOARD_VERSION;
  return out;
};

/**
 * The widgets a saved dashboard config (a summaryConfig shareable's data or the
 * per-user state) shows: its widgets[], else its legacy v6 fields[] migrated to
 * widgets, else the starter dashboard. Each widget is normalized and ids are
 * unique.
 */
export const widgetsFromConfig = (cfg) => {
  let widgets;
  if (Array.isArray(cfg?.widgets) && cfg.widgets.length) {
    widgets = cfg.widgets.slice(0, MAX_WIDGETS).map(w => normalizeWidget(w || {}));
  } else if (Array.isArray(cfg?.fields) && cfg.fields.length) {
    widgets = cfg.fields.slice(0, MAX_WIDGETS).map(f => normalizeWidget({
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
export const GRAPH_KEYS = ['field', 'fields', 'viewMode', 'metricType', 'metrics', 'metricAgg', 'sortMetric', 'splitField', 'splitLength', 'barMode', 'orientation', 'seriesType', 'yScale', 'other', 'bucketInterval', 'treeRoot', 'treeDepth', 'colorField', 'maxNodes', 'length', 'order', 'orderBy', 'gaugeWarn', 'gaugeCrit', 'compare', 'expression', 'view'];

// view modes a graph can have: the field-bound dashboard types SPI Graph
// draws, and a timeline, area or time of day grid split by a field (SPI
// Graph's Timeline, Area and Time of day)
export const GRAPH_VIEW_MODES = ['bar', 'pie', 'table', 'tagcloud', 'intersection', 'treemap', 'sankey', 'heatmap', 'matrix', 'mosaic', 'waffle', 'connections', 'tree', 'circle', 'timeline', 'area', 'timeofday', 'combo', 'scatter'];

// the session-wide types that are a graph once split by a field
const SPLIT_GRAPH_VIEW_MODES = ['timeline', 'area', 'timeofday'];

// SPI Graph's metric choices are the user's timeline metrics (the per value
// histogram sums /api/spigraph returns); a combo's line and a scatter's y
// default to bytes
const DEFAULT_SPIGRAPH_METRIC2 = 'network.bytesHisto';

/** True when a widget can be saved as a graph. */
export const widgetIsGraph = (w) => GRAPH_VIEW_MODES.includes(w?.viewMode) &&
  (SPLIT_GRAPH_VIEW_MODES.includes(w.viewMode) ? !!w.splitField : (Array.isArray(w.fields) ? w.fields.length > 0 : !!w.field));

/**
 * True for the graphs drawn from the hierarchy endpoint, which only counts
 * sessions: a split or nested pie, a split waffle, a nested treemap (the
 * dashboard and SPI Graph both draw them that way).
 */
export const graphIsHierarchyFed = (g) => {
  const nested = Array.isArray(g?.fields) && g.fields.length > 1;
  return (g?.viewMode === 'pie' && (!!g.splitField || nested)) || (g?.viewMode === 'waffle' && !!g.splitField) || (g?.viewMode === 'treemap' && nested);
};

/**
 * True when SPI Graph draws the graph with the dashboard's own widget: its
 * settings are the summary endpoint's (a metric the values are sized by, an
 * aggregation, Top/Bottom order, a bucket interval, a table's metric columns
 * and levels), which SPI Graph's own charts of session counts can't show.
 */
export const graphDrawnAsWidget = (graph) => {
  const g = normalizeWidget(graph || {});
  if (!g.fields.length || graphIsHierarchyFed(g)) { return false; }
  const ordered = g.order === 'asc' || g.orderBy === 'key';
  const metric = g.metricType !== 'sessions';
  switch (g.viewMode) {
  case 'bar':
  case 'pie':
  case 'tagcloud':
  case 'waffle':
    return metric || ordered || g.bucketInterval > 0;
  case 'table':
    return metric || ordered || g.bucketInterval > 0 || g.metrics.some(m => m !== 'sessions') || g.gaugeWarn > 0 || g.gaugeCrit > 0;
  case 'treemap':
    return metric;
  case 'combo':
  case 'scatter':
    return ordered || g.metricAgg !== 'sum';
  default:
    return false;
  }
};

/** The saved-graph data of a widget (normalized, layout and title dropped). */
export const graphFromWidget = (w) => {
  const n = normalizeWidget(w || {});
  const graph = {};
  for (const key of GRAPH_KEYS) { graph[key] = n[key]; }
  return graph;
};

/** A new widget built from a saved graph plus layout overrides (title, width, height). */
export const widgetFromGraph = (graph, overrides = {}) => normalizeWidget({ ...(graph || {}), id: undefined, ...overrides });

// SPI Graph's types and the graph each is (its Timeline, one histogram per
// value, is a timeline split by the field)
const SPIGRAPH_TO_VIEW_MODE = { default: 'timeline', area: 'area', timeofday: 'timeofday', bar: 'bar', pie: 'pie', tagcloud: 'tagcloud', treemap: 'treemap', sankey: 'sankey', heatmap: 'heatmap', matrix: 'matrix', mosaic: 'mosaic', waffle: 'waffle', connections: 'connections', tree: 'tree', circle: 'circle', table: 'table', combo: 'combo', scatter: 'scatter' };

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
  // the timeline and heatmap can draw a percentile of their metric (?metricAgg=p95)
  const percentile = (type === 'default' || type === 'heatmap') && isPercentileAgg(query.metricAgg) ? query.metricAgg : undefined;
  // the settings every type keeps in the url the same way, each only for the
  // types that have it (the url keeps the others' while the type is changed)
  const shared = (vm) => ({
    ...(hasOrder(vm) ? { order: query.order === 'asc' ? 'asc' : 'desc', orderBy: query.orderBy === 'key' ? 'key' : 'value' } : {}),
    ...(allowsBucketInterval(vm) ? { bucketInterval: nonNeg(query.bucketInterval) } : {}),
    ...(hasLevels(vm) ? { gaugeWarn: nonNeg(query.warn), gaugeCrit: nonNeg(query.crit) } : {})
  });
  // a metric's aggregation, for the types that take one other than a sum
  const aggOf = (vm, metrics) => (metrics.some(m => m && m !== 'sessions') && metricAggsFor(vm).includes(query.metricAgg) ? query.metricAgg : 'sum');
  const subFields = str(query.subFields).split(',').filter(Boolean);
  const exp = str(query.exp) || str(query.field);
  // a <dbField>Histo key back to its field exp (undefined for sessionsHisto)
  const histoMetric = (graphType) => {
    if (!str(graphType) || graphType === 'sessionsHisto') { return undefined; }
    const db = graphType.replace(/Histo$/, '');
    return (fieldsArr || []).find(f => f.dbField === db)?.exp;
  };
  if (type === 'default' || type === 'area' || type === 'timeofday') {
    if (!exp) { return null; }
    // a split timeline takes 3, 5 or 10 series; SPI Graph's more are cut to 10
    const size = int(query.size);
    return graphFromWidget({
      viewMode,
      splitField: exp,
      splitLength: SPLIT_LENGTHS.includes(size) ? size : (size > 10 ? 10 : 5),
      metricType: histoMetric(query.graphType) || 'sessions',
      ...(percentile && histoMetric(query.graphType) ? { metricAgg: percentile } : {}),
      // SPI Graph's percent layout is the widget's percent y axis, its
      // overlaid layout the widget's grouped one (always lines)
      ...((type === 'default' || type === 'area') ? { yScale: query.timelineMode === 'percent' ? 'percent' : query.yScale === 'log' ? 'log' : 'linear' } : {}),
      ...((type === 'default' || type === 'area') ? { barMode: query.timelineMode === 'overlaid' ? 'grouped' : 'stacked' } : {}),
      ...(type === 'default' ? { seriesType: query.timelineMode === 'overlaid' || query.seriesType === 'lines' ? 'lines' : 'bars' } : {}),
      ...shared(viewMode),
      expression: str(expression),
      view: str(query.view)
    });
  }
  let fields = exp ? [exp, ...subFields] : [];
  let overrides = {};
  if (type === 'table' && subFields.length) {
    // SPI Graph's intersection table is the dashboard's intersection view, its
    // unique one the dashboard's table
    overrides.viewMode = query.tableMode === 'unique' ? 'table' : 'intersection';
  } else if (type === 'bar') {
    fields = exp ? [exp] : [];
    overrides = {
      splitField: subFields[0] || '',
      splitLength: SPLIT_LENGTHS.includes(int(query.splitLength)) ? int(query.splitLength) : 5,
      barMode: query.barMode === 'grouped' ? 'grouped' : 'stacked',
      orientation: query.barOrientation === 'horizontal' ? 'horizontal' : 'vertical'
    };
  } else if (type === 'tagcloud') {
    // a tag cloud draws one field
    fields = exp ? [exp] : [];
  } else if (type === 'waffle') {
    // a waffle draws one field, with or without its "Other"; with a second
    // field it is split: one waffle of the second field per value of the
    // first, as many as SPI Graph's pie count
    overrides = { other: query.other === 'true' || query.other === true };
    if (exp && subFields.length) {
      fields = subFields.slice(0, 1);
      overrides.splitField = exp;
      const pieCount = parseInt(query.pieCount, 10);
      if (SPLIT_LENGTHS.includes(pieCount)) { overrides.splitLength = pieCount; }
    } else {
      fields = exp ? [exp] : [];
    }
  } else if (type === 'matrix' || type === 'mosaic') {
    // a matrix (or mosaic) is the first field by the second, the rest are dropped
    fields = exp ? [exp, ...subFields.slice(0, 1)] : [];
  } else if (type === 'pie') {
    overrides = { other: query.other === 'true' || query.other === true };
    if (query.pieMode === 'split' && subFields.length) {
      // one pie per value of the first field, each of the fields after it
      fields = subFields;
      overrides.splitField = exp;
      // how many pies: SPI Graph's pieCount is the widget's splitLength
      const pieCount = parseInt(query.pieCount, 10);
      if (SPLIT_LENGTHS.includes(pieCount)) { overrides.splitLength = pieCount; }
    }
  } else if (type === 'connections' || type === 'tree' || type === 'circle') {
    // the connections url carries database field names
    const byDb = (db) => ((db === 'ip.dst:port' || db === 'destination.ip:port') ? 'ip.dst:port' : (fieldsArr || []).find(f => f.dbField === db)?.exp || '');
    fields = [byDb(str(query.srcField) || 'source.ip'), byDb(str(query.dstField) || 'destination.ip')];
    if (type === 'tree') {
      // the tree's root and depth are url params of its own
      overrides = { treeRoot: str(query.treeRoot), treeDepth: int(query.treeDepth) };
    } else if (type === 'circle') {
      overrides = { colorField: str(query.colorField) ? byDb(str(query.colorField)) : '', maxNodes: int(query.maxNodes) };
    }
  } else if (type === 'heatmap') {
    // the intensity metric is a <dbField>Histo key
    const metric = histoMetric(query.graphType);
    if (metric) { overrides.metricType = metric; }
    if (metric && percentile) { overrides.metricAgg = percentile; }
  } else if (type === 'combo' || type === 'scatter') {
    // one field; its metrics are <dbField>Histo keys: the ranking one
    // (graphType) is the bars or x, then the line, or the y and the size
    fields = exp ? [exp] : [];
    const first = histoMetric(query.graphType) || 'sessions';
    const second = histoMetric(str(query[type === 'combo' ? 'lineMetric' : 'yMetric']) || DEFAULT_SPIGRAPH_METRIC2) || 'sessions';
    const metrics = [first, second];
    if (type === 'scatter') { metrics.push(str(query.sizeMetric) ? (histoMetric(query.sizeMetric) || 'sessions') : ''); }
    overrides = { metrics, metricType: first, sortMetric: first, metricAgg: aggOf(viewMode, metrics) };
    if (type === 'scatter' && query.yScale === 'log') { overrides.yScale = 'log'; }
  }
  if (!fields.length || !fields[0]) { return null; }
  const mode = overrides.viewMode || viewMode;
  if (mode === 'table') {
    // a table's metric columns and the one it ranks by are field exps
    const metrics = str(query.metrics).split(',').filter(Boolean).slice(0, 4);
    const sortMetric = str(query.sortMetric) || histoMetric(query.graphType) || 'sessions';
    overrides = { ...overrides, metrics: metrics.length ? metrics : (sortMetric !== 'sessions' ? [sortMetric] : []), sortMetric, metricType: sortMetric };
    overrides.metricAgg = aggOf(mode, [sortMetric, ...overrides.metrics]);
  } else if (['bar', 'pie', 'tagcloud', 'waffle', 'treemap'].includes(mode) && !graphIsHierarchyFed({ viewMode: mode, fields, splitField: overrides.splitField })) {
    // the metric the values are sized by, a <dbField>Histo key like the heatmap's
    const metric = histoMetric(query.graphType) || 'sessions';
    overrides = { ...overrides, metricType: metric, metricAgg: aggOf(mode, [metric]) };
  }
  // connections' limit is its session sample size, a url param of its own
  const limit = int((type === 'connections' || type === 'tree' || type === 'circle') ? query.connectionsLength : query.size);
  return graphFromWidget({
    field: fields[0],
    fields,
    viewMode,
    length: limit > 0 ? limit : defaultLength(viewMode),
    expression: str(expression),
    view: str(query.view),
    ...shared(mode),
    ...overrides
  });
};

// the <dbField>Histo key SPI Graph ranks by (and a heatmap colors by)
const histoKey = (metricType) => {
  const db = metricType && metricType !== 'sessions' ? FieldService.getField(metricType, true)?.dbField : undefined;
  return db ? `${db}Histo` : 'sessionsHisto';
};

/**
 * The SPI Graph url query that shows a saved graph (no time: the page keeps
 * its own). Null for a graph SPI Graph can't draw.
 */
export const spigraphQueryFromGraph = (graph) => {
  const g = normalizeWidget(graph || {});
  const mode = g.viewMode;
  // the metric the values are ranked and sized by; the hierarchy endpoint's
  // graphs only count sessions
  const histo = graphIsHierarchyFed(g) ? 'sessionsHisto' : histoKey(g.metricType);
  const graphType = histo !== 'sessionsHisto' ? histo : undefined;
  // SPI Graph draws a timeline's or heatmap's percentile of its metric too
  const metricAgg = (g.viewMode === 'timeline' || g.viewMode === 'heatmap') && isPercentileAgg(g.metricAgg) && histo !== 'sessionsHisto' ? g.metricAgg : undefined;
  // any other aggregation of a metric, for the types that take one
  const aggOf = (metrics) => (g.metricAgg !== 'sum' && !graphIsHierarchyFed(g) && metrics.some(m => m && m !== 'sessions') && metricAggsFor(mode).includes(g.metricAgg) ? g.metricAgg : undefined);
  // the settings every type keeps in the url the same way
  const shared = {
    order: hasOrder(mode) && g.order === 'asc' ? 'asc' : undefined,
    orderBy: hasOrder(mode) && g.orderBy === 'key' ? 'key' : undefined,
    bucketInterval: allowsBucketInterval(mode) && g.bucketInterval > 0 ? g.bucketInterval : undefined,
    warn: hasLevels(mode) && g.gaugeWarn > 0 ? g.gaugeWarn : undefined,
    crit: hasLevels(mode) && g.gaugeCrit > 0 ? g.gaugeCrit : undefined
  };
  if (SPLIT_GRAPH_VIEW_MODES.includes(g.viewMode)) {
    // SPI Graph's Timeline (or Time of day) of the split field; its smallest
    // Max Elements is 5, so a 3 series split opens with 5. A widget's split
    // timeline stacks its series, so it opens SPI Graph's stacked layout
    // The metric is always named (as for the heatmap): left out, SPI Graph
    // would draw whichever one it showed last
    if (!g.splitField) { return null; }
    const q = { ...shared, exp: g.splitField, size: Math.max(5, g.splitLength), view: g.view || undefined, graphType: histo, spiGraphType: g.viewMode === 'timeline' ? 'default' : g.viewMode };
    // the timeline's bars or lines are always named too, the page would
    // otherwise draw whichever the user last picked
    if (g.viewMode === 'timeline') {
      q.timelineMode = g.yScale === 'percent' && !metricAgg ? 'percent' : g.barMode === 'grouped' ? 'overlaid' : 'stacked';
      q.seriesType = g.barMode === 'grouped' ? 'lines' : g.seriesType;
    }
    if (metricAgg) { q.metricAgg = metricAgg; }
    // an area is stacked unless it is a percent or an overlaid one
    if (g.viewMode === 'area') { q.timelineMode = g.yScale === 'percent' ? 'percent' : g.barMode === 'grouped' ? 'overlaid' : undefined; }
    if (g.viewMode !== 'timeofday' && g.yScale === 'log') { q.yScale = 'log'; }
    return q;
  }
  const fields = g.fields.length ? g.fields : (g.field ? [g.field] : []);
  if (!fields.length) { return null; }
  const base = { ...shared, exp: fields[0], size: g.length, view: g.view || undefined, graphType, metricAgg: aggOf([g.metricType]) };
  switch (g.viewMode) {
  case 'bar':
    return { ...base, spiGraphType: 'bar', subFields: g.splitField || undefined, splitLength: g.splitField && g.splitLength !== 5 ? g.splitLength : undefined, barMode: g.barMode, barOrientation: g.orientation === 'horizontal' ? 'horizontal' : undefined };
  case 'pie':
    // a split pie is SPI Graph's split pie mode: its first field (the split)
    // draws one pie per value of the fields after it
    if (g.splitField) {
      return { ...base, exp: g.splitField, spiGraphType: 'pie', subFields: fields.join(','), pieMode: 'split', pieCount: g.splitLength, other: g.other ? 'true' : undefined };
    }
    return { ...base, spiGraphType: 'pie', subFields: fields.slice(1).join(',') || undefined, other: g.other ? 'true' : undefined };
  case 'tagcloud':
    return { ...base, spiGraphType: 'tagcloud' };
  case 'waffle':
    // a split waffle's split field is SPI Graph's first field
    if (g.splitField) {
      return { ...base, exp: g.splitField, spiGraphType: 'waffle', subFields: fields[0], pieCount: g.splitLength, other: g.other ? 'true' : undefined };
    }
    return { ...base, spiGraphType: 'waffle', other: g.other ? 'true' : undefined };
  case 'table':
    // each field's own top values are SPI Graph's unique table; its metric
    // columns and the one it ranks by are field exps
    return {
      ...base,
      spiGraphType: 'table',
      subFields: fields.slice(1).join(',') || undefined,
      tableMode: fields.length > 1 ? 'unique' : undefined,
      // left out when they are just the one it ranks by
      metrics: g.metrics.join(',') !== (g.sortMetric !== 'sessions' ? g.sortMetric : '') ? (g.metrics.join(',') || undefined) : undefined,
      sortMetric: g.sortMetric !== 'sessions' ? g.sortMetric : undefined,
      graphType: histoKey(g.sortMetric) !== 'sessionsHisto' ? histoKey(g.sortMetric) : undefined,
      metricAgg: aggOf([g.sortMetric, ...g.metrics])
    };
  case 'intersection':
    return { ...base, spiGraphType: 'table', subFields: fields.slice(1).join(',') || undefined };
  case 'heatmap':
    // always named, so the page doesn't keep its last intensity
    return { ...base, spiGraphType: 'heatmap', graphType: histo, metricAgg };
  case 'treemap':
  case 'sankey':
    return { ...base, spiGraphType: g.viewMode, subFields: fields.slice(1).join(',') || undefined };
  case 'matrix':
  case 'mosaic':
    return { ...base, spiGraphType: g.viewMode, subFields: fields[1] || undefined };
  case 'combo':
  case 'scatter': {
    // every metric is named, as for the heatmap; SPI Graph draws sums
    const [first, second, size] = slotMetrics(g);
    const q = { ...base, spiGraphType: g.viewMode, graphType: histoKey(first), metricAgg: aggOf([first, second, size]) };
    if (g.viewMode === 'combo') {
      q.lineMetric = histoKey(second);
    } else {
      q.yMetric = histoKey(second);
      q.sizeMetric = size ? histoKey(size) : undefined;
      q.yScale = g.yScale === 'log' ? 'log' : undefined;
    }
    return q;
  }
  case 'connections':
  case 'tree':
  case 'circle': {
    const toDb = (e) => ((e === 'ip.dst:port' || e === 'destination.ip:port') ? e : FieldService.getField(e, true)?.dbField);
    const q = { spiGraphType: g.viewMode, srcField: toDb(fields[0]), dstField: toDb(fields[1]), connectionsLength: g.length, view: base.view };
    // the tree's root and depth are url params of its own
    if (g.viewMode === 'tree') { q.treeRoot = g.treeRoot || undefined; q.treeDepth = g.treeDepth; }
    if (g.viewMode === 'circle') {
      q.colorField = g.colorField ? toDb(g.colorField) : undefined;
      q.maxNodes = g.maxNodes;
    }
    return q;
  }
  default:
    return null;
  }
};

/** True when SPI Graph shows two graphs the same way (their urls and searches match). */
export const sameSpigraphGraph = (a, b) => {
  const shown = (g) => JSON.stringify([spigraphQueryFromGraph(g), normalizeWidget(g || {}).expression]);
  return shown(a) === shown(b);
};

// what the url can't hold exactly: a timeline of 3 series opens with 5 (Max
// Elements' smallest), one of 20 reads back as 10, and its limit isn't shown
const LOSSY_KEYS = ['splitLength', 'length'];

/**
 * The graph SPI Graph shows, as saved-graph data: what the page shows on top of
 * the saved graph it loaded, if any. The saved graph keeps compare, which no
 * graph SPI Graph draws can have, and a value the url can't hold exactly while
 * the page still shows the graph the same with it.
 * @param {object|null} saved - the loaded saved graph's data
 * @param {object|null} page - graphFromSpigraphQuery for the page (null: no graph)
 */
export const graphOnSpigraph = (saved, page) => {
  if (!page) { return null; }
  if (!saved) { return page; }
  const kept = graphFromWidget(saved);
  // an unchanged page is the saved graph
  if (sameSpigraphGraph(kept, page)) { return kept; }
  let merged = graphFromWidget({ ...saved, ...page, compare: saved.compare });
  for (const key of LOSSY_KEYS) {
    const withSaved = { ...merged, [key]: kept[key] };
    if (kept[key] !== merged[key] && sameSpigraphGraph(withSaved, merged)) { merged = withSaved; }
  }
  return merged;
};

// map v7-only field view modes to the closest mode a v6 viewer can render
// (v6 knows only bar/pie/table) for the dual-written legacy fields[] shape
const V6_VIEW_MODES = { bar: 'bar', pie: 'pie', table: 'table', intersection: 'table', heatmap: 'bar', treemap: 'bar', sankey: 'table', connections: 'table', tagcloud: 'bar', matrix: 'table', combo: 'bar', scatter: 'table' };

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
  // a control picks values rather than charting them, v6 has nothing like it
  const fieldWidgets = (widgets || []).filter(w => w.field && isFieldMode(w.viewMode) && w.viewMode !== 'control');

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
