<script>
export default {
  name: 'caretPos',
  bind: function (el, binding, vnode) {
    if (!binding.value) { binding.value = 0; }

    const setCaretPos = () => {
      if ('selectionStart' in el) {
        binding.value = el.selectionStart;
      } else if (document.selection) {
        // the user has highlighted text in the input
        el.focus();
        let selection = document.selection.createRange();
        let selectionLen = document.selection.createRange().text.length;
        selection.moveStart('character', -el.value.length);
        binding.value = selection.text.length - selectionLen;
      }

      // update the parent value
      vnode.context.caretPos = binding.value;
    };

    // register listeners
    el.addEventListener('click', setCaretPos);
    el.addEventListener('keydown', setCaretPos);

    el.destroy = () => {
      el.removeEventListener('click', setCaretPos);
      el.removeEventListener('keydown', setCaretPos);
    };
  },
  unbind: function (el, binding, vnode) {
    // cleanup listeners
    el.destroy();
  }
};
</script>
