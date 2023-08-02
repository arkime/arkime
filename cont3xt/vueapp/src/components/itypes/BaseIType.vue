<template>
  <b-card v-if="indicator.query" class="cursor-pointer itype-card" :class="{ 'border-danger': isActiveIndicator }" @click.stop="setSelfAsActiveIndicator">
    <div class="d-xl-flex">
      <div class="d-xl-flex flex-grow-1 flex-wrap mt-1 mw-100">
        <h4 class="text-warning m-0">
          {{ indicator.itype.toUpperCase() }}
        </h4>
        <cont3xt-field
            :value="indicator.query"
            class="align-self-center mr-1"
            :id="`${indicator.query}-${indicator.itype}`"
        />
        <!--    unlabeled tidbits    -->
        <template v-for="(tidbit, index) in unlabeledTidbits">
          <integration-tidbit :tidbit="tidbit" :key="index"
                              :id="`${indicator.query}-tidbit-${index}`"/>
        </template><!--    /unlabeled tidbits    -->
      </div>
    </div>

    <!--  labeled tidbits  -->
    <div v-if="labeledTidbits.length > 0"  class="mt-1">
      <div v-for="(tidbit, index) in labeledTidbits" :key="index" class="row ml-3" :class="{ 'mt-1': index > 0 }">
        <div class="col">
          <integration-tidbit :tidbit="tidbit" :id="`${indicator.query}-labeled-tidbit-${index}`"/>
        </div>
      </div>
    </div><!--  /labeled tidbits  -->

    <!--  children  -->
    <div v-if="children.length > 0" class="mt-2">
      <span v-for="(child, index) in children" :key="index">
        <i-type-node :node="child" />
      </span>
    </div> <!--  /children  -->
  </b-card>
</template>

<script>
import Cont3xtField from '@/utils/Field';
import IntegrationTidbit from '@/components/integrations/IntegrationTidbit';
import { mapGetters } from 'vuex';
import { Cont3xtIndicatorProp } from '@/utils/cont3xtUtil';

export default {
  name: 'BaseIType',
  components: {
    // NOTE: need async import here because there's a circular dependency
    //       between BaseIType and the different implementation types (which are contained in ITypeNode)
    // see: vuejs.org/v2/guide/components.html#Circular-References-Between-Components
    ITypeNode: () => import('@/components/itypes/ITypeNode'),
    Cont3xtField,
    IntegrationTidbit
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
    }
  },
  computed: {
    ...mapGetters(['getResults', 'getActiveIndicator']),
    labeledTidbits () {
      return this.tidbits.filter(tidbit => tidbit.label?.length);
    },
    unlabeledTidbits () {
      return this.tidbits.filter(tidbit => !tidbit.label?.length);
    },
    isActiveIndicator () {
      return this.indicator.query === this.getActiveIndicator.query && this.indicator.itype === this.getActiveIndicator.itype;
    }
  },
  methods: {
    setSelfAsActiveIndicator () {
      if (this.isActiveIndicator) { return; }

      this.$store.commit('SET_ACTIVE_INDICATOR', this.indicator);
      this.$store.commit('SET_ACTIVE_SOURCE', undefined);
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
