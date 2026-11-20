/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for viewer/viewerUtils.js. Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const cjsRequire = createRequire(import.meta.url);
const ViewerUtils = cjsRequire('../viewer/viewerUtils.js');

describe('ViewerUtils.isSafeSegment', () => {
  test('allows the session ids Db.session2Sid makes', () => {
    for (const sid of [
      '3@191021-abcdefghijklmnopqrstuvwxyz_-AB',
      '3@191021:191021-abcd',
      'x@other_index-2024.01.02:AbC_d-123'
    ]) {
      assert.equal(ViewerUtils.isSafeSegment(sid), true, sid);
    }
  });

  test('allows node names', () => {
    for (const node of ['test', 'sensor-01', 'sensor_01.example.com', 'a']) {
      assert.equal(ViewerUtils.isSafeSegment(node), true, node);
    }
  });

  test('refuses anything that would change the s2s path or ids body', () => {
    for (const bad of [
      '', '.', '..', 'a/b', '../a', 'a\\b', 'a?b=1', 'a#b', 'a&ids=b', 'a,b', 'a%2Fb', 'a b', 'a\nb', 'a\0b', 'é'
    ]) {
      assert.equal(ViewerUtils.isSafeSegment(bad), false, JSON.stringify(bad));
    }
  });

  test('refuses non strings', () => {
    for (const bad of [undefined, null, 1, true, ['a'], { toString: () => 'a' }]) {
      assert.equal(ViewerUtils.isSafeSegment(bad), false);
    }
  });
});
