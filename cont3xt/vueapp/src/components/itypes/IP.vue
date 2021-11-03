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
            :value="data[itype]._query"
          />
          <span v-if="data[itype].Maxmind && data[itype].Maxmind.data">
            <h5 class="display-inline">
              <label class="badge badge-dark"
                v-b-tooltip="data[itype].Maxmind.data.creationDate">
                {{ data[itype].Maxmind.data.asn.autonomous_system_organization }}
              </label>
            </h5>
            <h5 class="display-inline"><label class="badge badge-dark">
              {{ data[itype].Maxmind.data.country.country.names.en }}
              ({{ data[itype].Maxmind.data.country.country.iso_code }})
            </label></h5>
          </span>
          <integration-btns
            :data="data"
            :itype="itype"
          />
        </div>
      </div>
      <template v-if="data[itype].RDAP">
        <hr>
        <template class="row medium"
          v-for="(value, key) in data[itype].RDAP.data">
          <div
            v-if="key[0] !== '_'"
            class="col ml-3"
            :key="key">
            <dl class="dl-horizontal">
              <dt>{{ key }}</dt>
              <dd>
                <cont3xt-field
                  :value="value"
                />
              </dd>
            </dl>
          </div>
        </template>
      </template>
    </div>
  </div>
</template>

<script>
import Cont3xtField from '@/utils/Field';
import IntegrationBtns from '@/components/itypes/IntegrationBtns';

export default {
  name: 'Cont3xtIp',
  components: {
    Cont3xtField,
    IntegrationBtns
  },
  props: {
    data: Object,
    itype: String
  }
};
</script>
