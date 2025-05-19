<template>
  <v-text-field :model-value="upstreamHasBetter ? model.trim() : internalValue" @update:model-value="updateText">
    <slot/>
  </v-text-field>
</template>

<script setup>
// this utility component does what you would probably *want* v-model.trim="..." to do for v-text-fields
// ie: the value the consumer sees will always be trimmed, but users can still add whitespace to the start/end
// of the input (which will only then be included if they add a non-whitespace character after/before).

import { ref, computed } from 'vue';

const model = defineModel();
const internalValue = ref(model.value);

// use model value, when we detect external change
const upstreamHasBetter = computed(() => model.value && model.value.trim() !== internalValue.value.trim());

function updateText (newValue) {
  internalValue.value = newValue;
  model.value = newValue.trim();
}
</script>
