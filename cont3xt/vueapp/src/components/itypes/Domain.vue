<template>
  <i-type-basis
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
                <template v-for="(item, index) in value.Answer">
                  <cont3xt-field
                      :id="`${key}-${index}`"
                      :key="`${key}-${index}`"
                      :value="item.data"
                  />
                  <ttl-tooltip :ttl="item.TTL" :key="`${key}-${index}-tooltip`" :target="`${key}-${index}`"/>
                </template>
              </dd>
            </dl>
          </div>
        </div>
      </template>
    </template><!--  /non-ip dns records  -->
  </i-type-basis>
</template>

<script>
import Cont3xtField from '@/utils/Field';
import TtlTooltip from '@/utils/TtlTooltip';
import ITypeBasis from '@/components/itypes/ITypeBasis';
import { ITypeMixin } from './ITypeMixin';

export default {
  name: 'Cont3xtDomain',
  mixins: [ITypeMixin],
  components: {
    Cont3xtField,
    TtlTooltip,
    ITypeBasis
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
  }
};
</script>
