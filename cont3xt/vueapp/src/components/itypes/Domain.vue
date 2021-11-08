<template>
  <b-card v-if="data && data[itype]">
    <div class="row">
      <div class="col">
        <h4 class="text-orange display-inline mt-1">
          {{ itype.toUpperCase() }}
        </h4>
        <cont3xt-field
          :value="data[itype]._query"
        />
        <template v-if="data[itype].Whois">
          <template v-for="whois in data[itype].Whois">
            <span v-if="whois.data"
              :key="`whois-${whois._query}`">
              <h5 class="display-inline"
                v-if="whois.data.creationDate">
                <label class="badge badge-dark"
                  v-b-tooltip="whois.data.creationDate">
                  {{ whois.data.creationDate | removeTime }}
                </label>
              </h5>
              <h5 class="display-inline"
                v-if="whois.data.registrar">
                <label class="badge badge-dark">
                  {{ whois.data.registrar }}
                </label>
              </h5>
            </span>
          </template>
        </template>
        <integration-btns
          :data="data"
          :itype="itype"
          :value="data[itype]._query"
        />
      </div>
    </div>
    <template v-if="data[itype].DNS">
      <hr>
      <span
        :key="`dns-${dns._query}`"
        v-for="dns in data[itype].DNS">
        <div
          :key="key"
          class="row medium"
          v-for="(value, key) in dns.data">
          <div
            class="col ml-3"
            v-if="value.Answer && value.Answer.length">
            <template v-if="key === 'A' || key === 'AAAA'">
              <template v-for="item in value.Answer">
                <span :key="item.data">
                  <cont3xt-ip
                    itype="ip"
                    :data="data"
                    :value="item.data"
                  />
                </span>
              </template>
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
      </span>
    </template>
  </b-card>
</template>

<script>
import Cont3xtField from '@/utils/Field';
import Cont3xtIp from '@/components/itypes/IP';
import IntegrationBtns from '@/components/integrations/IntegrationBtns';

export default {
  name: 'Cont3xtDomain',
  components: {
    Cont3xtField,
    Cont3xtIp,
    IntegrationBtns
  },
  props: {
    data: { // the data returned from cont3xt search
      type: Object,
      required: true
    }
  },
  data () {
    return {
      itype: 'domain'
    };
  }
};
</script>
