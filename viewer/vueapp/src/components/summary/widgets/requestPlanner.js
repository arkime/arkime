/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

The dashboard's request planner. A dashboard load has every widget ask for its
own data at once; the planner collects the requests made in the same moment
and sends fewer, sooner, before any of them goes out:

- identical requests (same endpoint and body) go out once, and each widget
  gets the one response;
- connections requests that differ only in the fields they carry (the
  connection graph, tree and circle of one field pair) go out once, carrying
  every field any of them asked for;
- summary requests with the same search and time window go out as one, its
  widgets renamed apart (a widget two of them ask for goes once), and the
  streamed response is handed back to each widget as soon as its own chunks
  are in;
- the widgets on screen go first, the rest right behind them: being on screen
  only sets the order, nothing waits for an answer before it goes, so a fast
  database answers the whole page in about the time of its slowest request;
- at most a few requests are in flight at once (the browser's own limit).

This module has no browser dependencies: dashboardRequests.js gives it the
transport, the query times tracker and the on screen test.
*/

// /api/connections' fields when a request names none (apiConnections.js)
export const DEFAULT_CONNECTION_FIELDS = ['network.bytes', 'totDataBytes', 'network.packets', 'node'];
// /api/sessions/summary's widget limit (apiSessions.js MAX_WIDGETS)
export const MAX_SUMMARY_WIDGETS = 100;

const CONNECTIONS = 'api/connections';
const SUMMARY = 'api/sessions/summary';

function abortError () {
  const err = new Error('The operation was aborted.');
  err.name = 'AbortError';
  return err;
}

const withoutKey = (data, key) => {
  const copy = { ...data };
  delete copy[key];
  return copy;
};

// A stable JSON of a request body (keys sorted), so bodies built in a
// different key order still match
export function stableJson (value) {
  if (Array.isArray(value)) { return `[${value.map(stableJson).join(',')}]`; }
  if (value && typeof value === 'object') {
    return `{${Object.keys(value).sort().filter(k => value[k] !== undefined).map(k => `${JSON.stringify(k)}:${stableJson(value[k])}`).join(',')}}`;
  }
  return JSON.stringify(value);
}

/**
 * What a request merges by: its kind and the key of the group it can join.
 * @param {object} req - { url, data }
 * @returns {{ kind: 'same'|'connections'|'summary', key: string }}
 */
export function mergeKey (req) {
  const data = req.data || {};
  if (req.url === CONNECTIONS) {
    return { kind: 'connections', key: `${req.url} ${stableJson(withoutKey(data, 'fields'))}` };
  }
  if (req.url === SUMMARY && Array.isArray(data.widgets) && (data.noStats === true || data.noStats === 'true')) {
    return { kind: 'summary', key: `${req.url} ${stableJson(withoutKey(data, 'widgets'))}` };
  }
  return { kind: 'same', key: `${req.url} ${stableJson(data)}` };
}

// a connections request's fields, the endpoint's defaults when it names none
export function connectionFields (data) {
  return typeof data?.fields === 'string' && data.fields ? data.fields.split(',') : DEFAULT_CONNECTION_FIELDS;
}

/**
 * The groups a batch of requests goes out as, in the order they were asked
 * for. A summary group holds at most MAX_SUMMARY_WIDGETS widgets.
 * @param {object[]} reqs - { url, data, ... }
 * @returns {object[]} { kind, url, data, members: [req, ...] } (a merged
 *                     summary group also has routes, see below)
 */
export function groupRequests (reqs) {
  const groups = [];
  const openGroups = new Map(); // merge key -> the group still taking requests
  for (const req of reqs) {
    const { kind, key } = mergeKey(req);
    let group = openGroups.get(key);
    if (group && kind === 'summary' && group.widgetCount + req.data.widgets.length > MAX_SUMMARY_WIDGETS) { group = undefined; }
    if (!group) {
      group = { kind, url: req.url, members: [], widgetCount: 0 };
      groups.push(group);
      openGroups.set(key, group);
    }
    group.members.push(req);
    if (kind === 'summary') { group.widgetCount += req.data.widgets.length; }
  }

  for (const group of groups) {
    const first = group.members[0].data;
    if (group.kind === 'connections' && group.members.length > 1) {
      const fields = [...new Set(group.members.flatMap(m => connectionFields(m.data)))];
      group.data = { ...first, fields: fields.join(',') };
    } else if (group.kind === 'summary' && group.members.length > 1) {
      // each member's widgets renamed apart, <member index>:<widget id>; a
      // widget asked for again (the same but for its id) isn't sent again,
      // its chunk goes to everyone who asked: routes has who, by the id sent
      const widgets = [];
      const sentIds = new Map(); // a widget less its id -> the id it was sent with
      group.routes = new Map(); // id sent -> [[member index, the member's id for it], ...]
      group.members.forEach((m, i) => {
        for (const w of m.data.widgets) {
          const spec = stableJson(withoutKey(w, 'id'));
          let sentId = sentIds.get(spec);
          if (sentId === undefined) {
            sentId = `${i}:${w.id}`;
            sentIds.set(spec, sentId);
            group.routes.set(sentId, []);
            widgets.push({ ...w, id: sentId });
          }
          group.routes.get(sentId).push([i, w.id]);
        }
      });
      group.data = { ...first, widgets };
    } else {
      group.data = first;
    }
  }
  return groups;
}

/**
 * Makes a planner.
 * @param {object} deps
 * @param {(url, data, signal) => Promise<{ data, serverMs }>} deps.send - one JSON request
 * @param {(url, data, signal, onChunk) => Promise<{ serverMs }>} deps.stream - a streamed summary request, calling onChunk(chunk) per chunk
 * @param {(owner) => boolean} [deps.isVisible] - whether a widget is on screen (all are by default)
 * @param {object} [deps.track] - query times: { start(req) => handle, finish(handle, { serverMs, error }) }
 * @param {(fn, ms) => any} [deps.setTimer] - setTimeout
 * @param {number} [deps.maxInFlight] - requests in flight at once
 * @returns {{ request: (req) => Promise<any> }}
 */
export function createPlanner ({
  send,
  stream,
  isVisible = () => true,
  track = { start: () => undefined, finish: () => {} },
  setTimer = (fn, ms) => setTimeout(fn, ms),
  maxInFlight = 6
}) {
  let queued = []; // requests asked for since the last flush
  let flushTimer;
  const ready = []; // groups waiting for a slot, on screen ones first
  let inFlight = 0;

  const finished = () => {
    inFlight--;
    pump();
  };

  // a member that was aborted gets nothing more
  const live = (m) => !m.done;
  const resolveMember = (m, value) => { if (live(m)) { m.done = true; m.resolve(value); } };
  const rejectMember = (m, err) => { if (live(m)) { m.done = true; m.reject(err); } };

  async function runSame (group) {
    let serverMs;
    try {
      const res = await send(group.url, group.data, group.controller.signal);
      serverMs = res.serverMs;
      // a shared response's time counts once in the batch's sum: with the
      // first widget still waiting for it (one that let go has no time)
      let counted = false;
      for (const m of group.members) {
        const waiting = live(m);
        track.finish(m.handle, { serverMs, shared: waiting && counted });
        counted ||= waiting;
        resolveMember(m, res.data);
      }
    } catch (err) {
      for (const m of group.members) {
        track.finish(m.handle, { serverMs: err.serverMs ?? serverMs, error: err });
        rejectMember(m, err);
      }
    }
  }

  async function runSummary (group) {
    const merged = group.members.length > 1;
    // each member's chunks, and how many of its widgets are still to come
    const chunks = group.members.map(() => []);
    const left = group.members.map(m => m.data.widgets.length);
    // how many of a member's chunks another member's time already counts
    const copies = group.members.map(() => 0);
    const done = (i, error) => {
      const m = group.members[i];
      if (m.settled) { return; }
      m.settled = true;
      const timed = chunks[i].filter(c => Number.isFinite(c?.took));
      // a twin's copy is the server's one search over again: its time is the
      // member's only when the member has no search of its own
      const searched = timed.filter(c => c.shared !== true);
      // the stream's time header only covers its first chunk: time a member
      // by its own chunks (a member all of whose chunks were another's too
      // adds nothing to the batch's sum)
      track.finish(m.handle, {
        serverMs: timed.length ? (searched.length ? searched : timed).reduce((total, c) => total + c.took, 0) : undefined,
        error: error || chunks[i].some(c => c?.error || c?.bsqErr),
        shared: timed.length > 0 && copies[i] >= timed.length
      });
      if (error) { rejectMember(m, error); } else { resolveMember(m, chunks[i]); }
    };
    const onChunk = (chunk) => {
      if (!chunk || typeof chunk !== 'object') { return; }
      if (typeof chunk.id !== 'string' || !merged) {
        if (!merged) {
          chunks[0].push(chunk);
          // the server's copy of a twin widget's search
          if (chunk.shared === true) { copies[0]++; }
          if (typeof chunk.id === 'string' && --left[0] === 0) { done(0); }
        } else {
          // a chunk for the whole search (a bsqErr): every member's
          chunks.forEach(c => c.push(chunk));
        }
        return;
      }
      // to each member that asked for this widget, under the member's own
      // id; its time counts with the first of them still waiting for it
      let counted = false;
      for (const [i, id] of (group.routes.get(chunk.id) || [])) {
        chunks[i].push({ ...chunk, id });
        if (counted || chunk.shared === true) { copies[i]++; }
        counted ||= live(group.members[i]);
        if (--left[i] === 0) { done(i); }
      }
    };
    try {
      await stream(group.url, group.data, group.controller.signal, onChunk);
      group.members.forEach((m, i) => done(i));
    } catch (err) {
      group.members.forEach((m, i) => done(i, err));
    }
  }

  function start (group) {
    inFlight++;
    // the request is cancelled only once every widget it serves has let go
    group.controller = new AbortController();
    const run = group.kind === 'summary' ? runSummary : runSame;
    run(group).finally(finished);
  }

  function pump () {
    while (inFlight < maxInFlight && ready.length) {
      const group = ready.shift();
      if (group.members.every(m => !live(m))) { continue; }
      start(group);
    }
  }

  function flush () {
    flushTimer = undefined;
    const reqs = queued.filter(live);
    queued = [];
    if (!reqs.length) { return; }
    // on screen groups ahead of any off screen ones still waiting for a slot
    const onscreen = groupRequests(reqs.filter(r => r.onscreen));
    const firstOffscreen = ready.findIndex(g => !g.onscreen);
    for (const group of onscreen) { group.onscreen = true; }
    ready.splice(firstOffscreen === -1 ? ready.length : firstOffscreen, 0, ...onscreen);
    ready.push(...groupRequests(reqs.filter(r => !r.onscreen)));
    for (const group of ready) {
      for (const m of group.members) { m.group = group; }
    }
    pump();
  }

  /**
   * Asks for a widget's data.
   * @param {object} req - { url, data, signal, owner, label }
   * @returns {Promise<any>} the response (a summary request's: its chunks)
   */
  function request (req) {
    return new Promise((resolve, reject) => {
      const m = { ...req, resolve, reject, done: false };
      if (req.signal?.aborted) { reject(abortError()); return; }
      m.onscreen = !!isVisible(req.owner);
      m.handle = track.start(req);
      req.signal?.addEventListener('abort', () => {
        if (!live(m)) { return; }
        track.finish(m.handle, { error: abortError() });
        rejectMember(m, abortError());
        if (m.group?.controller && m.group.members.every(x => !live(x))) { m.group.controller.abort(); }
      }, { once: true });
      queued.push(m);
      if (!flushTimer) { flushTimer = setTimer(flush, 0); }
    });
  }

  return { request };
}
