<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<script>
import UserService from './UserService';

// NOTE: must be used on a real html element (not <template>)
// NOTE: assumes we are using bootstrap and hides the element with d-none
export default {
  name: 'HasRole',
  bind (el, binding) {
    if (!binding.value) { return; }
    if (!binding.value.roles) { return; }

    el.classList.add('d-none');

    if (UserService.hasRole(binding.value.user, binding.value.roles)) {
      el.classList.remove('d-none');
    }
  }
};
</script>
