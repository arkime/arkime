/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Unit tests for the dashboard's request planner
(summary/widgets/requestPlanner.js), which merges and orders the requests a
dashboard's widgets make. Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';

const { createPlanner, groupRequests, mergeKey, stableJson, DEFAULT_CONNECTION_FIELDS, MAX_SUMMARY_WIDGETS } =
  await import('../viewer/vueapp/src/components/summary/widgets/requestPlanner.js');

const tick = () => new Promise(resolve => setImmediate(resolve));

// a planner over a fake transport: each send/stream waits until the test
// answers it, timers run when the test says so
function harness ({ visible = () => true, maxInFlight } = {}) {
  const sends = [];
  const streams = [];
  const timers = [];
  const tracked = [];
  const planner = createPlanner({
    send: (url, data, signal) => new Promise((resolve, reject) => sends.push({ url, data, signal, resolve, reject })),
    stream: (url, data, signal, onChunk) => new Promise((resolve, reject) => streams.push({ url, data, signal, onChunk, resolve, reject })),
    isVisible: visible,
    track: {
      start: (req) => { const h = { owner: req.owner }; tracked.push(h); return h; },
      finish: (h, result) => { if (!h.result) { h.result = result; } }
    },
    setTimer: (fn, ms) => timers.push({ fn, ms }),
    ...(maxInFlight ? { maxInFlight } : {})
  });
  const runTimers = (ms) => {
    for (const timer of timers.splice(0).filter(x => x.ms <= ms)) { timer.fn(); }
  };
  // the batch goes out on the next timer turn
  const flush = async () => { await tick(); runTimers(0); await tick(); };
  return { planner, sends, streams, timers, tracked, runTimers, flush };
}

const settled = (promise) => {
  const state = { done: false };
  promise.then((value) => { state.done = true; state.value = value; }, (err) => { state.done = true; state.error = err; });
  return state;
};

describe('stableJson and mergeKey', () => {
  test('key order does not matter', () => {
    assert.equal(stableJson({ b: 1, a: [{ d: 2, c: 3 }] }), stableJson({ a: [{ c: 3, d: 2 }], b: 1 }));
    assert.notEqual(stableJson({ a: 1 }), stableJson({ a: 2 }));
  });

  test('connections merge by everything but fields, summary by everything but widgets', () => {
    const conn = (fields) => mergeKey({ url: 'api/connections', data: { srcField: 'source.ip', dstField: 'destination.ip', length: 500, fields } });
    assert.equal(conn('protocol').key, conn(undefined).key);
    assert.equal(conn('protocol').kind, 'connections');
    const sum = (exp, widgets) => mergeKey({ url: 'api/sessions/summary', data: { noStats: true, expression: exp, widgets } });
    assert.equal(sum('a', [{ id: 'm' }]).key, sum('a', [{ id: 'w' }]).key);
    assert.notEqual(sum('a', [{ id: 'm' }]).key, sum('b', [{ id: 'm' }]).key);
    // a summary request with its stats is never merged
    assert.equal(mergeKey({ url: 'api/sessions/summary', data: { widgets: [] } }).kind, 'same');
  });
});

describe('groupRequests', () => {
  test('connections carry every field asked for, with the defaults', () => {
    const base = { srcField: 'source.ip', dstField: 'destination.ip', length: 500 };
    const groups = groupRequests([
      { url: 'api/connections', data: base },
      { url: 'api/connections', data: { ...base, fields: 'protocol' } },
      { url: 'api/connections', data: base }
    ]);
    assert.equal(groups.length, 1);
    assert.deepEqual(groups[0].data.fields.split(',').sort(), [...DEFAULT_CONNECTION_FIELDS, 'protocol'].sort());
    assert.equal(groups[0].data.length, 500);
  });

  test('a lone request goes out as it was', () => {
    const data = { srcField: 'a', dstField: 'b', fields: 'protocol' };
    assert.equal(groupRequests([{ url: 'api/connections', data }])[0].data, data);
  });

  test('summary widgets are renamed apart, and a group stays under the widget limit', () => {
    const many = Array.from({ length: MAX_SUMMARY_WIDGETS - 1 }, (_, i) => ({ id: `w${i}` }));
    const groups = groupRequests([
      { url: 'api/sessions/summary', data: { noStats: true, widgets: [{ id: 'm', metricType: 'bytes' }] } },
      { url: 'api/sessions/summary', data: { noStats: true, widgets: [{ id: 'm', metricType: 'packets' }] } },
      { url: 'api/sessions/summary', data: { noStats: true, widgets: many } }
    ]);
    assert.equal(groups.length, 2);
    assert.deepEqual(groups[0].data.widgets.map(w => w.id), ['0:m', '1:m']);
    assert.equal(groups[1].members.length, 1);
    assert.equal(groups[1].data.widgets.length, MAX_SUMMARY_WIDGETS - 1);
  });

  test('a summary widget asked for more than once is sent once', () => {
    const total = { field: '', total: true, metricType: 'bytes' };
    const [group] = groupRequests([
      { url: 'api/sessions/summary', data: { noStats: true, widgets: [{ id: 'm', ...total }] } },
      { url: 'api/sessions/summary', data: { noStats: true, widgets: [{ id: 'lo', field: 'ip.src' }, { ...total, id: 'g' }] } }
    ]);
    assert.deepEqual(group.data.widgets, [{ id: '0:m', ...total }, { id: '1:lo', field: 'ip.src' }]);
    assert.deepEqual(group.routes.get('0:m'), [[0, 'm'], [1, 'g']]);
  });
});

describe('createPlanner', () => {
  test('identical requests go out once and share the response, its time counted once', async () => {
    const h = harness();
    const a = settled(h.planner.request({ url: 'api/spigraph', data: { exp: 'protocols', size: 5 }, owner: 'a' }));
    const b = settled(h.planner.request({ url: 'api/spigraph', data: { size: 5, exp: 'protocols' }, owner: 'b' }));
    await h.flush();
    assert.equal(h.sends.length, 1);
    h.sends[0].resolve({ data: { items: [1] }, serverMs: '42' });
    await tick();
    assert.deepEqual(a.value, { items: [1] });
    assert.equal(b.value, a.value);
    assert.deepEqual(h.tracked.map(t => t.result), [{ serverMs: '42', shared: false }, { serverMs: '42', shared: true }]);
  });

  test('different requests each go out', async () => {
    const h = harness();
    h.planner.request({ url: 'api/spigraph', data: { exp: 'a' } });
    h.planner.request({ url: 'api/spigraph', data: { exp: 'b' } });
    await h.flush();
    assert.equal(h.sends.length, 2);
  });

  test('summary requests over one search stream as one, each widget answered when its chunks are in', async () => {
    const h = harness();
    const a = settled(h.planner.request({ url: 'api/sessions/summary', data: { noStats: true, expression: 'x', widgets: [{ id: 'lo', metricAgg: 'min' }, { id: 'hi', metricAgg: 'max' }] }, owner: 'a' }));
    const b = settled(h.planner.request({ url: 'api/sessions/summary', data: { noStats: true, expression: 'x', widgets: [{ id: 'm', metricAgg: 'sum' }] }, owner: 'b' }));
    await h.flush();
    assert.equal(h.streams.length, 1);
    assert.deepEqual(h.streams[0].data.widgets.map(w => w.id), ['0:lo', '0:hi', '1:m']);
    const s = h.streams[0];
    s.onChunk({ id: '0:lo', data: [1], took: 5 });
    s.onChunk({ id: '1:m', data: [2], took: 7 });
    await tick();
    assert.ok(b.done && !a.done, 'b is answered before the stream ends, a still waits for hi');
    assert.deepEqual(b.value, [{ id: 'm', data: [2], took: 7 }]);
    s.onChunk({ id: '0:hi', data: [3], took: 11 });
    await tick();
    assert.deepEqual(a.value.map(c => c.id), ['lo', 'hi']);
    assert.deepEqual(h.tracked.map(t => t.result.serverMs), [16, 7]);
    s.onChunk({});
    s.resolve({});
  });

  test('a summary widget two widgets asked for is answered to both, its time counted once', async () => {
    const h = harness();
    const a = settled(h.planner.request({ url: 'api/sessions/summary', data: { noStats: true, widgets: [{ id: 'm', total: true }] }, owner: 'a' }));
    const b = settled(h.planner.request({ url: 'api/sessions/summary', data: { noStats: true, widgets: [{ id: 'g', total: true }] }, owner: 'b' }));
    await h.flush();
    assert.deepEqual(h.streams[0].data.widgets, [{ id: '0:m', total: true }]);
    h.streams[0].onChunk({ id: '0:m', data: [4], took: 9 });
    await tick();
    assert.deepEqual(a.value, [{ id: 'm', data: [4], took: 9 }]);
    assert.deepEqual(b.value, [{ id: 'g', data: [4], took: 9 }]);
    assert.deepEqual(h.tracked.map(t => [t.result.serverMs, t.result.shared]), [[9, false], [9, true]]);
    h.streams[0].resolve({});
  });

  test("the server's copy of a twin widget's search counts once", async () => {
    const h = harness();
    const a = settled(h.planner.request({ url: 'api/sessions/summary', data: { noStats: true, widgets: [{ id: 'm', field: 'ip' }, { id: 'n', field: 'ip' }] }, owner: 'a' }));
    const b = settled(h.planner.request({ url: 'api/spigraph', data: { exp: 'x' }, owner: 'b' }));
    const c = settled(h.planner.request({ url: 'api/sessions/summary', data: { noStats: true, expression: 'y', widgets: [{ id: 'p', field: 'ip' }, { id: 'q', field: 'ip' }] }, owner: 'c' }));
    await h.flush();
    // a lone stream: the member with one widget searched and one copied isn't all copies
    const lone = h.streams.find(s => s.data.widgets[0].id === 'm');
    lone.onChunk({ id: 'm', data: [1], took: 7 });
    lone.onChunk({ id: 'n', data: [1], took: 7, shared: true });
    await tick();
    assert.equal(a.value.length, 2);
    // and its time is the one search's, not the copy's again
    assert.deepEqual(h.tracked[0].result, { serverMs: 7, error: false, shared: false });
    h.sends[0].resolve({ data: {} });
    // all of its chunks copies: shared
    const other = h.streams.find(s => s.data.widgets[0].id === 'p');
    other.onChunk({ id: 'p', data: [2], took: 3, shared: true });
    other.onChunk({ id: 'q', data: [2], took: 3, shared: true });
    await tick();
    assert.equal(c.value.length, 2);
    assert.equal(h.tracked[2].result.shared, true);
    lone.resolve({});
    other.resolve({});
    await tick();
    assert.ok(b.done);
  });

  test('a shared response counts with the first widget still waiting for it', async () => {
    const h = harness();
    const ca = new AbortController();
    h.planner.request({ url: 'api/spigraph', data: { exp: 'p' }, signal: ca.signal, owner: 'a' }).catch(() => {});
    const b = settled(h.planner.request({ url: 'api/spigraph', data: { exp: 'p' }, owner: 'b' }));
    await h.flush();
    ca.abort();
    await tick();
    h.sends[0].resolve({ data: { items: [] }, serverMs: '42' });
    await tick();
    assert.deepEqual(b.value, { items: [] });
    assert.equal(h.tracked[0].result.error.name, 'AbortError');
    assert.deepEqual(h.tracked[1].result, { serverMs: '42', shared: false });
  });

  test('a summary error for the whole search reaches every widget of the stream', async () => {
    const h = harness();
    const a = settled(h.planner.request({ url: 'api/sessions/summary', data: { noStats: true, widgets: [{ id: 'm' }] } }));
    const b = settled(h.planner.request({ url: 'api/sessions/summary', data: { noStats: true, widgets: [{ id: 'm' }] } }));
    await h.flush();
    h.streams[0].onChunk({ bsqErr: 'bad expression' });
    h.streams[0].resolve({});
    await tick();
    assert.deepEqual(a.value, [{ bsqErr: 'bad expression' }]);
    assert.deepEqual(b.value, [{ bsqErr: 'bad expression' }]);
    assert.equal(h.tracked[0].result.error, true);
  });

  test('a lone summary request keeps its own ids', async () => {
    const h = harness();
    const a = settled(h.planner.request({ url: 'api/sessions/summary', data: { noStats: true, widgets: [{ id: 'm' }] } }));
    await h.flush();
    assert.deepEqual(h.streams[0].data.widgets, [{ id: 'm' }]);
    h.streams[0].onChunk({ id: 'm', data: [] });
    await tick();
    assert.deepEqual(a.value, [{ id: 'm', data: [] }]);
  });

  test('a failed request fails each widget it served', async () => {
    const h = harness();
    const a = settled(h.planner.request({ url: 'api/connections', data: { srcField: 's', dstField: 'd' } }));
    const b = settled(h.planner.request({ url: 'api/connections', data: { srcField: 's', dstField: 'd', fields: 'protocol' } }));
    await h.flush();
    h.sends[0].reject(Object.assign(new Error('boom'), { serverMs: '9' }));
    await tick();
    assert.equal(a.error.message, 'boom');
    assert.equal(b.error.message, 'boom');
    assert.equal(h.tracked[1].result.serverMs, '9');
  });

  test('the widgets on screen go first, the rest right behind without waiting', async () => {
    const h = harness({ visible: (owner) => owner !== 'below' });
    h.planner.request({ url: 'api/spigraph', data: { exp: 'below' }, owner: 'below' });
    h.planner.request({ url: 'api/spigraph', data: { exp: 'top' }, owner: 'top' });
    await h.flush();
    // nothing is answered yet and no timer has run: both are out, on screen first
    assert.deepEqual(h.sends.map(s => s.data.exp), ['top', 'below']);
    assert.equal(h.timers.length, 0);
  });

  test('a later on screen request jumps ahead of off screen ones still waiting for a slot', async () => {
    const h = harness({ visible: (owner) => !owner.startsWith('below'), maxInFlight: 1 });
    h.planner.request({ url: 'api/spigraph', data: { exp: 'top' }, owner: 'top' });
    h.planner.request({ url: 'api/spigraph', data: { exp: 'below1' }, owner: 'below1' });
    h.planner.request({ url: 'api/spigraph', data: { exp: 'below2' }, owner: 'below2' });
    await h.flush();
    h.planner.request({ url: 'api/spigraph', data: { exp: 'top2' }, owner: 'top2' });
    await h.flush();
    assert.deepEqual(h.sends.map(s => s.data.exp), ['top']);
    for (let i = 0; i < 3; i++) {
      h.sends[i].resolve({ data: {} });
      await tick();
    }
    assert.deepEqual(h.sends.map(s => s.data.exp), ['top', 'top2', 'below1', 'below2']);
  });

  test('with nothing on screen they all go', async () => {
    const h = harness({ visible: () => false });
    h.planner.request({ url: 'api/spigraph', data: { exp: 'a' } });
    h.planner.request({ url: 'api/spigraph', data: { exp: 'b' } });
    await h.flush();
    assert.equal(h.sends.length, 2);
  });

  test('only a few are in flight at once', async () => {
    const h = harness({ maxInFlight: 2 });
    for (const exp of ['a', 'b', 'c', 'd']) { h.planner.request({ url: 'api/spigraph', data: { exp } }); }
    await h.flush();
    assert.equal(h.sends.length, 2);
    h.sends[0].resolve({ data: {} });
    await tick();
    assert.equal(h.sends.length, 3);
  });

  test('a shared request is cancelled only once every widget it serves lets go', async () => {
    const h = harness();
    const ca = new AbortController();
    const cb = new AbortController();
    const a = settled(h.planner.request({ url: 'api/spigraph', data: { exp: 'p' }, signal: ca.signal }));
    const b = settled(h.planner.request({ url: 'api/spigraph', data: { exp: 'p' }, signal: cb.signal }));
    await h.flush();
    ca.abort();
    await tick();
    assert.equal(a.error.name, 'AbortError');
    assert.equal(h.sends[0].signal.aborted, false);
    cb.abort();
    await tick();
    assert.equal(b.error.name, 'AbortError');
    assert.equal(h.sends[0].signal.aborted, true);
  });

  test('a request cancelled before it goes out is never sent', async () => {
    const h = harness();
    const c = new AbortController();
    const a = settled(h.planner.request({ url: 'api/spigraph', data: { exp: 'p' }, signal: c.signal }));
    c.abort();
    await h.flush();
    assert.equal(h.sends.length, 0);
    assert.equal(a.error.name, 'AbortError');
  });
});
