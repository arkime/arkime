<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-select
    v-model="perPage"
    class="medium-input"
    :items="perPageItems"
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
import { useI18n } from 'vue-i18n';

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
    default: () => [50, 100, 200, 500]
  }
});
const emit = defineEmits(['per-page-change']);

const { t } = useI18n();
const perPageItems = computed(() => props.perPageOptions.map(
  option => (typeof option === 'number' ? { value: option, text: t('common.perPage', { count: option }) } : option)
));

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
