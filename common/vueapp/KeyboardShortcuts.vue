<template>
  <div data-testid="shortcut-test">
    <!-- shortcuts help display -->
    <transition :name="shortcutsHelpTransition">
      <div
        v-if="displayHelp"
        id="shortcutsHelp"
        class="shortcuts-help"
        :class="shortcutsClass"
        data-testid="shortcuts-help-content">
        <div class="pl-2 pt-1 pb-1">
          <!-- close shortcuts help -->
          <a @click="close"
            title="Close shortcuts help"
            class="pull-right mr-2 cursor-pointer">
            <span class="fa fa-close" />
          </a>
          <!-- slot for keyboard shortcut help content -->
          <slot name="content"></slot>
        </div>
      </div>
    </transition> <!-- /shortcuts help display -->
    <!-- shortcuts help toggle btn -->
    <transition :name="shortcutsBtnTransition">
      <div @click="open"
        class="cursor-pointer"
        :class="shortcutsClass"
          title="Display shortcuts help"
        v-if="shiftHold && !displayHelp">
        <span class="fa fa-question fa-fw" />
      </div>
    </transition>
  </div>
</template>

<script>
// NOTE: the parent application still needs to watch for shift + key clicks to initiate events
// NOTE: emits shift-hold-change event when shift hold is changed (watches window with event
// listener). watch for this change to update other parts of the app on shift key hold
export default {
  name: 'KeyboardShortcuts',
  props: {
    shortcutsClass: { // the class to apply to the help dialog and the btn
      // it should include position, z-index, color, border-radius, and shadow
      // NOTE: it is assumed that the width of the dialog is 465px
      type: String,
      required: true
    },
    shortcutsBtnTransition: { // the transition class name used to animate btn enter/leave
      type: String,
      default: 'shortcuts-slide' // default slides in from the left
    },
    shortcutsHelpTransition: { // the transition class name used to animate help dialog enter/leave
      type: String,
      default: 'shortcuts-slide-long' // default sides in from the left
    }
  },
  data () {
    return {
      shiftHold: false,
      displayHelp: false
    };
  },
  watch: {
    shiftHold (val) {
      this.$emit('shift-hold-change', val);
    }
  },
  mounted () {
    window.addEventListener('keyup', this.escEvent);

    const inputs = ['input', 'select', 'textarea'];

    window.addEventListener('keyup', (e) => {
      const activeElement = document.activeElement;

      if (e.keyCode === 16) { // shift
        this.shiftHold = false;
        return;
      }

      // quit if the user is in an input or not holding the shift key
      if (!this.shiftHold || (activeElement && inputs.indexOf(activeElement.tagName.toLowerCase()) !== -1)) {
        return;
      }

      if (e.keyCode === 191) { // /
        this.displayHelp ? this.close() : this.open();
      }
    });

    window.addEventListener('keydown', (e) => {
      const activeElement = document.activeElement;

      // quit if the user is in an input or not holding the shift key
      if (activeElement && inputs.indexOf(activeElement.tagName.toLowerCase()) !== -1) {
        return;
      }

      if (e.keyCode === 16) { // shift
        this.shiftHold = true;
      } else if (e.keyCode === 27) { // esc
        this.shiftHold = false;
      }
    });

    // if the user focus is not in the web page, remove shift hold
    window.addEventListener('blur', (e) => {
      this.shiftHold = false;
    });
  },
  methods: {
    open () {
      this.displayHelp = true;
      document.addEventListener('mouseup', this.isOutsideClick);
    },
    close () {
      this.displayHelp = false;
      document.removeEventListener('mouseup', this.isOutsideClick);
    },
    escEvent (e) {
      if (e.keyCode === 27) { // esc
        this.close();
      }
    },
    isOutsideClick (e) {
      let element = e.target;
      let clickInShortcutsHelp = false;

      while (element) {
        if (element.id === 'shortcutsHelp') {
          clickInShortcutsHelp = true;
          break;
        }
        element = element.parentElement;
      }

      if (!clickInShortcutsHelp) {
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
.shortcuts-help {
  width: 465px;
}

/* keyboard shortcuts help animation */
.shortcuts-slide-enter-active, .shortcuts-slide-leave-active {
  transition: all 0.5s ease;
}
.shortcuts-slide-long-enter-active, .shortcuts-slide-long-leave-active {
  transition: all 1s ease;
}
.shortcuts-slide-enter, .shortcuts-slide-leave,
.shortcuts-slide-long-enter, .shortcuts-slide-long-leave {
  transform: translateX(-465px);
}
</style>
