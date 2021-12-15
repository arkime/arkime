<template>
  <b-card
    v-if="data"
    class="mb-2">
    <div class="d-xl-flex mb-2">
      <div class="d-xl-flex flex-grow-1 flex-wrap mt-1">
        <h4 class="text-warning">
          {{ itype.toUpperCase() }}
        </h4>
        <cont3xt-field
          :value="initialQuery"
          class="align-self-center mr-1"
        />
      </div>
      <div class="d-flex align-self-center justify-content-end">
        <integration-btns
          :data="data"
          :itype="itype"
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
import IntegrationBtns from '@/components/integrations/IntegrationBtns';

export default {
  name: 'Cont3xtEmail',
  components: {
    Cont3xtField,
    Cont3xtDomain,
    IntegrationBtns
  },
  props: {
    data: { // the data returned from cont3xt search
      type: Object,
      required: true
    },
    query: { // the query string to display (needed because emails don't get
      // searched, domains and IPs do so there is no data.email)
      type: String,
      required: true
    }
  },
  data () {
    return {
      itype: 'email',
      initialQuery: this.query
    };
  }
};
</script>
