<template>
  <b-card
    class="mb-2"
    v-if="data">
    <div class="row">
      <div class="col">
        <h4 class="text-warning display-inline mt-1">
          {{ itype.toUpperCase() }}
        </h4>
        <cont3xt-field
          :value="initialQuery"
        />
      </div>
    </div>
  </b-card>
</template>

<script>
import Cont3xtField from '@/utils/Field';

export default {
  name: 'Cont3xtPhone',
  components: {
    Cont3xtField
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
