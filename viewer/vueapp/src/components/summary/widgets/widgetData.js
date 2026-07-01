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

/**
 * A widget's field list (1-3 field exps). Multi-field widgets store `fields`;
 * single-field widgets store `field` (= fields[0]). Always returns an array.
 */
export function widgetFields (widget) {
  if (Array.isArray(widget?.fields) && widget.fields.length) { return widget.fields.slice(0, 3); }
  return widget?.field ? [widget.field] : [];
}

/**
 * The graph/histo key a SPIGraph-backed widget visualizes for its metric:
 * 'sessionsHisto' (session count) by default, else the chosen numeric field's
 * `<dbField>Histo` series (which the backend emits when sent `metric`).
 */
export function metricHistoKey (widget) {
  const metric = widget?.metricType;
  if (!metric || metric === 'sessions') { return 'sessionsHisto'; }
  const dbField = FieldService.getField(metric, true)?.dbField;
  return dbField ? `${dbField}Histo` : 'sessionsHisto';
}

/** True when a widget's metric is a byte-valued field (wants human-readable formatting). */
export function metricIsBytes (metricType) {
  if (!metricType || metricType === 'sessions') { return false; }
  return /bytes/i.test(metricType) || /bytes/i.test(FieldService.getField(metricType, true)?.dbField || '');
}

/** Format a widget metric value: human-readable bytes for byte metrics, else a comma count. */
export function formatMetricValue (metricType, v) {
  return metricIsBytes(metricType) ? humanReadableBytes(v || 0) : commaString(v || 0);
}

// Neutralize CSV/spreadsheet formula injection (a cell starting with = + - @ TAB CR
// can execute in Excel/Sheets), then quote cells containing comma/quote/newline.
function escapeCSV (value) {
  let s = String(value ?? '');
  if (s.length > 0 && '=+-@\t\r'.includes(s[0])) { s = `'${s}`; }
  if (s.includes('"') || s.includes(',') || s.includes('\n')) { return `"${s.replace(/"/g, '""')}"`; }
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
 * (either may be unset). The View is resolved from store.state.views by id/name.
 * @returns {string|undefined} the combined local expression (or undefined)
 */
export function widgetLocalExpression (widget, views) {
  const parts = [];
  if (widget?.view) {
    const v = (views || []).find(x => x.id === widget.view || x.name === widget.view);
    if (v?.expression) { parts.push(v.expression); }
  }
  if (widget?.expression) { parts.push(widget.expression); }
  if (!parts.length) { return undefined; }
  return parts.length === 1 ? parts[0] : parts.map(p => `(${p})`).join(' && ');
}

/**
 * Combine the global search expression with a widget's local filter (View +
 * expression).
 * @returns {string|undefined} the combined expression (or undefined if neither)
 */
export function combinedExpression (route, store, widget) {
  const globalExp = route.query.expression;
  const localExp = widgetLocalExpression(widget, store.state.views);
  if (globalExp && localExp) { return `(${globalExp}) && (${localExp})`; }
  return localExp || globalExp || undefined;
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
    extra.metric = widget.metricType;
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
export async function fetchGraph (route, store, widget, { signal } = {}) {
  const extra = { exp: 'node', size: 1 };
  if (widget.metricType && widget.metricType !== 'sessions') {
    extra.metric = widget.metricType;
  }
  const data = buildWidgetParams(route, store, widget, extra);
  return await fetchWrapper({ url: 'api/spigraph', method: 'POST', data, signal });
}

/**
 * Fetch SPIGraph hierarchy data for one field (the "Intersection" table). For a
 * single field the endpoint returns `tableResults` as a flat list of
 * { name, size, parents: [] } — one row per unique value. Returns the raw
 * response ({ tableResults, hierarchicalResults }).
 */
export async function fetchHierarchy (route, store, widget, { signal } = {}) {
  const data = buildWidgetParams(route, store, widget, {
    exp: widgetFields(widget).join(','), // 1-3 nested fields
    size: widget.length
  });
  return await fetchWrapper({ url: 'api/spigraphhierarchy', method: 'POST', data, signal });
}

/**
 * Fetch each of a widget's fields' top-N (with the chosen metric) in a single
 * /api/sessions/summary batch (one sub-widget per field) for the side-by-side
 * multi-field table. The summary response is a JSON array of chunks; field chunks
 * carry `id` + `data`. Returns one entry per field: { exp, friendlyName, data, error }.
 */
export async function fetchSummaryFields (route, store, widget, { signal } = {}) {
  const fields = widgetFields(widget);
  const localExp = widgetLocalExpression(widget, store.state.views);
  const body = {
    facets: 1,
    noStats: true,
    widgets: fields.map((exp, i) => ({
      id: `f${i}`,
      field: exp,
      length: widget.length,
      order: widget.order,
      metricType: widget.metricType,
      metrics: widget.metrics,
      sortMetric: widget.sortMetric,
      ...(localExp ? { expression: localExp } : {})
    }))
  };
  if (route.query.expression) { body.expression = route.query.expression; } // global search
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

  const res = await fetchWrapper({ url: 'api/sessions/summary', method: 'POST', data: body, signal });
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
