/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for the config page's redaction. Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const cjsRequire = createRequire(import.meta.url);
const ViewConfig = cjsRequire('../common/viewConfig.js');

describe('ViewConfig unredactValue', () => {
  const url = 'postgres://u:pw1@db1/arkime';

  test('an unchanged value is the stored one', () => {
    const { value } = ViewConfig.redactValue('usersDbUrl', url);
    assert.equal(value, 'postgres://u:[redacted]@db1/arkime');
    assert.equal(ViewConfig.unredactValue('usersDbUrl', value, url), url);
    assert.equal(ViewConfig.unredactValue('apiKey', '[redacted]', 'abc'), 'abc');
  });

  test('an edited value keeps the secrets it still has [redacted]', () => {
    assert.equal(ViewConfig.unredactValue('usersDbUrl', 'postgres://u:[redacted]@db2/arkime', url), 'postgres://u:pw1@db2/arkime');
    const remote = 'url:https://a:p2@h;passwordSecret:sec;name:T';
    assert.equal(ViewConfig.unredactValue('remote', 'url:https://a:[redacted]@h;passwordSecret:[redacted];name:X', remote),
      'url:https://a:p2@h;passwordSecret:sec;name:X');
  });

  test('a retyped secret is saved as typed', () => {
    assert.equal(ViewConfig.unredactValue('usersDbUrl', 'postgres://u:new@db2/arkime', url), 'postgres://u:new@db2/arkime');
  });

  test('a [redacted] that no longer lines up is refused', () => {
    assert.throws(() => ViewConfig.unredactValue('usersDbUrl', 'postgres://u:[redacted]@a,postgres://u:[redacted]@b', url), /can't be matched/);
    assert.throws(() => ViewConfig.unredactValue('apiKey', '[redacted]x', 'abc'), /can't be matched/);
  });
});
