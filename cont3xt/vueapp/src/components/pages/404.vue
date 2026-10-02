<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-container class="whole-page-info container">
    <div class="center-area">
      <img
        :src="watchingSrc"
        alt="hoot">

      <div class="well well-lg">
        <h1>404</h1>
        <h4>{{ $t('common.nothingToSeeHere') }}</h4>
      </div>

      <br>

      <div
        class="margined-bottom"
        v-html="$t('cont3xt.404.messageHtml')" />
    </div>
  </v-container>
</template>

<script>
export default {
  name: 'Cont3xt404',
  data () {
    return {
      watchingSrc: 'assets/watching.gif' // out-of-line, since we don't want this path bundled by vite
    };
  }
};
</script>
