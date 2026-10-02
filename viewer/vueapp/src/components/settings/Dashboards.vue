<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<!--
  Settings page for the Arkime page's dashboards. It owns a dashboard's name,
  description and sharing, and lets its widgets be reordered; what each widget
  shows is still edited on the Arkime page.
-->
<template>
  <div>
    <h3 class="d-flex align-center">
      <span class="flex-grow-1">{{ $t('settings.dashboards.title') }}</span>
    </h3>

    <p>
      {{ $t('settings.dashboards.info') }}
    </p>

    <div class="d-flex align-center mb-2">
      <div class="flex-grow-1">
        <v-text-field
          density="compact"
          variant="outlined"
          hide-details
          clearable
          prepend-inner-icon="mdi-magnify"
          v-model="search"
          :placeholder="$t('settings.dashboards.searchPlaceholder')" />
      </div>
    </div>

    <table class="arkime-table">
      <thead>
        <tr>
          <th>{{ $t('settings.dashboards.table-name') }}</th>
          <th>{{ $t('settings.dashboards.table-description') }}</th>
          <th>{{ $t('settings.dashboards.widgets') }}</th>
          <th>{{ $t('settings.dashboards.table-creator') }}</th>
          <th>{{ $t('settings.dashboards.table-view') }}</th>
          <th>{{ $t('settings.dashboards.table-edit') }}</th>
          <th>&nbsp;</th>
        </tr>
      </thead>
      <tbody>
        <tr
          v-for="item in visibleDashboards"
          :key="item.id">
          <td class="no-wrap">
            {{ item.name }}
            <label
              v-if="item.id === defaultId"
              class="arkime-badge arkime-badge--info ms-1 mb-0">
              {{ $t('settings.dashboards.isDefault') }}
            </label>
          </td>
          <td>{{ item.description }}</td>
          <td>{{ (item.data && item.data.widgets || []).length }}</td>
          <td class="no-wrap">
            {{ item.creator }}
            <label
              v-if="item.shared"
              class="arkime-badge arkime-badge--grey ms-1 mb-0 cursor-help"
              :id="`dashboardShared-${item.id}`">
              {{ $t('common.shared') }}
              <v-tooltip :activator="`#dashboardShared-${item.id}`">
                {{ $t('common.sharedTip', { creator: item.creator }) }}
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
              variant="flat"
              size="small"
              density="comfortable"
              icon
              :style="item.id === defaultId ? accentBtnStyle : tertiaryBtnStyle"
              class="ms-1"
              :id="`defaultDashboard-${item.id}`"
              @click="toggleDefault(item)">
              <v-icon :icon="item.id === defaultId ? 'mdi-star' : 'mdi-star-outline'" />
              <v-tooltip :activator="`#defaultDashboard-${item.id}`">
                {{ $t('settings.dashboards.defaultTip') }}
              </v-tooltip>
            </v-btn>
            <v-btn
              v-if="item.canDelete"
              color="error"
              variant="flat"
              size="small"
              density="comfortable"
              icon
              class="ms-1"
              :id="`deleteDashboard-${item.id}`"
              @click="deleteDashboard(item)">
              <v-icon icon="mdi-trash-can-outline" />
              <v-tooltip :activator="`#deleteDashboard-${item.id}`">
                {{ $t('settings.dashboards.deleteTip') }}
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
              :id="`editDashboard-${item.id}`"
              @click="editDashboard(item)">
              <v-icon icon="mdi-pencil" />
              <v-tooltip :activator="`#editDashboard-${item.id}`">
                {{ $t('settings.dashboards.editTip') }}
              </v-tooltip>
            </v-btn>
          </td>
        </tr>
      </tbody>
    </table>

    <p
      v-if="dashboards && !visibleDashboards.length && !listError"
      class="text-medium-emphasis mt-3">
      {{ search ? $t('settings.dashboards.noneFiltered') : $t('settings.dashboards.none') }}
    </p>

    <v-alert
      v-if="listError"
      type="error"
      variant="tonal"
      density="compact"
      class="mt-3">
      {{ listError }}
    </v-alert>

    <!-- edit form: name, description, sharing and widget order. What a widget
         shows is edited on the Arkime page, not here. -->
    <v-dialog
      v-model="showEditModal"
      max-width="800">
      <v-card>
        <v-card-title>
          {{ $t('settings.dashboards.editTitle') }}
        </v-card-title>
        <v-card-text>
          <div class="arkime-input-group arkime-input-group--fluid mb-2">
            <span class="arkime-input-label">{{ $t('settings.dashboards.name') }}</span>
            <input
              type="text"
              class="arkime-input-control"
              v-model="editName"
              maxlength="256"
              :placeholder="$t('settings.dashboards.namePlaceholder')">
          </div>
          <div class="arkime-input-group arkime-input-group--fluid mb-3">
            <span class="arkime-input-label">{{ $t('settings.dashboards.description') }}</span>
            <input
              type="text"
              class="arkime-input-control"
              v-model="editDescription"
              :placeholder="$t('settings.dashboards.descriptionPlaceholder')">
          </div>

          <!-- widget order -->
          <div class="mb-1">
            <strong>
              {{ $t('settings.dashboards.widgets') }}
              <span class="text-medium-emphasis small ms-1">{{ $t('settings.layoutEditor.dragHint') }}</span>
            </strong>
          </div>
          <div
            class="dashboards-widgets mb-3"
            ref="draggableWidgets">
            <div
              v-for="widget in editWidgets"
              :key="widget.id"
              class="dashboards-widget d-flex align-center">
              <v-icon
                icon="mdi-drag-vertical"
                size="small"
                class="text-medium-emphasis me-1" />
              <span class="flex-grow-1">{{ widgetLabel(widget) }}</span>
              <label class="arkime-badge arkime-badge--grey mb-0 ms-2">{{ viewModeLabel(widget.viewMode) }}</label>
            </div>
            <span
              v-if="!editWidgets.length"
              class="text-medium-emphasis small">
              {{ $t('settings.dashboards.noWidgets') }}
            </span>
          </div>

          <ShareInputs
            v-model="editShare"
            :roles="roles" />

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
            {{ $t('settings.dashboards.cancel') }}
          </v-btn>
          <v-btn
            color="success"
            variant="flat"
            @click="saveDashboard">
            {{ $t('settings.dashboards.save') }}
          </v-btn>
        </v-card-actions>
      </v-card>
    </v-dialog>
  </div>
</template>

<script>
import Sortable from 'sortablejs';
import { createShareableService } from '../users/ShareableService';
import { toV6Shape } from '../summary/dashboardConfig';
import FieldService from '../search/FieldService';
import UserService from '../users/UserService';
import { resolveMessage } from '@common/resolveI18nMessage';
import ShareInputs from './ShareInputs.vue';

const DashboardService = createShareableService('summaryConfig');

// same labels the widget edit modal offers on the Arkime page
const VIEW_MODE_LABELS = {
  bar: 'sessions.summary.barChart',
  pie: 'sessions.summary.pieChart',
  table: 'sessions.summary.tableView',
  intersection: 'sessions.summary.intersectionView',
  heatmap: 'sessions.summary.heatmapView',
  treemap: 'sessions.summary.treemapView',
  sankey: 'sessions.summary.sankeyView',
  connections: 'sessions.summary.connectionsView',
  timeline: 'sessions.summary.timelineView',
  map: 'sessions.summary.mapView',
  stats: 'sessions.summary.statsView',
  time: 'sessions.summary.timeView'
};

export default {
  name: 'Dashboards',
  emits: ['display-message'],
  components: { ShareInputs },
  data () {
    return {
      dashboards: undefined,
      search: '',
      listError: '',
      formError: '',
      sortable: undefined,
      showEditModal: false,
      editing: undefined,
      editName: '',
      editDescription: '',
      editWidgets: [],
      editShare: { viewRoles: [], editRoles: [], viewUsers: [], editUsers: [] },
      // Arkime theme-color v-btn styles. Vuetify :color can't take CSS vars.
      tertiaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-tertiary))',
        color: 'rgb(var(--v-theme-button-fg))'
      },
      accentBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-foreground-accent))',
        color: 'rgb(var(--v-theme-button-fg))'
      }
    };
  },
  computed: {
    roles () {
      return this.$store.state.roles;
    },
    defaultId () {
      return this.$store.state.user?.settings?.defaultDashboardId || '';
    },
    visibleDashboards () {
      const term = this.search?.trim().toLowerCase();
      if (!term) { return this.dashboards || []; }
      return (this.dashboards || []).filter((d) => {
        return `${d.name || ''} ${d.description || ''}`.toLowerCase().includes(term);
      });
    }
  },
  watch: {
    showEditModal (isOpen) {
      // the dialog content only exists once it is open
      if (isOpen) { this.$nextTick(this.initDragDrop); } else { this.destroyDragDrop(); }
    }
  },
  created () {
    this.loadData();
  },
  beforeUnmount () {
    this.destroyDragDrop();
  },
  methods: {
    /* yours first, then the ones shared with you, each still in the name
       order the API returned */
    async loadData () {
      try {
        // no pager here, so ask for more than the API's default page of 50
        const response = await DashboardService.list({ viewOnly: false, length: 1000 });
        const data = response.data || [];
        this.listError = '';
        this.dashboards = [...data.filter(d => !d.shared), ...data.filter(d => d.shared)];
      } catch (error) {
        this.dashboards = [];
        this.listError = resolveMessage(error, this.$t);
      }
    },
    viewModeLabel (viewMode) {
      const key = VIEW_MODE_LABELS[viewMode];
      return key ? this.$t(key) : viewMode;
    },
    /* what the widget is titled on the dashboard: its own title, else the
       field it aggregates, else what kind of widget it is */
    widgetLabel (widget) {
      return widget.title ||
        FieldService.getField(widget.field, true)?.friendlyName ||
        widget.field ||
        this.viewModeLabel(widget.viewMode);
    },
    /* widget order is the order they are laid out in, so let it be dragged */
    initDragDrop () {
      if (!this.$refs.draggableWidgets || this.sortable) { return; }
      this.sortable = Sortable.create(this.$refs.draggableWidgets, {
        animation: 100,
        onEnd: (e) => {
          if (e.oldIndex === e.newIndex) { return; }
          const moved = this.editWidgets.splice(e.oldIndex, 1)[0];
          this.editWidgets.splice(e.newIndex, 0, moved);
        }
      });
    },
    destroyDragDrop () {
      if (this.sortable) {
        this.sortable.destroy();
        this.sortable = undefined;
      }
    },
    async toggleDefault (item) {
      const newId = this.defaultId === item.id ? '' : item.id;
      try {
        await UserService.saveSettings({
          ...(this.$store.state.user?.settings || {}),
          defaultDashboardId: newId
        });
      } catch (error) {
        this.$emit('display-message', { msg: resolveMessage(error, this.$t), type: 'danger' });
      }
    },
    editDashboard (item) {
      this.formError = '';
      this.editing = item;
      this.editName = item.name ?? '';
      this.editDescription = item.description ?? '';
      this.editWidgets = JSON.parse(JSON.stringify(item.data?.widgets ?? []));
      this.editShare = {
        viewRoles: [...(item.viewRoles ?? [])],
        editRoles: [...(item.editRoles ?? [])],
        viewUsers: [...(item.viewUsers ?? [])],
        editUsers: [...(item.editUsers ?? [])]
      };
      this.showEditModal = true;
    },
    /* the widgets go back with the legacy v6 shape rebuilt from their new
       order, so the two views of the same dashboard cannot drift apart */
    async saveDashboard () {
      this.editName = this.editName.trim();
      if (!this.editName) {
        this.formError = this.$t('settings.dashboards.nameRequired');
        return;
      }

      // a v6 shaped dashboard has fields[] and no widgets[], so there is
      // nothing here to reorder -- leave its data alone rather than writing an
      // empty widget list over it
      const hadWidgets = Array.isArray(this.editing.data?.widgets);
      const body = {
        name: this.editName,
        description: this.editDescription,
        ...this.editShare
      };
      if (hadWidgets) {
        body.data = {
          ...(this.editing.data || {}),
          widgets: this.editWidgets,
          ...toV6Shape(this.editWidgets)
        };
      }

      try {
        const response = await DashboardService.update(this.editing.id, body);
        this.showEditModal = false;
        this.$emit('display-message', { msg: resolveMessage(response, this.$t), type: 'success' });
        this.loadData();
      } catch (error) {
        this.formError = resolveMessage(error, this.$t);
      }
    },
    async deleteDashboard (item) {
      if (!window.confirm(this.$t('settings.dashboards.deleteConfirm', { name: item.name }))) {
        return;
      }

      try {
        const response = await DashboardService.delete(item.id);
        this.dashboards = this.dashboards.filter(d => d.id !== item.id);
        if (this.defaultId === item.id) { await this.toggleDefault(item); }
        this.$emit('display-message', { msg: resolveMessage(response, this.$t), type: 'success' });
      } catch (error) {
        this.$emit('display-message', { msg: resolveMessage(error, this.$t), type: 'danger' });
      }
    }
  }
};
</script>

<style scoped>
.dashboards-widgets {
  max-height: 260px;
  overflow-y: auto;
}
.dashboards-widget {
  padding: 0.25rem 0.5rem;
  border: 1px solid rgb(var(--v-theme-border-color, var(--v-theme-on-surface)), 0.2);
  border-radius: 3px;
  margin-bottom: 0.25rem;
  cursor: grab;
  user-select: none;
}
.dashboards-widget:active {
  cursor: grabbing;
}
</style>
