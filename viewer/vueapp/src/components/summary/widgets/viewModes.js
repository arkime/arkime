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
export const FIELD_VIEW_MODES = ['bar', 'pie', 'table', 'heatmap', 'treemap', 'intersection'];

// Session-wide types: describe the whole result set (no field, fed by the host's
// global stats chunk). (map is field-bound — it plots a chosen geo field.)
export const SESSION_VIEW_MODES = ['timeline', 'stats', 'time'];

// Field-bound to a geo field only (country/*.geo): the map choropleth.
export const GEO_FIELD_VIEW_MODES = ['map'];

// Types that visualize a single selectable metric (Sessions or a numeric field).
// timeline plots the metric's <dbField>Histo series over time.
export const METRIC_VIEW_MODES = ['bar', 'pie', 'table', 'heatmap', 'treemap', 'timeline'];

// Types that honor a Top/Bottom N (length) + order (direction).
export const AGG_VIEW_MODES = ['bar', 'pie', 'table', 'heatmap', 'treemap', 'intersection'];

// Types rendered from the batched /api/sessions/summary stream (vs. self-fetch).
export const STREAM_VIEW_MODES = ['bar', 'pie'];

// Types that fetch their own endpoint (spigraph / spigraphhierarchy / summary).
// table self-fetches (one summary sub-widget per field) so it can carry multiple
// fields and multiple metric columns.
export const SELF_FETCH_VIEW_MODES = ['heatmap', 'treemap', 'intersection', 'map', 'table'];

// Types that accept multiple fields (up to 3) — nested combinations (pie/treemap/
// intersection via spigraphhierarchy) or side-by-side columns (table via summary).
// bar is single-dimension; heatmap has no combination-over-time data path.
export const MULTI_FIELD_VIEW_MODES = ['pie', 'treemap', 'table', 'intersection'];

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

/** True when the widget exposes Top/Bottom N (length) + order (direction). */
export const hasAgg = (viewMode) => AGG_VIEW_MODES.includes(viewMode);

/** True when the widget accepts up to 3 fields (chips multi-select). */
export const allowsMultiField = (viewMode) => MULTI_FIELD_VIEW_MODES.includes(viewMode);

/** True when the widget accepts multiple metric columns (chips multi-select). */
export const allowsMultiMetric = (viewMode) => MULTI_METRIC_VIEW_MODES.includes(viewMode);

/** True when the widget fetches its own data and can take a per-widget local
 *  filter (a saved View + expression). Only the global capture-stats widgets
 *  (stats/time), which describe the whole result set, can't. */
export const hasLocalFilter = (viewMode) => !['stats', 'time'].includes(viewMode);
