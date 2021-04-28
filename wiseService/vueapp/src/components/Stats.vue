<template>
  <div class="container-fluid">
    <Alert :initialAlert="alertMessage" variant="alert-danger" v-on:clear-initialAlert="alertMessage = ''"/>

    <div v-if="sourceStats.length">
      <b-tabs content-class="mt-3"
        :dark="getTheme ==='dark'">
        <b-tab title="Sources" active>
          <div v-if="sourceStats.length > 0">
            <b-table striped hover small borderless
              :dark="getTheme ==='dark'"
              :items="sourceStats"
              :fields="sourceTableFields">
            </b-table>
          </div>
        </b-tab>
        <b-tab title="Types">
          <div v-if="typeStats.length > 0">
            <b-table striped hover small borderless
              :dark="getTheme ==='dark'"
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

    <div v-else
      class="vertical-center info-area mt-5 pt-5">
      <div>
        <h1><b-icon-folder2-open /></h1>
        Looks like you don't have any WISE sources yet.
        <br>
        Check out our
        <a href="help#getStarted"
          class="no-decoration">
          getting started section
        </a> for help.
        <br>
        Or add a source on the
        <a href="config"
          class="no-decoration">
          Config Page</a>.
      </div>
    </div>

  </div>
</template>

<script>
import moment from 'moment-timezone';
import { mapGetters } from 'vuex';

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
  computed: {
    ...mapGetters(['getTheme'])
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
                obj.formatter = (value) => value.toString().replace(/\B(?<!\.\d*)(?=(\d{3})+(?!\d))/g, ',');
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
                obj.formatter = (value) => value.toString().replace(/\B(?<!\.\d*)(?=(\d{3})+(?!\d))/g, ',');
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
