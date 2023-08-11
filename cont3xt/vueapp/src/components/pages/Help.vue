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
          @click="openView('overviews')"
          :class="{'active':visibleTab === 'overviews'}">
          <span class="fa fa-fw fa-file-o mr-2" />
          Overviews
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

      <!-- overviews -->
      <div v-if="visibleTab === 'overviews'">
        <!-- this page should be updated if the card format section of descriptions.txt is changed -->
        <h3>
          <span class="fa fa-fw fa-file-o mr-2"></span>
          Overviews
        </h3>

        <p>Overviews let you see all of your favorite fields in one place!</p>

        <p>You can use one of the provided system defaults or <a class="no-decoration" href="settings#overviews">create your own</a>!</p>

        <p>An <span class="text-info cursor-help" v-b-tooltip.hover.html="itypeTip">itype</span>'s selected default Overview will be the first thing displayed in the card panel when a search is executed.</p>

        <p>Change your <span class="text-info cursor-help" v-b-tooltip.hover.html="itypeTip">itype</span> defaults by pressing the <span class="fa fa-star-o text-warning"/> icons on either the <a
            class="no-decoration" href="settings#overviews"
        >Overview Settings</a> page or
          <b-dropdown disabled><template #button-content><span class="fa fa-file-o"/> Overview Selector</template></b-dropdown> during a search.
        </p>

        <p>Overview fields will appear whenever the integration and data necessary to render them is available.</p>
        <hr class="w-100">
        <h5>Configuring Existing Fields</h5>
        <p>Simply provide the integration the field is from and the label of the field.</p>

        <h5>Configuring  Custom Fields</h5>
        <p>Set the integration name and field as <code>Custom</code> to create a custom field. This will open a JSON-edit box for custom configuration... use the following format:</p>

        <h6>JSON Custom Field Format</h6>
        <ul>
          <li><strong>"label"</strong>: the field label to show the user</li>
          <li><strong>"field"</strong>: the path (it can have dots) to the data for the field, if not set the field is the same as <code>"label"</code>. For <span class="text-success">"table"</span> type this will be the path to the object array.</li>
          <li><strong class="text-secondary">"path"</strong>: alternative to field, this is the field path pre-separated into a string array (eg. <code>"field": "foo.bar"</code> is equivalent to <code>"path": ["foo", "bar"]</code>)</li>
          <li>
            <strong>"type"</strong>: specify the type of display, default is <span class="text-success">"string"</span>
            <ul>
              <li><span class="text-success">"string"</span>: display as a regular string</li>
              <li><span class="text-success">"url"</span>: a url that should be made clickable</li>
              <li><span class="text-success">"externalLink"</span>: a button that leads out to an external link</li>
              <li><span class="text-success">"table"</span>: the <code>"field"</code> property will point to an array of objects, this must have a <code>"fields"</code> property</li>
              <li><span class="text-success">"array"</span>: the <code>"field"</code> property will point to an array, display 1 item per line unless <code>"join"</code> is true</li>
              <li><span class="text-success">"json"</span>: display raw json</li>
              <li><span class="text-success">"date"</span>: a date value</li>
              <li><span class="text-success">"ms"</span>: a ms time value</li>
              <li><span class="text-success">"seconds"</span>: a second time value</li>
              <li><span class="text-success">"dnsRecords"</span>: display of non-A/AAAA dns records (for use with DNS integration; see below <i>Examples</i>)</li>
            </ul>
          </li>
          <li><strong>"fields"</strong> <b-badge variant="success">type: table</b-badge>: the list of fields to create columns for, same format as this</li>
          <li><strong>"defang"</strong>: when true defang the string, change "http" to "hXXp" and change "." to "[.]"</li>
          <li><strong>"pivot"</strong> <b-badge variant="primary">element-of: table</b-badge>: when true this field should be added to action menu for table entry that you can replace query with</li>
          <li><strong>"join"</strong> <b-badge variant="success">type: array</b-badge>: display on one line with this value as the separator (eg. <code>", "</code>)</li>
          <li><strong>"fieldRoot"</strong> <b-badge variant="success">type: table/array</b-badge>: the path (it can have dots) from each object in the array to its desired field. Effectively maps the root array of objects to an array of values/sub-objects.</li>
          <li><strong class="text-secondary">"fieldRootPath"</strong> <b-badge variant="success">type: table/array</b-badge>: alternative to fieldRoot, this is the fieldRoot path pre-separated into a string array (eg. <code>"fieldRoot": "foo.bar"</code> is equivalent to <code>"fieldRootPath": ["foo", "bar"]</code>)</li>
          <li><strong>"filterEmpty"</strong> <b-badge variant="success">type: table/array</b-badge>: removes empty (nullish & empty string/array) rows/elements when true (default is true)</li>
          <li><strong>"defaultSortField"</strong> <b-badge variant="success">type: table</b-badge>: sorts the rows by this field</li>
          <li><strong>"defaultSortDirection"</strong> <b-badge variant="success">type: table</b-badge>: with <code>"defaultSortField"</code>, sorts the rows in this direction (<code>"asc"</code> or <code>"desc"</code>)</li>
          <li><strong>"altText"</strong> <b-badge variant="success">type: externalLink</b-badge>: optional text to be display on tooltip instead of URL</li>
          <li><strong>"noSearch"</strong> <b-badge variant="primary">element-of: table</b-badge>: boolean to turn off search-ability of a column, default false (but true for <span class="text-success">"externalLink"</span> types)</li>
          <li><strong>"postProcess"</strong>: array of <span class="text-info cursor-help" v-b-tooltip.hover.html="postProcessorTip">postProcessors</span> to modify the data value</li>
        </ul>
        <h6>JSON Custom Field Shorthand</h6>
        <p>Instead of an object, you can provide a single string to be used as both the <code>"label"</code> and <code>"field"</code>.</p>

        <p class="m-0">so</p>
        <textarea
            class="w-50"
            :value="overviewShorthandExample"
            :disabled="true"
            size="sm"
            rows="1"
        />
        <p class="m-0">is equivalent to:</p>
        <textarea
            class="w-50"
            :value="JSON.stringify(overviewShorthandExampleExpanded, undefined, 2)"
            :disabled="true"
            size="sm"
            rows="4"
        />

        <hr class="w-100">
        <h6>Examples</h6>

        <p>Here are some field configurations to give you inspiration!</p>

        <div v-for="({ description, config, rows }, i) in overviewCustomConfigExamples" :key="i">
          <p class="m-0">{{ description }}</p>
          <textarea
              class="w-50"
              :value="JSON.stringify(config, undefined, 2)"
              :disabled="true"
              size="sm"
              :rows="rows"
          />
        </div>
      </div> <!-- /overviews -->

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
        </p>
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
      visibleTab: 'general',
      itypeTip: 'Indicator type (<code>domain</code>, <code>ip</code>, <code>url</code>, <code>email</code>, <code>phone</code>, <code>hash</code>, or <code>text</code>)',
      postProcessorTip: 'See <code>cont3xt/descriptions.txt</code> on the <a class="no-decoration" href="https://github.com/arkime/arkime">Arkime GitHub</a> for more info',
      overviewShorthandExample: '"phone_number"',
      overviewShorthandExampleExpanded: { label: 'phone_number', field: 'phone_number' },
      overviewCustomConfigExamples: [
        {
          description: 'DNS records',
          config: {
            label: 'DNS Records',
            type: 'dnsRecords',
            path: []
          },
          rows: 5
        },
        {
          description: 'Shodan Tags array',
          config: {
            label: 'tags',
            type: 'array',
            join: ', '
          },
          rows: 5
        },
        {
          description: 'URL Scan report link',
          config: {
            label: 'report',
            field: 'task.uuid',
            type: 'externalLink',
            postProcess: { template: 'https://urlscan.io/result/<value>' }
          },
          rows: 8
        },
        {
          description: 'Censys Services table',
          config: {
            label: 'Services',
            field: 'result.services',
            defaultSortField: 'observed_at',
            defaultSortDirection: 'asc',
            type: 'table',
            fields: [
              {
                label: 'service',
                field: 'extended_service_name'
              },
              {
                label: 'port',
                field: 'port'
              },
              {
                label: 'proto',
                field: 'transport_protocol'
              },
              {
                label: 'banner',
                field: 'banner'
              },
              {
                label: 'product',
                field: 'software',
                type: 'array',
                fieldRoot: 'uniform_resource_identifier'
              },
              {
                label: 'observed_at',
                field: 'observed_at',
                type: 'date'
              }
            ]
          },
          rows: 20
        }
      ]
    };
  },
  created () {
    let tab = window.location.hash;
    if (tab) { // if there is a tab specified and it's a valid tab
      tab = tab.replace(/^#/, '');
      if (['general', 'integrations', 'overviews', 'linkgroups'].includes(tab)) {
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
