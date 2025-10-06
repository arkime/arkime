<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="whole-page-info container">
    <div class="center-area">
      <img
        :src="watching"
        alt="hoot"
      >

      <div class="well well-lg">
        <h1>Incompatible Browser</h1>
        <h4>Please upgrade!</h4>
      </div>

      <br>

      <div class="mb-2 mt-2">
        <a
          href="https://arkime.com/faq#what-browsers-are-supported"
          class="no-decoration"
        >
          Which browsers are supported?
        </a>
      </div>
    </div>
  </div>
</template>

<script>
export default {
  name: 'ParliamentUpgradeBrowser',
  data () {
    return {
      watching: 'assets/watching.gif'
    };
  }
};
</script>
