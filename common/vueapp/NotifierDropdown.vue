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
        <button
          v-bind="activatorProps"
          type="button"
          :disabled="disabled"
          class="btn btn-sm btn-secondary notifier-trigger no-wrap">
          {{ displayText || getNotifiersStr(localSelectedNotifiers) }}
          <v-icon end>
            fa-caret-down
          </v-icon>
          <v-tooltip
            v-if="tooltip"
            activator="parent"
            location="top">
            {{ tooltip }}
          </v-tooltip>
        </button>
      </template>
      <v-list
        density="compact"
        class="notifier-list">
        <!-- search -->
        <div class="px-2 pt-2 pb-1 sticky-top notifier-search-row">
          <div class="input-group input-group-sm">
            <input
              v-focus="focus"
              type="text"
              class="form-control"
              :value="searchTerm"
              @input="searchNotifiersLocal($event.target.value)"
              :placeholder="$t('settings.notifiers.searchTermPlaceholder')">
            <button
              type="button"
              class="btn btn-outline-secondary"
              :disabled="!searchTerm"
              @click="clearSearchTerm">
              <span class="fa fa-close" />
            </button>
          </div>
        </div>
        <v-divider />

        <template v-if="filteredNotifiers && filteredNotifiers.length">
          <v-list-item
            v-for="notifier in filteredNotifiers"
            :key="notifier.id"
            @click.stop="toggleNotifier(notifier.id)">
            <span class="d-flex align-items-center">
              <input
                type="checkbox"
                class="form-check-input me-2 cursor-pointer"
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
              <span class="d-flex align-items-center">
                <input
                  type="checkbox"
                  class="form-check-input me-2 cursor-pointer"
                  :checked="true"
                  @click.stop="toggleNotifier(notifierId)">
                {{ notifierId }}
                <span
                  class="fa fa-times-circle cursor-help ms-2"
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
    clearSearchTerm () {
      this.searchTerm = '';
      this.searchNotifiersLocal();
      this.setFocus();
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
  background-color: var(--color-background);
  z-index: 2;
}
</style>
