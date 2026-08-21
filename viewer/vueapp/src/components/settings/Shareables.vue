<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <h3 class="d-flex align-center">
      <span class="flex-grow-1">{{ $t('settings.shareables.title') }}</span>
    </h3>

    <p>
      {{ $t('settings.shareables.info') }}
    </p>

    <div class="d-flex align-center">
      <div class="flex-grow-1 me-2">
        <v-text-field
          density="compact"
          variant="outlined"
          hide-details
          clearable
          prepend-inner-icon="mdi-magnify"
          :model-value="query.search"
          @update:model-value="updateSearch"
          :placeholder="$t('settings.shareables.searchPlaceholder')" />
      </div>
      <arkime-paging
        v-if="shareables"
        :length-default="size"
        :records-total="recordsTotal"
        :records-filtered="recordsFiltered"
        @change-paging="changePaging" />
    </div>

    <table class="arkime-table">
      <thead>
        <tr>
          <th
            v-for="col in sortableColumns"
            :key="col.field"
            class="cursor-pointer"
            @click.self="sortBy(col.field)">
            {{ $t(col.label) }}
            <v-icon
              icon="mdi-chevron-up"
              v-show="query.sortField === col.field && !query.desc" />
            <v-icon
              icon="mdi-chevron-down"
              v-show="query.sortField === col.field && query.desc" />
          </th>
          <th>{{ $t('settings.shareables.table-view') }}</th>
          <th>{{ $t('settings.shareables.table-edit') }}</th>
          <th>&nbsp;</th>
        </tr>
      </thead>
      <tbody>
        <tr
          v-for="(item, index) in shareables"
          :key="item.id">
          <td class="no-wrap">
            <label class="arkime-badge arkime-badge--grey mb-0">{{ item.type }}</label>
          </td>
          <td>{{ item.name }}</td>
          <td>{{ item.description }}</td>
          <td class="no-wrap">
            {{ item.creator }}
            <label
              v-if="item.shared"
              class="arkime-badge arkime-badge--grey ms-1 mb-0 cursor-help"
              :id="`shareableShared-${item.id}`">
              {{ $t('settings.shareables.shared') }}
              <v-tooltip :activator="`#shareableShared-${item.id}`">
                {{ $t('settings.shareables.sharedTip', { creator: item.creator }) }}
              </v-tooltip>
            </label>
          </td>
          <td>
            <label
              v-for="who in item.viewUsers"
              :key="`vu-${who}`"
              class="arkime-badge arkime-badge--grey me-1 mb-0">{{ who }}</label>
            <label
              v-for="who in item.viewRoles"
              :key="`vr-${who}`"
              class="arkime-badge arkime-badge--info me-1 mb-0">{{ who }}</label>
          </td>
          <td>
            <label
              v-for="who in item.editUsers"
              :key="`eu-${who}`"
              class="arkime-badge arkime-badge--grey me-1 mb-0">{{ who }}</label>
            <label
              v-for="who in item.editRoles"
              :key="`er-${who}`"
              class="arkime-badge arkime-badge--info me-1 mb-0">{{ who }}</label>
          </td>
          <td class="text-end no-wrap">
            <v-btn
              v-if="item.canDelete"
              color="error"
              variant="flat"
              size="small"
              density="comfortable"
              icon
              class="ms-1"
              :id="`deleteShareable-${item.id}`"
              @click="deleteShareable(item, index)">
              <v-icon icon="mdi-trash-can-outline" />
              <v-tooltip :activator="`#deleteShareable-${item.id}`">
                {{ $t('settings.shareables.deleteTip') }}
              </v-tooltip>
            </v-btn>
            <v-btn
              v-if="item.canEdit"
              variant="flat"
              size="small"
              density="comfortable"
              icon
              :style="tertiaryBtnStyle"
              class="ms-1"
              :id="`editShareable-${item.id}`"
              @click="editShareable(item)">
              <v-icon icon="mdi-pencil" />
              <v-tooltip :activator="`#editShareable-${item.id}`">
                {{ $t('settings.shareables.editTip') }}
              </v-tooltip>
            </v-btn>
          </td>
        </tr>
      </tbody>
    </table>

    <p
      v-if="shareables && !shareables.length && !listError"
      class="text-medium-emphasis mt-3">
      {{ query.search ? $t('settings.shareables.noneFiltered') : $t('settings.shareables.none') }}
    </p>

    <v-alert
      v-if="listError"
      type="error"
      variant="tonal"
      density="compact"
      class="mt-3">
      {{ listError }}
    </v-alert>

    <!-- edit form: metadata and sharing only, never the item's data -->
    <v-dialog
      v-model="showEditModal"
      max-width="800">
      <v-card>
        <v-card-title>
          {{ $t('settings.shareables.editTitle') }}
        </v-card-title>
        <v-card-text>
          <div class="arkime-input-group arkime-input-group--fluid mb-2">
            <span class="arkime-input-label">{{ $t('settings.shareables.name') }}</span>
            <input
              type="text"
              class="arkime-input-control"
              v-model="editName"
              :placeholder="$t('settings.shareables.namePlaceholder')">
          </div>
          <div class="arkime-input-group arkime-input-group--fluid mb-2">
            <span class="arkime-input-label">{{ $t('settings.shareables.description') }}</span>
            <input
              type="text"
              class="arkime-input-control"
              v-model="editDescription"
              :placeholder="$t('settings.shareables.descriptionPlaceholder')">
          </div>
          <div class="mb-2">
            <RoleDropdown
              size="large"
              :roles="roles"
              class="d-inline me-1"
              :display-text="$t('common.rolesCanView')"
              :selected-roles="editViewRoles"
              @selected-roles-updated="(r) => { editViewRoles = r; }" />
            <RoleDropdown
              size="large"
              :roles="roles"
              class="d-inline"
              :display-text="$t('common.rolesCanEdit')"
              :selected-roles="editEditRoles"
              @selected-roles-updated="(r) => { editEditRoles = r; }" />
          </div>
          <div class="arkime-input-group arkime-input-group--fluid mb-2">
            <span class="arkime-input-label">{{ $t('settings.shareables.usersCanView') }}</span>
            <input
              type="text"
              class="arkime-input-control"
              v-model="editViewUsers"
              :placeholder="$t('settings.shareables.usersPlaceholder')">
          </div>
          <div class="arkime-input-group arkime-input-group--fluid">
            <span class="arkime-input-label">{{ $t('settings.shareables.usersCanEdit') }}</span>
            <input
              type="text"
              class="arkime-input-control"
              v-model="editEditUsers"
              :placeholder="$t('settings.shareables.usersPlaceholder')">
          </div>

          <v-alert
            v-if="formError"
            type="error"
            variant="tonal"
            density="compact"
            class="mt-3">
            {{ formError }}
          </v-alert>
        </v-card-text>
        <v-card-actions>
          <v-spacer />
          <v-btn
            variant="text"
            @click="showEditModal = false">
            {{ $t('settings.shareables.cancel') }}
          </v-btn>
          <v-btn
            color="success"
            variant="flat"
            @click="saveShareable">
            {{ $t('settings.shareables.save') }}
          </v-btn>
        </v-card-actions>
      </v-card>
    </v-dialog>
  </div>
</template>

<script>
import { ShareableService } from '../users/ShareableService';
import { resolveMessage } from '@common/resolveI18nMessage';
import ArkimePaging from '@common/Pagination.vue';
import RoleDropdown from '@common/RoleDropdown.vue';

export default {
  name: 'Shareables',
  emits: ['display-message'],
  components: {
    ArkimePaging,
    RoleDropdown
  },
  data () {
    return {
      shareables: undefined,
      listError: '',
      formError: '',
      size: 50,
      start: 0,
      recordsTotal: 0,
      recordsFiltered: 0,
      query: {
        search: '',
        sortField: 'name',
        desc: false
      },
      // roles/users hold arrays, so only the scalar columns are sortable
      sortableColumns: [
        { field: 'type', label: 'settings.shareables.table-type' },
        { field: 'name', label: 'settings.shareables.table-name' },
        { field: 'description', label: 'settings.shareables.table-description' },
        { field: 'creator', label: 'settings.shareables.table-creator' }
      ],
      showEditModal: false,
      editing: undefined,
      editName: '',
      editDescription: '',
      editViewRoles: [],
      editEditRoles: [],
      editViewUsers: '',
      editEditUsers: '',
      // Arkime theme-color v-btn styles. Vuetify :color can't take CSS vars.
      tertiaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-tertiary))',
        color: 'rgb(var(--v-theme-button-fg))'
      }
    };
  },
  computed: {
    roles () {
      return this.$store.state.roles;
    }
  },
  watch: {
    'query.search' () {
      this.start = 0; // a new search starts back at the first page
      this.loadData();
    }
  },
  created () {
    this.loadData();
  },
  methods: {
    /* a shareable is listed if the user can view OR edit it, so viewOnly is off */
    async loadData () {
      try {
        const response = await ShareableService.list({
          viewOnly: false,
          searchTerm: this.query.search || undefined,
          sort: this.query.sortField,
          desc: this.query.desc,
          start: this.start,
          length: this.size
        });
        this.listError = '';
        this.shareables = response.data;
        this.recordsTotal = response.recordsTotal;
        this.recordsFiltered = response.recordsFiltered;
      } catch (error) {
        this.shareables = [];
        this.listError = resolveMessage(error, this.$t);
      }
    },
    changePaging (newParams) {
      this.size = newParams.length;
      this.start = newParams.start;
      this.loadData();
    },
    /* clicking the sorted column flips direction, a new column starts ascending */
    sortBy (field) {
      this.query.desc = this.query.sortField === field ? !this.query.desc : false;
      this.query.sortField = field;
      this.loadData();
    },
    updateSearch (newSearch) {
      this.query.search = newSearch ?? ''; // NOTE watch will trigger loadData
    },
    editShareable (item) {
      this.formError = '';
      this.editing = item;
      this.editName = item.name ?? '';
      this.editDescription = item.description ?? '';
      this.editViewRoles = [...(item.viewRoles ?? [])];
      this.editEditRoles = [...(item.editRoles ?? [])];
      this.editViewUsers = (item.viewUsers ?? []).join(',');
      this.editEditUsers = (item.editUsers ?? []).join(',');
      this.showEditModal = true;
    },
    /* only name/description/sharing are sent, the API keeps data as it was */
    async saveShareable () {
      if (!this.editName) {
        this.formError = this.$t('settings.shareables.namePlaceholderRequired');
        return;
      }

      try {
        const response = await ShareableService.update(this.editing.id, {
          name: this.editName,
          description: this.editDescription,
          viewRoles: this.editViewRoles,
          editRoles: this.editEditRoles,
          viewUsers: this.splitUsers(this.editViewUsers),
          editUsers: this.splitUsers(this.editEditUsers)
        });
        this.showEditModal = false;
        this.$emit('display-message', { msg: resolveMessage(response, this.$t), type: 'success' });
        this.loadData();
      } catch (error) {
        this.formError = resolveMessage(error, this.$t);
      }
    },
    async deleteShareable (item, index) {
      if (!window.confirm(this.$t('settings.shareables.deleteConfirm', { name: item.name }))) {
        return;
      }

      try {
        const response = await ShareableService.delete(item.id);
        this.shareables.splice(index, 1);
        this.recordsTotal--;
        this.recordsFiltered--;
        this.$emit('display-message', { msg: resolveMessage(response, this.$t), type: 'success' });
      } catch (error) {
        this.$emit('display-message', { msg: resolveMessage(error, this.$t), type: 'danger' });
      }
    },
    splitUsers (str) {
      return (str || '').split(',').map(s => s.trim()).filter(s => s !== '');
    }
  }
};
</script>
