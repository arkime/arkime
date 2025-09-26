<template>

  <!-- just show info, no controls -->
  <div v-if="infoOnly"
    class="pagination-info info-only">
    Showing {{ commaString(recordsFiltered) }} entries,
    filtered from {{ commaString(recordsTotal) }} total entries
  </div>

  <!-- show info and controls -->
  <div v-else>

    <!-- page size -->
    <BFormSelect
      class="page-select"
      :options="lengthOptions"
      :model-value="pageLength"
      @update:model-value="lengthUpdated"
    /> <!-- /page size -->

    <!-- paging -->
    <BPagination
      size="sm"
      no-ellipsis
      :per-page="pageLength"
      :model-value="currentPage"
      :total-rows="props.recordsFiltered"
      @update:model-value="currentPageUpdated"
    /> <!-- /paging -->

    <!-- page info -->
    <div id="pagingInfo"
      class="pagination-info cursor-help">
      <span v-if="recordsFiltered">
        {{ t('common.showingRange', { start: commaString(start + 1), end: commaString(Math.min((start + pageLength), recordsFiltered)), total: commaString(recordsFiltered) }) }}
      </span>
      <span v-else>
        {{ ('common.showingAll', { start: commaString(start), total: commaString(recordsFiltered) }) }}
      </span>
      <BTooltip target="pagingInfo">{{ pagingInfoTitle }}</BTooltip>
    </div> <!-- /page info -->

  </div>

</template>

<script setup>
// setup ----------------------------------------------------------------------
// external imports
import { useStore } from 'vuex';
import { ref, computed } from 'vue';
import { useRoute, useRouter } from 'vue-router';
// internal imports
import { commaString } from '@common/vueFilters.js';

// router
const route = useRoute();
const router = useRouter();

// store
const store = useStore();

// emits
const emit = defineEmits(['changePaging']);

// data -----------------------------------------------------------------------
// props
const props = defineProps({
  infoOnly: Boolean,
  lengthDefault: Number,
  recordsTotal: Number,
  recordsFiltered: Number
});

import { useI18n } from 'vue-i18n';
const { t } = useI18n();

// local data
const start = ref(0);
const currentPage = ref(1);
const pageLength = ref(Math.min(parseInt(route.query.length || props.lengthDefault || 50), 1000));

// computed -------------------------------------------------------------------
const pagingInfoTitle = computed(() => {
  const total = commaString(props.recordsTotal);
  return `filtered from ${total} total entries`;
});

const lengthOptions = computed(() => {
  const options = [
    { value: 10, text: t('common.perPage', 10) },
    { value: 50, text: t('common.perPage', 50) },
    { value: 100, text: t('common.perPage', 100) },
    { value: 200, text: t('common.perPage', 200) },
    { value: 500, text: t('common.perPage', 500) },
    { value: 1000, text: t('common.perPage', 1000) }
  ];

  let exists = false;
  for (const option of options) {
    if (pageLength.value === option.value) {
      exists = true;
      break;
    }
  }

  if (!exists) {
    options.push({
      value: pageLength.value,
      label: `${pageLength.value} per page`
    });
  }

  options.sort(function (a, b) {
    return a.value - b.value;
  });

  return options;
});

// exposed page methods -------------------------------------------------------
function currentPageUpdated (newPage) {
  currentPage.value = newPage;
  notifyParent();
}

function lengthUpdated (newLength) {
  if (newLength === pageLength.value) {
    return;
  }

  pageLength.value = newLength; // update local value

  const newQuery = { ...route.query, length: newLength };

  const exprChanged = store.state.expression !== route.query.expression;
  if (exprChanged) {
    newQuery.expression = store.state.expression;
  }

  router.push({ query: newQuery }); // update route

  if (!exprChanged) {
    // only issue a new query if the expression hasn't changed. if it
    // has changed, a query will be issued by ExpressionTypeahead.vue
    notifyParent();
  }
};

// helper methods -------------------------------------------------------------
function notifyParent () {
  start.value = (currentPage.value - 1) * pageLength.value;

  const pagingParams = {
    issueQuery: true,
    start: start.value,
    length: pageLength.value
  };

  emit('changePaging', pagingParams);
};
</script>

<style scoped>
.pagination {
  display: inline-flex;
}

select.page-select {
  width: 130px;
  font-size: .8rem;
  display: inline-flex;
  height: 31px !important;
  margin-top: 1px;
  margin-right: -5px;
  margin-bottom: var(--px-xs);
  padding-top: var(--px-xs);
  padding-bottom: var(--px-xs);
  border-right: none;
  border-radius: var(--px-sm) 0 0 var(--px-sm);
  border-color: var(--color-gray-light);
  -webkit-appearance: none;
}

.pagination-info {
  display: inline-block;
  font-size: .8rem;
  color: var(--color-gray-dark);
  border: 1px solid var(--color-gray-light);
  padding: 5px 10px 4px 10px;
  margin-left: -6px;
  border-radius: 0 var(--px-sm) var(--px-sm) 0;
  background-color: var(--color-white);
}

.pagination-info.info-only {
  border-radius: var(--px-sm);
}
</style>
