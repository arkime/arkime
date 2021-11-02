<template>
  <div class="card">
    <div class="card-body">
      <div class="row">
        <div class="col" v-if="data">
          <strong class="text-orange">
            DOMAIN
          </strong>
          <cont3xt-field
            :value="data._query"
          />
          <!-- TODO better way? -->
          <span v-if="data && data.PassiveTotalWhois && data.PassiveTotalWhois.data">
            {{ data.PassiveTotalWhois.data.registered | removeTime }}
            {{ data.PassiveTotalWhois.data.registrar }}
          </span>
          <span v-for="(value, key) in data"
            :key="key">
            <template v-if="key !== 'DNS' && key[0] !== '_'">
              <img
                :alt="key"
                v-b-tooltip.hover="key"
                :src="`public/${key}.png`"
                class="integration-img cursor-pointer"
                @click="$emit('integration', { itype: 'domain', source: key })"
              />
            </template>
          </span>
        </div>
      </div>
      <template v-if="data.DNS">
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
import Cont3xtField from '@/utils/Field';

export default {
  name: 'Cont3xtDomain',
  components: { Cont3xtField },
  props: {
    data: Object
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
