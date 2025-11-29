<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="whole-page-info container">
    <div class="center-area">
      <img
        :src="watching"
        alt="hoot">

      <div class="well well-lg">
        <h1>404</h1>
        <h4>{{ $t('common.nothingToSeeHere') }}</h4>
      </div>

      <br>

      <div
        class="margined-bottom"
        v-html="$t('parliament.404.messageHtml')" />
    </div>
  </div>
</template>

<script>
export default {
  name: 'Parliament404',
  data () {
    return {
      watching: 'assets/watching.gif'
    };
  }
};
</script>
