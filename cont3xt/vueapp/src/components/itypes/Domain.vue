<template>
  <b-card
    v-if="data && data[itype]">
    <div class="d-xl-flex mb-2">
      <div class="d-xl-flex flex-grow-1 flex-wrap mt-1">
        <h4 class="text-warning">
          {{ itype.toUpperCase() }}
        </h4>
        <cont3xt-field
          :value="data[itype]._query"
           class="align-self-center mr-1"
        />
        <!-- use whois first -->
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
        <!-- fallback to pt whois -->
        <template v-else-if="data[itype]['PT Whois']">
          <template v-for="ptwhois in data[itype]['PT Whois']">
            <template v-if="ptwhois.data">
              <template v-if="ptwhois.data.registered">
                <h5 class="align-self-end mr-1"
                  :key="`ptwhois-${ptwhois._query}-date`"
                  v-if="ptwhois.data.registered">
                  <b-badge
                    variant="light"
                    class="cursor-help"
                    v-b-tooltip="ptwhois.data.registered">
                    {{ ptwhois.data.registered | removeTime }}
                  </b-badge>
                </h5>
                <h5 class="align-self-end"
                  :key="`ptwhois-${ptwhois._query}-reg`"
                  v-if="ptwhois.data.registrar">
                  <b-badge variant="light">
                    {{ ptwhois.data.registrar }}
                  </b-badge>
                </h5>
              </template>
            </template>
          </template>
        </template>
        <!-- lastly check for vt whois -->
        <template v-else-if="data[itype]['VT Domain']">
          <template v-for="vtdomain in data[itype]['VT Domain']">
            <template v-if="vtdomain.data">
              <template v-if="vtdomain.data.whois">
                <h5 class="align-self-end"
                  :key="`vtdomain-${vtdomain._query}-date`">
                  <b-badge variant="light">
                    {{ getVTDomainField(vtdomain.data.whois, 'Creation Date: ') | removeTime }}
                  </b-badge>
                </h5>
                <h5 class="align-self-end"
                  :key="`vtdomain-${vtdomain._query}-reg`">
                  <b-badge variant="light">
                    {{ getVTDomainField(vtdomain.data.whois, 'Registrar: ') }}
                  </b-badge>
                </h5>
              </template>
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
              <dt>
                {{ key }}
                ({{ value.Answer.length }})
              </dt>
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
  <basic-i-type-card :itype="itype" :query="query" v-else/>
</template>

<script>
import BasicITypeCard from '@/utils/BasicITypeCard';
import Cont3xtField from '@/utils/Field';
import Cont3xtIp from '@/components/itypes/IP';
import IntegrationBtns from '@/components/integrations/IntegrationBtns';

export default {
  name: 'Cont3xtDomain',
  components: {
    Cont3xtIp,
    Cont3xtField,
    IntegrationBtns,
    BasicITypeCard
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
  data () {
    return {
      itype: 'domain'
    };
  },
  methods: {
    // parse the VT Domain whois field for values
    // NOTE: assumes that each value ends with \n
    getVTDomainField (data, fStr) {
      const start = data.indexOf(fStr) + fStr.length;
      const leftover = data.slice(start);
      const end = leftover.indexOf('\n');
      return data.slice(start, end + start);
    }
  }
};
</script>
