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

  test('leaves plain paths and uris without credentials alone', () => {
    assert.equal(ArkimeConfig.redactUri('/opt/arkime/etc/config.ini'), '/opt/arkime/etc/config.ini');
    assert.equal(ArkimeConfig.redactUri('https://es:9200/idx/_doc/cfg'), 'https://es:9200/idx/_doc/cfg');
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

describe('redactUri query', () => {
  test('strips query values but not the path', () => {
    assert.equal(ArkimeConfig.redactUri('https://cfg.example.com/c.json?token=abc123&format=json'), 'https://cfg.example.com/c.json?token=****&format=****');
    assert.equal(ArkimeConfig.redactUri('https://es/a&b=c/d.json?x=1'), 'https://es/a&b=c/d.json?x=****');
    assert.equal(ArkimeConfig.redactUri('https://host?token=a@b'), 'https://host?token=****');
  });
});

describe('redactConfig passwords', () => {
  test('redacts the password in any form', () => {
    const uri = 'elasticsearch://elastic:p:w%40d@es:9200/idx/_doc/cfg';
    const out = ArkimeConfig.redactConfig(`decoded p:w@d raw p:w%40d encoded ${encodeURIComponent('p:w@d')} ${encodeURIComponent(uri)}`, uri);
    for (const secret of ['p:w@d', 'p:w%40d', 'p%3Aw%40d', 'p%3Aw%2540d']) {
      assert.ok(!out.includes(secret), `${secret} in ${out}`);
    }
  });

  test('keeps the username readable', () => {
    assert.equal(ArkimeConfig.redactConfig('/opt/arkime/bin', 'https://arkime@cfg/c.json'), '/opt/arkime/bin');
  });

  test('redacts both redis-sentinel passwords, longest first', () => {
    assert.equal(ArkimeConfig.redactConfig('x abcd abcdef y', 'redis-sentinel://abcd:abcdef@h:1/n/0/k'), 'x **** **** y');
    assert.equal(ArkimeConfig.redactConfig('x sp#1x rp?2x y', 'redis-sentinel://sp#1x:rp?2x@h:1/n/0/k'), 'x **** **** y');
  });

  test('redacts the query as capture prints it', () => {
    const uri = 'https://cfg.example.com/c.json?token=abc123';
    assert.equal(ArkimeConfig.redactConfig('host: https://cfg.example.com url: /c.json?token=abc123', uri), 'host: https://cfg.example.com url: /c.json?token=****');
  });

  test('skips short passwords that would mangle the output', () => {
    assert.equal(ArkimeConfig.redactConfig('Loaded /opt/arkime/etc/a.ini', 'redis://:a@r/0/k'), 'Loaded /opt/arkime/etc/a.ini');
  });

  test('handles passwords that are not valid uri encoding', () => {
    assert.equal(ArkimeConfig.redactConfig('pw 100%x', 'elasticsearch://u:100%x@h/i/_doc/c'), 'pw ****');
    assert.equal(ArkimeConfig.redactConfig('ok', 'elasticsearch://u:ab\ud800cd@h/i/_doc/c'), 'ok');
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
