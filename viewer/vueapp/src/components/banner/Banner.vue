<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <page-layout>
    <BannerPage />
  </page-layout>
</template>

<script>
// BannerPage is the shared /banner page; viewer only adds its app shell.
import PageLayout from '../utils/PageLayout.vue';
import BannerPage from '@common/BannerPage.vue';

export default {
  name: 'Banner',
  components: {
    PageLayout,
    BannerPage
  }
};
</script>
