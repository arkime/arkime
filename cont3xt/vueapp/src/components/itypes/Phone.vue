<template>
  <b-card
    v-if="data">
    <div class="d-xl-flex mb-2">
      <div class="d-xl-flex flex-grow-1 flex-wrap mt-1">
        <h4 class="text-warning">
          {{ itype.toUpperCase() }}
        </h4>
        <cont3xt-field
          :value="initialQuery"
          class="align-self-center mr-1"
        />
      </div>
      <div class="d-flex align-self-center justify-content-end">
        <integration-btns
          :data="data"
          :itype="itype"
          :value="initialQuery"
        />
      </div>
    </div>
  </b-card>
</template>

<script>
import Cont3xtField from '@/utils/Field';
import IntegrationBtns from '@/components/integrations/IntegrationBtns';

export default {
  name: 'Cont3xtPhone',
  components: {
    Cont3xtField,
    IntegrationBtns
  },
  props: {
    data: { // the data returned from cont3xt search
      type: Object,
      required: true
    },
    query: { // the query string to display (needed because phones don't get
      // searched so there is no data.phone)
      type: String,
      required: true
    }
  },
  data () {
    return {
      itype: 'phone',
      initialQuery: this.query
    };
  }
};
</script>
