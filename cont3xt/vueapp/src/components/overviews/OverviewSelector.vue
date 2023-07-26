<template>
  <b-dropdown size="sm" class="overview-dropdown" no-caret v-b-tooltip.hover.top="'Select overview'">
    <template #button-content>
      <div class="no-wrap d-inline-flex align-items-center">
        <span class="fa fa-fw fa-file-o mr-1" />
        <div class="overview-name-shorten">{{ selectedOverview.name }}</div>
      </div>
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
    </div>

    <div>
      <template v-for="(overview, i) in filteredOverviews">
        <b-dropdown-item-btn :key="i"
          @click="selectOverview(overview._id)"
          button-class="px-1 py-0"
        >
          <overview-selector-line :overview="overview"
              :show-i-type-icon="false" />
        </b-dropdown-item-btn>
      </template>
    </div>
  </b-dropdown>
</template>

<script>
import { mapGetters } from 'vuex';
import { iTypeIconMap, iTypeColorStyleMap } from '@/utils/iTypes';
import OverviewSelectorLine from '@/components/overviews/OverviewSelectorLine';

export default {
  name: 'OverviewSelector',
  components: { OverviewSelectorLine },
  props: {
    iType: {
      type: String,
      required: true
    },
    selectedOverview: {
      type: Object,
      required: true
    }
  },
  data () {
    return {
      query: ''
    };
  },
  computed: {
    ...mapGetters(['getSortedOverviews', 'getCorrectedSelectedOverviewIdMap']),
    filteredOverviews () {
      return this.getSortedOverviews.filter(overview => {
        return overview.iType === this.iType &&
            (!this.query.length || overview.name.toLowerCase().match(this.query.toLowerCase())?.length > 0);
      });
    },
    iTypeColorStyleMap () {
      return iTypeColorStyleMap;
    },
    iTypeIconMap () {
      return iTypeIconMap;
    }
  },
  methods: {
    selectOverview (id) {
      this.$store.commit('SET_ACTIVE_SOURCE', undefined);
      this.$emit('set-override-overview', id);
    }
  }
};
</script>

<style>
.overview-dropdown .dropdown-menu {
  width: 240px;
  overflow: hidden;
}

.overview-name-shorten {
  max-width: 6rem;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
</style>
