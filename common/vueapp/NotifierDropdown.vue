<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div
    ref="notifierDropdown"
    class="d-inline-block">
    <BTooltip
      v-if="tooltip"
      :target="$refs.notifierDropdown"
      placement="top">
      {{ tooltip }}
    </BTooltip>
    <b-dropdown
      size="sm"
      auto-close="outside"
      @shown="setFocus"
      :disabled="disabled"
      class="notifiers-dropdown no-wrap"
      :text="displayText || getNotifiersStr(localSelectedNotifiers)">
      <!-- notifiers search -->
      <b-dropdown-header class="w-100 sticky-top">
        <b-input-group size="sm">
          <b-form-input
            v-focus="focus"
            :model-value="searchTerm"
            @update:model-value="searchNotifiersLocal"
            :placeholder="$t('settings.notifiers.searchTermPlaceholder')" />
          <template #append>
            <b-button
              :disabled="!searchTerm"
              @click="clearSearchTerm"
              variant="outline-secondary">
              <span class="fa fa-close" />
            </b-button>
          </template>
        </b-input-group>
        <b-dropdown-divider />
      </b-dropdown-header> <!-- /notifiers search -->
      <b-dropdown-form v-if="filteredNotifiers && filteredNotifiers.length">
        <!-- notifier checkboxes -->
        <b-form-checkbox-group
          stacked
          :model-value="localSelectedNotifiers"
          @update:model-value="updateNotifiers">
          <b-form-checkbox
            :key="notifier.id"
            :value="notifier.id"
            v-for="notifier in filteredNotifiers">
            {{ notifier.name }} ({{ notifier.type }})
          </b-form-checkbox>
          <template v-for="notifierId in localSelectedNotifiers">
            <!-- previously deleted notifiers -->
            <b-form-checkbox
              :key="notifierId"
              :value="notifierId"
              v-if="!notifiers.find(n => n.id === notifierId)">
              {{ notifierId }}
              <span
                class="fa fa-times-circle cursor-help ms-2"
                :title="$t('settings.notifiers.missingNotifier')" />
            </b-form-checkbox>
          </template>
        </b-form-checkbox-group> <!-- /notifier checkboxes -->
      </b-dropdown-form>
      <b-dropdown-item
        disabled
        v-if="filteredNotifiers && !filteredNotifiers.length && searchTerm">
        {{ $t('settings.notifiers.noMatch') }}
      </b-dropdown-item>
    </b-dropdown>
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
    }
  },
  methods: {
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
      this.searchTerm = newVal;
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

<style>
.notifiers-dropdown > ul.dropdown-menu li > form > div {
  color: var(--color-foreground, black) !important;
}
/* hides elements scrolling behind sticky search bar */
.notifiers-dropdown .sticky-top {
  top: -8px;
}
.notifiers-dropdown .dropdown-header {
  padding: 0rem 0.5rem;
  background-color: var(--color-background);
}
.notifiers-dropdown .dropdown-header > li {
  padding-top: 10px;
  background-color: var(--color-background);
}
.notifiers-dropdown .dropdown-divider {
  margin-top: 0px;
}

.notifiers-dropdown .dropdown-item,
.notifiers-dropdown .custom-control {
  padding-left: 0.5rem;
}
</style>
