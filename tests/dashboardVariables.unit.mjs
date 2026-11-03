/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Unit tests for the dashboard variables (filter controls) in the viewer's
vueapp. Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { register } from 'node:module';

// the vueapp imports the shared filters through vite's @common alias
const common = new URL('../common/vueapp/', import.meta.url).href;
register(`data:text/javascript,${encodeURIComponent(`
export async function resolve (specifier, context, next) {
  if (specifier.startsWith('@common/')) { return next(${JSON.stringify(common)} + specifier.slice(8), context); }
  return next(specifier, context);
}`)}`);

const {
  defaultVariableName, controlVariable, urlVariableValues, dashboardVariables,
  controlFilter, controlFilters, substituteExpression, substituteTitle, withoutPicks, withoutVariableParams, VARIABLE_NAME_RE
} = await import('../viewer/vueapp/src/components/summary/widgets/dashboardVariables.js');

const vars = {
  node: { field: 'node', applyAll: true, values: ['nyc-cap1', 'nyc-cap2'] },
  proto: { field: 'protocols', applyAll: false, values: ['tls'] },
  host: { field: 'host.http', applyAll: true, values: [] }
};

describe('variable names', () => {
  test('come from the field when unset or invalid', () => {
    assert.equal(defaultVariableName('ip.src'), 'ip_src');
    assert.equal(defaultVariableName('node'), 'node');
    assert.equal(defaultVariableName('1x'), 'v_1x');
    assert.equal(controlVariable({ field: 'ip.dst', variable: '' }), 'ip_dst');
    assert.equal(controlVariable({ field: 'ip.dst', variable: 'bad name' }), 'ip_dst');
    assert.equal(controlVariable({ field: 'ip.dst', variable: 'dst' }), 'dst');
  });

  test('url values are a string or a repeated param, empty ones dropped', () => {
    assert.deepEqual(urlVariableValues({ 'var.node': 'a' }, 'node'), ['a']);
    assert.deepEqual(urlVariableValues({ 'var.node': ['a', '', 'b'] }, 'node'), ['a', 'b']);
    assert.deepEqual(urlVariableValues({}, 'node'), []);
  });

  test('the first control claiming a name owns it, others are ignored', () => {
    const widgets = [
      { viewMode: 'bar', field: 'node' },
      { id: 'c1', viewMode: 'control', field: 'node', applyAll: false },
      { id: 'c2', viewMode: 'control', field: 'ip.src', variable: 'node' },
      { viewMode: 'control', field: '' }
    ];
    assert.deepEqual(dashboardVariables(widgets, { 'var.node': 'x' }), { node: { field: 'node', applyAll: false, values: ['x'], owner: 'c1', kind: 'list' } });
  });

  test('__proto__ is not a variable name, so it never sets the prototype', () => {
    assert.ok(!VARIABLE_NAME_RE.test('__proto__'));
    assert.equal(controlVariable({ field: 'node', variable: '__proto__' }), 'node');
    const out = dashboardVariables([{ id: 'c', viewMode: 'control', field: 'node', variable: '__proto__' }], {});
    assert.equal(Object.getPrototypeOf(out), Object.prototype);
    assert.deepEqual(Object.keys(out), ['node']);
  });

  test('withoutVariableParams drops the named picks from the url, or all of them', () => {
    const query = { date: '-1', 'var.node': ['a', 'b'], 'var.proto': 'tls', variant: 'x' };
    assert.deepEqual(withoutVariableParams(query, ['node']), { date: '-1', 'var.proto': 'tls', variant: 'x' });
    assert.deepEqual(withoutVariableParams(query, ['nothere']), query);
    assert.deepEqual(withoutVariableParams(query), { date: '-1', variant: 'x' });
    assert.deepEqual(withoutVariableParams(undefined, ['node']), {});
    assert.notEqual(withoutVariableParams(query, ['node']), query);
  });

  test('withoutPicks treats one variable as unpicked and leaves the rest', () => {
    const out = withoutPicks(vars, 'node');
    assert.deepEqual(out.node.values, []);
    assert.deepEqual(out.proto.values, ['tls']);
    assert.deepEqual(vars.node.values, ['nyc-cap1', 'nyc-cap2']);
    assert.equal(withoutPicks(vars, undefined), vars);
  });
});

describe('control filters', () => {
  test('a list, quoted as needed, with odd values in their own clause', () => {
    assert.equal(controlFilter('node', ['a']), 'node == [a]');
    assert.equal(controlFilter('node', ['a', 'TLS 1.3']), 'node == [a,"TLS 1.3"]');
    assert.equal(controlFilter('host.http', ['a,b', 'c']), '(host.http == [c] || host.http == "a,b")');
  });

  test('only filter-everything controls with values, one left out on request', () => {
    assert.deepEqual(controlFilters(vars), ['node == [nyc-cap1,nyc-cap2]']);
    assert.deepEqual(controlFilters(vars, 'node'), []);
  });
});

describe('substitution', () => {
  test('fills ${name} with the values', () => {
    assert.equal(substituteExpression('node == ${node}', vars), 'node == [nyc-cap1,nyc-cap2]');
    assert.equal(substituteExpression('protocols == ${proto} && port.dst == 443', vars), 'protocols == [tls] && port.dst == 443');
    assert.equal(substituteExpression('node != ${node}', vars), 'node != [nyc-cap1,nyc-cap2]');
  });

  test('drops the && clause of a variable with nothing picked', () => {
    assert.equal(substituteExpression('host.http == ${host} && port.dst == 443', vars), 'port.dst == 443');
    assert.equal(substituteExpression('host.http == ${host}', vars), undefined);
    // a top level || keeps the expression whole, so it is all dropped
    assert.equal(substituteExpression('port.dst == 80 || host.http == ${host}', vars), undefined);
    // a quoted && doesn't split
    assert.equal(substituteExpression('http.uri == "a && b" && host.http == ${host}', vars), 'http.uri == "a && b"');
  });

  test('a ${name} in a quoted or regex value is text, and they don\'t split', () => {
    assert.equal(substituteExpression('http.uri == "a && tags == ${proto}"', vars), 'http.uri == "a && tags == ${proto}"');
    // the regex's paren isn't a group, so an unset variable's clause is the only one dropped
    assert.equal(substituteExpression('http.uri == /a(b/ && host.http == ${host}', vars), 'http.uri == /a(b/');
    assert.equal(substituteExpression('http.uri == /x && ${host}/', vars), 'http.uri == /x && ${host}/');
    // a CIDR's / isn't a regex
    assert.equal(substituteExpression('ip == 10.0.0.0/8 && node == ${node}', vars), 'ip == 10.0.0.0/8 && node == [nyc-cap1,nyc-cap2]');
  });

  test('leaves $shortcuts, unknown variables and plain text alone', () => {
    assert.equal(substituteExpression('ip.src == $myshortcut', vars), 'ip.src == $myshortcut');
    assert.equal(substituteExpression('ip.src == ${nope}', vars), 'ip.src == ${nope}');
    assert.equal(substituteExpression('${node}', vars), '${node}');
    assert.equal(substituteExpression('', vars), undefined);
  });

  test('titles show the values, or All', () => {
    assert.equal(substituteTitle('Sessions on ${node}', vars, 'All'), 'Sessions on nyc-cap1, nyc-cap2');
    assert.equal(substituteTitle('Hosts ${host}', vars, 'All'), 'Hosts All');
    assert.equal(substituteTitle('Plain', vars, 'All'), 'Plain');
  });
});

describe('range and text controls', () => {
  const rv = { field: 'port.dst', kind: 'range', applyAll: true, values: ['80', '443'] };
  const tv = { field: 'host.http', kind: 'text', applyAll: true, values: ['*.example.com'] };

  test('a range is between low and high, a != outside it', () => {
    assert.equal(controlFilter('port.dst', ['80', '443'], 'range'), '(port.dst >= 80 && port.dst <= 443)');
    assert.equal(controlFilter('port.dst', ['80', '443'], 'range', '!='), '(port.dst < 80 || port.dst > 443)');
  });

  test('text is quoted with its wildcards kept, quotes escaped', () => {
    assert.equal(controlFilter('host.http', ['*.example.com'], 'text'), 'host.http == "*.example.com"');
    assert.equal(controlFilter('host.http', ['a"b'], 'text'), 'host.http == "a\\"b"');
    // the parser unescapes only \", so a backslash is sent as it is
    assert.equal(controlFilter('http.uri', ['C:\\temp\\*'], 'text'), 'http.uri == "C:\\temp\\*"');
  });

  test('a backslash before a quote or the end can\'t end the value early', () => {
    // the lexer reads \ and the next character as one, so a run there is
    // doubled: C:\ would otherwise take the closing quote, a\" its escape
    assert.equal(controlFilter('http.uri', ['C:\\'], 'text'), 'http.uri == "C:\\\\"');
    assert.equal(controlFilter('http.uri', ['end\\\\'], 'text'), 'http.uri == "end\\\\\\\\"');
    assert.equal(controlFilter('host.http', ['a\\"b'], 'text'), 'host.http == "a\\\\\\"b"');
    // a crafted pick stays one quoted value, it can't add a condition
    assert.equal(controlFilter('host.http', ['a\\" || ip.src == "1.2.3.4'], 'text'),
      'host.http == "a\\\\\\" || ip.src == \\"1.2.3.4"');
  });

  test('the url keeps a range only as two ordered numbers, text as one value', () => {
    const w = [{ id: 'r', viewMode: 'control', field: 'port.dst', controlType: 'range' }, { id: 't', viewMode: 'control', field: 'host.http', controlType: 'text' }];
    assert.deepEqual(dashboardVariables(w, { 'var.port_dst': ['80', '443'], 'var.host_http': ['a', 'b'] }).port_dst.values, ['80', '443']);
    assert.deepEqual(dashboardVariables(w, { 'var.port_dst': ['443', '80'] }).port_dst.values, []);
    assert.deepEqual(dashboardVariables(w, { 'var.port_dst': ['80'] }).port_dst.values, []);
    assert.deepEqual(dashboardVariables(w, { 'var.port_dst': ['x', '9'] }).port_dst.values, []);
    assert.deepEqual(dashboardVariables(w, { 'var.host_http': ['a', 'b'] }).host_http.values, ['a']);
    assert.equal(dashboardVariables(w, {}).host_http.kind, 'text');
  });

  test('${name} and titles use the control type', () => {
    const vs = { port: rv, host: tv };
    assert.equal(substituteExpression('port.dst == ${port}', vs), '(port.dst >= 80 && port.dst <= 443)');
    assert.equal(substituteExpression('host.http != ${host}', vs), 'host.http != "*.example.com"');
    assert.equal(substituteTitle('Ports ${port} on ${host}', vs, 'All'), 'Ports 80–443 on *.example.com');
    assert.deepEqual(controlFilters(vs), ['(port.dst >= 80 && port.dst <= 443)', 'host.http == "*.example.com"']);
  });
});
