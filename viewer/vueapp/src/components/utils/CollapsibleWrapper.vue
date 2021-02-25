<template>
  <b-collapse :visible="showToolBars" v-on:recalc-collapse="getOffset">
    <span ref="collapseBox">
      <slot></slot>
    </span>
    <div :style="{ paddingTop: offset + 'px'}"></div>
  </b-collapse>
</template>

<script>
export default {
  data: function () {
    return {
      offset: 0
    };
  },
  mounted: function () {
    this.getOffset();
  },
  computed: {
    // Boolean in the store will remember chosen toggle state for all pages
    showToolBars: function () {
      return this.$store.state.showToolBars;
    }
  },
  methods: {
    // Dynamically sets the height of the wrapper component to the total of inner components
    getOffset: function () {
      this.$nextTick(() => {
        // Could give odd results passing in a mix of position:fixed and others
        this.offset = Array.from(this.$refs.collapseBox.children)
          .reduce((total, el) => (el.offsetHeight) ? el.offsetHeight + total : total, 0);
      });
    }
  },
  watch: {
    // Check offset height after toggle is updated and mounted()
    showToolBars: function () {
      this.$nextTick(() => {
        // Momentarily override toolbars position so animation can occur
        // WARNING: inline position or non-scoped css position will be modified
        Array.from(this.$refs.collapseBox.children).map((el) => {
          const position = (this.showToolBars) ? '' : 'static';
          el.style.position = position;
          return position;
        });

        this.getOffset();
      });
    }
  }
};
</script>
