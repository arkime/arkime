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
      :per-page="pageLength"
      :model-value="currentPage"
      :total-rows="props.recordsFiltered"
      @update:model-value="currentPageUpdated"
    /> <!-- /paging -->

    <!-- page info -->
    <div id="pagingInfo"
      class="pagination-info cursor-help">
      Showing
      <span v-if="recordsFiltered">
        {{ commaString(start + 1) }}
      </span>
      <span v-else>
        {{ commaString(start) }}
      </span>
      <span v-if="recordsFiltered">
        - {{ commaString(Math.min((start + pageLength), recordsFiltered)) }}
      </span>
      of {{ commaString(recordsFiltered) }} entries
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
import { commaString } from '@real_common/vueFilters.js';

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
    { value: 10, text: '10 per page' },
    { value: 50, text: '50 per page' },
    { value: 100, text: '100 per page' },
    { value: 200, text: '200 per page' },
    { value: 500, text: '500 per page' },
    { value: 1000, text: '1000 per page' }
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

  const newQuery = { ...route.query, pageLength: newLength };

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
