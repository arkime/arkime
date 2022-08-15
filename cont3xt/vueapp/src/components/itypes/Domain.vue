<template>
  <base-i-type
      :itype="itype"
      :value="query"
      :data="data"
      :tidbits="tidbits"
      :children="dnsIpChildren"
  >
    <!--  non-ip dns records  -->
    <template #after-children>
      <template v-if="integrationData.DNS">
        <div
            :key="key"
            class="row medium"
            v-for="(value, key) in integrationData.DNS">
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

export default {
  name: 'Cont3xtDomain',
  mixins: [ITypeMixin],
  components: {
    Cont3xtField,
    TtlTooltip,
    BaseIType
  },
  props: {
    data: { // the data returned from cont3xt search
      type: Object,
      required: true
    },
    query: { // fallback in case data is non-existent
      type: String,
      required: false // not necessary when domain is sub-element (from url) -- as it is then guaranteed data
    }
  },
  computed: {
    dnsIpChildren () {
      return Object.entries(this.integrationData.DNS || [])
        .filter(([key, value]) => (key === 'A' || key === 'AAAA') && value.Answer?.length)
        .flatMap(([_, value]) => {
          return value.Answer.map(item => (
            { query: item.data, itype: 'ip', ttl: item.TTL }
          ));
        });
    }
  },
  data () {
    return {
      itype: 'domain'
    };
  },
  methods: {
    // can split the elements in an Answer into multiple groups to have horizontal rules in between
    answerGroups (recordType, answer) {
      // split txt records into domain-verification, site-verification, other
      if (recordType === 'TXT') {
        const txtGroups = [[], [], []]; // initialize for all 3 possible groups
        for (const txtEntry of answer) {
          txtGroups[this.txtGroupIndex(txtEntry.data)].push(txtEntry);
        }
        // return only groups with 1 or more entries
        return txtGroups.filter(group => group.length > 0);
      }
      // most record types only have 1 group, so make an array of size one
      return [answer];
    },
    txtGroupIndex (txtData) {
      if (txtData.includes('domain-verification')) { return 0; }
      if (txtData.includes('site-verification')) { return 1; }
      return 2;
    }
  }
};
</script>
