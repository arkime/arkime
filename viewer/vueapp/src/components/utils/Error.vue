<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="info-area vertical-center font-monospace">
    <div class="text-danger">
      <span class="fa fa-2x fa-warning" />
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
.info-area > .text-danger {
  white-space: pre-line;
  line-height: 1.1em;
  font-size: 1.5rem;
}
</style>
