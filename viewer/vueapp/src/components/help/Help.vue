<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="help-content">
    <!-- Side navbar -->
    <div class="help-nav">
      <template
        v-for="item in nav"
        :key="item.id">
        <a
          :href="`help#${item.id}`"
          class="help-link">
          <v-icon :icon="item.icon" />&nbsp;
          {{ item.title }}
        </a>
        <a
          v-for="child in item.children"
          :key="child.id"
          :href="`help#${child.id}`"
          class="help-link nested">
          {{ child.title }}
        </a>
      </template>
    </div> <!-- End of navbar -->

    <!-- Page content -->
    <div class="mt-2 ms-4 me-4 navbar-offset help-body">
      <!-- ABOUT -->
      <h3 id="about">
        <v-icon icon="mdi-help-circle" />&nbsp;
        About
      </h3>
      <p class="lead">
        <strong>
          Arkime is a large scale, open source, full packet capturing, indexing,
          and database system.
        </strong>
      </p>
      <p class="lead">
        Arkime augments your current security infrastructure by storing and
        indexing network traffic in standard PCAP format, while also providing
        fast indexed access. Arkime is built with an intuitive UI/UX which
        reduces the analysis time of suspected incidents.
        Arkime is not meant to replace Intrusion Detection Systems (IDS), but work with them to provide a more complete picture of network traffic.
      </p>

      <hr>

      <!-- LINKS -->
      <h3 id="links">
        <v-icon icon="mdi-link" />&nbsp;
        Links
      </h3>
      <v-row>
        <v-col
          cols="12"
          sm="12">
          <a href="https://arkime.com"><v-icon
            icon="mdi-home"
            class="mr-1" />Arkime Home</a> |
          <a href="https://arkime.com/faq"><v-icon
            icon="mdi-frequently-asked-questions"
            class="mr-1" />FAQ</a> |
          <a href="https://arkime.com/learn"><v-icon
            icon="mdi-book-open-variant"
            class="mr-1" />Docs</a> |
          <a href="https://github.com/arkime/arkime"><v-icon
            icon="mdi-github"
            class="mr-1" />GitHub</a> |
          <a href="https://slackinvite.arkime.com/"><v-icon
            icon="mdi-slack"
            class="mr-1" />Join us on Slack</a> |
          <a href="https://arkime.com/sponsor"><v-icon
            icon="mdi-heart-outline"
            class="mr-1" />Sponsor Arkime</a>
        </v-col>
      </v-row>

      <hr>

      <!-- NAVIGATION -->
      <h3 id="navigation">
        <v-icon icon="mdi-compass-outline" />&nbsp;
        Navigation
      </h3>
      <p>
        The navigation bar at the top of every page links to the Arkime, Sessions, SPI View, SPI Graph,
        Hunt, Featherprint, Files, Stats, History, Upload, and Settings pages.
        Some items only appear when you have access to them:
        Hunt needs packet search permission, Files and Stats can be hidden per user,
        and Upload only appears when the administrator has configured an upload command and you have an upload role.
      </p>
      <dl class="dl-horizontal has-icons">
        <dt><v-icon icon="mdi-help-circle" /> Help</dt>
        <dd>The owl logo and the help button open this help page at the section for the page you are on.</dd>
        <dt><v-icon icon="mdi-shield-crown-outline" /> Admin</dt>
        <dd>
          The Admin menu holds the Users, Roles, Banner, Database, Featherprint, and View Config pages.
          It only appears if you can use at least one of them.
          See the <a
            href="help#admin"
            class="no-decoration">Admin section</a>.
        </dd>
        <dt>Version</dt>
        <dd>Hover to see build information and to check whether a newer Arkime release is available.</dd>
        <dt>Language</dt>
        <dd>The flag menu changes the language of the user interface. The choice is saved in this browser.</dd>
        <dt><v-icon icon="mdi-information" /> App Info</dt>
        <dd>Colored by the health of the database. Hover to see your user and the database version and status.</dd>
        <dt><v-icon icon="mdi-chevron-up-circle" /> Toolbars</dt>
        <dd>Hides (or shows) the search bar, toolbars, graph, and map on the current page to give you more room.</dd>
        <dt>Footer</dt>
        <dd>Shows how long the slowest query of the last search or refresh took. Click it to see the time of each query.</dd>
        <dt><v-icon icon="mdi-lightbulb-on-outline" /> Help Notes</dt>
        <dd>
          A card in the bottom left corner with tips about the page you are on and links to this page.
          Use the arrows to cycle through the tips.
          <v-icon icon="mdi-close" /> puts them away until the next page load,
          "Dismiss" never shows that message again, and "Dismiss all" never shows any of them again.
        </dd>
      </dl>

      <hr>

      <!-- SEARCH BAR -->
      <h3 id="search">
        <v-icon icon="mdi-magnify" />&nbsp;
        Search Bar
      </h3>
      <div>
        <p>
          The Arkime, Sessions, SPI View, SPI Graph, and Hunt pages share a search bar at the top of the page.
          Arkime uses a simple query language for building searches, described below.
          Other pages have their own filter boxes.
        </p>
        <h6 id="searchControls">
          <v-icon icon="mdi-magnify" />&nbsp;
          Search Bar Controls
        </h6>
        <dl class="dl-horizontal has-icons">
          <dt><v-icon icon="mdi-magnify" /> Search expression</dt>
          <dd>
            Type your query and press Enter or click "Search".
            As you type, suggestions appear for field names (the fields you used recently are listed first, marked <v-icon icon="mdi-history" />),
            operators, values from the database (after two characters), <code>EXISTS!</code>, country codes, view names, and <code>$shortcuts</code>.
            Up and Down move through the suggestions, Tab or Enter picks one, and Esc closes the list.
          </dd>
          <dt><v-icon icon="mdi-arrow-expand" /> Expand</dt>
          <dd>Opens a large text area for editing a long search expression.</dd>
          <dt><v-icon icon="mdi-content-save" /> Save</dt>
          <dd>Saves the search expression as a new <a href="help#views">view</a>.</dd>
          <dt><v-icon icon="mdi-close" /> Clear</dt>
          <dd>Clears the search expression.</dd>
          <dt><v-icon icon="mdi-clock-outline" /> Time range</dt>
          <dd>Opens the time controls. See <a href="help#timebounding">Time Range</a>.</dd>
          <dt><v-icon icon="mdi-database" /> Clusters</dt>
          <dd>Only in a multi-viewer. Picks which clusters to search. The Hunt page can only search one cluster at a time.</dd>
          <dt><v-icon icon="mdi-eye" /> Views</dt>
          <dd>Applies, creates, and edits views. See <a href="help#views">Views</a>.</dd>
          <dt><v-icon icon="mdi-menu-down" /> Actions</dt>
          <dd>Sessions page only. Exports, tags, removes, or sends sessions. See <a href="help#actions">Actions</a>.</dd>
          <dt><v-icon icon="mdi-cog" /> Visualizations</dt>
          <dd>
            Sessions and SPI View only.
            "Pin graph and map" keeps the graph and map (and on Sessions, the column headers) at the top of the page while you scroll.
            "Hide graph and map" stops fetching them, which speeds up large queries.
            If the time range is longer than your cluster fetches graphs for by default, the button becomes "Fetch Viz Data",
            which fetches the graph for this query, for this browser session, or always in this browser.
          </dd>
        </dl>
        <h6 id="basic">
          <v-icon icon="mdi-magnify" />&nbsp;
          Basic Query
        </h6>
        <p>
          Searches are done using field names, operators, and values.
          This is sometimes also called a search expression. (e.g. <code>ip.src == 1.2.3.4</code>)
        </p>
        <dl class="dl-horizontal">
          <dt>Fields</dt>
          <dd>
            See <a
              href="help#fields"
              class="no-decoration">table below</a> for list of all fields and operators supported.
          </dd>
          <dt>Grouping</dt>
          <dd>You can use parentheses to group search terms (e.g. <code>field1==value1 &amp;&amp; (field2==value2 || field3==value3)</code>).</dd>
          <dt>Logical Operators</dt>
          <dd>Combine search terms using AND (&amp;&amp;) and OR (||). Negate a term or group with <code>!</code> (e.g. <code>!(ip.dst == 10.0.0.0/8)</code>).</dd>
          <dt>OR List Queries</dt>
          <dd>Search for ANY of the listed values in a field using square brackets and comma-separated values (e.g., <code>field==[value1,value2,value3]</code>).</dd>
          <dt>AND List Queries</dt>
          <dd>Search for ALL of the listed values in a field using reversed square brackets (e.g., <code>field==]value1,value2,value3[</code>).</dd>
        </dl>
      </div>
      <div>
        <h6 id="timebounding">
          <v-icon icon="mdi-clock-outline" />&nbsp;
          Time Range
        </h6>
        <p>
          All queries require a start and stop time.
          Click the time button ( <v-icon icon="mdi-clock-outline" /> ), which shows the current range such as "Last 1 hour", to open the time controls:
        </p>
        <dl class="dl-horizontal">
          <dt>Presets</dt>
          <dd>
            Relative ranges from 15 minutes to 1 year, plus "All (careful)".
            Picking one searches right away, and a relative range is recalculated for every new search.
            Presets longer than your query time limit are not shown.
          </dd>
          <dt>Start / End</dt>
          <dd>
            Enter an exact time as <code>YYYY/MM/DD HH:mm:ss</code>, or use the calendar and time pickers.
            ISO 8601 times and a trailing time zone (<code>Z</code>, <code>+02:00</code>, <code>EDT</code>, <code>America/New_York</code>) are accepted.
            The <v-icon icon="mdi-skip-previous" /> and <v-icon icon="mdi-skip-next" /> buttons jump to the beginning or end of a day.
            Exact times take effect when you click "Search".
          </dd>
          <dt>Bounding</dt>
          <dd>How sessions are matched against the time range, see below.</dd>
          <dt>Interval</dt>
          <dd>
            How much time each bar or point in the graphs represents: Auto, Seconds, Minutes, Hours, Days, or Weeks.
            Auto picks the bucket size based on the time range.
          </dd>
        </dl>
        <p>
          Since every session has a first packet, last packet, and database timestamp, Arkime offers
          a choice on how it matches sessions:
        </p>
        <dl class="dl-horizontal">
          <dt>First Packet</dt>
          <dd>The timestamp of the first packet received for the session.</dd>
          <dt>Last Packet</dt>
          <dd>The timestamp of the last packet received for the session. This is the default.</dd>
          <dt>Bounded</dt>
          <dd>Both the first and last packet timestamps for the session must be inside the time window.</dd>
          <dt>Session Overlaps</dt>
          <dd>The timestamp of the first packet must be before the end of the time window AND the timestamp of the last packet must be after the start of the time window.</dd>
          <dt>Database</dt>
          <dd>The timestamp the session was written to the database.  This can be up to several minutes AFTER the last packet was received.</dd>
        </dl>
      </div>
      <div>
        <h6 id="stringSearch">
          <v-icon icon="mdi-magnify" />&nbsp;
          String Search
        </h6>
        <p>
          In Arkime, string fields are special since they can be searched in several different ways.
          When fields are indexed, their case may or may not be normalized, which is documented in the
          <a
            href="help#fields"
            class="no-decoration">fields table below</a>.
          The types of string searches are:
        </p>
        <dl class="dl-horizontal">
          <dt>Wildcard</dt>
          <dd>
            If a <code>*</code> appears in an expression, it is assumed a wildcard
            match is being used. Supported wildcards are <code>*</code>, which
            matches any character sequence (including the empty one), and
            <code>?</code>, which matches any single character. The wildcard query
            is run against the full text strings, after case normalization if
            enabled for the field. For example
            <code>http.uri == "www.f*k.com"</code> will capture an http.uri string
            which contains either www.fork.com or www.frack.com.
          </dd>
          <dt>Regex</dt>
          <dd>
            A regex query must be surrounded by forward slashes and will always be
            anchored. This means you will almost always want to include a leading
            and trailing <code>.*</code> within your regex query. The regex query
            is run against the full text strings, after case normalization (if enabled) for the field.
            For example <code>http.uri == /.*www\.f.*k\.com.*/</code>. It uses the
            Lucene regex implementation which doesn't support most PCRE features.
            Check out the ES <a href="https://www.elastic.co/guide/en/elasticsearch/reference/current/regexp-syntax.html">Regexp Syntax</a> guide.
          </dd>
          <dt>OR Lists</dt>
          <dd>
            In Arkime, OR Lists are used as a shorthand method for doing multiple OR queries.
            A list containing wildcard or regex strings will process as wildcard/regexes.
            For example instead of <code>(protocols == http || protocols == s*)</code> use <code>protocols == [http,s*]</code>.
            This query will search for any sessions where the protocols field contains either the exact 'http' value OR values starting with 's'.
            Using an OR List many times can be faster for the system to process than using <code>||</code>.
          </dd>
          <dt>AND Lists</dt>
          <dd>
            In Arkime, AND Lists are used as a shorthand method for doing multiple AND queries.
            A list containing wildcard or regex strings will process as wildcard/regexes.
            For example instead of <code>(protocols == http &amp;&amp; protocols == s*)</code> use <code>protocols == ]http,s*[</code>.
            This query will search for any sessions where the protocols field contains both the exact 'http' value AND a value starting with 's'.
            Using an AND List many times can be faster for the system to process than using <code>&amp;&amp;</code>.
          </dd>
        </dl>
        <h6 id="ipSearch">
          <v-icon icon="mdi-magnify" />&nbsp;
          IP Search
        </h6>
        <p>
          IP searching is very flexible and can be performed using the full IP
          address, a partial IP address, or CIDR representation. For fields
          that include a port number, it is possible to follow any of the IP
          representations with a colon (ip4) or dot (ip6) and then the port number to further refine a query. Ports are also
          first class searchable and may be searched for directly. For example:
          <code>ip == 1.2.3.0/24:80</code>. This query will search for all sessions which contain an IP address within the 1.2.3.0/24 CIDR range as well as utilizing port 80 during the session.
          An IP search can also be done with a list of IPs which may be in mixed representations, both OR Lists and AND Lists are supported: <code>ip == [1.2.3.4,5.6.7.8,1.3.0.0/16]</code>
          If you only want to find ipv4 or ipv6 traffic, you can search using those tokens: <code>ip.src == ipv6</code>
        </p>
        <h6 id="numericSearch">
          <v-icon icon="mdi-magnify" />&nbsp;
          Numeric Search
        </h6>
        <p>
          Numeric fields support simple range operators besides the default equals
          and not equals query types. For example, to show events with bytes transferred being less than 10000,
          use this query: <code>bytes &lt;= 10000</code>.
          Numeric fields also support both OR Lists and AND Lists. For example, <code>port == [80,443,23]</code>
        </p>
        <h6 id="dateSearch">
          <v-icon icon="mdi-magnify" />&nbsp;
          Date Search
        </h6>
        <p>
          Date fields support simple range operators besides the default equals
          and not equals. For example: <code>starttime == "2004/07/31 05:33:41"</code>.
          They also support both OR Lists and AND Lists for a simple OR/AND queries. For example:
          <code>stoptime == ["2004/07/31 05:33:41","2004/07/31 06:33:41"]</code>.
          However, usually it's much easier to use the
          <a
            href="help#timebounding"
            class="no-decoration">time range</a>
          controls in the search bar.
          <strong>IMPORTANT</strong>, using starttime or stoptime does <strong>NOT</strong>
          change the overall <a
            href="help#timebounding"
            class="no-decoration">time range</a>
          of the query.
          Finally, relative dates and optional snapping are supported using the
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
        <h6 id="fieldExistsSearch">
          <v-icon icon="mdi-magnify" />&nbsp;
          Field Exists Search
        </h6>
        <p>
          It is possible to check if a field has been set or not in the session by
          using the special comparison value of <code>field == EXISTS!</code> OR negated: <code>field != EXISTS!</code>.
          For example, to verify that a certificate doesn't have an issuer common name, but does
          have an issuer organizational name, the following query could be used:
          <code>cert.issuer.cn != EXISTS! &amp;&amp; cert.issuer.on == EXISTS!</code>
        </p>
        <h6 id="shortcutSearch">
          <v-icon icon="mdi-magnify" />&nbsp;
          Shortcuts
        </h6>
        <p>
          A shortcut is a named list of values, such as a list of IPs, that you can use in place of a value in a search:
          <code>ip.src == $MY_IPS</code>. Type <code>$</code> in the search bar to see the shortcuts you can use.
          A shortcut that doesn't exist is an error.
          Shortcuts are created and shared in <a href="help#settings">Settings</a>.
        </p>
        <h6 id="examples">
          <v-icon icon="mdi-magnify" />&nbsp;
          Examples
        </h6>
        <div class="mt-2">
          Find all the sessions involving Russia (RU) or China (CN) that are
          using port 80 and also a hostname which contains "com":
          <pre>(country == RU || country == CN) &amp;&amp; port == 80 &amp;&amp; host == *com</pre>
        </div>
        <div class="mt-2">
          Find all the sessions of type "text/plain", involving Canada (CA), and
          containing less than 20 packets:
          <pre>tags == "http:content:text/plain" &amp;&amp; country == CA &amp;&amp; packets &lt; 20</pre>
        </div>
      </div>

      <hr>

      <!-- ARKIME (DASHBOARDS) -->
      <h3 id="arkime">
        <v-icon icon="mdi-view-dashboard" />&nbsp;
        Arkime
      </h3>
      <p>
        The Arkime page is a dashboard: a grid of widgets that each chart the current search and time range.
        Every widget searches the search bar expression, plus the dashboard filter, plus the widget's own filter or view.
        To open the viewer on this page instead of Sessions, set Start Page to Arkime in
        <a href="help#settings">Settings</a>.
      </p>
      <p>
        <em>
          <strong>Note:</strong>
          Dashboards can only search a limited amount of time at once (7 days unless your administrator changed
          <code>dashboardMaxQueryDays</code>). With a longer time range, or All, the widgets don't load and a warning explains why.
        </em>
      </p>
      <div>
        <h6 id="dashboards">
          <v-icon icon="mdi-view-dashboard" />&nbsp;
          Dashboards
        </h6>
        <p>
          Arkime ships with built-in dashboards: Overview, Connections, one for each protocol,
          and Certificates, Databases, Files, Hosts, Multicast, OT, Routing, Security, Software, and Tunnels in the Analysis folder.
          Plugins such as Suricata and Zeek Intel add their own once their fields exist.
          Built-in dashboards can't be changed, but you can save a copy and change that.
          Administrators can add more by putting exported <code>*.dashboard.json</code> files in the <code>dashboardsDir</code> directory.
        </p>
        <p>
          The page opens the dashboard named in the URL (<code>?dashboard=&lt;id&gt;</code>),
          otherwise your default dashboard, otherwise Overview.
          Star a dashboard in the dashboards menu to make it your default.
        </p>
        <dl class="dl-horizontal has-icons">
          <dt><v-icon icon="mdi-palette" /> Palette</dt>
          <dd>Picks the color palette used by the charts.</dd>
          <dt><v-icon icon="mdi-plus" /> Add widget</dt>
          <dd>Adds a widget and opens the widget editor.</dd>
          <dt><v-icon icon="mdi-content-save" /> Dashboards</dt>
          <dd>
            Lists the built-in and saved dashboards (grouped into folders) and loads one when clicked.
            From here you can also start a new dashboard, save the current one (or save it as a new dashboard),
            import or export a dashboard as JSON, export every chart as a PNG, save the dashboard as a PDF using the browser's print dialog,
            set your default (<v-icon icon="mdi-star-outline" />), and edit or delete your own dashboards.
            Dashboards can be shared with roles and users, each with view or edit access, and organized in folders.
          </dd>
          <dt><v-icon icon="mdi-cog" /> Settings</dt>
          <dd>
            Dashboard settings. The dashboard filter is a search expression added to every widget on this dashboard.
            When it is set, a <v-icon icon="mdi-filter-outline" /> chip shows it.
          </dd>
          <dt><v-icon icon="mdi-tune-variant" /> Controls</dt>
          <dd>Shows the values picked in the dashboard's filter controls. Its <v-icon icon="mdi-close" /> clears them.</dd>
          <dt><v-icon icon="mdi-cancel" /> Cancel</dt>
          <dd>Cancels loading. Widgets that didn't finish can then be reloaded with "Retry all failed".</dd>
          <dt><v-icon icon="mdi-refresh-auto" /> Auto refresh</dt>
          <dd>Reloads every widget on a timer, from 30 seconds to 1 hour. It is skipped while the page is hidden.</dd>
        </dl>
        <p>
          Your dashboards, and the ones shared with you, can also be renamed, re-shared, reordered, and deleted in
          <a href="help#settings">Settings</a>.
        </p>
        <h6 id="widgets">
          <v-icon icon="mdi-view-dashboard" />&nbsp;
          Widgets
        </h6>
        <ul>
          <li>Drag a widget by <v-icon icon="mdi-drag-vertical" /> to move it, and drag its right or bottom edge to resize it (1 to 4 columns wide, 1 to 8 rows tall).</li>
          <li>Click a widget's title to open the same graph in SPI Graph, or the same sessions on the Sessions page.</li>
          <li><v-icon icon="mdi-information-outline" /> shows how the widget is configured.</li>
          <li>
            The <v-icon icon="mdi-dots-vertical" /> menu edits the widget, opens it in SPI Graph or Sessions, saves it as an
            <a href="help#savedGraphs">SPI Graph saved graph</a>, downloads it as a PNG or CSV, shows how long its query took, or removes it.
          </li>
          <li>Click a value in a chart for the <a href="help#valueMenu">value menu</a>. Drag across a timeline to change the time range.</li>
        </ul>
        <p>
          The chart widgets are the same as the <a href="help#spigraphTypes">SPI Graph graph types</a>
          (in the widget editor, Donut is called Pie Chart, and Table is called Unique or Intersection).
          These widgets are only on dashboards:
        </p>
        <dl class="dl-horizontal dl-horizontal-wide">
          <dt>Map</dt>
          <dd>The world map, with countries shaded by sessions or any metric.</dd>
          <dt>Metric</dt>
          <dd>One big number for the search, or one per top value of a field, with an optional trend line and comparison to the previous day or week.</dd>
          <dt>Gauge</dt>
          <dd>A metric drawn as a gauge, with warning and critical levels.</dd>
          <dt>Sessions List</dt>
          <dd>The matching sessions as a compact table, with your choice of columns, sort, and number of rows.</dd>
          <dt>Capture Statistics</dt>
          <dd>The capture nodes' current statistics.</dd>
          <dt>Time Information</dt>
          <dd>The search's time range and bounding.</dd>
          <dt>Markdown</dt>
          <dd>Notes, with headings, bold, italic, code, lists, and links.</dd>
          <dt>Dashboards</dt>
          <dd>Links to other dashboards.</dd>
          <dt>Filter control</dt>
          <dd>Lets you pick values to filter the dashboard by, see below.</dd>
        </dl>
        <p>
          The widget editor sets the widget's title, view (type), field or fields, and, depending on the type,
          the metric and how it is aggregated (sum, average, min, max, unique count, median, or the 90th, 95th, or 99th percentile),
          a second field to split by, stacked or grouped bars, orientation, linear, log, or percent axes,
          a comparison to the previous day or week, warning and critical levels, numeric buckets, how many values to show and in what order,
          the widget size, and a filter or view of its own. It can also start from an SPI Graph saved graph.
        </p>
        <h6 id="dashboardVariables">
          <v-icon icon="mdi-tune-variant" />&nbsp;
          Filter Controls &amp; Variables
        </h6>
        <p>
          A filter control widget is a list of a field's top values (with their session counts), a range slider of a numeric field,
          or a text box (<code>*</code> wildcards allowed).
          The values you pick filter every widget, or only set a variable.
          Use a variable as <code>${name}</code> in a widget's title or filter, or in the dashboard filter, and it is replaced with the picked values.
          With nothing picked, a title shows "All" and the part of a filter that uses the variable is left out.
          The picks are kept in the URL, so a filtered dashboard can be shared as a link.
          Clicking a value in a chart can also offer "Filter the dashboard by this".
        </p>
      </div>

      <hr>

      <!-- SESSIONS -->
      <h3 id="sessions">
        <v-icon icon="mdi-swap-horizontal" />&nbsp;
        Sessions
      </h3>
      <p>
        The Sessions page within Arkime is where an analyst will find the bulk of the details regarding
        the sessions being investigated. Enter a search in the <a href="help#search">search bar</a>,
        then open a session to see its metadata and packets.
      </p>
      <div>
        <h6>
          <v-icon icon="mdi-swap-horizontal" />&nbsp;
          Paging
        </h6>
        <p>
          The paging controls under the search bar move through the sessions returned, 10 to 1,000 per page (50 by default).
          Hover over the "Showing ... entries" text to see how many sessions were searched in total.
        </p>
        <h6 id="views">
          <v-icon icon="mdi-eye" />&nbsp;
          Views
        </h6>
        <p>
          A "View" is a search expression that gets added to existing queries.
          It limits the session set and is analogous to a database view.
          Views should be used for common search queries that an analyst regularly finds themselves running.
          A view can be shared with other users and roles.
        </p>
        <p>
          A user can save the current Sessions column configuration alongside a view.
          If a view is saved with a column configuration, when the view is applied,
          the Sessions table columns are updated as well.
        </p>
        <p>
          Additionally, a user can easily save a search expression as a view by clicking the save
          button ( <v-icon icon="mdi-content-save" /> ) to the right of the search expression input.
        </p>
        <div class="mt-2">
          To view your views, click the eyeball button
          ( <v-icon icon="mdi-eye" /> ). This menu allows an analyst to:
          <ul>
            <li>
              Apply a view by clicking its name. This overlays the view onto the current query and issues a search.
              If the Session's table columns were saved with the view, the table columns are updated.
              Click "None" to stop using a view. Shared views are marked with <v-icon icon="mdi-share-all" />.
            </li>
            <li>
              Click "New View" ( <v-icon icon="mdi-plus-circle" /> ) to add the current query as a new view.
            </li>
            <li>
              Update a view by clicking the edit button.
              ( <v-icon icon="mdi-pencil-box" /> )
              Only the view's creator or an admin can edit or delete it.
            </li>
            <li>
              Delete a view by clicking the delete button.
              ( <v-icon icon="mdi-trash-can-outline" /> )
            </li>
            <li>
              Put the view's search expression into the search expression input
              by clicking the apply expression button. ( <v-icon
                icon="mdi-share"
                class="mdi-flip-h" /> )
              Note: this does not issue a search.
            </li>
            <li>
              Apply this view's column configuration to the sessions table by clicking
              the apply column button. ( <v-icon icon="mdi-view-column" /> )
              Note: this will issue a search and update the sessions table columns.
            </li>
          </ul>
        </div>
        <h6 id="actions">
          <v-icon icon="mdi-menu-down" />&nbsp;
          Actions
        </h6>
        <div>
          The actions ( <v-icon icon="mdi-menu-down" /> ) button next to the search bar contains:
          <ol>
            <li>Export PCAP or Export PCAPNG of the sessions' packets. PCAPNG keeps every interface and link type; a PCAP that would mix link types is refused.</li>
            <li>Export CSV of the sessions' fields.</li>
            <li>Add Tags or Remove Tags. Tagged sessions can later be found with <code>tags == "blah"</code>.</li>
            <li>Remove Data, which scrubs the packets and/or deletes the SPI data (if you have data removal privileges).</li>
            <li>Send the sessions to another Arkime cluster for further analysis (one item per configured cluster).</li>
            <li>Export Intersection, which counts the unique combinations of several fields and opens the results in a new tab.</li>
            <li>Create Periodic Query, which opens the periodic query section of the Settings page in a new tab.</li>
          </ol>
          The export, tag, remove, and send actions can be applied to the sessions which have been opened,
          the sessions visible on the current page, or all the sessions matching the search.
          They can also include the other segments of long sessions that were split into linked segments.
        </div>
        <h6 id="visualizations">
          <v-icon icon="mdi-chart-bar" />&nbsp;
          Visualizations
        </h6>
        <div>
          The Sessions and SPI View pages show a timeline graph of the search above the results.
          Hover over the graph to show its controls:
          <ul>
            <li>
              View by Sessions or by the Timeline Data Filters picked in <a href="help#settings">Settings</a>
              (Packets, Bytes, and Databytes by default).
              Packets, Bytes, and Databytes show source and destination in different colors.
            </li>
            <li>Lines or Bars, on a Linear or Log scale.</li>
            <li>
              Click and drag over the graph to drill into that time frame.
              Note: this issues a new query.
            </li>
            <li>
              <v-icon icon="mdi-magnify-plus" /> and <v-icon icon="mdi-magnify-minus" /> zoom in and out,
              and <v-icon icon="mdi-chevron-left" /> and <v-icon icon="mdi-chevron-right" /> move backwards and forwards in time
              by the amount picked in the dropdown. Note: these issue a new query.
            </li>
            <li>
              "Cap Restarts" draws a dashed line each time a capture node started.
              Use this indicator to avoid incorrect assumptions about sessions:
              when a capture process restarts there is a loss of protocol state
              awareness and segment counts are restarted.
            </li>
            <li>"Spanning" counts each session in every time bucket it spans, instead of only one.</li>
            <li>A dashed line marks the current time, and business hours are shaded if your administrator configured them.</li>
          </ul>
          Click <v-icon icon="mdi-earth" /> to open the world map of the sessions' countries.
          Toggle source (S), destination (D), and XFF countries, expand it with <v-icon icon="mdi-arrow-expand" /> to see the top 10,
          and zoom with the buttons or the mouse wheel. Click a country to add it to the search.
        </div>
        <h6 id="sessionsTable">
          <v-icon icon="mdi-table" />&nbsp;
          Sessions Table
        </h6>
        <p>
          The table options button ( <v-icon icon="mdi-table-cog" /> ) in the first column header can:
        </p>
        <ul>
          <li>Open all the visible sessions (up to 50), or close all open sessions.</li>
          <li>Fit the table to the window.</li>
          <li>Toggle visible columns ( <v-icon icon="mdi-view-column" /> ), with a search box for finding fields.</li>
          <li>
            Save the columns as a column layout ( <v-icon icon="mdi-content-save-plus" /> ), update the loaded layout,
            or load a saved layout ( <v-icon icon="mdi-folder-open" /> ), including layouts shared with you
            ( <v-icon icon="mdi-account-multiple" /> ) and "Arkime Default".
            Layouts are edited and shared in <a href="help#settings">Settings</a>.
          </li>
        </ul>
        <p>
          <em>
            <strong>Example:</strong>
            An analyst investigating suspicious IRC sessions can show only the Start Time, Src IP, Dst IP, Dst Port, IRC Channel, and IRC Nickname columns,
            save that as a layout, and load it again for the next investigation.
          </em>
        </p>
        <p>
          The Info column, always last, shows a few interesting fields for each session.
          Its own <v-icon icon="mdi-table-cog" /> button picks, saves, and loads which fields it shows.
        </p>
        <div class="mt-2">
          Hover over a column header and click the column menu ( <v-icon icon="mdi-menu-down" /> ) button to:
          <ul>
            <li>Hide the column</li>
            <li>Export the column's unique values, with or without counts (and with ports, for IP fields)</li>
            <li>Open the field in <a href="help#spigraph">SPI Graph</a></li>
            <li>Add <code>field == EXISTS!</code> to the search</li>
            <li>Pivot on the field, which opens a new Sessions tab searching for all the values on this page</li>
          </ul>
        </div>
        <p>
          Click a column header to sort by it, and shift-click to add (or remove) a secondary sort.
          Drag a column header to move the column, and drag its edge to resize it.
        </p>
        <p>
          While sessions are open, the <v-icon icon="mdi-chevron-double-left" /> tab on the right side of the page
          lists them. Click one to scroll to it.
        </p>
        <h6 id="sessionDetail">
          <v-icon icon="mdi-plus" />&nbsp;
          Session Detail
        </h6>
        <p>
          Click the plus button to the left of a session to open it.
          The session detail has up to three tabs; which one opens first is set in <a href="help#settings">Settings</a>.
        </p>
        <dl class="dl-horizontal has-icons">
          <dt><v-icon icon="mdi-information-outline" /> Details</dt>
          <dd>
            Every field of the session, in cards for General and then each protocol.
            Click a card's title to collapse it, and use the <v-icon icon="mdi-view-column" /> button to show one, two, or three columns.
            The label menu next to each field exports its unique values, opens it in SPI Graph,
            toggles it as a column or in the Info column, or adds <code>EXISTS!</code> to the search.
          </dd>
          <dt><v-icon icon="mdi-package-variant-closed" /> Packets</dt>
          <dd>
            The session's packets. Pick how many packets to show and the format (natural, ascii, utf8, or hex),
            and show or hide the source and destination packets.
            The Packet Options menu shows raw or reassembled packets, packet info (timestamps), and line numbers (hex),
            uncompresses data, shows images and files (be cautious with this if investigating sensitive images),
            applies decodings (UnXOR, Unbase64), and opens the packets in CyberChef.
            Hidden if you aren't allowed to see PCAP.
          </dd>
          <dt><v-icon icon="mdi-magnify-scan" /> Shark</dt>
          <dd>
            A Wireshark style dissection of the packets, with a packet list, protocol tree, and hex bytes in resizable panes.
            Only shown when the viewer has tshark installed.
          </dd>
        </dl>
        <p>
          Each tab has a find box ( <v-icon icon="mdi-magnify" /> ). In Packets, it searches the packet content as ascii, hex, or regex.
          The toolbar above the tabs can:
        </p>
        <ul>
          <li><v-icon icon="mdi-link" /> copy, open, or pivot to a permalink of the session</li>
          <li><v-icon icon="mdi-download" /> download the session as PCAP or PCAPNG, and for segmented sessions, the segment or the entire session</li>
          <li><v-icon icon="mdi-arrow-up-circle" /> <v-icon icon="mdi-arrow-down-circle" /> download the raw source or destination data</li>
          <li><v-icon icon="mdi-source-branch" /> search for all the segments of a long session</li>
          <li><v-icon icon="mdi-cog" /> export, tag, remove, or send this session</li>
        </ul>
        <h6 id="valueMenu">
          <v-icon icon="mdi-menu-down" />&nbsp;
          Value Menus
        </h6>
        <p>
          Click a field value almost anywhere in Arkime (the sessions table, session detail, SPI View, SPI Graph, Featherprint) to open its menu:
        </p>
        <ul>
          <li><strong>and</strong>, <strong>and not</strong>, <strong>or</strong>, or <strong>or not</strong> the value into the search (and with the port, for IP fields)</li>
          <li>Open Sessions, New Sessions Tab, or New Sessions Tab with only this value</li>
          <li>Copy Value</li>
          <li>Any value actions your administrator configured, such as Featherprint Lookup for IPs</li>
        </ul>
        <p>Clicking a start or stop time sets the search's start or stop time.</p>
      </div>

      <hr>

      <!-- SPI VIEW -->
      <h3 id="spiview">
        <v-icon icon="mdi-eyedropper" />&nbsp;
        SPI View
      </h3>
      <p>
        The SPI (Session Profile Information) View is used to dive into specific metrics of a session which an analyst wishes to investigate further.
        It shows the top values of the fields you pick, with their counts, so you can scan what's in your data at a glance.
        Instead of manually writing a query, an analyst can click a value to add it to the search with the <a href="help#valueMenu">value menu</a>.
        As an example, to see all the BASIC authorization headers recorded over the current time window, open the http category and enable the
        http.authorization field. The search can then be updated to include a specific authorization string,
        or a wildcard to see all of a certain type of authorization header (Basic *, BEARER *, etc).
      </p>
      <ul>
        <li>
          Fields are grouped in categories. Click a category's title to open or close it.
          Each category lists its protocols with counts, and "Load All" and "Unload All" show or hide all its fields.
        </li>
        <li>
          Click a field's name in the category to show or hide it, or search for fields in the category.
          <v-icon icon="mdi-chevron-double-down" /> shows all the category's fields.
        </li>
        <li>
          Each shown field lists its values with counts. Click "more" or "less" to show 100 more or fewer values.
          The field's menu hides it, exports its unique values, opens it in SPI Graph, or pivots on it
          (a new Sessions tab searching for all its listed values).
        </li>
        <li>The <a href="help#visualizations">timeline graph and map</a> work the same as on the Sessions page.</li>
      </ul>
      <p>
        <strong>Tip:</strong>
        The analyst can save or load the fields that they have displayed/hidden on the SPI View page by clicking the save/load field configuration
        ( <v-icon icon="mdi-view-column" /> ) button. These layouts can be shared and edited in <a href="help#settings">Settings</a>.
      </p>

      <hr>

      <!-- SPI GRAPH -->
      <h3 id="spigraph">
        <v-icon icon="mdi-chart-bar" />&nbsp;
        SPI Graph
      </h3>
      <p>
        The SPI Graph page charts the top values of any field. This page is very useful for both at a glance views of activity,
        as well as deep dive analysis. For example, to chart the http.user values seen in the current time window,
        type http.user in the SPI Graph field box.
        Pick a graph type, and the Connections, Tree, and Circle graph types show who talks to whom.
      </p>
      <div>
        <h6>
          <v-icon icon="mdi-chart-bar" />&nbsp;
          Toolbar
        </h6>
        <dl class="dl-horizontal has-icons">
          <dt>Graph Type</dt>
          <dd>The type of graph, see <a href="help#spigraphTypes">below</a>.</dd>
          <dt><v-icon icon="mdi-content-save" /> Saved graphs</dt>
          <dd>The <a href="help#savedGraphs">saved graphs</a> menu.</dd>
          <dt><v-icon icon="mdi-cog" /> Settings</dt>
          <dd>Every setting of the graph, see <a href="help#spigraphSettings">Graph Settings</a>.</dd>
          <dt>SPI Graph</dt>
          <dd>The field to graph. The default is set in <a href="help#settings">Settings</a>.</dd>
          <dt>Max Elements</dt>
          <dd>How many of the field's top values to show. Increase it to see more values of a noisy field.</dd>
          <dt>Metric</dt>
          <dd>Sessions, or one of your Timeline Data Filters, for the types that support it.</dd>
          <dt>Add another field</dt>
          <dd>
            Bar, Donut, Waffle, Table, Treemap, Sankey, Matrix, and Mosaic can graph more than one field.
            Each added field shows as a chip that can be dragged to reorder or <v-icon icon="mdi-close" /> removed.
          </dd>
          <dt><v-icon icon="mdi-download" /> Export CSV</dt>
          <dd>Exports the data as a CSV file, for the types that support it.</dd>
        </dl>
        <h6 id="spigraphTypes">
          <v-icon icon="mdi-chart-bar" />&nbsp;
          Graph Types
        </h6>
        <dl class="dl-horizontal dl-horizontal-wide">
          <dt>Timeline</dt>
          <dd>
            A timeline for each top value. The Layout option shows one timeline (and map) per value,
            or a single graph with the values stacked, as percents, or overlaid. Drag across it to change the time range.
          </dd>
          <dt>Area</dt>
          <dd>The values' timelines as filled areas, stacked, as percents, or overlaid.</dd>
          <dt>Time of day</dt>
          <dd>A week of hours colored by the metric, for the whole search and for each top value, to spot off hours traffic.</dd>
          <dt>Heatmap</dt>
          <dd>One row per value and one column per time bucket, shaded by the metric. Hover a cell for its count.</dd>
          <dt>Bar</dt>
          <dd>One bar per value. A second field splits each bar into stacked or grouped segments. Bars can be horizontal.</dd>
          <dt>Combo</dt>
          <dd>The top values as bars of one metric with a line of another metric on its own axis (sessions and bytes, say).</dd>
          <dt>Scatter</dt>
          <dd>The top values placed by two metrics and sized by an optional third, to spot values that stand apart.</dd>
          <dt>Donut</dt>
          <dd>
            Nested rings, one per field, or split into several small pies, one per value of the first field.
            "Show Other" adds a slice for everything outside the top values. Scroll to zoom and drag to pan.
          </dd>
          <dt>Tag cloud</dt>
          <dd>The field's values sized by count. Click a word to add it to the search.</dd>
          <dt>Waffle</dt>
          <dd>100 squares, each 1% of the sessions, colored by the top values. A second field draws one waffle per value.</dd>
          <dt>Table</dt>
          <dd>
            The values and counts. With several fields, Intersection lists their combinations and Unique lists each field's top values side by side.
            Click a header to sort, and hide or show columns.
          </dd>
          <dt>Treemap</dt>
          <dd>Nested boxes sized by count, one level per field.</dd>
          <dt>Sankey</dt>
          <dd>Flows from each field's values to the next field's values.</dd>
          <dt>Matrix</dt>
          <dd>One field's values down the side and a second field's across the top, each cell shaded by the sessions they share.</dd>
          <dt>Mosaic</dt>
          <dd>The first field's values as columns as wide as their share of the sessions, each split by the second field's values.</dd>
          <dt>Connections</dt>
          <dd>A force graph of source and destination values, see <a href="help#connections">Connections</a>.</dd>
          <dt>Tree</dt>
          <dd>Who reached what from one host, hop by hop.</dd>
          <dt>Circle</dt>
          <dd>Hosts on a ring with an arc between each pair that talked, colored by protocol or a field you pick.</dd>
        </dl>
        <p>
          Donut, Table, Treemap, Sankey, Matrix, Mosaic, and split Bar and Waffle graphs count sessions only.
          Hover over a value for a popup with its count and its <a href="help#valueMenu">value menu</a>;
          Esc or <v-icon icon="mdi-close" /> closes it.
        </p>
        <h6 id="spigraphSettings">
          <v-icon icon="mdi-cog" />&nbsp;
          Graph Settings
        </h6>
        <p>
          The <v-icon icon="mdi-cog" /> button opens every setting of the graph, including the ones that don't have a control in the toolbar.
          These are the same settings a dashboard widget has, such as how the metric is aggregated
          (sum, average, min, max, unique count, median, or a percentile), warning and critical levels, orientation, axis scale,
          numeric buckets, and order.
          For Timeline, Area, Time of day, Heatmap, Combo, and Scatter it also has "Sort by" (count or alphabetically)
          and "Refresh every", which reloads the graph every few seconds.
          Click Apply to redraw the graph.
        </p>
        <h6 id="savedGraphs">
          <v-icon icon="mdi-content-save" />&nbsp;
          Saved Graphs
        </h6>
        <p>
          The <v-icon icon="mdi-content-save" /> menu saves the current graph (its type, fields, options, search, and view),
          updates the loaded graph, or loads a saved graph, including graphs shared with you ( <v-icon icon="mdi-share-all" /> ).
          Saved graphs can be shared with roles and users and organized in folders.
          "Add current graph to dashboard" adds it to an <a href="help#arkime">Arkime dashboard</a> as a widget.
          Your saved graphs can also be managed in <a href="help#settings">Settings</a>.
        </p>
        <h6 id="connections">
          <v-icon icon="mdi-sitemap" />&nbsp;
          Connections, Tree &amp; Circle
        </h6>
        <p>
          The Connections graph type (which replaced the old Connections page) draws a graph between source and destination values of your choosing.
          Relationships may be visually determined using this method.
          For example, set Src to ip.src and Dst to ip.dst:port to see which source IPs talk to which destination IP and port combinations.
          Nodes that are only a source, only a destination, or both are colored differently, as the Src, Dst, and "Src &amp; dst" chips show.
          The Tree and Circle graph types draw the same connections differently.
        </p>
        <p>
          <em>
            <strong>Note:</strong>
            The default Src and Dst fields may be set in the general settings (Connections Src and Connections Dst).
          </em>
        </p>
        <dl class="dl-horizontal has-icons">
          <dt>Query size</dt>
          <dd>How many connections to fetch.</dd>
          <dt>Min connections</dt>
          <dd>The minimum number of sessions between two nodes for them to be shown.</dd>
          <dt>Weight</dt>
          <dd>What sizes the nodes and links: Sessions, Packets, Total Raw Bytes, Total Data Bytes, or None.</dd>
          <dt><v-icon icon="mdi-circle-outline" /> Popup fields</dt>
          <dd>
            <v-icon icon="mdi-circle-outline" /> picks the fields shown in the node popups,
            and <v-icon icon="mdi-link" /> the fields shown in the link popups.
          </dd>
          <dt>Root &amp; Depth</dt>
          <dd>Tree only. The host the tree starts from (the busiest source by default), and how many hops to follow. Click a node to make it the root.</dd>
          <dt>Max shown</dt>
          <dd>Circle only. How many of the busiest hosts are on the ring. Hover a host or a color in the legend to light up only its arcs.</dd>
        </dl>
        <ul>
          <li>
            Hover over a node or link for a popup with its sessions, links, and the fields you picked.
            The popup can hide the node or link until the graph is redrawn.
            Esc or <v-icon icon="mdi-close" /> closes it.
          </li>
          <li>Connections only: drag a node to lock it in place, and click <v-icon icon="mdi-lock-open" /> to unlock all nodes.</li>
          <li>Connections only: <v-icon icon="mdi-plus-network" /> and <v-icon icon="mdi-minus-network" /> increase or decrease the distance between nodes.</li>
          <li><v-icon icon="mdi-format-font-size-increase" /> and <v-icon icon="mdi-format-font-size-decrease" /> change the size of the node labels.</li>
          <li>Scroll, or click <v-icon icon="mdi-magnify-plus" /> and <v-icon icon="mdi-magnify-minus" />, to zoom in and out.</li>
          <li><v-icon icon="mdi-download" /> downloads a PNG of the graph.</li>
        </ul>
        <div class="mt-2">
          Set a "Baseline" time range to show changes in the network by
          highlighting new or old connections appearing in and disappearing from the network.
          You'll see indications next to each node label and in node popups to describe the state of each node:
          <ul>
            <li>New nodes ( ✨) - in actual but not baseline results.</li>
            <li>Old nodes ( 🚫) - only in baseline results.</li>
            <li>Both nodes - in both actual and baseline results.</li>
          </ul>
        </div>
        <div class="mt-2">
          Once you have selected a time range as your baseline, "Baseline Visibility" shows:
          <ul>
            <li>All - all nodes</li>
            <li>Actual - nodes present in the "current" timeframe query results</li>
            <li>Baseline - nodes present in the "baseline" timeframe query results</li>
            <li>New only - nodes present in the "current" but NOT the "baseline" timeframe</li>
            <li>Baseline only - nodes present in the "baseline" but NOT the "current" timeframe</li>
          </ul>
        </div>
      </div>

      <hr>

      <!-- HUNT -->
      <h3 id="hunt">
        <v-icon icon="mdi-eye" />&nbsp;
        Hunt
      </h3>
      <p>
        The Hunt page allows users to search within session packets for text, to find what indexing missed.
        To perform a hunt, users should start by narrowing down their search on the Sessions page.
        Then, switch to the Hunt tab where any new hunt will only search within the sessions searched for earlier.
        Underneath the search bar, the Hunt page will display the number of sessions that will be searched.
        To begin your hunt, click on the "Create a packet search job" button on the top right of the page (under the search bar).
        The form that is presented is described below:
      </p>
      <dl class="dl-horizontal dl-horizontal-wide">
        <dt>Name</dt>
        <dd>The name of the hunt (multiple hunts can have the same name)</dd>
        <dt>Max number of packets to examine per session</dt>
        <dd>The maximum number of packets that the hunt will search within each session</dd>
        <dt>Notifiers</dt>
        <dd>Optional notifiers to fire when there is an error, or there are matches (every 10 minutes), or when the hunt is complete.</dd>
        <dt>Description</dt>
        <dd>The description of the hunt (useful when sharing)</dd>
        <dt>Search</dt>
        <dd>The text to search for, as ascii, case sensitive ascii, hex, safe regex, or safe hex regex (RE2 syntax)</dd>
        <dt>Search src/dst packets</dt>
        <dd>Whether to search source or destination packets, or both. Must select at least one.</dd>
        <dt>Search raw/reassembled packets</dt>
        <dd>Whether to search raw or reassembled packets</dd>
        <dt>Share with roles</dt>
        <dd>The roles that can view the results.</dd>
        <dt>Users</dt>
        <dd>A comma separated list of users to be added to the hunt so they can view the results.</dd>
      </dl>
      <p>
        Once the hunt has been created, it will be added to the <strong>hunt job queue</strong>. Hunts run one at a time and
        can be paused so that another hunt can run. After a hunt has completed, it will be added to
        the <strong>hunt job history</strong>. Here, you can open the sessions that matched the hunt,
        rerun it on the current search ( <v-icon icon="mdi-refresh" /> ) or repeat it over its original time range ( <v-icon icon="mdi-repeat" /> ),
        change its description and who it is shared with, or delete it from the history.
        You can also cancel a hunt. This simply stops the packet searching and moves the hunt to the hunt history.
      </p>
      <p>
        Hunts add <code>huntName</code> and <code>huntId</code> fields to the sessions as they match.
        You can search for these fields in the sessions search bar at any time. Deleting a hunt from the history
        does not remove these fields from the sessions. If there are matches and you have remove privileges, you
        can remove these fields by clicking the <v-icon icon="mdi-close" /> button.
      </p>
      <p>
        <strong>Info:</strong> A normal user can only view/pause/delete their own hunts (and see the results of hunts shared with them), but an admin can view/pause/delete all hunts.<br>
        <strong>Warning:</strong> The packet search will take a long time and possibly slow down the viewer if you search many sessions.
        Users will be alerted if they are trying to search for more than 100,000 sessions. Normal users cannot search more than 1,000,000
        sessions, and admins cannot search more than 10,000,000 sessions (these values can be overwritten in the config).
      </p>

      <hr>

      <!-- FEATHERPRINT -->
      <h3 id="featherprint">
        <v-icon icon="mdi-fingerprint" />&nbsp;
        Featherprint
      </h3>
      <p>
        Featherprint is passive device fingerprinting. It builds a record for each IP from the ARP, DHCP, DNS/mDNS, NBNS, and SSDP
        metadata Arkime already captures: the device's MAC address (and previous MACs), host names, advertised services,
        DHCP and SSDP attributes, first and last seen times, and a classification (printer, chromecast, windows_pc, and so on).
        The viewer keeps the records up to date for the subnets your administrator configured, records a history of changes,
        and raises alerts on new or changed devices.
      </p>
      <dl class="dl-horizontal has-icons">
        <dt><v-icon icon="mdi-format-list-bulleted" /> Tracked IPs</dt>
        <dd>
          The devices in the watched subnets, with their IP, MAC, name, classification, and last seen time.
          Filter by IP, MAC, name, or classification, click a header to sort,
          and expand a row to see the device's details and change history.
          Click an IP, MAC, or name for its <a href="help#valueMenu">value menu</a>.
          This list is empty if no subnets are configured.
        </dd>
        <dt><v-icon icon="mdi-magnify" /> Lookup</dt>
        <dd>
          Fingerprints any IP on demand from the sessions in the time range you pick (15 minutes to 30 days).
          A lookup is not saved and doesn't raise alerts.
          Every IP's <a href="help#valueMenu">value menu</a> has a "Featherprint Lookup" item that opens this.
        </dd>
        <dt><v-icon icon="mdi-alert-circle-outline" /> Alerts</dt>
        <dd>
          New or changed devices, such as a new IP or MAC, a changed MAC, name, or service.
          Show open, acknowledged, or all alerts, and click an alert's IP to see the device.
          If you are allowed to, Ack ( <v-icon icon="mdi-check" /> ) or Unack ( <v-icon icon="mdi-undo-variant" /> )
          an alert, or all the listed alerts at once. The acknowledgement is shared by everyone.
        </dd>
      </dl>
      <p>
        "Refresh Every" reloads the page on a timer, and hovering <v-icon icon="mdi-progress-clock" /> shows how far the
        background scan has gotten. Tracked IPs and Alerts need the database upgraded to version 89; Lookup works without it.
        See <a href="help#featherprintadmin">Featherprint admin</a> for how it is configured.
      </p>

      <hr>

      <!-- FILES -->
      <h3 id="files">
        <v-icon icon="mdi-file-multiple-outline" />&nbsp;
        Files
      </h3>
      <p>
        The Files page shows a table view of the PCAP files which have been written.
        By default it shows the file number, node, name, whether the file is locked, the first date, and the file size.
        Use the column menu to add more columns, such as the last date, packets, sessions, encoding, and compression.
        Search the files by name.
      </p>

      <hr>

      <!-- STATS -->
      <h3 id="stats">
        <v-icon icon="mdi-chart-line" />&nbsp;
        Stats
      </h3>
      <p>
        There are several tabs that contain statistics about your capture nodes and database and are enumerated below.
        The database tabs depend on which database your cluster uses.
      </p>
      <div>
        <h6>
          <v-icon icon="mdi-chart-areaspline" />&nbsp;
          Capture Graphs
        </h6>
        <p>
          The Capture Graphs tab displays real time graphs representing what the capture node is doing.
          Larger values are overplotted in successively darker colors.
          Hover over the graph to see what each capture node is capturing at any specific time.
        </p>
        <p>
          <strong>Tip:</strong>
          These graphs are 1440 pixels wide (4 hours of data at 10 second intervals).
          Expand your browser window to at least 1500 pixels wide for best viewing or you will be shown a horizontal scroll bar (ew).
        </p>
        <h6>
          <v-icon icon="mdi-speedometer" />&nbsp;
          Capture Statistics
        </h6>
        <p>
          The Capture Statistics tab displays information for each capture node,
          either as a dashboard of node cards ( <v-icon icon="mdi-view-dashboard-outline" /> ) or as a table ( <v-icon icon="mdi-table" /> ).
          In the table, use the column config drop down to change which columns are shown.
        </p>
        <dl class="dl-horizontal">
          <dt>Node</dt>
          <dd>The name of the capture node</dd>
          <dt>Time</dt>
          <dd>The time reported by the capture node</dd>
          <dt>Sessions</dt>
          <dd>The number of sessions currently being monitored in memory</dd>
          <dt>Free Space</dt>
          <dd>Free space across all configured disks</dd>
          <dt>CPU</dt>
          <dd>CPU percentage that Arkime is using</dd>
          <dt>Memory</dt>
          <dd>Amount of memory that Arkime is using</dd>
          <dt>Packet Q</dt>
          <dd>Number of packets that are waiting to be processed</dd>
          <dt>Disk Q</dt>
          <dd>Number of blocks of data that are waiting to be written to disk</dd>
          <dt>ES Q</dt>
          <dd>Number of database requests that are waiting to be sent</dd>
          <dt>Packet/s</dt>
          <dd>The number of packets that we've received that aren't corrupt that we try to add to a packetQ per second</dd>
          <dt>Bytes/s</dt>
          <dd>The size of all the packets that we've received that aren't corrupt that we try to add to a packetQ per second</dd>
          <dt>Sessions/s</dt>
          <dd>Number of sessions sent to the database per second</dd>
          <dt>Packet Drops/s</dt>
          <dd>Number of dropped packets as reported by the OS or network card (Arkime never sees these) per second</dd>
          <dt>Bits/Sec</dt>
          <dd>Same as Bytes/s but in bits per second</dd>
          <dt>Written Bytes/s</dt>
          <dd>The size of all the packets that Arkime is going to write to disk per second</dd>
          <dt>Unwritten Bytes/s</dt>
          <dd>The size of all the packets that Arkime isn't going to write to disk, but that we processed, per second</dd>
          <dt>Active TCP/UDP/ICMP/SCTP/ESP Sessions</dt>
          <dd>Number of sessions of each protocol Arkime is currently monitoring</dd>
          <dt>Used Space</dt>
          <dd>Space used across all configured disks</dd>
          <dt>ES Health Response MS</dt>
          <dd>How long the database health check took</dd>
          <dt>Closing Q</dt>
          <dd>Number of TCP sessions that have received a FIN and Arkime is waiting to see if it actually closed</dd>
          <dt>Waiting Q</dt>
          <dd>Number of sessions that are ready to be written but are waiting on an asynchronous request (wise, plugins) to finish</dd>
          <dt>Active Fragments</dt>
          <dd>Number of packets that are waiting on remaining IP fragments to show up</dd>
          <dt>Fragments Dropped/Sec</dt>
          <dd>Number of packets that were dropped because frag overload or timeouts</dd>
          <dt>Total Dropped/Sec</dt>
          <dd>Sum of the inputs dropped and overload metrics</dd>
          <dt>ES Session Bytes/Sec</dt>
          <dd>The size of the session documents sent to the database per second</dd>
          <dt>Overload Drops/s</dt>
          <dd>Number of packets dropped because there was no packet queue that was free to process them on</dd>
          <dt>Dup Drops/s</dt>
          <dd>Number of duplicate packets dropped per second</dd>
          <dt>ES Drops/s</dt>
          <dd>Number of database requests that were dropped because of queue overflow per second</dd>
          <dt>ES Session Size/Sec</dt>
          <dd>The average size of the session documents sent to the database</dd>
          <dt>Retention</dt>
          <dd>How far back the node's oldest PCAP goes</dd>
          <dt>Start Time / Running Time</dt>
          <dd>When the capture process started, and how long it has been running</dd>
          <dt>Arkime Version</dt>
          <dd>The version of Arkime the capture node is running</dd>
        </dl>
        <p>
          <em>
            <strong>Tip:</strong>
            Click the <v-icon icon="mdi-plus" /> button to display graphs about the table information.
          </em>
        </p>
        <h6>
          <v-icon icon="mdi-server" />&nbsp;
          ES Nodes
        </h6>
        <p>
          The ES Nodes tab displays information for each OpenSearch/Elasticsearch node, as a dashboard or a table.
          In the table, use the column config drop down to change which columns are shown.
          A <code>dbAdmin</code> can exclude or include a node or its IP from shard allocation using the row menu.
        </p>
        <dl class="dl-horizontal">
          <dt>Name</dt>
          <dd>The name of the ES node</dd>
          <dt>Documents</dt>
          <dd>The total number of documents that this node has ingested</dd>
          <dt>Disk Used</dt>
          <dd>The disk used for OpenSearch/Elasticsearch store</dd>
          <dt>Disk Free</dt>
          <dd>The disk free for OpenSearch/Elasticsearch store</dd>
          <dt>Heap Size</dt>
          <dd>JVM heap used in bytes</dd>
          <dt>OS Load</dt>
          <dd>Currently calculated average load of the system</dd>
          <dt>CPU</dt>
          <dd>CPU usage in percent</dd>
          <dt>Read/s</dt>
          <dd>The number of bytes read across all devices used by OpenSearch/Elasticsearch per second</dd>
          <dt>Write/s</dt>
          <dd>The number of bytes written across all devices used by OpenSearch/Elasticsearch per second</dd>
          <dt>Search/s</dt>
          <dd>Current query phase operations per second</dd>
          <dt>Scrolls/s</dt>
          <dd>Scroll operations per second</dd>
          <dt>IP</dt>
          <dd>IP of the node</dd>
          <dt>IP Excluded</dt>
          <dd>Is the node currently excluded by IP</dd>
          <dt>Node Excluded</dt>
          <dd>Is the node currently excluded by node name</dd>
          <dt>Non Heap Size</dt>
          <dd>Memory being used that isn't on the heap</dd>
          <dt>Search Time</dt>
          <dd>Time spent searching</dd>
          <dt>Write Tasks</dt>
          <dd>Completed and rejected write tasks, and the write queue limit</dd>
          <dt>Hot/Warm &amp; Zone</dt>
          <dd>The node's hot/warm and zone attributes</dd>
          <dt>Shards &amp; Segments</dt>
          <dd>Number of shards and segments on the node</dd>
          <dt>Uptime &amp; Version</dt>
          <dd>How long the node has been running, and its version</dd>
        </dl>
        <h6>
          <v-icon icon="mdi-database" />&nbsp;
          ES Indices
        </h6>
        <p>
          The ES Indices tab displays a table containing information for each OpenSearch/Elasticsearch index.
          Please use the column config drop down to change which fields are shown.
          A <code>dbAdmin</code> can delete (with data removal privileges), optimize, close or open, and shrink an index using the row menu.
        </p>
        <dl class="dl-horizontal">
          <dt>Name</dt>
          <dd>The name of the index</dd>
          <dt>Documents</dt>
          <dd>The number of documents in the index</dd>
          <dt>Disk Size</dt>
          <dd>The size of the OpenSearch/Elasticsearch store for this index</dd>
          <dt>Shards</dt>
          <dd>Number of shards created for this index</dd>
          <dt>Segments</dt>
          <dd>Number of segments for this index</dd>
          <dt>Replicas</dt>
          <dd>Number of replicas for the primary shard in this index</dd>
          <dt>Memory</dt>
          <dd>How much memory is used for this index</dd>
          <dt>Health</dt>
          <dd>The health of the index (green, yellow, or red)</dd>
          <dt>Status</dt>
          <dd>Whether an index is open or closed</dd>
          <dt>Created Date</dt>
          <dd>When the index was created</dd>
          <dt>Current Query Phase Ops</dt>
          <dd>Searches currently running on the index's primary shards</dd>
          <dt>UUID</dt>
          <dd>The internal UUID for the index</dd>
          <dt>Hot/Warm</dt>
          <dd>Whether the index is on hot or warm nodes</dd>
          <dt>Shards/Node</dt>
          <dd>The maximum number of the index's shards allowed on one node</dd>
          <dt>ES Version</dt>
          <dd>The version the index was created with</dd>
          <dt>Disk Per Doc</dt>
          <dd>The average disk used per document</dd>
          <dt>Codec</dt>
          <dd>The index's compression codec</dd>
        </dl>
        <h6>
          <v-icon icon="mdi-format-list-checks" />&nbsp;
          ES Tasks
        </h6>
        <p>
          The ES Task tab displays a table containing information for each OpenSearch/Elasticsearch task.
          Please use the column config drop down to change which fields are shown.
        </p>
        <dl class="dl-horizontal">
          <dt>Action</dt>
          <dd>The action that the task is performing</dd>
          <dt>Description</dt>
          <dd>More detail about the action that is being performed</dd>
          <dt>Start Time</dt>
          <dd>The time that the task was initiated (the task is removed from this list upon completion)</dd>
          <dt>Running Time</dt>
          <dd>The amount of time that the task has taken</dd>
          <dt>Children</dt>
          <dd>The number of child tasks associated with this task</dd>
          <dt>User</dt>
          <dd>The Arkime user that started the task</dd>
          <dt>Cancellable</dt>
          <dd>Can the task be stopped</dd>
          <dt>ID</dt>
          <dd>The task id</dd>
          <dt>Type</dt>
          <dd>The type of task</dd>
        </dl>
        <p>
          <em>
            <strong>Tip:</strong>
            A <code>dbAdmin</code> can cancel a task by clicking the
            <v-icon icon="mdi-delete" /> button, or every cancellable task with "Cancel All Tasks".
            This is useful when you run a query that is taking longer than intended.
          </em>
        </p>
        <h6>
          <v-icon icon="mdi-sitemap" />&nbsp;
          ES Shards
        </h6>
        <p>
          The ES Shards tab displays a matrix containing the ES indices and nodes.
          Each cell describes the number of shards that that index has in that particular node.
          <span class="arkime-badge arkime-badge--grey">Gray</span> means it's an alternate shard and the
          <span
            class="arkime-badge"
            :style="{backgroundColor: 'rgb(var(--v-theme-primary))', color: 'rgb(var(--v-theme-button-fg))'}">other color</span> means it's a primary shard.
          Hover over a cell to get more information.
          A <code>dbAdmin</code> can explain why a shard isn't allocated, or delete unassigned shards.
        </p>
        <h6>
          <v-icon icon="mdi-lifebuoy" />&nbsp;
          ES Recovery
        </h6>
        <p>
          The ES Recovery tab displays a table containing information for each OpenSearch/Elasticsearch index
          recovery.  By default it only shows information about indices still waiting to be recovered.
          It is a simple view of the <code>/_cat/recovery</code> OpenSearch/Elasticsearch API.
        </p>
        <dl class="dl-horizontal">
          <dt>Index</dt>
          <dd>The index name</dd>
          <dt>Shard</dt>
          <dd>The shard number</dd>
          <dt>Time</dt>
          <dd>The recovery time</dd>
          <dt>Type</dt>
          <dd>The recovery type</dd>
          <dt>Stage</dt>
          <dd>The recovery stage</dd>
          <dt>Src Host</dt>
          <dd>The source host</dd>
          <dt>Src Node</dt>
          <dd>The source node name</dd>
          <dt>Dst Host</dt>
          <dd>The target host</dd>
          <dt>Dst Node</dt>
          <dd>The target node name</dd>
          <dt>Files</dt>
          <dd>The number of files to recover</dd>
          <dt>Files Recovered</dt>
          <dd>The files recovered</dd>
          <dt>Files %</dt>
          <dd>The percent of files recovered</dd>
          <dt>Files total</dt>
          <dd>The total number of files</dd>
          <dt>Bytes</dt>
          <dd>The number of bytes to recover</dd>
          <dt>Bytes Recovered</dt>
          <dd>The bytes recovered</dd>
          <dt>Bytes %</dt>
          <dd>The percent of bytes recovered</dd>
          <dt>Bytes total</dt>
          <dd>The total number of bytes</dd>
          <dt>Translog</dt>
          <dd>The number of translog ops to recover</dd>
          <dt>Translog Recovered</dt>
          <dd>The translog ops recovered</dd>
          <dt>Translog %</dt>
          <dd>The percent of translog ops recovered</dd>
        </dl>
        <h6>
          <v-icon icon="mdi-database" />&nbsp;
          ClickHouse &amp; PostgreSQL
        </h6>
        <p>
          When sessions are stored in ClickHouse, the CH Queries and CH Partitions tabs show the running queries and the
          size, rows, and memory of each partition. When the cluster or users database is PostgreSQL, the PG Queries and PG Tables
          tabs show the same for its queries and tables.
          A <code>dbAdmin</code> can kill a query, optimize or drop (with data removal privileges) a partition, and vacuum a table.
        </p>
      </div>

      <hr>

      <!-- HISTORY -->
      <h3 id="history">
        <v-icon icon="mdi-history" />&nbsp;
        History
      </h3>
      <p>
        The History page provides the ability to view Arkime actions and queries.
        It is usable both as a history for a user and for auditing abilities
        for an admin. A non-admin user can only view their own actions. An admin
        user can view all users' actions.
      </p>
      <dl class="dl-horizontal has-icons">
        <dt><v-icon icon="mdi-magnify" /> Search</dt>
        <dd>
          Search for specific history items
          (<code>*</code> and <code>?</code> wildcards are supported).
        </dd>
        <dt><v-icon icon="mdi-clock-outline" /> Time range</dt>
        <dd>Filter history by a time range using the time controls under the search bar.</dd>
        <dt><v-icon icon="mdi-account-circle" /> All / My</dt>
        <dd>Admins can switch between seeing everyone's history and only their own.</dd>
        <dt><v-icon icon="mdi-sort" /> Sort</dt>
        <dd>Sort history by clicking any column header.</dd>
        <dt><v-icon icon="mdi-checkbox-marked" /> Exists</dt>
        <dd>The checkboxes in the Expression and View column headers only show history items that have a value for that field.</dd>
        <dt><v-icon icon="mdi-plus" /> Expand</dt>
        <dd>Displays more information about a history item.</dd>
        <dt><v-icon icon="mdi-folder-open" /> Open</dt>
        <dd>
          "Goes to" the history item, opening the page that the action/query was issued from.
          This is only available for the main GET requests from a page within the application.
        </dd>
        <dt><v-icon icon="mdi-trash-can-outline" /> Delete</dt>
        <dd>Admins with data removal privileges can delete history items.</dd>
      </dl>

      <hr>

      <!-- UPLOAD -->
      <h3 id="upload">
        <v-icon icon="mdi-tray-arrow-up" />&nbsp;
        Upload
      </h3>
      <p>
        The Upload page uploads a PCAP file to be processed by Arkime, with optional comma separated tags added to its sessions.
        It only appears when your administrator has configured an <code>uploadCommand</code> and you have one of the
        <code>uploadRoles</code> (arkimeUser by default).
      </p>

      <hr>

      <!-- SETTINGS -->
      <h3 id="settings">
        <v-icon icon="mdi-cog" />&nbsp;
        Settings
      </h3>
      <p>
        The Settings page allows for general user based settings to be managed and is separated into different tabs.
        An admin can update the settings of any user.
      </p>
      <div>
        <h6>
          <v-icon icon="mdi-cog" />&nbsp;
          General
        </h6>
        <div>
          Here, a user can:
          <ul>
            <li>Manage their timezone format (local, local with time zone, or UTC) and whether to show milliseconds</li>
            <li>Set their start page (Arkime or Sessions)</li>
            <li>Set which session detail tab opens first (Details, Packets, Shark, or the last one used)</li>
            <li>Set their default session detail packets format</li>
            <li>Set their default number of packets returned</li>
            <li>Set the default for showing or hiding packet info (timestamps)</li>
            <li>Manage whether to issue a query on Sessions page load</li>
            <li>Set their default Sessions table sort</li>
            <li>Set their default SPI Graph field</li>
            <li>Set their default Connections source and destination fields, used by the SPI Graph connections graphs</li>
            <li>Pick up to 4 Timeline Data Filters, the metrics offered by the timeline graphs and SPI Graph</li>
            <li>Hide tags from the sessions</li>
          </ul>
        </div>
        <h6>
          <v-icon icon="mdi-view-dashboard" />&nbsp;
          Arkime Dashboards
        </h6>
        <p>
          Here, a user can rename, share, reorder the widgets of, set as default, and delete the
          <a href="help#arkime">dashboards</a> they created or that were shared with them.
          The widgets themselves are edited on the Arkime page.
        </p>
        <h6>
          <v-icon icon="mdi-view-column" />&nbsp;
          Column, Info Field &amp; SPI View Layouts
        </h6>
        <p>
          Here, a user can edit, share, and delete their saved Sessions table column layouts, Info column field layouts,
          and SPI View field layouts, including the field order (and the sort, for column layouts).
        </p>
        <h6>
          <v-icon icon="mdi-chart-bar" />&nbsp;
          SPI Graphs
        </h6>
        <p>
          Here, a user can open, edit, share, and delete the <a href="help#savedGraphs">graphs saved on the SPI Graph page</a>.
        </p>
        <h6>
          <v-icon icon="mdi-brush" />&nbsp;
          Themes
        </h6>
        <p>
          Here, a user can select from several predefined user interface themes, and a logo.
          The theme is saved to your account and applies across all the Arkime apps.
          If they're feeling adventurous and maybe a little dangerous, they can create their own custom theme and share it.
          <br>
          <em>
            <strong>Note:</strong>
            Be careful with this feature, it is easy to make the UI completely unusable.
          </em>
        </p>
        <h6>
          <v-icon icon="mdi-lock" />&nbsp;
          Password
        </h6>
        <p>
          Here, a user can update their password.
        </p>
        <h6>
          <v-icon icon="mdi-two-factor-authentication" />&nbsp;
          Two-Factor Authentication
        </h6>
        <p>
          Here, an admin can turn on two-factor authentication for their account by scanning a QR code
          with an authenticator app, or turn it off with a code.
        </p>
        <h6>
          <v-icon icon="mdi-eye" />&nbsp;
          Views
        </h6>
        <p>
          Here, a user can manage saved views by updating, deleting, copying, sharing them, or transferring them to another user.
          A user can also create a new view in this section.
          See the <a
            href="help#views"
            class="no-decoration">Views</a> section for more information.
        </p>
        <h6>
          <v-icon icon="mdi-format-list-bulleted" />&nbsp;
          Shortcuts
        </h6>
        <p>
          Here, a user can create, update, and delete shortcuts.
          Shortcuts are lists of IPs, strings, or numbers that can be used in search queries.
          For example, create a list of IPs and use them in a query
          expression <code>ip.src == $MY_IPS</code>. Users can share
          shortcuts with specific users by listing each user's userId.
          Users can share shortcuts with multiple users by sharing to
          roles. For example, sharing with the "arkimeUser" role will share
          the shortcut with any user that has access to Arkime.
          <br>
          <strong>Tip:</strong> Use <code>$</code> to autocomplete shortcuts
          in search expressions.
          <br>
          <strong>Note:</strong> If your clusters are configured with usersElasticsearch
          and cronQueries, shortcuts will sync across clusters and be available
          to use on all clusters that have access to the usersElasticsearch.
          <a
            href="https://arkime.com/settings#userselasticsearch"
            class="no-decoration">
            See settings for more information.
          </a>
          The shortcuts can take up to one minute to sync to all clusters.
        </p>
        <h6>
          <v-icon icon="mdi-magnify" />&nbsp;
          Periodic Queries
        </h6>
        <p>
          Here, a user can manage their queries by updating, copying, transferring, or deleting them.
          They can also create a new query.
          <br>
          Periodic Queries can tag the sessions that match, or tag them and send them to another cluster,
          and notify notifiers. The query runs against sessions delayed by 90 seconds to make sure all
          updates have been completed for that session.
        </p>
        <h6>
          <v-icon icon="mdi-bell" />&nbsp;
          Notifiers
        </h6>
        <p>
          Here, an <strong>admin</strong> user can create, update, test, share, and delete notifiers.
          The available notifiers are Slack, Microsoft Teams, Email, Twilio, Pushover, Syslog, and SNMP.
          Notifiers can be used to alert users that periodic queries have found matching sessions or that hunts have matches.
          Notifiers alert every <strong>10</strong> minutes if there are <strong>new</strong> matches.
        </p>
      </div>

      <hr>

      <!-- ADMIN -->
      <h3 id="admin">
        <v-icon icon="mdi-shield-crown-outline" />&nbsp;
        Admin
      </h3>
      <p>
        The <v-icon icon="mdi-shield-crown-outline" /> Admin menu on the right side of the navigation bar holds the admin pages.
        Each page is only listed for the users that can use it, and the menu is hidden if you can't use any of them.
      </p>
      <div>
        <h6 id="users">
          <v-icon icon="mdi-account-multiple" />&nbsp;
          Users
        </h6>
        <p>
          The Users page is where a <code>usersAdmin</code> can add, modify, and delete both Users and Roles for the entire Arkime ecosystem.
          Arkime has builtin Roles that control how different subsystems are accessed.
          You can also create user defined Roles that are used to share different items.
          Basic inheritance is supported for role, so if you create a 'team' role and assign it the cont3xtUser role,
          any user assigned the new 'team' role will also have the 'cont3xtUser' role assigned.
          A user defined role can name Role Assigners, who can add and remove users from that role on the Roles page.
        </p>

        <p>System Roles:</p>
        <dl class="dl-horizontal dl-horizontal-wide">
          <dt>superAdmin</dt>
          <dd>Has all system roles assigned. Can only be assigned by another superAdmin. Users with superAdmin assigned are the only ones that can assign Admin roles to other users.</dd>
          <dt>usersAdmin</dt>
          <dd>Can use the Users page to add/modify/delete users, except superAdmin users. Users with usersAdmin assigned can unassign Admin roles, but not assign them to other users.</dd>
          <dt>arkimeAdmin</dt>
          <dd>Can perform Arkime configuration, view data for other Arkime users, and automatically assigned the arkimeUser role.</dd>
          <dt>arkimeUser</dt>
          <dd>Can use the Arkime viewer application</dd>
          <dt>cont3xtAdmin</dt>
          <dd>Can perform cont3xt configuration, view data for other Cont3xt users, and automatically assigned the cont3xtUser role.</dd>
          <dt>cont3xtUser</dt>
          <dd>Can use the Cont3xt application</dd>
          <dt>parliamentAdmin</dt>
          <dd>Can perform parliament configuration, automatically assigned the parliamentUser role.</dd>
          <dt>parliamentUser</dt>
          <dd>Can dismiss parliament issues and notifications. Unlike other roles parliament doesn't require a role to be set to use as a dashboard</dd>
          <dt>wiseAdmin</dt>
          <dd>Can perform wise configuration, automatically assigned the wiseUser.</dd>
          <dt>wiseUser</dt>
          <dd>Can use the WISE UI to do queries and view stats</dd>
          <dt>dbAdmin</dt>
          <dd>Can use the Database admin page and the ES index, shard, and task actions. Pair it with arkimeUser, which the viewer requires to let anyone in at all.</dd>
          <dt>mcpUser</dt>
          <dd>Can use the MCP (Model Context Protocol) endpoint, along with the app's own user role.</dd>
        </dl>

        <p>
          As each user is added you'll need to assign and sometimes change the default permissions.
          On a role, each permission can be inherited, on, or off.
        </p>

        <dl class="dl-horizontal dl-horizontal-wide">
          <dt>Enabled</dt>
          <dd>Can this user/role actually be used</dd>
          <dt>Web Interface</dt>
          <dd>Can this user use the web interface, or only API calls</dd>
          <dt>Web Auth Header</dt>
          <dd>Can this user be authenticated by the Web Auth Header setting, or only digest</dd>
          <dt>Disable Arkime Email Search</dt>
          <dd>Should the user be able to use the email.* search criteria, they will still see email field when opening a session, just not able to search.</dd>
          <dt>Disable Arkime Data Removal</dt>
          <dd>Should the user be able to remove data, such as tags</dd>
          <dt>Disable Arkime Hunting</dt>
          <dd>Should the user be able to create new Arkime Hunts</dd>
          <dt>Hide Arkime Stats Page</dt>
          <dd>Should the user be able to view the Arkime Stats tabs</dd>
          <dt>Hide Arkime Files Page</dt>
          <dd>Should the user be able to view the Arkime Files page</dd>
          <dt>Hide Arkime PCAP</dt>
          <dd>When opening a session, should the user see the PCAP section</dd>
          <dt>Disable Arkime PCAP Download</dt>
          <dd>Should the user be able to download PCAP files</dd>
          <dt>Forced Expression</dt>
          <dd>An Arkime search expression that is silently added to all queries. Useful to limit what sessions can be accessed (e.g. which nodes or IPs).</dd>
          <dt>Query Time Limit</dt>
          <dd>Restrict the maximum time window of a query</dd>
        </dl>
        <p>
          A <code>usersAdmin</code> can also view a user's history, download the users as a CSV, and (with <code>arkimeAdmin</code>)
          change a user's settings.
          Search for <code>!role</code> to find users without a role.
        </p>

        <h6 id="roles">
          <v-icon icon="mdi-account-key" />&nbsp;
          Roles
        </h6>
        <p>
          The Roles page lists the user defined roles you are a Role Assigner for,
          and lets you add users to and remove users from them.
        </p>

        <h6 id="banner">
          <v-icon icon="mdi-bullhorn" />&nbsp;
          Banner
        </h6>
        <p>
          An <code>arkimeAdmin</code> can show a banner across the top of the viewer: an info, warning, or critical message,
          with an optional marquee, blink, or rainbow effect, and an optional time when it stops showing.
          "Sync to All Apps" ( <v-icon icon="mdi-sync" /> ) copies the banner to Cont3xt, Parliament, and WISE.
          Users can close the banner, and it comes back when it is changed.
        </p>

        <h6 id="esadmin">
          <v-icon icon="mdi-database-cog" />&nbsp;
          Database
        </h6>
        <p>
          A <code>dbAdmin</code> can change OpenSearch/Elasticsearch cluster settings here, such as disk watermarks, shard allocation,
          recovery and rebalancing limits, and the number of shards and replicas for new sessions indices,
          and can retry failed shards, flush, clear the cache, or unflood the cluster.
          Only shown when the cluster uses OpenSearch/Elasticsearch.
          <strong>Be careful</strong>, these settings change the whole cluster.
        </p>

        <h6 id="featherprintadmin">
          <v-icon icon="mdi-fingerprint" />&nbsp;
          Featherprint
        </h6>
        <p>
          An <code>arkimeAdmin</code> can see the <a href="help#featherprint">Featherprint</a> settings, defaults, and watched subnets here,
          and click "Scan now" to run the background scan immediately instead of waiting for the next one.
          The page is read-only: the settings come from the <code>featherprint*</code> settings and the
          <code>[featherprint-defaults]</code> and <code>[featherprint-subnet]</code> sections of the config file,
          which list the subnets to watch and which changes raise alerts.
          The scan only runs on the viewer that runs periodic queries (<code>cronQueries</code>).
          <code>featherprintAckRoles</code> (arkimeAdmin by default) controls who can acknowledge alerts.
        </p>

        <h6 id="viewconfig">
          <v-icon icon="mdi-cog-outline" />&nbsp;
          View Config
        </h6>
        <p>
          Shows the configuration the viewer is actually running with, including command line <code>-o</code> overrides and settings
          that came from the environment, with secrets hidden.
          Filter by key or value, and compare with another node to see only the differences.
          Who can see it is set by <code>viewConfigMode</code>: superAdmin (the default), appAdmin (arkimeAdmin), or off.
          If you use two-factor authentication you'll be asked for a code.
        </p>
      </div>

      <hr>

      <!-- HOT KEYS -->
      <h3 id="hotkeys">
        <v-icon icon="mdi-keyboard-outline" />&nbsp;
        Keyboard Hot Keys
      </h3>
      <p>
        Hold <code>shift</code> and press a key. Hot keys don't work while you are typing in an input.
        While <code>shift</code> is held, the owl, search, and time icons show their keys.
      </p>
      <p>
        <code>'shift + Q'</code> - set focus to the query bar (or the filter box on the Featherprint, Files, History, and Stats pages)
        <br>
        <code>'shift + T'</code> - set focus to time range selector
        <br>
        <code>'shift + A'</code> - jump to the Arkime page
        <br>
        <code>'shift + S'</code> - jump to the Sessions page
        <br>
        <code>'shift + V'</code> - jump to the SPI View page
        <br>
        <code>'shift + G'</code> - jump to the SPI Graph page
        <br>
        <code>'shift + C'</code> - jump to the Connections graph in SPI Graph
        <br>
        <code>'shift + U'</code> - jump to the Hunt page
        <br>
        <code>'shift + H'</code> - jump to this Help page
        <br>
        <code>'shift + enter'</code> - issue search/refresh
        <br>
        <code>'esc'</code> - remove focus from any input, close popups, and close the keyboard shortcuts help dialog
        <br>
        <code>'?'</code> - shows you the keyboard shortcuts help dialog
      </p>

      <hr>

      <!-- FIELDS -->
      <h3
        id="fields"
        class="d-flex align-center gap-2">
        <span class="flex-grow-1">
          <v-icon icon="mdi-format-list-bulleted" />&nbsp;
          Fields
        </span>
        <div class="arkime-input-group header-input">
          <span class="arkime-input-label arkime-input-label-fw">
            <v-icon icon="mdi-magnify" />
          </span>
          <input
            type="text"
            v-model="searchFields"
            class="arkime-input-control"
            :placeholder="$t('common.searchForFields')">
        </div>
        <v-btn
          color="primary"
          variant="flat"
          size="small"
          density="comfortable"
          @click="toggleDBFields">
          {{ showDBFields ? 'Hide' : 'Display' }} Database Fields
        </v-btn>
      </h3>

      <table
        v-if="!error && fields"
        class="arkime-table">
        <thead>
          <tr>
            <th
              class="cursor-pointer"
              @click="sortFields('friendlyName')">
              Name
              <v-icon
                icon="mdi-chevron-up"
                v-show="fieldQuery.sortField === 'friendlyName' && !fieldQuery.desc" />
              <v-icon
                icon="mdi-chevron-down"
                v-show="fieldQuery.sortField === 'friendlyName' && fieldQuery.desc" />
            </th>
            <th
              class="cursor-pointer"
              @click="sortFields('exp')">
              Field
              <v-icon
                icon="mdi-chevron-up"
                v-show="fieldQuery.sortField === 'exp' && !fieldQuery.desc" />
              <v-icon
                icon="mdi-chevron-down"
                v-show="fieldQuery.sortField === 'exp' && fieldQuery.desc" />
            </th>
            <th
              v-if="showDBFields"
              class="cursor-pointer"
              @click="sortFields('dbField')">
              Database Field
              <v-icon
                icon="mdi-chevron-up"
                v-show="fieldQuery.sortField === 'dbField' && !fieldQuery.desc" />
              <v-icon
                icon="mdi-chevron-down"
                v-show="fieldQuery.sortField === 'dbField' && fieldQuery.desc" />
            </th>
            <th>
              Operators
            </th>
            <th class="cursor-pointer">
              Data Type
            </th>
            <th>
              What?
            </th>
          </tr>
        </thead>
        <transition-group
          name="list"
          tag="tbody">
          <tr
            v-for="field in filteredFields"
            :key="field.exp">
            <td class="no-wrap">
              {{ field.friendlyName }}
            </td>
            <td class="no-wrap">
              {{ field.exp }}
            </td>
            <td
              class="no-wrap"
              v-if="showDBFields">
              {{ field.dbField }}
            </td>
            <td class="no-wrap">
              {{ fieldOperator(field) }}
            </td>
            <td class="no-wrap">
              {{ fieldType(field) }}
            </td>
            <td>
              {{ field.help }}
            </td>
          </tr>
        </transition-group>
      </table>
      <div
        v-if="!filteredFields || !filteredFields.length"
        class="text-danger text-center mb-4">
        <v-icon icon="mdi-alert" />&nbsp;
        No results match your search
      </div>
      <v-alert
        v-if="error"
        type="warning"
        variant="tonal"
        density="compact"
        class="mt-3 mb-4">
        Error retrieving fields:
        {{ error }}
      </v-alert>
    </div> <!-- End of page content -->
  </div> <!-- /help content -->
</template>

<script>
import { searchFields } from '@common/vueFilters.js';

let timeout;

const info = {
  ip: { operator: '==, !=', type: 'ip' },
  lotermfield: { operator: '==, !=', type: 'lower case string' },
  termfield: { operator: '==, !=', type: 'mixed case string' },
  uptermfield: { operator: '==, !=', type: 'upper case string' },
  integer: { operator: '<, <=, ==, >=, >, !=', type: 'integer' },
  seconds: { operator: '<, <=, ==, >=, >, !=', type: 'date time' }
};

// The side nav, in page order. Every id must match a heading id below. The
// navbar's help button links to help#<page>, so each page's route name
// (arkime, sessions, ..., esadmin, viewconfig) needs a matching id too.
const nav = [
  { id: 'about', title: 'About', icon: 'mdi-help-circle' },
  { id: 'links', title: 'Links', icon: 'mdi-link' },
  { id: 'navigation', title: 'Navigation', icon: 'mdi-compass-outline' },
  {
    id: 'search',
    title: 'Search Bar',
    icon: 'mdi-magnify',
    children: [
      { id: 'searchControls', title: 'Controls' },
      { id: 'basic', title: 'Basic Query' },
      { id: 'timebounding', title: 'Time Range' },
      { id: 'stringSearch', title: 'String' },
      { id: 'ipSearch', title: 'IP' },
      { id: 'numericSearch', title: 'Numeric' },
      { id: 'dateSearch', title: 'Date' },
      { id: 'fieldExistsSearch', title: 'Field Exists' },
      { id: 'shortcutSearch', title: 'Shortcuts' },
      { id: 'examples', title: 'Examples' }
    ]
  },
  {
    id: 'arkime',
    title: 'Arkime',
    icon: 'mdi-view-dashboard',
    children: [
      { id: 'dashboards', title: 'Dashboards' },
      { id: 'widgets', title: 'Widgets' },
      { id: 'dashboardVariables', title: 'Filter Controls' }
    ]
  },
  {
    id: 'sessions',
    title: 'Sessions',
    icon: 'mdi-swap-horizontal',
    children: [
      { id: 'views', title: 'Views' },
      { id: 'actions', title: 'Actions' },
      { id: 'visualizations', title: 'Visualizations' },
      { id: 'sessionsTable', title: 'Sessions Table' },
      { id: 'sessionDetail', title: 'Session Detail' },
      { id: 'valueMenu', title: 'Value Menus' }
    ]
  },
  { id: 'spiview', title: 'SPI View', icon: 'mdi-eyedropper' },
  {
    id: 'spigraph',
    title: 'SPI Graph',
    icon: 'mdi-chart-bar',
    children: [
      { id: 'spigraphTypes', title: 'Graph Types' },
      { id: 'spigraphSettings', title: 'Graph Settings' },
      { id: 'savedGraphs', title: 'Saved Graphs' },
      { id: 'connections', title: 'Connections' }
    ]
  },
  { id: 'hunt', title: 'Hunt', icon: 'mdi-eye' },
  { id: 'featherprint', title: 'Featherprint', icon: 'mdi-fingerprint' },
  { id: 'files', title: 'Files', icon: 'mdi-file-multiple-outline' },
  { id: 'stats', title: 'Stats', icon: 'mdi-chart-line' },
  { id: 'history', title: 'History', icon: 'mdi-history' },
  { id: 'upload', title: 'Upload', icon: 'mdi-tray-arrow-up' },
  { id: 'settings', title: 'Settings', icon: 'mdi-cog' },
  {
    id: 'admin',
    title: 'Admin',
    icon: 'mdi-shield-crown-outline',
    children: [
      { id: 'users', title: 'Users' },
      { id: 'roles', title: 'Roles' },
      { id: 'banner', title: 'Banner' },
      { id: 'esadmin', title: 'Database' },
      { id: 'featherprintadmin', title: 'Featherprint' },
      { id: 'viewconfig', title: 'View Config' }
    ]
  },
  { id: 'hotkeys', title: 'Hot Keys', icon: 'mdi-keyboard-outline' },
  { id: 'fields', title: 'Fields', icon: 'mdi-format-list-bulleted' }
];

export default {
  name: 'Help',
  data: function () {
    return {
      nav,
      error: '',
      searchFields: '',
      showDBFields: true,
      filteredFields: [],
      fieldQuery: {
        sortField: 'exp',
        desc: true
      }
    };
  },
  watch: {
    searchFields: function (newVal, oldVal) {
      this.debounceGetFilteredFields();
    }
  },
  computed: {
    fields () {
      return this.fixFields(this.$store.state.fieldsArr);
    }
  },
  created: function () {
    const hash = this.$route.hash;
    if (hash) { this.scrollFix(hash); }

    this.debounceGetFilteredFields();
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    debounceGetFilteredFields: function () {
      if (timeout) { clearTimeout(timeout); }
      // debounce the input so it only issues a request after keyups cease for 400ms
      timeout = setTimeout(() => {
        timeout = null;

        this.sortFields(this.fieldQuery.sortField);

        this.filteredFields = searchFields(this.searchFields, this.fields);
      }, 400);
    },
    toggleDBFields: function () {
      this.showDBFields = !this.showDBFields;
    },
    sortFields: function (field) {
      if (this.fieldQuery.sortField === field) {
        this.fieldQuery.desc = !this.fieldQuery.desc;
      }
      this.fieldQuery.sortField = field;

      this.filteredFields = this.filteredFields.sort((a, b) => {
        if (this.fieldQuery.desc) {
          return a[field].localeCompare(b[field]);
        } else {
          return b[field].localeCompare(a[field]);
        }
      });
    },
    fieldOperator: function (field) {
      if (!info[field.type]) { return; }
      return info[field.type].operator;
    },
    fieldType: function (field) {
      if (!info[field.type]) { return; }
      return info[field.type].type;
    },
    /* helper functions ------------------------------------------ */
    fixFields: function (fields) {
      fields.forEach((item) => {
        if (item.regex) {
          item.dbField = '';
        } else if (item.rawField) {
          item.dbField = item.dbField + ', ' + item.rawField;
        } else if (item.dbField === 'ipall') {
          item.dbField = '';
        }
      });

      return fields;
    },
    scrollFix: function (hash) {
      // remove the hash from the url
      this.$router.replace({
        ...this.$route,
        hash: undefined
      });

      setTimeout(() => {
        // add the hash back to the url so that router/index.js
        // scrollBehavior actually triggers on page load
        this.$router.replace({
          ...this.$route,
          hash
        });
      }, 750);
    }
  }
};
</script>

<style scoped>
/* make pre readable for dark and light themes */
.help-content pre {
  color: #333;
  white-space: pre-wrap;
  word-break: break-all;
  word-wrap: break-word;
  background-color: #ccc;
  border: 1px solid #ddd;
  border-radius: 4px;
  padding: 8px 12px;
}

/* help navigation */
.help-content .help-nav {
  display: flex;
  flex-direction: column;
  width: 150px;
  border: 1px solid rgb(var(--v-theme-neutral));
  border-radius: 0 8px 8px 0;
  position: fixed;
  top: 40px;
  height: calc(100vh - 50px);
  overflow-x: hidden;
  overflow-y: auto;
  white-space: nowrap;
  box-shadow: 0 0 16px -2px black;
}

.help-content .help-nav .help-link {
  display: block;
  width: 100%;
  padding: 5px 8px;
  color: rgb(var(--v-theme-foreground));
  text-decoration: none;
}
.help-content .help-nav .help-link:hover {
  background-color: rgb(var(--v-theme-neutral-light));
}
.help-content .help-nav .help-link.nested {
  margin-left: 2.4em;
  font-size: 0.85em;
  padding: 2px 5px;
}

/* indentation, one step per level: section titles (h3) flush left, the
   section's content and subsection titles (h6) one step in, and a
   subsection's content one more step in, under the h6's text. Every
   subsection lives in a wrapper div that is a child of .help-body */
.help-content .help-body {
  --help-indent: 24px;
}
.help-content .help-body > :not(h3):not(hr) {
  margin-left: var(--help-indent);
}
.help-content .help-body > div > h6 ~ :not(h6) {
  margin-left: var(--help-indent);
}
/* keep list bullets inside the content edge instead of hanging left of it,
   and give lists and code the same space below them as paragraphs */
.help-content .help-body ul,
.help-content .help-body ol {
  padding-left: 1.25rem;
  margin-bottom: 1rem;
}
.help-content .help-body li > ul,
.help-content .help-body li > ol {
  margin-bottom: 0;
}
.help-content .help-body pre {
  margin-bottom: 1rem;
}
/* the same space above every subsection title, whatever ends the one
   before it (margins collapse, so this is the gap, not added to it) */
.help-content .help-body h6 {
  margin-top: 1.5rem;
}

/* description lists: a two column grid instead of the global floated,
   right aligned dl-horizontal, so terms line up on the left and the term
   column is only as wide as each list's longest term */
.help-content dl.dl-horizontal {
  display: grid;
  grid-template-columns: fit-content(280px) 1fr;
  column-gap: 16px;
  row-gap: 4px;
  margin-bottom: 1rem;
}
.help-content dl.dl-horizontal dt {
  float: none;
  width: auto;
  text-align: left;
  white-space: normal;
  font-weight: 500;
}
.help-content dl.dl-horizontal dd {
  margin-left: 0;
}
/* lists with icons get an icon gutter so the icons form one column and
   the labels another, whether or not a term has an icon */
.help-content dl.dl-horizontal.has-icons dt {
  position: relative;
  padding-left: 32px;
}
.help-content dl.dl-horizontal.has-icons dt > .v-icon:first-child {
  position: absolute;
  left: 0;
  top: 0;
}

/* content offset for left nav */
.navbar-offset {
  padding-left: 160px;
  overflow-x: hidden;
}

.help-content .header-input {
  width: 70%;
}

/* field table animation */
.help-content .list-enter-active, .list-leave-active {
  transition: all .5s;
}
.help-content .list-enter-from, .list-leave-to {
  opacity: 0;
  transform: translateX(30px);
}
.help-content .list-move {
  transition: transform .5s;
}
</style>
