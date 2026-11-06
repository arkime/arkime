<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-flex flex-column flex-grow-1 overflow-auto pt-3 position-relative d-flex flex-grow h-100">
    <v-overlay
      :model-value="loading"
      class="align-center justify-center blur-overlay"
      contained>
      <div class="d-flex flex-column align-center justify-center">
        <v-progress-circular
          color="info"
          size="64"
          indeterminate />
        <p>{{ $t('cont3xt.stats.loading') }}</p>
      </div>
    </v-overlay>

    <!-- search -->
    <div class="d-flex flex-row align-center">
      <v-text-field
        prepend-inner-icon="mdi-magnify"
        variant="outlined"
        v-debounce="value => search = value"
        class="mx-4 medium-input"
        :placeholder="activeTab === 'itypes' ? $t('cont3xt.stats.searchByItype') : $t('cont3xt.stats.searchByName')"
        clearable />
    </div>
    <!-- /search -->

    <div class="d-flex flex-row align-center ms-4">
      <v-tabs
        content-class="mt-3"
        :model-value="activeTab"
        @update:model-value="setTab">
        <v-tab value="integrations">
          {{ $t('cont3xt.stats.integrations') }}
        </v-tab>
        <v-tab value="itypes">
          {{ $t('cont3xt.stats.itypes') }}
        </v-tab>
      </v-tabs>
      <li
        role="presentation"
        class="nav-item align-self-center startup-time">
        {{ $t('cont3xt.stats.startedAt') }}
        <strong>{{ dateString(data.startTime) }}</strong>
      </li>
    </div>

    <v-data-table
      hover
      must-sort
      hide-default-footer
      :search="search"
      :loading="loading"
      :headers="headers"
      :items="statItems"
      v-model:sort-by="sortBy"
      :no-data-text="noDataText"
      :items-per-page="-1"
      :header-props="{ class: 'text-end' }" />

    <!-- stats error -->
    <v-alert
      v-if="error.length"
      type="warning"
      variant="tonal"
      density="compact"
      closable
      class="mt-2"
      @click:close="error = ''">
      {{ error }}
    </v-alert> <!-- /stats error -->
  </div>
</template>

<script setup>
import Cont3xtService from '@/components/services/Cont3xtService';
import { dateString } from '@/utils/filters.js';
import { commaString, roundCommaString } from '@common/vueFilters.js';
import { ref, computed, onMounted } from 'vue';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();

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
const noDataText = computed(() => {
  const empty = statItems.value == null || statItems.value.length === 0;
  if (activeTab.value === 'itypes') {
    return empty ? t('cont3xt.stats.noItypes') : t('cont3xt.stats.noItypesMatch', { search: search.value });
  }
  return empty ? t('cont3xt.stats.noIntegrations') : t('cont3xt.stats.noIntegrationsMatch', { search: search.value });
});

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
const headers = computed(() => [{
  title: t('cont3xt.stats.name'),
  key: 'name',
  sortable: true
}, {
  title: t('cont3xt.stats.cacheLookup'),
  key: 'cacheLookup',
  value: format('cacheLookup', commaString),
  sortable: true,
  filterable: false,
  align: 'end'
}, {
  title: t('cont3xt.stats.cacheFound'),
  key: 'cacheFound',
  value: format('cacheFound', commaString),
  sortable: true,
  tdClass: 'text-end',
  thClass: 'text-end',
  filterable: false,
  align: 'end'
}, {
  title: t('cont3xt.stats.cacheGood'),
  key: 'cacheGood',
  value: format('cacheGood', commaString),
  sortable: true,
  tdClass: 'text-end',
  thClass: 'text-end',
  filterable: false,
  align: 'end'
}, {
  title: t('cont3xt.stats.cacheRecentAvgMS'),
  key: 'cacheRecentAvgMS',
  value: format('cacheRecentAvgMS', commaStringRound),
  sortable: true,
  tdClass: 'text-end',
  thClass: 'text-end',
  filterable: false,
  align: 'end'
}, {
  title: t('cont3xt.stats.directLookup'),
  key: 'directLookup',
  value: format('directLookup', commaString),
  sortable: true,
  tdClass: 'text-end',
  thClass: 'text-end',
  filterable: false,
  align: 'end'
}, {
  title: t('cont3xt.stats.directFound'),
  key: 'directFound',
  value: format('directFound', commaString),
  sortable: true,
  tdClass: 'text-end',
  thClass: 'text-end',
  filterable: false,
  align: 'end'
}, {
  title: t('cont3xt.stats.directGood'),
  key: 'directGood',
  value: format('directGood', commaString),
  sortable: true,
  tdClass: 'text-end',
  thClass: 'text-end',
  filterable: false,
  align: 'end'
}, {
  title: t('cont3xt.stats.directError'),
  key: 'directError',
  value: format('directError', commaString),
  sortable: true,
  tdClass: 'text-end',
  thClass: 'text-end',
  filterable: false,
  align: 'end'
}, {
  title: t('cont3xt.stats.directRecentAvgMS'),
  key: 'directRecentAvgMS',
  value: format('directRecentAvgMS', commaStringRound),
  sortable: true,
  tdClass: 'text-end',
  thClass: 'text-end',
  filterable: false,
  align: 'end'
}, {
  title: t('cont3xt.stats.total'),
  key: 'total',
  value: format('total', commaString),
  sortable: true,
  tdClass: 'text-end',
  thClass: 'text-end',
  filterable: false,
  align: 'end'
}]);
</script>

<style scoped>
.startup-time {
  right: 15px;
  position: absolute;
  list-style: none;
}
</style>
