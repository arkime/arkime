<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div id="history-page" class="d-flex flex-column flex-grow-1 overflow-auto pt-3 position-relative d-flex flex-grow h-100">
    <v-overlay
      :model-value="loading"
      class="align-center justify-center blur-overlay"
      contained
    >
      <div class="d-flex flex-column align-center justify-center">
        <v-progress-circular
          color="info"
          size="64"
          indeterminate
        />
        <p>Loading history...</p>

      </div>
    </v-overlay>
    <div class="d-flex flex-row align-center mb-2 mx-4">
      <div class="flex-grow-1">
        <v-text-field
          prepend-inner-icon="mdi-magnify"
          variant="outlined"
          v-debounce="val => search = val"
          class="w-100 medium-input"
          placeholder="Search by name"
          clearable
        />
      </div>

      <div>
        <!-- time range inputs -->
        <time-range-input
            class="ml-2 align-center" input-group-size="s" input-width="12rem"
            v-model="timeRangeInfo" :place-holder-tip="timePlaceHolderTip"/>
        <!-- /time range inputs -->
      </div>

      <v-btn
        class="ml-2 search-row-btn"
        color="primary"
        @click="seeAll = !seeAll"
        v-tooltip="seeAll ? 'Just show the audit logs created from your activity' : 'See all the audit logs that exist for all users (you can because you are an ADMIN!)'"
        @input="seeAllChanged"
        v-if="roles.includes('cont3xtAdmin')"
        :title="seeAll ? 'Just show the audit logs created from your activity' : 'See all the audit logs that exist for all users (you can because you are an ADMIN!)'">
        <span class="fa fa-user-circle mr-1" />
        See {{ seeAll ? ' MY ' : ' ALL ' }} History
      </v-btn>
    </div>

    <!--  history table  -->
    <v-data-table
      hover
      class="table-striped"
      :loading="loading"
      :headers="headers"
      :items="auditLogs"
      v-model:sort-by="sortBy"
      :items-per-page="-1"
      hide-default-footer
      :no-data-text="(search === '') ? 'There is no history to show for this period' : `There are no entries that match the search '${search}'`"
      >
      <!-- customize set-width columns -->
      <template #colgroup="scope">
        <col
            v-for="column in scope.columns"
            :key="column.key"
            :style="{ width: column.setWidth, ['min-width']: column.setMinWidth }"
        >
      </template>

      <!--   Button Column   -->
      <template #item.buttons="data">
        <v-btn v-if="getUser && getUser.removeEnabled"
            @click="deleteLog(data.item._id)"
            size="small"
            class="mini-table-button mr-1"
            color="warning"
            v-tooltip:top.close-on-content-click="'Delete history item'"
            title="Delete history item">
          <span class="fa fa-trash"/>
        </v-btn>
        <v-btn
            target="_blank"
            :href="reissueSearchLink(data.item)"
            size="small"
            class="mini-table-button"
            color="success"
            v-tooltip:top.close-on-content-click="'Repeat search'"
            title="Repeat search">
          <span class="fa fa-external-link"/>
        </v-btn>
      </template>
      <!--   /Button Column   -->

      <!--   Indicator Column (enforces max length)-->
      <template #item.indicator="data">
        <div class="indicator-limit-width">
          {{ data.item.indicator }}
        </div>
      </template>
      <!--   /Indicator Column (enforces max length)-->

      <!--   Tag Column   -->
      <template #item.tags="data">
        <template v-if="data.item.tags.length">
          <indicator-tag v-for="(tag, index) of data.item.tags" :key="index" :value="tag"/>
        </template>
        <template v-else>
          -
        </template>
      </template>
      <!--   /Tag Column   -->

      <!--   View Column   -->
      <template #item.viewId="data">
        <template v-if="data.item.viewId != null">
          <span v-if="viewLookup[data.item.viewId] != null" v-tooltip="data.item.viewId" class="text-success">
            {{viewLookup[data.item.viewId]}}
          </span>
          <span v-else>
            {{data.item.viewId}}
          </span>
        </template>
        <template v-else>
          -
        </template>
      </template>
      <!--   /View Column   -->
    </v-data-table>
  </div>
</template>

<script setup>
import AuditService from '@/components/services/AuditService';
import { reDateString } from '@/utils/filters';
import IndicatorTag from '@/utils/IndicatorTag.vue';
import TimeRangeInput from '@/utils/TimeRangeInput.vue';
import { useStore } from 'vuex';
import { useRouter, useRoute } from 'vue-router';
import { paramStr } from '@/utils/paramStr';
import { ref, computed, onMounted, watch } from 'vue';
import { useGetters } from '@/vue3-helpers';

const store = useStore();
const router = useRouter();
const route = useRoute();

const { getViews, getUser } = useGetters(store);

const roles = computed(() => getUser.value?.roles ?? []);
const viewLookup = computed(() => Object.fromEntries(getViews.value.map(view => [view._id, view.name])));

const headers = computed(() => {
  const showUserIds = roles.value.includes('cont3xtAdmin');

  // ensure that button field is NEVER broken
  const buttonFieldPx = (getUser.value?.removeEnabled)
    ? 90
    : 30;

  function format (key, formatterFn) {
    return (item) => formatterFn(item[key]);
  }
  return [
    { // virtual button field
      title: '',
      key: 'buttons',
      setWidth: `${buttonFieldPx}px`,
      setMinWidth: `${buttonFieldPx}px`,
      sortable: false
    },
    {
      title: 'Time',
      key: 'issuedAt',
      value: format('issuedAt', reDateString),
      setWidth: '12rem',
      setMinWidth: '12rem',
      sortRaw: (a, b) => 1
    },
    ...(showUserIds
      ? [{
        title: 'User ID',
        key: 'userId',
        setWidth: '5rem'
      }]
      : []),
    {
      title: 'iType',
      key: 'iType',
      setWidth: '5rem'
    },
    {
      title: 'Indicator',
      key: 'indicator',
      setWidth: '30rem'
    },
    {
      title: 'Tags',
      key: 'tags',
      sortable: true
    },
    {
      title: 'View',
      key: 'viewId',
      setWidth: '8rem'
    },
    {
      title: 'Results',
      key: 'resultCount',
      setWidth: '4rem',
      tdClass: 'text-right',
      value: format('resultCount', orQuestionMark)
    },
    {
      title: 'Took',
      key: 'took',
      setWidth: '4rem',
      cellProps: { class: 'text-right' },
      value: format('took', millisecondStr)
    }
  ];
});

onMounted(() => {
  if (route.query.seeAll === 'true') {
    seeAll.value = true;
  }

  loadAuditsFromSearch();
});

const loading = ref(true);
const auditLogs = ref([]);

const timeRangeInfo = ref({
  numDays: 7, // 1 week
  numHours: 7 * 24, // 1 week
  startDate: new Date(new Date().getTime() - (3600000 * 24 * 7)).toISOString().slice(0, -5) + 'Z', // 1 week ago
  stopDate: new Date().toISOString().slice(0, -5) + 'Z', // now
  startMs: Date.now() - (3600000 * 24 * 7), // by default, looks back one week
  stopMs: Date.now() // now
});
const lastTimeRangeInfoSearched = ref(null);
const timePlaceHolderTip = ref({
  title: 'These values specify the date range searched.<br>' +
      'Try using <a href="help#general" class="no-decoration">relative times</a> like -5d or -1h.'
});
const sortBy = ref([{ key: 'issuedAt', order: 'desc' }]);
const search = ref('');
const seeAll = ref(false);

watch(search, () => {
  loadAuditsFromSearch();
});
watch(timeRangeInfo, (newVal) => {
  // only re-search audits if there has already been a search, and the time-range has actually changed
  if (lastTimeRangeInfoSearched.value != null &&
      (newVal.startMs !== lastTimeRangeInfoSearched.value.startMs || newVal.stopMs !== lastTimeRangeInfoSearched.value.stopMs)) {
    loadAuditsFromSearch();
  }
});

function orQuestionMark (obj) {
  return obj ?? '?';
}
function millisecondStr (msNum) {
  return typeof msNum === 'number' ? `${msNum}ms` : '?';
}
function reissueSearchLink (log) {
  const resubmittedTags = log.tags?.length ? log.tags.join(',') : undefined;
  const allQueryParams = { b: window.btoa(log.indicator), view: log.viewId, tags: resubmittedTags, submit: 'y' };
  return `/${paramStr(allQueryParams)}`;
}
function deleteLog (id) {
  AuditService.deleteAudit(id).then(() => {
    auditLogs.value = auditLogs.value.filter(log => log._id !== id);
  }).catch((err) => console.log('ERROR - ', err));
}
function loadAuditsFromSearch () {
  lastTimeRangeInfoSearched.value = JSON.parse(JSON.stringify(timeRangeInfo.value));
  AuditService.getAudits({
    startMs: timeRangeInfo.value.startMs,
    stopMs: timeRangeInfo.value.stopMs,
    searchTerm: search.value === '' ? undefined : search.value,
    seeAll: seeAll.value
  }).then(audits => {
    auditLogs.value = audits;
    loading.value = false;
  }).catch(() => {
    loading.value = false;
  });
}
function seeAllChanged () {
  router.push({ query: { ...route.query, seeAll: seeAll.value ? 'true' : undefined } });
  loadAuditsFromSearch();
}
</script>

<style scoped>
.indicator-limit-width {
  max-width: 30rem;
  overflow-wrap: break-word;
}
.mini-table-button {
  height: 22px;
  min-width: revert;
  width: 22px;
  padding: 0;
}
</style>
