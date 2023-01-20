<template>
  <span>
    <b-dropdown
      right
      no-caret
      size="sm"
      variant="warning"
      v-if="getLinkGroups.length > 1"
      v-b-tooltip.hover="'Copy this link to another group'">
      <template #button-content>
        <span class="fa fa-copy fa-fw" />
      </template>
      <template
        v-for="group in getLinkGroups">
        <b-dropdown-item
          class="small"
          :key="group._id"
          v-if="group._id !== linkGroup._id"
          @click="$emit('copyLink', { link: linkGroup.links[index], groupId: group._id })">
          {{ group.name }}
        </b-dropdown-item>
      </template>
    </b-dropdown>
    <b-dropdown
      right
      size="sm"
      variant="primary"
      v-b-tooltip.hover="'Actions'">
      <b-dropdown-item
        class="small"
        @click="$emit('pushLink', { index, target: 0 })">
        <span class="fa fa-arrow-circle-up fa-fw" />
        Push to the TOP
      </b-dropdown-item>
      <b-dropdown-item
        class="small"
        @click="$emit('pushLink', { index, target: linkGroup.links.length })">
        <span class="fa fa-arrow-circle-down fa-fw" />
        Push to the BOTTOM
      </b-dropdown-item>
      <b-dropdown-item
        class="small"
        @click="$emit('addSeparator', index)">
        <span class="fa fa-underline fa-fw" />
        Add a Separator after this Link
      </b-dropdown-item>
      <b-dropdown-item
        class="small"
        @click="$emit('addLink', index)">
        <span class="fa fa-link fa-fw" />
        Add a Link after this one
      </b-dropdown-item>
      <b-dropdown-item
        class="small"
        @click="$emit('removeLink', index)">
        <span class="fa fa-times-circle fa-fw" />
        Remove this link
      </b-dropdown-item>
    </b-dropdown>
  </span>
</template>

<script>
import { mapGetters } from 'vuex';

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
  },
  computed: {
    ...mapGetters(['getLinkGroups'])
  }
};
</script>
