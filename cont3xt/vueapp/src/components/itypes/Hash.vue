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
  name: 'Cont3xtHash',
  components: {
    ITypeBasis
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
