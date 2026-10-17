<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span>
    <v-btn
      size="small"
      class="ms-1"
      v-tooltip="$t('cont3xt.linkGroups.copyToGroupTip')"
      color="warning">
      <v-icon icon="mdi-content-copy" />
      <v-menu
        activator="parent"
        location="bottom right">
        <v-card>
          <v-list class="d-flex flex-column">
            <template
              v-for="group in getLinkGroups"
              :key="group._id">
              <v-btn
                v-if="group._id !== linkGroup._id"
                @click="$emit('copyLink', { link: linkGroup.links[index], groupId: group._id })"
                variant="text"
                class="justify-start">
                {{ group.name }}
              </v-btn>
            </template>
          </v-list>
        </v-card>
      </v-menu>
    </v-btn>

    <action-dropdown
      size="small"
      :actions="[
        {
          icon: 'mdi-arrow-up',
          text: $t('cont3xt.linkGroups.pushTop'),
          action: () => $emit('pushLink', { index, target: 0 })
        },
        {
          icon: 'mdi-arrow-down',
          text: $t('cont3xt.linkGroups.pushBottom'),
          action: () => $emit('pushLink', { index, target: linkGroup.links.length })
        },
        {
          icon: 'mdi-format-underline',
          text: $t('cont3xt.linkGroups.addSeparator'),
          action: () => $emit('addSeparator', index)
        },
        {
          icon: 'mdi-link',
          text: $t('cont3xt.linkGroups.addLink'),
          action: () => $emit('addLink', index)
        },
        {
          icon: 'mdi-close',
          text: $t('cont3xt.linkGroups.removeLink'),
          action: () => $emit('removeLink', index)
        }
      ]"
      flat
      class="ms-1"
      tabindex="-1"
      color="info" />
  </span>
</template>

<script>
import { mapGetters } from 'vuex';
import ActionDropdown from '@/utils/ActionDropdown.vue';

export default {
  name: 'LinkBtns',
  emits: ['copyLink', 'pushLink', 'addSeparator', 'addLink', 'removeLink'],
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
