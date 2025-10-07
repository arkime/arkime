<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <b-modal
    :model-value="showModal"
    id="transfer-modal"
    @keyup.stop.prevent.enter="transferResource"
    :title="$t('settings.transfer.title')"
  >
    <b-form
      @submit="transferResource"
      @keyup.stop.prevent.enter="transferResource"
    >
      <!-- user ID input -->
      <b-input-group
        :prepend="$t('settings.transfer.id')"
      >
        <b-form-input
          autofocus
          required
          id="userId"
          type="text"
          :model-value="userId"
          @update:model-value="userId = $event"
          :state="!userId ? false : true"
          @keyup.stop.prevent.enter="transferResource"
          :placeholder="$t('settings.transfer.id')"
        />
      </b-input-group> <!-- /user ID input -->
    </b-form>
    <!-- modal footer -->
    <template #footer>
      <div class="w-100 d-flex justify-content-between">
        <b-button
          :title="$t('common.cancel')"
          variant="danger"
          @click="cancel"
        >
          <span class="fa fa-times" />
          {{ $t('common.cancel') }}
        </b-button>
        <b-button
          variant="success"
          :disabled="!userId"
          @click="transferResource"
        >
          <span class="fa fa-share mr-1" />
          {{ $t('common.transfer') }}
        </b-button>
      </div>
    </template> <!-- /modal footer -->
  </b-modal>
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
