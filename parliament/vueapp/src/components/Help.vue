<template>

  <div class="help-content">
    <div class="container-fluid">
      <div class="row pl-2">

        <div class="nav flex-column nav-pills col-1">
          <a href="help#about"
            class="nav-link">
            <span class="fa fa-fw fa-question-circle">
            </span>&nbsp;
            About
          </a>
          <a href="help#dashboard"
            class="nav-link">
            <span class="fa fa-fw fa-tachometer">
            </span>&nbsp;
            Dashboard
          </a>
          <a href="help#issues"
            class="nav-link">
            <span class="fa fa-fw fa-exclamation-triangle">
            </span>&nbsp;
            Issues
          </a>
          <a href="help#settings"
            class="nav-link">
            <span class="fa fa-fw fa-cogs">
            </span>&nbsp;
            Settings
          </a>
          <a href="help#general"
            class="nav-link nested">
            General
          </a>
          <a href="help#password"
            class="nav-link nested">
            Password
          </a>
          <a href="help#notifiers"
            class="nav-link nested">
            Notifiers
          </a>
        </div>

        <div class="col mt-2 mb-3">

          <!-- about -->
          <h3 id="about">
            <span class="fa fa-question-circle">
            </span>&nbsp;
            About
          </h3>
          <p class="lead"><strong>
            Parliament contains a grouped list of your Moloch clusters.
          </strong></p>
          <p class="lead">
            Parliament allows users to view your Moloch cluster's status and
            navigate to each cluster.
          </p>
          <!-- /about -->

          <hr>

          <!-- dashboard -->
          <h3 id="dashboard">
            <span class="fa fa-tachometer">
            </span>&nbsp;
            Parliament Dashboard
          </h3>
          <p class="lead">
            The Parliament dashboard includes links, ES health, and issues for each Moloch cluster.
          </p>
          <p>
            The dashboard page allows users to view and interact with the Molochs in your Parliament.
            You can search for Molochs in your Parliament, change the data refresh time
            (15 seconds is the default),
            and hover over issues and ES health statuses for more information.
          </p>
          <p>
            <strong>Once logged in</strong>, a user can acknowledge and ignore issues for each cluster as well as update
            the Parliament when in Edit Mode. To enter this mode, toggle the switch on the top right
            (below the navbar). Now you can add, update, delete, and reorder groups and clusters in your Parliament.
          </p>
          <!-- /dashboard -->

          <hr>

          <!-- issues -->
          <h3 id="issues">
            <span class="fa fa-exclamation-triangle">
            </span>&nbsp;
            Issues
          </h3>
          <p class="lead">
            The issues page contains a list of issues that your Parliament is experiencing.
          </p>
          <p>
            <strong>Once logged in</strong>, a user can ignore, acknowledge, and remove
            acknowledged issues for every cluster.
          </p>
          <p>
            <span class="fa fa-check">
            </span>&nbsp;
            <strong>Acknowledged issues</strong> will not show up on the main Parliament page,
              but will remain here (grayed out) to be removed
              (via the trashcan button or by waiting for them to be removed automatically after the set time).
          </p>
          <p>
            <span class="fa fa-eye-slash">
            </span>&nbsp;
            <strong>Ignored issues</strong> will not show up on the main Parliament page,
            but will remain here (grayed out) to be unignored
            (via the ignore dropdown button or automatically after the set ignore time has expired).
          </p>

          <!-- TODO describe issue types -->
          <p>
            Issue types include:
          </p>
          <ul>
            <li>
              <strong>ES Down:</strong>
              Elasticsearch was unreachable
              <em class="text-muted">(configurable in settings)</em>.
            </li>
            <li>
              <strong>Out of Date:</strong>
              The capture node has not checked in
              <em class="text-muted">(configurable in settings)</em>.
            </li>
            <li>
              <strong>Low Packets:</strong>
              The capture node is not receiving many packets
              <em class="text-muted">(configurable in settings)</em>.
            </li>
            <li>
              <strong>ES Red:</strong>
              The Elasticsearch status returned from the cluster health checkwas RED.
            </li>
            <li>
              <strong>ES Dropped:</strong>
              The capture node is overloading Elasticsearch and is now dropping bulk inserts.
            </li>
          </ul>
          <!-- /issues -->

          <hr>

          <!-- settings -->
          <h3 id="settings">
            <span class="fa fa-cogs">
            </span>&nbsp;
            Settings
          </h3>
          <p class="lead">
            The settings page allows you to configure your Parliament for your needs.
          </p>
          <p>
            <em>
              <strong>Note:</strong> To view this page, you cannot run the Parliament in
              dashboard only mode and the user must be logged in or not have a password configured.
            </em>
          </p>

          <div class="ml-4">
            <!-- general -->
            <h6 id="general">
              <span class="fa fa-fw fa-cog">
              </span>&nbsp;
              General
            </h6>
            <p>
              The general section has a few settings that pertain to issues in your Parliament:
            </p>
            <ul>
              <li>
                <strong>Capture nodes must check in this often</strong>
                controls how behind a node's cluster's timestamp can be from the current time.
                If the timestamp exceeds this time setting, an <strong>Out Of Date</strong>
                issue is added to the cluster.
                <br>
                <em class="text-muted">
                  The default for this setting is <strong>30 seconds</strong>.
                </em>
              </li>
              <li>
                <strong>Elasticsearch query timeout</strong> controls the maxiumum Elasticsearch
                status query duration. If the query exceeds this time setting, an
                <strong>ES Down</strong> issue is added to the cluster.
                <br>
                <em class="text-muted">
                  The default for this setting is <strong>5 seconds</strong>.
                </em>
              </li>
              <li>
                <strong>Low Packets Threshold</strong> controls the minumum number of packets that
                the capture node must receive. If the capture node is not receiving enough packets,
                a <strong>Low Packets</strong> issue is added to the cluster. You can set this value
                to <strong>-1</strong> to ignore this issue altogether. This setting also includes a
                time range for how long this problem must persist before adding an issue to the cluster.
                <br>
                <em class="text-muted">
                  The default for this setting is <strong>0 packets</strong> for
                  <strong>10 seconds</strong>.
                </em>
              </li>
              <li>
                <strong>Remove all issues after</strong> controls when an issue is removed if it has
                not occurred again. The issue is removed from the cluster after this time expires as
                long as the issue has not occurred again.
                <br>
                <em class="text-muted">
                  The default for this setting is <strong>60 minutes</strong>.
                </em>
              </li>
              <li>
                <strong>Remove acknowledged issues after</strong> controls when an acknowledged issue
                is removed. The issue is removed from the cluster after this time expires
                (so you don't have to remove issues manually with the trashcan button).
                <br>
                <em class="text-muted">
                  The default for this setting is <strong>15 minutes</strong>.
                </em>
              </li>
            </ul>
            <!-- /general -->

            <!-- password -->
            <h6 id="password">
              <span class="fa fa-fw fa-lock">
              </span>&nbsp;
              Password
            </h6>
            <p>
              The password section allows a user to update the Parliament password or create
              a new password if the Parliament was started without one.
            </p>
            <!-- /password -->

            <!-- notifiers -->
            <h6 id="notifiers">
              <span class="fa fa-fw fa-bell">
              </span>&nbsp;
              Notifiers
            </h6>
            <p>
              The notifiers section provides the ability to configure alerts for your Parliament.
              Users can be alerted via:
            </p>
            <ol>
              <li>
                Slack
              </li>
              <li>
                Twilio
              </li>
              <li>
                Email
              </li>
            </ol>
            <p>
              <strong>Note:</strong>
              Each notifier can be configured to alert on different types of issues.
            </p>
            <!-- /notifiers -->
          </div>
          <!-- /settings -->

        </div>

      </div>
    </div>
  </div> <!-- /help content -->

</template>

<script>
export default {
  name: 'Help'
};
</script>

<style scoped>
/* help navigation */
.help-content div.nav-pills {
  min-width: 150px;
  border: 1px solid var(--color-gray);
  border-radius: 4px;
  position: sticky;
  top: 70px;
  height: calc(100vh - 85px);
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
</style>
