<template>
  <base-i-type
      :value="query"
      :itype="itype"
      :data="data"
      :children="sliceChildren"
  />
</template>

<script>
import BaseIType from '@/components/itypes/BaseIType';

export default {
  name: 'Cont3xtUrl',
  components: {
    BaseIType
  },
  props: {
    data: { // the data returned from cont3xt search
      type: Object,
      required: true
    },
    query: { // the query string to display (needed because urls don't get
      // searched, domains and IPs do so there is no data.url)
      type: String,
      required: true
    }
  },
  computed: {
    sliceChildren () { // array containing the child (either IP or domain) resulting from slicing the URL
      const childType = (!this.data?.ip || this.data?.domain) ? 'domain' : 'ip';
      const query = this.data?.[childType]?._query;
      return query ? [{ itype: childType, query }] : [];
    }
  },
  data () {
    return {
      itype: 'url'
    };
  }
};
</script>
