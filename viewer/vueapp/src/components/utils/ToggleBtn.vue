<template>

  <a class="btn btn-xs btn-toggle"
    v-on:click="toggleStatDetail"
    :class="{'collapsed btn-theme-tertiary':!opened, 'btn-danger':opened}">
    <span class="fa fa-close"></span>
  </a>

</template>

<script>
export default {
  name: 'ToggleBtn',
  data: function () {
    return {
      opened: false
    };
  },
  methods: {
    toggleStatDetail: function () {
      this.opened = !this.opened;
      this.$emit('toggle');
    }
  }
};
</script>

<style scoped>
  /* transition for font awesome icon
   * only works for rotating icons 45 degrees  */
  .btn-toggle .fa {
    -webkit-transform-origin: 50% 50%;
    -ms-transform-origin    : 50% 50%;
    transform-origin        : 50% 50%;
    -webkit-transition      : all 750ms;
    transition              : all 750ms;
  }
  .btn-toggle.collapsed > .fa {
    -webkit-transform : rotate(-45deg);
    -ms-transform     : rotate(-45deg);
    transform         : rotate(-45deg);
  }
</style>
