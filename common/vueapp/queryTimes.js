/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0
*/
import { reactive } from 'vue';

// Tracks the timed API requests a page makes, grouped into batches. A batch
// is one burst of requests: a search, an auto refresh tick, a detail open.
const SETTLE_MS = 500; // idle time after which the next request starts a new batch
const MAX_BATCHES = 6;

export const queryTimes = reactive({
  batches: [] // newest first: { id, start, end, pending, entries: [] }
});

let nextId = 1;
let forceNew = false;

function currentBatch () {
  const now = Date.now();
  const last = queryTimes.batches[0];
  if (last && !forceNew && (last.pending > 0 || now - last.end <= SETTLE_MS)) {
    return last;
  }
  forceNew = false;
  // a batch whose requests were all untimed is not worth keeping
  if (last && !last.entries.length && !last.pending) { queryTimes.batches.shift(); }
  queryTimes.batches.unshift({ id: nextId++, start: now, end: now, pending: 0, entries: [] });
  if (queryTimes.batches.length > MAX_BATCHES) { queryTimes.batches.length = MAX_BATCHES; }
  return queryTimes.batches[0];
}

/** The next request starts a new batch, even if the last one has not settled. */
export function newQueryBatch () {
  forceNew = true;
}

/**
 * Starts tracking a request.
 * @param {object} info - { url, method, label, owner } (owner: a dashboard widget id)
 * @returns {object} The handle to pass to finishQuery
 */
export function startQuery ({ url, method, label, owner }) {
  const batch = currentBatch();
  const entry = {
    id: nextId++,
    path: String(url || '').split('?')[0],
    method: method || 'GET',
    label,
    owner,
    start: Date.now(),
    status: 'pending',
    serverMs: undefined,
    clientMs: undefined
  };
  batch.entries.push(entry);
  batch.pending++;
  return { batchId: batch.id, entryId: entry.id };
}

/**
 * Finishes a tracked request. A successful request the server did not time
 * (no response time header) is dropped.
 * @param {object} handle - What startQuery returned (undefined is ignored)
 * @param {object} result - { serverMs, error } (error: what the request threw,
 *                          or true when it failed without throwing)
 */
export function finishQuery (handle, { serverMs, error } = {}) {
  if (!handle) { return; }
  const result = !error ? 'ok' : (error.name === 'AbortError' ? 'cancelled' : 'error');
  const batch = queryTimes.batches.find(b => b.id === handle.batchId);
  if (!batch) { return; }
  const index = batch.entries.findIndex(e => e.id === handle.entryId);
  if (index === -1 || batch.entries[index].status !== 'pending') { return; }
  const entry = batch.entries[index];
  const now = Date.now();
  batch.pending--;
  batch.end = now;

  const ms = parseInt(serverMs, 10);
  if (isNaN(ms) && result === 'ok') {
    batch.entries.splice(index, 1);
    return;
  }
  entry.serverMs = isNaN(ms) ? undefined : ms;
  entry.clientMs = now - entry.start;
  entry.status = result;
}

/**
 * Adds an already finished part of a tracked request, such as one widget of
 * a streamed dashboard response.
 * @param {object} handle - The parent request's handle
 * @param {object} info - { label, owner, serverMs, status }
 */
export function addQueryPart (handle, { label, owner, serverMs, status: result = 'ok' }) {
  const batch = handle && queryTimes.batches.find(b => b.id === handle.batchId);
  const main = batch?.entries.find(e => e.id === handle.entryId);
  const ms = parseInt(serverMs, 10);
  if (!main || isNaN(ms)) { return; }
  batch.entries.push({
    id: nextId++,
    path: main.path,
    method: main.method,
    label,
    owner,
    parent: main.id,
    start: Date.now() - ms,
    status: result,
    serverMs: ms,
    clientMs: undefined
  });
}

/** The batches that have something to show, newest first. */
export function shownBatches () {
  return queryTimes.batches.filter(b => b.entries.length);
}

/** The longest server time of a batch's finished requests (undefined when none). */
export function batchLongest (batch) {
  let longest;
  for (const e of batch.entries) {
    if (e.serverMs !== undefined && (longest === undefined || e.serverMs > longest)) { longest = e.serverMs; }
  }
  return longest;
}

/** The longest server time of the owner's requests in the newest batch it is in
 *  (undefined while they run, or when they failed without a time). */
export function ownerTook (owner) {
  if (!owner) { return undefined; }
  const batch = queryTimes.batches.find(b => b.entries.some(e => e.owner === owner));
  const times = (batch?.entries || []).filter(e => e.owner === owner && e.serverMs !== undefined).map(e => e.serverMs);
  return times.length ? Math.max(...times) : undefined;
}
