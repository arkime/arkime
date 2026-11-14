/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for ArkimeUtil. Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { Readable } from 'node:stream';

const cjsRequire = createRequire(import.meta.url);
const ArkimeUtil = cjsRequire('../common/arkimeUtil.js');

// What jsonParser does with a body, an error or the parsed json
function parse (body) {
  const req = Readable.from([Buffer.from(body)]);
  req.headers = { 'content-type': 'application/json', 'content-length': String(Buffer.byteLength(body)) };
  return new Promise((resolve) => {
    ArkimeUtil.jsonParser(req, {}, (err) => resolve(err ?? req.body));
  });
}

describe('ArkimeUtil jsonParser', () => {
  test('nesting deeper than 32 is refused', async () => {
    assert.deepEqual(await parse('['.repeat(32) + ']'.repeat(32)), JSON.parse('['.repeat(32) + ']'.repeat(32)));
    const err = await parse('{"a":'.repeat(33) + '1' + '}'.repeat(33));
    assert.equal(err.status, 400);
    assert.match(err.message, /nested deeper than 32/);
  });

  test('brackets inside strings are not nesting', async () => {
    const body = JSON.stringify({ s: '['.repeat(100) + '\\"{'.repeat(50) });
    assert.deepEqual(await parse(body), JSON.parse(body));
  });
});
