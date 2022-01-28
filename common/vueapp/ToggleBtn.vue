<template>

  <a role="button"
    @click="toggle"
    :class="btnClass"
    class="btn btn-sm btn-toggle">
    <span class="fa fa-close" />
  </a>

</template>

<script>
export default {
  name: 'ToggleBtn',
  props: ['opened'],
  computed: {
    btnClass () {
      return {
        'btn-warning': this.opened,
        'collapsed btn-primary': !this.opened
      };
    }
  },
  methods: {
    toggle () {
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

/* use bootstrap theme styles for button */
.btn-toggle {
  border-color: var(--primary);
  background-color: var(--primary);
}
.btn-toggle:hover {
  border-color: var(--primary);
  background-color: var(--primary);
}
.btn-toggle:not(.collapsed) {
  border-color: var(--warning);
  background-color: var(--warning);
}
.btn-toggle:not(.collapsed):hover {
  border-color: var(--warning);
  background-color: var(--warning);
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
