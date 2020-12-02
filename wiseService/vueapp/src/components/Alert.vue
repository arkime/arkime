<template>
  <transition name="fade">
    <!-- alert -->
    <div
      v-if="alertMessage"
      class="alert"
      :class="variant"
    >
      <span v-if="variant === 'alert-danger'" class="fa fa-exclamation-triangle"></span>
      <span v-if="variant === 'alert-success'" class="fa fa-check"></span>
      <span v-if="variant === 'alert-warning'" class="fa fa-exclamation-circle"></span>

      &nbsp;
      {{ alertMessage }}
      <button type="button"
        class="close cursor-pointer"
        @click="clear">
        <span>&times;</span>
      </button>
    </div> <!-- /alert -->
  </transition>
</template>

<script>
export default {
  name: 'Alert',
  props: ['initialAlert', 'variant'],
  data: function () {
    return {
      alertMessage: this.initialAlert || '',
      timeLimit: 5000,
      timeoutID: ''
    };
  },
  methods: {
    clear: function () {
      this.alertMessage = '';
      this.$emit('clear-initialAlert');
      clearTimeout(this.timeoutID);
      this.timeoutID = '';
    }
  },
  watch: {
    initialAlert: {
      immediate: true,
      handler (newVal, OldVal) {
        if (newVal) {
          if (this.timeoutID) {
            this.clear();
          }
          this.alertMessage = newVal;
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
