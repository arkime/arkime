<template>
  <div class="container-fluid mb-4 row mt-3">

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
        <h3 id="dateInputs">
          <span class="fa fa-search"></span>&nbsp;
          Dates
        </h3>
        <p>
          The date fields displayed under the search bar are used to fill
          in placeholder values within
          <a @click="openView('linkgroups')" class="no-decoration cursor-pointer">links</a>.
        </p>
        <p>
          Relative dates and optional snapping are supported using the
          Splunk syntax:
        </p>
        <ul>
          <li>
            Begin the string with a plus (+) or minus (-) to indicate the offset from
            the current time.
          </li>
          <li>
            Define the time amount with a number and a unit.
            The supported time units are:
            <ul>
              <li>
                <strong>second:</strong> s, sec, secs, second, seconds
              </li>
              <li>
                <strong>minute:</strong> m, min, minute, minutes
              </li>
              <li>
                <strong>hour:</strong> h, hr, hrs, hour, hours
              </li>
              <li>
                <strong>day:</strong> d, day, days
              </li>
              <li>
                <strong>week:</strong> w, week, weeks
              </li>
              <li>
                <strong>month:</strong> mon, month, months
              </li>
              <li>
                <strong>quarter:</strong> q, qtr, qtrs, quarter, quarters
              </li>
              <li>
                <strong>year:</strong> y, yr, yrs, year, years
              </li>
            </ul>
          </li>
          <li>
            Optionally, specify a "snap to" time unit that indicates the nearest
            or latest time to which the time amount rounds down. Separate the time
            amount from the "snap to" time unit with an "@" character.
          </li>
        </ul>
      </div> <!-- /general -->

      <!-- integrations -->
      <div v-if="visibleTab === 'integrations'">
        <h3>
          <span class="fa fa-fw fa-key mr-2"></span>
          Integrations
        </h3>
        <p>
          You must use this page to configure your integations.
          Most integrations require API keys (and some require more data) in order to use them.
          Integrations are configured per user, not per Cont3xt application.
          Therefore <strong>every user must update this page for the application to work as intended.</strong>
          Alternatively, it is possible to set keys globally in the config file (<code>cont3xt.ini</code>).
          In that case a globe (<span class="fa fa-globe"></span>) will appear.
          This method is not recommended as some integrations have a maximum number of API requests
          and this could burn through them quickly if you have many users.
        </p>
        <p>
          Check the disabled checkbox to disable integrations.
          Disabled integrations will not show up in the integrations panel on the main Cont3xt page
          and no API requests will ever be issued for these integrations.
        </p>
        <p>
          The home (<span class="fa fa-home"></span>) button navigates to the home page
          of the integration (if applicable). This helps provide documentation and understanding
          for users who might not be familiar with the integration.
        </p>
        <p>
          Use the search bar to search for integrations by name.
        </p>
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
          the data from the Cont3xt results. Updating the
          <a @click="openView('general')" class="no-decoration cursor-pointer">date fields</a>
          updates the date placeholder values in the links.
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
      visibleTab: 'general'
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
      if (tabName === this.visibleTab) { return; }

      this.visibleTab = tabName;
      this.$router.push({
        hash: tabName
      });
    }
  }
};
</script>
