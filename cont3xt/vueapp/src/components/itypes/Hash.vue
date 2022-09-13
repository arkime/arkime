<template>
  <base-i-type
      :value="query"
      :itype="itype"
      :data="data"
      :tidbits="tidbits"
  />
</template>

<script>
import BaseIType from '@/components/itypes/BaseIType';
import { ITypeMixin } from './ITypeMixin';

export default {
  name: 'Cont3xtHash',
  mixins: [ITypeMixin], // for tidbits
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
