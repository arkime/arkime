<template>
  <div class="container-fluid mb-4 row">

    <!-- navigation -->
    <div
      role="tablist"
      aria-orientation="vertical"
      class="col-xl-2 col-lg-3 col-md-3 col-sm-4 col-xs-12 no-overflow">
      <div class="nav flex-column nav-pills">
        <a
          @click="openView('general')"
          class="nav-link cursor-pointer"
          :class="{'active':visibleTab === 'general'}">
          <span class="fa fa-fw fa-cog mr-2" />
          General
        </a>
        <a
          class="nav-link cursor-pointer"
          @click="openView('integrations')"
          :class="{'active':visibleTab === 'integrations'}">
          <span class="fa fa-fw fa-key mr-2" />
          Integrations
        </a>
        <a
          class="nav-link cursor-pointer"
          @click="openView('linkgroups')"
          :class="{'active':visibleTab === 'linkgroups'}">
          <span class="fa fa-fw fa-link mr-2" />
          Link Groups
        </a>
      </div>
    </div>

    <div class="col-xl-10 col-lg-9 col-md-9 col-sm-8 col-xs-12 settings-right-panel">

      <!-- general -->
      <div v-if="visibleTab === 'general'">
        <div class="text-center">
          <b-card>
            <h1>Coming soon!</h1>
          </b-card>
        </div>
      </div> <!-- /general -->

      <!-- integrations -->
      <div v-if="visibleTab === 'integrations'">
        <div class="text-center">
          <b-card>
            <h1>Coming soon!</h1>
          </b-card>
        </div>
      </div> <!-- /integrations -->

      <!-- linkgroups -->
      <div v-if="visibleTab === 'linkgroups'">
        <h3>
          <span class="fa fa-fw fa-link mr-2"></span>
          Links Groups
        </h3>
        <p>
          Create links to pivot into different tools!
          Link groups are can be configured on the settings page and are
          displayed underneath the Cont3xt results.
        <p>
        <p class="mb-0">
          Use placeholder values in your links that will be filled in with
          the data from the Cont3xt results.
        </p>
        <dl class="dl-horizontal">
          <dt>${indicator}</dt>
          <dd>Your search query (will be refanged)</dd>
          <dt>${type}</dt>
          <dd>The search type (ip, url, etc)</dd>
          <dt>${numDays}</dt>
          <dd>The number of days defined in the "Days" input</dd>
          <dt>${numHours}</dt>
          <dd>The number of hours defined in the "Hours" input</dd>
          <dt>${stopTS}</dt>
          <dd>The stop date timestamp defined in the "Stop Date" input (YYYY-mm-ddTHH.mm.ssZ)</dd>
          <dt>${startTS}</dt>
          <dd>The start date timestamp defined in the "Start Date" input (YYYY-mm-ddTHH.mm.ssZ)</dd>
          <dt>${stopDate}</dt>
          <dd>The stop date defined in the "Stop Date" input (YYYY-mm-dd)</dd>
          <dt>${startDate}</dt>
          <dd>The start date defined in the "Start Date" input (YYYY-mm-dd)</dd>
          <dt>${stopEpoch}</dt>
          <dd>The stop date timestamp since epoch (in seconds) in the "Stop Date" input</dd>
          <dt>${startEpoch}</dt>
          <dd>The start date timestamp since epoch (in seconds) defined in the "Start Date" input</dd>
          <dt>${stopSplunk}</dt>
          <dd>The stop date timestamp defined in the "Stop Date" input (MM/DD/YYYY:HH:mm:ss)</dd>
          <dt>${startSplunk}</dt>
          <dd>The start date timestamp defined in the "Start Date" input (MM/DD/YYYY:HH:mm:ss)</dd>
        </dl>
        <p class="mt-1">
          Example:
          <pre>https://othertool.com?q=${indicator}&startTime=${startTS}</pre>
        </p>
      </div> <!-- /linkgroups -->

    </div>
  </div>
</template>

<script>
export default {
  name: 'Cont3xtHelp',
  data () {
    return {
      visibleTab: 'linkgroups'
    };
  },
  created () {
    let tab = window.location.hash;
    if (tab) { // if there is a tab specified and it's a valid tab
      tab = tab.replace(/^#/, '');
      if (tab === 'general' || tab === 'integrations' || tab === 'linkgroups') {
        this.visibleTab = tab;
      }
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    /* opens a specific settings tab */
    openView (tabName) {
      this.visibleTab = tabName;
      this.$router.push({
        hash: tabName
      });
    }
  }
};
</script>
