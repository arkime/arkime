<template>
  <div class="no-wrap d-flex d-inline-flex w-100 justify-content-between align-items-center">
    <span
        v-if="showITypeIcon"
        class="fa fa-fw mr-1"
        :class="[ iTypeIconMap[overview.iType] ]"
        v-b-tooltip.hover="overview.iType"
    />
    <span class="flex-grow-1 overview-nav-name">
      {{ overview.name }}
    </span>
    <span
        @click.stop="setAsDefaultOverview"
        class="fa fa-fw pull-right"
        :class="[isSetAsDefault ? 'fa-star' : 'fa-star-o']"
        :style="iTypeColorStyleMap[overview.iType]"
        v-b-tooltip.hover.right="isSetAsDefault ? `Default for ${overview.iType} iType` : `Set as default for ${overview.iType} iType`"
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
  width: calc(100% - 10px);
  overflow: hidden;
  position: relative;
  white-space: nowrap;
  text-overflow: ellipsis;
}
</style>
