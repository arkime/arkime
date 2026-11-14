/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Unit tests for building search expressions from captured values: whatever
the value, clicking it must search for exactly that value. Each expression
goes through the real search parser. Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { buildExpression, buildListExpression } from '../common/vueapp/vueFilters.js';

const cjsRequire = createRequire(import.meta.url);
const { parser } = cjsRequire('../viewer/arkimeparser.js');

const parse = (expression) => {
  parser.yy = {
    fieldsMap: { 'host.http': { dbField: 'http.host', exp: 'host.http', type: 'termfield' } },
    dbFieldsMap: {},
    prefix: '',
    emailSearch: false,
    lookups: {},
    lookupTypeMap: {},
    shortcuts: { evil: { _source: { string: ['x'] } } },
    shortcutTypeMap: { termfield: 'string' }
  };
  return parser.parse(expression);
};

const term = (value) => ({ term: { 'http.host': value } });
const exact = (value) => parse(buildExpression('host.http', value, '=='));

describe('buildExpression', () => {
  test('plain values stay bare', () => {
    assert.equal(buildExpression('host.http', 'example.com', '=='), 'host.http == example.com');
    assert.deepEqual(exact('example.com'), term('example.com'));
  });

  test('a value that looks like a regex is a term', () => {
    assert.deepEqual(exact('/.*/'), { wildcard: { 'http.host': '/.\\*/' } });
    assert.deepEqual(exact('/evil/'), term('/evil/'));
  });

  test('a * matches only itself', () => {
    assert.equal(buildExpression('host.http', '*.example.com', '=='), 'host.http == "\\*.example.com"');
    assert.deepEqual(exact('*.example.com'), { wildcard: { 'http.host': '\\*.example.com' } });
    // the ? and \ that wildcard would read are escaped too
    assert.deepEqual(exact('a?b*c\\d'), { wildcard: { 'http.host': 'a\\?b\\*c\\\\d' } });
    assert.deepEqual(exact('a"b*'), { wildcard: { 'http.host': 'a"b\\*' } });
  });

  test('a ? without a * is a plain term', () => {
    assert.deepEqual(exact('a?b'), term('a?b'));
  });

  test('a captured EXISTS! is a value, not an exists check', () => {
    assert.equal(buildExpression('host.http', 'EXISTS!', '=='), 'host.http == "EXISTS!"');
    assert.deepEqual(exact('EXISTS!'), term('EXISTS!'));
  });

  test('an empty value searches for an empty string', () => {
    assert.equal(buildExpression('host.http', '', '=='), 'host.http == ""');
    assert.deepEqual(exact(''), term(''));
  });

  test('commas never pull in shortcuts', () => {
    assert.deepEqual(exact('a,$evil,b'), term('a,$evil,b'));
    assert.deepEqual(exact('a, $evil'), term('a, $evil'));
    assert.deepEqual(exact('$evil'), term('$evil'));
  });

  test('quotes and backslashes survive', () => {
    assert.deepEqual(exact('a"b'), term('a"b'));
    assert.deepEqual(exact('a\\b'), term('a\\b'));
    assert.deepEqual(exact('a\\'), term('a\\\\'));
  });
});

describe('buildListExpression', () => {
  test('a * value gets its own exact clause', () => {
    assert.deepEqual(parse(buildListExpression('host.http', ['a', '*.b'], '==')), {
      bool: { should: [{ terms: { 'http.host': ['a'] } }, { wildcard: { 'http.host': '\\*.b' } }] }
    });
  });

  test('plain values stay one list', () => {
    assert.deepEqual(parse(buildListExpression('host.http', ['a', 'EXISTS!', ''], '==')), {
      terms: { 'http.host': ['a', 'EXISTS!', ''] }
    });
  });
});

describe('shortcuts typed by the user still work', () => {
  test('a bare $shortcut', () => {
    assert.equal(parse('host.http == $evil').terms['http.host'].path, 'string');
  });

  test('a $shortcut in a list', () => {
    const should = parse('host.http == [a,$evil]').bool.should;
    assert.deepEqual(should[0], { terms: { 'http.host': ['a'] } });
    assert.equal(should[1].terms['http.host'].path, 'string');
  });
});
