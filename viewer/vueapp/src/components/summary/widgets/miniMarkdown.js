/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Mini markdown for the dashboard text widget. Widget text comes from other
users (shared and imported dashboards), so this only builds a plain token tree
and never HTML; MarkdownWidget renders the tree with h() so all of the text ends up
in text nodes. Pure JS (no DOM, no Vue) so it can be unit tested in node.

Supported: paragraphs, line breaks, # headings, **bold**, *italic* / _italic_,
`code`, one level of - * + or 1. lists, --- rules, and [text](url) links whose
url passes safeLink. Everything else (html, images, autolinks, reference links,
tables) stays literal text.

Blocks:  { type: 'paragraph' | 'heading', level (heading only), children }
         { type: 'list', ordered, start, items: [children, ...] }
         { type: 'hr' }
Inlines: { type: 'text', text } | { type: 'br' } | { type: 'code', text }
         { type: 'strong' | 'em', children }
         { type: 'link', href, external, host, children }

Links are http(s) only. One on this site may only open one of the app's own
pages (see safeLink), so a shared dashboard can't hand out a one-click api
call, export or download.
*/

export const MAX_TEXT = 5000; // widget text past this is ignored
export const MAX_HREF = 2048;
const MAX_DEPTH = 16; // open emphasis/bracket frames, and emphasis nesting, per inline run

// ---------------------------------------------------------------------------
// links
// ---------------------------------------------------------------------------
const SCHEMES = new Set(['http:', 'https:']);
// control chars, whitespace, invisible/bidi formatting chars, characters that
// don't belong in a url, and html entities
const BAD_HREF_RE = /[\u0000-\u001f\u007f-\u009f\s\u200b-\u200f\u202a-\u202e\u2060-\u2069\\<>"'`]|&(?:#|[a-z][a-z0-9]*;)/i;
// a . or .. path segment, plain or percent encoded, before any ? or #
const DOT_SEGMENT_RE = /^[^?#]*(?:^|\/)(?:\.|%2e){1,2}(?:[/?#]|$)/i;

/**
 * True when a url on this site opens one of the app's pages: its path is the
 * web base path plus a page route, exactly (any query or hash). So no api
 * endpoint, file, legacy route or path outside the base, however spelled.
 */
function isAppPage (url, site) {
  const webBase = typeof site?.webBase === 'string' && site.webBase.startsWith('/') ? site.webBase : null;
  if (!webBase || !Array.isArray(site.pages)) { return false; }
  const prefix = webBase.endsWith('/') ? webBase : `${webBase}/`;
  if (url.pathname === prefix.slice(0, -1)) { return true; } // the base itself, without its slash
  if (!url.pathname.startsWith(prefix)) { return false; }
  const page = url.pathname.slice(prefix.length).replace(/\/$/, '');
  return page === '' || site.pages.includes(page);
}

/**
 * Checks a markdown link destination.
 * @param {string} href - the raw destination
 * @param {object} site - where the widget is shown: { href (the page url
 *   relative links resolve against), webBase (the app's web base path, eg
 *   '/' or '/arkime/'), pages (the page routes under it, eg ['sessions',
 *   'spigraph']) }. Without it only links to other sites are kept.
 * @returns {object|null} { href, external, host } with the normalized href to
 *   use, or null when the link must not be rendered as a link
 */
export function safeLink (href, site) {
  if (typeof href !== 'string') { return null; }
  const raw = href.trim();
  if (!raw || raw.length > MAX_HREF || BAD_HREF_RE.test(raw) || raw.startsWith('//') || DOT_SEGMENT_RE.test(raw)) {
    return null;
  }

  let base = null;
  try { base = new URL(site?.href); } catch {}
  if (base && base.protocol !== 'http:' && base.protocol !== 'https:') { base = null; }

  let url;
  // a ':' before any / ? # means the link names a scheme
  const colon = raw.indexOf(':');
  const end = raw.search(/[/?#]/);
  if (colon !== -1 && (end === -1 || colon < end)) {
    if (!SCHEMES.has(raw.slice(0, colon + 1).toLowerCase())) { return null; }
    try { url = new URL(raw); } catch { return null; }
  } else { // relative (path, ?query, #fragment): must stay on this site
    if (!base) { return null; }
    try { url = new URL(raw, base); } catch { return null; }
    if (url.origin !== base.origin) { return null; }
  }

  if (!SCHEMES.has(url.protocol) || url.username || url.password || !url.hostname || url.href.length > MAX_HREF) {
    return null;
  }

  // this host on another scheme or port (another listener on this server:
  // capture, WISE, a plain http viewer) is never an outside site
  if (base && url.hostname === base.hostname) {
    if (url.origin !== base.origin || !isAppPage(url, site)) { return null; }
    return { href: url.href, external: false, host: url.host };
  }
  return { href: url.href, external: true, host: url.host };
}

// ---------------------------------------------------------------------------
// inline
// ---------------------------------------------------------------------------
const SPECIAL_RE = /[\\`*_[\]!\n]/g;
const ASCII_PUNCT_RE = /[!-/:-@[-`{-~]/;
const PUNCT_RE = /[\p{P}\p{S}]/u;
const WS_RE = /\s/;

// output nesting depth of a node list (strong/em/link nodes record theirs)
const nodeDepth = new WeakMap();
function depthOf (children) {
  let d = 0;
  for (const c of children) { const cd = nodeDepth.get(c) ?? 0; if (cd > d) { d = cd; } }
  return d;
}

function addText (children, text) {
  if (!text) { return; }
  const last = children[children.length - 1];
  if (last?.type === 'text') {
    last.text += text;
  } else {
    children.push({ type: 'text', text });
  }
}

function addNodes (children, nodes) {
  for (const node of nodes) {
    if (node.type === 'text') {
      addText(children, node.text);
    } else {
      children.push(node);
    }
  }
}

// the destination of "](dest)" starting at the '(' at pos (balanced parens,
// one line, bounded length). Returns { dest, end } or null.
function parseDest (text, pos) {
  if (text[pos] !== '(') { return null; }
  let depth = 0;
  const max = Math.min(text.length, pos + MAX_HREF + 64);
  for (let i = pos + 1; i < max; i++) {
    const c = text[i];
    if (c === '\n') { return null; }
    if (c === '(') {
      if (++depth > 32) { return null; }
    } else if (c === ')') {
      if (depth === 0) { return { dest: text.slice(pos + 1, i), end: i + 1 }; }
      depth--;
    }
  }
  return null;
}

/**
 * Parses inline markdown with a single left to right scan. Open '*' / '_' runs
 * and '[' are kept on a small stack of frames; a closer finds its frame on the
 * stack, anything left open above it falls back to literal text.
 * @param {string} text - one block's text, lines separated by '\n'
 * @param {object} site - see safeLink
 * @returns {Array} inline nodes
 */
export function parseInline (text, site) {
  const stack = [{ children: [] }]; // root, then { delim, count } or { bracket, active }
  const current = () => stack[stack.length - 1];
  const noCloser = new Set(); // backtick run lengths with no closing run left
  const n = text.length;

  // unclosed frames above stack[idx] become literal text in their parent
  const flattenAbove = (idx) => {
    while (stack.length - 1 > idx) {
      const frame = stack.pop();
      addText(current().children, frame.bracket ? frame.raw : frame.delim.repeat(frame.count));
      addNodes(current().children, frame.children);
    }
  };

  let i = 0;
  while (i < n) {
    SPECIAL_RE.lastIndex = i;
    const m = SPECIAL_RE.exec(text);
    const next = m ? m.index : n;
    addText(current().children, text.slice(i, next));
    if (!m) { break; }
    i = next;
    const c = text[i];

    if (c === '\n') {
      current().children.push({ type: 'br' });
      i++;
    } else if (c === '\\') {
      if (i + 1 < n && ASCII_PUNCT_RE.test(text[i + 1])) {
        addText(current().children, text[i + 1]);
        i += 2;
      } else {
        addText(current().children, '\\');
        i++;
      }
    } else if (c === '`') {
      let k = 1;
      while (text[i + k] === '`') { k++; }
      let closeAt = -1;
      if (!noCloser.has(k)) {
        for (let j = text.indexOf('`', i + k); j !== -1; j = text.indexOf('`', j)) {
          let r = 1;
          while (text[j + r] === '`') { r++; }
          if (r === k) { closeAt = j; break; }
          j += r;
        }
        if (closeAt === -1) { noCloser.add(k); }
      }
      if (closeAt === -1) {
        addText(current().children, text.slice(i, i + k));
        i += k;
      } else {
        let code = text.slice(i + k, closeAt).replace(/\n/g, ' ');
        if (code.length > 2 && code[0] === ' ' && code[code.length - 1] === ' ' && code.trim()) {
          code = code.slice(1, -1);
        }
        current().children.push({ type: 'code', text: code });
        i = closeAt + k;
      }
    } else if (c === '[' || (c === '!' && text[i + 1] === '[')) {
      const raw = c === '[' ? '[' : '![';
      if (stack.length < MAX_DEPTH) {
        // images are only bracketed so their ']' can't close an outer link
        stack.push({ bracket: true, raw, active: c === '[', children: [] });
      } else {
        addText(current().children, raw);
      }
      i += raw.length;
    } else if (c === ']') {
      let b = stack.length - 1;
      while (b > 0 && !stack[b].bracket) { b--; }
      const link = b > 0 && stack[b].active ? parseDest(text, i + 1) : null;
      if (b === 0) {
        addText(current().children, ']');
        i++;
      } else if (!link) { // not a link: the brackets are literal
        flattenAbove(b);
        const frame = stack.pop();
        addText(current().children, frame.raw);
        addNodes(current().children, frame.children);
        addText(current().children, ']');
        i++;
      } else {
        flattenAbove(b);
        const frame = stack.pop();
        const safe = safeLink(link.dest, site);
        if (safe) {
          const linkNode = { type: 'link', ...safe, children: frame.children };
          nodeDepth.set(linkNode, 1 + depthOf(frame.children));
          current().children.push(linkNode);
        } else { // unsafe destination: keep only the link text
          addNodes(current().children, frame.children);
        }
        // no links inside links
        for (const f of stack) { if (f.bracket) { f.active = false; } }
        i = link.end;
      }
    } else if (c === '!') {
      addText(current().children, '!');
      i++;
    } else { // '*' or '_'
      let k = 1;
      while (text[i + k] === c) { k++; }
      const before = i > 0 ? text[i - 1] : ' ';
      const after = i + k < n ? text[i + k] : ' ';
      const wsBefore = WS_RE.test(before);
      const wsAfter = WS_RE.test(after);
      const pBefore = PUNCT_RE.test(before);
      const pAfter = PUNCT_RE.test(after);
      const left = !wsAfter && (!pAfter || wsBefore || pBefore);
      const right = !wsBefore && (!pBefore || wsAfter || pAfter);
      // '_' doesn't open or close inside a word (snake_case_names)
      const canOpen = c === '*' ? left : left && (!right || pBefore);
      const canClose = c === '*' ? right : right && (!left || pAfter);

      let remaining = k;
      while (canClose && remaining > 0) {
        let f = stack.length - 1;
        while (f > 0 && !stack[f].bracket && stack[f].delim !== c) { f--; }
        if (f === 0 || stack[f].bracket) { break; }
        // nesting is bounded too, not just open frames: past MAX_DEPTH the
        // rest of the closer run stays literal
        const d = 1 + Math.max(depthOf(stack[f].children), ...stack.slice(f + 1).map(fr => depthOf(fr.children)));
        if (d > MAX_DEPTH) { break; }
        flattenAbove(f);
        const openFrame = current();
        const use = remaining >= 2 && openFrame.count >= 2 ? 2 : 1;
        const node = { type: use === 2 ? 'strong' : 'em', children: openFrame.children };
        nodeDepth.set(node, d);
        openFrame.count -= use;
        remaining -= use;
        if (openFrame.count === 0) {
          stack.pop();
          current().children.push(node);
        } else { // leftover opener delimiters stay open around the new node
          openFrame.children = [node];
        }
      }
      if (remaining > 0) {
        if (canOpen && stack.length < MAX_DEPTH) {
          stack.push({ delim: c, count: remaining, children: [] });
        } else {
          addText(current().children, c.repeat(remaining));
        }
      }
      i += k;
    }
  }

  flattenAbove(0);
  return stack[0].children;
}

// ---------------------------------------------------------------------------
// blocks
// ---------------------------------------------------------------------------
const HR_RE = /^([-*_])(?:[ \t]*\1){2,}$/;
const HEADING_RE = /^(#{1,6})(?:[ \t]|$)/;
const ITEM_RE = /^(?:[-*+]|(\d{1,9})[.)])[ \t]+/;

// "## Title ##" -> "Title"
function stripClosingHashes (s) {
  let i = s.length;
  while (i > 0 && s[i - 1] === '#') { i--; }
  if (i === s.length) { return s; }
  if (i === 0) { return ''; }
  return s[i - 1] === ' ' || s[i - 1] === '\t' ? s.slice(0, i).trimEnd() : s;
}

/**
 * Parses widget text into a block token tree (see the top of this file).
 * @param {string} src - the markdown, only the first MAX_TEXT chars are used
 * @param {object} site - where the widget is shown, see safeLink
 * @returns {Array} block nodes
 */
export function parseMarkdown (src, site) {
  if (typeof src !== 'string' || !src) { return []; }

  const lines = src.slice(0, MAX_TEXT).replace(/\r\n?/g, '\n').split('\n');
  const blocks = [];
  let para = null; // lines of the open paragraph
  let list = null; // open list, items are line arrays until it closes
  let gap = false; // blank line since the last list line

  const closePara = () => {
    if (para) {
      blocks.push({ type: 'paragraph', children: parseInline(para.join('\n'), site) });
      para = null;
    }
  };
  const closeList = () => {
    if (list) {
      list.items = list.items.map(item => parseInline(item.join('\n'), site));
      blocks.push(list);
      list = null;
    }
  };

  for (const rawLine of lines) {
    const line = rawLine.trim();
    let m;

    if (!line) {
      closePara();
      gap = true;
    } else if (HR_RE.test(line)) {
      closePara();
      closeList();
      blocks.push({ type: 'hr' });
    } else if ((m = HEADING_RE.exec(line))) {
      closePara();
      closeList();
      const children = parseInline(stripClosingHashes(line.slice(m[1].length).trim()), site);
      blocks.push({ type: 'heading', level: m[1].length, children });
    } else if ((m = ITEM_RE.exec(line))) {
      const ordered = m[1] !== undefined;
      closePara();
      if (list && list.ordered !== ordered) { closeList(); }
      if (!list) { list = { type: 'list', ordered, start: ordered ? Number(m[1]) : 1, items: [] }; }
      list.items.push([line.slice(m[0].length)]);
      gap = false;
    } else if (list && !gap) { // continues the last list item
      list.items[list.items.length - 1].push(line);
    } else {
      closeList();
      para ??= [];
      para.push(line);
    }
  }

  closePara();
  closeList();
  return blocks;
}
