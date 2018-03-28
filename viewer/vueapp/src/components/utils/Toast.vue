<template>

  <!-- alert -->
  <div v-if="message"
    class="alert alert-sm alert-dismissible"
    :class="alertClass">

    <!-- icon -->
    <span class="fa fa-check"
      v-if="type === 'success'">
    </span>
    <span class="fa fa-info-circle"
      v-if="type === 'info'">
    </span>
    <span class="fa fa-exclamation-triangle"
      v-if="type === 'warning' || type === 'danger'">
    </span> <!-- /icon -->
    <!-- message -->
    &nbsp;{{ message || 'undefined message' }}
    <!-- /message -->
    <!-- dismiss alert button -->
    <button type="button"
      class="close"
      @click="done()">
      <span>
        &times;
      </span>
    </button> <!-- /dismiss alert button -->

  </div> <!-- /alert -->

</template>

<script>
let timeout;

export default {
  name: 'MolochToast',
  props: {
    message: String,
    done: Function,
    duration: {
      type: Number,
      default: 5000
    },
    type: {
      type: String,
      default: 'info'
    }
  },
  watch: {
    message: function (newVal, oldVal) {
      if (newVal !== oldVal) {
        this.dismissAfter();
      }
    }
  },
  computed: {
    alertClass: function () {
      return `alert-${this.type}`;
    }
  },
  created: function () {
    if (this.message) {
      this.dismissAfter();
    }
  },
  methods: {
    /* helper functions ------------------------------------------ */
    dismissAfter: function () {
      if (timeout) { clearTimeout(timeout); }

      timeout = setTimeout(() => {
        this.done();
      }, this.duration);
    }
  },
  beforeDestroy: function () {
    if (timeout) { clearTimeout(timeout); }
  }
};
</script>
