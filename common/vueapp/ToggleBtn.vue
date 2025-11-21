<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <a
    role="button"
    title="toggle"
    @click="toggle"
    :class="btnClass"
    class="btn btn-sm btn-toggle">
    <span class="fa fa-close" />
  </a>
</template>

<script>
export default {
  name: 'ToggleBtn',
  emits: ['toggle'],
  props: {
    opened: {
      type: Boolean,
      default: false
    }
  },
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

<style>
/* override btn-sm style to better fit in table rows */
.btn-toggle.btn-sm {
  padding: 2px 6px;
  font-size: 12px;
}

/* use bootstrap theme styles for button */
.btn-toggle {
  border-color: var(--bs-primary) !important;
  background-color: var(--bs-primary) !important;
}
.btn-toggle:hover {
  border-color: var(--bs-primary) !important;
  background-color: var(--bs-primary) !important;
}
.btn-toggle:not(.collapsed) {
  border-color: var(--bs-warning) !important;
  background-color: var(--bs-warning) !important;
}
.btn-toggle:not(.collapsed):hover {
  border-color: var(--bs-warning) !important;
  background-color: var(--bs-warning) !important;
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
