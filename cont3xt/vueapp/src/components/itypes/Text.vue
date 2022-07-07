<template>
  <basic-i-type-card itype="text" :query="query"/>
</template>

<script>
import BasicITypeCard from '@/utils/BasicITypeCard';

export default {
  name: 'Cont3xtText',
  components: {
    BasicITypeCard
  },
  props: {
    query: { // the query string to display (needed because text doesn't get
      // searched so there is no data.phone)
      type: String,
      required: true
    }
  },
  data () {
    return {
      itype: 'text'
    };
  }
};
</script>
