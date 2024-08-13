<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>

  <div>
    <v-container class="whole-page-info mt-3">

      <div class="center-area">
        <!-- TODO: toby-rm ?? was assets/watching.gif, what do we want to do? -->
        <img
          :src="watching"
          alt="hoot" />

        <div class="well well-lg">
          <h1>Incompatible Browser</h1>
          <h4>Please upgrade!</h4>
        </div>

        <br/>

        <div class="mb-2 mt-2">
          <a href="https://arkime.com/faq#what-browsers-are-supported"
            class="no-decoration">
            Which browsers are supported?
          </a>
        </div>

      </div>

    </v-container>
  </div>

</template>

<script>
import watching from '../../../../../assets/watching.gif';
export default {
  name: 'Cont3xtUpgradeBrowser',
  data () {
    return {
      watching
    };
  }
};
</script>
