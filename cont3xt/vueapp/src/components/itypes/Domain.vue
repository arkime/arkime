<template>
  <div class="card">
    <div class="card-body">
      <div class="row">
        <div class="col" v-if="data">
          <!-- TODO vertical align -->
          <h4 class="text-orange display-inline mt-1">
            DOMAIN
          </h4>
          <cont3xt-field
            :value="data._query"
          />
          <span v-if="data && data.Whois && data.Whois.data">
            <h5 class="display-inline">
              <label class="badge badge-dark"
                v-b-tooltip="data.Whois.data.creationDate">
                {{ data.Whois.data.creationDate | removeTime }}
              </label>
            </h5>
            <h5 class="display-inline"><label class="badge badge-dark">
              {{ data.Whois.data.registrar }}
            </label></h5>
          </span>
          <template v-for="(value, key) in getIntegrations">
            <b-button
              :key="key"
              variant="outline-dark pull-right ml-1"
              v-if="data[key] && value.icon"
              v-b-tooltip.hover="key"
              @click="$emit('integration', { itype: 'domain', source: key })">
              <img
                :alt="key"
                :src="value.icon"
                class="integration-img cursor-pointer"
              />
              <b-badge
                :variant="getLoading.failures.indexOf(key) < 0 ? 'success' : 'secondary'"
                v-if="data[key].data._count">
                {{ data[key].data._count }}
              </b-badge>
            </b-button>
          </template>
        </div>
      </div>
      <template v-if="data.DNS">
        <hr>
        <!-- TODO for A and AAAA show one per line and display ip itype underneath -->
        <div class="row medium"
          v-for="(value, key) in data.DNS.data"
          :key="key">
          <div class="col ml-3"
            v-if="value.Answer && value.Answer.length">
            <dl class="dl-horizontal">
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
import { mapGetters } from 'vuex';

import Cont3xtField from '@/utils/Field';

export default {
  name: 'Cont3xtDomain',
  components: { Cont3xtField },
  props: {
    data: Object
  },
  computed: {
    ...mapGetters(['getIntegrations', 'getLoading'])
  },
  mounted () { // TODO remove
    console.log('DATA!', this.data);
  }
};
</script>

<style scoped>
.integration-img {
  height: 25px;
  margin: 0 6px;
}
</style>
