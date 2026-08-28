/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { isFieldMode } from './widgets/viewModes';

// map v7-only field view modes to the closest mode a v6 viewer can render
// (v6 knows only bar/pie/table) for the dual-written legacy fields[] shape
const V6_VIEW_MODES = { bar: 'bar', pie: 'pie', table: 'table', intersection: 'table', heatmap: 'bar', treemap: 'bar' };

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
