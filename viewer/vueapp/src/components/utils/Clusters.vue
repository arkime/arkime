<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <b-dropdown
    v-if="multiviewer"
    right
    size="sm"
    class="multies-menu-dropdown pull-right ms-1"
    no-caret
    toggle-class="rounded"
    variant="theme-secondary"
    @show="esVisMenuOpen = true"
    @hide="esVisMenuOpen = false">
    <template #button-content>
      <div id="esMenuHoverText">
        <span class="fa fa-database me-1" />
        {{ selectedCluster.length }}
        <BTooltip target="esMenuHoverText">
          {{ esMenuHoverText }}
        </BTooltip>
      </div>
    </template>
    <b-dropdown-header>
      <input
        type="text"
        v-model="esQuery"
        class="form-control form-control-sm dropdown-typeahead"
        :placeholder="$t('utils.searchForClustersPlaceholder')">
    </b-dropdown-header>
    <template v-if="!selectOne">
      <b-dropdown-divider />
      <b-dropdown-item @click.prevent.stop="selectAllCluster">
        <span class="fa fa-list" />&nbsp;
        {{ $t('common.selectAll') }}
      </b-dropdown-item>
      <b-dropdown-item @click.prevent.stop="clearAllCluster">
        <span class="fa fa-eraser" />&nbsp;
        {{ $t('common.clearAll') }}
      </b-dropdown-item>
    </template>
    <b-dropdown-divider />
    <template v-if="esVisMenuOpen">
      <template
        v-for="(clusters, group) in filteredClusters"
        :key="group">
        <b-dropdown-header
          class="group-header">
          {{ group + ' (' + clusters.length + ')' }}
        </b-dropdown-header>
        <template
          v-for="cluster in clusters"
          :key="group + cluster + 'item'">
          <b-dropdown-item
            :id="group + cluster + 'item'"
            :class="{'active':isClusterVis(cluster)}"
            @click.prevent.stop="toggleClusterSelection(cluster)">
            {{ cluster }}
          </b-dropdown-item>
        </template>
      </template>
    </template>
  </b-dropdown>
  <div
    v-if="showMessage"
    class="alert alert-warning position-fixed fixed-bottom m-0 rounded-0"
    style="z-index: 2000;">
    <button
      type="button"
      class="btn-close pull-right"
      @click="showMessage = false" />
    {{ $t('utils.onlyOne') }}
  </div>
</template>

<script>
import { searchCluster } from '@common/vueFilters.js';

export default {
  name: 'Clusters',
  props: {
    selectOne: {
      type: Boolean,
      default: false
    }
  },
  emits: ['updateCluster'],
  data () {
    return {
      esQuery: '', // query for ES to toggle visibility
      showMessage: false,
      esVisMenuOpen: false,
      multiviewer: this.$constants.MULTIVIEWER
    };
  },
  watch: {
    selectOne (newValue) {
      if (!this.multiviewer) { return; }
      if (!newValue && this.$route.query.cluster !== 'none') {
        const clusterParam = this.$route.query.cluster.split(',') || [];
        if (clusterParam !== this.selectedCluster) {
          this.selectedCluster = clusterParam;
        }
      } else if (newValue && this.selectedCluster.length > 1) {
        this.showClusterAlert();
        this.selectedCluster = [this.availableCluster.active[0]] || undefined; // just the first one
        // update parent to override route params
        this.$emit('updateCluster', { cluster: this.selectedCluster.join(',') });
      }
    }
  },
  computed: {
    filteredClusters () {
      const filteredGroupedClusters = {};
      for (const group in this.availableCluster) {
        filteredGroupedClusters[group] = searchCluster(
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
      if (this.selectOne) { return; }
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
        if (this.selectOne) { // add to selected list
          this.selectedCluster = [cluster];
        } else {
          this.selectedCluster.push(cluster);
        }
      }
      this.updateRouteQueryForClusters(this.selectedCluster);
    },
    /* helper functions ---------------------------------------------------- */
    getClusters () { // set clusters to search if in multiviewer mode
      if (!this.multiviewer) { return; }

      // route query cluster param overrides clusterDefault
      const routeClusters = this.$route.query.cluster ? this.$route.query.cluster.split(',') : [];
      if (routeClusters.length > 0) {
        this.selectedCluster = [];
        for (let i = 0; i < routeClusters.length; i++) {
          if (this.availableCluster.active.includes(routeClusters[i])) {
            this.selectedCluster.push(routeClusters[i]);
          }
        }
        return;
      }

      // use clusterDefault if no route query params
      const clusterDefault = this.$constants.CLUSTER_DEFAULT;
      if (clusterDefault) {
        const defaultClusters = clusterDefault.split(',').map(c => c.trim());
        this.selectedCluster = [];
        for (let i = 0; i < defaultClusters.length; i++) {
          if (this.availableCluster.active.includes(defaultClusters[i])) {
            this.selectedCluster.push(defaultClusters[i]);
          }
        }
        return;
      }

      // default to ALL active available clusters if no route query params and clusterDefault is not set
      this.selectedCluster = this.availableCluster.active;
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
    },
    showClusterAlert () {
      this.showMessage = true;
      setTimeout(() => {
        this.showMessage = false;
      }, 10000);
    }
  },
  mounted () {
    if (!this.multiviewer) { return; } // only visible/enabled in multiviewer mode

    this.getClusters();

    if (this.selectOne && this.selectedCluster.length > 1) {
      this.showClusterAlert();
      this.selectedCluster = [this.availableCluster.active[0]] || undefined; // just the first one
      // update parent to override route params
      this.$emit('updateCluster', { cluster: this.selectedCluster.join(',') });
    }

    this.updateRouteQueryForClusters(this.selectedCluster);
  }
};
</script>
