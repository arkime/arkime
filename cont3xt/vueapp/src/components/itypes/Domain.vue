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
          <span v-if="data[itype].Whois && data[itype].Whois.data">
            <h5 class="display-inline">
              <label class="badge badge-dark"
                v-b-tooltip="data[itype].Whois.data.creationDate">
                {{ data[itype].Whois.data.creationDate | removeTime }}
              </label>
            </h5>
            <h5 class="display-inline"><label class="badge badge-dark">
              {{ data[itype].Whois.data.registrar }}
            </label></h5>
          </span>
          <integration-btns
            :data="data"
            :itype="itype"
          />
        </div>
      </div>
      <template v-if="data[itype].DNS">
        <hr>
        <div class="row medium"
          v-for="(value, key) in data[itype].DNS.data"
          :key="key">
          <div class="col ml-3"
            v-if="value.Answer && value.Answer.length">
            <template v-if="key === 'A' || key === 'AAAA'">
              <cont3xt-ip
                itype="ip"
                :data="data"
              />
            </template>
            <dl v-else
              class="dl-horizontal">
              <dt>{{ key }}</dt>
              <dd>
                <template v-for="(item, index) in value.Answer">
                  <cont3xt-field
                    :key="key+index"
                    :value="item.data"
                  />
                </template>
              </dd>
            </dl>
          </div>
        </div>
      </template>
    </div>
  </div>
</template>

<script>
import Cont3xtField from '@/utils/Field';
import Cont3xtIp from '@/components/itypes/IP';
import IntegrationBtns from '@/components/itypes/IntegrationBtns';

export default {
  name: 'Cont3xtDomain',
  components: {
    Cont3xtField,
    Cont3xtIp,
    IntegrationBtns
  },
  props: {
    data: Object,
    itype: String
  }
};
</script>
