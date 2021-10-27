<template>
  <div id="app">
    <router-view />
    <b-overlay
      fixed
      no-wrap
      :opacity="0.4"
      :show="getLoading"
    />
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

export default {
  name: 'App',
  computed: {
    ...mapGetters(['getLoading'])
  }
};
</script>
