<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="page-shell">
    <div class="page-chrome">
      <slot name="chrome" />
    </div>
    <div class="page-body">
      <div
        class="page-scroll"
        ref="scrollEl"
        tabindex="-1">
        <slot />
      </div>
      <div
        v-if="$slots.overlay"
        class="page-overlay"
        :style="{ right: `${gutter.v}px`, bottom: `${gutter.h}px` }">
        <slot name="overlay" />
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, reactive, provide, onMounted, onBeforeUnmount } from 'vue';

// App-shell layout: chrome rows (toolbar, paging, pinned viz) live in normal
// flow above a single scroll container, so collapsing the toolbar is plain
// reflow — no fixed positioning, no measured offsets. The overlay slot floats
// content (sticky sessions panel, etc.) over the scroll area without
// viewport-coordinate math.
const scrollEl = ref(null);
provide('pageScrollEl', scrollEl);
defineExpose({ scrollEl });

// inset the overlay so it doesn't cover the scroller's scrollbars
// (classic scrollbars render inside .page-scroll, not at the window edge)
const gutter = reactive({ v: 0, h: 0 });
let gutterObserver;

onMounted(() => {
  const el = scrollEl.value;
  const measure = () => {
    gutter.v = el.offsetWidth - el.clientWidth;
    gutter.h = el.offsetHeight - el.clientHeight;
  };
  measure();
  gutterObserver = new ResizeObserver(measure);
  gutterObserver.observe(el);
  // the document no longer scrolls, so give keyboard scrolling
  // (PageDown/Space/arrows) a target on load
  if (document.activeElement === document.body) {
    el.focus({ preventScroll: true });
  }
});

onBeforeUnmount(() => gutterObserver?.disconnect());
</script>

<style scoped>
.page-shell {
  display: flex;
  flex-direction: column;
  /* navbar is fixed (`.navbarOffset` spacer sits above us) and the footer is
     html-absolute with #app padding-bottom reserving it — subtracting both
     lands total flow height at exactly 100vh, so no document scrollbar. */
  height: calc(100vh - var(--arkime-navbar-height, 36px) - var(--arkime-footer-height, 25px));
  overflow: hidden;
}
/* z-index matches the legacy .fixed-header: the chrome and its shadow
   paint over the overlay, so overlay panels slide under the nav (this
   also keeps an expanded map in the pinned viz above the overlay) */
.page-chrome {
  position: relative;
  z-index: 5;
  flex: 0 0 auto;
}
.page-body {
  position: relative;
  flex: 1 1 0;
  min-height: 0;
  display: flex;
}
.page-scroll {
  flex: 1 1 0;
  min-width: 0;
  overflow: auto;
  position: relative;
  /* always reserve the vertical scrollbar gutter so clientWidth doesn't
     shift when rows arrive (pages measure table width against it) */
  scrollbar-gutter: stable;
  outline: none;
}
.page-overlay {
  position: absolute;
  inset: 0;
  z-index: 4;
  pointer-events: none;
}
.page-overlay > :deep(*) {
  pointer-events: auto;
}
</style>
