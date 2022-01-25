<script>
import UserService from '../users/UserService';

// NOTE: must be used on a real html element (not <template>)
export default {
  name: 'HasRole',
  bind: function (el, binding, vnode) {
    if (!binding.value) { return; }

    $(el).hide();

    if (UserService.hasRole(binding.value)) {
      $(el).show();
    }
  }
};
</script>
