<template>
  <div class="card mb-2">
    <div class="card-body"
       v-if="data && data[itype]">
      <div class="row">
        <div class="col">
          <!-- TODO vertical align -->
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
                    :display="rdap.data.link | baseRIR"
                  />
                </span>
              </template>
            </template>
          </template>
          <template v-if="data[itype].Maxmind">
            <template v-for="(mm, index) in data[itype].Maxmind">
              <template v-if="value && value === mm._query || !value">
                <span :key="`maxmind-${value}-${index}`">
                  <h5 class="display-inline">
                    <label class="badge badge-dark"
                      v-b-tooltip="mm.data.creationDate">
                      {{ mm.data.asn.autonomous_system_organization }}
                    </label>
                  </h5>
                  <!-- TODO country flag? -->
                  <h5 class="display-inline"><label class="badge badge-dark">
                    {{ mm.data.country.country.names.en }}
                    ({{ mm.data.country.country.iso_code }})
                  </label></h5>
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
    </div>
  </div>
</template>

<script>
import Cont3xtField from '@/utils/Field';
import IntegrationBtns from '@/components/integrations/IntegrationBtns';

export default {
  name: 'Cont3xtIp',
  components: {
    Cont3xtField,
    IntegrationBtns
  },
  props: {
    data: Object,
    itype: String,
    value: String
  }
};
</script>
