<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<script>
export default {
  name: 'focus',
  mounted: function (el, binding, vnode) {
    if (binding.value) { el.focus(); }
  },
  // TODO: toby-rm? was update -> updated or should be beforeUpdate?
  updated: function (el, binding, vnode) {
    if (binding.value && binding.value !== binding.oldValue) {
      el.focus();
    }
  }
};
</script>
