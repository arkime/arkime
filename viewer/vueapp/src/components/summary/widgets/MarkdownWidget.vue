<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Markdown widget: static notes/instructions on the dashboard. No fetch, no
field; the text is part of the widget definition and edited in the widget
modal. The text may come from another user (shared or imported dashboards), so it is
parsed as a small markdown subset (miniMarkdown.js) and rendered with h()
into elements and text nodes only, never as HTML.
-->
<template>
  <WidgetCard
    :title="title"
    :has-data="true"
    :info-items="infoItems"
    :link="link"
    @edit="$emit('edit')"
    @remove="$emit('remove')">
    <div
      v-if="widget.text"
      class="text-widget">
      <MarkdownBlocks :blocks="blocks" />
    </div>
    <div
      v-else
      class="text-widget text-widget--empty text-medium-emphasis">
      {{ $t('sessions.summary.widget.textHint') }}
    </div>
  </WidgetCard>
</template>

<script setup>
import { computed, h } from 'vue';
import { useI18n } from 'vue-i18n';
import { useRoute, useRouter } from 'vue-router';
// render functions don't go through vuetify's template auto-import
import { VIcon } from 'vuetify/components/VIcon';
import WidgetCard from './WidgetCard.vue';
import { parseMarkdown } from './miniMarkdown';

const { t } = useI18n();
const route = useRoute();
const router = useRouter();

const props = defineProps({
  widget: { type: Object, required: true },
  infoItems: { type: Array, default: () => [] },
  link: { type: Object, default: null }
});

defineEmits(['edit', 'remove']);

const title = computed(() => props.widget.title || t('sessions.summary.markdownView'));

// a link on this site may only open one of the app's pages: the router's
// static routes under the web base path (PATH is a global viewer.js injects
// into the page)
const webBase = typeof PATH === 'string' ? PATH : '/';
const pages = router.getRoutes()
  .map(r => r.path.replace(/^\//, ''))
  .filter(p => p && !/[:*(]/.test(p));

// relative links resolve against the current page, so reparse when it changes
const blocks = computed(() => {
  void route?.fullPath;
  return parseMarkdown(props.widget.text, { href: window.location.href, webBase, pages });
});

// strings become text nodes; every attribute value comes from safeLink
const renderInlines = (nodes) => nodes.map(renderInline);

// a plain click on one of the app's pages goes through the router, so the
// dashboard's state survives; a modified click (new tab) and the href itself
// still work as an ordinary link
function openInApp (e, href) {
  if (e.defaultPrevented || e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey) { return; }
  const url = new URL(href);
  const base = webBase.endsWith('/') ? webBase.slice(0, -1) : webBase;
  e.preventDefault();
  router.push((url.pathname.slice(base.length) || '/') + url.search + url.hash)
    .catch(() => window.location.assign(href));
}

function renderLink (node) {
  const children = renderInlines(node.children);
  if (!node.external) {
    return h('a', { href: node.href, onClick: (e) => openInApp(e, node.href) }, children);
  }
  return h('a', {
    href: node.href,
    target: '_blank',
    rel: 'noopener noreferrer nofollow',
    referrerpolicy: 'no-referrer',
    title: `${node.host}\n${node.href}`
  }, [...children, h(VIcon, { icon: 'mdi-open-in-new', size: 'x-small', class: 'ms-1 md-external' })]);
}

function renderInline (node) {
  switch (node.type) {
  case 'text': return node.text;
  case 'br': return h('br');
  case 'code': return h('code', node.text);
  case 'strong': return h('strong', renderInlines(node.children));
  case 'em': return h('em', renderInlines(node.children));
  case 'link': return renderLink(node);
  default: return null;
  }
}

function renderBlock (node) {
  switch (node.type) {
  case 'heading': return h(`h${node.level}`, renderInlines(node.children));
  case 'paragraph': return h('p', renderInlines(node.children));
  case 'hr': return h('hr');
  case 'list':
    return h(node.ordered ? 'ol' : 'ul',
      node.ordered && node.start !== 1 ? { start: node.start } : null,
      node.items.map(item => h('li', renderInlines(item))));
  default: return null;
  }
}

const MarkdownBlocks = (p) => p.blocks.map(renderBlock);
MarkdownBlocks.props = { blocks: { type: Array, required: true } };
</script>

<style scoped>
.text-widget {
  flex: 1;
  overflow: auto;
  word-break: break-word;
  font-size: 0.9rem;
  line-height: 1.4;
  padding: 0.15rem 0.1rem;
}
.text-widget--empty {
  font-style: italic;
}

/* the markdown is rendered by h() in a child, so it needs :deep() */
.text-widget :deep(:is(h1, h2, h3, h4, h5, h6)) {
  margin: 0.6em 0 0.3em;
  font-weight: 600;
  line-height: 1.25;
}
.text-widget :deep(h1) { font-size: 1.3rem; }
.text-widget :deep(h2) { font-size: 1.2rem; }
.text-widget :deep(h3) { font-size: 1.1rem; }
.text-widget :deep(h4) { font-size: 1rem; }
.text-widget :deep(h5) { font-size: 0.95rem; }
.text-widget :deep(h6) { font-size: 0.9rem; }
.text-widget :deep(p),
.text-widget :deep(ul),
.text-widget :deep(ol) {
  margin: 0 0 0.5em;
}
.text-widget :deep(ul),
.text-widget :deep(ol) {
  padding-left: 1.4em;
}
.text-widget > :deep(:first-child) {
  margin-top: 0;
}
.text-widget > :deep(:last-child) {
  margin-bottom: 0;
}
.text-widget :deep(code) {
  font-family: monospace;
  font-size: 0.85em;
  padding: 0.05em 0.3em;
  border-radius: 3px;
  background: rgb(var(--v-theme-neutral-lighter));
}
.text-widget :deep(hr) {
  border: 0;
  border-top: 1px solid rgb(var(--v-theme-neutral));
  margin: 0.5em 0;
}
.text-widget :deep(.md-external) {
  vertical-align: baseline;
  opacity: 0.7;
}
</style>
