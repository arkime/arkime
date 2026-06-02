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
    class="d-inline-flex align-center ga-1">
    <v-select
      density="compact"
      hide-details
      style="width: 130px;"
      item-title="text"
      item-value="value"
      :items="lengthOptions"
      :model-value="pageLength"
      @update:model-value="lengthUpdated" />

    <div
      class="arkime-input-group paging-wrapper"
      :class="{ 'paging-wrapper--hide-last': totalPages > 5 }">
      <v-pagination
        density="compact"
        size="small"
        :total-visible="5"
        :length="totalPages"
        :model-value="currentPage"
        @update:model-value="currentPageUpdated" />
    </div>

    <div
      id="pagingInfo"
      class="arkime-input-group paging-info-wrapper cursor-help">
      <span class="arkime-input-label">
        <span v-if="recordsFiltered">
          {{ t('common.showingRange', { start: commaString(start + 1), end: commaString(Math.min((start + pageLength), recordsFiltered)), total: commaString(recordsFiltered) }) }}
        </span>
        <span v-else>
          {{ t('common.showingAll', { start: commaString(start), total: commaString(recordsFiltered) }) }}
        </span>
      </span>
      <v-tooltip activator="#pagingInfo">
        {{ pagingInfoTitle }}
      </v-tooltip>
    </div>
  </div>
</template>

<style scoped>
:deep(.v-pagination__list) {
  margin-bottom: 0;
  padding-left: 0;
}

/* Drop the trailing "last page" button (e.g. ...1000) so the strip
   reads "< 1 2 3 ... >" instead of "< 1 2 3 ... 1000 >". Only applied
   when totalPages > totalVisible (i.e. when Vuetify would emit an
   ellipsis), via the paging-wrapper--hide-last class. The last <li>
   in v-pagination__list is .v-pagination__next; second-to-last is the
   trailing page item. */
.paging-wrapper--hide-last :deep(.v-pagination__list > li:nth-last-child(2)) {
  display: none;
}

/* v-pagination wrapped in an .arkime-input-group -- let the
   v-pagination fill the container vertically and zero its margin so
   the bordered box hugs it. */
.paging-wrapper {
  padding: 0 4px;
}
.paging-wrapper :deep(.v-pagination__list) {
  height: 100%;
  align-items: center;
}
/* Restyle the v-select to match .arkime-input-group -- override
   Vuetify's outlined v-field border with our gray border, 4px radius,
   32px height, theme-aware bg. Hides the SVG legend outline that
   v-field renders for floating labels. */
:deep(.v-select.v-input--density-compact) {
  --v-input-control-height: 32px;
  font-size: 0.8rem;
}
:deep(.v-select .v-field) {
  border: 1px solid rgb(var(--v-theme-neutral));
  border-radius: 4px;
  background-color: rgb(var(--v-theme-background));
  min-height: 32px;
  font-size: 0.8rem;
  transition: border-color 0.15s ease, box-shadow 0.15s ease;
}
:deep(.v-select .v-field__input) {
  font-size: 0.8rem;
}
:deep(.v-select .v-field__outline) {
  display: none;
}
:deep(.v-select .v-field:hover) {
  border-color: rgb(var(--v-theme-neutral-dark));
}
:deep(.v-select .v-field--focused) {
  border-color: rgb(var(--v-theme-primary));
  box-shadow: 0 0 0 1px rgb(var(--v-theme-primary));
}
</style>

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
