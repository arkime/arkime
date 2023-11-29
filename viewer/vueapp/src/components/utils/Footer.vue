<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="footer">
    <p>
      <small>
        <div id="footerConfig"></div>
      </small>
    </p>
  </div>
</template>

<script>
import Vue from 'vue';

let footer;

export default {
  name: 'ArkimeFooter',
  mounted () {
    footer = new Vue({
      parent: this,
      el: '#footerConfig',
      template: new DOMParser().parseFromString(this.$constants.FOOTER_CONFIG, 'text/html').documentElement.textContent,
      computed: {
        responseTime () {
          return this.$parent.$store.state.responseTime;
        },
        loadingData () {
          return this.$parent.$store.state.loadingData;
        }
      }
    });
  },
  beforeDestroy () {
    footer.$destroy();
  }
};
</script>

<style scoped>
.footer {
  text-align: center;
  color: var(--color-gray-dark);
  border-top: 1px solid var(--color-gray-light);
  position: absolute;
  bottom: 0;
  width: 100%;
  height: 25px;
  line-height: 25px;
  overflow: hidden;
}
</style>
