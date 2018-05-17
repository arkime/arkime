<script>
import UserService from '../users/UserService';

export default {
  name: 'HasPermission',
  bind: function (el, binding, vnode) {
    if (!binding.value) { return; }

    $(el).hide();

    UserService.hasPermission(binding.value)
      .then((response) => {
        if (response) { $(el).show(); }
      });
  }
};
</script>
