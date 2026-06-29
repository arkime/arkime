/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Single source of truth for dashboard field-widget view-mode metadata, shared by
Arkime.vue (page), Summary.vue (host) and SummaryWidgetEditModal.vue.
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

// View modes that fetch their own data (vs. the batched summary stream)
export const SELF_FETCH_VIEW_MODES = ['heatmap', 'treemap'];

/** True for stream-mode (bar/pie/table) widgets, false for self-fetch ones. */
export const isStreamMode = (viewMode) => !SELF_FETCH_VIEW_MODES.includes(viewMode);

// View modes with no selectable metric. Table is inherently multi-column
// (sessions/packets/bytes); bar/pie/heatmap/treemap all visualize one metric.
export const METRICLESS_VIEW_MODES = ['table'];
