<script>
import UserService from '../UserService';

export default {
  name: 'HasPermission',
  bind: function (el, binding, vnode) {
    if (!binding.value) { return; }

    el.style.display = 'none'; // hide the element by default

    UserService.hasPermission(binding.value)
      .then((response) => {
        if (response) {
          el.style.display = 'inline-block';
        }
      });
  }
};
</script>
