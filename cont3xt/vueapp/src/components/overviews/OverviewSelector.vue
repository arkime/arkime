<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div
    v-if="selectedOverview && filteredOverviews && filteredOverviews.length > 0"
    class="text-no-wrap"
  >
    <v-btn
      id="overview-select-btn"
      color="secondary"
      size="small"
      variant="flat"
      class="btn-connect-right border-grey border-e-sm text-none"
      @click="selectOverview(selectedOverview._id)"
      v-tooltip:top="'Display this overview'"
    >
      <div class="no-wrap d-flex flex-row align-center">
        <span
          v-if="getShiftKeyHold"
          class="text-warning overview-hotkey-o"
        >O</span>
        <div class="overview-name-shorten">
          {{ selectedOverview.name }}
        </div>
      </div>
    </v-btn>
    <v-btn
      color="secondary"
      class="btn-connect-left"
      variant="flat"
      size="small"
      v-tooltip:top="'Select overview'"
    >
      <v-icon
        icon="mdi-menu-down"
        size="large"
      />

      <v-menu
        v-model="menuOpen"
        activator="parent"
        target="#overview-select-btn"
        :close-on-content-click="false"
      >
        <v-sheet class="d-flex flex-column overview-dropdown-menu">
          <v-text-field
            class="ma-1"
            prepend-inner-icon="mdi-magnify"
            ref="overviewSearchRef"
            v-model="query"
          />
          <v-btn
            v-for="(overview, i) in filteredOverviews"
            :key="i"
            block
            size="small"
            @click="selectOverview(overview._id)"
            variant="text"
            class="nav-link cursor-pointer btn-space-between"
          >
            <overview-selector-line
              :overview="overview"
              :show-i-type-icon="false"
            />
          </v-btn>
        </v-sheet>
      </v-menu>
    </v-btn>
  </div>
</template>

<script setup>
import { useStore } from 'vuex';
import { useGetters } from '@/vue3-helpers';
import OverviewSelectorLine from '@/components/overviews/OverviewSelectorLine.vue';
import { ref, computed, watch } from 'vue';

const props = defineProps({
  iType: {
    type: String,
    required: true
  },
  selectedOverview: {
    type: Object,
    required: false
  }
});

const emit = defineEmits(['set-override-overview']);

const store = useStore();
const {
  getSortedOverviews,
  getShiftKeyHold, getFocusOverviewSearch
} = useGetters(store);

const query = ref('');
const menuOpen = ref(false);
const overviewSearchRef = ref(undefined);

const filteredOverviews = computed(() =>
  getSortedOverviews.value.filter(overview =>
    overview.iType === props.iType &&
        (!query.value.length || overview.name.toLowerCase().match(query.value.toLowerCase())?.length > 0)
  )
);

watch(getFocusOverviewSearch, (val) => {
  if (val) { // shortcut for view dropdown search
    menuOpen.value = true;
    // we need a short timeout before we can focus the search bar within
    // the menu, since the input element is not rendered when closed
    setTimeout(() => {
      overviewSearchRef.value?.select();
    }, 100);
  }
});

function selectOverview (id) {
  store.commit('SET_ACTIVE_SOURCE', undefined);
  emit('set-override-overview', id);
}

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

.overview-dropdown-menu {
  width: 240px;
  overflow: hidden;
}

.overview-name-shorten {
  max-width: 14rem;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
</style>
