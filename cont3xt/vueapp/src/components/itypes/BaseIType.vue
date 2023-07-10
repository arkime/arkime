<template>
  <b-card v-if="indicator.query">
    <div class="d-xl-flex mb-2">
      <div class="d-xl-flex flex-grow-1 flex-wrap mt-1">
        <h4 class="text-warning">
          {{ indicator.itype.toUpperCase() }}
        </h4>
        <cont3xt-field
            :value="indicator.query"
            class="align-self-center mr-1"
            :id="`${indicator.query}-${indicator.itype}`"
        />
        <slot name="after-field"></slot>

        <!--    unlabeled tidbits    -->
        <template v-for="(tidbit, index) in unlabeledTidbits">
          <integration-tidbit :tidbit="tidbit" :key="index"
                              :id="`${indicator.query}-tidbit-${index}`"/>
        </template><!--    /unlabeled tidbits    -->

      </div>
      <div class="d-flex align-self-center justify-content-end">
        <integration-btns
          :indicator="indicator"
        />
      </div>
    </div>

        <!--  labeled tidbits  -->
    <div v-for="(tidbit, index) in labeledTidbits" class="row ml-3" :key="index">
      <div class="col">
        <integration-tidbit :tidbit="tidbit" :id="`${indicator.query}-labeled-tidbit-${index}`"/>
      </div>
    </div><!--  /labeled tidbits  -->

    <span v-for="(child, index) in children" :key="index">
      <i-type-node :node="child" />
    </span>
    <slot name="after-children"></slot>
  </b-card>
</template>

<script>
import Cont3xtField from '@/utils/Field';
import IntegrationBtns from '@/components/integrations/IntegrationBtns';
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
    IntegrationBtns,
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
    ...mapGetters(['getResults']),
    labeledTidbits () {
      return this.tidbits.filter(tidbit => tidbit.label?.length);
    },
    unlabeledTidbits () {
      return this.tidbits.filter(tidbit => !tidbit.label?.length);
    }
  }
};
</script>

<style scoped>

</style>
