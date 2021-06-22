<script>
import UserService from '../users/UserService';

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
