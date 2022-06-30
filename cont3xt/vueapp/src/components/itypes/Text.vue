<template>
  <b-card>
    <div class="d-xl-flex mb-2">
      <div class="d-xl-flex flex-grow-1 flex-wrap mt-1" style="margin-bottom:-10px">
        <h4 class="text-warning">
          {{ itype.toUpperCase() }}
        </h4>
        <cont3xt-field
          :value="initialQuery"
          class="align-self-center mr-1"
        />
      </div>
    </div>
  </b-card>
</template>

<script>
import Cont3xtField from '@/utils/Field';

export default {
  name: 'Cont3xtText',
  components: {
    Cont3xtField
  },
  props: {
    query: { // the query string to display (needed because text doesn't get
      // searched so there is no data.phone)
      type: String,
      required: true
    }
  },
  data () {
    return {
      itype: 'text',
      initialQuery: this.query
    };
  }
};
</script>
