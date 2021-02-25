<template>
  <div class="container-fluid">
    <Alert :initialAlert="alertMessage" variant="alert-danger" v-on:clear-initialAlert="alertMessage = ''"/>

    <div>
      <b-tabs content-class="mt-3">
        <b-tab title="Sources" active>
          <div v-if="sourceStats.length > 0">
            <b-table striped hover small borderless
              :items="sourceStats"
              :fields="sourceTableFields">
            </b-table>
          </div>
        </b-tab>
        <b-tab title="Types">
          <div v-if="typeStats.length > 0">
            <b-table striped hover small borderless
              :items="typeStats"
              :fields="typeTableFields">
            </b-table>
          </div>
        </b-tab>
        <template #tabs-end>
          <li role="presentation"
            class="nav-item align-self-center startup-time">
            Started at
            <strong>{{ startTime }}</strong>
          </li>
        </template>
      </b-tabs>
    </div>

  </div>
</template>

<script>
import moment from 'moment-timezone';

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
      typeTableFields: [],
      startTime: undefined
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
          if (data && data.startTime) {
            this.startTime = moment.tz(data.startTime, Intl.DateTimeFormat().resolvedOptions().timeZone).format('YYYY/MM/DD HH:mm:ss z');
          }
          if (data && data.sources && data.sources.length > 0) {
            this.sourceStats = data.sources;
            Object.keys(this.sourceStats[0]).forEach(key => {
              const obj = { key: key, sortable: true };
              if (key !== 'source') {
                obj.formatter = (value, key, item) => value.toString().replace(/\B(?<!\.\d*)(?=(\d{3})+(?!\d))/g, ',');
                obj.tdClass = 'text-right';
                obj.thClass = 'text-right';
              }
              this.sourceTableFields.push(obj);
            });
          }
          if (data && data.types && data.types.length > 0) {
            this.typeStats = data.types;
            Object.keys(this.typeStats[0]).forEach(key => {
              const obj = { key: key, sortable: true };
              if (key !== 'type') {
                obj.formatter = (value, key, item) => value.toString().replace(/\B(?<!\.\d*)(?=(\d{3})+(?!\d))/g, ',');
                obj.tdClass = 'text-right';
                obj.thClass = 'text-right';
              }
              this.typeTableFields.push(obj);
            });
          }
        })
        .catch((error) => {
          this.alertMessage = error.text ||
            'Error fetching resource stats for wise.';
        });
    }
  }
};
</script>

<style scoped>
.startup-time {
  right: 15px;
  position: absolute;
}
</style>
