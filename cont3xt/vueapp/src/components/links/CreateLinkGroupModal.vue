<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-dialog
    v-model="modalOpen"
    @after-leave="reset"
    scrollable
  >
    <v-card>
      <!-- header -->
      <template #title>
        <h4 class="mb-0">
          Create New Link Group
        </h4>
      </template> <!-- /header -->
      <!-- form -->
      <link-group-form
        class="mx-4"
        :raw-edit-mode="rawEditMode"
        @update-link-group="update"
      />
      <!-- footer -->
      <template #actions>
        <div class="w-100 d-flex justify-space-between align-start">
          <v-btn
            @click="closeModal"
            color="warning"
          >
            Cancel
          </v-btn>
          <v-alert
            height="40px"
            color="error"
            v-if="!!error.length"
            class="mb-0 alert-sm mr-1 ml-1"
          >
            {{ error }}
          </v-alert>
          <div>
            <v-btn
              color="secondary"
              @click="rawEditMode = !rawEditMode"
              v-tooltip:start="'Toggle raw configuration for this link group'"
            >
              <v-icon icon="mdi-pencil" />
            </v-btn>
            <v-btn
              @click="create"
              color="success"
            >
              Create
            </v-btn>
          </div>
        </div>
      </template> <!-- /footer -->
    </v-card>
  </v-dialog>
</template>

<script setup>
import LinkService from '@/components/services/LinkService';
import LinkGroupForm from '@/components/links/LinkGroupForm.vue';
import { ref } from 'vue';

const props = defineProps({
  modelValue: {
    type: Boolean,
    default: false
  }
});

const modalOpen = defineModel({ required: false, default: false, type: Boolean });

const error = ref('');
const linkGroup = ref({});
const rawEditMode = ref(false);

/* page functions ------------------------------------------------------ */
function update (updatedLinkGroup) {
  linkGroup.value = updatedLinkGroup;
}
function closeModal () {
  modalOpen.value = false;
}
function reset () { // reset fields when hidden
  error.value = '';
  linkGroup.value = {};
  rawEditMode.value = false;
}
function create () {
  if (!linkGroup.value.name || !linkGroup.value.name.length) {
    error.value = 'Group Name is required';
    return;
  }

  // validate the links
  for (const link of linkGroup.value.links) {
    if (!link.name.length) {
      error.value = 'Link Names are required';
      return;
    }
    if (!link.url.length) {
      error.value = 'Link URLs are required';
      return;
    }

    if (!link.itypes.length) {
      error.value = 'Must have at least one type per link';
      return;
    }
  }

  LinkService.createLinkGroup(linkGroup.value).then(() => {
    closeModal();
  }).catch((err) => {
    error.value = err;
  });
}
</script>

<style scoped>
.alert.alert-sm {
  padding: 0.4rem 0.8rem;
}
</style>
