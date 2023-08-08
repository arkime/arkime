<template>
  <base-i-type
      :indicator-id="indicatorId"
      :indicator="indicator"
      :tidbits="tidbits"
  />
</template>

<script>
import BaseIType from '@/components/itypes/BaseIType';
import { ITypeMixin } from './ITypeMixin';
import { Cont3xtIndicatorProp } from '@/utils/cont3xtUtil';

export default {
  name: 'Cont3xtHash',
  mixins: [ITypeMixin], // for tidbits
  components: {
    BaseIType
  },
  props: {
    indicator: Cont3xtIndicatorProp,
    children: {
      type: Array,
      required: true
    },
    indicatorId: {
      type: String,
      required: true
    }
  }
};
</script>
