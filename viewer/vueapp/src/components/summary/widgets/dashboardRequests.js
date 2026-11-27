/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

The dashboard's one request planner (requestPlanner.js), wired to the
browser: JSON requests through fetchWrapper, the summary endpoint's stream
read chunk by chunk, the footer's query times, and the dashboard's own test
of which widgets are on screen (Summary.vue sets it).
*/
import { fetchWrapper, ArkimeError } from '@common/fetchWrapper.js';
import setReqHeaders from '@common/setReqHeaders';
import { startQuery, finishQuery } from '@common/queryTimes.js';
import { createPlanner } from './requestPlanner';

let visible = () => true;

/** Sets the test of whether a widget (by id) is on screen; none means all are. */
export function setVisibilityProbe (fn) {
  visible = typeof fn === 'function' ? fn : () => true;
}

async function send (url, data, signal) {
  let serverMs;
  try {
    const res = await fetchWrapper({
      url,
      method: 'POST',
      data,
      signal,
      background: true, // the planner times each widget's request itself
      onResponse: (response) => { serverMs = response.headers.get('x-arkime-response-time'); }
    });
    return { data: res, serverMs };
  } catch (err) {
    err.serverMs = serverMs;
    throw err;
  }
}

// The summary endpoint streams a JSON array a chunk per line:
// [{...}\n,{...}\n ... ,{}\n]
async function stream (url, data, signal, onChunk) {
  const response = await fetch(url, {
    method: 'POST',
    headers: setReqHeaders({ 'Content-Type': 'application/json' }),
    body: JSON.stringify(data),
    signal
  });
  const serverMs = response.headers.get('x-arkime-response-time');
  if (!response.ok) {
    const text = await response.text().catch(() => '');
    let body;
    try { body = JSON.parse(text); } catch (e) { /* not JSON */ }
    const err = new ArkimeError((body && body.text) || text || response.statusText || 'bad response status', body);
    err.status = response.status;
    err.serverMs = serverMs;
    throw err;
  }

  const reader = response.body.getReader();
  const decoder = new TextDecoder();
  let buffer = '';
  const parseLine = (line) => {
    const json = line.slice(1).trim(); // the leading '[' or ','
    if (!json || json === ']') { return; }
    try { onChunk(JSON.parse(json)); } catch (e) { /* a partial or closing line */ }
  };
  while (true) {
    const { done, value } = await reader.read();
    if (done) { break; }
    buffer += decoder.decode(value, { stream: true });
    let pos;
    while ((pos = buffer.indexOf('\n')) > -1) {
      parseLine(buffer.slice(0, pos));
      buffer = buffer.slice(pos + 1);
    }
  }
  return { serverMs };
}

const planner = createPlanner({
  send,
  stream,
  isVisible: (owner) => visible(owner),
  track: {
    start: (req) => startQuery({ url: req.url, method: 'POST', label: req.label, owner: req.owner }),
    finish: (handle, result) => finishQuery(handle, result)
  }
});

/**
 * A dashboard widget's POST, planned with the rest of the dashboard's.
 * @param {object} options - { url, data, signal, queryLabel, queryOwner }
 * @returns {Promise<any>} the JSON response (the summary endpoint's: its chunks)
 */
export function dashboardRequest ({ url, data, signal, queryLabel, queryOwner }) {
  return planner.request({ url, data, signal, label: queryLabel, owner: queryOwner });
}
