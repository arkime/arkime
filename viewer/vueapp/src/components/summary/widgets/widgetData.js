/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Shared helpers for field widgets whose view mode self-fetches (e.g. heatmap,
which needs per-value time series from /api/spigraph rather than the batched
summary stream). Combines the global search expression with the widget's local
filter and the global time window.
*/
import { fetchWrapper } from '@common/fetchWrapper.js';

/**
 * Combine the global search expression with a widget's local filter expression.
 * @returns {string|undefined} the combined expression (or undefined if neither)
 */
export function combinedExpression (route, widget) {
  const globalExp = route.query.expression;
  const localExp = widget?.expression;
  if (globalExp && localExp) { return `(${globalExp}) && (${localExp})`; }
  return localExp || globalExp || undefined;
}

/**
 * Build the shared query params (combined expression + time window + view /
 * bounding / cluster) for a widget's independent fetch, plus endpoint extras.
 */
export function buildWidgetParams (route, store, widget, extra = {}) {
  const params = { ...extra };

  const exp = combinedExpression(route, widget);
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
  const data = buildWidgetParams(route, store, widget, {
    exp: widget.field,
    size: widget.length
  });
  return await fetchWrapper({ url: 'api/spigraph', method: 'POST', data, signal });
}
