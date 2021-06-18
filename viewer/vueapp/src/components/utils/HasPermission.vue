<script>
import UserService from '../users/UserService';

export default {
  name: 'HasPermission',
  bind: function (el, binding, vnode) {
    if (!binding.value) { return; }

    el.style.display = 'none';

    if (UserService.hasPermission(binding.value)) {
      el.style.display = 'block';
    }
  }
};
</script>
