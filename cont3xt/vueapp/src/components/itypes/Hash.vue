<template>
  <b-card
    v-if="data && data[itype]">
    <div class="d-xl-flex mb-2">
      <div class="d-xl-flex flex-grow-1 flex-wrap mt-1">
        <h4 class="text-warning">
          {{ itype.toUpperCase() }}
        </h4>
        <cont3xt-field
          :value="data[itype]._query"
          class="align-self-center mr-1"
        />
      </div>
      <div class="d-flex align-self-center justify-content-end">
        <integration-btns
          :data="data"
          :itype="itype"
          :value="data[itype]._query"
        />
      </div>
    </div>
  </b-card>
  <basic-i-type-card :itype="itype" :query="query" v-else/>
</template>

<script>
import BasicITypeCard from '@/utils/BasicITypeCard';
import Cont3xtField from '@/utils/Field';
import IntegrationBtns from '@/components/integrations/IntegrationBtns';

export default {
  name: 'Cont3xtHash',
  components: {
    Cont3xtField,
    IntegrationBtns,
    BasicITypeCard
  },
  props: {
    data: { // the data returned from cont3xt search
      type: Object,
      required: true
    },
    query: { // fallback used in case data is non-existent
      type: String,
      required: true
    }
  },
  data () {
    return {
      itype: 'hash'
    };
  }
};
</script>
