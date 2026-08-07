<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <page-layout>
    <div class="arkime-container-fluid mt-3">
      <BannerSettings />
    </div>
  </page-layout>
</template>

<script>
// BannerSettings is shared with cont3xt, so it stays a plain form component
// and this page only supplies the viewer's shell around it.
import PageLayout from '../utils/PageLayout.vue';
import BannerSettings from '@common/BannerSettings.vue';

export default {
  name: 'Banner',
  components: {
    PageLayout,
    BannerSettings
  }
};
</script>
