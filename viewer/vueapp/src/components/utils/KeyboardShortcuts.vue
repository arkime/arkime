<template>

  <div id="keyboardShortcutsHelp"
    class="keyboard-shortcuts-help">

    <div class="pl-2 pt-1 pb-1">

      <a class="pull-right mr-2"
        href="javascript:void(0)"
        @click="close">
        <span class="fa fa-close">
        </span>
      </a>

      <code>'q'</code> - set focus to query bar
      <br>
      <code>'t'</code> - set focus to time range selector
      <br>
      <code>'s'</code> - jump to the Sessions page
      <br>
      <code>'v'</code> - jump to the SPI View page
      <br>
      <code>'g'</code> - jump to the SPI Graph page
      <br>
      <code>'c'</code> - jump to the Connections page
      <br>
      <code>'h'</code> - jump to the Moloch Help page
      <br>
      <code>'?'</code> - shows you this dialog, but I guess you already knew that

    </div>

  </div>

</template>

<script>
// TODO transition
export default {
  name: 'MolochKeyboardShortcuts',
  mounted: function () {
    window.addEventListener('keyup', this.escEvent);
    document.addEventListener('mouseup', this.isOutsideClick);
  },
  methods: {
    close: function () {
      this.$store.commit('setDisplayKeyboardShortcutsHelp', false);
    },
    escEvent: function (event) {
      if (event.keyCode === 27) { // esc
        this.close();
      }
    },
    isOutsideClick: function (event) {
      let element = $('#keyboardShortcutsHelp');
      if (!$(element).is(event.target) &&
        $(element).has(event.target).length === 0) {
        this.close();
      }
    }
  },
  beforeDestroy: function () {
    window.removeEventListener('keyup', this.escEvent);
    document.removeEventListener('mouseup', this.isOutsideClick);
  }
};
</script>

<style scoped>
.keyboard-shortcuts-help {
  position: absolute;
  top: 155px;
  width: 465px;
  z-index: 1;
  background: var(--color-background, white);
  border: 1px solid var(--color-gray);
  border-left: none;
  border-radius: 0 4px 4px 0 ;
  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}
</style>
