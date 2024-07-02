<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="container-fluid overflow-auto pt-3 position-relative flex flex-grow h-100">
    <v-overlay
      :model-value="loading"
      class="align-center justify-center blur-overlay"
      contained
    >
      <div class="flex flex-col align-items-center justify-content-center" style="display:flex;align-items:center;flex-direction: column;">
        <v-progress-circular
          color="info"
          size="64"
          indeterminate
        />
        <p>Loading stats...</p>

      </div>
    </v-overlay>

    <!-- search -->
    <!-- TODO: toby, this had debounce (400ms), no more ... address? (probably not important for this table [few entries]) -->
    <v-text-field
      prepend-inner-icon="mdi-magnify"
      variant="outlined"
      v-model="search"
      class="mx-4"
      :placeholder="activeTab === 'itypes' ? 'Search by itype' : 'Search by name'"
      clearable
    />
    <!-- /search -->

    <div class="d-flex flex-row align-items-center">

      <v-tabs content-class="mt-3" :model-value="activeTab" @update:modelValue="setTab">
        <v-tab value="integrations">Integrations</v-tab>
        <v-tab value="itypes">ITypes</v-tab>
      </v-tabs>
      <li role="presentation"
        class="nav-item align-self-center startup-time">
        Started at
        <strong>{{ dateString(data.startTime) }}</strong>
      </li>
    </div>

    <v-data-table
      hover
      class="table-striped"
      hide-default-footer
      :search="search"
      :loading="loading"
      :headers="headers"
      :items="statItems"
      v-model:sort-by="sortBy"
      :no-data-text="(statItems == null || statItems.length === 0) ? `There are no ${tableSubjects} to show stats for` : `There are no ${tableSubjects} that match the name: ${search}`"
      :items-per-page="-1"
      :header-props="{ class: 'text-right' }">
    </v-data-table>

    <!-- stats error -->
    <div
      v-if="error.length"
      class="mt-2 alert alert-warning">
      <span class="fa fa-exclamation-triangle" />&nbsp;
      {{ error }}
      <button
        type="button"
        @click="error = ''"
        class="close cursor-pointer">
        <span>&times;</span>
      </button>
    </div> <!-- /stats error -->
  </div>
</template>

<script setup>
import Cont3xtService from '@/components/services/Cont3xtService';
import { dateString } from '@/utils/filters.js';
import { commaString, roundCommaString } from '@common/vueFilters.js';
import { ref, computed, onMounted } from 'vue';

const data = ref({});
const error = ref('');
const loading = ref(true);
const sortBy = ref([{ key: 'name', order: 'asc' }]);
const search = ref('');
const activeTab = ref('integrations');

const statItems = computed(() => {
  if (activeTab.value === 'itypes') { return data.value.itypeStats; }
  if (activeTab.value === 'integrations') { return data.value.stats; }
  return data.value.stats; // integration stats in case of invalid type
});
const tableSubjects = computed(() => activeTab.value);

onMounted(() => {
  // set active tab
  const hash = location.hash.substring(1, location.hash.length);
  setTab((hash === 'itypes') ? 'itypes' : 'integrations');

  Cont3xtService.getStats().then((response) => {
    loading.value = false;
    data.value = response;
  }).catch((err) => {
    error.value = err;
    loading.value = false;
  });
});

function setTab (tab) {
  activeTab.value = tab;
  location.hash = tab;
}

function commaStringRound (val) {
  return roundCommaString(val, 2);
}

function format (key, formatterFn) {
  return (item) => formatterFn(item[key]);
}
const headers = [{
  title: 'Name',
  key: 'name',
  sortable: true
}, {
  title: 'Cache Lookup',
  key: 'cacheLookup',
  value: format('cacheLookup', commaString),
  sortable: true,
  filterable: false,
  align: 'end'
}, {
  title: 'Cache Found',
  key: 'cacheFound',
  value: format('cacheFound', commaString),
  sortable: true,
  tdClass: 'text-right',
  thClass: 'text-right',
  filterable: false,
  align: 'end'
}, {
  title: 'Cache Good',
  key: 'cacheGood',
  value: format('cacheGood', commaString),
  sortable: true,
  tdClass: 'text-right',
  thClass: 'text-right',
  filterable: false,
  align: 'end'
}, {
  title: 'Cache Recent Avg MS',
  key: 'cacheRecentAvgMS',
  value: format('cacheRecentAvgMS', commaStringRound),
  sortable: true,
  tdClass: 'text-right',
  thClass: 'text-right',
  filterable: false,
  align: 'end'
}, {
  title: 'Direct Lookup',
  key: 'directLookup',
  value: format('directLookup', commaString),
  sortable: true,
  tdClass: 'text-right',
  thClass: 'text-right',
  filterable: false,
  align: 'end'
}, {
  title: 'Direct Found',
  key: 'directFound',
  value: format('directFound', commaString),
  sortable: true,
  tdClass: 'text-right',
  thClass: 'text-right',
  filterable: false,
  align: 'end'
}, {
  title: 'Direct Good',
  key: 'directGood',
  value: format('directGood', commaString),
  sortable: true,
  tdClass: 'text-right',
  thClass: 'text-right',
  filterable: false,
  align: 'end'
}, {
  title: 'Direct Error',
  key: 'directError',
  value: format('directError', commaString),
  sortable: true,
  tdClass: 'text-right',
  thClass: 'text-right',
  filterable: false,
  align: 'end'
}, {
  title: 'Direct Recent Avg MS',
  key: 'directRecentAvgMS',
  value: format('directRecentAvgMS', commaStringRound),
  sortable: true,
  tdClass: 'text-right',
  thClass: 'text-right',
  filterable: false,
  align: 'end'
}, {
  title: 'Total',
  key: 'total',
  value: format('total', commaString),
  sortable: true,
  tdClass: 'text-right',
  thClass: 'text-right',
  filterable: false,
  align: 'end'
}];
</script>

<style scoped>
.startup-time {
  right: 15px;
  position: absolute;
  list-style: none;
}
</style>
