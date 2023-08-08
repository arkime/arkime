<template>
  <base-i-type
    :indicator-id="indicatorId"
    :indicator="indicator"
    :tidbits="tidbits"
  >
    <template #after-field>
      <ttl-tooltip v-if="enhanceInfo.ttl" :ttl="enhanceInfo.ttl" :target="`${indicator.query}-ip`"/>
    </template>
  </base-i-type>
</template>

<script>
import TtlTooltip from '@/utils/TtlTooltip';
import BaseIType from '@/components/itypes/BaseIType';
import { ITypeMixin } from './ITypeMixin';
import { Cont3xtIndicatorProp } from '@/utils/cont3xtUtil';

export default {
  name: 'Cont3xtIp',
  mixins: [ITypeMixin], // for tidbits
  components: {
    BaseIType,
    TtlTooltip
  },
  props: {
    indicator: Cont3xtIndicatorProp,
    children: {
      type: Array,
      required: true
    },
    enhanceInfo: {
      type: Object,
      required: true
    },
    indicatorId: {
      type: String,
      required: true
    }
  }
};
</script>
