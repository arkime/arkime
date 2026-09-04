<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="help-content">
    <v-container fluid>
      <v-row class="ps-2">
        <v-col
          cols="12"
          sm="2"
          md="2"
          lg="1"
          class="help-nav-col">
          <v-list
            density="compact"
            nav
            class="help-nav">
            <v-list-item
              href="help#about"
              prepend-icon="mdi-help-circle"
              title="About" />
            <v-list-item
              href="help#dashboard"
              prepend-icon="mdi-speedometer"
              title="Dashboard" />
            <v-list-item
              href="help#issues"
              prepend-icon="mdi-alert"
              title="Issues" />
            <v-list-item
              href="help#settings"
              prepend-icon="mdi-cog-outline"
              title="Settings" />
            <v-list-item
              href="help#general"
              title="General"
              class="nested" />
            <v-list-item
              href="help#notifiers"
              title="Notifiers"
              class="nested" />
          </v-list>
        </v-col>

        <v-col class="mt-2 mb-3">
          <!-- about -->
          <h2 id="about">
            <v-icon icon="mdi-help-circle" />&nbsp;
            About
          </h2>
          <p class="lead">
            <strong>
              Parliament contains a grouped list of your Arkime clusters.
            </strong>
          </p>
          <p class="lead">
            Parliament allows users to view each Arkime cluster's status and
            navigate to each cluster.
          </p>
          <!-- /about -->

          <v-divider class="my-3" />

          <!-- dashboard -->
          <h2 id="dashboard">
            <v-icon icon="mdi-speedometer" />&nbsp;
            Parliament Dashboard
          </h2>
          <p class="lead">
            The Parliament dashboard includes links, ES health, and issues for each Arkime cluster.
          </p>
          <p>
            The dashboard page allows users to view and interact with the Arkimes in your Parliament.
            You can search for Arkimes in your Parliament, change the data refresh time
            (15 seconds is the default),
            and hover over issues and ES health statuses for more information.
          </p>
          <p>
            <strong>Once logged in</strong>, a parliamentUser can acknowledge and ignore issues for each cluster.
            A parliamentAdmin can update the Parliament when in Edit Mode. To enter this mode, toggle the switch on the top right
            (below the navbar). Now you can add, update, delete, and reorder groups and clusters in your Parliament.
          </p>
          <!-- /dashboard -->

          <v-divider class="my-3" />

          <!-- issues -->
          <h2 id="issues">
            <v-icon icon="mdi-alert" />&nbsp;
            Issues
          </h2>
          <p class="lead">
            The issues page contains a list of issues that your Parliament is experiencing.
          </p>
          <p>
            <strong>Once logged in</strong>, a parliamentUser and a parliamentAdmin
            can ignore, acknowledge, and remove acknowledged issues for every cluster.
          </p>
          <p>
            <v-icon icon="mdi-check" />&nbsp;
            <strong>Acknowledged issues</strong> will not show up on the main Parliament page,
            but will remain here (grayed out) to be removed
            (via the trashcan button or by waiting for them to be removed automatically after the set time).
          </p>
          <p>
            <v-icon icon="mdi-eye-off" />&nbsp;
            <strong>Ignored issues</strong> will not show up on the main Parliament page,
            but will remain here (grayed out) to be unignored
            (via the ignore dropdown button or automatically after the set ignore time has expired).
          </p>

          <p>
            Issue types include:
          </p>
          <ul>
            <li>
              <strong>ES Down:</strong>
              OpenSearch/Elasticsearch was unreachable
              <em class="text-medium-emphasis">(configurable in settings)</em>.
            </li>
            <li>
              <strong>Out of Date:</strong>
              The capture node has not checked in
              <em class="text-medium-emphasis">(configurable in settings)</em>.
            </li>
            <li>
              <strong>Low Packets:</strong>
              The capture node is not receiving many packets
              <em class="text-medium-emphasis">(configurable in settings)</em>.
            </li>
            <li>
              <strong>ES Red:</strong>
              The OpenSearch/Elasticsearch status returned from the cluster health check was RED.
            </li>
            <li>
              <strong>ES Dropped:</strong>
              The capture node is overloading OpenSearch/Elasticsearch and is now dropping bulk inserts.
            </li>
            <li>
              <strong>Low Disk Space:</strong>
              The capture node has low disk space. Can be configured as a percentage or GB threshold. 4% default.
              <em class="text-medium-emphasis">(configurable in settings)</em>.
            </li>
            <li>
              <strong>ES Low Disk Space:</strong>
              An OpenSearch/Elasticsearch node has low disk space. Can be configured as a percentage or GB threshold. 15% default.
              <em class="text-medium-emphasis">(configurable in settings)</em>.
            </li>
          </ul>
          <!-- /issues -->

          <v-divider class="my-3" />

          <!-- settings -->
          <h2 id="settings">
            <v-icon icon="mdi-cog-outline" />&nbsp;
            Settings
          </h2>
          <p class="lead">
            The settings page allows you to configure your Parliament for your needs.
          </p>
          <p>
            <em>
              <strong>Note:</strong> To view this page, you must be logged in as a parliamentAdmin.
            </em>
          </p>

          <div class="ms-4">
            <!-- general -->
            <h4 id="general">
              <v-icon icon="mdi-cog" />&nbsp;
              General
            </h4>
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
                <em class="text-medium-emphasis">
                  The default for this setting is <strong>30 seconds</strong>.
                </em>
              </li>
              <li>
                <strong>OpenSearch/Elasticsearch query timeout</strong> controls the maximum OpenSearch/Elasticsearch
                status query duration. If the query exceeds this time setting, an
                <strong>ES Down</strong> issue is added to the cluster.
                <br>
                <em class="text-medium-emphasis">
                  The default for this setting is <strong>5 seconds</strong>.
                </em>
              </li>
              <li>
                <strong>Low Packets Threshold</strong> controls the minimum number of packets that
                the capture node must receive. If the capture node is not receiving enough packets,
                a <strong>Low Packets</strong> issue is added to the cluster. You can set this value
                to <strong>-1</strong> to ignore this issue altogether. This setting also includes a
                time range for how long this problem must persist before adding an issue to the cluster.
                <br>
                <em class="text-medium-emphasis">
                  The default for this setting is <strong>0 packets</strong> for
                  <strong>10 seconds</strong>.
                </em>
              </li>
              <li>
                <strong>Remove all issues after</strong> controls when an issue is removed if it has
                not occurred again. The issue is removed from the cluster after this time expires as
                long as the issue has not occurred again.
                <br>
                <em class="text-medium-emphasis">
                  The default for this setting is <strong>60 minutes</strong>.
                </em>
              </li>
              <li>
                <strong>Remove acknowledged issues after</strong> controls when an acknowledged issue
                is removed. The issue is removed from the cluster after this time expires
                (so you don't have to remove issues manually with the trashcan button).
                <br>
                <em class="text-medium-emphasis">
                  The default for this setting is <strong>15 minutes</strong>.
                </em>
              </li>
            </ul>
            <!-- /general -->

            <!-- notifiers -->
            <h4 id="notifiers">
              <v-icon
                icon="mdi-bell"
                class="me-2" />
              Notifiers
            </h4>
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
        </v-col>
      </v-row>
    </v-container>
  </div> <!-- /help content -->
</template>

<script>
export default {
  name: 'Help'
};
</script>

<style scoped>
/* help navigation -- sticky sidebar wrapping a Vuetify v-list */
.help-content .help-nav-col {
  min-width: 150px;
}

.help-content .help-nav {
  border: 1px solid rgb(var(--v-theme-outline));
  border-radius: 4px;
  position: sticky;
  top: 70px;
  max-height: calc(100vh - 85px);
  overflow-x: hidden;
  overflow-y: auto;
  white-space: nowrap;
  box-shadow: 0 0 16px -2px black;
  background-color: rgb(var(--v-theme-surface));
}

.help-content .help-nav :deep(.v-list-item.nested) {
  margin-left: 2em;
  font-size: 0.85em;
  min-height: 28px;
}
</style>
