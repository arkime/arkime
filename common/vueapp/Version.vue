<template>
  <span
    :title="buildInfo"
    v-b-tooltip.hover="buildInfo"
    class="navbar-text mr-2 text-right cursor-help">
    v{{ version }}
  </span>
</template>

<script>
import './vueFilters';

// NOTE: parent application must have the constants present in the application
export default {
  name: 'Version',
  props: {
    timezone: { // the timezone to format the time
      type: String,
      required: true
    }
  },
  data () {
    return {
      version: this.$constants.VERSION,
      buildDate: this.$constants.BUILD_DATE,
      buildVersion: this.$constants.BUILD_VERSION
    };
  },
  computed: {
    buildInfo () {
      const dateMs = new Date(this.buildDate).getTime();
      const date = this.$options.filters.timezoneDateString(dateMs, this.timezone);
      return `${this.buildVersion}${!isNaN(dateMs) ? ' @ ' + date : ''}`;
    }
  }
};
</script>
