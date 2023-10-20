<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <transfer-resource
      @transfer-resource="transferResource"
    />
    <b-button
      variant="primary"
      v-b-modal.transfer-modal
      title="Transfer Resource">
      Transfer Resource
    </b-button>
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
