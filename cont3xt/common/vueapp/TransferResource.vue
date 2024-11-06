<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-dialog
    width="800px"
    v-model="modalOpen"
    title="Transfer ownership to another user">
    <v-card>
      <v-form
        @submit="transferResource">
        <!-- user ID input -->
        <v-text-field
          class="medium-input ma-4 mb-0"
          autofocus
          label="User ID"
          v-model="userId"
          :rules=[!!userId]
          required
          @keyup.stop.prevent.enter="transferResource"
          placeholder="Enter a single user's ID"
        /> <!-- /user ID input -->
      </v-form>
      <!-- modal footer -->
      <template #actions>
        <div class="w-100 d-flex justify-space-between">
          <v-btn
            title="Cancel"
            color="error"
            @click="cancel">
            <span class="fa fa-times" />
            Cancel
          </v-btn>
          <v-btn
            color="success"
            v-tooltip="'Transfer Ownership'"
            title="Transfer Ownership"
            :disabled="!userId"
            @click="transferResource">
            <span class="fa fa-share mr-1" />
            Transfer
          </v-btn>
        </div>
      </template> <!-- /modal footer -->
    </v-card>
  </v-dialog>
</template>

<script setup>
import { ref } from 'vue';

const modalOpen = defineModel();

const emit = defineEmits(['transfer-resource']);

const userId = ref('');

/**
 * Cancel the transfer of a resource to another user and close the modal.
 * Emits a `transfer-resource` event with no payload to the parent.
 * NOTE: The parent should clear the resource to be transferred.
 */
function cancel () {
  userId.value = '';
  emit('transfer-resource', {});
  modalOpen.value = false;
}

/**
 * Transfer ownership of a resource to another user.
 * Emits a `transfer-resource` event with the `userId` as the payload to the parent.
 * NOTE: The parent needs to close the modal if there are no errors.
 */
function transferResource () {
  if (!userId.value) {
    return;
  }

  emit('transfer-resource', { userId: userId.value });
}
</script>
