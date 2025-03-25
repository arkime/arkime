<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <b-modal
    :model-value="showModal"
    id="transfer-modal"
    @keyup.stop.prevent.enter="transferResource"
    title="Transfer ownership to another user">
    <b-form
      @submit="transferResource"
      @keyup.stop.prevent.enter="transferResource">
      <!-- user ID input -->
      <b-input-group
        prepend="User ID">
        <b-form-input
          autofocus
          required
          id="userId"
          type="text"
          v-model="userId"
          :state="!userId ? false : true"
          @keyup.stop.prevent.enter="transferResource"
          placeholder="Enter a single user's ID">
        </b-form-input>
      </b-input-group> <!-- /user ID input -->
    </b-form>
    <!-- modal footer -->
    <template #footer>
      <div class="w-100 d-flex justify-content-between">
        <b-button
          title="Cancel"
          variant="danger"
          @click="cancel">
          <span class="fa fa-times" />
          Cancel
        </b-button>
        <b-button
          variant="success"
          :disabled="!userId"
          @click="transferResource">
          <span class="fa fa-share mr-1" />
          Transfer
        </b-button>
      </div>
    </template> <!-- /modal footer -->
  </b-modal>
</template>

<script>
export default {
  name: 'TransferResource',
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
