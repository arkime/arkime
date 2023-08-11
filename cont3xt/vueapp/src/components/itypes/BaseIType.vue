<template>
  <b-card v-if="indicator.query"
          class="cursor-pointer itype-card" :class="{ 'border-danger': isActiveIndicator }"
          @click.stop="setSelfAsActiveIndicator">
    <div class="d-xl-flex" ref="nodeCardScrollMarker">
      <div class="d-xl-flex flex-grow-1 flex-wrap mw-100">
        <h4 class="text-warning m-0">
          {{ indicator.itype.toUpperCase() }}
        </h4>
        <cont3xt-field
            :value="indicator.query"
            class="align-self-center mr-1"
            :id="`${indicator.query}-${indicator.itype}`"
        />

        <integration-severity-counts :indicator-id="indicatorId" />

        <!--    unlabeled tidbits    -->
        <template v-for="(tidbit, index) in unlabeledTidbits">
          <integration-tidbit :tidbit="tidbit" :key="index"
                              :id="`${indicatorId}-tidbit-${index}`"/>
        </template><!--    /unlabeled tidbits    -->
      </div>
    </div>

    <!--  labeled tidbits  -->
    <div v-if="labeledTidbits.length > 0"  class="mt-1">
      <div v-for="(tidbit, index) in labeledTidbits" :key="index" class="row ml-3" :class="{ 'mt-1': index > 0 }">
        <div class="col">
          <integration-tidbit :tidbit="tidbit" :id="`${indicatorId}-labeled-tidbit-${index}`"/>
        </div>
      </div>
    </div><!--  /labeled tidbits  -->

    <!--  children  -->
    <div v-if="children.length > 0" class="mt-2">
      <template v-if="isCollapsed">
        <b-card class="itype-card" @click.stop="toggleCollapse">
          <span class="fa fa-plus fa-lg"/> {{ children.length }} hidden
        </b-card>
      </template>
      <template v-else>
        <span v-for="(child, index) in children" :key="index">
          <i-type-node :node="child" :parent-indicator-id="indicatorId" />
        </span>
      </template>
    </div> <!--  /children  -->
  </b-card>
</template>

<script>
import Cont3xtField from '@/utils/Field';
import IntegrationTidbit from '@/components/integrations/IntegrationTidbit';
import { mapGetters } from 'vuex';
import { Cont3xtIndicatorProp } from '@/utils/cont3xtUtil';
import IntegrationSeverityCounts from '@/components/integrations/IntegrationSeverityCounts.vue';

export default {
  name: 'BaseIType',
  components: {
    // NOTE: need async import here because there's a circular dependency
    //       between BaseIType and the different implementation types (which are contained in ITypeNode)
    // see: vuejs.org/v2/guide/components.html#Circular-References-Between-Components
    ITypeNode: () => import('@/components/itypes/ITypeNode'),
    Cont3xtField,
    IntegrationTidbit,
    IntegrationSeverityCounts
  },
  props: {
    indicator: Cont3xtIndicatorProp,
    tidbits: {
      type: Array,
      default () { return []; }
    },
    children: { // array of shape [{ indicator: { itype: string, query: string } }, ...]
      type: Array,
      default () { return []; }
    },
    indicatorId: {
      type: String,
      required: true
    }
  },
  computed: {
    ...mapGetters(['getResults', 'getActiveIndicatorId', 'getIndicatorIdToFocus', 'getCollapsedIndicatorNodeMap']),
    labeledTidbits () {
      return this.tidbits.filter(tidbit => tidbit.label?.length);
    },
    unlabeledTidbits () {
      return this.tidbits.filter(tidbit => !tidbit.label?.length);
    },
    isActiveIndicator () {
      return this.indicatorId === this.getActiveIndicatorId;
    },
    isCollapsed () {
      return this.getCollapsedIndicatorNodeMap[this.indicatorId];
    }
  },
  methods: {
    setSelfAsActiveIndicator () {
      if (this.isActiveIndicator) { return; }

      this.$store.commit('SET_ACTIVE_INDICATOR_ID', this.indicatorId);
      this.$store.commit('SET_ACTIVE_SOURCE', undefined);
    },
    toggleCollapse () {
      this.$store.commit('TOGGLE_INDICATOR_NODE_COLLAPSE', this.indicatorId);
    }
  },
  watch: {
    getIndicatorIdToFocus (val) {
      if (this.indicatorId === val) {
        this.$refs.nodeCardScrollMarker.scrollIntoView({ block: 'center', behavior: 'smooth' });
        // though the nodeCardScrollMarker element is not a focusable input
        //   this moves the relative focus point to it (as if clicked),
        //   so the next tab will go to the IntegrationBtns
        this.$refs.nodeCardScrollMarker.focus({
          preventScroll: true // don't double up on scrolling!
        });
      }
    }
  }
};
</script>

<style scoped>
/* effects only the directly-hovered itype-card */
.itype-card:hover:not(:has(.itype-card:hover)) {
  background-color: #d9dbde;
}
body.dark .itype-card:hover:not(:has(.itype-card:hover)) {
  background-color: #3d3d3d;
}
</style>
