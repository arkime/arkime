<template>
  <b-button
    :size="size"
    v-if="authMode && authMode === 'form'"
    class="ml-2"
    title="Logout"
    @click="logout"
    v-b-tooltip.hover
    variant="outline-warning">
    <span class="fa fa-sign-out fa-fw"></span>
  </b-button>
</template>

<script>
export default {
  name: 'Logout',
  props: {
    size: {
      type: String,
      default: 'md'
    }
  },
  data () {
    return {
      authMode: this.$constants.AUTHMODE
    };
  },
  methods: {
    logout () {
      fetch('/logout', {
        method: 'POST',
        credentials: 'include'
      }).finally(() => {
        location.reload();
      });
    }
  }
};
</script>
