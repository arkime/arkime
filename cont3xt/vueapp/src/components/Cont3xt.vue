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
      results: [], // TODO object
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
      const self = this;
      self.results = [];
      // TODO how to make it like this: .subscribe().next(() => ...).error(() => ...).complete(() => ...)
      Cont3xtService.search(this.searchTerm).subscribe({
        next (data) {
          // TODO - check for finish: true - good finish
          console.log('GOT DATA IN CHUNK', data);
          self.results.push(data);
          // if (data.name) {
          //   self.results[data.name] = data.data;
          // }
          // TODO UPDATE PROGRESS BAR
        },
        error (e) {
          self.error = e;
        },
        complete () {
          console.log('request complete');
          // TODO UPDATE PROGRESS BAR
        }
      });
    }
  }
};
</script>
