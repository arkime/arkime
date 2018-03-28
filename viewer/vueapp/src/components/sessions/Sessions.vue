<template>

  <div class="sessions-content">

    <!-- TODO fields="headers" -->
    <moloch-search
      :open-sessions="stickySessions"
      :num-visible-sessions="query.length"
      :num-matching-sessions="sessions.recordsFiltered"
      :start="query.start"
      :timezone="settings.timezone">
    </moloch-search>

    sessions go here!

  </div>

</template>

<script>
import UserService from '../UserService';
import MolochSearch from '../search/Search';

export default {
  name: 'Sessions',
  components: { MolochSearch },
  data: function () {
    return {
      user: null,
      loading: true,
      error: '',
      sessions: {}, // page data
      stickySessions: [],
      settings: {}, // user settings
      query: { // set query defaults:
        length: 50, // page length
        start: 0, // first item index
        facets: 1
      }
    };
  },
  created: function () {
    this.loadUser();
    this.loadData();
  },
  methods: {
    loadUser: function () {
      UserService.getCurrent()
        .then((response) => {
          this.settings = response;

          // TODO
          // if settings has custom sort field and the custom sort field
          // exists in the table headers, apply it
          // if (this.settings && this.settings.sortColumn !== 'last' &&
          //    this.tableState.visibleHeaders.indexOf(this.settings.sortColumn) > -1) {
          //   this.query.sorts = [[this.settings.sortColumn, this.settings.sortDirection]];
          //   this.tableState.order = this.query.sorts;
          // }
          //
          // // IMPORTANT: kicks off the initial search query
          // if (!this.settings.manualQuery || componentInitialized) { this.getData(); }
          // else {
          //   this.loading  = false;
          //   this.error    = 'Now, issue a query!';
          // }
          //
          // componentInitialized = true;
        }, (error) => {
          this.settings = { timezone: 'local' };
          this.error = error;
        });
    },
    loadData: function () {
      // TODO
      console.log('load data');
    }
  }
};
</script>

<style scoped>
.sessions-content {
  margin-top: 36px;
}
</style>
