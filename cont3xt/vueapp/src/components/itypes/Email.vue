<template>
  <base-i-type
    :value="query"
    :itype="itype"
    :data="data"
    :tidbits="tidbits"
    :children="domainChildren"
  />
</template>

<script>
import BaseIType from '@/components/itypes/BaseIType';
import { ITypeMixin } from './ITypeMixin';

export default {
  name: 'Cont3xtEmail',
  mixins: [ITypeMixin], // for tidbits
  components: {
    BaseIType
  },
  props: {
    data: { // the data returned from cont3xt search
      type: Object,
      required: true
    },
    query: { // the query string to display (needed because emails don't get
      // searched, domains and IPs do so there is no data.email)
      type: String,
      required: true
    }
  },
  computed: {
    domainChildren () { // array containing the domain child resulting from
      const query = this.data?.domain?._query;
      return query ? [{ itype: 'domain', query }] : [];
    }
  },
  data () {
    return {
      itype: 'email'
    };
  }
};
</script>
