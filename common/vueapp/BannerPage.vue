<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="arkime-container-fluid mt-3">
    <BannerSettings />
  </div>
</template>

<script setup>
// The admin page every app routes /banner at. Access is gated by each
// app's route guard, since only the app knows how it learns admin-ness.
import BannerSettings from '@common/BannerSettings.vue';
</script>
