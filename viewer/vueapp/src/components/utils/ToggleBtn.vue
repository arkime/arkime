<template>

  <a @click="toggle"
    :class="btnClass"
    class="btn btn-sm btn-toggle">
    <span class="fa fa-close">
    </span>
  </a>

</template>

<script>
export default {
  name: 'ToggleBtn',
  props: [ 'opened' ],
  computed: {
    btnClass: function () {
      return {
        'btn-danger': this.opened,
        'collapsed btn-theme-tertiary': !this.opened
      };
    }
  },
  methods: {
    toggle: function () {
      this.$emit('toggle');
    }
  }
};
</script>

<style scoped>
/* override btn-sm style to better fit in table rows */
.btn-sm {
  padding: 3px 8px;
  font-size: 12px;
}

/* transition for font awesome icon
 * only works for rotating icons 45 degrees  */
.btn-toggle .fa {
  -webkit-transform-origin: 50% 50%;
          transform-origin: 50% 50%;

  -webkit-transition: all 750ms;
          transition: all 750ms;
}
.btn-toggle.collapsed > .fa {
  -webkit-transform: rotate(-45deg);
      -ms-transform: rotate(-45deg);
          transform: rotate(-45deg);
}
</style>
