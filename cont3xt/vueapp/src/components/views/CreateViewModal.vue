<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-dialog
      width="800px"
      v-model="modalOpen"
      @after-leave="reset"
      scrollable>
    <v-card>
      <!-- header -->
      <template #title>
        <h4 class="mb-0">
          Create New View
        </h4>
      </template> <!-- /header -->
      <!-- form -->
      <ViewForm
      class="mx-4"
        :view="view"
        :focus="focusView"
        @update-view="update"
      />
      <!-- footer -->
      <template #actions>
        <div class="w-100 d-flex justify-space-between align-start">
          <v-btn
            @click="closeModal"
            color="warning">
            Cancel
          </v-btn>
          <v-alert
            height="40px"
            color="error"
            v-if="!!error.length"
            class="mb-0 alert-sm mr-1 ml-1">
            {{ error }}
          </v-alert>
          <v-btn
            @click="create"
            color="success">
            Create
          </v-btn>
        </div>
      </template> <!-- /footer -->
    </v-card>
  </v-dialog>
</template>

<script setup>
import ViewForm from '@/components/views/ViewForm.vue';
import UserService from '@/components/services/UserService';
import { ref, defineModel, watch } from 'vue';

const modalOpen = defineModel();

function makeDefaultView () {
  return {
    name: '',
    editRoles: [],
    viewRoles: [],
    integrations: []
  };
}

const error = ref('');
const focusView = ref(false);
const view = ref(makeDefaultView());

// when opened, set focus after delay to allow for rendering
watch(modalOpen, (newVal) => {
  if (newVal) {
    setTimeout(() => { focusView.value = true; }, 200);
  }
});

/* page functions ------------------------------------------------------ */
function closeModal () {
  modalOpen.value = false;
}
function update (updatedView) {
  view.value = updatedView;
}
function reset () {
  error.value = '';
  focusView.value = false;
  view.value = makeDefaultView();
}
function create () {
  error.value = '';

  if (!view.value.name) {
    error.value = 'Name required';
    return;
  }

  // NOTE: this function handles fetching the updated view list and storing it
  UserService.saveIntegrationsView(view.value).then((response) => {
    closeModal();
  }).catch((err) => {
    error.value = err.text || err;
  });
}
</script>

<style scoped>
.alert.alert-sm {
  padding: 0.4rem 0.8rem;
}
</style>
