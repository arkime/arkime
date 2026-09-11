<!--
Inner body of FieldSelectDropdown — the search input + clear button +
grouped field list. Extracted so FieldSelectDropdown can render itself
as either a standalone v-menu (default) or just the list body (when
hosted inside another menu as a submenu). All state lives on the parent
FieldSelectDropdown; this component is presentational.

Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <div class="px-2 py-1">
      <div class="arkime-input-group arkime-input-group--fluid">
        <input
          autofocus
          type="text"
          class="arkime-input-control"
          :value="query"
          :placeholder="searchPlaceholder"
          @input="$emit('update:query', $event.target.value)"
          @click.stop>
      </div>
    </div>
    <v-list-item
      v-if="selectedFields.length > 0"
      class="text-danger"
      @click="$emit('clear')">
      <v-icon
        icon="mdi-close"
        class="me-1" />
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
          class="field-item"
          @click="$emit('toggle', getFieldId(field))">
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
        @click.stop="$emit('show-all')">
        <strong>Show {{ filteredFieldsCount - visibleCount }} more fields</strong>
      </v-list-item>
    </template>
  </div>
</template>

<script>
export default {
  name: 'FieldSelectDropdownBody',
  props: {
    menuOpen: { type: Boolean, required: true },
    selectedFields: { type: Array, required: true },
    searchPlaceholder: { type: String, default: '' },
    query: { type: String, default: '' },
    showAllFields: { type: Boolean, default: false },
    visibleFilteredFields: { type: Object, required: true },
    filteredFieldsCount: { type: Number, required: true },
    hasMoreFields: { type: Boolean, default: false },
    getFieldId: { type: Function, required: true },
    isSelected: { type: Function, required: true }
  },
  emits: ['update:query', 'toggle', 'clear', 'show-all'],
  computed: {
    visibleCount () {
      let n = 0;
      for (const g of Object.values(this.visibleFilteredFields || {})) { n += g.length; }
      return n;
    }
  }
};
</script>

<style scoped>
/* Group headers sit flush at the menu's natural left edge; field rows
   underneath get a small left indent so the hierarchy reads at a glance. */
.group-header {
  text-transform: uppercase;
  font-weight: bold;
  font-size: 0.75rem;
  padding: 0.4rem 0.5rem 0.2rem;
  color: rgb(var(--v-theme-foreground-accent));
}
.field-item {
  padding-inline-start: 1.25rem !important;
}
</style>
