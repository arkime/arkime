<!--
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="tb-group content-find">
    <v-text-field
      :model-value="query"
      :placeholder="placeholder"
      prepend-inner-icon="mdi-magnify"
      clearable
      density="compact"
      variant="outlined"
      hide-details
      :error="!!error"
      :title="error || ''"
      class="tshark-filter-input content-find-input"
      @update:model-value="onInput"
      @keydown.enter.prevent="onEnter" />

    <!-- text mode: case-insensitive substring with an optional regex toggle -->
    <v-btn
      v-if="mode === 'text'"
      :active="isRegex"
      variant="text"
      :title="$t('sessions.detail.findRegex')"
      @click="toggleRegex">
      <v-icon icon="mdi-regex" />
    </v-btn>

    <v-menu
      v-else
      location="bottom start">
      <template #activator="{ props: activatorProps }">
        <v-btn
          v-bind="activatorProps"
          variant="text"
          :title="$t('sessions.detail.findTypeHint')"
          class="packet-options-select-btn">
          {{ $t('sessions.detail.findType.' + searchType) }}
          <v-icon
            icon="mdi-menu-down"
            class="ms-1" />
        </v-btn>
      </template>
      <v-list density="compact">
        <v-list-item
          v-for="st in searchTypes"
          :key="st"
          :active="st === searchType"
          @click="setSearchType(st)">
          {{ $t('sessions.detail.findType.' + st) }}
        </v-list-item>
      </v-list>
    </v-menu>

    <span
      class="content-find-count"
      :class="{ 'text-disabled': !matchCount }">
      {{ countLabel }}
    </span>
    <v-btn
      icon="mdi-chevron-up"
      variant="text"
      size="small"
      :disabled="!matchCount"
      :title="$t('sessions.detail.findPrev')"
      @click="$emit('prev')" />
    <v-btn
      icon="mdi-chevron-down"
      variant="text"
      size="small"
      :disabled="!matchCount"
      :title="$t('sessions.detail.findNext')"
      @click="$emit('next')" />
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue';

const props = defineProps({
  mode: { type: String, default: 'text' }, // 'text' (client) | 'bytes' (server)
  matchCount: { type: Number, default: 0 },
  currentIndex: { type: Number, default: -1 },
  capped: { type: Boolean, default: false },
  error: { type: String, default: '' },
  placeholder: { type: String, default: '' },
  initialQuery: { type: String, default: '' } // restore the box text when re-mounted (tab switch)
});

const emit = defineEmits(['search', 'next', 'prev']);

const query = ref('');
const isRegex = ref(false);
const searchTypes = ['ascii', 'asciicase', 'hex', 'regex', 'hexregex'];
const searchType = ref('ascii');

let debounceTimeout = null;

const payload = () => props.mode === 'bytes'
  ? { query: query.value, searchType: searchType.value }
  : { query: query.value, regex: isRegex.value };

const emitSearch = () => {
  debounceTimeout = null;
  emit('search', payload());
};

const onInput = (val) => {
  query.value = val ?? '';
  if (debounceTimeout) { clearTimeout(debounceTimeout); }
  if (!query.value) { emitSearch(); return; } // clear right away, don't wait out the debounce
  debounceTimeout = setTimeout(emitSearch, 500);
};

// Enter commits a pending edit; with nothing pending it steps through hits (Shift = back).
const onEnter = (e) => {
  if (debounceTimeout) { clearTimeout(debounceTimeout); emitSearch(); return; }
  e.shiftKey ? emit('prev') : emit('next');
};

const toggleRegex = () => {
  isRegex.value = !isRegex.value;
  if (query.value) { emitSearch(); }
};

const setSearchType = (st) => {
  searchType.value = st;
  if (query.value) { emitSearch(); }
};

const countLabel = computed(() => {
  if (!query.value || props.error) { return ''; }
  if (!props.matchCount) { return '0'; }
  const total = props.capped ? `${props.matchCount}+` : props.matchCount;
  if (props.currentIndex < 0) { return `${total}`; } // highlighted, not yet navigated
  return `${props.currentIndex + 1}/${total}`;
});

// Let a parent drive the box (e.g. tshark histogram chips).
const setQuery = (text, options = {}) => {
  query.value = text ?? '';
  if (options.regex !== undefined) { isRegex.value = options.regex; }
  if (debounceTimeout) { clearTimeout(debounceTimeout); }
  emitSearch();
};

defineExpose({ setQuery });

onMounted(() => { if (props.initialQuery) { query.value = props.initialQuery; } });
onUnmounted(() => { if (debounceTimeout) { clearTimeout(debounceTimeout); } });
</script>

<style scoped>
.content-find-count {
  font-size: 12px;
  min-width: 2.5rem;
  text-align: center;
  font-variant-numeric: tabular-nums;
  white-space: nowrap;
}
</style>
