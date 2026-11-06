/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Data helpers for the Sessions List widget: a compact sessions table, the
dashboard's take on a saved-search document table. Its columns are field exps
resolved to field objects; its rows come from /api/sessions, the endpoint and
params the Sessions page lists with, narrowed to the columns' db fields.
*/
import { fetchWrapper } from '@common/fetchWrapper.js';
import { timezoneDateString, protocol, extractIPv6String } from '@common/vueFilters.js';
import FieldService from '../../search/FieldService';
import { lengthOptions, defaultLength } from './viewModes';
import { buildWidgetParams, widgetFields } from './widgetData';

// session start time: the widget's sort when it has none (the Sessions page's too)
export const DEFAULT_SORT_DB_FIELD = 'firstPacket';

const getFieldDefault = (exp) => FieldService.getField(exp, true);

// the db fields with no column of their own: All IP fields and Dst IP:Dst Port
// are summary-only groupings
const NO_COLUMN_DB_FIELDS = ['ip', 'destination.ip:port'];

/** True when a field is a real session column: not an "All X" regex group, a
 *  noFacet pseudo-field (All IP fields' ipall, the view) or a summary-only
 *  grouping. The Sessions page column picker leaves the same ones out. */
export function fieldHasColumn (field) {
  return !!field?.dbField && field.regex === undefined && !field.noFacet && !NO_COLUMN_DB_FIELDS.includes(field.dbField);
}

/** The db field a column sorts by (a custom column sorts by its sortBy), or
 *  undefined when it can't be sorted. */
export function columnSortDbField (field) {
  if (field?.sortBy) { return field.sortBy; }
  if (!field || field.unsortable || field.children || !fieldHasColumn(field)) { return undefined; }
  return field.dbField;
}

/** The db field a widget sorts by: its sortField's, else the session start time. */
export function widgetSortDbField (widget, getField = getFieldDefault) {
  return (widget?.sortField && columnSortDbField(getField(widget.sortField))) || DEFAULT_SORT_DB_FIELD;
}

/**
 * The widget's columns, one per field exp: the header label, the field, and
 * the fields whose values fill its cells (just the field, or a custom
 * multi-field column's children, eg Src IP / Country).
 * @param {string[]} exps - the column field exps
 * @param {(exp: string) => object} [getField] - resolves an exp to its field
 * @returns {{ exp: string, label: string, field: object, sortDb: string|undefined, parts: object[] }[]}
 */
export function sessionsColumns (exps, getField = getFieldDefault) {
  return (exps || []).map(exp => {
    const field = getField(exp);
    // customCols children are exps until the Sessions page swaps in fields
    const children = Array.isArray(field?.children)
      ? field.children.map(c => (typeof c === 'string' ? getField(c) : c))
      : [field];
    return {
      exp,
      label: field?.friendlyName || exp,
      field,
      sortDb: columnSortDbField(field),
      parts: children.filter(fieldHasColumn)
    };
  });
}

/** The row count to fetch: the widget's limit, capped at the largest choice. */
export function sessionsLength (widget) {
  const n = parseInt(widget?.length, 10);
  const max = Math.max(...lengthOptions('sessions'));
  return n > 0 ? Math.min(n, max) : defaultLength('sessions');
}

/**
 * The /api/sessions params for the columns (on top of buildWidgetParams'
 * search/time): the first `length` rows ordered by the sort field, flattened
 * (dotted keys, as the Sessions page asks), with only the columns' db fields.
 * The server always adds id, node and the src/dst ip and port; no facets, so
 * none of the page's map/graph aggregations run.
 */
export function sessionsRequestParams (columns, widget, getField = getFieldDefault) {
  const sortDb = widgetSortDbField(widget, getField);
  const fields = new Set();
  for (const col of columns) {
    for (const part of col.parts) { fields.add(part.dbField); }
  }
  // the sort field too, which also keeps the list from being empty (columns
  // with no db field): an empty one gets the server's default wide field set
  fields.add(sortDb);
  return {
    flatten: 1,
    start: 0,
    length: sessionsLength(widget),
    order: `${sortDb}:${widget?.order === 'asc' ? 'asc' : 'desc'}`,
    fields: [...fields].join(',')
  };
}

/** A row's values for one field as a list (none, one, or a multi-valued field's). */
export function cellValues (row, field) {
  const v = row?.[field?.dbField];
  if (v === undefined || v === null || v === '') { return []; }
  return Array.isArray(v) ? v : [v];
}

/** A value as plain text, formatted like SessionField's parse (dates in the
 *  user's timezone, ip protocol names, ipv6 hex) but without number commas. */
export function formatCellText (field, value, settings = {}) {
  switch (field?.type) {
  case 'date':
  case 'seconds':
    return timezoneDateString(parseInt(value, 10), settings.timezone, settings.ms);
  case 'lotermfield':
    if (field.transform === 'ipv6ToHex') { return extractIPv6String(value); }
    if (field.transform === 'ipProtocolLookup') { return protocol(value); }
    break;
  }
  return String(value);
}

/** The CSV of the table: a header of column labels and one line per row, a
 *  cell's values joined with ", ". Returns { headers, rows } for downloadCSV. */
export function sessionsCSV (columns, rows, settings = {}) {
  return {
    headers: columns.map(c => c.label),
    rows: (rows || []).map(row => columns.map(col => col.parts
      .flatMap(part => cellValues(row, part).map(v => formatCellText(part, v, settings)))
      .join(', ')))
  };
}

/**
 * Fetch the widget's sessions from /api/sessions: the combined search, time
 * window and route params (buildWidgetParams) plus the column/sort/limit
 * params above. Returns { rows, total } (total = all sessions matching).
 */
export async function fetchSessions (route, store, widget, { signal } = {}) {
  const columns = sessionsColumns(widgetFields(widget));
  if (!columns.length) { throw { i18n: 'sessions.summary.widget.fieldRequired' }; }
  const data = buildWidgetParams(route, store, widget, sessionsRequestParams(columns, widget));
  const res = await fetchWrapper({ url: 'api/sessions', method: 'POST', data, signal });
  return { rows: Array.isArray(res?.data) ? res.data : [], total: res?.recordsFiltered || 0 };
}
