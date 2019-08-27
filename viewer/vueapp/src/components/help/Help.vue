<template>

  <div class="help-content">
    <div class="container-fluid">
      <div class="row pl-2">

        <div class="nav nav-pills col-1">
          <a href="help#about"
            class="nav-link">
            <span class="fa fa-fw fa-question-circle">
            </span>&nbsp;
            About
          </a>
          <a href="help#links"
            class="nav-link">
            <span class="fa fa-fw fa-link">
            </span>&nbsp;
            Links
          </a>
          <a href="help#search"
            class="nav-link">
            <span class="fa fa-fw fa-search">
            </span>&nbsp;
            Search Bar
          </a>
          <a href="help#stringSearch"
            class="nav-link nested">
            String
          </a>
          <a href="help#ipSearch"
            class="nav-link nested">
            IP
          </a>
          <a href="help#numericSearch"
            class="nav-link nested">
            Numeric
          </a>
          <a href="help#dateSearch"
            class="nav-link nested">
            Date
          </a>
          <a href="help#fieldExistsSearch"
            class="nav-link nested">
            Field Exists
          </a>
          <a href="help#examples"
            class="nav-link nested">
            Examples
          </a>
          <a href="help#sessions"
            class="nav-link">
            <span class="fa fa-fw fa-exchange">
            </span>&nbsp;
            Sessions
          </a>
          <a href="help#spiview"
            class="nav-link">
            <span class="fa fa-fw fa-eyedropper">
            </span>&nbsp;
            SPI View
          </a>
          <a href="help#spigraph"
            class="nav-link">
            <span class="fa fa-fw fa-bar-chart">
            </span>&nbsp;
            SPI Graph
          </a>
          <a href="help#connections"
            class="nav-link">
            <span class="fa fa-fw fa-sitemap">
            </span>&nbsp;
            Connections
          </a>
          <a href="help#hunt"
            class="nav-link">
            <span class="fa fa-fw fa-eye">
            </span>&nbsp;
            Hunt
          </a>
          <a href="help#files"
            class="nav-link">
            <span class="fa fa-fw fa-files-o">
            </span>&nbsp;
            Files
          </a>
          <a href="help#stats"
            class="nav-link">
            <span class="fa fa-fw fa-line-chart">
            </span>&nbsp;
            Stats
          </a>
          <a href="help#history"
            class="nav-link">
            <span class="fa fa-fw fa-history">
            </span>&nbsp;
            History
          </a>
          <a href="help#settings"
            class="nav-link">
            <span class="fa fa-fw fa-cog">
            </span>&nbsp;
            Settings
          </a>
          <a href="help#users"
            v-has-permission="'createEnabled'"
            class="nav-link">
            <span class="fa fa-fw fa-users">
            </span>&nbsp;
            Users
          </a>
          <a href="help#shortcuts"
            class="nav-link">
            <span class="fa fa-fw fa-keyboard-o">
            </span>&nbsp;
            Shortcuts
          </a>
          <a href="help#fields"
            class="nav-link">
            <span class="fa fa-fw fa-list">
            </span>&nbsp;
            Fields
          </a>
        </div>

        <div class="col mt-5">

          <h3 id="about">
            <span class="fa fa-question-circle"></span>&nbsp;
            About
          </h3>
          <p class="lead"><strong>
            Moloch is a large scale, open source, full packet capturing, indexing,
            and database system.
          </strong></p>
          <p class="lead">
            Moloch is not meant to replace Intrusion Detection Systems (IDS).
            Moloch augments your current security infrastructure by storing and
            indexing network traffic in standard PCAP format, while also providing
            fast indexed access. Moloch is built with an intuitive UI/UX which
            reduces the analysis time of suspected incidents.
          </p>

          <hr>

          <h3 id="links">
            <span class="fa fa-fw fa-link"></span>&nbsp;
            Links
          </h3>
          <div class="row">
            <div class="col-sm-12">
              <a class="btn btn-link" href="https://molo.ch">Home Page</a> |
              <a class="btn btn-link" href="https://molo.ch/faq">FAQ</a> |
              <a class="btn btn-link" href="https://molo.ch/learn">Docs</a> |
              <a class="btn btn-link" href="https://github.com/aol/moloch">GitHub</a> |
              <a class="btn btn-link" href="https://slackinvite.molo.ch/">Request Slack Invite</a>
            </div>
          </div>

          <hr>

          <h3 id="search">
            <span class="fa fa-search"></span>&nbsp;
            Search Bar
          </h3>
          <p>
            Most Moloch tabs have a search bar on the top of the page.
            Moloch uses a very simple query language for building expressions. It
            supports grouping using parenthesis as well as logical AND and OR statements using
            <code>&amp;&amp;</code> and <code>||</code> respectively.
            Fields can be accessed directly using the field names
            and operators described in the
            <a href="help#fields" class="no-decoration">table below</a>.
            Most fields also support a shorthand OR query using square brackets
            using CSV rules to list possible values (<code>field==[item1,item2,item3]</code>).
          </p>
          <p>
            All queries are bounded by a start and stop time. The bounded start and stop times can be
            set either by selecting a choice from a quick relative drop down or by entering exact time sections.
            Since every session has a first packet, last packet, and database timestamp, Moloch offers
            a choice on how to select the sessions:
          </p>
          <dl class="dl-horizontal">
            <dt>First Packet</dt>
            <dd>The timestamp of the first packet received for the session.</dd>
            <dt>Last Packet</dt>
            <dd>The timestamp of the last packet received for the session.</dd>
            <dt>Bounded</dt>
            <dd>Both the first and last packet timestamps for the session must be inside the time window.</dd>
            <dt>Session Overlaps</dt>
            <dd>The timestamp of the first packet must be before the end of the time window AND the timestamp of the last packet must be after the start of the time window.</dd>
            <dt>Database</dt>
            <dd>The timestamp the session was written to the database.  This can be up to several minutes AFTER the last packet was received.</dd>
          </dl>
          <br>
          <div class="ml-4">
            <h6 id="stringSearch">
              <span class="fa fa-search"></span>&nbsp;
              String Search
            </h6>
            <p>
              In Moloch, string fields are special since they can be searched in several different
              ways. When fields are indexed, their case may or may not be normalized,
              which is documented in the
              <a href="help#fields" class="no-decoration">fields table below</a>.
              The types of string searches are:
            </p>
            <dl class="dl-horizontal">
              <dt>Wildcard</dt>
              <dd>
                If a <code>*</code> appears in a expression, it is assumed a wildcard
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
                and trailing <code>.*.</code> within your regex query. The regex query
                is run against the full text strings, after case normalization (if enabled) for the field.
                For example <code>http.uri == /.*www\.f.*k\.com.*/.</code> It uses the
                Lucene regex implementation which doesn't support most PCRE features.
              </dd>
              <dt>Lists</dt>
              <dd>
                In Moloch, lists are used as a short hand method for doing multiple OR queries. For example
                <code>protocols == [http,ssh]</code>. This query will search for any sessions containing either http OR ssh.
                <strong>Note:</strong> A list containing wildcard or regex strings will be processed as normal strings instead
                of wildcards and regexes.
              </dd>
            </dl>
            <h6 id="ipSearch">
              <span class="fa fa-search"></span>&nbsp;
              IP Search
            </h6>
            <p>
              IP searching is very flexible and can be performed using the full IP
              address, a partial IP address, or CIDR representation. For fields
              that include a port number, it is possible to follow any of the IP
              representations with a colon (ip4) or dot (ip6) and then the port number to further refine a query. Ports are also
              first class searchable and may be searched for directly. For example:
              <code>ip == 1.2.3/24:80</code>. This query will search for all sessions which contain an IP address within the 1.2.3/24 CIDR range as well as utilizing port 80 during the session.
              An IP search can also be done with list of IPs which may be in mixed representations: <code>ip == [1.2.3.4,1.3/16]</code>
              If you only want to find ipv4 or ipv6 traffic, you can search using those tokens: <code>ip.src == ipv6</code>
            </p>
            <h6 id="numericSearch">
              <span class="fa fa-search"></span>&nbsp;
              Numeric Search
            </h6>
            <p>
              Numeric fields support simple range operators besides the default equals
              and not equals query types. For example, to show events with bytes transferred being less then 10000,
              use this query: <code>bytes &lt;= 10000</code>.
              Numeric fields also support lists for a simple OR query. For example, <code>port == [80,443,23]</code>
            </p>
            <h6 id="dateSearch">
              <span class="fa fa-search"></span>&nbsp;
              Date Search
            </h6>
            <p>
              Date fields support simple range operators besides the default equals
              and not equals. For example: <code>starttime == "2004/07/31 05:33:41"</code>.
              They also support lists for a simple OR query. For example:
              <code>stoptime == ["2004/07/31 05:33:41","2004/07/31 06:33:41"]</code>.
              However, it's much easier to use the
              <a href="help#timebounding" class="no-decoration">time bounding</a>
              controls under the search bar.
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
              <span class="fa fa-search"></span>&nbsp;
              Field Exists Search
            </h6>
            <p>
              It is possible to check if a field has been set or not in the session by
              using the special comparison value of <code>field == EXISTS!</code> OR negated: <code>field != EXISTS!</code>.
              For example, to verify that a certificate doesn't have an issuer common name, but does
              have a issuer organizational name, the follow query could be used:
              <code>cert.issuer.cn != EXISTS! &amp;&amp; cert.issuer.on == EXISTS!</code>
            </p>
            <h6 id="examples">
              <span class="fa fa-search"></span>&nbsp;
              Examples
            </h6>
            <p>
              Find all the sessions involving Russia (RU) or China (CN) that are
              using port 80 and also a hostname which contains "com":
              <pre>
                (country == RU || country == CN) &amp;&amp; port == 80 &amp;&amp; host == *com
              </pre>
            </p>
            <p>
              Find all the sessions of type "text/plain", involving Canada (CA), and
              containing less than 20 packets:
              <pre>
                <code>tags == "http:content:text/plain" &amp;&amp; country == CA &amp;&amp; packets &lt; 20</code>
              </pre>
            </p>
          </div>

          <hr>

          <h3 id="sessions">
            <span class="fa fa-fw fa-exchange"></span>&nbsp;
            Sessions
          </h3>
          <p>
            The Sessions page within Moloch is where an analyst will find the bulk of the details regarding
            the sessions being investigated.
          </p>
          <div class="ml-4">
            <h6>
              <span class="fa fa-fw fa-exchange"></span>&nbsp;
              Search
            </h6>
            <p>
              The magnifying glass ( <span class="fa fa-search"></span> ) in the top left corner indicates the search bar. Enter your query string here and then hit ENTER or click the "Search" button to run your query.
              While typing fieldnames into the query bar predicative typing will overlay with potential fieldname choices based on what has been typed so far.
              See the <a href="help#search" class="no-decoration">search section</a> for more in depth information.
            </p>
            <h6 id="timebounding">
              <span class="fa fa-fw fa-exchange"></span>&nbsp;
              Time Bounding
            </h6>
            <p>
              The controls under the search bar contain the time bounding selections. The first element sets relative timing (such as last hour, last day, etc).
              The Start box allows a start time/date to be selected. The End box allows an end time/date to be selected.
              The bounding box is used to select where time bounding is applied (last packet, bounded, Session Overlaps, Database)
            </p>
            <h6>
              <span class="fa fa-fw fa-exchange"></span>&nbsp;
              Paging
            </h6>
            <p>
              The paging controls and records per page select box is found under the time range inputs and allows a user to navigate through the sessions returned.
              Hover over the records per page text for more information on how many total entries were searched.
            </p>
            <h6 id="views">
              <span class="fa fa-fw fa-exchange"></span>&nbsp;
              Views
            </h6>
            <p>
              The eyeball icon ( <span class="fa fa-eye"></span> ) button allows an analyst to either overlay a saved "View" onto their current query, or save the current query as a "View" for easy recall in the future.
              Views should be used for often used search queries that an analyst regularly finds themselves running.
            </p>
            <h6>
              <span class="fa fa-fw fa-exchange"></span>&nbsp;
              Actions
            </h6>
            <p>
              The down arrow ( <span class="fa fa-caret-down"></span> ) button contains a few options:
              <ol>
                <li>Export a PCAP of the required sessions data.</li>
                <li>Data within the viewer may also be exported as a CSV for further review and manipulation.</li>
                <li>Tags may be added or removed from the sessions which have been detected by the analyst's query. Theses events can then later be recalled by using the <code>tags == "blah"</code> statements.</li>
                <li>Send the selected data to another system for further analysis.</li>
                <li>Scrub packet data by overwriting the packets (if a user has data removal privelages).</li>
                <li>Delete SPI and PCAP data entirely (if a user has data removal privelages).</li>
              </ol>
              Each of these options may be applied to the sessions which have been opened (by clicking the sessions + box),
              any items visible (on the current page), or all items which have matched the query string.
            </p>
            <h6>
              <span class="fa fa-fw fa-exchange"></span>&nbsp;
              Visualizations
            </h6>
            <p>
              The section above the Sessions table contains a visualisation of the query's output. This quick glance visualization may be viewed by Session count, Packets count, or Databytes count.
              Clicking and dragging over bars within the chart will drill into the selected time frame so only it is selected.
              Additionally, a user can click the "+" or "-" magnifying glasses to quickly zoom out or zoom in the time window being observed.
            </p>
            <h6>
              <span class="fa fa-fw fa-exchange"></span>&nbsp;
              Sessions table
            </h6>
            <p>
              Now we arrive at the meat of the Sessions page... Our Session Data! Before we drill into what can be observed within the sessions data, please take note of the column configuration ( <span class="fa fa-th"></span> ) button. By clicking this button an analyst may select
              any field that they wish to observe without opening a session fully by adding it as a column to the table. Predictive typing is also applied within this box. Example: An analyst is only investigating suspicious IRC sessions via the analyst's search query. Instead of drilling into each
              session, that analyst has decided to only show the Start Time, End Time, Src IP, Src Port, Dst IP, Dst Pot, IRC Channel, and IRC Nickname. This column selection tool allows for an analyst to readily view the information which is important to them, configurable per investigation.
            </p>
            <p>
              <em>
                <strong>Tip:</strong>
                An analyst can save or load previously saved column configurations by clicking the save/load column configuration
                ( <span class="fa fa-columns"></span> ) button.
              </em>
            </p>
            <p>
              Hover over a column header and click the column action ( <span class="fa fa-caret-down"></span> ) button to do a few things:
              <ol>
                <li>Hide the column from the table</li>
                <li>Export the column values</li>
                <li>Export the column values with counts</li>
                <li>Open the SPI Graph page with that field (see the <a href="help#spigraph">SPI Graph</a> section for more details)</li>
              </ol>
            </p>
            <p>
              Viewing the sessions data of a specific network session is as easy as clicking the plus ( <span class="fa fa-plus"></span> ) button to the left of every session. Upon clicking the button, the session drawer will expand giving further context to the session.
              All packet data which was parsed will now be displayed. This includes everything from the user making the connection to the TCP Flags observed during the session.
              If the sessions is HTTP based further data such as Method, User Agents, and Response Headers are readily observable. The extracted Request and Response packet text will also be readily available in either a natural, ascii, utf8, or hex format.
              Furthermore, an analyst can uncompress response data as well as image files which were transferred during the session (be cautious with this option click if investigating sensitive images). Other files (such as mp3s, swf, or js files) may be clicked so that
              they may be downloaded and analyzed when necessary.
            </p>
          </div>

          <hr>

          <h3 id="spiview">
            <span class="fa fa-fw fa-eyedropper"></span>&nbsp;
            SPI View
          </h3>
          <p>
            The SPI (Session Profile Information) View is used to dive into specific metrics of a session which an analyst wishes to investigate further. Instead of manually writing a query,
            an analyst can hover over field values within the SPI View to add the specific item as an AND or AND NOT to their query. This page also allows an
            analyst a quick view into counts of each item that the user is interested in. As an example, if a analyst wanted to see all BASIC authorization headers that have been recorded
            over the current time window, an analyst could open the http drawer and click to enable the http.authorization field. The analyst could then update their search query to either
            include a specific authorization string which has been observed, or use a wild card to see all of a certain type of authorization header (Basic *, BEARER *, etc). Additionally,
            the SPI View allows an analyst a quick view of observed IP addresses within the time window, http response codes, IRC NICKs/Channels, and much, much more.
          </p>
          <p>
            <strong>Tip:</strong>
            The analyst can save or load the fields that they have displayed/hidden on the SPI View page by clicking the save/load field configuration
            ( <span class="fa fa-columns"></span> ) button.
          </p>

          <hr>

          <h3 id="spigraph">
            <span class="fa fa-fw fa-bar-chart"></span>&nbsp;
            SPI Graph
          </h3>
          <p>
            The SPI Graph page allows a user to visualize, via bar charts over time, any item within the SPI View page. This page is very useful for both at a glance views of activity per SPI type,
            as well as deep dive analysis. For example, if you wanted to chart all of the currently recorded http.users within your current time window, select http.user from the SPI Graph selection typeahead.
            Data will be displayed based upon count of observances over the time period. Increasing the Max Elements setting will allow an analyst to see additional items if the investigates SPI type is noisy.
            An analyst can sort by either the noisiest value (graph) or by alphabetical order (name). This page also has the ability to update every X seconds.
          </p>

          <hr>

          <h3 id="connections">
            <span class="fa fa-fw fa-sitemap"></span>&nbsp;
            Connections
          </h3>
          <p>
            The Connections page allows a user to view a tree force graph based on a source node and destination node of their choosing. Relationships may be visually determined using this method.
            For example, set your Src node to ip.src, Dst node to ip.dst:port and you will be able to visualize the relationship of source IP addresses to destination IP address / port combinations.
            Those who prefer to analyze session data visually may rely heavily on these graphs. The default settings for this page may be set in the settings page.
          </p>

          <hr>

          <h3 id="hunt">
            <span class="fa fa-fw fa-eye"></span>&nbsp;
            Hunt
          </h3>
          <p>
            The Hunt page allows users to search within session packets for text.
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
              <dt>Search</dt>
              <dd>The text to search for (ascii, case sensitive ascii, hex, regex, or hex regex)</dd>
              <dt>Search src/dst packets</dt>
              <dd>Whether to search source or destination packets, or both</dd>
              <dt>Search raw/reassembled packets</dt>
              <dd>Whether to search raw or reassembled packets</dd>
            </dl>
          <p>
            Once the hunt has been created, it will be added to the <strong>hunt job queue</strong>. Hunts run one at a time and
            can be paused so that another hunt can run. After a hunt has completed, it will be added to
            the <strong>hunt job history</strong>. Here, you can rerun a hunt on a different set of sessions, open the
            sessions that matched the hunt, or delete the hunt job from the history.
          </p>
          <p>
            Hunts add <code>huntName</code> and <code>huntId</code> fields to the sessions as they match.
            You can search for these fields in the sessions search bar at any time. Deleting a hunt from the history
            does not remove these fields from the sessions.
          </p>
          <p>
            <strong>Info:</strong> A normal user can only view/pause/delete their own hunts, but an admin can view/pause/delete all hunts.<br>
            <strong>Warning:</strong> The packet search will take a long time and possibly slow down viewer if you search many sessions.
            Users will be alerted if they are trying to search for more than 100,000 sessions. Normal users cannot search more than 1,000,000
            sessions, and admins cannot search more than 10,000,000 sessions (these values can be overwritten in the config).
          </p>

          <hr>

          <h3 id="files">
            <span class="fa fa-fw fa-files-o"></span>&nbsp;
            Files
          </h3>
          <p>
            The files page shows a table view of PCAPs which have been written. Details included are: File Number. Node, filename, if the file is locked, the first date and the file size.
          </p>

          <hr>

          <h3 id="stats">
            <span class="fa fa-fw fa-line-chart"></span>&nbsp;
            Stats
          </h3>
          <p>
            There are several tabs that contain statistics about your capture node and Elasticsearch node and are enumerated below:
          </p>
          <div class="ml-4">
            <h6>
              <span class="fa fa-fw fa-line-chart"></span>&nbsp;
              Capture Graphs
            </h6>
            <p>
              The Capture Graphs tab displays realtime graphs representing what the capture node is doing.
              Larger values are overplotted in successively darker colors.
              Hover over the graph to see what each capture node is capturing at any specific time.
            </p>
            <p>
              <strong>Tip:</strong>
              These graphs are 1440 pixels wide (4 hours of data at 10 second intervals).
              Expand your browser window to at least 1500 pixels wide for best viewing or you will be shown a horizontal scroll bar (ew).
            </p>
            <h6>
              <span class="fa fa-fw fa-line-chart"></span>&nbsp;
              Capture Stats
            </h6>
            <p>
              The Capture Stats tab displays a table containing information for each capture node.
              Please use the column config drop down to change which fields are shown.
            </p>
            <dl class="dl-horizontal">
              <dt>Node</dt>
              <dd>The name of the capture node</dd>
              <dt>Time</dt>
              <dd>The time reported by the capture node</dd>
              <dt>Sessions</dt>
              <dd>The number of sessions currently being monitored in memory</dd>
              <dt>Free Space</dt>
              <dd>Percentage of free space across all configured disks</dd>
              <dt>CPU</dt>
              <dd>CPU percentage that Moloch is using</dd>
              <dt>Memory %</dt>
              <dd>Perentage of memory that Moloch is using</dd>
              <dt>Packet Q</dt>
              <dd>Number of packets that are waiting to processed</dd>
              <dt>Packets/s</dt>
              <dd>The number of packets that we've received that aren't corrupt that we try to add to a packetQ per second</dd>
              <dt>Bytes/s</dt>
              <dd>The size of all the packets that we've received that aren't corrupt that we try add to a packetQ per second</dd>
              <dt>Sessions/s</dt>
              <dd>Number of sessions sent to Elasticsearch per second</dd>
              <dt>Packet Drops/s</dt>
              <dd>Number of dropped packets as reported by the OS or network card (Moloch never sees these) per second</dd>
              <dt>Overload Drops/s</dt>
              <dd>Number of packets dropped because there was no packet queue that was free to process them on</dd>
              <dt>ES Drops/s</dt>
              <dd>Number of elasticsearch requests that were dropped because of queue overflow per second</dd>
              <dt>Bits/Sec</dt>
              <dd>Same as Bytes/Sec but in bits per second</dd>
              <dt>Sessions</dt>
              <dd>Number of sessions Moloch is currently monitoring</dd>
              <dt>Active TCP Sessions</dt>
              <dd>Number of TCP sessions Moloch is currently monitoring</dd>
              <dt>Active UDP Sessions</dt>
              <dd>Number of UDP sessions Moloch is currently monitoring</dd>
              <dt>Active ICMP Sessions</dt>
              <dd>Number of ICMP sessions Moloch is currently monitoring</dd>
              <dt>Free Space</dt>
              <dd>Free space across all configured disks</dd>
              <dt>Memory</dt>
              <dd>Amount of memory that Moloch is using</dd>
              <dt>Disk Q</dt>
              <dd>Number of blocks of data that are waiting to be written to disk</dd>
              <dt>ES Q</dt>
              <dd>Number of elasticsearch requests that are waiting to be sent</dd>
              <dt>Closing Q</dt>
              <dd>Number of TCP sessions that have received a FIN and Moloch is waiting to see if actually closed</dd>
              <dt>Waiting Q</dt>
              <dd>Number of sessions that are ready to be written but are waiting on an asynchronus request (wise, plugins) to finish</dd>
              <dt>Active Fragments</dt>
              <dd>Number of packets that are waiting on remaining IP fragments to show up</dd>
              <dt>Fragments Dropped/Sec</dt>
              <dd>Number of packets that were dropped because frag overload or timeouts</dd>
              <dt>Total Dropped/Sec</dt>
              <dd>Sum of the inputs dropped and overload metrics</dd>
              <dt>Written Bytes/Sec</dt>
              <dd>The size of all the packets that Moloch is going write to disk</dd>
              <dt>Unritten Bytes/Sec</dt>
              <dd>The size of all the packets that Moloch isn't going to write to disk, but that we processed</dd>
            </dl>
            <p>
              <em>
                <strong>Tip:</strong>
                Click the <span class="fa fa-plus"></span> button to display graphs about the table information.
              </em>
            </p>
            <h6>
              <span class="fa fa-fw fa-line-chart"></span>&nbsp;
              ES Nodes
            </h6>
            <p>
              The ES Nodes tab displays a table containing information for each Elasticsearch node.
              Please use the column config drop down to change which fields are shown.
            </p>
            <dl class="dl-horizontal">
              <dt>Name</dt>
              <dd>The name of the ES node</dd>
              <dt>Documents</dt>
              <dd>The total number of documents that this node has ingested</dd>
              <dt>Disk Used</dt>
              <dd>The disk used for Elasticsearch store</dd>
              <dt>Disk Free</dt>
              <dd>The disk free for Elasticsearch store</dd>
              <dt>Heap Size</dt>
              <dd>JVM heap used in bytes</dd>
              <dt>OS Load</dt>
              <dd>Currently calculated average load of the system</dd>
              <dt>CPU</dt>
              <dd>CPU usage in percent</dd>
              <dt>Reads/s</dt>
              <dd>The number of bytes read across all devices used by Elasticsearch per second</dd>
              <dt>Writes/s</dt>
              <dd>The number of bytes written across all devices used by Elasticsearch per second</dd>
              <dt>Searches/s</dt>
              <dd>Current query phase operations per second</dd>
              <dt>IP</dt>
              <dd>IP of the node</dd>
              <dt>IP Excluded</dt>
              <dd>Is the node currently excluded by IP</dd>
              <dt>Node Excluded</dt>
              <dd>Is the node currently excluded by node name</dd>
              <dt>Non Heap Size</dt>
              <dd>Memory being used that isn't on the heap</dd>
            </dl>
            <h6>
              <span class="fa fa-fw fa-line-chart"></span>&nbsp;
              ES Indices
            </h6>
            <p>
              The ES Indices tab displays a table containing information for each Elasticsearch index.
              Please use the column config drop down to change which fields are shown.
            </p>
            <dl class="dl-horizontal">
              <dt>Name</dt>
              <dd>The name of the index</dd>
              <dt>Documents</dt>
              <dd>The number of documents in the index</dd>
              <dt>Disk Size</dt>
              <dd>The size of the Elasticsearch store for this index</dd>
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
              <dt>Create Date</dt>
              <dd>When the index was created</dd>
              <dt>UUID</dt>
              <dd>The internal UUID for the index</dd>
            </dl>
            <h6>
              <span class="fa fa-fw fa-line-chart"></span>&nbsp;
              ES Tasks
            </h6>
            <p>
              The ES Task tab displays a table containing information for each Elasticsearch task.
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
                If you have data removal privelages, you can cancel tasks by clicking the
                <span class="fa fa-trash"></span> button.
                This is useful when you run a query that is taking longer than intended.
                <br>
                You can also filter the table by only cancelable tasks by clicking the checkbox
                in the top right corner of the table.
              </em>
            </p>
            <h6>
              <span class="fa fa-fw fa-line-chart"></span>&nbsp;
              ES Shards
            </h6>
            <p>
              The ES Shards tab displays a matrix containing the ES indices and nodes.
              Each cell describes the number of shards that that index has in that particular node.
              <span class="badge badge-pill badge-secondary">Gray</span> means it's an alternate shard and the
              <span class="badge badge-pill badge-primary">other color</span> means it's a primary shard.
              Hover over a cell to get more information.
            </p>
            <h6>
              <span class="fa fa-fw fa-line-chart"></span>&nbsp;
              ES Recoveery
            </h6>
            <p>
              The ES Recovery tab displays a table containing information for each Elasticsearch index
              recovery.  By default it only shows information about indices still waiting to be recovered.
              It is a simple view of the <code>/_cat/recovery</code> Elasticssearch API.
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
              <dt>Repository</dt>
              <dd>The repository</dd>
              <dt>Snapshot</dt>
              <dd>The snapshot</dd>
              <dt>Files</dt>
              <dd>The number of files to recover</dd>
              <dt>Files recovered</dt>
              <dd>The files recovered</dd>
              <dt>Files percent</dt>
              <dd>The percent of files recovered</dd>
              <dt>Files total</dt>
              <dd>The total number of files</dd>
              <dt>Bytes</dt>
              <dd>The number of bytes to recover</dd>
              <dt>Bytes recovered</dt>
              <dd>The bytes recovered</dd>
              <dt>Bytes percent</dt>
              <dd>The percent of bytes recovered</dd>
              <dt>Bytes total</dt>
              <dd>The total number of bytes</dd>
              <dt>Translog</dt>
              <dd>The number of translog ops to recover</dd>
              <dt>Translog Recovered</dt>
              <dd>The translog ops recovered</dd>
              <dt>Translog percent</dt>
              <dd>The percent of translog ops recovered</dd>
            </dl>
          </div>

          <hr>

          <h3 id="history">
            <span class="fa fa-fw fa-history"></span>&nbsp;
            History
          </h3>
          <p>
            The History page provides the ability to view Moloch actions/queries to
            ES. It is usable both as a history for a user and for auditing abilities
            for an admin. A non-admin user can only view their own actions. An admin
            user can view all users' actions.
          </p>
          <div class="ml-4">
            <span class="fa fa-fw fa-search"></span>&nbsp;
            Use the search input at the top of the page to search for specific history items.
            <br>
            <em style="margin-left:2.75rem;">
              Check out the ES
              <a href="https://www.elastic.co/guide/en/elasticsearch/reference/current/query-dsl-query-string-query.html#query-string-syntax">
                query string syntax</a>
              for more information about how to query the history table.
            </em>
          </div>
          <div class="ml-4">
            <span class="fa fa-fw fa fa-clock-o"></span>&nbsp;
            Filter history by a time range by utilizing the time controls under the search bar.
          </div>
          <div class="ml-4">
            <span class="fa fa-fw fa fa-sort"></span>&nbsp;
            Sort history by clicking any column header.
          </div>
          <div class="ml-4">
            <span class="fa fa-fw fa-filter"></span>&nbsp;
            Use the filter button to filter history by specific field values.
          </div>
          <div class="ml-4">
            <span class="fa fa-fw fa-check-square"></span>&nbsp;
            Use the checkboxes within column headers to display history items that
            always have a value for that field.
          </div>
          <div class="ml-4">
            <span class="fa fa-fw fa-plus"></span>&nbsp;
            Use the expand button to display more information about a history item.
          </div>
          <div class="ml-4">
            <span class="fa fa-fw fa-folder-open"></span>&nbsp;
            Use the open button to "go to" the history item. This will open the page
            that the action/query was issued from.
            <br>
            <em style="margin-left:2.5rem;">
              &nbsp;This is only available for the main GET requests from a page
              within the application.
            </em>
          </div>

          <hr>

          <h3 id="settings">
            <span class="fa fa-fw fa-cog"></span>&nbsp;
            Settings
          </h3>
          <p>
            The Settings page allows for general user based settings to be managed and is separated into different sections.
            An admin can update the settings of any user.
          </p>
          <div class="ml-4">
            <h6>
              <span class="fa fa-fw fa-cog"></span>&nbsp;
              General
            </h6>
            <p>
              Here, a user can:
              <ol>
                <li>Manage their timezone format</li>
                <li>Set their default session detail packets format</li>
                <li>Set their default number of packets returned</li>
                <li>Set the default for showing or hiding packet timestamps</li>
                <li>Manage whether to issue a query on Sessions page load</li>
                <li>Set their default Sessions table sort</li>
                <li>Set their default SPI Graph field</li>
                <li>Set their default Connections source field</li>
                <li>Set their default Connetions destination field</li>
              </ol>
            </p>
            <h6>
              <span class="fa fa-fw fa-cog"></span>&nbsp;
              Views
            </h6>
            <p>
              Here, a user can manage their saved views by updating, deleting, or sharing them.
              Sharing a view allows all other users to use that view (only admins can edit it).
              A user can also create a new view in this section.
              See the <a href="help#views" class="no-decoration">Views</a> section for more information.
            </p>
            <h6>
              <span class="fa fa-fw fa-cog"></span>&nbsp;
              Cron Queries
            </h6>
            <p>
              Here, a user can manage their cron queries by updating or deleting them.
              They can also create a new cron query.
            </p>
            <h6>
              <span class="fa fa-fw fa-cog"></span>&nbsp;
              Column Configurations
            </h6>
            <p>
              Here, a user can view and remove their currently saved custom Sessions table column configurations.
            </p>
            <h6>
              <span class="fa fa-fw fa-cog"></span>&nbsp;
              SPI View Field Configurations
            </h6>
            <p>
              Here, a user can view and remove their currently saved custom SPI View field configurations.
            </p>
            <h6>
              <span class="fa fa-fw fa-cog"></span>&nbsp;
              Themes
            </h6>
            <p>
              Here, a user can select from several predefined user interface themes.
              If they're feeling adventurous and maybe a little dangerous, they can create their own custom theme.
              <br>
              <em>
                <strong>Note:</strong>
                Be careful with this feature, it is easy to make the UI completley unusable.
              </em>
            </p>
            <h6>
              <span class="fa fa-fw fa-cog"></span>&nbsp;
              Password
            </h6>
            <p>
              Here, a user can update their password.
            </p>
            <h6>
              <span class="fa fa-fw fa-cog"></span>&nbsp;
              Notifiers
            </h6>
            <p>
              Here, an <strong>admin</strong> user can create, update, and delete notifiers.
              The available notifiers are Slack, Email, and texts via Twilio.
              Notifiers can be used to alert users that cron queries have found matching sessions.
              Notifiers alert every <strong>10</strong> minutes if there are <strong>new</strong> matches.
            </p>
          </div>

          <hr>

          <h3 id="users" v-has-permission="'createEnabled'">
            <span class="fa fa-fw fa-users"></span>&nbsp;
            Users
          </h3>
          <p v-has-permission="'createEnabled'">
            The Users page, as you may have guessed, is where user options are configured and added to the system. Multiple options for role based access control (RBAC) may be leveraged.
            These options in include: The User ID, The Name of the user, a Forced expression (only allows a user to see data related to the specified expression/query), an Account enabled toggle, an Admin toggle,
            if the user is allowed access to the web interface, if the user is allowed access to http based Authorization Headers, if the user may search captured email data, if the user may remove data from the system (scrub).
            This page also allows for the deletion of a previously created user. Clicking on the Settings link will jump to the users <a href="help#settings" class="no-decoration">Settings</a> page.
          </p>

          <hr>

          <h3 id="shortcuts">
            <span class="fa fa-fw fa-keyboard-o"></span>&nbsp;
            Keyboard Shortcuts
          </h3>
          <p class="ml-4">
            <code>'Q'</code> - set focus to query bar
            <br>
            <code>'T'</code> - set focus to time range selector
            <br>
            <code>'S'</code> - jump to the Sessions page
            <br>
            <code>'V'</code> - jump to the SPI View page
            <br>
            <code>'G'</code> - jump to the SPI Graph page
            <br>
            <code>'C'</code> - jump to the Connections page
            <br>
            <code>'H'</code> - jump to the Moloch Help page
            <br>
            <code>'U'</code> - jump to the Moloch Hunt page
            <br>
            <code>'shift + enter'</code> - issue search/refresh
            <br>
            <code>'esc'</code> - remove focus from any input and close the keyboard shortcuts help dialog
            <br>
            <code>'?'</code> - shows you the keyboard shortcuts help dialog
          </p>

          <hr v-has-permission="'createEnabled'">

          <h3 id="fields">
            <span class="fa fa-fw fa-list"></span>&nbsp;
            Fields
            <div class="input-group input-group-sm pull-right header-input">
              <div class="input-group-prepend">
                <span class="input-group-text">
                  <span class="fa fa-search">
                  </span>
                </span>
              </div>
              <input type="text"
                v-model="searchFields"
                class="form-control"
                placeholder="Search for fields in the table below"
              />
            </div>
            <button type="button"
              class="btn btn-secondary btn-checkbox btn-sm pull-right mr-1"
              :class="{'active':showDBFields}"
              @click="toggleDBFields">
              Display Database Fields
            </button>
          </h3>

          <table v-if="!error && fields"
            class="table table-sm table-striped">
            <thead>
              <tr>
                <th class="cursor-pointer"
                  @click="sortFields('friendlyName')">
                  Name
                  <span v-show="fieldQuery.sortField === 'friendlyName' && !fieldQuery.desc" class="fa fa-sort-asc"></span>
                  <span v-show="fieldQuery.sortField === 'friendlyName' && fieldQuery.desc" class="fa fa-sort-desc"></span>
                  <span v-show="fieldQuery.sortField !== 'friendlyName'" class="fa fa-sort"></span>
                </th>
                <th class="cursor-pointer"
                  @click="sortFields('exp')">
                  Exp
                  <span v-show="fieldQuery.sortField === 'exp' && !fieldQuery.desc" class="fa fa-sort-asc"></span>
                  <span v-show="fieldQuery.sortField === 'exp' && fieldQuery.desc" class="fa fa-sort-desc"></span>
                  <span v-show="fieldQuery.sortField !== 'exp'" class="fa fa-sort"></span>
                </th>
                <th v-if="showDBFields"
                  class="cursor-pointer"
                  @click="sortFields('dbField')">
                  Database Field
                  <span v-show="fieldQuery.sortField === 'dbField' && !fieldQuery.desc" class="fa fa-sort-asc"></span>
                  <span v-show="fieldQuery.sortField === 'dbField' && fieldQuery.desc" class="fa fa-sort-desc"></span>
                  <span v-show="fieldQuery.sortField !== 'dbField'" class="fa fa-sort"></span>
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
            <transition-group name="list"
              tag="tbody">
              <tr v-for="field in filteredFields"
                :key="field.exp">
                <td class="no-wrap">
                  {{ field.friendlyName }}
                </td>
                <td class="no-wrap">
                  {{ field.exp }}
                </td>
                <td class="no-wrap"
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
          <div v-if="!filteredFields || !filteredFields.length"
            class="text-danger text-center mb-4">
            <span class="fa fa-warning">
            </span>&nbsp;
            No results match your search
          </div>
          <div v-if="error"
            class="alert alert-warning mt-3 mb-4">
            <span class="fa fa-warning">
            </span>&nbsp;
            Error retrieving fields:
            {{ error }}
          </div>

        </div>

      </div>
    </div>
  </div> <!-- /help content -->

</template>

<script>
import FieldService from '../search/FieldService';

let timeout;
let info = {
  ip: { operator: '==, !=', type: 'ip' },
  lotermfield: { operator: '==, !=', type: 'lower case string' },
  termfield: { operator: '==, !=', type: 'mixed case string' },
  uptermfield: { operator: '==, !=', type: 'upper case string' },
  integer: { operator: '<, <=, ==, >=, >, !=', type: 'integer' },
  seconds: { operator: '<, <=, ==, >=, >, !=', type: 'date time' }
};

export default {
  name: 'Help',
  data: function () {
    return {
      error: '',
      fields: [],
      searchFields: '',
      showDBFields: false,
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
  created: function () {
    const hash = this.$route.hash;
    if (hash) { this.scrollFix(hash); }

    this.loadFields();
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

        this.filteredFields = this.$options.filters.searchFields(this.searchFields, this.fields);
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
    loadFields: function () {
      FieldService.get(true)
        .then((response) => {
          this.fields = this.fixFields(response);
        }, (error) => {
          this.error = error;
        });
    },
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
          hash: hash
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
  word-break: break-all;
  word-wrap: break-word;
  background-color: #ccc;
  border: 1px solid #ddd;
  border-radius: 4px;
  padding-top: 16px;
}

/* help navigation */
.help-content div.nav-pills {
  min-width: 150px;
  border: 1px solid var(--color-gray);
  border-radius: 4px;
  position: sticky;
  top: 40px;
  height: calc(100vh - 50px);
  overflow-x: hidden;
  overflow-y: auto;
  white-space: nowrap;

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

.help-content .nav-pills .nav-link {
  width: 100%;
}

.help-content div.nav-pills a {
  padding: 5px 8px !important;
}

.help-content div.nav-pills a.nested {
  margin-left: 2.4em;
  font-size: 0.85em;
  padding: 2px 5px !important;
}

.help-content .header-input {
  width: 70%;
}

/* field table animation */
.help-content .list-enter-active, .list-leave-active {
  transition: all .5s;
}
.help-content .list-enter, .list-leave-to {
  opacity: 0;
  transform: translateX(30px);
}
.help-content .list-move {
  transition: transform .5s;
}

.badge.badge-primary {
  font-weight: bold;
  background-color: var(--color-primary);
}
</style>
