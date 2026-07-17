<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <input
    type="text"
    ref="typeahead"
    v-model="value"
    :class="{'dropup':dropup}"
    @click="openTypeaheadResults"
    @blur="closeTypeaheadResults"
    @input="filterFields($event.target.value)"
    @keyup.stop="keyup($event)"
    @keydown.down.stop="down"
    @keydown.up.stop="up"
    @keyup.enter.stop="enterClick"
    @keyup.esc.stop="closeTypeaheadResults"
    class="arkime-input-control"
    :placeholder="$t('utils.beginTypingPlaceholder')">

  <!-- dropdown is teleported to body so it escapes navbar/toolbar
       overflow:hidden clipping. Positioned via fixed top/left from the
       input's bounding rect. -->
  <Teleport to="body">
    <div
      class="arkime-dropdown-menu field-typeahead-dropdown"
      v-show="showDropdown"
      :style="dropdownStyle"
      role="dropdown">
      <a
        v-for="(field, index) in filteredFieldHistory"
        :key="field.exp + '-history'"
        :class="{'active': index === current,'last-history-item':index === filteredFieldHistory.length - 1}"
        class="arkime-dropdown-item cursor-pointer"
        @mousedown.prevent
        @click.stop="changeField(field)">
        <v-icon icon="mdi-history" />&nbsp;
        {{ field.friendlyName }}
        <small>({{ field.exp }})</small>
        <v-icon
          icon="mdi-close"
          class="float-right mt-1"
          :title="$t('utils.removeFromHistory')"
          @click.stop.prevent="removeFromFieldHistory(field)" />
      </a>
      <div
        v-if="filteredFieldHistory.length"
        class="arkime-dropdown-divider" />
      <a
        v-for="(field, index) in filteredFields"
        :key="field.exp"
        :class="{'active':index + filteredFieldHistory.length === current}"
        class="arkime-dropdown-item cursor-pointer"
        @mousedown.prevent
        @click.stop="changeField(field)">
        {{ field.friendlyName }}
        <small>({{ field.exp }})</small>
      </a>
      <a
        v-if="(!filteredFields || !filteredFields.length)"
        class="arkime-dropdown-item">
        {{ $t('utils.noFieldsMatch') }}
      </a>
    </div>
  </Teleport>
</template>

<script>
import UserService from '../users/UserService';
import { searchFields } from '@common/vueFilters.js';

let inputTimeout;

export default {
  name: 'ArkimeFieldTypeahead',
  emits: ['fieldSelected'],
  props: {
    fields: {
      type: Array,
      required: true
    },
    initialValue: {
      type: String,
      default: ''
    },
    queryParam: {
      type: String,
      default: ''
    },
    page: {
      type: String,
      default: ''
    },
    dropup: Boolean,
    history: {
      type: Array,
      default: () => []
    }
  },
  data: function () {
    return {
      showDropdown: false,
      filteredFields: this.fields,
      value: this.initialValue,
      current: 0, // select first field
      fieldHistory: this.history || [],
      filteredFieldHistory: this.history || [],
      dropdownPos: { top: 0, left: 0, width: 0 }
    };
  },
  computed: {
    routeParams: function () {
      return this.$route.query[this.queryParam || 'exp'];
    },
    dropdownStyle: function () {
      return {
        position: 'fixed',
        top: this.dropup
          ? 'auto'
          : `${this.dropdownPos.top}px`,
        bottom: this.dropup
          ? `${window.innerHeight - this.dropdownPos.top + (this.$refs.typeahead?.offsetHeight || 0)}px`
          : 'auto',
        left: `${this.dropdownPos.left}px`,
        minWidth: `${this.dropdownPos.width}px`,
        width: '360px',
        maxWidth: 'calc(100vw - 20px)',
        // Above Vuetify's v-dialog content (~2400) so the dropdown
        // surfaces when the input lives inside a modal.
        zIndex: 2500,
        maxHeight: '300px',
        overflowY: 'auto',
        overflowX: 'hidden'
      };
    }
  },
  watch: {
    // update the value when the url query param
    routeParams: function (newVal, oldVal) {
      if (!newVal) { this.value = this.constantInitialVal; }
      for (const field of this.fields) {
        if (field.dbField === newVal) {
          this.value = field.friendlyName;
          continue;
        }
      }
    },
    initialValue: function () {
      this.value = this.initialValue;
    }
  },
  created: function () {
    // save initial value for later use (specifically if the route param
    // is undefined and we need to fill the input with the initial value)
    this.constantInitialVal = this.initialValue;

    if (this.page && !this.history) {
      // get the field history for this page
      UserService.getState(`fieldHistory${this.page}`).then((response) => {
        this.fieldHistory = response.fields || [];
        this.filteredFieldHistory = response.fields || [];
      });
    }
  },
  methods: {
    /* wait for changeField click event before closing results */
    closeTypeaheadResults: function () {
      setTimeout(() => {
        this.current = 0; // reset
        this.showDropdown = false;
      }, 250);
    },
    openTypeaheadResults: function () {
      this.updateDropdownPos();
      this.showDropdown = true;
    },
    /* recompute the teleported dropdown's fixed top/left/width from the
       input's current bounding rect (called on open + on each keyup). */
    updateDropdownPos: function () {
      const input = this.$refs.typeahead;
      if (!input) { return; }
      const rect = input.getBoundingClientRect();
      this.dropdownPos = {
        top: rect.bottom,
        left: rect.left,
        width: rect.width
      };
    },
    filterFields: function (searchFilter) {
      if (inputTimeout) { clearTimeout(inputTimeout); }

      inputTimeout = setTimeout(() => {
        this.filteredFields = searchFields(searchFilter, this.fields, true);
        this.filteredFieldHistory = searchFields(searchFilter, this.fieldHistory, true);
      }, 250);
    },
    changeField: function (field) {
      this.value = field.friendlyName;
      this.addFieldToHistory(field);
      this.$emit('fieldSelected', field);
    },
    /**
     * selects the active field in the dropdown if it exists
     * then closes the dropdown
     */
    enterClick: function () {
      if (this.showDropdown && this.filteredFields.length) {
        let selectedField;
        if (this.current < this.filteredFieldHistory.length) {
          selectedField = this.filteredFieldHistory[this.current];
        } else {
          selectedField = this.filteredFields[this.current - this.filteredFieldHistory.length];
        }
        this.changeField(selectedField);
      }
      this.closeTypeaheadResults();
    },
    /* shows the dropdown results if the user is typing (except enter/esc) */
    keyup: function (e) {
      if (e.key === 'Enter' || e.key === 'Escape') {
        return;
      }
      this.updateDropdownPos();
      this.showDropdown = true;
    },
    /* navigates up through field and field history results */
    up: function () {
      this.current = (this.current > 0
        ? this.current
        : (this.filteredFieldHistory.length + this.filteredFields.length)) - 1;
    },
    /* navigates down through field and field history results */
    down: function () {
      this.current = (this.current + 1) %
        (this.filteredFieldHistory.length + this.filteredFields.length);
    },
    /**
     * Adds an item to the beginning of the field history
     * @param {object} field The field to add to the history
     */
    addFieldToHistory: function (field) {
      let found = false;

      if (!field) { return found; }

      let index = 0;
      for (const historyField of this.fieldHistory) {
        if (historyField.exp === field.exp) {
          found = true;
          break;
        }
        index++;
      }

      if (found) { // if the field was found, remove it
        this.fieldHistory.splice(index, 1);
      }

      // add the field to the beginning of the list
      this.fieldHistory.unshift(field);

      // if the list is larger than 10 items
      if (this.fieldHistory.length > 10) {
        // remove the last item in the history
        this.fieldHistory.splice(this.fieldHistory.length - 1, 1);
      }

      // save the field history for the user
      UserService.saveState({ fields: this.fieldHistory }, `fieldHistory${this.page}`);

      return found;
    },
    /**
     * Removes an item from the field history (and results)
     * @param {object} field The field to remove from the history
     */
    removeFromFieldHistory: function (field) {
      let index = 0;
      for (const historyField of this.fieldHistory) {
        if (historyField.exp === field.exp) {
          break;
        }
        index++;
      }

      // remove the item from the history
      this.fieldHistory.splice(index, 1);

      // find it in the field history results (displayed in the typeahead)
      index = 0;
      for (const historyField of this.filteredFieldHistory) {
        if (historyField.exp === field.exp) {
          break;
        }
        index++;
      }

      // remove the item from the field history results
      this.filteredFieldHistory.splice(index, 1);

      // save the field history for the user
      UserService.saveState({ fields: this.fieldHistory }, `fieldHistory${this.page}`);
    }
  }
};
</script>

<style>
/* The dropdown is teleported to <body> so it lives outside the
   component's scoped-CSS scope -- styles here must be unscoped to
   reach it. Positioning (top/left/width/z-index/max-height) is
   handled inline via :style="dropdownStyle". */
.field-typeahead-dropdown div.arkime-dropdown-divider {
  margin: .15rem 0;
  border-color: rgb(var(--v-theme-neutral));
}
</style>
