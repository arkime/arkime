<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="info-area vertical-center arkime-error-box">
    <div class="text-danger">
      <v-icon
        icon="mdi-alert"
        size="large" />
      <span v-if="message">
        {{ message }}
      </span>
      <span
        v-else-if="messageHtml"
        v-html="messageHtml" />
    </div>
  </div>
</template>

<script>
export default {
  name: 'ArkimeError',
  props: {
    message: {
      type: String,
      default: ''
    },
    messageHtml: {
      type: String,
      default: ''
    }
  }
};
</script>

<style scoped>
/* Bootstrap used to provide .font-monospace; with bootstrap.css gone
   we render the parse-error text monospace inline so unbalanced parens
   etc. line up readably. */
.arkime-error-box {
  font-family: SFMono-Regular, Menlo, Monaco, Consolas, monospace;
}
.info-area > .text-danger {
  white-space: pre-line;
  line-height: 1.1em;
  font-size: 1.5rem;
}
</style>
