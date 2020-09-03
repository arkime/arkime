<template>
  <div>
    <Alert :initialAlert="alertMessage" variant="alert-danger" v-on:clear-initialAlert="alertMessage = ''"/>

    <div>
      <b-tabs content-class="mt-3">
        <b-tab title="Sources" active>
          <div v-if="sourceStats.length > 0">
            <b-table striped hover :items="sourceStats" :fields="sourceTableFields"></b-table>
          </div>
        </b-tab>

        <b-tab title="Types">
          <div v-if="typeStats.length > 0">
            <b-table striped hover :items="typeStats" :fields="typeTableFields"></b-table>
          </div>
        </b-tab>
      </b-tabs>
    </div>

  </div>
</template>

<script>
import WiseService from './wise.service';
import Alert from './Alert';

export default {
  name: 'Stats',
  components: {
    Alert
  },
  data: function () {
    return {
      alertMessage: '',
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
          this.alertMessage = '';
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
          this.alertMessage = error.text ||
            `Error fetching resource stats for wise.`;
        });
    }
  }
};
</script>
