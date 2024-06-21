<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <transfer-resource
      @transfer-resource="transferResource"
    />
    <v-btn
      color="primary"
      v-b-modal.transfer-modal
      title="Transfer Resource">
      Transfer Resource
    </v-btn>
  </div>
</template>

<script>
import TransferResource from '../../TransferResource';
export default {
  name: 'TransferResourceTest',
  components: {
    TransferResource
  },
  methods: {
    transferResource ({ userId }) {
      this.$emit('transfer-resource', { userId });
    }
  }
};
</script>
