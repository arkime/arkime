<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<script>
export default {
  name: 'focus',
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
