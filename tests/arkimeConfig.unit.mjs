/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Unit tests for the ArkimeConfig config uri redaction and the upload command
helpers. Run with `npm run test:unit`.

The config can be loaded from a uri with credentials in it, so anything that
shows or logs the config location must not leak them.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const cjsRequire = createRequire(import.meta.url);
const ArkimeConfig = cjsRequire('../common/arkimeConfig.js');
const ArkimeUtil = cjsRequire('../common/arkimeUtil.js');

describe('redactUri', () => {
  test('strips userinfo from uris', () => {
    assert.equal(ArkimeConfig.redactUri('elasticsearch://user:pass@es:9200/idx/_doc/cfg'), 'elasticsearch://****@es:9200/idx/_doc/cfg');
    assert.equal(ArkimeConfig.redactUri('redis://:secret@r:6379/0/k'), 'redis://****@r:6379/0/k');
    assert.equal(ArkimeConfig.redactUri('redis-sentinel://sp:rp@h1:1,h2:2/n/0/k'), 'redis-sentinel://****@h1:1,h2:2/n/0/k');
    assert.equal(ArkimeConfig.redactUri('opensearchs://a:b@c@h/i/_doc/d'), 'opensearchs://****@h/i/_doc/d');
    assert.equal(ArkimeConfig.redactUri('redis-sentinel://sp#1:rp?2x@h:1/n/0/k'), 'redis-sentinel://****@h:1/n/0/k');
  });

  test('leaves plain paths, uris without credentials and @ after the host alone', () => {
    assert.equal(ArkimeConfig.redactUri('/opt/arkime/etc/config.ini'), '/opt/arkime/etc/config.ini');
    assert.equal(ArkimeConfig.redactUri('https://es:9200/idx/_doc/cfg'), 'https://es:9200/idx/_doc/cfg');
    assert.equal(ArkimeConfig.redactUri('https://host?x=a@b'), 'https://host?x=a@b');
    assert.equal(ArkimeConfig.redactUri(undefined), undefined);
  });
});

describe('redactConfig', () => {
  const uri = 'elasticsearch://user:s3cr3t@es:9200/idx/_doc/cfg';

  test('redacts the uri', () => {
    assert.equal(ArkimeConfig.redactConfig(`capture -c ${uri} ok`, uri), 'capture -c elasticsearch://****@es:9200/idx/_doc/cfg ok');
  });

  test('redacts the uri as capture rewrites it', () => {
    assert.equal(ArkimeConfig.redactConfig('host: http://user:s3cr3t@es:9200', uri), 'host: http://****@es:9200');
  });

  test('leaves plain paths and the username readable', () => {
    assert.equal(ArkimeConfig.redactConfig('capture -c /opt/arkime/etc/config.ini', '/opt/arkime/etc/config.ini'), 'capture -c /opt/arkime/etc/config.ini');
    assert.equal(ArkimeConfig.redactConfig('user logged in', uri), 'user logged in');
  });

  test('handles a missing config or string', () => {
    assert.equal(ArkimeConfig.redactConfig('abc', undefined), 'abc');
    assert.equal(ArkimeConfig.redactConfig(undefined, uri), undefined);
  });
});

describe('expandPlaceholders', () => {
  test('substituted values cannot add placeholders', () => {
    assert.equal(ArkimeUtil.expandPlaceholders('a {NAME} {CONFIG}', { NAME: '{CONFIG}.pcap', CONFIG: 'secret' }), 'a {CONFIG}.pcap secret');
  });

  test('$ patterns in values are literal', () => {
    assert.equal(ArkimeUtil.expandPlaceholders('{NAME}', { NAME: "$&$`$'" }), "$&$`$'");
  });

  test('unknown placeholders are left alone', () => {
    assert.equal(ArkimeUtil.expandPlaceholders('{FOO} {NAME}', { NAME: 'x' }), '{FOO} x');
  });
});
