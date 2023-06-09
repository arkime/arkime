<template>
  <b-dropdown size="sm" class="ml-1 overview-dropdown" no-caret>
    <template #button-content>
      <span class="fa fa-fw fa-file-o" />
    </template>

    <div class="ml-1 mr-1 mb-2">
      <b-input-group size="sm" class="mb-1">
        <template #prepend>
          <b-input-group-text>
            <span class="fa fa-search" />
          </b-input-group-text>
        </template>
        <b-form-input
            v-model="query"
        />
      </b-input-group>
      <b-input-group size="sm">
        <template #prepend>
          <b-input-group-text>
            iType
          </b-input-group-text>
        </template>
        <b-form-select
            v-model="activeIType"
            :options="anyOrITypes"
        />
      </b-input-group>
    </div>

    <div>
      <template v-for="({ name, iType, _id }, i) in filteredOverviews">
        <b-dropdown-item-btn :key="i"
          @click="selectOverview(iType, _id)"
          button-class="p-0"
        >
          <div class="d-flex flex-row align-items-center w-100">
            <!--      TODO: toby, clean up styling please      -->
            <div class="pull-left" style="width: 10px; height: 30px" :style="iTypeColorStyleMap[iType]"></div>
            <span :class="{ 'text-success': getCorrectedSelectedOverviewIdMap[iType] === _id }">
              <span class="fa fa-fw" :class="[iTypeIconMap[iType]]" />{{ name }}
            </span>
          </div>
        </b-dropdown-item-btn>
        <hr v-if="i < filteredOverviews.length - 1 && iType !== filteredOverviews[i + 1].iType"
            :key="`hr-${i}`" class="border-secondary my-0">
      </template>
    </div>
  </b-dropdown>
</template>

<script>
import { mapGetters } from 'vuex';
import { iTypes, iTypeIconMap, iTypeColorStyleMap } from '@/utils/iTypes';
import UserService from '@/components/services/UserService';

export default {
  name: 'OverviewSelector',
  data () {
    return {
      query: ''
    };
  },
  computed: {
    ...mapGetters(['getSortedOverviews', 'getCorrectedSelectedOverviewIdMap']),
    filteredOverviews () {
      return this.getSortedOverviews.filter(overview => {
        if (this.activeIType !== 'any' && overview.iType !== this.activeIType) { return false; }
        return !this.query.length || overview.name.toLowerCase().match(this.query)?.length > 0;
      });
    },
    iTypeColorStyleMap () {
      return iTypeColorStyleMap;
    },
    iTypeIconMap () {
      return iTypeIconMap;
    },
    anyOrITypes () {
      return ['any'].concat(iTypes);
    },
    activeIType: {
      get () { return this.$store.state.overviewSelectorActiveIType; },
      set (value) { this.$store.commit('SET_OVERVIEW_SELECTOR_ACTIVE_ITYPE', value); }
    }
  },
  methods: {
    selectOverview (iType, id) {
      this.$store.commit('SET_SELECTED_OVERVIEW_ID_FOR_ITYPE', { iType, id });
      UserService.setUserSettings({ selectedOverviews: this.getCorrectedSelectedOverviewIdMap });
    }
  }
};
</script>

<style>
.overview-dropdown .dropdown-menu {
  width: 240px;
  overflow: hidden;
}
</style>
