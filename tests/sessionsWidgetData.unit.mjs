/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for the dashboard Sessions List widget's data helpers
(summary/widgets/sessionsWidgetData.js). Run with `npm run test:unit`.

The vueapp modules are written for vite: extensionless relative imports and
the @common alias. A resolve hook adds the extension and swaps in stubs for
the modules that need the browser app (FieldService, utils, fetchWrapper,
vueFilters); the fetchWrapper stub records its request and returns
globalThis.fetchResult.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { register } from 'node:module';

const stub = (src) => `data:text/javascript,${encodeURIComponent(src)}`;
const STUBS = {
  '/search/FieldService': stub(`
    const find = (s) => (globalThis.testFields || []).find(f => f.exp === s || f.dbField === s);
    export default { getField: (s) => find(s) };`),
  '/utils/utils': stub(`
    let n = 0;
    export default { createRandomString: () => 'rand' + (++n) };`),
  '@common/fetchWrapper.js': stub(`
    export const fetchWrapper = async (opts) => { globalThis.lastFetch = opts; return globalThis.fetchResult; };`),
  '@common/vueFilters.js': stub(`
    export const commaString = String;
    export const humanReadableBytes = String;
    export const timezoneDateString = (ms, tz, showMs) => 'date:' + ms + ':' + tz + ':' + !!showMs;
    export const protocol = (p) => ({ 6: 'tcp', 17: 'udp' })[p] || String(p);
    export const extractIPv6String = (v) => 'v6:' + v;`)
};

register(stub(`
const STUBS = ${JSON.stringify(STUBS)};
export async function resolve (specifier, context, next) {
  for (const [suffix, url] of Object.entries(STUBS)) {
    if (specifier.endsWith(suffix)) { return { url, shortCircuit: true }; }
  }
  if (specifier.startsWith('.') && !/\\.[a-z]+$/.test(specifier)) {
    return next(specifier + '.js', context);
  }
  return next(specifier, context);
}`));

globalThis.testFields = [
  { exp: 'starttime', dbField: 'firstPacket', friendlyName: 'Start Time', type: 'seconds' },
  { exp: 'ip.src', dbField: 'source.ip', friendlyName: 'Src IP', type: 'ip' },
  { exp: 'port.dst', dbField: 'destination.port', friendlyName: 'Dst Port', type: 'integer' },
  { exp: 'host.http', dbField: 'http.host', friendlyName: 'Hostname', type: 'lotermfield' },
  { exp: 'ip.protocol', dbField: 'ipProtocol', friendlyName: 'IP Protocol', type: 'lotermfield', transform: 'ipProtocolLookup' },
  { exp: 'country.src', dbField: 'source.geo.country_iso_code', friendlyName: 'Src Country', type: 'uptermfield' },
  // no column of their own
  { exp: 'ip', dbField: 'ipall', friendlyName: 'All IP fields', type: 'ip', noFacet: 'true' },
  { exp: 'port', dbField: 'portall', friendlyName: 'All port fields', type: 'integer', regex: '^port' },
  { exp: 'ip.dst:port', dbField: 'destination.ip:port', friendlyName: 'Dst IP:Dst Port' },
  // a Sessions page custom column (children still exps) and the info column
  { exp: 'src', dbField: 'src', friendlyName: 'Src IP / Country', sortBy: 'source.ip', children: ['ip.src', 'country.src'] },
  { exp: 'info', dbField: 'info', friendlyName: 'Info', unsortable: true, children: ['host.http'] }
];

const {
  DEFAULT_SORT_DB_FIELD, fieldHasColumn, columnSortDbField, widgetSortDbField, sessionsColumns,
  sessionsLength, sessionsRequestParams, cellValues, formatCellText, sessionsCSV, fetchSessions
} = await import('../viewer/vueapp/src/components/summary/widgets/sessionsWidgetData.js');

const field = (exp) => globalThis.testFields.find(f => f.exp === exp);

describe('columns', () => {
  test('one column per exp, labeled by friendly name, unknown exps kept', () => {
    const cols = sessionsColumns(['ip.src', 'nope']);
    assert.deepEqual(cols.map(c => [c.exp, c.label, c.parts.map(p => p.dbField)]), [
      ['ip.src', 'Src IP', ['source.ip']],
      ['nope', 'nope', []]
    ]);
  });

  test('a custom column reads its children', () => {
    const [col] = sessionsColumns(['src']);
    assert.deepEqual(col.parts.map(p => p.dbField), ['source.ip', 'source.geo.country_iso_code']);
    assert.equal(col.sortDb, 'source.ip');
  });

  test('fields with no column of their own have no parts', () => {
    for (const exp of ['ip', 'port', 'ip.dst:port']) {
      assert.equal(fieldHasColumn(field(exp)), false, exp);
      assert.deepEqual(sessionsColumns([exp])[0].parts, [], exp);
    }
    assert.equal(fieldHasColumn(field('host.http')), true);
  });

  test('sortable columns', () => {
    assert.equal(columnSortDbField(field('port.dst')), 'destination.port');
    assert.equal(columnSortDbField(field('info')), undefined);
    assert.equal(columnSortDbField(field('ip')), undefined);
    assert.equal(columnSortDbField(undefined), undefined);
  });
});

describe('sort', () => {
  test('defaults to the session start time', () => {
    assert.equal(DEFAULT_SORT_DB_FIELD, 'firstPacket');
    assert.equal(widgetSortDbField({ sortField: '' }), 'firstPacket');
    assert.equal(widgetSortDbField({}), 'firstPacket');
  });

  test('uses the sort field, or start time when it can not sort', () => {
    assert.equal(widgetSortDbField({ sortField: 'port.dst' }), 'destination.port');
    assert.equal(widgetSortDbField({ sortField: 'src' }), 'source.ip');
    assert.equal(widgetSortDbField({ sortField: 'info' }), 'firstPacket');
    assert.equal(widgetSortDbField({ sortField: 'ip.dst:port' }), 'firstPacket');
    assert.equal(widgetSortDbField({ sortField: 'missing' }), 'firstPacket');
  });
});

describe('request params', () => {
  test('columns, sort and limit', () => {
    const cols = sessionsColumns(['ip.src', 'src', 'port.dst', 'ip']);
    const params = sessionsRequestParams(cols, { sortField: 'port.dst', order: 'asc', length: 50 });
    assert.deepEqual(params, {
      flatten: 1,
      start: 0,
      length: 50,
      order: 'destination.port:asc',
      fields: 'source.ip,source.geo.country_iso_code,destination.port'
    });
  });

  test('default sort is start time descending and is always requested', () => {
    const params = sessionsRequestParams(sessionsColumns(['ip.src']), { length: 10 });
    assert.equal(params.order, 'firstPacket:desc');
    assert.equal(params.fields, 'source.ip,firstPacket');
  });

  test('never an empty field list', () => {
    assert.equal(sessionsRequestParams(sessionsColumns(['ip']), {}).fields, 'firstPacket');
  });

  test('limit falls back to the default and is capped', () => {
    assert.equal(sessionsLength({}), 20);
    assert.equal(sessionsLength({ length: 'x' }), 20);
    assert.equal(sessionsLength({ length: 100000 }), 100);
    assert.equal(sessionsLength({ length: '10' }), 10);
  });
});

describe('cells', () => {
  test('values as a list', () => {
    const f = field('host.http');
    assert.deepEqual(cellValues({ 'http.host': ['a', 'b'] }, f), ['a', 'b']);
    assert.deepEqual(cellValues({ 'http.host': 'a' }, f), ['a']);
    assert.deepEqual(cellValues({ 'http.host': 0 }, f), [0]);
    assert.deepEqual(cellValues({ 'http.host': '' }, f), []);
    assert.deepEqual(cellValues({}, f), []);
  });

  test('text formatting', () => {
    assert.equal(formatCellText(field('starttime'), 1700000000000, { timezone: 'gmt', ms: false }), 'date:1700000000000:gmt:false');
    assert.equal(formatCellText(field('ip.protocol'), 6), 'tcp');
    assert.equal(formatCellText(field('port.dst'), 1234), '1234');
  });

  test('csv', () => {
    const cols = sessionsColumns(['starttime', 'src', 'host.http']);
    const rows = [
      { firstPacket: 5, 'source.ip': '10.0.0.1', 'source.geo.country_iso_code': 'US', 'http.host': ['a.com', 'b.com'] },
      { firstPacket: 6, 'source.ip': '10.0.0.2' }
    ];
    assert.deepEqual(sessionsCSV(cols, rows, { timezone: 'local' }), {
      headers: ['Start Time', 'Src IP / Country', 'Hostname'],
      rows: [
        ['date:5:local:false', '10.0.0.1, US', 'a.com, b.com'],
        ['date:6:local:false', '10.0.0.2', '']
      ]
    });
  });
});

describe('fetchSessions', () => {
  const route = { query: { expression: 'port.dst == 80', bounding: 'first', interval: 'auto' } };
  const store = { state: { views: [{ id: 'v1', expression: 'ip.src == 10.0.0.1' }], timeRange: '-1', time: {} } };

  test('POSTs /api/sessions with the combined search, time and columns', async () => {
    globalThis.fetchResult = { data: [{ id: 'a' }], recordsFiltered: 1234, recordsTotal: 9999 };
    const widget = { viewMode: 'sessions', fields: ['ip.src'], view: 'v1', expression: 'tags == x', length: 10, order: 'desc' };
    const res = await fetchSessions(route, store, widget, { signal: 'sig' });
    assert.deepEqual(res, { rows: [{ id: 'a' }], total: 1234 });
    const { url, method, data, signal } = globalThis.lastFetch;
    assert.equal(url, 'api/sessions');
    assert.equal(method, 'POST');
    assert.equal(signal, 'sig');
    assert.deepEqual(data, {
      flatten: 1,
      start: 0,
      length: 10,
      order: 'firstPacket:desc',
      fields: 'source.ip,firstPacket',
      expression: '(port.dst == 80) && ((ip.src == 10.0.0.1) && (tags == x))',
      bounding: 'first',
      interval: 'auto',
      date: '-1'
    });
  });

  test('a widget with no columns asks for a field', async () => {
    await assert.rejects(fetchSessions(route, store, { viewMode: 'sessions', fields: [] }), { i18n: 'sessions.summary.widget.fieldRequired' });
  });

  test('a missing data list is no rows', async () => {
    globalThis.fetchResult = {};
    assert.deepEqual(await fetchSessions(route, store, { viewMode: 'sessions', fields: ['ip.src'] }), { rows: [], total: 0 });
  });
});
