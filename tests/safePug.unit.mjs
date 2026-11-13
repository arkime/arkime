/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for viewer/safePug.js, the check WISE views must pass before the viewer
uses them. Run with `npm run test:unit`.
*/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const cjsRequire = createRequire(import.meta.url);
const { checkView } = cjsRequire('../viewer/safePug.js');

// Indented like viewer.js does before checking
const check = (view) => checkView(view.replace(/^/mg, '    '));

const GOOD = {
  'require view': "if (session.misp)\n  h4.session-card-title MISP\n  dl\n    +arrayList(session.misp, 'server', 'Server', 'misp.server')",
  'tests/email.wise': "if (session.wise)\n  div.sessionDetailMeta.bold Wise\n  dl.sessionDetailMeta\n    +arrayList(session.wise, 'str', 'Str', 'wise.str')",
  'malcolm style': "if (session.event.hash)\n  div.sessionDetailMeta.bold Malcolm\n  dl.sessionDetailMeta(suffix=\"IDs and Basic Connection Info\")\n    +arrayList(session.source.geo, 'city_name', 'City', 'source.geo.city_name')",
  'else, unless and comments': "unless session.x\n  //- hidden\n  // shown\n  span.a.b(title='t') hi #[b bold]\nelse if (session.a && !session.b) || session.c\n  dl\n    dt x\n    dd y\nelse\n  | plain & text"
};

const BAD = {
  'unbuffered code': 'if (session.x)\n  - process.exit()',
  'buffered code': 'div= process.pid',
  'interpolation': 'div #{process.pid}',
  'unescaped interpolation': 'div !{session.x}',
  'vue interpolation': 'div {{ 1+1 }}',
  'vue interpolation in arg': "+arrayList(session.x, 'a', '{{1}}', 'c')",
  'html text': 'div <img src=x onerror=alert(1)>',
  'html in comment': '// --><img src=x>',
  'script tag': 'script alert(1)',
  'vue component': 'v-menu x',
  'event attribute': 'div(onclick="alert(1)") x',
  'vue attribute': 'div(v-html="x") x',
  'unescaped attribute': "div(title!='x') x",
  'attribute expression': 'div(title=process.pid) x',
  '&attributes': 'div&attributes({onclick: "x"}) y',
  'call in if': "if (session.constructor.constructor('return process')())\n  div x",
  'comment in if': 'if (session.x /* y */)\n  div x',
  'call in args': "+arrayList(session, require('fs'), 'a', 'b')",
  'computed member': "+arrayList(session['constructor'], 'a', 'b', 'c')",
  'proto property': "+arrayList(session.__proto__, 'a', 'b', 'c')",
  'other identifier': "+arrayList(process.env, 'a', 'b', 'c')",
  'string escape': "+arrayList(session.x, 'a\\x3c', 'b', 'c')",
  'raw html extra arg': "+arrayList(session.x, 'a', 'b', 'c', session.http.uri)",
  'spread arg': '+arrayList(...session.x)',
  'other mixin': '+customViewFields(session, x)',
  'mixin definition': 'mixin arrayList(a)\n  div x',
  'mixin block': "+arrayList(session.x, 'a', 'b', 'c')\n  div x",
  'include': 'include /etc/passwd',
  'extends': 'extends /etc/passwd',
  'filter': ':markdown-it\n  # hi',
  'each': 'each v in session.x\n  div x',
  'case': 'case session.x\n  when 1\n    div x',
  'named block': 'block foo\n  div x',
  'doctype': 'doctype html',
  'tabs': 'if (session.x)\n\tdiv x',
  // What a dl. text block becomes after the old whole template card transform
  'code after card transform': 'if (session.a)\n  span x\n    div.session-detail-card\n      h4.session-card-title x\n  dl.\n  - process.exit()'
};

for (const [desc, view] of Object.entries(GOOD)) {
  test(`allows ${desc}`, () => {
    assert.equal(check(view), undefined);
  });
}

for (const [desc, view] of Object.entries(BAD)) {
  test(`rejects ${desc}`, () => {
    assert.equal(typeof check(view), 'string');
  });
}
