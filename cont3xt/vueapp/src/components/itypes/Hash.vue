<template>
  <base-i-type
      :value="query"
      :itype="itype"
      :data="data"
  />
</template>

<script>
import BaseIType from '@/components/itypes/BaseIType';

export default {
  name: 'Cont3xtHash',
  components: {
    BaseIType
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
