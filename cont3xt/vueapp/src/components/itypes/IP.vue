<template>
  <b-card
    class="mb-2"
    v-if="data && data[itype]">
    <div class="d-flex mb-2">
      <div class="d-flex flex-grow-1 flex-wrap mt-1">
        <h4 class="text-warning">
          {{ itype.toUpperCase() }}
        </h4>
        <cont3xt-field
          class="align-self-center mr-1"
          :value="value || data[itype]._query"
        />
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
                <b-badge
                  variant="light"
                  class="cursor-help">
                  AS{{ mm.data.asn.autonomous_system_number }}
                </b-badge>
                <b-badge
                  variant="light"
                  class="cursor-help">
                  {{ mm.data.asn.autonomous_system_organization }}
                </b-badge>
              </h5>
              <h5 class="align-self-end"
                v-if="mm.data && mm.data.country"
                :key="`maxmind-${value}-${index}-country`"
                v-b-tooltip="`${mm.data.country.country.names.en} (${mm.data.country.country.iso_code})`">
                <b-badge
                  variant="light"
                  class="cursor-help">
                  {{ countryEmoji(mm.data.country.country.iso_code) }}&nbsp;
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
    <div class="row ml-4"
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
</template>

<script>
import { countryCodeEmoji } from 'country-code-emoji';

import Cont3xtField from '@/utils/Field';
import IntegrationBtns from '@/components/integrations/IntegrationBtns';

export default {
  name: 'Cont3xtIp',
  components: {
    Cont3xtField,
    IntegrationBtns
  },
  props: {
    data: { // the data returned from cont3xt search
      type: Object,
      required: true
    },
    value: { // the value to display as the IP (used if there are multiple ip
      type: String // results - like in a domain search)
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
