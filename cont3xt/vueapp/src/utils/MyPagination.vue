<template>
  <v-select
    v-model="perPage"
    :items="perPageOptions"
    item-title="text"
    item-value="value"
    style="max-width: fit-content"
    size="small"
  />
  <v-pagination
    :id="`pagination-${length}-${paginationIdCounter}`"
    :key="`pagination-${length}-${paginationIdCounter}`"
    size="small"
    class="search-row-btn m-0"
    :v-model="currentPage"
    :total-visible="pageNumbersVisible"
    :length="maxPages"
  />
</template>

<script setup>
import { defineProps, defineModel, defineEmits, watch, computed, ref } from 'vue';

// pagination id that is incremented to remove inbetween-state (otherwise, each increment [eg: '1 per page'] track currentPage locally)
const paginationIdCounter = ref(0);

const perPage = defineModel('perPage');
const currentPage = defineModel('currentPage');

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
