<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <page-layout>
    <ViewConfigPage :remote="true">
      <template #loading>
        <ArkimeLoading />
      </template>
    </ViewConfigPage>
  </page-layout>
</template>

<script>
// ViewConfigPage is the shared /viewconfig page; viewer adds its app shell and
// is the only app that can ask other nodes for their config.
import PageLayout from '../utils/PageLayout.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ViewConfigPage from '@common/ViewConfigPage.vue';

export default {
  name: 'ViewConfig',
  components: {
    PageLayout,
    ArkimeLoading,
    ViewConfigPage
  }
};
</script>
