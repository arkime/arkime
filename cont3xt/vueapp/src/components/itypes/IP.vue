<template>
  <base-i-type
    :value="query"
    :itype="itype"
    :data="data"
    :tidbits="tidbits"
  >
    <template #after-field>
      <ttl-tooltip v-if="ttl" :ttl="ttl" :target="`${query}-ip`"/>
    </template>
  </base-i-type>
</template>

<script>
import { countryCodeEmoji } from 'country-code-emoji';

import TtlTooltip from '@/utils/TtlTooltip';
import BaseIType from '@/components/itypes/BaseIType';
import { ITypeMixin } from './ITypeMixin';

export default {
  name: 'Cont3xtIp',
  mixins: [ITypeMixin], // for tidbits
  components: {
    BaseIType,
    TtlTooltip
  },
  props: {
    data: { // the data returned from cont3xt search
      type: Object,
      required: true
    },
    query: { // the value to display as the IP (used if there are multiple ip
      type: String,
      required: false // fallback in case of no results -- not required since DNS IPs are guaranteed results of some kind
    },
    ttl: {
      type: Number,
      required: false // TTL property on A/AAAA records
    }
  },
  data () {
    return {
      itype: 'ip'
    };
  },
  methods: {
    countryEmoji (code) {
      return countryCodeEmoji(code);
    }
  }
};
</script>
