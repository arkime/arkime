/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

The parliament Issues page searches and sorts its issues in the browser, since
the titles and messages are translated there. Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { issueList } from '../parliament/vueapp/src/components/issueList.js';

const issues = [
  { id: 1, type: 'esDown', severity: 'red', cluster: 'east', title: 'ES Down', message: 'ES is down: timeout', firstNoticed: 30 },
  { id: 2, type: 'outOfDate', severity: 'red', cluster: 'west', node: 'node9', title: 'Out of Date', message: 'node9 has not checked in since: x', firstNoticed: 10, acknowledged: 5 },
  { id: 3, type: 'esDropped', severity: 'yellow', cluster: 'east', node: 'node2', title: 'ES Dropped', message: 'node2 ES is dropping bulk inserts: 4', firstNoticed: 20 }
];
const german = { 1: { title: 'ES Ausfall', message: 'ES ist ausgefallen: timeout' }, 2: { title: 'Veraltet', message: 'node9 hat sich nicht gemeldet seit: x' }, 3: { title: 'ES Verloren', message: 'node2 ES verwirft Bulk-Inserts: 4' } };
const texts = (issue) => german[issue.id];
const ids = (opts) => issueList(issues, { texts, locale: 'de', ...opts }).map(issue => issue.id);

describe('issueList', () => {
  test('with no search or sort is the issues as fetched', () => {
    assert.deepEqual(ids({}), [1, 2, 3]);
  });

  test('searches the translated and the English strings, any case', () => {
    assert.deepEqual(ids({ term: 'AUSFALL' }), [1]);
    assert.deepEqual(ids({ term: 'gemeldet' }), [2]);
    assert.deepEqual(ids({ term: 'es down' }), [1]);
    assert.deepEqual(ids({ term: 'dropping' }), [3]);
    assert.deepEqual(ids({ term: 'node' }), [2, 3]);
    assert.deepEqual(ids({ term: 'east' }), [1, 3]);
    assert.deepEqual(ids({ term: 'yellow' }), [3]);
    assert.deepEqual(ids({ term: 'nothing' }), []);
  });

  test('sorts titles by their translation', () => {
    assert.deepEqual(ids({ sort: 'title', order: 'asc' }), [1, 3, 2]);
    assert.deepEqual(ids({ sort: 'title', order: 'desc' }), [2, 3, 1]);
  });

  test('sorts numbers as numbers and a missing value as 0 or empty', () => {
    assert.deepEqual(ids({ sort: 'firstNoticed', order: 'asc' }), [2, 3, 1]);
    assert.deepEqual(ids({ sort: 'firstNoticed' }), [1, 3, 2]);
    assert.deepEqual(ids({ sort: 'acknowledged', order: 'desc' })[0], 2);
    assert.deepEqual(ids({ sort: 'node', order: 'asc' }), [1, 3, 2]);
  });

  test('sorts with a locale the runtime does not know and leaves the input alone', () => {
    assert.deepEqual(ids({ sort: 'cluster', order: 'asc', locale: 'x-pl', term: 'node' }), [3, 2]);
    assert.deepEqual(issues.map(issue => issue.id), [1, 2, 3]);
  });
});
