/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Single source of truth for dashboard widget visualization-type metadata, shared
by Arkime.vue (page), Summary.vue (host) and SummaryWidgetEditModal.vue. Each
view mode advertises its capabilities (field-bound? metric? top-N? how it's fed)
so the edit modal can gray out inapplicable inputs and the host can dispatch.
*/

// Default chart view mode per field expression (mirrors the server's fieldMetadata)
export const DEFAULT_VIEW_MODES = {
  ip: 'bar',
  'ip.src': 'bar',
  'ip.dst': 'bar',
  'port.src': 'bar',
  'port.dst': 'bar',
  protocols: 'pie',
  tags: 'pie',
  'ip.dst:port': 'table',
  'host.http': 'table',
  'dns.query.host': 'table'
};

// Field-bound types: aggregate one field's top-N values (require a field).
export const FIELD_VIEW_MODES = ['bar', 'pie', 'table', 'heatmap', 'treemap', 'sankey', 'connections', 'intersection'];

// Session-wide types: describe the whole result set (no field, fed by the host's
// global stats chunk). (map is field-bound — it plots a chosen geo field.)
export const SESSION_VIEW_MODES = ['timeline', 'stats', 'time'];

// Field-bound to a geo field only (country/*.geo): the map choropleth.
export const GEO_FIELD_VIEW_MODES = ['map'];

// Types that visualize a single selectable metric (Sessions or a numeric field).
// timeline plots the metric's <dbField>Histo series over time.
export const METRIC_VIEW_MODES = ['bar', 'pie', 'table', 'heatmap', 'treemap', 'timeline'];

// Types that honor a result limit. bar/pie/table/heatmap/treemap/sankey/
// intersection cap their top-N values; connections uses it as a session sample
// size (see SAMPLE_SIZE_VIEW_MODES).
export const LENGTH_VIEW_MODES = ['bar', 'pie', 'table', 'heatmap', 'treemap', 'sankey', 'connections', 'intersection'];

// Types that also honor an order (direction). Only the /api/sessions/summary
// paths pass one through; /api/spigraph, /api/spigraphhierarchy and
// /api/connections have no order parameter, so offering the control for those
// modes would silently do nothing.
export const ORDER_VIEW_MODES = ['bar', 'pie', 'table'];

// Types whose limit is a session sample size rather than a top-N cut, so they
// get their own (much larger) scale.
export const SAMPLE_SIZE_VIEW_MODES = ['connections'];

// Types whose fetch resolves fields through the server's dbFieldsMap, which
// Config.loadFields builds while skipping noFacet fields (All IP Fields, Arkime
// ID, Payload Src/Dst UTF8, View Name) — so those can't be resolved however
// they are spelled. Every other mode resolves through fieldsMap, which keeps
// them: All IP Fields drives a working bar/pie/table widget, because
// /api/sessions/summary aggregates it with a script instead of a db field.
export const DB_FIELD_VIEW_MODES = ['connections'];

// Types whose fields are an ordered source -> destination pair rather than an
// unordered set, so the editor gives each end its own picker over its own list.
// This is a different axis from FIELD_COUNT_LIMITS: that says how many fields a
// mode takes, this says the positions mean different things.
export const FIELD_PAIR_VIEW_MODES = ['connections'];

// Types rendered from the batched /api/sessions/summary stream (vs. self-fetch).
export const STREAM_VIEW_MODES = ['bar', 'pie'];

// Types that fetch their own endpoint (spigraph / spigraphhierarchy / summary).
// table self-fetches (one summary sub-widget per field) so it can carry multiple
// fields and multiple metric columns.
export const SELF_FETCH_VIEW_MODES = ['heatmap', 'treemap', 'sankey', 'connections', 'intersection', 'map', 'table'];

// [min, max] fields each view mode accepts — nested combinations (pie/treemap/
// sankey/intersection via spigraphhierarchy), side-by-side columns (table via
// summary), or a source + destination pair (connections). bar is
// single-dimension; heatmap has no combination-over-time data path. Modes not
// listed take a single field (or none, for the session-wide ones).
export const FIELD_COUNT_LIMITS = {
  pie: [1, 3],
  treemap: [1, 3],
  sankey: [1, 3],
  table: [1, 3],
  intersection: [1, 3],
  connections: [2, 2]
};

// Types that accept multiple metric columns (the table's [value | m0 | m1 …]).
// Charts visualize a single metric, so they stay single-select.
export const MULTI_METRIC_VIEW_MODES = ['table'];

/** True for stream-mode (bar/pie/table) widgets fed by the summary stream. */
export const isStreamMode = (viewMode) => STREAM_VIEW_MODES.includes(viewMode);

/** True when the widget aggregates a chosen field (needs a field selection). */
export const isFieldMode = (viewMode) => FIELD_VIEW_MODES.includes(viewMode);

/** True for session-wide widgets (timeline/stats/time) — no field, host-fed. */
export const isSessionMode = (viewMode) => SESSION_VIEW_MODES.includes(viewMode);

/** True when the widget aggregates a chosen geo field (the map choropleth). */
export const isGeoFieldMode = (viewMode) => GEO_FIELD_VIEW_MODES.includes(viewMode);

/** True when the widget exposes a metric selector. */
export const hasMetric = (viewMode) => METRIC_VIEW_MODES.includes(viewMode);

/** True when the widget exposes a result limit. */
export const hasLength = (viewMode) => LENGTH_VIEW_MODES.includes(viewMode);

/** True when the widget exposes an order (direction) that its fetch honors. */
export const hasOrder = (viewMode) => ORDER_VIEW_MODES.includes(viewMode);

/** True when the widget's limit is a session sample size, not a top-N cut. */
export const isSampleSize = (viewMode) => SAMPLE_SIZE_VIEW_MODES.includes(viewMode);

/** The limit options a view mode offers (sample sizes mirror the SPI Graph
 *  connections page, whose smallest sample is the top-N scale's largest). */
export const lengthOptions = (viewMode) => isSampleSize(viewMode)
  ? [100, 500, 1000, 5000, 10000, 50000, 100000]
  : [10, 20, 50, 100];

/** The default limit for a view mode (matches the SPI Graph page for samples). */
export const defaultLength = (viewMode) => isSampleSize(viewMode) ? 100 : 20;

/** [min, max] fields a view mode accepts (single field unless listed). */
export const fieldCountLimits = (viewMode) => FIELD_COUNT_LIMITS[viewMode] ?? [1, 1];

/** True when the widget accepts more than one field (chips multi-select). */
export const allowsMultiField = (viewMode) => fieldCountLimits(viewMode)[1] > 1;

/** True when the widget's fields are an ordered source -> destination pair. */
export const isFieldPair = (viewMode) => FIELD_PAIR_VIEW_MODES.includes(viewMode);

/** True when a field can back a widget of this view mode at this position
 *  ('src' | 'dst'). Only the modes resolving through dbFieldsMap need narrowing.
 *  /api/connections rewrites ip.dst:port to destination.ip before its lookup,
 *  but only for dstField — as a source it resolves to nothing, so the pair's
 *  two ends get different lists. */
export const fieldUsableBy = (viewMode, field, position) => {
  if (!DB_FIELD_VIEW_MODES.includes(viewMode)) { return true; }
  if (field?.exp === 'ip.dst:port' || field?.exp === 'destination.ip:port') {
    return position === 'dst';
  }
  return !field?.noFacet;
};

/** True when the widget accepts multiple metric columns (chips multi-select). */
export const allowsMultiMetric = (viewMode) => MULTI_METRIC_VIEW_MODES.includes(viewMode);

/** True when the widget fetches its own data and can take a per-widget local
 *  filter (a saved View + expression). Only the global capture-stats widgets
 *  (stats/time), which describe the whole result set, can't. */
export const hasLocalFilter = (viewMode) => !['stats', 'time'].includes(viewMode);
