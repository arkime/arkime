<template>
  <div class="help-content">
    <!-- side navbar -->
    <div class="nav nav-pills col-1">
      <a href="help#about"
        class="nav-link">
        <b-icon-question-circle-fill />&nbsp;
        About
      </a>
      <a href="help#links"
        class="nav-link">
        <b-icon-link45deg />&nbsp;
        Links
      </a>
      <a href="help#getStarted"
        class="nav-link">
        <b-icon-play />&nbsp;
        Get Started
      </a>
      <a href="help#query"
        class="nav-link">
        <b-icon-search />&nbsp;
        Query
      </a>
      <a href="help#stats"
        class="nav-link">
        <b-icon-bar-chart-fill />&nbsp;
        Stats
      </a>
      <a href="help#sources"
        class="nav-link nested">
        sources
      </a>
      <a href="help#types"
        class="nav-link nested">
        types
      </a>
      <a href="help#config"
        class="nav-link">
        <b-icon-gear-fill />&nbsp;
        Config
      </a>
      <a href="help#configWiseService"
        class="nav-link nested">
        WISE Service
      </a>
      <a href="help#configCache"
        class="nav-link nested">
        WISE Cache
      </a>
      <a href="help#configSources"
        class="nav-link nested">
        WISE Sources
      </a>
    </div> <!-- /side navbar -->

    <!-- page content -->
    <div class="mt-5 ml-4 mr-4 navbar-offset">

      <h3 id="about">
        <b-icon-question-circle-fill />&nbsp;
        About
      </h3>
      <p class="lead"><strong>
        WISE
      </strong></p>
      <p class="lead">
        WISE is a framework for integrating data feeds into Arkime. The data
        feeds can be sourced from local files, remote URLs, or commercial
        services such as OpenDNS, Emerging Threats Pro, and others. The data
        feeds can set almost any Arkime field or even create new Arkime fields.
        Think of WISE as the next and better version of the tagger plugin.
      </p>

      <hr>

      <h3 id="links">
        <b-icon-link45deg />&nbsp;
        Links
      </h3>
      <div class="row">
        <div class="col-sm-12">
          <a class="btn btn-link" href="https://arkime.com/wise">
            All About WISE</a> |
          <a class="btn btn-link" href="https://arkime.com/wisesources">
            Building WISE Sources</a> |
          <a class="btn btn-link" href="https://arkime.com/wiseapi">
            WISE API</a>
        </div>
      </div>

      <hr>

      <h3 id="getStarted">
        <b-icon-play />&nbsp;
        Get Started
      </h3>

      <p>
        <!-- TODO -->
        To get started...
      </p>

      <hr>

      <h3 id="query">
        <b-icon-search />&nbsp;
        Query
      </h3>

      <p>
        <!-- TODO -->
        The Query page is for searching for...
      </p>

      <hr>

      <h3 id="stats">
        <b-icon-bar-chart-fill />&nbsp;
        Stats
      </h3>

      <p>
        <!-- TODO -->
        The Stats page displays statistics about your WISE configuration.
      </p>

      <h6 id="sources">
        Sources
      </h6>
      <p>
        <!-- TODO -->
        The Sources tab allows you to view your WISE sources and how many
        sessions they are augmenting.
      </p>

      <h6 id="types">
        Types
      </h6>
      <p>
        <!-- TODO -->
        The Types tab allows you to view data about your WISE source types.
      </p>

      <hr>

      <h3 id="config">
        <b-icon-gear-fill />&nbsp;
        Config
      </h3>

      <p>
        The Config page allows you to configure you WISE service, cache, and
        WISE sources.
        <br>
        <strong>Note:</strong> You must enter the config pin code
        every time you make a change. This can be found in the logs created
        when running WISE. Look for <code>IMPORTANT - Config pin code is</code>
      </p>

      <h6 id="configWiseService">
        Configuring WISE Service
      </h6>
      <p>
        <!-- TODO -->
        Here's how to configure the WISE service
      </p>

      <h6 id="configCache">
        Configuring WISE Cache
      </h6>
      <p>
        <!-- TODO -->
        Here's how to configure the WISE cache
      </p>

      <h6 id="configSources">
        Configuring WISE Sources
      </h6>
      <p>
        Use this tool instead of manually creating and configuring your WISE
        sources!
      </p>

      <div class="ml-4">
        <h6>
          <b-icon-plus />&nbsp;
          Add Sources
        </h6>
        <p>
          To add a source, simply click "Add Source". From there you can
          continue to configure or any other source that has been added.
        </p>
        <h6>
          <b-icon-download />&nbsp;
          Import Config
        </h6>
        <p>
          You can also Import a JSON or INI config by clicking the "Import Config"
          Button. Paste your configuration file into the text area and continue
          to configure it.
        </p>
        <h6>
          <b-icon-pencil />&nbsp;
          Import Config
        </h6>
        <p>
          Click "Edit" to edit the WISE source file directly. The file is
          automatically reloaded if it changes and will immediately start loading
          the appropriate data into WISE.
        </p>
        <h6>
          <b-icon-eye />&nbsp;
          Import Config
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

/* help navigation */
.help-content div.nav-pills {
  display: block;
  min-width: 150px;
  border: 1px solid var(--color-gray);
  border-radius: 0 8px 8px 0;
  position: fixed;
  top: 60px;
  height: calc(100vh - 70px);
  overflow-x: hidden;
  overflow-y: auto;
  white-space: nowrap;

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

/* content offset for left nav */
.navbar-offset {
  padding-left: 150px;
  overflow-x: hidden;
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
