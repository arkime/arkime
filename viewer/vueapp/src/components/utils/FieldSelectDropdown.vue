<template>
  <v-menu
    v-model="menuOpen"
    :close-on-content-click="false"
    location="bottom end">
    <template #activator="{ props: activatorProps }">
      <button
        v-bind="activatorProps"
        type="button"
        class="btn btn-sm field-dropdown-trigger d-inline-block"
        :class="`btn-${buttonVariant}`">
        <span class="fa fa-bars" />
        <v-tooltip
          activator="parent"
          location="right">
          {{ tooltipText }}
        </v-tooltip>
      </button>
    </template>
    <v-list
      density="compact"
      class="field-dropdown-menu">
      <div class="px-2 py-1">
        <input
          autofocus
          type="text"
          class="form-control form-control-sm"
          v-model="query"
          @input="debounceQuery"
          @click.stop
          :placeholder="searchPlaceholder">
      </div>
      <v-list-item
        v-if="selectedFields.length > 0"
        class="text-danger"
        @click="$emit('clear')">
        <span class="fa fa-times me-1" />
        Clear all
      </v-list-item>
      <v-divider />
      <template v-if="menuOpen">
        <v-list-item v-if="!filteredFieldsCount">
          No fields match
        </v-list-item>
        <template
          v-for="(groupFields, group) in visibleFilteredFields"
          :key="group">
          <div
            v-if="groupFields.length"
            class="group-header">
            {{ group }}
          </div>
          <v-list-item
            v-for="(field, idx) in groupFields"
            :id="group + idx + 'item'"
            :key="group + idx + 'item'"
            :active="isSelected(getFieldId(field))"
            @click="toggle(getFieldId(field))">
            {{ field.friendlyName }}
            <small>({{ field.exp }})</small>
            <v-tooltip
              v-if="field.help"
              activator="parent">
              {{ field.help }}
            </v-tooltip>
          </v-list-item>
        </template>
        <v-list-item
          v-if="hasMoreFields"
          class="text-center cursor-pointer"
          @click.stop="showAllFields = true">
          <strong>Show {{ filteredFieldsCount - maxVisibleFields }} more fields</strong>
        </v-list-item>
      </template>
    </v-list>
  </v-menu>
</template>

<script>
import { searchFields } from '@common/vueFilters.js';
import FieldService from '../search/FieldService';

let filterTimeout;

export default {
  name: 'FieldSelectDropdown',
  props: {
    selectedFields: {
      type: Array,
      required: true
    },
    tooltipText: {
      type: String,
      default: 'Toggle fields'
    },
    searchPlaceholder: {
      type: String,
      default: 'Search for fields...'
    },
    buttonVariant: {
      type: String,
      default: 'theme-primary'
    },
    // Which field property to use as the ID (dbField for columns, exp for summary)
    fieldIdKey: {
      type: String,
      default: 'dbField'
    },
    excludeTokens: {
      type: Boolean,
      default: false
    },
    excludeFilename: {
      type: Boolean,
      default: true
    },
    excludeInfo: {
      type: Boolean,
      default: false
    },
    maxVisibleFields: {
      type: Number,
      default: 200
    },
    // Include special summary fields (All IP Fields, Dst IP:Dst Port)
    includeSummaryFields: {
      type: Boolean,
      default: false
    }
  },
  emits: ['toggle', 'clear'],
  data () {
    return {
      menuOpen: false,
      showAllFields: false,
      query: '',
      groupedFields: {},
      filteredFields: {},
      filteredFieldsCount: 0
    };
  },
  computed: {
    fields () {
      const baseFields = this.$store.state.fieldsArr;
      if (this.includeSummaryFields) {
        return FieldService.addSummarySpecialFields(baseFields);
      }
      return baseFields;
    },
    visibleFilteredFields () {
      if (!this.filteredFields || this.showAllFields) {
        return this.filteredFields;
      }

      const limited = {};
      let totalCount = 0;

      for (const [groupName, fields] of Object.entries(this.filteredFields)) {
        if (totalCount >= this.maxVisibleFields) break;
        limited[groupName] = fields.slice(0, Math.max(0, this.maxVisibleFields - totalCount));
        totalCount += limited[groupName].length;
      }

      return limited;
    },
    hasMoreFields () {
      return !this.showAllFields && this.filteredFieldsCount > this.maxVisibleFields;
    }
  },
  watch: {
    fields: {
      immediate: true,
      handler () {
        this.buildGroupedFields();
        this.filterFields();
      }
    },
    menuOpen (opened) {
      // Reset showAllFields when menu closes (matches BVN @hide handler).
      if (!opened) { this.showAllFields = false; }
    }
  },
  methods: {
    buildGroupedFields () {
      const existingFieldsLookup = {};
      this.groupedFields = {};

      for (const field of this.fields) {
        if (!existingFieldsLookup[field.exp]) {
          existingFieldsLookup[field.exp] = field;
          if (!this.groupedFields[field.group]) {
            this.groupedFields[field.group] = [];
          }
          this.groupedFields[field.group].push(field);
        }
      }
    },
    filterFields () {
      const filteredGroupedFields = {};
      let count = 0;

      for (const group in this.groupedFields) {
        const filtered = searchFields(
          this.query,
          this.groupedFields[group],
          this.excludeTokens,
          this.excludeFilename,
          this.excludeInfo
        );
        count += filtered.length;
        filteredGroupedFields[group] = filtered;
      }

      this.filteredFields = filteredGroupedFields;
      this.filteredFieldsCount = count;
    },
    debounceQuery () {
      if (filterTimeout) clearTimeout(filterTimeout);
      filterTimeout = setTimeout(() => {
        this.showAllFields = false;
        this.filterFields();
      }, 400);
    },
    getFieldId (field) {
      return field[this.fieldIdKey] || field.exp || field.dbField;
    },
    isSelected (fieldId) {
      return this.selectedFields.includes(fieldId);
    },
    toggle (fieldId) {
      this.$emit('toggle', fieldId);
    }
  }
};
</script>

<style scoped>
.field-dropdown-menu {
  max-height: 400px;
  overflow-y: auto;
  width: 320px;
}

.group-header {
  text-transform: uppercase;
  font-weight: bold;
  font-size: 0.75rem;
  padding: 0.4rem 0.5rem 0.2rem;
  color: var(--color-foreground-accent);
}
</style>
