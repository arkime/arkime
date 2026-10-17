<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="help-content">
    <!-- side navbar -->
    <v-list
      density="compact"
      nav
      class="help-nav">
      <v-list-item
        href="help#about"
        prepend-icon="mdi-help-circle"
        title="About" />
      <v-list-item
        href="help#links"
        prepend-icon="mdi-link"
        title="Links" />
      <v-list-item
        href="help#getStarted"
        prepend-icon="mdi-play-circle"
        title="Get Started" />
      <v-list-item
        href="help#query"
        prepend-icon="mdi-magnify"
        title="Query" />
      <v-list-item
        href="help#stats"
        prepend-icon="mdi-chart-bar"
        title="Stats" />
      <v-list-item
        href="help#sources"
        title="sources"
        class="nested" />
      <v-list-item
        href="help#types"
        title="types"
        class="nested" />
      <v-list-item
        href="help#config"
        prepend-icon="mdi-cog"
        title="Config" />
      <v-list-item
        href="help#configWiseService"
        title="WISE Service"
        class="nested" />
      <v-list-item
        href="help#configCache"
        title="WISE Cache"
        class="nested" />
      <v-list-item
        href="help#configSources"
        title="WISE Sources"
        class="nested" />
    </v-list> <!-- /side navbar -->

    <!-- page content -->
    <div class="mt-5 ms-4 me-4 navbar-offset">
      <h3 id="about">
        <v-icon
          icon="mdi-help-circle"
          class="me-2" />
        About
      </h3>
      <p class="lead">
        <strong>
          WISE
        </strong>
      </p>
      <p class="lead">
        WISE is a framework for integrating data feeds into Arkime, where they
        are used to enrich the session meta data.
        The data feeds can be sourced from local files, remote URLs, or commercial
        services such as OpenDNS, Emerging Threats Pro, and others.
        The data feeds can set almost any Arkime field or even create new Arkime fields.
        Think of WISE as the next and better version of the tagger plugin.
      </p>

      <v-divider class="my-3" />

      <h3 id="links">
        <v-icon
          icon="mdi-link"
          class="me-2" />
        Links
      </h3>
      <div class="wise-help-links">
        <a href="https://arkime.com/wise">All About WISE</a> |
        <a href="https://arkime.com/wisesources">Building WISE Sources</a> |
        <a href="https://arkime.com/wiseapi">WISE API</a>
      </div>

      <v-divider class="my-3" />

      <h3 id="getStarted">
        <v-icon
          icon="mdi-play-circle"
          class="me-2" />
        Get Started
      </h3>

      <div>
        Setting up WISE to use with Arkime is a multi step process.
        <ol>
          <li>
            In config.ini you'll need to make some changes.
            Add wise.so to plugins line.
            Set wiseURL to point to the WISE host and port.
            Add wise.js to the viewerPlugins line.
            See <a
              href="https://arkime.com/wise"
              class="no-decoration"
              target="_blank">
              arkime.com/wise</a> for more options.
            Sample:
            <pre>
              plugins=wise.so
              viewerPlugins=wise.js
              wiseURL=http://localhost:8081
            </pre>
          </li>
          <li>
            You'll need to provision sources into WISE service using either the wise.ini file or the config builder
          </li>
          <li>
            You'll need to restart all the capture and viewer processes with the new config.ini
          </li>
        </ol>
      </div>

      <v-divider class="my-3" />

      <h3 id="query">
        <v-icon
          icon="mdi-magnify"
          class="me-2" />
        Query
      </h3>

      <p>
        The Query page of the WISE UI is for searching for indicators and testing what WISE knows about them.
        This is basically the same thing that capture is doing when it uses the WISE plugin, but in a user friendly way.
      </p>

      <v-divider class="my-3" />

      <h3 id="stats">
        <v-icon
          icon="mdi-chart-bar"
          class="me-2" />
        Stats
      </h3>

      <p>
        The Stats page displays statistics about your WISE configuration.
      </p>

      <h6 id="sources">
        Sources
      </h6>
      <p>
        The Sources tab allows you to view your WISE sources and how many sessions they are enriching.
      </p>

      <h6 id="types">
        Types
      </h6>
      <p>
        The Types tab allows you to view data about your WISE source types.
      </p>

      <v-divider class="my-3" />

      <h3 id="config">
        <v-icon
          icon="mdi-cog"
          class="me-2" />
        Config
      </h3>

      <p>
        The Config page allows you to configure your WISE service, cache, and WISE sources.
        <br>
        <strong>Note:</strong> You must enter the config pin code
        every time you make a change. This can be found in the logs created
        when running WISE. Look for <code>IMPORTANT - Config pin code is</code>
      </p>

      <h6 id="configWiseService">
        Configuring WISE Service
      </h6>
      <p>
        The wiseService tab allows you to configure your WISE service.
        You can use this tool instead of configuring fields in the config.ini.
      </p>

      <h6 id="configCache">
        Configuring WISE Cache
      </h6>
      <p>
        The cache tab allows you to configure how WISE should cache results
        from sources that support it. Using a redis setup is especially useful
        when there are multiple WISE servers or a large amount of results to cache.
      </p>

      <h6 id="configSources">
        Configuring WISE Sources
      </h6>
      <p>
        The rest of the tabs on this page list your WISE Sources.
        Use this tool instead of manually creating and configuring them.
      </p>

      <div class="ms-4">
        <h6>
          <v-icon
            icon="mdi-plus"
            class="me-2" />
          Add Sources
        </h6>
        <p>
          To add a source, simply click "Add Source". From there you can
          continue to configure it or any other source that has been added.
        </p>
        <h6>
          <v-icon
            icon="mdi-download"
            class="me-2" />
          Import Config
        </h6>
        <p>
          You can also Import a JSON or INI config by clicking the "Import Config"
          Button. Paste your configuration file into the text area and continue
          to configure it.
        </p>
        <h6>
          <v-icon
            icon="mdi-pencil"
            class="me-2" />
          Edit Source
        </h6>
        <p>
          Click "Edit" to edit the WISE source file directly. The file is
          automatically reloaded if it changes and will immediately start loading
          the appropriate data into WISE.
        </p>
        <h6>
          <v-icon
            icon="mdi-eye"
            class="me-2" />
          View Source
        </h6>
        <p>
          Click "Display" to view the JSON output.
        </p>
      </div>
    </div> <!-- /page content -->
  </div>
</template>

<script>
export default {
  name: 'Help'
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

/* help navigation -- fixed sidebar wrapping a Vuetify v-list */
.help-content .help-nav {
  width: 150px;
  border: 1px solid rgb(var(--v-theme-outline));
  border-radius: 0 8px 8px 0;
  position: fixed;
  top: 60px;
  height: calc(100vh - 70px);
  overflow-x: hidden;
  overflow-y: auto;
  white-space: nowrap;
  background-color: rgb(var(--v-theme-surface));
  box-shadow: 0 0 16px -2px black;
}

/* content offset for left nav */
.navbar-offset {
  padding-left: 150px;
  overflow-x: hidden;
}

.help-content .help-nav :deep(.v-list-item.nested) {
  margin-left: 2em;
  font-size: 0.85em;
  min-height: 28px;
}
</style>
