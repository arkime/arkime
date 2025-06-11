<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="footer">
    <p>
      <small>
        <FooterDataComponent v-if="FooterDataComponent" />
      </small>
    </p>
  </div>
</template>

<script setup>
import { inject, shallowRef } from 'vue';
import footerData from './FooterData.js';
// async component defined above with html injected from constants
// use shallowRef to avoid performance overhead because this variable is a Vue component, not a normal data object
const FooterDataComponent = shallowRef(null);

// Inject the constants object to access the footer configuration
const constants = inject('constants');

// Create a Vue instance for the footer data component using the HTML string from constants
FooterDataComponent.value = footerData.getVueInstance(new DOMParser().parseFromString(constants.FOOTER_CONFIG, 'text/html').documentElement.textContent);
</script>

<style scoped>
.footer {
  text-align: center;
  color: var(--color-gray-dark);
  border-top: 1px solid var(--color-gray-light);
  position: absolute;
  bottom: 0;
  width: 100%;
  height: 25px;
  line-height: 25px;
  overflow: hidden;
}
</style>
