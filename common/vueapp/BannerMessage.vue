<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<!--
Linkifies bare http/https URLs without v-html (Vue escapes the text + :href,
so non-http schemes can't become links). Effects animate separate nested
layers (scroll/blink/content) so they can combine.
-->
<template>
  <span
    class="banner-message"
    :class="effectClasses"><span class="banner-message__scroll"><span class="banner-message__blink"><span class="banner-message__content"><template
      v-for="(part, index) in parts"
      :key="index"><a
        v-if="part.url"
        :href="part.url"
        target="_blank"
        rel="noopener noreferrer">{{ part.url }}</a><span v-else>{{ part.text }}</span></template></span></span></span></span>
</template>

<script>
const URL_RE = /(https?:\/\/[^\s]+)/g;
// trailing chars that are almost always sentence punctuation, not the URL
const TRAILING_RE = /[.,!?;:)\]}'"]+$/;

export default {
  name: 'BannerMessage',
  props: {
    message: { type: String, default: '' },
    effects: { type: Array, default: () => [] }
  },
  computed: {
    effectClasses () {
      const e = this.effects || [];
      return {
        'banner-message--marquee': e.includes('marquee'),
        'banner-message--blink': e.includes('blink'),
        'banner-message--rainbow': e.includes('rainbow')
      };
    },
    parts () {
      const text = this.message || '';
      const out = [];
      let last = 0;
      let m;
      URL_RE.lastIndex = 0;
      while ((m = URL_RE.exec(text)) !== null) {
        if (m.index > last) { out.push({ text: text.slice(last, m.index) }); }
        let url = m[0];
        const trail = url.match(TRAILING_RE);
        const trailing = trail ? trail[0] : '';
        if (trailing) { url = url.slice(0, url.length - trailing.length); }
        out.push({ url });
        if (trailing) { out.push({ text: trailing }); }
        last = m.index + m[0].length;
      }
      if (last < text.length) { out.push({ text: text.slice(last) }); }
      return out;
    }
  }
};
</script>

<style scoped>
/* honor newlines an admin types into the message; pin the size so it's
   consistent across apps (cont3xt shrinks body to 0.85rem) */
.banner-message {
  white-space: pre-line;
  font-size: 1rem;
}
.banner-message a {
  color: inherit;
  font-weight: 600;
  text-decoration: underline;
}

/* marquee: classic right-to-left scroll */
.banner-message--marquee {
  display: block;
  width: 100%;
  overflow: hidden;
  white-space: nowrap;
}
.banner-message--marquee .banner-message__scroll {
  display: inline-block;
  padding-left: 100%;
  white-space: nowrap;
  animation: app-banner-marquee 18s linear infinite;
}
@keyframes app-banner-marquee {
  0%   { transform: translateX(0); }
  100% { transform: translateX(-100%); }
}

/* blink: hard on/off flash */
.banner-message--blink .banner-message__blink {
  animation: app-banner-blink 1.2s steps(1) infinite;
}
@keyframes app-banner-blink {
  0%, 49%   { opacity: 1; }
  50%, 100% { opacity: 0; }
}

/* rainbow: cycle the text hue (stays fully opaque -- never vanishes) */
.banner-message--rainbow .banner-message__content {
  color: #ff2d2d;
  font-weight: 700;
  animation: app-banner-rainbow 5s linear infinite;
}
@keyframes app-banner-rainbow {
  to { filter: hue-rotate(360deg); }
}

/* reduced motion: static, readable text (flashing is an a11y hazard) */
@media (prefers-reduced-motion: reduce) {
  .banner-message--marquee {
    white-space: pre-line;
  }
  .banner-message--marquee .banner-message__scroll {
    display: inline;
    padding-left: 0;
    animation: none;
  }
  .banner-message__blink {
    animation: none !important;
    opacity: 1 !important;
  }
  .banner-message--rainbow .banner-message__content {
    animation: none;
    filter: none;
    color: inherit;
  }
}
</style>
