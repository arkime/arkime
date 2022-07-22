<template>
  <b-card
    v-if="data && data[itype]">
    <div class="d-xl-flex mb-2">
      <div class="d-xl-flex flex-grow-1 flex-wrap mt-1">
        <h4 class="text-warning">
          {{ itype.toUpperCase() }}
        </h4>
        <cont3xt-field
          class="align-self-center mr-1"
          :value="value || data[itype]._query"
          :id="`${value || data[itype]._query}-ip`"
        />
        <ttl-tooltip v-if="ttl" :ttl="ttl" :target="`${value || data[itype]._query}-ip`"/>

        <template v-if="data[itype].RDAP">
          <template v-for="(rdap, index) in data[itype].RDAP">
            <template v-if="value && value === rdap._query || !value">
              <cont3xt-field
                :value="rdap.data.link"
                class="align-self-center mr-1"
                :key="`rdap-${value}-${index}`"
                :options="{ copy: 'copy link' }"
                :display="rdap.data.link | baseRIR"
              />
            </template>
          </template>
        </template>
        <template v-if="data[itype].Maxmind">
          <template v-for="(mm, index) in data[itype].Maxmind">
            <template v-if="(value && value === mm._query) || !value">
              <h5 class="align-self-end mr-1"
                v-if="mm.data && mm.data.asn"
                :key="`maxmind-${value}-${index}-asn`">
                <b-badge variant="light">
                  AS{{ mm.data.asn.autonomous_system_number }}
                </b-badge>
                <b-badge variant="light">
                  {{ mm.data.asn.autonomous_system_organization }}
                </b-badge>
              </h5>
              <h5
                v-if="mm.data && mm.data.country"
                class="align-self-end mr-1 cursor-help"
                :key="`maxmind-${value}-${index}-country`"
                v-b-tooltip="`${mm.data.country.country.names.en} (${mm.data.country.country.iso_code})`">
                <b-badge variant="light">
                  {{ countryEmoji(mm.data.country.country.iso_code) }}&nbsp;
                </b-badge>
              </h5>
            </template>
          </template>
        </template>
        <template v-if="data[itype].Spur">
          <template v-for="(spur, index) in data[itype].Spur">
            <template v-if="(value && value === spur._query) || !value">
              <h5
                class="align-self-end mr-1"
                v-if="spur.data && spur.data.infrastructure"
                :key="`spur-${value}-${index}-infrastructure`">
                <b-badge variant="light">
                  {{ spur.data.infrastructure }}
                </b-badge>
              </h5>
            </template>
          </template>
        </template>
      </div>
      <div class="d-flex align-self-center justify-content-end">
        <integration-btns
          :data="data"
          :itype="itype"
          :value="value || data[itype]._query"
        />
      </div>
    </div>
    <div class="row ml-3"
      v-if="data[itype].RDAP">
      <div class="col">
        <label class="text-warning">
          Name
        </label>
        <template v-for="rdap in data[itype].RDAP">
          <template>
            <span :key="rdap.data.name"
              v-if="rdap._query === (value || data[itype]._query)">
              <cont3xt-field
                :value="rdap.data.name"
              />
            </span>
          </template>
        </template>
      </div>
    </div>
  </b-card>
  <basic-i-type-card :itype="itype" :query="query" v-else/>
</template>

<script>
import { countryCodeEmoji } from 'country-code-emoji';

import Cont3xtField from '@/utils/Field';
import BasicITypeCard from '@/utils/BasicITypeCard';
import IntegrationBtns from '@/components/integrations/IntegrationBtns';
import TtlTooltip from '@/utils/TtlTooltip';

export default {
  name: 'Cont3xtIp',
  components: {
    Cont3xtField,
    IntegrationBtns,
    BasicITypeCard,
    TtlTooltip
  },
  props: {
    data: { // the data returned from cont3xt search
      type: Object,
      required: true
    },
    value: { // the value to display as the IP (used if there are multiple ip
      type: String // results - like in a domain search)
    },
    query: {
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
