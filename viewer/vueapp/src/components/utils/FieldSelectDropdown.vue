<template>
  <!-- bodyOnly: render just the v-list so a parent v-menu can host us
       as a submenu. Otherwise: the standalone v-menu + v-btn + v-list
       form used everywhere a FieldSelectDropdown stands on its own. -->
  <v-menu
    v-if="!bodyOnly"
    v-model="menuOpen"
    :close-on-content-click="false"
    location="bottom end">
    <template #activator="{ props: activatorProps }">
      <v-btn v-bind="activatorProps">
        <v-icon icon="mdi-menu" />
        <v-tooltip
          activator="parent"
          location="right">
          {{ tooltipText }}
        </v-tooltip>
      </v-btn>
    </template>
    <v-list
      density="compact"
      class="field-dropdown-menu">
      <FieldSelectDropdownBody
        :menu-open="menuOpen"
        :selected-fields="selectedFields"
        :search-placeholder="searchPlaceholder"
        :query="query"
        :show-all-fields="showAllFields"
        :visible-filtered-fields="visibleFilteredFields"
        :filtered-fields-count="filteredFieldsCount"
        :has-more-fields="hasMoreFields"
        :get-field-id="getFieldId"
        :is-selected="isSelected"
        @update:query="onQueryInput"
        @toggle="toggle"
        @clear="$emit('clear')"
        @show-all="showAllFields = true" />
    </v-list>
  </v-menu>
  <v-list
    v-else
    density="compact"
    class="field-dropdown-menu">
    <FieldSelectDropdownBody
      :menu-open="true"
      :selected-fields="selectedFields"
      :search-placeholder="searchPlaceholder"
      :query="query"
      :show-all-fields="showAllFields"
      :visible-filtered-fields="visibleFilteredFields"
      :filtered-fields-count="filteredFieldsCount"
      :has-more-fields="hasMoreFields"
      :get-field-id="getFieldId"
      :is-selected="isSelected"
      @update:query="onQueryInput"
      @toggle="toggle"
      @clear="$emit('clear')"
      @show-all="showAllFields = true" />
  </v-list>
</template>

<script>
import { searchFields } from '@common/vueFilters.js';
import FieldService from '../search/FieldService';
import FieldSelectDropdownBody from './FieldSelectDropdownBody.vue';

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
    },
    // When true, render only the v-list body (no v-menu wrapper, no
    // v-btn activator). Used when this component is nested inside a
    // parent v-menu acting as a submenu.
    bodyOnly: {
      type: Boolean,
      default: false
    }
  },
  emits: ['toggle', 'clear'],
  components: { FieldSelectDropdownBody },
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
    onQueryInput (value) {
      this.query = value;
      this.debounceQuery();
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
/* .group-header + .field-item styling lives in FieldSelectDropdownBody.vue
   since that's where those elements are now rendered (and scoped styles
   here would no longer reach them across the component boundary). */
</style>
