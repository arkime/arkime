// SPDX-License-Identifier: Apache-2.0
import { ref } from 'vue';

const MARK_CLASS = 'find-hit';
const START_CLASS = 'find-hit-start'; // one per match — used for counting/navigation
const CURRENT_CLASS = 'find-hit--current';

function escapeRegExp (s) {
  return s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

// offsetParent catches a display:none ancestor; visibility inherits, so one
// computed-style read catches a hidden (visibility:hidden/height:0) column.
function hitVisible (el) {
  return el.offsetParent !== null && window.getComputedStyle(el).visibility !== 'hidden';
}

// Find-in-content engine for a DOM subtree: marks matches and steps through them.
// navigateOnly = don't mark, just collect server-rendered .find-hit spans (packets).
export function useContentFind (getRoot, opts = {}) {
  const { skipSelector = '', maxHits = 5000, navigateOnly = false, skipHidden = false, onBeforeNav } = opts;

  const query = ref('');
  const isRegex = ref(false);
  const matchCount = ref(0);
  const currentIndex = ref(-1);
  const capped = ref(false);
  const error = ref('');

  let hits = [];

  function reset () {
    hits = [];
    matchCount.value = 0;
    currentIndex.value = -1;
    capped.value = false;
  }

  function unmark (root) {
    if (!root) { return; }
    root.querySelectorAll('mark.' + MARK_CLASS).forEach((m) => {
      const markParent = m.parentNode;
      if (!markParent) { return; }
      while (m.firstChild) { markParent.insertBefore(m.firstChild, m); }
      markParent.removeChild(m);
      markParent.normalize(); // re-merge split text nodes so repeated searches don't fragment
    });
  }

  function buildMatcher () {
    const q = (query.value || '').trim();
    if (!q) { return null; }
    error.value = '';
    try {
      return new RegExp(isRegex.value ? q : escapeRegExp(q), 'gi');
    } catch (e) {
      error.value = e.message || 'Invalid regex';
      return null;
    }
  }

  function skip (textNode) {
    let el = textNode.parentElement;
    while (el) {
      const tag = el.tagName;
      if (tag === 'SCRIPT' || tag === 'STYLE') { return true; }
      if (el.classList && el.classList.contains(MARK_CLASS)) { return true; }
      el = el.parentElement;
    }
    if (skipSelector && textNode.parentElement && textNode.parentElement.closest(skipSelector)) { return true; }
    return false;
  }

  function setCurrent (idx, doScroll) {
    hits.forEach((h, i) => h.classList.toggle(CURRENT_CLASS, i === idx));
    currentIndex.value = idx;
    if (doScroll && hits[idx]) {
      if (onBeforeNav) { onBeforeNav(hits[idx]); }
      hits[idx].scrollIntoView({ block: 'center', behavior: 'smooth' });
    }
  }

  // Collect server-rendered match anchors (navigateOnly mode). Highlights only —
  // no current hit until the user navigates with next/prev.
  function collectExisting (root) {
    let found = Array.from(root.querySelectorAll('.' + START_CLASS));
    if (found.length > maxHits) { found = found.slice(0, maxHits); capped.value = true; }
    if (skipHidden) { found = found.filter(hitVisible); }
    hits = found;
    matchCount.value = hits.length;
  }

  function runHighlight () {
    const root = getRoot();
    if (!root) { reset(); return; }

    if (navigateOnly) { reset(); collectExisting(root); return; }

    unmark(root);
    reset();

    const re = buildMatcher();
    if (!re) { return; }

    const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, {
      acceptNode (node) {
        if (!node.nodeValue) { return NodeFilter.FILTER_REJECT; }
        return skip(node) ? NodeFilter.FILTER_REJECT : NodeFilter.FILTER_ACCEPT;
      }
    });
    const textNodes = [];
    let n;
    while ((n = walker.nextNode())) { textNodes.push(n); }

    let count = 0;
    for (const textNode of textNodes) {
      if (count >= maxHits) { capped.value = true; break; }
      const text = textNode.nodeValue;
      re.lastIndex = 0;
      const ranges = [];
      let m;
      while ((m = re.exec(text)) !== null) {
        if (m[0].length === 0) { re.lastIndex++; continue; } // guard zero-width matches
        ranges.push([m.index, m.index + m[0].length]);
        if (count + ranges.length >= maxHits) { capped.value = true; break; }
      }
      // wrap back-to-front so earlier offsets stay valid as the node splits
      for (let i = ranges.length - 1; i >= 0; i--) {
        const range = document.createRange();
        range.setStart(textNode, ranges[i][0]);
        range.setEnd(textNode, ranges[i][1]);
        const mark = document.createElement('mark');
        mark.className = MARK_CLASS + ' ' + START_CLASS;
        try { range.surroundContents(mark); count++; } catch (e) { /* skip un-wrappable range */ }
      }
    }

    hits = Array.from(root.querySelectorAll('mark.' + START_CLASS)); // re-query for document order
    matchCount.value = hits.length;
  }

  function search (q, options = {}) {
    if (q !== undefined) { query.value = q; }
    if (options.regex !== undefined) { isRegex.value = options.regex; }
    runHighlight();
  }

  const reapply = runHighlight;

  // first nav after a search jumps to the first (next) / last (prev) hit
  function next () {
    if (!hits.length) { return; }
    const idx = currentIndex.value < 0 ? 0 : (currentIndex.value + 1) % hits.length;
    setCurrent(idx, true);
  }

  function prev () {
    if (!hits.length) { return; }
    const idx = currentIndex.value < 0 ? hits.length - 1 : (currentIndex.value - 1 + hits.length) % hits.length;
    setCurrent(idx, true);
  }

  return { matchCount, currentIndex, capped, error, search, reapply, next, prev };
}
