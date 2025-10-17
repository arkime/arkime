<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<script>
export default {
  name: 'Focus',
  mounted: function (el, binding, vnode) {
    if (binding.value) { el.focus(); }
  },
  updated: function (el, binding, vnode) {
    if (binding.value && binding.value !== binding.oldValue) {
      el.focus();
    }
  }
};
</script>
