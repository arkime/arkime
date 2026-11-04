<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span>
    <v-icon
      v-if="link.infoField"
      icon="mdi-information"
      class="cursor-help"
      v-tooltip:top.close-on-content-click="link.infoField" />
    <a
      :href="docUrl"
      target="_blank"
      rel="noopener noreferrer"
      class="external-link-color cursor-pointer"
      :id="`${elementId}-guidance`">
      <v-icon
        v-if="docUrl"
        icon="mdi-information" />
      <id-tooltip :target="`${elementId}-guidance`">
        <a :href="docUrl">{{ link.externalDocName || 'External Documentation' }} <v-icon icon="mdi-open-in-new" /></a>
      </id-tooltip>
    </a>
  </span>
</template>

<script>
import IdTooltip from '@/utils/IdTooltip.vue';
import { safeUrl } from '@common/vueFilters';

export default {
  name: 'LinkGuidance',
  components: { IdTooltip },
  props: {
    link: {
      type: Object,
      required: true
    },
    elementId: {
      type: String,
      required: true
    }
  },
  computed: {
    docUrl () {
      return safeUrl(this.link.externalDocUrl);
    }
  }
};
</script>

<style scoped>
.external-link-color {
  color: rgb(var(--v-theme-dark));
}
</style>
