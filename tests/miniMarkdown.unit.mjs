/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for the dashboard text widget's mini markdown parser and link
sanitizer (viewer/vueapp/src/components/summary/widgets/miniMarkdown.js).
Run with `npm run test:unit`.
*/
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';

const { parseMarkdown, parseInline, safeLink, MAX_TEXT, MAX_HREF } =
  await import('../viewer/vueapp/src/components/summary/widgets/miniMarkdown.js');

// the widget on the summary page of an Arkime served under /arkime/
const SITE = {
  href: 'https://arkime.example.com/arkime/summary?date=-1',
  webBase: '/arkime/',
  pages: ['sessions', 'spiview', 'spigraph', 'arkime', 'summary', 'featherprint/tracked']
};
const md = (src) => parseMarkdown(src, SITE);
const inl = (src) => parseInline(src, SITE);
const text = (s) => ({ type: 'text', text: s });

// all text in a node tree, in order
const textOf = (nodes) => nodes.map(n => {
  if (n.type === 'text' || n.type === 'code') { return n.text; }
  if (n.type === 'br') { return '\n'; }
  if (n.type === 'list') { return n.items.map(textOf).join('\n'); }
  return n.children ? textOf(n.children) : '';
}).join('');

// every link node in a node tree
const linksOf = (nodes) => nodes.flatMap(n => [
  ...(n.type === 'link' ? [n] : []),
  ...(n.children ? linksOf(n.children) : []),
  ...(n.items ? n.items.flatMap(linksOf) : [])
]);

describe('parseMarkdown blocks', () => {
  test('empty and non string input', () => {
    assert.deepEqual(md(''), []);
    assert.deepEqual(md(undefined), []);
    assert.deepEqual(md(null), []);
    assert.deepEqual(md(42), []);
    assert.deepEqual(md({ text: 'x' }), []);
    assert.deepEqual(md('\n \n\t\n'), []);
  });

  test('paragraphs and line breaks', () => {
    assert.deepEqual(md('one\ntwo\n\n\nthree'), [
      { type: 'paragraph', children: [text('one'), { type: 'br' }, text('two')] },
      { type: 'paragraph', children: [text('three')] }
    ]);
    assert.deepEqual(md('a\r\nb\rc'), [
      { type: 'paragraph', children: [text('a'), { type: 'br' }, text('b'), { type: 'br' }, text('c')] }
    ]);
    // surrounding whitespace on a line is dropped
    assert.deepEqual(md('   indented   \n\tx'), [
      { type: 'paragraph', children: [text('indented'), { type: 'br' }, text('x')] }
    ]);
  });

  test('headings', () => {
    for (let level = 1; level <= 6; level++) {
      assert.deepEqual(md(`${'#'.repeat(level)} Title`), [{ type: 'heading', level, children: [text('Title')] }]);
    }
    assert.deepEqual(md('## Closed ##'), [{ type: 'heading', level: 2, children: [text('Closed')] }]);
    assert.deepEqual(md('## C#'), [{ type: 'heading', level: 2, children: [text('C#')] }]);
    assert.deepEqual(md('####### seven'), [{ type: 'paragraph', children: [text('####### seven')] }]);
    assert.deepEqual(md('#hashtag'), [{ type: 'paragraph', children: [text('#hashtag')] }]);
    assert.deepEqual(md('para\n# Head\nmore'), [
      { type: 'paragraph', children: [text('para')] },
      { type: 'heading', level: 1, children: [text('Head')] },
      { type: 'paragraph', children: [text('more')] }
    ]);
  });

  test('horizontal rules', () => {
    for (const hr of ['---', '***', '___', '- - -', '-----', ' * * * ']) {
      assert.deepEqual(md(hr), [{ type: 'hr' }], hr);
    }
    assert.deepEqual(md('a\n---\nb'), [
      { type: 'paragraph', children: [text('a')] },
      { type: 'hr' },
      { type: 'paragraph', children: [text('b')] }
    ]);
    assert.equal(md('--')[0].type, 'paragraph');
    assert.equal(md('-*-')[0].type, 'paragraph');
  });

  test('bullet lists', () => {
    const list = { type: 'list', ordered: false, start: 1, items: [[text('a')], [text('b')], [text('c')]] };
    assert.deepEqual(md('- a\n* b\n+ c'), [list]);
    // a blank line between items keeps the list, nested items are flattened
    assert.deepEqual(md('- a\n\n- b\n    - c'), [list]);
    // a following line continues the item, a blank line then text ends the list
    assert.deepEqual(md('- a\nmore\n\nafter'), [
      { type: 'list', ordered: false, start: 1, items: [[text('a'), { type: 'br' }, text('more')]] },
      { type: 'paragraph', children: [text('after')] }
    ]);
    // a list can interrupt a paragraph
    assert.deepEqual(md('Intro:\n- x'), [
      { type: 'paragraph', children: [text('Intro:')] },
      { type: 'list', ordered: false, start: 1, items: [[text('x')]] }
    ]);
    // not list items
    assert.equal(md('-no space')[0].type, 'paragraph');
    assert.equal(md('**bold** line')[0].type, 'paragraph');
    assert.equal(md('+1 agreed')[0].type, 'paragraph');
  });

  test('ordered lists', () => {
    assert.deepEqual(md('1. a\n2. b'), [
      { type: 'list', ordered: true, start: 1, items: [[text('a')], [text('b')]] }
    ]);
    assert.deepEqual(md('3) a\n4) b'), [
      { type: 'list', ordered: true, start: 3, items: [[text('a')], [text('b')]] }
    ]);
    // switching between bullet and ordered starts a new list
    assert.deepEqual(md('- a\n1. b'), [
      { type: 'list', ordered: false, start: 1, items: [[text('a')]] },
      { type: 'list', ordered: true, start: 1, items: [[text('b')]] }
    ]);
    assert.equal(md('1234567890. too long')[0].type, 'paragraph');
  });

  test('inline markup in headings and list items', () => {
    assert.deepEqual(md('## **Bold** `code`'), [{
      type: 'heading',
      level: 2,
      children: [{ type: 'strong', children: [text('Bold')] }, text(' '), { type: 'code', text: 'code' }]
    }]);
    assert.deepEqual(md('- *it*'), [
      { type: 'list', ordered: false, start: 1, items: [[{ type: 'em', children: [text('it')] }]] }
    ]);
  });

  test('only the first MAX_TEXT chars are parsed', () => {
    const blocks = md('x'.repeat(MAX_TEXT + 5000));
    assert.equal(textOf(blocks).length, MAX_TEXT);
    assert.equal(md('a'.repeat(MAX_TEXT) + '\n\n# late')[0].type, 'paragraph');
    assert.equal(md('a'.repeat(MAX_TEXT) + '\n\n# late').length, 1);
  });
});

describe('parseInline', () => {
  test('plain text', () => {
    assert.deepEqual(inl('hello world'), [text('hello world')]);
    assert.deepEqual(inl(''), []);
  });

  test('bold, italic and code', () => {
    assert.deepEqual(inl('**b** *i* _u_ `c`'), [
      { type: 'strong', children: [text('b')] }, text(' '),
      { type: 'em', children: [text('i')] }, text(' '),
      { type: 'em', children: [text('u')] }, text(' '),
      { type: 'code', text: 'c' }
    ]);
    assert.deepEqual(inl('__b__'), [{ type: 'strong', children: [text('b')] }]);
    assert.deepEqual(inl('***both***'), [{ type: 'em', children: [{ type: 'strong', children: [text('both')] }] }]);
    assert.deepEqual(inl('**a *b* c**'), [{
      type: 'strong', children: [text('a '), { type: 'em', children: [text('b')] }, text(' c')]
    }]);
    assert.deepEqual(inl('in*word*s'), [text('in'), { type: 'em', children: [text('word')] }, text('s')]);
  });

  test('underscores inside words stay literal', () => {
    assert.deepEqual(inl('zeek.files.extracted_uri and snake_case_name'), [text('zeek.files.extracted_uri and snake_case_name')]);
  });

  test('unmatched or space surrounded delimiters stay literal', () => {
    assert.deepEqual(inl('**open'), [text('**open')]);
    assert.deepEqual(inl('close*'), [text('close*')]);
    assert.deepEqual(inl('a * b * c'), [text('a * b * c')]);
    assert.deepEqual(inl('2 * 3 = 6'), [text('2 * 3 = 6')]);
    assert.deepEqual(inl('**a*'), [text('*'), { type: 'em', children: [text('a')] }]);
    assert.deepEqual(inl('*a _b* c_'), [{ type: 'em', children: [text('a _b')] }, text(' c_')]);
  });

  test('code spans are literal', () => {
    assert.deepEqual(inl('`**not bold** [x](http://a.com)`'), [{ type: 'code', text: '**not bold** [x](http://a.com)' }]);
    assert.deepEqual(inl('`` a ` b ``'), [{ type: 'code', text: 'a ` b' }]);
    assert.deepEqual(inl('`<b>`'), [{ type: 'code', text: '<b>' }]);
    assert.deepEqual(inl('`unclosed'), [text('`unclosed')]);
    assert.deepEqual(inl('``a`'), [text('``a`')]);
    assert.deepEqual(inl('`a\nb`'), [{ type: 'code', text: 'a b' }]);
  });

  test('backslash escapes', () => {
    assert.deepEqual(inl('\\*not em\\* \\[x\\](y) \\`'), [text('*not em* [x](y) `')]);
    assert.deepEqual(inl('C:\\dir\\file'), [text('C:\\dir\\file')]);
  });

  test('line breaks', () => {
    assert.deepEqual(inl('a\nb'), [text('a'), { type: 'br' }, text('b')]);
  });

  test('raw html, images, autolinks, reference links and tables stay literal', () => {
    for (const s of [
      '<script>alert(1)</script>',
      '<img src=x onerror=alert(1)>',
      '<a href="javascript:alert(1)">x</a>',
      '<http://example.com>',
      '[ref][1]',
      '[ref]',
      '| a | b |',
      '&lt;b&gt; &#x3C;',
      '![alt](http://example.com/x.png)'
    ]) {
      assert.deepEqual(inl(s), [text(s)], s);
    }
    // a linked image: the image stays literal text inside the outer link
    const nodes = inl('[![img](http://a.com/i.png)](http://b.com/)');
    assert.deepEqual(linksOf(nodes).map(l => l.href), ['http://b.com/']);
    assert.equal(textOf(nodes), '![img](http://a.com/i.png)');
  });

  test('html in markdown blocks stays literal text', () => {
    const src = '# <h1>x</h1>\n\n<div onclick="x()">**hi**</div>\n\n- <iframe src="//evil.com">';
    const blocks = md(src);
    assert.equal(blocks[0].children[0].text, '<h1>x</h1>');
    assert.deepEqual(blocks[1].children, [
      text('<div onclick="x()">'), { type: 'strong', children: [text('hi')] }, text('</div>')
    ]);
    assert.deepEqual(blocks[2].items[0], [text('<iframe src="//evil.com">')]);
  });

  test('links', () => {
    assert.deepEqual(inl('see [docs](https://docs.zeek.org/x.html)!'), [
      text('see '),
      {
        type: 'link',
        href: 'https://docs.zeek.org/x.html',
        external: true,
        host: 'docs.zeek.org',
        children: [text('docs')]
      },
      text('!')
    ]);
    // link text may hold bold/italic/code
    const [link] = inl('[**b** _i_ `c`](https://a.com/)');
    assert.deepEqual(link.children, [
      { type: 'strong', children: [text('b')] }, text(' '),
      { type: 'em', children: [text('i')] }, text(' '),
      { type: 'code', text: 'c' }
    ]);
    // links with balanced parens in the url
    assert.equal(inl('[w](https://en.wikipedia.org/wiki/Foo_(bar))')[0].href, 'https://en.wikipedia.org/wiki/Foo_(bar)');
    // emphasis can't span a link boundary
    assert.deepEqual(textOf(inl('*a [b* c](https://a.com/)')), '*a b* c');
  });

  test('no links inside links', () => {
    const nodes = inl('[a [b](https://b.com/) c](https://c.com/)');
    assert.deepEqual(linksOf(nodes).map(l => l.href), ['https://b.com/']);
    assert.equal(textOf(nodes), '[a b c](https://c.com/)');
  });

  test('unsafe links keep only their text', () => {
    for (const href of ['javascript:alert(1)', 'JaVaScRiPt:alert(1)', 'data:text/html,<b>', '//evil.com/', 'https://u:p@evil.com/', 'vbscript:x',
      'mailto:soc@example.com', '/arkime/api/user', '../sessions']) {
      const nodes = inl(`[click **me**](${href})`);
      assert.deepEqual(linksOf(nodes), [], href);
      assert.deepEqual(nodes, [text('click '), { type: 'strong', children: [text('me')] }], href);
    }
    // a title or spaces in the destination aren't supported
    assert.deepEqual(inl('[t](https://a.com/ "title")'), [text('t')]);
    assert.deepEqual(inl('[t](https://a.com/ x)'), [text('t')]);
    // unterminated destinations aren't links
    assert.deepEqual(inl('[t](https://a.com/'), [text('[t](https://a.com/')]);
    assert.deepEqual(inl('[t](https://a.com/\n)'), [text('[t](https://a.com/'), { type: 'br' }, text(')')]);
  });

  test('every rendered link is http(s)', () => {
    const hostile = [
      '[a](javascript:alert(1))', '[a](java\tscript:alert(1))', '[a](java%0ascript:alert(1))',
      '[a](javascript&#58;alert(1))', '[a](&#106;avascript:alert(1))', '[a](%6Aavascript:alert(1))',
      '[a]( javascript:alert(1) )', '[a](<javascript:alert(1)>)', '[a](\\javascript:alert(1))',
      '[a](data:text/html;base64,PHNjcmlwdD4=)', '[a](file:///etc/passwd)', '[a](blob:https://a.com/x)',
      '[a](https://ok.com/)[b](javascript:x)', '[[a](javascript:x)](https://ok.com/)', '[a](mailto:a@b.com)'
    ];
    for (const src of hostile) {
      for (const link of linksOf(md(src))) {
        assert.match(link.href, /^https?:/, src);
      }
    }
  });

  test('deep nesting is bounded', () => {
    const deep = '['.repeat(100) + '*'.repeat(100) + 'x';
    assert.equal(textOf(inl(deep)), deep);
    const nested = '*a '.repeat(100) + 'x' + ' a*'.repeat(100);
    assert.equal(textOf(inl(nested)).replace(/\*/g, ''), nested.replace(/\*/g, ''));
  });

  test('one long delimiter run cannot nest deeper than the limit', () => {
    const depth = (nodes) => Math.max(0, ...nodes.map(n => n.children ? 1 + depth(n.children) : 0));
    // the longest runs that still fit in MAX_TEXT with the link around them
    for (const k of [40, MAX_TEXT / 8, MAX_TEXT / 2 - 10]) {
      for (const src of ['*'.repeat(k) + 'a' + '*'.repeat(k), '_'.repeat(k) + 'a' + '_'.repeat(k),
        '[' + '*'.repeat(k) + 'a' + '*'.repeat(k) + '](https://ok.com/)']) {
        const nodes = md(src);
        assert.ok(depth(nodes) <= 20, `depth ${depth(nodes)} for ${k}`);
        assert.equal(textOf(nodes).replace(/[*_]/g, ''), 'a');
      }
    }
  });

  test('hostile input parses quickly', () => {
    let backticks = '';
    for (let k = 1; backticks.length < MAX_TEXT; k++) { backticks += '`'.repeat(k) + 'a'; }
    const inputs = [
      '['.repeat(MAX_TEXT), ']'.repeat(MAX_TEXT), '*'.repeat(MAX_TEXT), '_a'.repeat(MAX_TEXT / 2),
      '*_'.repeat(MAX_TEXT / 2), '[a](' + 'x'.repeat(MAX_TEXT), '[](x'.repeat(MAX_TEXT / 4),
      '](('.repeat(MAX_TEXT / 3), '`'.repeat(MAX_TEXT), backticks, '# ' + ' '.repeat(MAX_TEXT) + '#',
      '- '.repeat(MAX_TEXT / 2), '\n'.repeat(MAX_TEXT), '\\'.repeat(MAX_TEXT), '![['.repeat(MAX_TEXT / 3)
    ];
    for (const src of inputs) {
      const start = Date.now();
      md(src.repeat(3)); // past MAX_TEXT too
      assert.ok(Date.now() - start < 1000, `slow on ${JSON.stringify(src.slice(0, 12))}`);
    }
  });

  test('kibana dashboard markdown', () => {
    assert.deepEqual(md('#### Extracted File Downloads\n\nClick the `zeek.files.extracted_uri` value to download the file'), [
      { type: 'heading', level: 4, children: [text('Extracted File Downloads')] },
      {
        type: 'paragraph',
        children: [text('Click the '), { type: 'code', text: 'zeek.files.extracted_uri' }, text(' value to download the file')]
      }
    ]);
    assert.deepEqual(md("Some fields aren't indexed. Click **JSON** to view")[0].children, [
      text("Some fields aren't indexed. Click "), { type: 'strong', children: [text('JSON')] }, text(' to view')
    ]);
    const [sig] = inl('[`signatures.log`](https://docs.zeek.org/en/master/frameworks/signatures.html)');
    assert.equal(sig.href, 'https://docs.zeek.org/en/master/frameworks/signatures.html');
    assert.deepEqual(sig.children, [{ type: 'code', text: 'signatures.log' }]);

    const nav = md('### [Overview](#/dashboard/0ad3d7c2-3441) ● [Files](#/dashboard/248cae60-1d5f) ● [Docs](https://docs.zeek.org/)');
    assert.equal(nav[0].type, 'heading');
    const links = linksOf(nav);
    assert.deepEqual(links.map(l => [l.href, l.external]), [
      ['https://arkime.example.com/arkime/summary?date=-1#/dashboard/0ad3d7c2-3441', false],
      ['https://arkime.example.com/arkime/summary?date=-1#/dashboard/248cae60-1d5f', false],
      ['https://docs.zeek.org/', true]
    ]);
    assert.equal(textOf(nav), 'Overview ● Files ● Docs');
  });
});

describe('safeLink', () => {
  const ok = (href, expected, site = SITE) => assert.deepEqual(safeLink(href, site), expected, href);
  const no = (href, site = SITE) => assert.equal(safeLink(href, site), null, JSON.stringify(href));
  const page = (href, abs, site = SITE) => ok(href, { href: abs, external: false, host: new URL(site.href).host }, site);

  test('absolute http and https links', () => {
    ok('https://docs.zeek.org/en/master/', { href: 'https://docs.zeek.org/en/master/', external: true, host: 'docs.zeek.org' });
    ok('http://example.com', { href: 'http://example.com/', external: true, host: 'example.com' });
    // the normalized url is used, not the raw string
    ok('HTTPS://Docs.Zeek.ORG:443/b?q=1#f', { href: 'https://docs.zeek.org/b?q=1#f', external: true, host: 'docs.zeek.org' });
    ok('https:example.com/x', { href: 'https://example.com/x', external: true, host: 'example.com' });
    ok('  https://example.com/  ', { href: 'https://example.com/', external: true, host: 'example.com' });
    // unicode hosts show as punycode
    assert.equal(safeLink('https://ex\u0430mple.com/', SITE).host, 'xn--exmple-4nf.com');
    // non ascii in the path is percent encoded
    assert.equal(safeLink('https://a.com/\u00fc', SITE).href, 'https://a.com/%C3%BC');
    // other sites' api paths are just external links
    assert.ok(safeLink('https://docs.example.com/api/x', SITE).external);
  });

  test('mailto and every other scheme are rejected in any case or encoding', () => {
    for (const href of [
      'mailto:soc@example.com', 'MAILTO:soc@example.com', 'mailto:soc@example.com?subject=hi',
      'javascript:alert(1)', 'JaVaScRiPt:alert(1)', 'JAVASCRIPT:alert(1)', 'javascript://%0aalert(1)',
      'java\nscript:alert(1)', 'java\tscript:alert(1)', 'java\rscript:alert(1)', 'java\u0000script:alert(1)',
      ' javascript:alert(1)', '\u0001javascript:alert(1)', 'java%0ascript:alert(1)', 'java%09script:alert(1)',
      '%6Aavascript:alert(1)', 'javascript&#58;alert(1)', 'javascript&#x3A;alert(1)',
      'javascript&colon;alert(1)', '&#106;avascript:alert(1)', '&#x6A;avascript:alert(1)', 'jav&#x09;ascript:alert(1)',
      'vbscript:msgbox(1)', 'VBScript:msgbox(1)', 'data:text/html,<script>alert(1)</script>',
      'data:text/html;base64,PHNjcmlwdD5hbGVydCgxKTwvc2NyaXB0Pg==', 'DATA:image/png;base64,AAAA',
      'file:///etc/passwd', 'FILE://c:/windows', 'blob:https://arkime.example.com/1234', 'ftp://example.com/',
      'about:blank', 'chrome://settings', 'intent://x#Intent;end', 'ws://example.com/', 'tel:5551212',
      'sms:5551212', 'view-source:https://a.com/', 'x-javascript:alert(1)', 'jav ascript:alert(1)',
      'javascript\u200b:alert(1)', 'java\u00adscript:alert(1)',
      // without a literal ':' it is a path on this site, and no app page
      'javascript%3Aalert(1)'
    ]) {
      no(href);
    }
  });

  test('control chars, whitespace, backslash and quote characters are rejected', () => {
    for (const c of ['\u0000', '\u0008', '\t', '\n', '\r', '\u001f', '\u007f', '\u0085', '\u009f', ' ',
      '\u00a0', '\u2028', '\u3000', '\u200b', '\u200e', '\u202e', '\u2066', '\ufeff',
      '\\', '<', '>', '"', "'", '`']) {
      no(`https://example.com/a${c}b`);
      no(`/arkime/sessions?a${c}b`);
    }
    no('https://example.com/a&#x27;b');
    no('https://example.com/?a=1&amp;b=2');
    assert.ok(safeLink('https://example.com/?a=1&b=2', SITE)); // a plain & is fine
  });

  test('credentials are rejected', () => {
    no('https://user:pass@example.com/');
    no('https://user@example.com/');
    no('https://:pass@example.com/');
    no('https://arkime.example.com@evil.com/');
    no('http://evil.com%2F@arkime.example.com/');
    // relative links can't pick up credentials from the page either
    no('/arkime/sessions', { ...SITE, href: 'https://u:p@arkime.example.com/arkime/summary' });
  });

  test('protocol relative and backslash host tricks are rejected', () => {
    no('//evil.com');
    no('//evil.com/path');
    no('///evil.com');
    no('/\\evil.com');
    no('\\\\evil.com');
    no('\\/evil.com');
    no('https:\\\\evil.com');
  });

  test('links on this site open its pages', () => {
    page('/arkime/sessions?expression=ip%3D10.0.0.1', 'https://arkime.example.com/arkime/sessions?expression=ip%3D10.0.0.1');
    page('sessions', 'https://arkime.example.com/arkime/sessions');
    page('/arkime/sessions/', 'https://arkime.example.com/arkime/sessions/');
    page('/arkime/featherprint/tracked', 'https://arkime.example.com/arkime/featherprint/tracked');
    page('/arkime/', 'https://arkime.example.com/arkime/');
    page('/arkime', 'https://arkime.example.com/arkime');
    page('https://arkime.example.com/arkime/spigraph?exp=ip.src', 'https://arkime.example.com/arkime/spigraph?exp=ip.src');
    // a query or hash stays on the current page
    page('?date=24', 'https://arkime.example.com/arkime/summary?date=24');
    page('?q=a:b', 'https://arkime.example.com/arkime/summary?q=a:b');
    page('#frag', 'https://arkime.example.com/arkime/summary?date=-1#frag');
    page('#/dashboard/248cae60-1d5f-11ec-9e3b-a9bcc9a9e3b4', 'https://arkime.example.com/arkime/summary?date=-1#/dashboard/248cae60-1d5f-11ec-9e3b-a9bcc9a9e3b4');
    // a colon in the first path segment would read as a scheme
    no('foo:bar');
    no('a:b/c');
  });

  test('links on this site never reach anything but a page', () => {
    for (const href of [
      // api endpoints, however spelled
      '/arkime/api/sessions/csv?expression=x', '/arkime/api/user', 'api/sessions', '/arkime/%61pi/user', '/arkime/API/user',
      'https://arkime.example.com/arkime/api/sessions/pcap', '/api/user',
      // legacy routes, files and paths under a page
      '/arkime/sessions.csv', '/arkime/sessions.pcap', '/arkime/sessions/x', '/arkime/spigraph.json',
      '/arkime/test/session/1-abc/pcap', '/arkime/featherprint/tracked/x', '/arkime/%73essions',
      // outside the web base path
      '/sessions', '/', '/other/sessions', '/arkimex/sessions', '/arkime2/', '/%2F/evil.com', '/arkime%2Fsessions',
      // the web base path again inside the path
      '/arkime/arkime/api/user'
    ]) {
      no(href);
    }
  });

  test('dot segments are rejected in any spelling', () => {
    for (const href of [
      '..', '.', './sessions', '../sessions', '../../arkime/sessions', 'sessions/..', 'sessions/.',
      '/arkime/x/../sessions', '/arkime/./sessions', '/arkime/sessions/..', '/arkime/sessions/../',
      '/arkime/%2e%2e/api/user', '/arkime/%2E%2E/sessions', '/arkime/%2e/sessions', '/arkime/.%2e/sessions',
      '/arkime/%2e./sessions', '%2e%2e/sessions', 'https://arkime.example.com/arkime/x/../sessions',
      'https://docs.zeek.org/a/../b', '../x?y=1', '..#x'
    ]) {
      no(href);
    }
    // dots in a query, a hash or a name are fine
    assert.ok(safeLink('/arkime/sessions?expression=host.http==a..b', SITE));
    assert.ok(safeLink('/arkime/sessions#../x', SITE));
    assert.ok(safeLink('https://docs.zeek.org/a.b/c..d', SITE));
  });

  test('this host on another scheme or port is never an outside site', () => {
    no('http://arkime.example.com/arkime/sessions');
    no('https://arkime.example.com:8443/arkime/sessions');
    no('https://arkime.example.com:8081/sources');
    no('http://arkime.example.com:8005/test/session/1/pcap');
    // another host on the same domain is
    assert.ok(safeLink('https://cont3xt.example.com/', SITE).external);
  });

  test('the viewer\'s api is refused however the server is named', () => {
    for (const href of [
      'https://10.0.0.5/arkime/api/sessions/pcap?expression=x', 'http://arkime.example.com./arkime/api/user',
      'https://arkime-alias.example.com/arkime/API/user', 'https://[::1]/arkime/api/user', 'https://10.0.0.5/arkime/%61pi/user',
      'https://10.0.0.5/arkime//api/user', 'https://10.0.0.5/arkime/api', 'https://10.0.0.5/arkime/api?x=1', 'https://10.0.0.5/arkime%2Fapi/user'
    ]) {
      no(href);
    }
    // and so are its pre-/api aliases
    for (const href of [
      'https://10.0.0.5/arkime/sessions.pcap?expression=x', 'https://10.0.0.5/arkime/sessions.pcapng', 'https://10.0.0.5/arkime/sessions.csv',
      'https://10.0.0.5/arkime/x/sessions.csv', 'https://10.0.0.5/arkime/unique.txt?exp=ip', 'https://10.0.0.5/arkime/MultiUnique.txt',
      'https://10.0.0.5/arkime/connections.csv', 'https://10.0.0.5/arkime/sessions.json', 'https://10.0.0.5/arkime/spigraph.json/',
      'https://10.0.0.5/arkime/test/raw/1-abc', 'https://10.0.0.5/arkime/test/1-abc/body/file/1/x.exe',
      'https://10.0.0.5/arkime/test/1-abc/bodypng/file/1/x', 'https://10.0.0.5/arkime/bodyHash/abc', 'https://10.0.0.5/arkime/decodings',
      // the sessions.csv/pcap routes' dot is a regex wildcard
      'https://10.0.0.5/arkime/sessions/csv?expression=x', 'https://10.0.0.5/arkime/sessions_pcap', 'https://10.0.0.5/arkime/x/sessionsXcsv',
      'https://10.0.0.5/arkime/cyberchef/test/session/1-abc'
    ]) {
      no(href);
    }
    assert.ok(safeLink('https://docs.example.com/arkime/sessions.html', SITE).external);
    assert.ok(safeLink('https://docs.example.com/arkime/stats', SITE).external);
    // a trailing dot is this host, and not this origin
    no('https://arkime.example.com./arkime/sessions');
    // another site's pages under the same base path are just links; only the api is refused
    assert.ok(safeLink('https://docs.example.com/arkime/sessions', SITE).external);
    assert.ok(safeLink('https://docs.example.com/arkime/apis/x', SITE).external);
    assert.ok(safeLink('https://docs.example.com/v1/arkime/api/x', SITE).external);
    // at the web root that is any /api path
    const root = { href: 'https://a.example.com/', webBase: '/', pages: ['sessions'] };
    no('https://10.0.0.5/api/user', root);
    no('https://docs.example.com/api/x', root);
    assert.ok(safeLink('https://docs.example.com/v1/api/x', root).external);
  });

  test('a site at the web root', () => {
    const root = { href: 'https://a.example.com/arkime?date=1', webBase: '/', pages: ['sessions', 'arkime'] };
    page('/sessions', 'https://a.example.com/sessions', root);
    page('sessions?date=-1', 'https://a.example.com/sessions?date=-1', root);
    page('/', 'https://a.example.com/', root);
    page('#/dashboard/x', 'https://a.example.com/arkime?date=1#/dashboard/x', root);
    for (const href of ['/api/user', '/sessions.csv', '/test/session/1/pcap', '/help', '../sessions', '/%2e%2e/api']) {
      no(href, root);
    }
  });

  test('without the pages or web base no link on this site is kept', () => {
    for (const site of [{ href: SITE.href }, { href: SITE.href, webBase: '/arkime/' }, { href: SITE.href, pages: SITE.pages },
      { ...SITE, webBase: 'arkime' }, { ...SITE, pages: 'sessions' }]) {
      no('/arkime/sessions', site);
      no('#x', site);
      assert.ok(safeLink('https://docs.zeek.org/', site).external);
    }
  });

  test('links on this site need an http(s) page', () => {
    for (const href of [undefined, null, '', 'not a url', 'file:///tmp/x.html', 'data:text/html,x', 'about:blank']) {
      no('/arkime/sessions', { ...SITE, href });
      no('#x', { ...SITE, href });
    }
    // absolute links still work without one, and count as external
    for (const site of [undefined, null, {}]) {
      assert.deepEqual(safeLink('https://arkime.example.com/', site),
        { href: 'https://arkime.example.com/', external: true, host: 'arkime.example.com' });
    }
  });

  test('length limit and bad input', () => {
    const path = (len) => 'https://a.com/' + 'x'.repeat(len - 'https://a.com/'.length);
    assert.ok(safeLink(path(MAX_HREF), SITE));
    no(path(MAX_HREF + 1));
    no('/' + 'x'.repeat(MAX_HREF));
    // a short raw url that normalizes past the limit
    no('https://a.com/' + '\u00fc'.repeat(400));
    no('');
    no('   ');
    no(undefined);
    no(null);
    no(42);
    no({ toString: () => 'https://a.com/' });
    no('https://');
    no('http://');
    no('https://exa mple.com/');
  });
});
