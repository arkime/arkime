<template>
  <b-card v-if="data && data[itype]">
    <div class="row">
      <div class="col">
        <h4 class="text-orange display-inline mt-1">
          {{ itype.toUpperCase() }}
        </h4>
        <cont3xt-field
          :value="value || data[itype]._query"
        />
        <template v-if="data[itype].RDAP">
          <template v-for="(rdap, index) in data[itype].RDAP">
            <template v-if="value && value === rdap._query || !value">
              <span :key="`rdap-${value}-${index}`">
                <cont3xt-field
                  :value="rdap.data.link"
                  :options="{ copy: 'copy link' }"
                  :display="rdap.data.link | baseRIR"
                />
              </span>
            </template>
          </template>
        </template>
        <template v-if="data[itype].Maxmind">
          <template v-for="(mm, index) in data[itype].Maxmind">
            <template v-if="(value && value === mm._query) || !value">
              <span :key="`maxmind-${value}-${index}`">
                <h5 class="display-inline"
                  v-if="mm.data && mm.data.asn">
                  <label class="badge badge-dark cursor-help"
                    v-b-tooltip="mm.data.asn.autonomous_system_organization">
                    AS{{ mm.data.asn.autonomous_system_number }}
                    <!-- {{ mm.data.asn.autonomous_system_organization }} -->
                  </label>
                </h5>
                <h5 class="display-inline"
                  v-if="mm.data && mm.data.country"
                  v-b-tooltip="`${mm.data.country.country.names.en} (${mm.data.country.country.iso_code})`">
                  <label class="badge badge-dark cursor-help">
                    {{ countryEmoji(mm.data.country.country.iso_code) }}&nbsp;
                  </label>
                </h5>
              </span>
            </template>
          </template>
        </template>
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
        <label class="text-orange">
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
