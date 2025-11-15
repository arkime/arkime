<template>
  <v-select
    v-model="perPage"
    class="medium-input"
    :items="perPageOptions"
    item-title="text"
    item-value="value"
    style="max-width: fit-content" />
  <v-pagination
    :id="`pagination-${perPage}-${paginationIdCounter}`"
    :key="`pagination-${perPage}-${paginationIdCounter}`"
    size="small"
    class="search-row-btn ma-0 pagination-reduce-padding"
    v-model="currentPage"
    :total-visible="pageNumbersVisible"
    :length="maxPages" />
</template>

<script setup>
import { watch, computed, ref } from 'vue';

// pagination id that is incremented to remove inbetween-state (otherwise, each increment [eg: '1 per page'] track currentPage locally)
const paginationIdCounter = ref(0);

const perPage = defineModel('perPage', { type: Number, default: 50 });
const currentPage = defineModel('currentPage', { type: Number, default: 1 });

const props = defineProps({
  pageNumbersVisible: {
    type: Number,
    default: 4
  },
  totalItems: {
    type: Number,
    required: true
  },
  perPageOptions: {
    type: Array,
    default: () => [
      { value: 50, text: '50 per page' },
      { value: 100, text: '100 per page' },
      { value: 200, text: '200 per page' },
      { value: 500, text: '500 per page' }
    ]
  }
});
const emit = defineEmits(['per-page-change']);

const maxPages = computed(() => Math.max(1, Math.ceil(props.totalItems / perPage.value)));

watch(perPage, (newValue) => {
  emit('per-page-change', newValue);
  paginationIdCounter.value++;

  if (currentPage.value > maxPages.value) {
    currentPage.value = maxPages.value;
  }
});
</script>

<style>
.pagination-reduce-padding ul {
  padding-left: 0.5rem;
}
</style>
