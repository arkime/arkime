<template>
  <div class="m-3">

    <!-- search -->
    <b-input-group>
      <template #prepend>
        <b-input-group-text>
          <span class="fa fa-search" />
        </b-input-group-text>
      </template>
      <b-form-input
        v-model="searchTerm"
        placeholder="Search"
        @keydown.enter="search"
      />
      <b-input-group-append>
        <b-button
          @click="search"
          variant="success">
          Search
        </b-button>
      </b-input-group-append>
    </b-input-group> <!-- /search -->

    <!-- page error -->
    <div
      v-if="error"
      class="mt-2 alert alert-danger">
      <span class="fa fa-exclamation-triangle" />&nbsp;
      {{ error }}
      <button
        type="button"
        @click="error = false"
        class="close cursor-pointer">
        <span>&times;</span>
      </button>
    </div> <!-- /page error -->

    <!-- results -->
    <div v-if="results">
      {{ results }}
    </div> <!-- /results -->

  </div>
</template>

<script>
import Cont3xtService from './Cont3xtService';

export default {
  name: 'Cont3xt',
  data () {
    return {
      error: '',
      results: null,
      searchTerm: ''
    };
  },
  computed: {
    loading: {
      get () { return this.$store.state.loading; },
      set (val) { this.$store.commit('SET_LOADING', val); }
    }
  },
  methods: {
    search () {
      console.log('SEARCHING FOR:', this.searchTerm); // TODO remove
      Cont3xtService.search(this.searchTerm).then((response) => {
        console.log('SUCCESS!', response); // TODO remove
        this.results = response;
      }).catch((error) => {
        console.log('FAILURE!', error); // TODO remove
        this.error = error;
      });
    }
  }
};
</script>
