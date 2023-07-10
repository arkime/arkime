<template>
  <base-i-type
      :indicator="indicator"
      :tidbits="tidbits"
      :children="children"
  >
    <!--  non-ip dns records  -->
    <template #after-children>
      <template v-if="integrationDataMap.DNS">
        <div
            :key="key"
            class="row medium"
            v-for="(value, key) in integrationDataMap.DNS">
          <div class="col"
               v-if="value.Answer && value.Answer.length">
            <dl v-if="key !== 'A' && key !== 'AAAA'"
                class="dl-horizontal">
              <dt>
                {{ key }}
                ({{ value.Answer.length }})
              </dt>
              <dd>
                <div v-for="(group, groupIndex) in answerGroups(key, value.Answer)" :key="`${key}-${groupIndex}`">
                  <hr v-if="groupIndex > 0" class="m-0 bg-secondary">
                  <template v-for="(item, index) in group">
                    <cont3xt-field
                        :id="`${key}-${groupIndex}-${index}`"
                        :key="`${key}-${groupIndex}-${index}`"
                        :value="item.data"
                    />
                    <ttl-tooltip
                      :ttl="item.TTL"
                      :key="`${key}-${groupIndex}-${index}-ttl`"
                      :target="`${key}-${groupIndex}-${index}`"
                    />
                  </template>
                </div>
              </dd>
            </dl>
          </div>
        </div>
      </template>
    </template><!--  /non-ip dns records  -->
  </base-i-type>
</template>

<script>
import Cont3xtField from '@/utils/Field';
import TtlTooltip from '@/utils/TtlTooltip';
import BaseIType from '@/components/itypes/BaseIType';
import { ITypeMixin } from './ITypeMixin';
import { Cont3xtIndicatorProp } from '@/utils/cont3xtUtil';

export default {
  name: 'Cont3xtDomain',
  mixins: [ITypeMixin], // for tidbits
  components: {
    Cont3xtField,
    TtlTooltip,
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
