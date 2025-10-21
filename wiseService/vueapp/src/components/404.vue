<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="whole-page-info container">
    <div class="center-area">
      <img
        src="/assets/watching.gif"
        alt="hoot">

      <div class="well well-lg">
        <h1>404</h1>
        <h4>{{ $t('common.nothingToSeeHere') }}</h4>
      </div>

      <br>

      <div
        class="margined-bottom"
        v-html="$t('wise.404.messageHtml')" />
      <div v-html="$t('wise.404.helpHtml')" />
    </div>
  </div>
</template>

<script>
export default {
  name: 'Wise404'
};
</script>
