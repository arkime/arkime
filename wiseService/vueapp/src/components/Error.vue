<template>
  <transition name="fade">
    <!-- page error -->
    <div v-if="error"
      class="alert alert-danger">
      <span class="fa fa-exclamation-triangle">
      </span>&nbsp;
      {{ error }}
      <button type="button"
        class="close cursor-pointer"
        @click="clear">
        <span>&times;</span>
      </button>
    </div> <!-- /page error -->
  </transition>
</template>

<script>
export default {
  name: 'Error',
  props: ['initialError'],
  data: function () {
    return {
      error: this.initialError || '',
      timeLimit: 5000,
      timeoutID: ''
    };
  },
  methods: {
    clear: function () {
      this.error = '';
      this.$emit('clear-initialError');
      clearTimeout(this.timeoutID);
      this.timeoutID = '';
    }
  },
  watch: {
    initialError: {
      immediate: true,
      handler (newVal, OldVal) {
        if (newVal) {
          if (this.timeoutID) {
            this.clear();
          }
          this.error = newVal;
          this.timeoutID = setTimeout(() => this.clear(), this.timeLimit);
        }
      }
    }
  }
};
</script>

<style>
.fade-enter-active, .fade-leave-active {
  transition: opacity .4s;
}
.fade-enter, .fade-leave-to {
  opacity: 0;
}
</style>
