<template>
  <base-i-type
      :indicator="indicator"
      :tidbits="tidbits"
  />
</template>

<script>
import BaseIType from '@/components/itypes/BaseIType';
import { ITypeMixin } from './ITypeMixin';
import { Cont3xtIndicatorProp } from '@/utils/cont3xtUtil';

export default {
  name: 'Cont3xtText',
  mixins: [ITypeMixin], // for tidbits
  components: {
    BaseIType
  },
  props: {
    indicator: Cont3xtIndicatorProp,
    children: {
      type: Array,
      required: true
    }
  }
};
</script>
