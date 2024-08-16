<template>
  <v-text-field :model-value="internalValue" @update:model-value="update">
    <slot/>
  </v-text-field>
</template>

<script setup>
// this utility component does what you would probably *want* v-model.trim="..." to do for v-text-fields
// ie: the value the consumer sees will always be trimmed, but users can still add whitespace to the start/end
// of the input (which will only then be included if they add a non-whitespace character after/before).

import { ref, watch, defineModel } from 'vue';

// TODO: toby define's eslint!
const model = defineModel();
const internalValue = ref(model.value);

// update internal value when the model is externally changed!
watch(model, () => {
  if (model.value !== internalValue.value.trim()) {
    update();
  }
});

function update (newValue) {
  internalValue.value = newValue;
  model.value = newValue.trim();
}
</script>
