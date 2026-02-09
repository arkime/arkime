<template>
  <b-dropdown
    lazy
    no-flip
    no-caret
    auto-close="outside"
    size="sm"
    menu-class="field-dropdown-menu"
    class="field-dropdown d-inline-block"
    :variant="buttonVariant"
    @show="menuOpen = true"
    @hide="menuOpen = false; showAllFields = false">
    <template #button-content>
      <span
        class="fa fa-bars"
        :id="dropdownId">
        <BTooltip
          :target="dropdownId"
          noninteractive
          placement="right">{{ tooltipText }}</BTooltip>
      </span>
    </template>
    <b-dropdown-header header-class="p-1">
      <b-input
        size="sm"
        autofocus
        type="text"
        v-model="query"
        @input="debounceQuery"
        @click.stop
        :placeholder="searchPlaceholder" />
    </b-dropdown-header>
    <li v-if="selectedFields.length > 0">
      <button
        type="button"
        class="dropdown-item text-danger py-1"
        @click="$emit('clear')">
        <span class="fa fa-times me-1" />
        Clear all
      </button>
    </li>
    <b-dropdown-divider />
    <template v-if="menuOpen">
      <b-dropdown-item v-if="!filteredFieldsCount">
        No fields match
      </b-dropdown-item>
      <template
        v-for="(groupFields, group) in visibleFilteredFields"
        :key="group">
        <b-dropdown-header
          v-if="groupFields.length"
          class="group-header"
          header-class="p-1 text-uppercase">
          {{ group }}
        </b-dropdown-header>
        <li
          v-for="(field, idx) in groupFields"
          :key="group + idx + 'item'">
          <button
            type="button"
            :id="group + idx + 'item'"
            class="dropdown-item"
            :class="{ active: isSelected(getFieldId(field)) }"
            @click="toggle(getFieldId(field))">
            {{ field.friendlyName }}
            <small>({{ field.exp }})</small>
            <BTooltip
              v-if="field.help"
              :target="group + idx + 'item'">
              {{ field.help }}
            </BTooltip>
          </button>
        </li>
      </template>
      <button
        v-if="hasMoreFields"
        type="button"
        @click.stop="showAllFields = true"
        class="dropdown-item text-center cursor-pointer">
        <strong>Show {{ filteredFieldsCount - maxVisibleFields }} more fields</strong>
      </button>
    </template>
  </b-dropdown>
</template>

<script>
import { searchFields } from '@common/vueFilters.js';
import Utils from './utils';
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
      filteredFieldsCount: 0,
      dropdownId: `field-dropdown-${Utils.createRandomString()}`
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
}

.group-header {
  font-weight: bold;
  color: var(--color-foreground-accent);
}
</style>
