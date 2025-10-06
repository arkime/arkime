<template>
  <v-btn>
    <v-icon
      v-if="caret"
      icon="mdi-menu-down"
    />
    <v-menu
      v-model="openModel"
      activator="parent"
      location="bottom right"
    >
      <v-card>
        <v-list class="d-flex flex-column">
          <v-btn
            v-for="({ icon, text, action, active, tooltip }) in actions"
            :key="text"
            :active="!!active"
            @click="action"
            variant="text"
            class="justify-start"
          >
            <v-tooltip
              v-if="tooltip"
              activator="parent"
              location="start"
            >
              {{ tooltip }}
            </v-tooltip>
            <v-icon
              class="mr-1"
              :icon="icon"
            />
            {{ text }}
          </v-btn>
        </v-list>
      </v-card>
    </v-menu>
  </v-btn>
</template>

<script setup>
const openModel = defineModel();

/** @type {{ actions: { text: string, icon: string, action: ()=>void, active?: boolean, tooltip?: string }, ... }} */
defineProps({
  modelValue: {
    type: Boolean,
    default: false
  },
  actions: {
    type: Object,
    required: true
  },
  caret: {
    type: Boolean,
    default: true
  }
});
</script>
