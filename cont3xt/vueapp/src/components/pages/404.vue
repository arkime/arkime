<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="whole-page-info container">

    <div class="center-area">

        <!-- TODO: toby-rm ?? was assets/watching.gif, what do we want to do? -->
      <img :src="watching"
        alt="hoot" />

      <div class="well well-lg">
        <h1>404</h1>
        <h4>Nothing to see here...</h4>
      </div>

      <br/>

      <div class="margined-bottom">
        Maybe you want to view
        <a href=""
          class="no-decoration">
          something useful</a>?
      </div>

    </div>

  </div>
</template>

<script>
  import watching from '../../../../../assets/watching.gif';
export default {
  name: 'Cont3xt404',
  data () {
    return { watching };
  }
};
</script>
