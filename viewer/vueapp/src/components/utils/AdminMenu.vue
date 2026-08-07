<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-menu location="bottom end">
    <template #activator="{ props: activatorProps }">
      <v-btn
        v-bind="activatorProps"
        size="small"
        :active="isActive"
        :variant="isActive ? 'flat' : 'text'"
        :style="isActive ? activePillStyle : null"
        :class="['btn-admin', additionalClasses]">
        <v-icon
          start
          icon="mdi-shield-crown-outline" />
        {{ $t('navigation.admin') }}
      </v-btn>
    </template>
    <v-list density="compact">
      <v-list-item
        v-for="item in items"
        :key="item.name"
        :active="item.isActive"
        :to="{ path: item.link, query: item.query, name: item.name }">
        {{ item.title }}
      </v-list-item>
    </v-list>
  </v-menu>
</template>

<script>
// Items come from Navbar's menu map already gated, query-preserved and
// active-flagged, so this stays presentational.
export default {
  name: 'AdminMenu',
  props: {
    items: {
      type: Array,
      default: () => []
    },
    additionalClasses: {
      type: String,
      default: ''
    },
    // same pill treatment the top-level nav buttons use when active
    activePillStyle: {
      type: Object,
      default: () => ({})
    }
  },
  computed: {
    isActive: function () {
      return this.items.some(item => item.isActive);
    }
  }
};
</script>
