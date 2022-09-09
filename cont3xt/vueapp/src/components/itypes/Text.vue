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
  name: 'Cont3xtText',
  mixins: [ITypeMixin], // for tidbits
  components: {
    BaseIType
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
