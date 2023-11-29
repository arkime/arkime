<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>

  <div class="footer">
    <p>
      <small>
        <span v-html="footerConfig"></span>
        <span v-if="responseTime && !loadingData">
          | {{ responseTime | commaString }}ms
        </span>
        <span v-if="loadingData">
          |
          <span class="fa fa-spinner fa-spin fa-lg text-theme-accent">
          </span>
        </span>
      </small>
    </p>
  </div>

</template>

<script>
export default {
  name: 'ArkimeFooter',
  data: function () {
    const footerConfig = new DOMParser().parseFromString(this.$constants.FOOTER_CONFIG, 'text/html').documentElement.textContent;
    return {
      version: this.$constants.VERSION,
      footerConfig
    };
  },
  computed: {
    responseTime: function () {
      return this.$store.state.responseTime;
    },
    loadingData: function () {
      return this.$store.state.loadingData;
    }
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
