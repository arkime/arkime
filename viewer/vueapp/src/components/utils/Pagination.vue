<template>
  <!-- just show info, no controls -->
  <div
    v-if="infoOnly"
    class="pagination-info info-only">
    {{ t('common.showingAllTip', { start: commaString(recordsFiltered), total: commaString(recordsTotal) }) }}
  </div>

  <!-- show info and controls -->
  <div
    v-else
    class="d-inline-flex align-items-center">
    <!-- page size -->
    <v-select
      class="page-select"
      density="compact"
      variant="outlined"
      hide-details
      item-title="text"
      item-value="value"
      :items="lengthOptions"
      :model-value="pageLength"
      @update:model-value="lengthUpdated">
      <template #selection="{ item }">
        <span class="page-select-display">{{ item.raw.value }}</span>
      </template>
    </v-select> <!-- /page size -->

    <!-- paging -->
    <v-pagination
      class="paging-control"
      density="compact"
      :total-visible="3"
      :length="totalPages"
      :model-value="currentPage"
      @update:model-value="currentPageUpdated" /> <!-- /paging -->

    <!-- page info -->
    <span
      id="pagingInfo"
      class="pagination-info cursor-help">
      <span v-if="recordsFiltered">
        {{ t('common.showingRange', { start: commaString(start + 1), end: commaString(Math.min((start + pageLength), recordsFiltered)), total: commaString(recordsFiltered) }) }}
      </span>
      <span v-else>
        {{ t('common.showingAll', { start: commaString(start), total: commaString(recordsFiltered) }) }}
      </span>
      <v-tooltip activator="#pagingInfo">
        {{ pagingInfoTitle }}
      </v-tooltip>
    </span> <!-- /page info -->
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
  lengthDefault: {
    type: Number,
    default: 50
  },
  recordsTotal: {
    type: Number,
    default: 0
  },
  recordsFiltered: {
    type: Number,
    default: 0
  }
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
  return t('common.filteredFrom', { total });
});

const totalPages = computed(() => {
  return Math.max(1, Math.ceil(props.recordsFiltered / pageLength.value));
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
}

// helper methods -------------------------------------------------------------
function notifyParent () {
  start.value = (currentPage.value - 1) * pageLength.value;

  const pagingParams = {
    issueQuery: true,
    start: start.value,
    length: pageLength.value
  };

  emit('changePaging', pagingParams);
}
</script>

<style scoped>
/* Page-size select: just shows the number; compact inline */
.page-select {
  width: 76px;
  flex: 0 0 76px;
  font-size: 0.8rem;
}
/* Flex-center children of .v-field so __field (text) and __append-inner
   (caret) sit on the same baseline. */
.page-select :deep(.v-field) {
  --v-input-control-height: 32px;
  align-items: center;
}
.page-select :deep(.v-field__field) {
  align-items: center;
}
.page-select :deep(.v-field__input) {
  font-size: 0.8rem;
  padding-top: 0;
  padding-bottom: 0;
  padding-inline-start: 8px;
  padding-inline-end: 0;
  min-height: 32px;
  display: flex;
  align-items: center;
}
.page-select :deep(.v-field__append-inner) {
  padding-top: 0;
  padding-bottom: 0;
  padding-inline: 4px;
  align-items: center;
}
.page-select :deep(.v-field__append-inner .v-icon) {
  font-size: 16px;
  opacity: 0.6;
}
.page-select-display {
  white-space: nowrap;
  font-size: 0.8rem;
  line-height: 1;
}

/* v-pagination: tight buttons, no inner list margin, baseline-aligned;
   also kill the v-pagination wrapper padding so it sits flush against the
   page-size select on the left and the info text on the right. */
.paging-control {
  padding-left: 0;
  padding-right: 0;
}
.paging-control :deep(.v-pagination__list) {
  margin: 0;
  padding: 0;
  gap: 0;
}
.paging-control :deep(.v-pagination__list > li:first-child) {
  margin-inline-start: 0;
}
.paging-control :deep(.v-pagination__list > li:last-child) {
  margin-inline-end: 0;
}
.paging-control :deep(.v-btn) {
  --v-btn-height: 24px;
  width: 24px;
  min-width: 24px;
  font-size: 0.75rem;
}

/* Inline pagination info -- no boxy chrome, just sits next to controls */
.pagination-info {
  display: inline-block;
  font-size: 0.8rem;
  color: var(--color-foreground);
  white-space: nowrap;
}

/* When this component is used with infoOnly, restore a subtle pill */
.pagination-info.info-only {
  font-size: 0.8rem;
  color: var(--color-gray-dark);
  padding: 2px 8px;
  border-radius: var(--px-sm);
  background-color: transparent;
}
</style>
