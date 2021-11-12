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
          :value="data[itype]._query"
           class="align-self-center mr-1"
        />
        <template v-if="data[itype].Whois">
          <template v-for="whois in data[itype].Whois">
            <template v-if="whois.data">
              <h5 class="align-self-end mr-1"
                :key="`whois-${whois._query}-date`"
                v-if="whois.data.creationDate">
                <b-badge
                  variant="light"
                  class="cursor-help"
                  v-b-tooltip="whois.data.creationDate">
                  {{ whois.data.creationDate | removeTime }}
                </b-badge>
              </h5>
              <h5 class="align-self-end"
                :key="`whois-${whois._query}-reg`"
                v-if="whois.data.registrar">
                <b-badge variant="light">
                  {{ whois.data.registrar }}
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
          :value="data[itype]._query"
        />
      </div>
    </div>
    <template v-if="data[itype].DNS">
      <span
        :key="`dns-${dns._query}`"
        v-for="dns in data[itype].DNS">
        <div
          :key="key"
          class="row medium"
          v-for="(value, key) in dns.data">
          <div class="col"
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
    Cont3xtIp,
    Cont3xtField,
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
