<template>
  <i-type-basis
      :value="query"
      :itype="itype"
      :data="data"
  />
</template>

<script>
import ITypeBasis from '@/components/itypes/ITypeBasis';

export default {
  name: 'Cont3xtText',
  components: {
    ITypeBasis
  },
  props: {
    data: { // the data returned from cont3xt search
      type: Object, // NOTE: there will never be data specifically for text
      required: true
    },
    query: { // the query string to display (needed because text doesn't get
      // searched so there is no data.phone)
      type: String,
      required: true
    }
  },
  data () {
    return {
      itype: 'text'
    };
  }
};
</script>
