<template>
  <b-dropdown v-if="multiviewer"
    right
    size="sm"
    class="multies-menu-dropdown pull-right ml-1"
    no-caret
    toggle-class="rounded"
    variant="theme-secondary"
    @show="esVisMenuOpen = true"
    @hide="esVisMenuOpen = false">
    <template slot="button-content">
      <div v-b-tooltip.hover.left :title="esMenuHoverText">
        <span class="fa fa-database"> </span>
        <span> {{ selectedCluster.length }} </span>
      </div>
    </template>
    <b-dropdown-header>
      <input type="text"
        v-model="esQuery"
        class="form-control form-control-sm dropdown-typeahead"
        placeholder="Search for Clusters..."
      />
    </b-dropdown-header>
    <b-dropdown-divider>
    </b-dropdown-divider>
      <b-dropdown-item @click.native.capture.stop.prevent="selectAllCluster">
      <span class="fa fa-list"></span>&nbsp;
      Select All
    </b-dropdown-item>
    <b-dropdown-item @click.native.capture.stop.prevent="clearAllCluster">
      <span class="fa fa-eraser"></span>&nbsp;
      Clear All
    </b-dropdown-item>
    <b-dropdown-divider>
    </b-dropdown-divider>
    <template v-if="esVisMenuOpen">
      <template v-for="(clusters, group) in filteredClusters">
        <b-dropdown-header
          :key="group"
          class="group-header">
          {{ group + ' (' + clusters.length + ')' }}
        </b-dropdown-header>
        <template v-for="cluster in clusters">
          <b-dropdown-item
            :id="group + cluster + 'item'"
            :key="group + cluster + 'item'"
            :class="{'active':isClusterVis(cluster)}"
            @click.native.capture.stop.prevent="toggleClusterSelection(cluster)">
            {{ cluster }}
          </b-dropdown-item>
        </template>
      </template>
    </template>
  </b-dropdown>
</template>

<script>
export default {
  name: 'Clusters',
  data () {
    return {
      esQuery: '', // query for ES to toggle visibility
      esVisMenuOpen: false,
      multiviewer: this.$constants.MOLOCH_MULTIVIEWER
    };
  },
  computed: {
    filteredClusters () {
      const filteredGroupedClusters = {};
      for (const group in this.availableCluster) {
        filteredGroupedClusters[group] = this.$options.filters.searchCluster(
          this.esQuery,
          this.availableCluster[group]
        );
      }
      return filteredGroupedClusters;
    },
    selectedCluster: {
      get () {
        return this.$store.state.esCluster.selectedCluster || [];
      },
      set (newValue) {
        this.$store.commit('setSelectedCluster', newValue);
      }
    },
    availableCluster: {
      get: function () {
        return this.$store.state.esCluster.availableCluster;
      },
      set: function (newValue) {
        this.$store.commit('setAvailableCluster', newValue);
      }
    },
    esMenuHoverText () {
      if (this.selectedCluster.length === 0) {
        return 'No Selection';
      } else if (this.selectedCluster.length === 1) {
        return this.selectedCluster[0];
      } else {
        return this.selectedCluster.length + ' out of ' + this.availableCluster.active.length + ' selected';
      }
    }
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    isClusterVis (cluster) {
      if (this.availableCluster.active.includes(cluster)) { // found in active cluster list
        return this.selectedCluster.includes(cluster); // returns True if found in selected cluster list
      } else { // inactive cluster
        return false;
      }
    },
    selectAllCluster () {
      this.selectedCluster = this.availableCluster.active;
      this.updateRouteQueryForClusters(this.selectedCluster);
    },
    clearAllCluster () {
      this.selectedCluster = [];
      this.updateRouteQueryForClusters(this.selectedCluster);
    },
    toggleClusterSelection (cluster) {
      if (this.selectedCluster.includes(cluster)) { // already selected; remove from selection
        this.selectedCluster = this.selectedCluster.filter((item) => {
          return item !== cluster;
        });
      } else if (!this.availableCluster.inactive.includes(cluster)) { // not in inactive cluster
        this.selectedCluster.push(cluster); // add to selected list
      }
      this.updateRouteQueryForClusters(this.selectedCluster);
    },
    /* helper functions ---------------------------------------------------- */
    getClusters () {
      if (this.multiviewer) { // set clusters to search if in multiviewer mode
        const clusters = this.$route.query.cluster ? this.$route.query.cluster.split(',') : [];
        if (clusters.length === 0) {
          this.selectedCluster = this.availableCluster.active;
        } else {
          this.selectedCluster = [];
          for (let i = 0; i < clusters.length; i++) {
            if (this.availableCluster.active.includes(clusters[i])) {
              this.selectedCluster.push(clusters[i]);
            }
          }
        }
      }
    },
    updateRouteQueryForClusters (clusters) {
      const cluster = clusters.length > 0 ? clusters.join(',') : 'none';
      if (!this.$route.query.cluster || this.$route.query.cluster !== cluster) {
        this.$router.push({
          query: {
            ...this.$route.query,
            cluster
          }
        });
      }
    }
  },
  mounted () {
    this.getClusters();
  }
};
</script>
