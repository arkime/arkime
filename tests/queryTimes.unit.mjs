/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for common/vueapp/queryTimes.js, the footer's query times. Run with
`npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';

const { queryTimes, enableQueryTimes, newQueryBatch, startQuery, finishQuery, addQueryPart, batchSum, batchLongest } =
  await import('../common/vueapp/queryTimes.js');

enableQueryTimes();

describe('queryTimes', () => {
  test("a shared part is listed with its time but adds nothing to the batch's sum", () => {
    newQueryBatch();
    const stream = startQuery({ url: 'api/sessions/summary', method: 'POST' });
    addQueryPart(stream, { label: 'Services', owner: 'w19', serverMs: 300 });
    addQueryPart(stream, { label: 'Services reached', owner: 'w3', serverMs: 300, shared: true });
    finishQuery(stream, { serverMs: 20 });
    const batch = queryTimes.batches[0];
    assert.deepEqual(batch.entries.map(e => [e.label, e.serverMs, !!e.shared]), [[undefined, 20, false], ['Services', 300, false], ['Services reached', 300, true]]);
    assert.equal(batchSum(batch), 320);
    assert.equal(batchLongest(batch), 300);
    assert.equal(queryTimes.owners.w3.ms, 300, "a shared part is still its widget's time");
  });

  test('a shared request adds nothing to the sum either', () => {
    newQueryBatch();
    const a = startQuery({ url: 'api/spigraph', owner: 'a' });
    const b = startQuery({ url: 'api/spigraph', owner: 'b' });
    finishQuery(a, { serverMs: 50 });
    finishQuery(b, { serverMs: 50, shared: true });
    assert.equal(batchSum(queryTimes.batches[0]), 50);
  });
});
