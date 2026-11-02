// Copyright Yahoo Inc.
// SPDX-License-Identifier: Apache-2.0
//
// Composite timeline metrics: the *Histo keys that sum into each selectable
// metric, with the src/dst role used for series color. Single source of truth
// shared by TimelineGraph (series defs) and Heatmap (intensity binning).
export const COMPOSITE_METRICS = {
  'network.packetsHisto': [
    { key: 'source.packetsHisto', label: 'Src packets', role: 'src' },
    { key: 'destination.packetsHisto', label: 'Dst packets', role: 'dst' }
  ],
  'network.bytesHisto': [
    { key: 'source.bytesHisto', label: 'Src bytes', role: 'src' },
    { key: 'destination.bytesHisto', label: 'Dst bytes', role: 'dst' }
  ],
  totDataBytesHisto: [
    { key: 'client.bytesHisto', label: 'Client bytes', role: 'src' },
    { key: 'server.bytesHisto', label: 'Server bytes', role: 'dst' }
  ]
};
// legacy aliases
COMPOSITE_METRICS.totPacketsHisto = COMPOSITE_METRICS['network.packetsHisto'];
COMPOSITE_METRICS.totBytesHisto = COMPOSITE_METRICS['network.bytesHisto'];

// histo keys that sum into a metric; a non-composite metric is its own key
export function metricHistoKeys (metric) {
  const defs = COMPOSITE_METRICS[metric];
  return defs ? defs.map(d => d.key) : [metric];
}

// a graph's [[time, value], ...] series for a metric, a composite metric's
// series summed by time
export function metricSeries (graph, metric) {
  const keys = metricHistoKeys(metric);
  if (keys.length === 1) { return graph?.[keys[0]] || []; }
  const sums = new Map();
  for (const key of keys) {
    for (const [t, v] of (graph?.[key] || [])) { sums.set(t, (sums.get(t) || 0) + (v || 0)); }
  }
  return [...sums.entries()].sort((a, b) => a[0] - b[0]);
}
