<template>
  <b-button
    v-if="authMode && authMode === 'form'"
    class="ml-2"
    @click="logout"
    variant="outline-danger">
    Logout
    <span class="fa fa-sign-out ml-1"></span>
  </b-button>
</template>

<script>
export default {
  name: 'Logout',
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
