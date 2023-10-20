<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>

  <div class="footer">
    <p>
      <small>
        Arkime v{{ version }} |
        <a href="https://arkime.com">arkime.com</a>
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
  name: 'MolochFooter',
  data: function () {
    return {
      version: this.$constants.VERSION
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
