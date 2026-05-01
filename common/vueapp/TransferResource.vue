<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-dialog
    :model-value="showModal"
    @update:model-value="onDialogUpdate"
    max-width="600"
    @keyup.stop.prevent.enter="transferResource">
    <v-card>
      <v-card-title>{{ $t('settings.transfer.title') }}</v-card-title>
      <v-card-text>
        <v-text-field
          autofocus
          required
          density="compact"
          variant="outlined"
          hide-details
          id="userId"
          :label="$t('settings.transfer.id')"
          :model-value="userId"
          :placeholder="$t('settings.transfer.id')"
          @update:model-value="userId = $event"
          @keyup.stop.prevent.enter="transferResource" />
      </v-card-text>
      <v-card-actions>
        <div class="w-100 d-flex justify-content-between">
          <button
            type="button"
            class="btn btn-danger"
            :title="$t('common.cancel')"
            @click="cancel">
            <span class="fa fa-times" />
            {{ $t('common.cancel') }}
          </button>
          <button
            type="button"
            class="btn btn-success"
            :disabled="!userId"
            @click="transferResource">
            <span class="fa fa-share me-1" />
            {{ $t('common.transfer') }}
          </button>
        </div>
      </v-card-actions>
    </v-card>
  </v-dialog>
</template>

<script>
export default {
  name: 'TransferResource',
  emits: ['transfer-resource'],
  props: {
    showModal: {
      type: Boolean,
      default: false
    }
  },
  data () {
    return {
      userId: ''
    };
  },
  methods: {
    /**
     * v-dialog two-way binds its open state via update:model-value;
     * we keep a one-way prop (showModal) as before, so when the dialog
     * is closed externally (esc, click-outside) we route through cancel().
     */
    onDialogUpdate (val) {
      if (!val) { this.cancel(); }
    },
    /**
     * Cancel the transfer of a resource to another user and close the modal.
     * Emits a `transfer-resource` event with no payload to the parent.
     * NOTE: The parent should clear the resource to be transferred.
     * NOTE: The parent should close the modal.
     */
    cancel () {
      this.userId = '';
      this.$emit('transfer-resource', {});
    },
    /**
     * Transfer ownership of a resource to another user.
     * Emits a `transfer-resource` event with the `userId` as the payload to the parent.
     * NOTE: The parent should close the modal.
     */
    transferResource () {
      if (!this.userId) {
        return;
      }

      this.$emit('transfer-resource', { userId: this.userId });
    }
  }
};
</script>
