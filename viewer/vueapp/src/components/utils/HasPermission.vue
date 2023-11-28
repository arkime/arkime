<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<script>
import UserService from '../users/UserService';

// NOTE: must be used on a real html element (not <template>)
export default {
  name: 'HasPermission',
  bind: function (el, binding, vnode) {
    if (!binding.value) { return; }

    $(el).hide();

    if (UserService.hasPermission(binding.value)) {
      $(el).show();
    }
  }
};
</script>
