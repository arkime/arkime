<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-inline-block">
    <v-menu
      v-model="menuOpen"
      :close-on-content-click="false"
      :disabled="disabled"
      location="bottom start">
      <template #activator="{ props: activatorProps }">
        <v-btn
          v-bind="activatorProps"
          size="large"
          variant="outlined"
          color="secondary"
          :disabled="disabled"
          class="notifier-trigger text-none">
          {{ displayText || getNotifiersStr(localSelectedNotifiers) }}
          <v-icon
            end
            icon="mdi:mdi-menu-down" />
          <v-tooltip
            v-if="tooltip"
            activator="parent"
            location="top">
            {{ tooltip }}
          </v-tooltip>
        </v-btn>
      </template>
      <v-list
        density="compact"
        class="notifier-list">
        <!-- search -->
        <div class="px-2 pt-2 pb-1 position-sticky notifier-search-row">
          <v-text-field
            v-focus="focus"
            density="compact"
            variant="outlined"
            hide-details
            clearable
            prepend-inner-icon="fa:fa-search"
            :model-value="searchTerm"
            @update:model-value="searchNotifiersLocal"
            :placeholder="$t('settings.notifiers.searchTermPlaceholder')" />
        </div>
        <v-divider />

        <template v-if="filteredNotifiers && filteredNotifiers.length">
          <v-list-item
            v-for="notifier in filteredNotifiers"
            :key="notifier.id"
            @click.stop="toggleNotifier(notifier.id)">
            <span class="d-flex align-center">
              <input
                type="checkbox"
                class="dropdown-check-input me-2 cursor-pointer"
                :checked="localSelectedNotifiers.includes(notifier.id)"
                @click.stop="toggleNotifier(notifier.id)">
              {{ notifier.name }} ({{ notifier.type }})
            </span>
          </v-list-item>
          <!-- previously deleted notifiers (still selected but no longer in the list) -->
          <template
            v-for="notifierId in localSelectedNotifiers"
            :key="notifierId">
            <v-list-item
              v-if="!notifiers.find(n => n.id === notifierId)"
              @click.stop="toggleNotifier(notifierId)">
              <span class="d-flex align-center">
                <input
                  type="checkbox"
                  class="dropdown-check-input me-2 cursor-pointer"
                  :checked="true"
                  @click.stop="toggleNotifier(notifierId)">
                {{ notifierId }}
                <v-icon
                  icon="mdi-close-circle"
                  class="cursor-help ms-2"
                  :title="$t('settings.notifiers.missingNotifier')" />
              </span>
            </v-list-item>
          </template>
        </template>

        <v-list-item
          disabled
          v-if="filteredNotifiers && !filteredNotifiers.length && searchTerm">
          {{ $t('settings.notifiers.noMatch') }}
        </v-list-item>
      </v-list>
    </v-menu>
  </div>
</template>

<script>
import Focus from './Focus.vue';

export default {
  name: 'NotifierDropdown',
  directives: { Focus },
  emits: ['selected-notifiers-updated'],
  props: {
    id: {
      type: String,
      default: ''
    },
    tooltip: {
      type: String,
      default: ''
    },
    displayText: {
      type: String,
      default: ''
    },
    selectedNotifiers: {
      type: Array,
      default: () => []
    },
    truncate: { type: Number, default: 0 },
    notifiers: { type: Array, required: true },
    disabled: { type: Boolean, default: false }
  },
  data () {
    return {
      focus: false,
      menuOpen: false,
      searchTerm: '',
      filteredNotifiers: this.notifiers,
      localSelectedNotifiers: this.selectedNotifiers || []
    };
  },
  watch: {
    notifiers (newNotifiers) {
      this.filteredNotifiers = newNotifiers;
    },
    selectedNotifiers (newValue) { // localSelectedNotifiers must be changed whenever selectedNotifiers is (this syncs during sorting)
      this.localSelectedNotifiers = newValue || [];
    },
    menuOpen (opened) {
      if (opened) { this.setFocus(); }
    }
  },
  methods: {
    toggleNotifier (notifierId) {
      const set = new Set(this.localSelectedNotifiers);
      if (set.has(notifierId)) {
        set.delete(notifierId);
      } else {
        set.add(notifierId);
      }
      this.updateNotifiers(Array.from(set));
    },
    updateNotifiers (newVal) {
      this.localSelectedNotifiers = newVal || [];
      this.$emit('selected-notifiers-updated', newVal, this.id);
    },
    getNotifiersStr (notifierIds) {
      const notifierNames = notifierIds
        .map(id => {
          const notifier = this.notifiers.find(n => n.id === id);
          return notifier ? `${notifier.name} (${notifier.type})` : id;
        })
        .sort();

      let displayNames = notifierNames;

      // Truncate list so button doesn't get too long if truncate is specified
      if (this.truncate && displayNames.length > this.truncate) {
        displayNames = displayNames.splice(0, this.truncate);
        displayNames.push('...');
      }

      return displayNames.join(', ');
    },
    searchNotifiersLocal (newVal) {
      this.searchTerm = newVal || '';
      if (!this.searchTerm) {
        this.filteredNotifiers = this.notifiers;
      } else {
        const searchLower = this.searchTerm.toLowerCase();
        this.filteredNotifiers = this.notifiers.filter(notifier =>
          notifier.name.toLowerCase().includes(searchLower) ||
          notifier.type.toLowerCase().includes(searchLower) ||
          notifier.id.toLowerCase().includes(searchLower)
        );
      }
    },
    setFocus () {
      this.focus = true;
      setTimeout(() => {
        this.focus = false;
      }, 100);
    }
  }
};
</script>

<style scoped>
.notifier-list {
  min-width: 280px;
  max-width: 360px;
  max-height: 360px;
  overflow-y: auto;
}
.notifier-search-row {
  background-color: rgb(var(--v-theme-background));
  z-index: 2;
}
/* native-checkbox dropdown row indicator. Replaces Bootstrap's
   .form-check-input visuals. Uses --color-* tokens so it themes
   correctly in dark mode. */
.dropdown-check-input {
  appearance: none;
  -webkit-appearance: none;
  width: 14px;
  height: 14px;
  border: 1px solid rgb(var(--v-theme-neutral));
  border-radius: 3px;
  background-color: rgb(var(--v-theme-background)) !important;
  flex-shrink: 0;
}
/* !important required to beat overrides.css's global `input { background:
   rgb(var(--v-theme-input-bg)) !important }` rule -- otherwise the primary fill
   doesn't paint and the white check glyph is invisible on the white bg. */
.dropdown-check-input:checked {
  background-color: rgb(var(--v-theme-primary)) !important;
  border-color: rgb(var(--v-theme-primary)) !important;
  background-image: url("data:image/svg+xml,%3csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 20 20'%3e%3cpath fill='none' stroke='%23fff' stroke-linecap='round' stroke-linejoin='round' stroke-width='3' d='M6 10l3 3 6-6'/%3e%3c/svg%3e") !important;
  background-size: 14px 14px !important;
  background-position: center !important;
  background-repeat: no-repeat !important;
}
</style>
