<script>
export default {
  name: 'focusInput',
  inserted: function (el, binding, vnode) {
    if (binding.value) { el.focus(); }
  },
  update: function (el, binding, vnode) {
    if (binding.value && binding.value !== binding.oldValue) {
      el.focus();
    }
  }
};
</script>
