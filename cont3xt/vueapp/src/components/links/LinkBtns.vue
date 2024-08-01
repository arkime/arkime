<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span>
    <v-btn
      class="ml-1 skinny-search-row-btn"
      v-tooltip="'Copy this link to another group'"
      color="warning"
    >
      <span class="fa fa-fw fa-copy" />
      <v-menu activator="parent" location="bottom right">
        <v-card>
          <v-list class="d-flex flex-column">
            <template v-for="group in getLinkGroups" :key="group._id">
              <v-btn
                v-if="group._id !== linkGroup._id"
                @click="$emit('copyLink', { link: linkGroup.links[index], groupId: group._id })"
                variant="text"
                class="justify-start"
              >
                {{ group.name }}
              </v-btn>
            </template>
          </v-list>
        </v-card>
      </v-menu>
    </v-btn>

    <action-dropdown
      :actions="[
        {
          icon: 'fa-arrow-circle-up',
          text: 'Push to the TOP',
          action: () => $emit('pushLink', { index, target: 0 })
        },
        {
          icon: 'fa-arrow-circle-down',
          text: 'Push to the BOTTOM',
          action: () => $emit('pushLink', { index, target: linkGroup.links.length })
        },
        {
          icon: 'fa-underline',
          text: 'Add a Separator after this link',
          action: () => $emit('addSeparator', index)
        },
        {
          icon: 'fa-link',
          text: 'Add a Link after this link',
          action: () => $emit('addLink', index)
        },
        {
          icon: 'fa-times-circle',
          text: 'Remove this link',
          action: () => $emit('removeLink', index)
        }
      ]"
      class="ml-1 skinny-search-row-btn"
      tabindex="-1"
      color="info"
    />
  </span>
</template>

<script>
import { mapGetters } from 'vuex';
import ActionDropdown from '@/utils/ActionDropdown.vue';

export default {
  name: 'LinkBtns',
  components: { ActionDropdown },
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
