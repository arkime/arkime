<template>
  <b-card>
    <div class="d-xl-flex mb-2">
      <div class="d-xl-flex flex-grow-1 flex-wrap mt-1" style="margin-bottom:-10px">
        <h4 class="text-warning">
          {{ itype.toUpperCase() }}
        </h4>
        <cont3xt-field
            :value="query == null ? '' : query"
            class="align-self-center mr-1"
        />
      </div>
    </div>
  </b-card>
</template>

<script>
import Cont3xtField from '@/utils/Field';

export default {
  name: 'BasicITypeCard',
  components: {
    Cont3xtField
  },
  props: {
    itype: {
      type: String,
      required: true
    },
    query: {
      type: [String, Object],
      required: true
    }
  }
};
</script>
