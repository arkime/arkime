<template>
  <b-dropdown
    split
    size="sm"
    ref="overviewsDropdown"
    class="overview-dropdown mr-1 mb-1"
    @shown="dropdownVisible = true"
    @hidden="dropdownVisible = false"
    @click="selectOverview(selectedOverview._id)"
    v-b-tooltip.hover.top="'Select overview'">
    <template #button-content>
      <div class="no-wrap d-flex flex-row align-items-center">
        <span v-if="getShiftKeyHold" class="text-warning overview-hotkey-o">O</span>
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
          ref="overviewsDropdownSearch"
          v-model="query"
        />
      </b-input-group>
    </div>

    <div>
      <template v-for="(overview, i) in filteredOverviews">
        <b-dropdown-item-btn :key="i"
          @click="selectOverview(overview._id)"
          button-class="px-1 py-0">
          <overview-selector-line
            :overview="overview"
            :show-i-type-icon="false"
          />
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
      query: '',
      needsFocus: false,
      dropdownVisible: false
    };
  },
  computed: {
    ...mapGetters([
      'getSortedOverviews', 'getCorrectedSelectedOverviewIdMap',
      'getShiftKeyHold', 'getFocusOverviewSearch'
    ]),
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
  watch: {
    getFocusOverviewSearch (val) {
      if (val) { // shortcut for view dropdown search
        if (!this.$refs.overviewsDropdown.visible) {
          this.$refs.overviewsDropdown.show();
        }
        if (this.dropdownVisible) {
          this.$refs.overviewsDropdownSearch.select();
        } else {
          // it can take a moment for the dropdown to show,
          //     so we'll focus it as soon as it pops up later
          this.needsFocus = true;
        }
      }
    },
    dropdownVisible (val) {
      if (val && this.needsFocus) {
        this.$refs.overviewsDropdownSearch.select();
        this.needsFocus = false;
      }
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
.overview-hotkey-o {
  /* pad the O shown when shifting to keep the button the same width */
  padding-inline: 0.13rem;
  /* and size it to look similar to the file-o icon */
  font-size: 1.15rem;
  /* keep the O from expanding the button to maintain the same height */
  margin-block: -5px;
}

.overview-dropdown .dropdown-menu {
  width: 240px;
  overflow: hidden;
}

.overview-name-shorten {
  max-width: 8rem;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
</style>
