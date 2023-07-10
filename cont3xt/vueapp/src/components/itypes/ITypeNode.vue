<template>
  <cont3xt-domain
      :indicator="node.indicator"
      :children="node.children"
      v-if="node.indicator.itype === 'domain'"
  />
  <cont3xt-ip
      :indicator="node.indicator"
      :children="node.children"
      :enhance-info="node.enhanceInfo"
      v-else-if="node.indicator.itype === 'ip'"
  />
  <cont3xt-url
      :indicator="node.indicator"
      :children="node.children"
      v-else-if="node.indicator.itype === 'url'"
  />
  <cont3xt-email
      :indicator="node.indicator"
      :children="node.children"
      v-else-if="node.indicator.itype === 'email'"
  />
  <cont3xt-hash
      :indicator="node.indicator"
      :children="node.children"
      v-else-if="node.indicator.itype === 'hash'"
  />
  <cont3xt-phone
      :indicator="node.indicator"
      :children="node.children"
      v-else-if="node.indicator.itype === 'phone'"
  />
  <cont3xt-text
      :indicator="node.indicator"
      :children="node.children"
      v-else-if="node.indicator.itype === 'text'"
  />
  <h3 v-else class="text-warning">
    <span v-if="node && node.indicator">No display for {{ node.indicator.itype }}</span>
    <span v-else>No indicator given</span>
  </h3>
</template>

<script>

import Cont3xtDomain from '@/components/itypes/Domain.vue';
import Cont3xtIp from '@/components/itypes/IP.vue';
import Cont3xtUrl from '@/components/itypes/URL.vue';
import Cont3xtEmail from '@/components/itypes/Email.vue';
import Cont3xtHash from '@/components/itypes/Hash.vue';
import Cont3xtPhone from '@/components/itypes/Phone.vue';
import Cont3xtText from '@/components/itypes/Text.vue';

export default {
  name: 'ITypeNode',
  components: {
    Cont3xtText,
    Cont3xtPhone,
    Cont3xtHash,
    Cont3xtEmail,
    Cont3xtUrl,
    Cont3xtIp,
    Cont3xtDomain
  },
  props: {
    node: {
      type: Object,
      required: true
    }
  }
};
</script>
