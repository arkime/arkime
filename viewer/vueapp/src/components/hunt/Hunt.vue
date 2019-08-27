<template>

  <div class="packet-search-page ml-2 mr-2">

    <!-- search navbar -->
    <moloch-search
      v-if="user.settings"
      :start="sessionsQuery.start"
      :timezone="user.settings.timezone"
      :hide-actions="true"
      :hide-interval="true"
      @changeSearch="cancelAndLoad(true)">
    </moloch-search> <!-- /search navbar -->

    <div>&nbsp;</div>

    <!-- hunt create navbar -->
    <form class="hunt-create-navbar">
      <div class="mt-1 ml-1 mr-1">
        <button type="button"
          v-if="!createFormOpened"
          @click="createFormOpened = true"
          class="btn btn-theme-tertiary btn-sm pull-right">
          Create a packet search job
        </button>
        <span v-if="loadingSessions">
          <div class="mt-1" style="display:inline-block;">
            <span class="fa fa-spinner fa-spin fa-fw">
            </span>
            Loading sessions...
          </div>
          <button type="button"
            class="btn btn-warning btn-sm ml-3"
            @click="cancelAndLoad">
            <span class="fa fa-ban">
            </span>&nbsp;
            cancel
          </button>
        </span>
        <span v-else-if="loadingSessionsError">
          <div class="mt-1" style="display:inline-block;">
            <span class="fa fa-exclamation-triangle fa-fw">
            </span>
            {{ loadingSessionsError }}
          </div>
        </span>
        <span v-else-if="!loadingSessions && !loadingSessionsError">
          <div class="mt-1" style="display:inline-block;">
            <span class="fa fa-info-circle fa-fw">
            </span>&nbsp;
            Creating a new packet search job will search the packets of
            <strong>
              {{ sessions.recordsFiltered | commaString }}
            </strong>
            sessions.
          </div>
        </span>
      </div>
    </form> <!-- /hunt create navbar -->

    <!-- loading overlay -->
    <moloch-loading
      v-if="loading">
    </moloch-loading> <!-- /loading overlay -->

    <!-- packet search jobs content -->
    <div class="packet-search-content ml-2 mr-2">

      <!-- create new packet search job -->
      <div class="mb-3">
        <transition name="slide">
          <div v-if="createFormOpened"
            class="card">
            <form class="card-body"
              @keyup.enter="createJob">
              <div class="row">
                <div class="col-12">
                  <div class="alert"
                    :class="{'alert-info':sessions.recordsFiltered < huntWarn || !sessions.recordsFiltered,'alert-danger':sessions.recordsFiltered >= huntWarn}">
                    <em v-if="sessions.recordsFiltered > huntWarn && !loadingSessions">
                      That's a lot of sessions, this job will take a while.
                      <strong>
                        Proceed with caution.
                      </strong>
                      <br>
                    </em>
                    <em v-if="loadingSessions">
                      <span class="fa fa-spinner fa-spin fa-fw">
                      </span>&nbsp;
                      Wait for session totals to be calculated.
                      <br>
                    </em>
                    <span v-if="!loadingSessions">
                    <span class="fa fa-exclamation-triangle fa-fw">
                      </span>&nbsp;
                      Make sure your sessions search above contains only the sessions that
                      you want in your packet search!
                    </span>
                  </div>
                </div>
              </div>
              <div class="row">
                <div class="form-group col-lg-4 col-md-12">
                  <!-- packet search job name -->
                  <div class="input-group input-group-sm">
                    <span class="input-group-prepend cursor-help"
                      v-b-tooltip.hover
                      title="Give your packet search job a short name (multiple jobs can have the same name)">
                      <span class="input-group-text">
                        Name
                      </span>
                    </span>
                    <input type="text"
                      v-model="jobName"
                      v-focus-input="true"
                      placeholder="Name your packet search job"
                      class="form-control"
                      maxlength="40"
                    />
                  </div> <!-- /packet search job name -->
                </div>
                <!-- packet search size -->
                <div class="form-group col-lg-4 col-md-12">
                  <div class="input-group input-group-sm">
                    <span class="input-group-prepend">
                      <span class="input-group-text">
                        Max number of packets to examine per session
                      </span>
                    </span>
                    <select class="form-control"
                      v-model="jobSize"
                      style="-webkit-appearance: none;">
                      <option value="50">50 packets</option>
                      <option value="500">500 packets</option>
                      <option value="5000">5000 packets</option>
                      <option value="10000">All packets</option>
                    </select>
                  </div>
                </div> <!-- /packet search size -->
                <!-- notifier -->
                <div class="form-group col-lg-4 col-md-12">
                  <div class="input-group input-group-sm">
                    <span class="input-group-prepend cursor-help"
                      v-b-tooltip.hover
                      title="Notifies upon completion">
                      <span class="input-group-text">
                        Notify
                      </span>
                    </span>
                    <select class="form-control"
                      v-model="jobNotifier"
                      style="-webkit-appearance: none;">
                      <option value=undefined>none</option>
                      <option v-for="notifier in notifiers"
                        :key="notifier.name"
                        :value="notifier.name">
                        {{ notifier.name }} ({{ notifier.type }})
                      </option>
                    </select>
                  </div>
                </div> <!-- /notifier -->
              </div>
              <div class="row">
                <!-- packet search text & text type -->
                <div class="form-group col-lg-6 col-md-12">
                  <div class="input-group input-group-sm">
                    <span class="input-group-prepend cursor-help"
                      v-b-tooltip.hover
                      title="Search for this text in packets">
                      <span class="input-group-text">
                        <span class="fa fa-search">
                        </span>
                      </span>
                    </span>
                    <input type="text"
                      v-model="jobSearch"
                      placeholder="Search packets for"
                      class="form-control"
                    />
                  </div>
                  <div class="form-check form-check-inline">
                    <input class="form-check-input"
                      :checked="jobSearchType === 'ascii'"
                      @click="setJobSearchType('ascii')"
                      type="radio"
                      id="ascii"
                      value="ascii"
                      name="packetSearchTextType"
                    />
                    <label class="form-check-label"
                      for="ascii">
                      ascii
                    </label>
                  </div>
                  <div class="form-check form-check-inline">
                    <input class="form-check-input"
                      :checked="jobSearchType === 'asciicase'"
                      @click="setJobSearchType('asciicase')"
                      type="radio"
                      id="asciicase"
                      value="asciicase"
                      name="packetSearchTextType"
                    />
                    <label class="form-check-label"
                      for="asciicase">
                      ascii (case sensitive)
                    </label>
                  </div>
                  <div class="form-check form-check-inline">
                    <input class="form-check-input"
                      :checked="jobSearchType === 'hex'"
                      @click="setJobSearchType('hex')"
                      type="radio"
                      id="hex"
                      value="hex"
                      name="packetSearchTextType"
                    />
                    <label class="form-check-label"
                      for="hex">
                      hex
                    </label>
                  </div>
                  <div class="form-check form-check-inline">
                    <input class="form-check-input"
                      :checked="jobSearchType === 'regex'"
                      @click="setJobSearchType('regex')"
                      type="radio"
                      id="regex"
                      value="regex"
                      name="packetSearchTextType"
                    />
                    <label class="form-check-label"
                      for="regex">
                      regex
                    </label>
                  </div>
                  <div class="form-check form-check-inline">
                    <input class="form-check-input"
                      :checked="jobSearchType === 'hexregex'"
                      @click="setJobSearchType('hexregex')"
                      type="radio"
                      id="hexregex"
                      value="hexregex"
                      name="packetSearchTextType"
                    />
                    <label class="form-check-label"
                      for="hexregex">
                      hex regex
                    </label>
                  </div>
                </div> <!-- /packet search text & text type -->
                <!-- packet search direction -->
                <div class="form-group col-lg-3 col-md-12">
                  <div class="form-check">
                    <input class="form-check-input"
                      :checked="jobSrc"
                      @click="jobSrc = !jobSrc"
                      type="checkbox"
                      id="src"
                      value="src"
                    />
                    <label class="form-check-label"
                      for="src">
                      search src packets
                    </label>
                  </div>
                  <div class="form-check">
                    <input class="form-check-input"
                      :checked="jobDst"
                      @click="jobDst = !jobDst"
                      type="checkbox"
                      id="dst"
                      value="dst"
                    />
                    <label class="form-check-label"
                      for="dst">
                      search dst packets
                    </label>
                  </div>
                </div> <!-- /packet search direction -->
                <!-- packet search type -->
                <div class="form-group col-lg-3 col-md-12">
                  <div class="form-check">
                    <input class="form-check-input"
                      :checked="jobType === 'raw'"
                      @click="setJobType('raw')"
                      type="radio"
                      id="raw"
                      value="raw"
                      name="packetSearchType"
                    />
                    <label class="form-check-label"
                      for="raw">
                      search raw packets
                    </label>
                  </div>
                  <div class="form-check">
                    <input class="form-check-input"
                      :checked="jobType === 'reassembled'"
                      @click="setJobType('reassembled')"
                      type="radio"
                      id="reassembled"
                      value="reassembled"
                      name="packetSearchType"
                    />
                    <label class="form-check-label"
                      for="reassembled">
                      search reassembled packets
                    </label>
                  </div>
                </div> <!-- /packet search type -->
              </div>
              <div class="row">
                <div class="col-12 mt-1">
                  <div v-if="createFormError"
                    class="pull-left alert alert-danger alert-sm">
                    <span class="fa fa-exclamation-triangle">
                    </span>&nbsp;
                    {{ createFormError }}
                  </div>
                  <!-- create search job button -->
                  <button type="button"
                    @click="createJob"
                    :disabled="loadingSessions"
                    class="pull-right btn btn-theme-tertiary pull-right ml-1">
                    <span class="fa fa-plus fa-fw">
                    </span>&nbsp;
                    Create
                  </button> <!-- /create search job button -->
                  <!-- cancel create search job button -->
                  <button type="button"
                    @click="cancelCreateForm"
                    class="pull-right btn btn-warning pull-right">
                    <span class="fa fa-ban fa-fw">
                    </span>&nbsp;
                    Cancel
                  </button> <!-- /cancel create search job button -->
                </div>
              </div>
            </form>
          </div>
        </transition>
      </div> <!-- /create new packet search job -->

      <!-- running job -->
      <transition name="slide">
        <div v-if="runningJob"
          class="card mb-3">
          <div class="card-body">
            <h5 class="card-title">
              Running Hunt Job:
              {{ runningJob.name }} by
              {{ runningJob.userId }}
              <span class="pull-right"
                v-if="user.userId === runningJob.userId || user.createEnabled">
                <button
                  @click="removeJob(runningJob, 'results')"
                  type="button"
                  v-b-tooltip.hover
                  title="Cancel and remove this job"
                  class="ml-1 pull-right btn btn-sm btn-danger">
                  <span class="fa fa-trash-o fa-fw">
                  </span>
                </button>
                <button type="button"
                  @click="openSessions(runningJob)"
                  v-if="runningJob.matchedSessions"
                  :id="`openresults${runningJob.id}`"
                  class="ml-1 pull-right btn btn-sm btn-theme-primary">
                  <span class="fa fa-folder-open fa-fw">
                  </span>
                </button>
                <b-tooltip v-if="runningJob.matchedSessions"
                  :target="`openresults${runningJob.id}`">
                  Open <strong>partial</strong> results in a new Sessions tab.
                  <br>
                  <strong>Note:</strong> ES takes a while to update sessions, so your results
                  might take a minute to show up.
                </b-tooltip>
                <button @click="pauseJob(runningJob)"
                  type="button"
                  v-b-tooltip.hover
                  title="Pause this job"
                  class="pull-right btn btn-sm btn-warning">
                  <span class="fa fa-pause fa-fw">
                  </span>
                </button>
              </span>
            </h5>
            <div class="card-text">
              <div class="row">
                <div class="col">
                  <toggle-btn
                    v-if="user.userId === runningJob.userId || user.createEnabled"
                    :opened="runningJob.expanded"
                    @toggle="toggleJobDetail(runningJob)">
                  </toggle-btn>
                  <div class="progress cursor-help"
                    id="runningJob"
                    v-b-tooltip.hover
                    style="height:26px;"
                    :class="{'progress-toggle':user.userId === runningJob.userId || user.createEnabled}">
                    <div class="progress-bar bg-success progress-bar-striped progress-bar-animated"
                      role="progressbar"
                      :style="{width: runningJob.progress + '%'}"
                      :aria-valuenow="runningJob.progress"
                      aria-valuemin="0"
                      aria-valuemax="100">
                      {{ runningJob.progress | round(1) }}%
                    </div>
                  </div>
                  <b-tooltip target="runningJob">
                    <div class="mt-2">
                      Found <strong>{{ runningJob.matchedSessions | commaString }}</strong> sessions
                      <span v-if="user.userId === runningJob.userId || user.createEnabled">
                        matching <strong>{{ runningJob.search }}</strong> ({{ runningJob.searchType }})
                      </span>
                      out of <strong>{{ runningJob.searchedSessions | commaString }}</strong>
                      sessions searched.
                      (Still need to search
                      <strong>{{ (runningJob.totalSessions - runningJob.searchedSessions) | commaString }}</strong>
                      of <strong>{{ runningJob.totalSessions }}</strong>
                      total sessions.)
                    </div>
                  </b-tooltip>
                </div>
              </div>
              <transition name="grow">
                <div v-if="runningJob.expanded"
                  class="mt-3">
                  <div class="mt-2">
                    <span class="fa fa-eye">
                    </span>&nbsp;
                    Found <strong>{{ runningJob.matchedSessions | commaString }}</strong> sessions
                    matching <strong>{{ runningJob.search }}</strong> ({{ runningJob.searchType }})
                    out of <strong>{{ runningJob.searchedSessions | commaString }}</strong>
                    sessions searched.
                    (Still need to search
                    <strong>{{ (runningJob.totalSessions - runningJob.searchedSessions) | commaString }}</strong>
                    of <strong>{{ runningJob.totalSessions }}</strong>
                    total sessions.)
                  </div>
                  <div class="row">
                    <div class="col-12">
                      <span class="fa fa-clock-o fa-fw">
                      </span>&nbsp;
                      Created:
                      <strong>
                        {{ runningJob.created * 1000 | timezoneDateString(user.settings.timezone, false) }}
                      </strong>
                    </div>
                  </div>
                  <div v-if="runningJob.lastUpdated"
                    class="row">
                    <div class="col-12">
                      <span class="fa fa-clock-o fa-fw">
                      </span>&nbsp;
                      Last Updated:
                      <strong>
                        {{ runningJob.lastUpdated * 1000 | timezoneDateString(user.settings.timezone, false) }}
                      </strong>
                    </div>
                  </div>
                  <div class="row"
                    v-if="runningJob.notifier">
                    <div class="col-12">
                      <span class="fa fa-bell fa-fw">
                      </span>&nbsp;
                      Notifying: {{ runningJob.notifier }}
                    </div>
                  </div>
                  <div class="row">
                    <div class="col-12">
                      <span class="fa fa-id-card fa-fw">
                      </span>&nbsp;
                      Hunt Job ID: {{ runningJob.id }}
                    </div>
                  </div>
                  <div class="row">
                    <div class="col-12">
                      <span class="fa fa-search fa-fw">
                      </span>&nbsp;
                      Examining
                      <strong v-if="runningJob.size > 0">{{ runningJob.size }}</strong>
                      <strong v-else>all</strong>
                      <strong>{{ runningJob.type }}</strong>
                      <strong v-if="runningJob.src">source</strong>
                      <span v-if="runningJob.src && runningJob.dst">
                        and
                      </span>
                      <strong v-if="runningJob.dst">destination</strong>
                      packets per session
                    </div>
                  </div>
                  <div v-if="runningJob.query.expression"
                    class="row">
                    <div class="col-12">
                      <span class="fa fa-search fa-fw">
                      </span>&nbsp;
                      The sessions query expression was:
                      <strong>{{ runningJob.query.expression }}</strong>
                    </div>
                  </div>
                  <div class="row">
                    <div class="col-12">
                      <span class="fa fa-clock-o fa-fw">
                      </span>&nbsp;
                      The sessions query time range was from
                      <strong>{{ runningJob.query.startTime * 1000 | timezoneDateString(user.settings.timezone, false) }}</strong>
                      to
                      <strong>{{ runningJob.query.stopTime * 1000 | timezoneDateString(user.settings.timezone, false) }}</strong>
                    </div>
                  </div>
                  <template v-if="runningJob.errors">
                    <div v-for="(error, index) in runningJob.errors"
                      :key="index"
                      class="row text-danger">
                      <div class="col-12">
                        <span class="fa fa-exclamation-triangle">
                        </span>&nbsp;
                        <span v-if="error.time">
                          {{ error.time * 1000 | timezoneDateString(user.settings.timezone, false) }}
                        </span>
                        <span v-if="error.node">
                          ({{ error.node }} node)
                        </span>
                        <span v-if="error.time || error.node">
                          -
                        </span>
                        {{ error.value }}
                      </div>
                    </div>
                  </template>
                </div>
              </transition>
            </div>
          </div>
        </div>
      </transition> <!-- /running job -->

      <h4 v-if="results.length">
        <span class="fa fa-list-ol">
        </span>&nbsp;
        Hunt Job Queue
      </h4>

      <!-- hunt job queue errors -->
      <div v-if="queuedListError"
        class="alert alert-danger">
        {{ queuedListError }}
      </div>
      <div v-if="queuedListLoadingError"
        class="alert alert-danger">
        Error loading hunt job queue:
        {{ queuedListLoadingError }}
      </div> <!-- /hunt job queue errors -->

      <table v-if="results.length"
        class="table table-sm table-striped mb-4">
        <thead>
          <tr>
            <th width="40px">&nbsp;</th>
            <th>
              Status
            </th>
            <th>
              Matches
            </th>
            <th>
              Name
            </th>
            <th>
              User
            </th>
            <th>
              Search text
            </th>
            <th>
              Notify
            </th>
            <th>
              Created
            </th>
            <th>
              ID
            </th>
            <th width="140px">&nbsp;</th>
          </tr>
        </thead>
        <transition-group name="list"
          tag="tbody">
          <!-- packet search jobs -->
          <template v-for="job in results">
            <tr :key="`${job.id}-row`">
              <td>
                <toggle-btn
                  v-if="user.userId === job.userId || user.createEnabled"
                  :opened="job.expanded"
                  @toggle="toggleJobDetail(job)">
                </toggle-btn>
              </td>
              <td>
                <span v-if="job.status === 'running'"
                  v-b-tooltip.hover
                  title="Starting"
                  class="fa fa-play-circle fa-fw cursor-help">
                </span>
                <span v-else-if="job.status === 'paused'"
                  v-b-tooltip.hover
                  title="Paused"
                  class="fa fa-pause fa-fw cursor-help">
                </span>
                <span v-else-if="job.status === 'queued'"
                  v-b-tooltip.hover
                  :title="`Queued (#${job.queueCount} in the queue)`"
                  class="fa fa-clock-o fa-fw cursor-help">
                </span>
                <span v-else-if="job.status === 'finished'"
                  v-b-tooltip.hover
                  title="Finished"
                  class="fa fa-check fa-fw cursor-help">
                </span>
                &nbsp;
                <span class="badge badge-secondary cursor-help"
                  :id="`jobmatches${job.id}`">
                  {{ ((job.searchedSessions / job.totalSessions) * 100) | round(1) }}%
                </span>
                <b-tooltip :target="`jobmatches${job.id}`">
                  Found {{ job.matchedSessions | commaString }} out of {{ job.searchedSessions | commaString }} sessions searched.
                  <div v-if="job.status !== 'finished'">
                    Still need to search {{ (job.totalSessions - job.searchedSessions) | commaString }} sessions.
                  </div>
                </b-tooltip>
                <span v-if="job.errors && job.errors.length"
                  class="badge badge-danger cursor-help">
                  <span class="fa fa-exclamation-triangle"
                    v-b-tooltip.hover
                    title="Errors were encountered while running this hunt job. Open the job to view the error details.">
                  </span>
                </span>
              </td>
              <td>
                {{ job.matchedSessions | commaString }}
              </td>
              <td>
                {{ job.name }}
              </td>
              <td>
                {{ job.userId }}
              </td>
              <td>
                <span v-if="user.userId === job.userId || user.createEnabled">
                  {{ job.search }} ({{ job.searchType }})
                </span>
              </td>
              <td>
                {{ job.notifier }}
              </td>
              <td>
                {{ job.created * 1000 | timezoneDateString(user.settings.timezone, false) }}
              </td>
              <td>
                <span v-if="user.userId === job.userId || user.createEnabled">
                  {{ job.id }}
                </span>
              </td>
              <td>
                <span v-if="user.userId === job.userId || user.createEnabled">
                  <button
                    @click="removeJob(job, 'results')"
                    type="button"
                    v-b-tooltip.hover
                    title="Remove this job from history"
                    class="ml-1 pull-right btn btn-sm btn-danger">
                    <span class="fa fa-trash-o fa-fw">
                    </span>
                  </button>
                  <button type="button"
                    @click="openSessions(job)"
                    v-if="job.matchedSessions"
                    :id="`openresults${job.id}`"
                    class="ml-1 pull-right btn btn-sm btn-theme-primary">
                    <span class="fa fa-folder-open fa-fw">
                    </span>
                  </button>
                  <b-tooltip v-if="job.matchedSessions"
                    :target="`openresults${job.id}`">
                    Open <strong>partial</strong> results in a new Sessions tab.
                    <br>
                    <strong>Note:</strong> ES takes a while to update sessions, so your results
                    might take a minute to show up.
                  </b-tooltip>
                  <button v-if="job.status === 'running' || job.status === 'queued'"
                    @click="pauseJob(job)"
                    type="button"
                    v-b-tooltip.hover
                    title="Pause this job"
                    class="pull-right btn btn-sm btn-warning">
                    <span class="fa fa-pause fa-fw">
                    </span>
                  </button>
                  <button v-if="job.status === 'paused'"
                    @click="playJob(job)"
                    type="button"
                    v-b-tooltip.hover
                    title="Play this job"
                    class="pull-right btn btn-sm btn-theme-secondary">
                    <span class="fa fa-play fa-fw">
                    </span>
                  </button>
                </span>
              </td>
            </tr>
            <tr :key="`${job.id}-detail`"
              v-if="job.expanded">
              <td colspan="10">
                <div class="row">
                  <div class="col-12">
                    This hunt is
                    <strong>{{ job.status }}</strong>
                  </div>
                </div>
                <div v-if="job.lastUpdated"
                  class="row">
                  <div class="col-12">
                    This hunt was last updated at:
                    <strong>
                      {{ job.lastUpdated * 1000 | timezoneDateString(user.settings.timezone, false) }}
                    </strong>
                  </div>
                </div>
                <div class="row">
                  <div class="col-12">
                    Examining
                    <strong v-if="job.size > 0">{{ job.size }}</strong>
                    <strong v-else>all</strong>
                    <strong>{{ job.type }}</strong>
                    <strong v-if="job.src">source</strong>
                    <span v-if="job.src && job.dst">
                      and
                    </span>
                    <strong v-if="job.dst">destination</strong>
                    packets per session
                  </div>
                </div>
                <div class="row">
                  <div class="col-12">
                    Found
                    <strong>{{ job.matchedSessions | commaString }}</strong> of
                    <strong>{{ job.searchedSessions | commaString }}</strong>
                    searched sessions out of
                    <strong>{{ job.totalSessions }}</strong>
                    total sessions to search
                  </div>
                </div>
                <div v-if="job.query.expression"
                  class="row">
                  <div class="col-12">
                    The sessions query expression was:
                    <strong>{{ job.query.expression }}</strong>
                  </div>
                </div>
                <div class="row">
                  <div class="col-12">
                    The sessions query time range was from
                    <strong>{{ job.query.startTime * 1000 | timezoneDateString(user.settings.timezone, false) }}</strong>
                    to
                    <strong>{{ job.query.stopTime * 1000 | timezoneDateString(user.settings.timezone, false) }}</strong>
                  </div>
                </div>
                <template v-if="job.errors">
                  <div v-for="(error, index) in job.errors"
                    :key="index"
                    class="row text-danger">
                    <div class="col-12">
                      <span class="fa fa-exclamation-triangle">
                      </span>&nbsp;
                      <span v-if="error.time">
                        {{ error.time * 1000 | timezoneDateString(user.settings.timezone, false) }}
                      </span>
                      <span v-if="error.node">
                        ({{ error.node }} node)
                      </span>
                      <span v-if="error.time || error.node">
                        -
                      </span>
                      {{ error.value }}
                    </div>
                  </div>
                </template>
              </td>
            </tr>
          </template> <!-- /packet search jobs -->
        </transition-group>
      </table>

      <h4>
        <span class="fa fa-clock-o">
        </span>&nbsp;
        Hunt Job History
      </h4>

      <!-- hunt job history errors -->
      <div v-if="historyListError"
        class="alert alert-danger">
        {{ historyListError }}
      </div>
      <div v-if="historyListLoadingError"
        class="alert alert-danger">
        Error loading hunt job history:
        {{ historyListLoadingError }}
      </div> <!-- /hunt job history errors -->

      <div v-if="!historyListLoadingError"
        class="row form-inline">
        <div class="col-12">
        <!-- job history paging -->
        <moloch-paging
          class="pull-right ml-2"
          :records-total="historyResults.recordsTotal"
          :records-filtered="historyResults.recordsFiltered"
          @changePaging="changePaging">
        </moloch-paging> <!-- /job history paging -->
          <!-- search packet search jobs -->
          <div class="input-group input-group-sm">
            <span class="input-group-prepend cursor-help">
              <span class="input-group-text">
                <span class="fa fa-search">
                </span>
              </span>
            </span>
            <input type="text"
              v-model="query.searchTerm"
              @input="debounceSearch"
              placeholder="Search your packet search job history"
              class="form-control"
            />
            <span class="input-group-append">
              <button type="button"
                @click="clear"
                :disabled="!query.searchTerm"
                class="btn btn-outline-secondary btn-clear-input">
                <span class="fa fa-close">
                </span>
              </button>
            </span>
          </div> <!-- /search packet search jobs -->
        </div>
      </div>

      <table class="table table-sm table-striped">
        <thead>
          <tr>
            <th width="40px">&nbsp;</th>
            <th class="cursor-pointer"
              @click="columnClick('status')">
              Status
              <span v-show="query.sortField === 'status' && !query.desc" class="fa fa-sort-asc"></span>
              <span v-show="query.sortField === 'status' && query.desc" class="fa fa-sort-desc"></span>
              <span v-show="query.sortField !== 'status'" class="fa fa-sort"></span>
            </th>
            <th>
              Matches
            </th>
            <th class="cursor-pointer"
              @click="columnClick('name')">
              Name
              <span v-show="query.sortField === 'name' && !query.desc" class="fa fa-sort-asc"></span>
              <span v-show="query.sortField === 'name' && query.desc" class="fa fa-sort-desc"></span>
              <span v-show="query.sortField !== 'name'" class="fa fa-sort"></span>
            </th>
            <th class="cursor-pointer"
              @click="columnClick('userId')">
              User
              <span v-show="query.sortField === 'userId' && !query.desc" class="fa fa-sort-asc"></span>
              <span v-show="query.sortField === 'userId' && query.desc" class="fa fa-sort-desc"></span>
              <span v-show="query.sortField !== 'userId'" class="fa fa-sort"></span>
            </th>
            <th>
              Search text
            </th>
            <th>
              Notify
            </th>
            <th class="cursor-pointer"
              @click="columnClick('created')">
              Created
              <span v-show="query.sortField === 'created' && !query.desc" class="fa fa-sort-asc"></span>
              <span v-show="query.sortField === 'created' && query.desc" class="fa fa-sort-desc"></span>
              <span v-show="query.sortField !== 'created'" class="fa fa-sort"></span>
            </th>
            <th>
              ID
            </th>
            <th width="140px">&nbsp;</th>
          </tr>
        </thead>
        <transition-group name="list"
          tag="tbody">
          <!-- packet search jobs -->
          <template v-for="(job, index) in historyResults.data">
            <tr :key="`${job.id}-row`">
              <td>
                <toggle-btn
                  v-if="user.userId === job.userId || user.createEnabled"
                  :opened="job.expanded"
                  @toggle="toggleJobDetail(job)">
                </toggle-btn>
              </td>
              <td>
                <span v-if="job.status === 'running'"
                  v-b-tooltip.hover
                  title="Running"
                  class="fa fa-spinner fa-spin fa-fw cursor-help">
                </span>
                <span v-else-if="job.status === 'paused'"
                  v-b-tooltip.hover
                  title="Paused"
                  class="fa fa-pause fa-fw cursor-help">
                </span>
                <span v-else-if="job.status === 'queued'"
                  v-b-tooltip.hover
                  title="Queued"
                  class="fa fa-clock-o fa-fw cursor-help">
                </span>
                <span v-else-if="job.status === 'finished'"
                  v-b-tooltip.hover
                  title="Finished"
                  class="fa fa-check fa-fw cursor-help">
                </span>
                &nbsp;
                <span class="badge badge-secondary cursor-help"
                  :id="`jobmatches${job.id}`">
                  {{ ((job.searchedSessions / job.totalSessions) * 100) | round(1) }}%
                </span>
                <b-tooltip :target="`jobmatches${job.id}`">
                  Found {{ job.matchedSessions | commaString }} out of {{ job.searchedSessions | commaString }} sessions searched.
                  <div v-if="job.status !== 'finished'">
                    Still need to search {{ (job.totalSessions - job.searchedSessions) | commaString }} sessions.
                  </div>
                </b-tooltip>
                <span v-if="job.errors && job.errors.length"
                  class="badge badge-danger cursor-help">
                  <span class="fa fa-exclamation-triangle"
                    v-b-tooltip.hover
                    title="Errors were encountered while running this hunt job. Open the job to view the error details.">
                  </span>
                </span>
              </td>
              <td>
                {{ job.matchedSessions | commaString }}
              </td>
              <td>
                {{ job.name }}
              </td>
              <td>
                {{ job.userId }}
              </td>
              <td>
                <span v-if="user.userId === job.userId || user.createEnabled">
                  {{ job.search }} ({{ job.searchType }})
                </span>
              </td>
              <td>
                {{ job.notifier }}
              </td>
              <td>
                {{ job.created * 1000 | timezoneDateString(user.settings.timezone, false) }}
              </td>
              <td>
                <span v-if="user.userId === job.userId || user.createEnabled">
                  {{ job.id }}
                </span>
              </td>
              <td>
                <span v-if="user.userId === job.userId || user.createEnabled">
                  <button
                    @click="removeJob(job, 'historyResults')"
                    type="button"
                    v-b-tooltip.hover
                    title="Remove this job from history"
                    class="ml-1 pull-right btn btn-sm btn-danger">
                    <span class="fa fa-trash-o fa-fw">
                    </span>
                  </button>
                  <button type="button"
                    @click="openSessions(job)"
                    v-if="job.matchedSessions"
                    :id="`openresults${index}`"
                    class="ml-1 pull-right btn btn-sm btn-theme-primary">
                    <span class="fa fa-folder-open fa-fw">
                    </span>
                  </button>
                  <b-tooltip v-if="job.matchedSessions"
                    :target="`openresults${index}`">
                    Open results in a new Sessions tab.
                    <br>
                    <strong>Note:</strong> ES takes a while to update sessions, so your results
                    might take a minute to show up.
                  </b-tooltip>
                  <button type="button"
                    @click="rerunJob(job)"
                    v-b-tooltip.hover
                    title="Rerun this hunt job using the current time frame and search criteria."
                    class="ml-1 pull-right btn btn-sm btn-theme-secondary">
                    <span class="fa fa-refresh fa-fw">
                    </span>
                  </button>
                </span>
              </td>
            </tr>
            <tr :key="`${job.id}-detail`"
              v-if="job.expanded">
              <td colspan="10">
                <div class="row">
                  <div class="col-12">
                    This hunt is
                    <strong>{{ job.status }}</strong>
                  </div>
                </div>
                <div v-if="job.lastUpdated"
                  class="row">
                  <div class="col-12">
                    This hunt was last updated at:
                    <strong>
                      {{ job.lastUpdated * 1000 | timezoneDateString(user.settings.timezone, false) }}
                    </strong>
                  </div>
                </div>
                <div class="row">
                  <div class="col-12">
                    Examining
                    <strong v-if="job.size > 0">{{ job.size }}</strong>
                    <strong v-else>all</strong>
                    <strong>{{ job.type }}</strong>
                    <strong v-if="job.src">source</strong>
                    <span v-if="job.src && job.dst">
                      and
                    </span>
                    <strong v-if="job.dst">destination</strong>
                    packets per session
                  </div>
                </div>
                <div class="row">
                  <div class="col-12">
                    Found
                    <strong>{{ job.matchedSessions | commaString }}</strong> of
                    <strong>{{ job.searchedSessions | commaString }}</strong>
                    searched sessions out of
                    <strong>{{ job.totalSessions | commaString }}</strong>
                    total sessions to search
                  </div>
                </div>
                <div v-if="job.query.expression"
                  class="row">
                  <div class="col-12">
                    The sessions query expression was:
                    <strong>{{ job.query.expression }}</strong>
                  </div>
                </div>
                <div class="row">
                  <div class="col-12">
                    The sessions query time range was from
                    <strong>{{ job.query.startTime * 1000 | timezoneDateString(user.settings.timezone, false) }}</strong>
                    to
                    <strong>{{ job.query.stopTime * 1000 | timezoneDateString(user.settings.timezone, false) }}</strong>
                  </div>
                </div>
                <template v-if="job.errors">
                  <div v-for="(error, index) in job.errors"
                    :key="index"
                    class="row text-danger">
                    <div class="col-12">
                      <span class="fa fa-exclamation-triangle">
                      </span>&nbsp;
                      <span v-if="error.time">
                        {{ error.time * 1000 | timezoneDateString(user.settings.timezone, false) }}
                      </span>
                      <span v-if="error.node">
                        ({{ error.node }} node)
                      </span>
                      <span v-if="error.time || error.node">
                        -
                      </span>
                      {{ error.value }}
                    </div>
                  </div>
                </template>
              </td>
            </tr>
          </template> <!-- /packet search jobs -->
        </transition-group>
      </table>

      <!-- no results -->
      <div v-if="!loading && !historyResults.data.length"
        class="ml-1 mr-1">
        <div class="mb-5 info-area horizontal-center">
          <div>
            <span class="fa fa-3x text-muted-more fa-folder-open">
            </span>&nbsp;
            <span v-if="!query.searchTerm">
              There are currently no packet search jobs in the history.
              <span v-if="!results.length">
                <br>
                Click the "Create a packet search job" button above, and fill out the form to create one.
              </span>
            </span>
            <span v-else>
              There are no packet search jobs that match your search.
            </span>
          </div>
        </div>
      </div> <!-- /no results -->

    </div> <!-- /packet search jobs content -->

  </div>

</template>

<script>
// import external
import Vue from 'vue';
// import services
import SessionsService from '../sessions/SessionsService';
import ConfigService from '../utils/ConfigService';
// import components
import ToggleBtn from '../utils/ToggleBtn';
import MolochSearch from '../search/Search';
import MolochLoading from '../utils/Loading';
import MolochPaging from '../utils/Pagination';
import FocusInput from '../utils/FocusInput';
// import utils
import Utils from '../utils/utils';

let timeout;
let interval;
let respondedAt;
let pendingPromise; // save a pending promise to be able to cancel it

export default {
  name: 'PacketSearch',
  components: {
    ToggleBtn,
    MolochSearch,
    MolochLoading,
    MolochPaging
  },
  directives: { FocusInput },
  data: function () {
    return {
      queuedListError: '',
      queuedListLoadingError: '',
      historyListError: '',
      historyListLoadingError: '',
      loading: true,
      results: [], // running/queued/paused hunt jobs
      historyResults: { // finished hunt jobs
        data: [],
        recordsTotal: 0,
        recordsFiltered: 0
      },
      runningJob: undefined, // the currenty running hunt job obj
      sessions: {}, // sessions a new job applies to
      loadingSessionsError: '',
      loadingSessions: false,
      // new job search form
      createFormError: '',
      createFormOpened: false,
      // new job search default values
      jobName: '',
      jobSize: 50,
      jobSearch: '',
      jobSearchType: 'ascii',
      jobSrc: true,
      jobDst: true,
      jobType: 'raw',
      jobNotifier: undefined,
      // notifiers
      notifiers: undefined,
      // hunt limits
      huntWarn: this.$constants.MOLOCH_HUNTWARN,
      huntLimit: this.$constants.MOLOCH_HUNTLIMIT
    };
  },
  computed: {
    query: function () {
      return { // packet search job history search query
        sortField: 'created',
        desc: true,
        searchTerm: '',
        start: 0, // first item index
        length: Math.min(this.$route.query.length || 50, 10000)
      };
    },
    sessionsQuery: function () {
      return { // sessions query defaults
        length: this.$route.query.length || 50, // page length
        start: 0, // first item index
        facets: 1,
        date: this.$store.state.timeRange,
        startTime: this.$store.state.time.startTime,
        stopTime: this.$store.state.time.stopTime,
        bounding: this.$route.query.bounding || 'last',
        interval: this.$route.query.interval || 'auto',
        expression: this.$store.state.expression || undefined,
        view: this.$route.query.view || undefined
      };
    },
    user: function () {
      return this.$store.state.user;
    }
  },
  mounted: function () {
    setTimeout(() => {
      // wait for computed queries
      this.loadData();
      this.cancelAndLoad(true);
      this.loadNotifiers();
    });

    // interval to load jobs every 5 seconds
    interval = setInterval(() => {
      if (respondedAt && Date.now() - respondedAt >= 5000) {
        this.loadData();
      }
    }, 500);
  },
  methods: {
    /**
     * Cancels the pending session query (if it's still pending) and runs a new
     * query if requested
     * @param {bool} runNewQuery  Whether to run a new spigraph query after
     *                            canceling the request
     */
    cancelAndLoad: function (runNewQuery) {
      if (pendingPromise) {
        ConfigService.cancelEsTask(pendingPromise.cancelId)
          .then((response) => {
            pendingPromise.source.cancel();
            pendingPromise = null;

            if (!runNewQuery) {
              this.loadingSessions = false;
              if (!this.sessions.data) {
                // show a page error if there is no data on the page
                this.loadingSessionsError = 'You canceled the search';
              }
              return;
            }

            this.loadSessions();
          });
      } else if (runNewQuery) {
        this.loadSessions();
      }
    },
    cancelCreateForm: function () {
      this.jobName = '';
      this.jobSearch = '';
      this.createFormError = '';
      this.createFormOpened = false;
    },
    createJob: function () {
      this.createFormError = '';

      if (!this.sessions.recordsFiltered) {
        this.createFormError = 'This hunt applies to no sessions. Try searching for sessions first.';
        return;
      }
      if (this.sessions.recordsFiltered > this.huntLimit) {
        this.createFormError = `This hunt applies to too many sessions. Narrow down your session search to less than ${this.huntLimit} first.`;
        return;
      }
      if (!this.jobName) {
        this.createFormError = 'Job name required';
        return;
      }
      if (!this.jobSearch) {
        this.createFormError = 'Job search text required';
        return;
      }
      if (!this.jobSrc && !this.jobDst) {
        this.createFormError = 'The packet search job must search source or destination packets (or both)';
        return;
      }

      let newJob = {
        name: this.jobName,
        size: this.jobSize,
        search: this.jobSearch,
        searchType: this.jobSearchType,
        type: this.jobType,
        src: this.jobSrc,
        dst: this.jobDst,
        totalSessions: this.sessions.recordsFiltered,
        query: this.sessionsQuery,
        notifier: this.jobNotifier
      };

      this.axios.post('hunt', { hunt: newJob })
        .then((response) => {
          this.createFormOpened = false;
          this.jobName = '';
          this.jobSearch = '';
          this.createFormError = '';
          this.jobNotifier = undefined;
          this.loadData();
        }, (error) => {
          this.createFormError = error.text || error;
        });
    },
    removeJob: function (job, arrayName) {
      this.setErrorForList(arrayName, '');
      this.axios.delete(`hunt/${job.id}`)
        .then((response) => {
          let array = this.results;
          if (arrayName === 'historyResults') {
            array = this.historyResults.data;
          }
          for (let i = 0, len = array.length; i < len; ++i) {
            if (array[i].id === job.id) {
              array.splice(i, 1);
              return;
            }
          }
          if (job.status === 'queued') { this.calculateQueue(); }
        }, (error) => {
          this.setErrorForList(arrayName, error.text || error);
        });
    },
    pauseJob: function (job) {
      this.setErrorForList('results', '');
      this.axios.put(`hunt/${job.id}/pause`)
        .then((response) => {
          if (job.status === 'running') {
            this.loadData();
            return;
          }
          // this.$set(job, 'status', 'paused');
          this.calculateQueue();
        }, (error) => {
          this.setErrorForList('results', error.text || error);
        });
    },
    playJob: function (job) {
      this.setErrorForList('results', '');
      this.axios.put(`hunt/${job.id}/play`)
        .then((response) => {
          this.$set(job, 'status', 'queued');
          this.calculateQueue();
        }, (error) => {
          this.setErrorForList('results', error.text || error);
        });
    },
    openSessions: function (job) {
      let url = `sessions?expression=huntId == ${job.id}&stopTime=${job.query.stopTime}&startTime=${job.query.startTime}`;
      window.open(url, '_blank');
    },
    setJobSearchType: function (val) {
      this.jobSearchType = val;
    },
    setJobType: function (val) {
      this.jobType = val;
    },
    toggleJobDetail: function (job) {
      this.$set(job, 'expanded', !job.expanded);
    },
    debounceSearch: function () {
      if (timeout) { clearTimeout(timeout); }
      timeout = setTimeout(() => {
        this.loadData();
      }, 400);
    },
    clear () {
      this.query.searchTerm = undefined;
      this.loadData();
    },
    columnClick: function (name) {
      if (name === this.query.sortField) {
        // same sort field, so toggle order direction
        this.query.desc = !this.query.desc;
      } else { // new sort field, so set default order (desc)
        this.query.sortField = name;
        this.query.desc = true;
      }

      this.loadData();
    },
    changePaging: function (args) {
      this.query.start = args.start;
      this.query.length = args.length;
      this.loadData();
    },
    rerunJob: function (job) {
      this.jobSrc = job.src;
      this.jobDst = job.dst;
      this.jobSize = job.size;
      this.jobType = job.type;
      this.jobName = job.name;
      this.jobSearch = job.search;
      this.jobNotifier = job.notifier;
      this.jobSearchType = job.searchType;
      this.createFormOpened = true;
    },
    /* helper functions ---------------------------------------------------- */
    setErrorForList: function (arrayName, errorText) {
      let errorArea = 'queuedListError';
      if (arrayName === 'historyResults') {
        errorArea = 'historyListError';
      }
      this[errorArea] = errorText;
    },
    calculateQueue: function () {
      let queueCount = 1;
      for (let job of this.results) {
        if (job.status === 'queued') {
          this.$set(job, 'queueCount', queueCount);
          queueCount++;
        }
      }
    },
    loadData: function () {
      respondedAt = undefined;

      let expanded = [];
      let runningJobExpanded = this.runningJob && this.runningJob.expanded;
      if (this.results && this.results.length) {
        // save the expanded ones
        for (let result of this.results) {
          if (result.expanded) {
            expanded.push(result.id);
          }
        }
      }
      if (this.historyResults.data && this.historyResults.data.length) {
        // save the expanded ones
        for (let result of this.historyResults.data) {
          if (result.expanded) {
            expanded.push(result.id);
          }
        }
      }

      // get the hunt history
      let historyReq = this.axios.get('hunt/list', { params: { ...this.query, history: true } });
      historyReq.then((response) => {
        this.historyListLoadingError = '';

        if (expanded.length) {
          // make sure expanded ones are still expanded
          for (let result of response.data.data) {
            if (expanded.indexOf(result.id) > -1) {
              result.expanded = true;
            }
          }
        }

        this.$set(this, 'historyResults', response.data);
      }, (error) => {
        this.historyListLoadingError = error.text || error;
      });

      // get the running, queued, paused hunts
      let queuedQuery = JSON.parse(JSON.stringify(this.query));
      queuedQuery.desc = false;
      queuedQuery.length = undefined;
      queuedQuery.start = 0;
      let queueReq = this.axios.get('hunt/list', { params: queuedQuery });
      queueReq.then((response) => {
        this.queuedListLoadingError = '';

        if (expanded.length) {
          // make sure expanded ones are still expanded
          for (let result of response.data.data) {
            if (expanded.indexOf(result.id) > -1) {
              result.expanded = true;
            }
          }
        }

        this.results = response.data.data;
        this.runningJob = response.data.runningJob;
        if (this.runningJob) {
          this.$set(this.runningJob, 'expanded', runningJobExpanded);
          this.$set(
            this.runningJob,
            'progress',
            this.runningJob.searchedSessions / this.runningJob.totalSessions * 100
          );
        }
        this.calculateQueue();
      }, (error) => {
        this.queuedListLoadingError = error.text || error;
      });

      // stop loading when both requests are done
      Promise.all([historyReq, queueReq])
        .then((values) => {
          respondedAt = Date.now();
          this.loading = false;
        })
        .catch(() => {
          this.loading = false;
          respondedAt = undefined;
        });
    },
    loadSessions: function () {
      this.loadingSessions = true;
      this.loadingSessionsError = '';

      // create unique cancel id to make canel req for corresponding es task
      const cancelId = Utils.createRandomString();
      this.sessionsQuery.cancelId = cancelId;

      const source = Vue.axios.CancelToken.source();
      const cancellablePromise = SessionsService.get(this.sessionsQuery, source.token);

      // set pending promise info so it can be cancelled
      pendingPromise = { cancellablePromise, source, cancelId };

      cancellablePromise.then((response) => {
        pendingPromise = null;
        this.sessions = response.data;
        this.loadingSessions = false;
      }).catch((error) => {
        pendingPromise = null;
        this.sessions = {};
        this.loadingSessions = false;
        this.loadingSessionsError = 'Problem loading sessions. Try narrowing down your results on the sessions page first.';
      });
    },
    /* retrieves the notifiers that have been configured */
    loadNotifiers: function () {
      this.$http.get('notifiers')
        .then((response) => {
          this.notifiers = response.data;
        });
    }
  },
  beforeDestroy: function () {
    if (pendingPromise) {
      pendingPromise.source.cancel();
      pendingPromise = null;
    }

    if (interval) { clearInterval(interval); }
  }
};
</script>

<style scoped>
/* packet search page, navbar, and content styles */
.packet-search-page {
  margin-top: 36px;
}

.packet-search-content {
  margin-top: 100px;
}

.info-area {
  font-size: var(--px-xxlg);
}

form.hunt-create-navbar {
  z-index: 4;
  position: fixed;
  top: 110px;
  left: 0;
  right: 0;
  height: 40px;
  background-color: var(--color-quaternary-lightest);

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

/* offset the progress bar to accommodate toggle button */
.progress-toggle {
  margin-left: 40px;
  margin-top: -26px;
}

/* slide running job in/out animation */
.slide-leave-active {
  transition: all .3s;
}
.slide-enter-active {
  max-height: 500px;
  transition: max-height .8s;
}
.slide-enter,
.slide-leave-active {
  max-height: 0px;
}
.slide-leave {
  max-height: 500px;
}
.slide-leave-to {
  transform: translateY(-500px);
}

/* running job info animation */
.grow-leave-active {
  transition: all .3s;
}
.grow-enter-active {
  max-height: 500px;
  transition: max-height .3s;
}
.grow-enter,
.grow-leave-active {
  max-height: 0px;
}
.grow-leave {
  max-height: 500px;
}
.grow-leave-to {
  opacity: 0;
  transform: translateY(-30px);
}

/* job list animation */
.list-enter-active, .list-leave-active {
  transition: all .8s;
}
.list-enter {
  opacity: 0;
  transform: translateX(30px);
}
.list-leave-to {
  opacity: 0;
  transform: translateY(-30px);
}
.list-move {
  transition: transform .8s;
}
</style>
