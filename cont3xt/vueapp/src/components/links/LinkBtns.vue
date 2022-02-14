<template>
  <span>
    <b-button
      size="sm"
      variant="primary"
      @click="$emit('pushLink', { index, target: linkGroup.links.length })"
      v-b-tooltip.hover="'Push to the BOTTOM'">
      <span class="fa fa-arrow-circle-down fa-fw" />
    </b-button>
    <b-button
      size="sm"
      variant="warning"
      @click="$emit('pushLink', { index, target: 0 })"
      v-b-tooltip.hover="'Push to the TOP'">
      <span class="fa fa-arrow-circle-up fa-fw" />
    </b-button>
    <b-button
      size="sm"
      variant="secondary"
      @click="$emit('addSeparator', index)"
      v-b-tooltip.hover="'Add a Separator after this Link'">
      <span class="fa fa-underline fa-fw" />
    </b-button>
    <b-button
      size="sm"
      variant="info"
      @click="$emit('addLink', index)"
      v-b-tooltip.hover="'Add a Link after this one'">
      <span class="fa fa-link fa-fw" />
    </b-button>
    <b-button
      size="sm"
      variant="danger"
      @click="$emit('removeLink', index)"
      v-b-tooltip.hover="'Remove this link'">
      <span class="fa fa-times-circle" />
    </b-button>
    <b-button
      size="sm"
      variant="success"
      @click="$emit('expandLink', index)"
      v-b-tooltip.hover="linkGroup.links[index].expanded ? 'Collapse this link\'s details' : 'Expand this link\'s details'">
      <span class="fa"
        :class="linkGroup.links[index].expanded ? 'fa-chevron-up': 'fa-chevron-down'"
      />
    </b-button>
  </span>
</template>

<script>
export default {
  name: 'LinkBtns',
  props: {
    index: {
      type: Number,
      required: true
    },
    linkGroup: {
      type: Object,
      required: true
    }
  }
};
</script>
