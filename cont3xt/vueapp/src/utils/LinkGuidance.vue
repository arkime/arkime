<template>
  <span>
    <span v-if="link.infoField" class="fa fa-info-circle cursor-help"
          v-b-tooltip.hover.topright.noninteractive="link.infoField" />
    <a :href="link.externalDocUrl" target="_blank" rel="noopener noreferrer"
       class="external-link-color cursor-pointer"
       v-b-tooltip.hover.topright.noninteractive="'External Documentation'">
      <span v-if="link.externalDocUrl" class="fa fa-external-link" />
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
    }
  }
};
</script>

<style scoped>
.external-link-color {
  color: var(--color-dark);
}
</style>
