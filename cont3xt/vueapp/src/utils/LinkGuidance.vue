<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span>
    <span v-if="link.infoField" class="fa fa-info-circle cursor-help"
          v-tooltip:top.close-on-content-click="link.infoField" />
    <a :href="link.externalDocUrl" target="_blank" rel="noopener noreferrer"
       class="external-link-color cursor-pointer"
       :id="`${elementId}-guidance`">
      <span v-if="link.externalDocUrl" class="fa fa-question-circle" />
      <v-tooltip :activator="`#${elementId}-guidance`">
        <a :href="link.externalDocUrl">{{ link.externalDocName || 'External Documentation' }} <span class="fa fa-external-link" /></a>
      </v-tooltip>
    </a>
  </span>
</template>

<script>
export default {
  name: 'LinkGuidance',
  props: {
    link: {
      type: Object,
      required: true
    },
    elementId: {
      type: String,
      required: true
    }
  }
};
</script>

<style scoped>
.external-link-color {
  color: rgb(var(--v-theme-dark));
}
</style>
