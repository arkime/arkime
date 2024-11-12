<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-dialog
      width="1000px"
      v-model="modalOpen"
      @after-leave="reset"
      scrollable>
    <v-card>
      <!-- header -->
      <template #title>
        <h4 class="mb-0">
          Create New Overview
        </h4>
      </template> <!-- /header -->
      <!-- form -->
      <overview-form
          class="mx-4"
          :modifiedOverview="overview"
          :raw-edit-mode="rawEditMode"
          @update-modified-overview="updateOverview"
      /> <!-- /form -->
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
              color="warning"
              @click="rawEditMode = !rawEditMode"
              v-tooltip="'Edit the raw config for this link group'">
            <v-icon icon="mdi-pencil" />
          </v-btn>
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

import OverviewForm from './OverviewForm.vue';
import OverviewService from '../services/OverviewService';
import { ref } from 'vue';

const defaultOverview = {
  name: '',
  title: 'Overview of %{query}',
  iType: '',
  fields: [],
  viewRoles: [],
  editRoles: []
};

const modalOpen = defineModel();

const error = ref('');
const overview = ref(defaultOverview);
const rawEditMode = ref(false);

function closeModal () {
  modalOpen.value = false;
}

function reset () { // reset fields when hidden
  error.value = '';
  overview.value = defaultOverview;
  rawEditMode.value = false;
}

function updateOverview (updatedOverview) {
  overview.value = updatedOverview;
}
function create () {
  OverviewService.createOverview(overview.value).then(() => {
    closeModal();
  }).catch(err => {
    error.value = err.text || err;
  });
}
</script>
