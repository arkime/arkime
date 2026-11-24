<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<!--
  The view/edit role and user controls for a shareable, matching what Views and
  Shortcuts show in their own edit forms. Used by the layout editor and by the
  Shareables tab so the two cannot drift apart.

  v-model is { viewRoles, editRoles, viewUsers, editUsers } with arrays
  throughout; the comma separated text of the user inputs is kept internally.
-->
<template>
  <div>
    <div class="mb-2">
      <RoleDropdown
        size="large"
        :roles="roles"
        class="d-inline me-1"
        :display-text="$t('common.rolesCanView')"
        :selected-roles="modelValue.viewRoles"
        @selected-roles-updated="(r) => update('viewRoles', r)" />
      <RoleDropdown
        size="large"
        :roles="roles"
        class="d-inline"
        :display-text="$t('common.rolesCanEdit')"
        :selected-roles="modelValue.editRoles"
        @selected-roles-updated="(r) => update('editRoles', r)" />
    </div>
    <div class="arkime-input-group arkime-input-group--fluid mb-2">
      <span class="arkime-input-label">{{ $t('common.usersCanView') }}</span>
      <input
        type="text"
        class="arkime-input-control"
        :value="viewUsersText"
        @input="updateUsers('viewUsers', $event.target.value)"
        :placeholder="$t('common.listOfUserIds')">
    </div>
    <div class="arkime-input-group arkime-input-group--fluid">
      <span class="arkime-input-label">{{ $t('common.usersCanEdit') }}</span>
      <input
        type="text"
        class="arkime-input-control"
        :value="editUsersText"
        @input="updateUsers('editUsers', $event.target.value)"
        :placeholder="$t('common.listOfUserIds')">
    </div>
  </div>
</template>

<script>
import RoleDropdown from '@common/RoleDropdown.vue';

export default {
  name: 'ShareInputs',
  emits: ['update:modelValue'],
  components: { RoleDropdown },
  props: {
    modelValue: {
      type: Object,
      required: true
    },
    roles: {
      type: Array,
      default: () => []
    }
  },
  data () {
    return {
      // what the user is typing, so a trailing comma is not eaten mid edit
      viewUsersText: (this.modelValue.viewUsers ?? []).join(','),
      editUsersText: (this.modelValue.editUsers ?? []).join(',')
    };
  },
  watch: {
    modelValue: {
      handler (share) {
        const view = (share.viewUsers ?? []).join(',');
        const edit = (share.editUsers ?? []).join(',');
        if (view !== this.splitUsers(this.viewUsersText).join(',')) { this.viewUsersText = view; }
        if (edit !== this.splitUsers(this.editUsersText).join(',')) { this.editUsersText = edit; }
      },
      deep: true
    }
  },
  methods: {
    splitUsers (str) {
      return (str || '').split(',').map(s => s.trim()).filter(s => s !== '');
    },
    update (key, value) {
      this.$emit('update:modelValue', { ...this.modelValue, [key]: value });
    },
    updateUsers (key, text) {
      this[key === 'viewUsers' ? 'viewUsersText' : 'editUsersText'] = text;
      this.update(key, this.splitUsers(text));
    }
  }
};
</script>
