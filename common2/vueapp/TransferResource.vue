<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <b-modal
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
    <template #modal-footer>
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
          title="Transfer Ownership">
          :disabled="!userId"
          @click="transferResource"
          <span class="fa fa-share mr-1" />
          Transfer
        </v-btn>
      </div>
    </template> <!-- /modal footer -->
  </b-modal>
</template>

<script>
export default {
  name: 'TransferResource',
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
     */
    cancel () {
      this.userId = '';
      this.$emit('transfer-resource', {});
      this.$root.$emit('bv::hide::modal', 'transfer-modal');
    },
    /**
     * Transfer ownership of a resource to another user.
     * Emits a `transfer-resource` event with the `userId` as the payload to the parent.
     * NOTE: The parent needs to close the modal if there are no errors.
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
