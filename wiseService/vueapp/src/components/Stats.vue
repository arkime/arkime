<template>
  <div class="container-fluid">
    <Error :initialError="error" v-on:clear-initialError="error = ''"/>

    <div>
      <b-tabs content-class="mt-3">
        <b-tab title="Sources" active>
          <div v-if="sourceStats.length > 0">
            <b-table striped hover small borderless :items="sourceStats" :fields="sourceTableFields"></b-table>
          </div>
        </b-tab>

        <b-tab title="Types">
          <div v-if="typeStats.length > 0">
            <b-table striped hover small borderless :items="typeStats" :fields="typeTableFields"></b-table>
          </div>
        </b-tab>
      </b-tabs>
    </div>

  </div>
</template>

<script>
import WiseService from './wise.service';
import Error from './Error';

export default {
  name: 'Stats',
  components: {
    Error
  },
  data: function () {
    return {
      error: '',
      sourceStats: [],
      typeStats: [],
      sourceTableFields: [],
      typeTableFields: []
    };
  },
  mounted: function () {
    this.loadResourceStats();
  },
  methods: {
    loadResourceStats: function () {
      WiseService.getResourceStats()
        .then((data) => {
          this.error = '';
          if (data && data.sources && data.sources.length > 0) {
            this.sourceStats = data.sources;
            Object.keys(this.sourceStats[0]).forEach(key => {
              this.sourceTableFields.push({ key: key, sortable: true });
            });
          }
          if (data && data.types && data.types.length > 0) {
            this.typeStats = data.types;
            Object.keys(this.typeStats[0]).forEach(key => {
              this.typeTableFields.push({ key: key, sortable: true });
            });
          }
        })
        .catch((error) => {
          this.error = error.text ||
            `Error fetching resource stats for wise.`;
        });
    }
  }
};
</script>
