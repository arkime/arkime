<template>
  <b-card
    class="mb-2"
    v-if="data">
    <div class="row mb-2">
      <div class="col">
        <h4 class="text-warning display-inline mt-1">
          {{ itype.toUpperCase() }}
        </h4>
        <cont3xt-field
          :value="initialQuery"
        />
      </div>
    </div>
    <cont3xt-domain
      :data="data"
    />
  </b-card>
</template>

<script>
import Cont3xtField from '@/utils/Field';
import Cont3xtDomain from '@/components/itypes/Domain';

export default {
  name: 'Cont3xtUrl',
  components: {
    Cont3xtField,
    Cont3xtDomain
  },
  props: {
    data: { // the data returned from cont3xt search
      type: Object,
      required: true
    },
    query: { // the query string to display (needed because urls don't get
      // searched, domains and IPs do so there is no data.url)
      type: String,
      required: true
    }
  },
  data () {
    return {
      itype: 'url',
      initialQuery: this.query
    };
  }
};
</script>
