<template>
  <b-modal
    id="transfer-modal"
    title="Transfer ownership to another user">
    <b-form>
      <b-input-group
        prepend="User ID">
        <b-form-input
          required
          id="userId"
          type="text"
          v-model="userId"
          :state="!userId ? false : true"
          placeholder="Enter a single user's ID">
        </b-form-input>
      </b-input-group>
    </b-form>
    <template #modal-footer>
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
          v-b-tooltip.hover
          :disabled="!userId"
          @click="transferResource"
          title="Transfer Ownership">
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
  data () {
    return {
      userId: ''
    };
  },
  methods: {
    cancel () {
      this.$emit('transfer-resource');
      this.$bvModal.hide('transfer-modal');
    },
    transferResource () {
      if (!this.userId) {
        return;
      }

      this.$emit('transfer-resource', { userId: this.userId });
      this.$bvModal.hide('transfer-modal');
    }
  }
};
</script>
