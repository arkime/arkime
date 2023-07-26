<template>
  <base-i-type
      :indicator="indicator"
      :tidbits="tidbits"
      :children="children"
  />
</template>

<script>
// TODO: Toby, revert this file to merge! :0

import BaseIType from '@/components/itypes/BaseIType';
import { ITypeMixin } from './ITypeMixin';
import { Cont3xtIndicatorProp } from '@/utils/cont3xtUtil';

export default {
  name: 'Cont3xtDomain',
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
  },
  methods: {
    // splits the elements of an Answer into groups to have horizontal rules in between
    answerGroups (recordType, answer) {
      // split txt records into domain-verification, site-verification, other
      if (recordType === 'TXT') {
        return this.txtGroups(answer);
      }
      // most record types only have 1 group, so make an array of size one
      return [answer];
    },
    // grouping for TXT records
    txtGroups (answer) {
      const txtGroups = [[], [], []]; // initialize for all 3 possible groups
      for (const txtEntry of answer) {
        txtGroups[this.txtGroupIndex(txtEntry.data)].push(txtEntry);
      }
      // filter out empty groups, then sort alphabetically within groups
      const filteredTxtGroups = txtGroups.filter(group => group.length > 0);
      filteredTxtGroups.forEach(group => group.sort((a, b) => a.data.localeCompare(b.data)));
      return filteredTxtGroups;
    },
    txtGroupIndex (txtData) {
      if (txtData.includes('domain-verification')) { return 0; }
      if (txtData.includes('site-verification')) { return 1; }
      return 2;
    }
  }
};
</script>
