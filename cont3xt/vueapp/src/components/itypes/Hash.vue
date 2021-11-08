<template>
  <b-card v-if="data && data[itype]">
    <div class="row">
      <div class="col">
        <h4 class="text-orange display-inline mt-1">
          {{ itype.toUpperCase() }}
        </h4>
        <cont3xt-field
          :value="data[itype]._query"
        />
        <integration-btns
          :data="data"
          :itype="itype"
          :value="data[itype]._query"
        />
      </div>
    </div>
  </b-card>
</template>

<script>
import Cont3xtField from '@/utils/Field';
import IntegrationBtns from '@/components/integrations/IntegrationBtns';

export default {
  name: 'Cont3xtHash',
  components: {
    Cont3xtField,
    IntegrationBtns
  },
  props: {
    data: { // the data returned from cont3xt search
      type: Object,
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
