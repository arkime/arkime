<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="no-wrap d-flex flex-row w-100 mw-100 justify-space-between align-center">
    <v-icon
      class="mr-2"
      v-if="showITypeIcon"
      v-tooltip="overview.iType"
      :icon="iTypeIconMap[overview.iType]"
    />
    <span class="flex-grow-1 overview-nav-name">
      {{ overview.name }}
    </span>
    <v-icon
      @click.stop="setAsDefaultOverview"
      :icon="isSetAsDefault ? 'mdi-star' : 'mdi-star-outline'"
      :style="iTypeColorStyleMap[overview.iType]"
      v-tooltip:end="isSetAsDefault ? `Default for ${overview.iType} iType` : `Set as default for ${overview.iType} iType`"
    />
  </div>
</template>

<script>
import { mapGetters } from 'vuex';
import UserService from '@/components/services/UserService';
import { iTypeColorStyleMap, iTypeIconMap } from '@/utils/iTypes';

export default {
  name: 'OverviewSelectorLine',
  props: {
    overview: {
      type: Object,
      required: true
    },
    showITypeIcon: {
      type: Boolean,
      default: true
    }
  },
  data () {
    return {
      iTypeIconMap,
      iTypeColorStyleMap
    };
  },
  computed: {
    ...mapGetters(['getCorrectedSelectedOverviewIdMap']),
    isSetAsDefault () {
      return this.getCorrectedSelectedOverviewIdMap[this.overview.iType] === this.overview._id;
    }
  },
  methods: {
    setAsDefaultOverview () {
      this.$store.commit('SET_SELECTED_OVERVIEW_ID_FOR_ITYPE',
        { iType: this.overview.iType, id: this.overview._id });
      UserService.setUserSettings({ selectedOverviews: this.getCorrectedSelectedOverviewIdMap });
    }
  }
};
</script>

<style scoped>
/* this shortens the overview name with ellipsis without offsetting the to its right star */
.overview-nav-name {
  overflow: hidden;
  position: relative;
  white-space: nowrap;
  text-overflow: ellipsis;
  text-align: start;
}
</style>
