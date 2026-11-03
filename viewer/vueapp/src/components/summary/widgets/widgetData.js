/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Shared helpers for field widgets whose view mode self-fetches (e.g. heatmap,
which needs per-value time series from /api/spigraph rather than the batched
summary stream). Combines the global search expression with the widget's local
filter and the global time window.
*/
import { fetchWrapper } from '@common/fetchWrapper.js';
import { commaString, humanReadableBytes } from '@common/vueFilters.js';
import FieldService from '../../search/FieldService';
import { defaultLength, fieldCountLimits, slotMetrics, isPercentileAgg, PERCENTILE_VIEW_MODES } from './viewModes';
import { spigraphQueryFromGraph } from '../dashboardConfig';
import { controlFilters, substituteExpression, withoutPicks } from './dashboardVariables';

/**
 * A widget's field list, capped at what its view mode takes (see
 * FIELD_COUNT_LIMITS). Multi-field widgets store `fields`; single-field widgets
 * store `field` (= fields[0]). Always returns an array.
 */
export function widgetFields (widget) {
  if (Array.isArray(widget?.fields) && widget.fields.length) { return widget.fields.slice(0, fieldCountLimits(widget.viewMode)[1]); }
  return widget?.field ? [widget.field] : [];
}

/** The percentile a SPIGraph-backed widget draws of its metric (p95, ...),
 *  undefined for its sum (and for the types that only sum). */
export function widgetPercentile (widget) {
  return PERCENTILE_VIEW_MODES.includes(widget?.viewMode) && isPercentileAgg(widget?.metricAgg) ? widget.metricAgg : undefined;
}

/**
 * The graph/histo key a SPIGraph-backed widget visualizes for its metric:
 * 'sessionsHisto' (session count) by default, else the chosen numeric field's
 * `<dbField>Histo` series (which the backend emits when sent `metric`), or
 * its `<dbField>:p<N>Histo` series for a percentile (sent `metricAgg` too).
 */
export function metricHistoKey (widget) {
  const metric = widget?.metricType;
  if (!metric || metric === 'sessions') { return 'sessionsHisto'; }
  const dbField = FieldService.getField(metric, true)?.dbField;
  if (!dbField) { return 'sessionsHisto'; }
  const pct = widgetPercentile(widget);
  return pct ? `${dbField}:${pct}Histo` : `${dbField}Histo`;
}

// a SPIGraph request's metric, and its percentile when the widget draws one
function spigraphMetric (widget, extra) {
  extra.metric = widget.metricType;
  const pct = widgetPercentile(widget);
  if (pct) { extra.metricAgg = pct; }
}

/** True when a widget's metric is a byte-valued field (wants human-readable
 *  formatting). A unique count of a bytes field is a count, not bytes. */
export function metricIsBytes (metricType, metricAgg) {
  if (!metricType || metricType === 'sessions' || metricAgg === 'cardinality') { return false; }
  return /bytes/i.test(metricType) || /bytes/i.test(FieldService.getField(metricType, true)?.dbField || '');
}

/** Format a widget metric value: human-readable bytes for byte metrics, else a
 *  comma count (averages keep up to two decimals). */
export function formatMetricValue (metricType, v, metricAgg) {
  if (metricIsBytes(metricType, metricAgg)) { return humanReadableBytes(v || 0); }
  if ((metricAgg === 'avg' || isPercentileAgg(metricAgg)) && Number.isFinite(v) && !Number.isInteger(v)) {
    return commaString(Math.round(v * 100) / 100);
  }
  return commaString(v || 0);
}

/** The i18n key for a metric aggregation's label (sessions.summary.agg.<agg>). */
export function metricAggKey (metricAgg) {
  return `sessions.summary.agg.${metricAgg || 'sum'}`;
}

/** A metric's display label: "Sessions", the field's name for a sum, else
 *  "<Agg> <Field>" (eg "Max Bytes"). One place, so every widget agrees. */
export function metricLabel (metricType, metricAgg, t) {
  if (!metricType || metricType === 'sessions') { return t('sessions.summary.sessions'); }
  const fieldName = FieldService.getField(metricType, true)?.friendlyName || metricType;
  return metricAgg && metricAgg !== 'sum' ? `${t(metricAggKey(metricAgg))} ${fieldName}` : fieldName;
}

/** True when a metric adds up: the session count or a sum. Averages, extremes
 *  and unique counts can't be stacked or totalled. */
export function metricIsAdditive (metricType, metricAgg) {
  return !metricType || metricType === 'sessions' || !metricAgg || metricAgg === 'sum';
}

/** True when a field can back a terms aggregation: not a noFacet field, not an
 *  "All X" group (their dbField isn't a real column) and not a text field. */
export function fieldIsAggregatable (field) {
  return !!field && !field.noFacet && !field.regex && !/textfield/.test(field.type || '');
}

// Neutralize CSV/spreadsheet formula injection (a cell starting with = + - @ TAB CR
// can execute in Excel/Sheets), then quote cells containing comma/quote/newline.
function escapeCSV (value) {
  let s = String(value ?? '');
  if (s.length > 0 && '=+-@\t\r'.includes(s[0])) { s = `'${s}`; }
  // ; too, since some locales' spreadsheets split on it
  if (/[",;\r\n]/.test(s)) { return `"${s.replace(/"/g, '""')}"`; }
  return s;
}

/** Build a CSV from header labels + row arrays and trigger a browser download. */
export function downloadCSV (headers, rows, filename) {
  const lines = [headers.map(escapeCSV).join(',')];
  for (const row of rows) { lines.push(row.map(escapeCSV).join(',')); }
  const blob = new Blob([lines.join('\n') + '\n'], { type: 'text/csv' });
  const url = window.URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  a.click();
  window.URL.revokeObjectURL(url);
}

/**
 * A widget's local filter = its saved View's expression AND its own expression
 * (either may be unset), with the dashboard's ${variables} filled in. The View
 * is resolved from store.state.views by id/name.
 * @returns {string|undefined} the combined local expression (or undefined)
 */
export function widgetLocalExpression (widget, views, vars) {
  const parts = [];
  if (widget?.view) {
    const v = (views || []).find(x => x.id === widget.view || x.name === widget.view);
    if (v?.expression) { parts.push(v.expression); }
  }
  const own = substituteExpression(widget?.expression, vars);
  if (own) { parts.push(own); }
  if (!parts.length) { return undefined; }
  return parts.length === 1 ? parts[0] : parts.map(p => `(${p})`).join(' && ');
}

/**
 * The page's search: the global search expression AND the dashboard's own
 * filter (store.state.dashboardExpression, the config's data.expression) AND
 * the picks of the controls that filter every widget, so every widget on the
 * page, the capture stats included, sees all of them. `excludeVariable`
 * leaves one control's own pick out (its value list): its filter, and its
 * ${name} as if nothing were picked.
 * @returns {string|undefined} the combined expression (or undefined if none)
 */
export function pageExpression (route, store, excludeVariable) {
  const vars = withoutPicks(store.state.dashboardVariables, excludeVariable);
  const parts = [
    route.query.expression,
    substituteExpression(store.state.dashboardExpression, vars),
    ...controlFilters(vars, excludeVariable)
  ].filter(Boolean);
  return parts.length > 1 ? parts.map(p => `(${p})`).join(' && ') : parts[0];
}

/**
 * Combine the page's search (global + dashboard filter) with a widget's local
 * filter (View + expression).
 * @returns {string|undefined} the combined expression (or undefined if neither)
 */
export function combinedExpression (route, store, widget) {
  const pageExp = pageExpression(route, store);
  const localExp = widgetLocalExpression(widget, store.state.views, store.state.dashboardVariables);
  if (pageExp && localExp) { return `(${pageExp}) && (${localExp})`; }
  return localExp || pageExp || undefined;
}

/**
 * The full page view of a widget: the SPI Graph type that draws the same
 * thing, or the Sessions page for the session-wide widgets, with the widget's
 * fields, limit and combined search carried over in the url. Null for the
 * widgets with no full page equivalent (map, stats, time, text).
 * @returns {object|null} a vue-router location ({ path, query })
 */
export function widgetFullPageLink (route, store, widget) {
  const mode = widget?.viewMode;
  const fields = widgetFields(widget);
  // SPI Graph aggregates a real column, so the "All X fields" groups (All IP
  // fields and friends, which the summary endpoint handles with a script)
  // have no full page; the Sessions page targets don't aggregate at all
  const usable = (fieldExp) => {
    if (fieldExp === 'ip.dst:port' || fieldExp === 'destination.ip:port') { return true; }
    const field = FieldService.getField(fieldExp, true);
    return !!field && !field.noFacet && !field.regex;
  };
  // a split timeline is SPI Graph's Timeline: one histogram per top value of
  // the split field (unsplit, it is the Sessions page's own timeline); a split
  // time of day grid is SPI Graph's Time of day the same way (unsplit, it has
  // no full page)
  const splitTimeline = (mode === 'timeline' || mode === 'area' || mode === 'timeofday') && !!widget.splitField && usable(widget.splitField);
  if (mode === 'timeofday' && !splitTimeline) { return null; }
  if (!['timeline', 'area', 'timeofday', 'metric', 'gauge', 'sessions'].includes(mode)) {
    // a bar's or pie's split field becomes another SPI Graph field
    const linked = ((mode === 'bar' || mode === 'pie') && widget.splitField) ? [...fields, widget.splitField] : fields;
    if (!fields.length || !linked.every(usable)) { return null; }
  }
  const query = {};
  const exp = combinedExpression(route, store, widget);
  if (exp) { query.expression = exp; }
  for (const p of ['view', 'bounding', 'interval', 'cluster']) {
    if (route.query[p]) { query[p] = route.query[p]; }
  }
  if (route.query.spanning === 'true') { query.spanning = 'true'; }
  // a summed metric ranks SPI Graph and draws the Sessions timeline the same
  // way; averages, extremes and unique counts have no full page equivalent
  const histo = metricHistoKey(widget);
  if (histo !== 'sessionsHisto' && (widget.metricAgg || 'sum') === 'sum') { query.graphType = histo; }
  const timeRange = parseInt(store.state.timeRange, 10);
  if (timeRange === 0 && store.state.time?.startTime) {
    query.startTime = store.state.time.startTime;
    query.stopTime = store.state.time.stopTime;
  } else {
    query.date = store.state.timeRange;
  }

  if (((mode === 'timeline' || mode === 'area') && !splitTimeline) || mode === 'metric' || mode === 'gauge' || mode === 'sessions') { return { path: '/sessions', query }; }
  // the same SPI Graph url a saved graph opens with; the widget's own view is
  // already in the combined expression, so the page keeps the global one
  const graphQuery = spigraphQueryFromGraph(widget);
  if (!graphQuery) { return null; }
  return { path: '/spigraph', query: { ...query, ...graphQuery, view: query.view } };
}

// a widget's warning and critical levels (the gauge's thresholds): the
// colors a metric or table cell turns at or above them, and the reference
// lines a bar or timeline draws at them
export const WARN_COLOR = '#fb8c00';
export const CRIT_COLOR = '#e53935';
/** [{ value, color }] for the widget's warning and critical levels that are set. */
export function widgetThresholds (widget) {
  return [[widget?.gaugeWarn, WARN_COLOR], [widget?.gaugeCrit, CRIT_COLOR]]
    .filter(([v]) => v > 0)
    .map(([value, color]) => ({ value, color }));
}
/** The color a value takes from the widget's levels, or '' below them. */
export function thresholdColor (widget, value) {
  if (widget?.gaugeCrit > 0 && value >= widget.gaugeCrit) { return CRIT_COLOR; }
  if (widget?.gaugeWarn > 0 && value >= widget.gaugeWarn) { return WARN_COLOR; }
  return '';
}

// how far back a widget's comparison looks (its compare setting), in seconds
export const COMPARE_OFFSETS = { day: 86400, week: 604800 };

/**
 * The seconds a widget's comparison reaches back (the same window a day or a
 * week earlier), 0 for none: no compare setting, or an all time search, which
 * has no earlier window.
 */
export function compareOffset (widget, store) {
  const offset = Object.hasOwn(COMPARE_OFFSETS, widget?.compare || '') ? COMPARE_OFFSETS[widget.compare] : 0;
  if (!offset || parseInt(store.state.timeRange, 10) === -1) { return 0; }
  return store.state.time?.startTime && store.state.time?.stopTime ? offset : 0;
}

// a request's time window moved back by `shift` seconds (a comparison)
function shiftWindow (params, store, shift) {
  if (!shift) { return params; }
  delete params.date;
  params.startTime = Number(store.state.time.startTime) - shift;
  params.stopTime = Number(store.state.time.stopTime) - shift;
  return params;
}

/**
 * Build the shared query params (combined expression + time window + view /
 * bounding / cluster) for a widget's independent fetch, plus endpoint extras.
 */
export function buildWidgetParams (route, store, widget, extra = {}) {
  const params = { ...extra };

  const exp = combinedExpression(route, store, widget);
  if (exp) { params.expression = exp; }

  for (const p of ['view', 'bounding', 'interval', 'cluster']) {
    if (route.query[p]) { params[p] = route.query[p]; }
  }
  if (route.query.spanning === 'true') { params.spanning = true; }

  if (parseInt(store.state.timeRange, 10) === -1) {
    params.date = store.state.timeRange;
  } else {
    params.startTime = store.state.time.startTime;
    params.stopTime = store.state.time.stopTime;
  }

  return params;
}

/**
 * Fetch SPIGraph data for one field (top-N values, each with a per-value graph).
 * The endpoint forces facets, so the per-value time series is always returned;
 * items come back sorted by doc_count. Returns { items, graph, ... }.
 */
export async function fetchSpigraph (route, store, widget, { signal } = {}) {
  const extra = { exp: widget.field, size: widget.length };
  // a numeric metric drives heatmap intensity / treemap size; the server sums it
  // per value into a <dbField>Histo series and we order Top-N by it
  if (widget.metricType && widget.metricType !== 'sessions') {
    spigraphMetric(widget, extra);
    extra.sort = metricHistoKey(widget);
  }
  const data = buildWidgetParams(route, store, widget, extra);
  return await fetchWrapper({ url: 'api/spigraph', method: 'POST', data, signal });
}

/**
 * Fetch just the timeline graph for a query (the Timeline widget). Reuses the
 * spigraph endpoint, which resolves `metric` into a <dbField>Histo series and
 * honors the widget's local filter (combinedExpression); we read the top-level
 * `graph` and ignore the per-value items. `exp: node` + `size: 1` keeps the
 * terms aggregation minimal. Returns { graph, ... }.
 */
export async function fetchGraph (route, store, widget, { signal, timeShift } = {}) {
  const extra = { exp: 'node', size: 1 };
  if (widget.metricType && widget.metricType !== 'sessions') {
    spigraphMetric(widget, extra);
  }
  const data = shiftWindow(buildWidgetParams(route, store, widget, extra), store, timeShift);
  return await fetchWrapper({ url: 'api/spigraph', method: 'POST', data, signal });
}

/**
 * Fetch the per-value time series of a timeline's split field (the split
 * Timeline widget): the split field's top splitLength values, each with its
 * own graph of the widget's metric, plus the top-level graph for the time
 * bounds. Returns { items, graph, ... }.
 */
export async function fetchSplitGraph (route, store, widget, { signal } = {}) {
  const extra = { exp: widget.splitField, size: widget.splitLength || 5 };
  if (widget.metricType && widget.metricType !== 'sessions') {
    spigraphMetric(widget, extra);
    extra.sort = metricHistoKey(widget);
  }
  const data = buildWidgetParams(route, store, widget, extra);
  return await fetchWrapper({ url: 'api/spigraph', method: 'POST', data, signal });
}

/**
 * The IANA zone an hour of week request folds in, from the user's timezone
 * setting: 'gmt' is UTC, 'local' and 'localtz' are the browser's zone.
 * @param {string} setting - the user's timezone setting
 * @returns {string}
 */
export function requestTimezone (setting) {
  if (setting === 'gmt') { return 'UTC'; }
  return Intl.DateTimeFormat().resolvedOptions().timeZone || 'UTC';
}

/**
 * The week of hours a time of day grid draws: the timeline fetched with
 * interval=hourOfWeek, which has the viewer fold the hour buckets by weekday
 * and hour in the user's timezone (168 slots at most per series, whatever the
 * range) -- unsplit, the search's own graph; split, one per top value of the
 * split field. Each graph comes back with hourOfWeek: true and
 * [[slot, value], ...] series, slot 0 being Monday 00:00.
 */
export async function fetchTimeOfDay (route, store, widget, { signal } = {}) {
  const extra = widget.splitField
    ? { exp: widget.splitField, size: widget.splitLength || 5 }
    : { exp: 'node', size: 1 };
  if (widget.metricType && widget.metricType !== 'sessions') {
    extra.metric = widget.metricType;
    if (widget.splitField) { extra.sort = metricHistoKey(widget); }
  }
  const data = {
    ...buildWidgetParams(route, store, widget, extra),
    interval: 'hourOfWeek',
    timezone: requestTimezone(store.state.user?.settings?.timezone)
  };
  return await fetchWrapper({ url: 'api/spigraph', method: 'POST', data, signal });
}

/**
 * The fields a hierarchy widget nests, outermost first: a split pie's split
 * field (one small pie per value) wraps the pie's own fields.
 */
export function hierarchyFields (widget) {
  const fields = widgetFields(widget);
  return widget?.viewMode === 'pie' && widget.splitField ? [widget.splitField, ...fields] : fields;
}

/**
 * Fetch SPIGraph hierarchy data for a widget's nested fields (the
 * "Intersection" table, nested pie/treemap/sankey). For a single field the
 * endpoint returns `tableResults` as a flat list of { name, size, parents: [] }
 * — one row per unique value. Returns the raw response ({ tableResults,
 * hierarchicalResults }).
 */
export async function fetchHierarchy (route, store, widget, { signal } = {}) {
  const fields = hierarchyFields(widget);
  const extra = { exp: fields.join(','), size: widget.length };
  // a split pie draws splitLength pies, each cut to the widget's limit
  if (fields.length > widgetFields(widget).length) {
    extra.sizes = [widget.splitLength || 5, ...fields.slice(1).map(() => widget.length)].join(',');
  }
  // a matrix reads a row's "Other" to know which of its missing values are zero
  if ((widget.viewMode === 'pie' && widget.other) || widget.viewMode === 'matrix') { extra.other = true; }
  const data = buildWidgetParams(route, store, widget, extra);
  return await fetchWrapper({ url: 'api/spigraphhierarchy', method: 'POST', data, signal });
}

/**
 * Fetch connections data for a src → dst field pair (the Connections widget).
 * The endpoint samples `length` sessions and returns { nodes, links } — nodes
 * carry { id, sessions, type (1 src | 2 dst | 3 both), network.bytes/packets
 * sums }, links carry { value, source, target } with node array indices.
 */
export async function fetchConnections (route, store, widget, { signal } = {}) {
  const fields = widgetFields(widget);
  // the endpoint resolves srcField/dstField through Config.getDBField, which
  // reads dbFieldsMap — and Config.loadFields deliberately keeps noFacet fields
  // (All IP Fields, Arkime ID, Payload Src/Dst UTF8, View Name) out of that map,
  // so no spelling of them resolves. Return undefined for those rather than a
  // dbField the server will map to undefined and hand Elasticsearch as an empty
  // field, which comes back as an x_content_parse_exception. 'ip.dst:port' is
  // the one exp the endpoint matches literally, before any lookup
  const toDb = (exp) => {
    if (exp === 'ip.dst:port' || exp === 'destination.ip:port') { return exp; }
    const field = FieldService.getField(exp, true);
    if (!field || field.noFacet) { return undefined; }
    return field.dbField;
  };
  const [srcField, dstField] = [toDb(fields[0]), toDb(fields[1])];
  // the modal enforces the pair, but an imported/hand-edited dashboard can
  // still get here with only one — say so on the card rather than letting the
  // server silently default to source.ip/destination.ip
  if (!fields[0] || !fields[1]) {
    throw { i18n: 'sessions.summary.widget.exactFieldsRequired', i18nParams: { count: 2 } };
  }
  // both picked, but one has no database field the endpoint can read (eg a
  // noFacet field like All IP Fields, which Config.loadFields keeps out of
  // dbFieldsMap) — name it instead of failing inside Elasticsearch
  if (!srcField || !dstField) {
    const bad = !srcField ? fields[0] : fields[1];
    throw {
      i18n: 'sessions.summary.widget.fieldUnsupported',
      i18nParams: { field: FieldService.getField(bad, true)?.friendlyName || bad }
    };
  }
  const data = buildWidgetParams(route, store, widget, {
    srcField,
    dstField,
    length: widget.length || defaultLength(widget.viewMode)
  });
  return await fetchWrapper({ url: 'api/connections', method: 'POST', data, signal });
}

/**
 * Fetch each of a widget's fields' top-N (with the chosen metric) in a single
 * /api/sessions/summary batch (one sub-widget per field) for the side-by-side
 * multi-field table. The summary response is a JSON array of chunks; field chunks
 * carry `id` + `data`. Returns one entry per field: { exp, friendlyName, data, error }.
 */
export async function fetchSummaryFields (route, store, widget, { signal, excludeVariable } = {}) {
  const fields = widgetFields(widget);
  const localExp = widgetLocalExpression(widget, store.state.views, withoutPicks(store.state.dashboardVariables, excludeVariable));
  const body = {
    facets: 1,
    noStats: true,
    widgets: fields.map((exp, i) => ({
      id: `f${i}`,
      field: exp,
      length: widget.length,
      order: widget.order,
      ...(widget.orderBy === 'key' ? { orderBy: 'key' } : {}),
      metricType: widget.metricType,
      metrics: widget.metrics,
      metricAgg: widget.metricAgg,
      sortMetric: widget.sortMetric,
      ...(widget.bucketInterval > 0 ? { bucketInterval: widget.bucketInterval } : {}),
      ...(localExp ? { expression: localExp } : {})
    }))
  };
  const res = await postSummary(route, store, body, signal, excludeVariable);
  const chunks = Array.isArray(res) ? res : [];
  return fields.map((exp, i) => {
    const chunk = chunks.find(c => c && c.id === `f${i}`);
    return {
      exp,
      friendlyName: FieldService.getField(exp, true)?.friendlyName || exp,
      data: chunk?.data || [],
      error: chunk?.error || null
    };
  });
}

/** A summary row's value of one metric: the session count, or the metric's
 *  aggregate (0 when the row has none). */
export function metricRowValue (row, metric) {
  if (!metric || metric === 'sessions') { return row?.sessions || 0; }
  return row?.metricValues?.[metric] ?? 0;
}

/**
 * Fetch a combo or scatter widget's rows: its field's top values, each with
 * every metric slot's aggregate (metricValues), ranked by the first slot.
 * Returns the [{ item, sessions, metricValues, ... }] rows.
 */
export async function fetchMetricsChart (route, store, widget, { signal } = {}) {
  const metrics = slotMetrics(widget).filter(Boolean);
  const [res] = await fetchSummaryFields(route, store, {
    ...widget,
    fields: widget.field ? [widget.field] : [],
    metrics,
    metricType: metrics[0],
    sortMetric: metrics[0]
  }, { signal });
  if (res?.error) { throw new Error(res.error); }
  return res?.data || [];
}

/**
 * Fetch a control widget's value list: its field's top values by session
 * count, searched without the control's own pick (so the other values stay
 * listed once some are picked). Returns the [{ item, sessions }] rows.
 */
export async function fetchControlValues (route, store, widget, excludeVariable, { signal } = {}) {
  const [res] = await fetchSummaryFields(route, store, {
    ...widget,
    fields: widget.field ? [widget.field] : [],
    metricType: 'sessions',
    metrics: [],
    sortMetric: 'sessions',
    order: 'desc',
    orderBy: 'value',
    bucketInterval: 0
  }, { signal, excludeVariable });
  if (res?.error) { throw new Error(res.error); }
  return res?.data || [];
}

/**
 * Fetch a range control's bounds: its numeric field's smallest and largest
 * value, searched without the control's own pick (so the slider can widen
 * again). Two whole result set metrics in one summary request.
 * Returns { min, max } (undefined when no session has the field).
 */
export async function fetchControlRange (route, store, widget, excludeVariable, { signal } = {}) {
  if (FieldService.getField(widget.field, true)?.type !== 'integer') {
    throw { i18n: 'sessions.summary.widget.controlRangeNumeric' };
  }
  const localExp = widgetLocalExpression(widget, store.state.views, withoutPicks(store.state.dashboardVariables, excludeVariable));
  const bound = (id, agg) => ({ id, field: '', total: true, metricType: widget.field, metricAgg: agg, ...(localExp ? { expression: localExp } : {}) });
  const res = await postSummary(route, store, { facets: 1, noStats: true, widgets: [bound('lo', 'min'), bound('hi', 'max')] }, signal, excludeVariable);
  const chunks = Array.isArray(res) ? res : [];
  const value = (id) => {
    const chunk = chunks.find(c => c && c.id === id);
    if (chunk?.error) { throw new Error(chunk.error); }
    const item = chunk?.data?.[0];
    return item?.sessions ? item.value : undefined;
  };
  return { min: value('lo'), max: value('hi') };
}

/** POST a widgets body to /api/sessions/summary with the page's search, time
 *  window and route params folded in. Returns the chunk array. */
async function postSummary (route, store, body, signal, excludeVariable, timeShift) {
  const pageExp = pageExpression(route, store, excludeVariable); // global search + dashboard filter + controls
  if (pageExp) { body.expression = pageExp; }
  for (const p of ['view', 'bounding', 'interval', 'cluster']) {
    if (route.query[p]) { body[p] = route.query[p]; }
  }
  if (route.query.spanning === 'true') { body.spanning = true; }
  if (parseInt(store.state.timeRange, 10) === -1) {
    body.date = store.state.timeRange;
  } else {
    body.startTime = store.state.time.startTime;
    body.stopTime = store.state.time.stopTime;
  }
  shiftWindow(body, store, timeShift);
  return await fetchWrapper({ url: 'api/sessions/summary', method: 'POST', data: body, signal });
}

/**
 * Fetch the Metric widget's number(s): the metric aggregated over the whole
 * result set (total: true, no field), or one per top-N value of its field.
 * Returns the widget chunk's data[] ({ item, sessions, value, metricValues }).
 */
export async function fetchMetric (route, store, widget, { signal, timeShift } = {}) {
  const localExp = widgetLocalExpression(widget, store.state.views, store.state.dashboardVariables);
  const field = widgetFields(widget)[0] || '';
  const body = {
    facets: 1,
    noStats: true,
    widgets: [{
      id: 'm',
      field,
      total: !field,
      length: widget.length,
      order: widget.order,
      ...(widget.orderBy === 'key' ? { orderBy: 'key' } : {}),
      metricType: widget.metricType,
      metricAgg: widget.metricAgg,
      ...(widget.bucketInterval > 0 ? { bucketInterval: widget.bucketInterval } : {}),
      ...(localExp ? { expression: localExp } : {})
    }]
  };
  const res = await postSummary(route, store, body, signal, undefined, timeShift);
  const chunk = (Array.isArray(res) ? res : []).find(c => c && c.id === 'm');
  if (chunk?.error) { throw new Error(chunk.error); }
  return chunk?.data || [];
}
