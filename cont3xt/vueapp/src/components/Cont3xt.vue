<template>
  <div class="mr-2 ml-2">

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
          Get Cont3xt
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
      <pre>{{ results }}</pre>
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
      results: {},
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
      this.error = '';
      this.results = {};
      this.$store.commit('RESET_LOADING');

      Cont3xtService.search(this.searchTerm).subscribe({
        next: (data) => {
          if (data.name) { // add the data to the page
            this.$set(this.results, data.name, data.data);
          }
          if (data.sent && data.total) {
            this.loading = { // update the progress bar
              total: data.total,
              failed: data.failed,
              received: data.sent
            };
          }
          if (data.finished) { // we finished receiving results
            this.loading = { // set received to total so progress bar completes
              received: this.$store.state.loading.total
            };
          }
        },
        error: (e) => {
          this.error = e;
        },
        complete: () => {
          this.loading = { done: true };
        }
      });
    }
  }
};
</script>
