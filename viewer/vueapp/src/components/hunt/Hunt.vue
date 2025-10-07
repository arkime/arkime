<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="packet-search-page ms-2 me-2">
    <ArkimeCollapsible>
      <span class="fixed-header">
        <!-- search navbar -->
        <arkime-search
          v-if="user.settings"
          :start="sessionsQuery.start"
          :hide-actions="true"
          :hide-interval="true"
          @change-search="cancelAndLoad(true)"
          @recalc-collapse="$emit('recalc-collapse')" /> <!-- /search navbar -->

        <!-- hunt create navbar -->
        <BRow
          gutter-x="1"
          align-h="between"
          class="hunt-create-navbar ps-2 pe-2 pt-1">
          <BCol cols="auto">
            <span v-if="loadingSessions">
              <div
                class="mt-1"
                style="display:inline-block;">
                <span class="fa fa-spinner fa-spin fa-fw" />
                {{ $t('common.loading') }}
              </div>
              <button
                type="button"
                class="btn btn-warning btn-sm ms-3"
                @click="cancelAndLoad">
                <span class="fa fa-ban" />&nbsp;
                {{ $t('common.cancel') }}
              </button>
            </span>
            <span v-else-if="loadingSessionsError">
              <div
                class="mt-1"
                style="display:inline-block;">
                <span class="fa fa-exclamation-triangle fa-fw" />
                {{ loadingSessionsError }}
              </div>
            </span>
            <span v-else-if="!loadingSessions && !loadingSessionsError">
              <div
                class="mt-1"
                style="display:inline-block;">
                <span v-html="$t('hunts.createMsgHtml', { count: commaString(sessions.recordsFiltered) })" />
              </div>
            </span>
          </BCol>
          <BCol cols="auto">
            <BButton
              size="sm"
              variant="theme-tertiary"
              v-if="!createFormOpened"
              @click="createFormOpened = true">
              {{ $t('hunts.createJob') }}
            </BButton>
          </BCol>
        </BRow> <!-- /hunt create navbar -->
      </span>
    </ArkimeCollapsible>

    <!-- loading overlay -->
    <arkime-loading
      v-if="loading" /> <!-- /loading overlay -->

    <!-- configuration error -->
    <div
      v-if="nodeInfo && !nodeInfo.node"
      style="z-index: 2000;"
      class="alert alert-danger position-fixed fixed-bottom m-0 rounded-0">
      <span class="fa fa-exclamation-triangle me-2" />
      <span v-html="$t('hunts.notConfiguredHtml')" />
    </div> <!-- /configuration error -->

    <!-- permission error -->
    <div
      v-if="permissionDenied"
      class="alert alert-danger mt-4">
      <p class="mb-0">
        <span class="fa fa-exclamation-triangle fa-fw me-2" />
        <strong>{{ $t('common.permisionDenied') }}</strong>
      </p>
      <p class="mb-0">
        <span class="fa fa-info-circle fa-fw me-2" />
        <template v-if="user.roles.includes('usersAdmin')">
          {{ $t('hunts.selfEnable') }}
        </template>
        <template v-else>
          {{ $t('hunts.adminEnable') }}
        </template>
      </p>
    </div> <!-- /permission error -->

    <!-- packet search jobs content -->
    <div
      v-if="!permissionDenied"
      class="packet-search-content ms-2 me-2">
      <!-- create new packet search job -->
      <div class="mb-3">
        <transition name="slide">
          <div
            v-if="createFormOpened"
            class="card">
            <form
              class="card-body"
              @keyup.enter="createJob">
              <div class="row">
                <div class="col-12">
                  <div
                    class="alert"
                    :class="{'alert-info':sessions.recordsFiltered < huntWarn || !sessions.recordsFiltered,'alert-danger':sessions.recordsFiltered >= huntWarn}">
                    <em v-if="sessions.recordsFiltered > huntWarn && !loadingSessions">
                      <span v-html="$t('hunts.lotOfSessionsHtml')" />
                      <br>
                    </em>
                    <em v-if="loadingSessions">
                      <span class="fa fa-spinner fa-spin fa-fw me-1" />
                      {{ $t('hunts.waitForCalculation') }}
                      <br>
                    </em>
                    <span v-if="!loadingSessions">
                      <span class="fa fa-exclamation-triangle fa-fw me-1" />
                      {{ $t('hunts.doubleCheckSessions') }}
                    </span>
                    <span v-if="multiviewer">
                      <br>
                      <span class="fa fa-info-circle fa-fw me-2" />
                      {{ $t('hunts.multiViewerHtml', { cluster: selectedCluster[0] }) }}
                    </span>
                  </div>
                </div>
              </div>
              <BRow gutter-x="1">
                <BCol
                  cols="auto"
                  class="mb-2">
                  <!-- packet search job name -->
                  <BInputGroup size="sm">
                    <BInputGroupText
                      id="jobName"
                      class="cursor-help">
                      {{ $t('hunts.jobName') }}
                      <BTooltip target="jobName">
                        <span v-i18n-btip="'hunts.'" />
                      </BTooltip>
                    </BInputGroupText>
                    <input
                      type="text"
                      v-model="jobName"
                      v-focus="true"
                      class="form-control"
                      :placeholder="$t('hunts.jobNamePlaceholder')"
                      maxlength="40">
                  </BInputGroup> <!-- /packet search job name -->
                </BCol>
                <!-- packet search size -->
                <BCol>
                  <BInputGroup size="sm">
                    <BInputGroupText>
                      {{ $t('hunts.jobSize') }}
                    </BInputGroupText>
                    <BFormSelect
                      v-model="jobSize"
                      :options="[50, 500, 5000, 10000]" />
                  </BInputGroup>
                </BCol> <!-- /packet search size -->
                <!-- notifier -->
                <BCol>
                  <BInputGroup size="sm">
                    <BInputGroupText
                      id="jobNotifier"
                      class=" cursor-help">
                      {{ $t('hunts.jobNotifier') }}
                      <BTooltip target="jobNotifier">
                        <span v-i18n-btip="'hunts.'" />
                      </BTooltip>
                    </BInputGroupText>
                    <select
                      class="form-control"
                      v-model="jobNotifier"
                      style="-webkit-appearance: none;">
                      <option value="undefined">
                        none
                      </option>
                      <option
                        v-for="notifier in notifiers"
                        :key="notifier.id"
                        :value="notifier.id">
                        {{ notifier.name }} ({{ notifier.type }})
                      </option>
                    </select>
                  </BInputGroup>
                </BCol> <!-- /notifier -->
              </BRow>
              <BRow
                gutter-x="1"
                class="mb-2">
                <BCol>
                  <BInputGroup size="sm">
                    <BInputGroupText
                      class="cursor-help"
                      id="jobDescription">
                      {{ $t('hunts.jobDescription') }}
                      <BTooltip target="jobDescription">
                        <span v-i18n-btip="'hunts.'" />
                      </BTooltip>
                    </BInputGroupText>
                    <input
                      type="text"
                      class="form-control"
                      v-model="jobDescription"
                      :placeholder="$t('hunts.jobDescriptionPlaceholder')">
                  </BInputGroup>
                </BCol>
              </BRow>
              <BRow class="mb-2">
                <BCol>
                  <BInputGroup size="sm">
                    <BInputGroupText
                      class="cursor-help"
                      id="jobSearch">
                      <span class="fa fa-search" />
                      <BTooltip target="jobSearch">
                        <span v-i18n-btip="'hunts.'" />
                      </BTooltip>
                    </BInputGroupText>
                    <input
                      type="text"
                      v-model="jobSearch"
                      :placeholder="$t('hunts.jobSearchPlaceholder')"
                      class="form-control">
                  </BInputGroup>
                </BCol>
              </BRow>
              <BRow
                gutter-x="1"
                align-h="start">
                <!-- packet search text & text type -->
                <BCol>
                  <BFormRadioGroup
                    class="d-inline"
                    v-model="jobSearchType"
                    :options="[
                      { text: 'ascii', value: 'ascii' },
                      { text: 'ascii (case sensitive)', value: 'asciicase' },
                      { text: 'hex', value: 'hex' },
                      { text: 'safe regex', value: 'regex' },
                      { text: 'safe hex regex', value: 'hexregex' }
                    ]" />
                  <a
                    href="https://github.com/google/re2/wiki/Syntax"
                    target="_blank"
                    id="safeRegexHelp">
                    <span class="fa fa-question-circle fa-lg" />
                    <BTooltip target="safeRegexHelp"><span v-i18n-btip="'hunts.'" /></BTooltip>
                  </a>
                </BCol>
              </BRow>
              <BRow gutter-x="1">
                <!-- packet search direction -->
                <div class="form-group col-lg-3 col-md-12">
                  <div class="form-check">
                    <input
                      id="src"
                      value="src"
                      type="checkbox"
                      role="checkbox"
                      :checked="jobSrc"
                      @click="jobSrc = !jobSrc"
                      class="form-check-input">
                    <label
                      class="form-check-label"
                      for="src">
                      {{ $t('hunts.jobSrc') }}
                    </label>
                  </div>
                  <div class="form-check">
                    <input
                      id="dst"
                      value="dst"
                      type="checkbox"
                      role="checkbox"
                      :checked="jobDst"
                      class="form-check-input"
                      @click="jobDst = !jobDst">
                    <label
                      class="form-check-label"
                      for="dst">
                      {{ $t('hunts.jobDst') }}
                    </label>
                  </div>
                </div> <!-- /packet search direction -->
                <!-- packet search type -->
                <div class="form-group col-lg-3 col-md-12">
                  <div class="form-check">
                    <input
                      class="form-check-input"
                      :checked="jobType === 'raw'"
                      @click="setJobType('raw')"
                      type="radio"
                      id="raw"
                      value="raw"
                      name="packetSearchType">
                    <label
                      class="form-check-label"
                      for="raw">
                      {{ $t('hunts.jobType-raw') }}
                    </label>
                  </div>
                  <div class="form-check">
                    <input
                      class="form-check-input"
                      :checked="jobType === 'reassembled'"
                      @click="setJobType('reassembled')"
                      type="radio"
                      id="reassembled"
                      value="reassembled"
                      name="packetSearchType">
                    <label
                      class="form-check-label"
                      for="reassembled">
                      {{ $t('hunts.jobType-reassembled') }}
                    </label>
                  </div>
                </div> <!-- /packet search type -->
                <!-- sharing with users/roles -->
                <div
                  class="form-group d-flex col-lg-6 col-md-12"
                  v-if="!anonymousMode">
                  <div class="align-self-start">
                    <RoleDropdown
                      :roles="roles"
                      :selected-roles="jobRoles"
                      :display-text="$t('common.shareWithRoles')"
                      @selected-roles-updated="updateNewJobRoles" />
                  </div>
                  <div class="flex-grow-1 ms-2">
                    <BInputGroup size="sm">
                      <BInputGroupText
                        class="cursor-help"
                        id="jobUsers">
                        <span class="fa fa-user" />
                        <BTooltip target="jobUsers">
                          <span v-i18n-btip="'hunts.'" />
                        </BTooltip>
                      </BInputGroupText>
                      <input
                        type="text"
                        v-model="jobUsers"
                        :placeholder="$t('hunts.jobUsersPlaceholder')"
                        class="form-control">
                    </BInputGroup>
                  </div>
                </div> <!-- /sharing with users/roles -->
              </BRow>
              <BRow>
                <div class="col-12 mt-1">
                  <div
                    v-if="createFormError"
                    class="pull-left alert alert-danger alert-sm">
                    <span class="fa fa-exclamation-triangle" />&nbsp;
                    {{ createFormError }}
                  </div>
                  <!-- create search job button -->
                  <button
                    type="button"
                    @click="createJob"
                    :disabled="loadingSessions"
                    title="Create this hunt"
                    class="pull-right btn btn-theme-tertiary pull-right ms-1">
                    <span class="fa fa-plus fa-fw" />&nbsp;
                    {{ $t('common.create') }}
                  </button> <!-- /create search job button -->
                  <!-- cancel create search job button -->
                  <button
                    type="button"
                    @click="cancelCreateForm"
                    title="Cancel creating this hunt"
                    class="pull-right btn btn-warning pull-right">
                    <span class="fa fa-ban fa-fw" />&nbsp;
                    {{ $t('common.cancel') }}
                  </button> <!-- /cancel create search job button -->
                </div>
              </BRow>
            </form>
          </div>
        </transition>
      </div> <!-- /create new packet search job -->

      <!-- running job -->
      <transition name="slide">
        <div
          v-if="runningJob"
          class="card mb-3">
          <div class="card-body">
            <h5 class="card-title">
              {{ $t('hunts.runningHuntJob', { name: runningJob.name, user: runningJob.userId }) }}
              <span class="pull-right">
                <button
                  v-if="canEdit"
                  :id="`remove${runningJob.id}`"
                  @click="removeJob(runningJob, 'results')"
                  :disabled="runningJob.disabled"
                  type="button"
                  class="ms-1 pull-right btn btn-sm btn-danger">
                  <span
                    v-if="!runningJob.loading"
                    class="fa fa-trash-o fa-fw" />
                  <span
                    v-else
                    class="fa fa-spinner fa-spin fa-fw" />
                  <BTooltip :target="`remove${runningJob.id}`">
                    {{ $t('hunts.cancelAndRemoveTip') }}
                  </BTooltip>
                </button>
                <span v-if="canView">
                  <button
                    type="button"
                    @click="openSessions(runningJob)"
                    v-if="runningJob.matchedSessions"
                    :id="`openresults${runningJob.id}`"
                    class="ms-1 pull-right btn btn-sm btn-theme-primary">
                    <span class="fa fa-folder-open fa-fw" />
                    <BTooltip :target="`openresults${runningJob.id}`">
                      <span v-html="$t('hunts.openResultsTipHtml')" />
                    </BTooltip>
                  </button>
                </span>
                <button
                  v-if="canEdit"
                  :id="`cancel${runningJob.id}`"
                  @click="cancelJob(runningJob)"
                  :disabled="runningJob.disabled"
                  type="button"
                  class="ms-1 pull-right btn btn-sm btn-danger">
                  <span
                    v-if="!runningJob.loading"
                    class="fa fa-ban fa-fw" />
                  <span
                    v-else
                    class="fa fa-spinner fa-spin fa-fw" />
                </button>
                <BTooltip :target="`cancel${runningJob.id}`">
                  {{ $t('hunts.cancelTip') }}
                </BTooltip>
                <button
                  v-if="canEdit"
                  :id="`pause${runningJob.id}`"
                  @click="pauseJob(runningJob)"
                  :disabled="runningJob.loading"
                  type="button"
                  class="pull-right btn btn-sm btn-warning">
                  <span
                    v-if="!runningJob.loading"
                    class="fa fa-pause fa-fw" />
                  <span
                    v-else
                    class="fa fa-spinner fa-spin fa-fw" />
                  <BTooltip :target="`pause${runningJob.id}`">
                    {{ $t('hunts.pauseTip') }}
                  </BTooltip>
                </button>
              </span>
            </h5>
            <div class="card-text">
              <div class="row">
                <div class="col">
                  <toggle-btn
                    v-if="canView"
                    :opened="runningJob.expanded"
                    @toggle="toggleJobDetail(runningJob)" />
                  <div
                    class="progress cursor-help"
                    id="runningJob"
                    style="height:26px;"
                    :class="{'progress-toggle':canView}">
                    <div
                      class="progress-bar bg-success progress-bar-striped progress-bar-animated"
                      role="progressbar"
                      :style="{width: runningJob.progress + '%'}"
                      :aria-valuenow="runningJob.progress"
                      aria-valuemin="0"
                      aria-valuemax="100">
                      {{ round(runningJob.progress, 1) }}%
                    </div>
                    <BTooltip target="runningJob">
                      <div class="mt-2">
                        <span v-html="$t('hunts.runningJob-headHtml', { matched: commaString(runningJob.matchedSessions) })" />
                        <span
                          v-if="canView"
                          v-html="$t('hunts.runningJob-byHtml', { search: runningJob.search, searchType: runningJob.searchType })" />
                        <span
                          v-if="runningJob.failedSessionIds && runningJob.failedSessionIds.length"
                          v-html="$t('hunts.runningJob-outOfHtml', { 
                            searched: commaString(runningJob.searchedSessions - runningJob.failedSessionIds.length), 
                            remaining: commaString(runningJob.totalSessions - runningJob.searchedSessions + runningJob.failedSessionIds.length), 
                            totalSessions: commaString(runningJob.totalSessions)})" />
                        <span
                          v-else
                          v-html="$t('hunts.runningJob-outOfHtml', { 
                            searched: commaString(runningJob.searchedSessions), 
                            remaining: commaString(runningJob.totalSessions - runningJob.searchedSessions), 
                            totalSessions: commaString(runningJob.totalSessions)})" />
                      </div>
                    </BTooltip>
                  </div>
                </div>
              </div>
              <transition name="grow">
                <div
                  v-if="runningJob.expanded"
                  class="mt-3">
                  <div class="row">
                    <div class="col-12">
                      <span class="fa fa-id-card fa-fw" />&nbsp;
                      {{ $t('hunts.huntJobId', { id: runningJob.id }) }}:
                    </div>
                  </div>
                  <hunt-data
                    :job="runningJob"
                    @remove-user="removeUser"
                    @add-users="addUsers"
                    :user="user"
                    @update-hunt="updateHunt"
                    :notifier-name="getNotifierName(runningJob.notifier)" />
                </div>
              </transition>
            </div>
          </div>
        </div>
      </transition> <!-- /running job -->

      <h4 v-if="results.length">
        <span class="fa fa-list-ol" />&nbsp;
        {{ $t('hunts.huntJobQueue') }}:
      </h4>

      <!-- hunt job queue errors -->
      <div
        v-if="queuedListError"
        class="alert alert-danger">
        {{ queuedListError }}
      </div>
      <div
        v-if="queuedListLoadingError"
        class="alert alert-danger">
        {{ $t('hunts.errorLoadingQueue') }}:
        {{ queuedListLoadingError }}
      </div> <!-- /hunt job queue errors -->

      <table
        v-if="results.length"
        class="table table-sm table-striped mb-4">
        <thead>
          <tr>
            <th width="40px">
&nbsp;
            </th>
            <th>
              {{ $t('hunts.jobStatus') }}
            </th>
            <th>
              {{ $t('hunts.jobMatches') }}
            </th>
            <th>
              {{ $t('hunts.jobName') }}
            </th>
            <th>
              {{ $t('hunts.jobUser') }}
            </th>
            <th>
              {{ $t('hunts.jobSearch') }}
            </th>
            <th>
              {{ $t('hunts.jobNotifier') }}
            </th>
            <th>
              {{ $t('common.created') }}
            </th>
            <th>
              ID
            </th>
            <th width="260px">
&nbsp;
            </th>
          </tr>
        </thead>
        <transition-group
          name="list"
          tag="tbody">
          <!-- packet search jobs -->
          <template
            v-for="job in results"
            :key="`${job.id}-row`">
            <hunt-row
              :job="job"
              :user="user"
              :can-rerun="true"
              :can-repeat="true"
              :can-cancel="true"
              array-name="results"
              @play-job="playJob"
              @rerun-job="rerunJob"
              @repeat-job="repeatJob"
              @pause-job="pauseJob"
              @cancel-job="cancelJob"
              @remove-job="removeJob"
              @toggle="toggleJobDetail"
              @open-sessions="openSessions"
              :notifier-name="getNotifierName(job.notifier)" />
            <tr
              :key="`${job.id}-detail`"
              v-if="job.expanded">
              <td colspan="10">
                <hunt-data
                  :job="job"
                  @remove-user="removeUser"
                  @add-users="addUsers"
                  :user="user"
                  @update-hunt="updateHunt"
                  :notifier-name="getNotifierName(job.notifier)" />
              </td>
            </tr>
          </template> <!-- /packet search jobs -->
        </transition-group>
      </table>

      <!-- hunt job history errors -->
      <div
        v-if="historyListError"
        class="alert alert-danger">
        <span class="fa fa-exclamation-triangle me-2" />
        {{ historyListError }}
      </div>
      <div
        v-if="historyListLoadingError"
        class="alert alert-danger">
        <span class="fa fa-exclamation-triangle me-2" />
        {{ $t('hunts.errorLoadingHistory') }}:
        {{ historyListLoadingError }}
      </div> <!-- /hunt job history errors -->

      <template v-if="!historyListLoadingError">
        <h4>
          <span class="fa fa-clock-o me-2" />
          {{ $t('hunts.title') }}
        </h4>
        <BRow
          gutter-x="1"
          align-h="start">
          <BCol>
            <!-- search packet search jobs -->
            <BInputGroup size="sm">
              <BInputGroupText>
                <span class="fa fa-search" />
              </BInputGroupText>
              <input
                type="text"
                v-model="query.searchTerm"
                @input="debounceSearch"
                :placeholder="$t('hunts.querySearchTermPlaceholder')"
                class="form-control">
              <button
                type="button"
                @click="clear"
                :disabled="!query.searchTerm"
                class="btn btn-outline-secondary btn-clear-input">
                <span class="fa fa-close" />
              </button>
            </BInputGroup> <!-- /search packet search jobs -->
          </BCol>
          <BCol cols="auto">
            <!-- job history paging -->
            <arkime-paging
              :records-total="historyResults.recordsTotal"
              :records-filtered="historyResults.recordsFiltered"
              @change-paging="changePaging" /> <!-- /job history paging -->
          </BCol>
        </BRow>

        <table class="table table-sm table-striped">
          <thead>
            <tr>
              <th width="40px">
&nbsp;
              </th>
              <th
                class="cursor-pointer"
                @click="columnClick('status')">
                {{ $t('hunts.jobStatus') }}
                <span
                  v-show="query.sortField === 'status' && !query.desc"
                  class="fa fa-sort-asc" />
                <span
                  v-show="query.sortField === 'status' && query.desc"
                  class="fa fa-sort-desc" />
                <span
                  v-show="query.sortField !== 'status'"
                  class="fa fa-sort" />
              </th>
              <th>
                {{ $t('hunts.jobMatches') }}
              </th>
              <th
                class="cursor-pointer"
                @click="columnClick('name')">
                {{ $t('hunts.jobName') }}
                <span
                  v-show="query.sortField === 'name' && !query.desc"
                  class="fa fa-sort-asc" />
                <span
                  v-show="query.sortField === 'name' && query.desc"
                  class="fa fa-sort-desc" />
                <span
                  v-show="query.sortField !== 'name'"
                  class="fa fa-sort" />
              </th>
              <th
                class="cursor-pointer no-wrap"
                @click="columnClick('userId')">
                {{ $t('hunts.jobUser') }}
                <span
                  v-show="query.sortField === 'userId' && !query.desc"
                  class="fa fa-sort-asc" />
                <span
                  v-show="query.sortField === 'userId' && query.desc"
                  class="fa fa-sort-desc" />
                <span
                  v-show="query.sortField !== 'userId'"
                  class="fa fa-sort" />
              </th>
              <th>
                {{ $t('hunts.jobSearch') }}
              </th>
              <th>
                {{ $t('hunts.jobNotifier') }}
              </th>
              <th
                class="cursor-pointer"
                @click="columnClick('created')">
                {{ $t('common.created') }}
                <span
                  v-show="query.sortField === 'created' && !query.desc"
                  class="fa fa-sort-asc" />
                <span
                  v-show="query.sortField === 'created' && query.desc"
                  class="fa fa-sort-desc" />
                <span
                  v-show="query.sortField !== 'created'"
                  class="fa fa-sort" />
              </th>
              <th>
                ID
              </th>
              <th width="260px">
&nbsp;
              </th>
            </tr>
          </thead>
          <transition-group
            name="list"
            tag="tbody">
            <!-- packet search jobs -->
            <template
              v-for="job in historyResults.data"
              :key="`${job.id}-row`">
              <hunt-row
                :job="job"
                :user="user"
                :can-rerun="true"
                :can-repeat="true"
                array-name="historyResults"
                :can-remove-from-sessions="true"
                @play-job="playJob"
                @pause-job="pauseJob"
                @rerun-job="rerunJob"
                @repeat-job="repeatJob"
                @remove-job="removeJob"
                @toggle="toggleJobDetail"
                @open-sessions="openSessions"
                @remove-from-sessions="removeFromSessions"
                :notifier-name="getNotifierName(job.notifier)" />
              <tr
                :key="`${job.id}-detail`"
                v-if="job.expanded">
                <td colspan="10">
                  <hunt-data
                    :job="job"
                    @remove-job="removeJob"
                    @remove-user="removeUser"
                    @add-users="addUsers"
                    :user="user"
                    @update-hunt="updateHunt"
                    :notifier-name="getNotifierName(job.notifier)" />
                </td>
              </tr>
            </template> <!-- /packet search jobs -->
          </transition-group>
        </table>

        <!-- no results -->
        <div
          v-if="!loading && !historyResults.data.length"
          class="ms-1 me-1">
          <div class="mb-5 info-area horizontal-center">
            <div>
              <span class="fa fa-3x text-muted-more fa-folder-open" />&nbsp;
              <span v-if="!query.searchTerm">
                {{ $t('hunts.emptyHistory') }}
                <span v-if="!results.length">
                  <br>
                  {{ $t('hunts.noHunts') }}
                </span>
              </span>
              <span v-else>
                {{ $t('hunts.noMatchHistory') }}
              </span>
            </div>
          </div>
        </div> <!-- /no results -->
      </template>
    </div> <!-- /packet search jobs content -->

    <!-- floating error -->
    <transition name="slide-fade">
      <div
        v-if="floatingError || floatingSuccess"
        class="card floating-msg">
        <div class="card-body">
          <a
            @click="floatingError = false; floatingSuccess = false"
            id="dismissError"
            class="no-decoration cursor-pointer pull-right">
            <span class="fa fa-close" />
            <BTooltip target="dismissError">$t('common.dismiss')</BTooltip>
          </a>
          <span :class="floatingError ? 'text-danger' : 'text-success'">
            <span
              v-if="floatingError"
              class="fa fa-exclamation-triangle me-2" />
            <span
              v-else
              class="fa fa-check me-2" />
            {{ floatingError || floatingSuccess }}
          </span>
        </div>
      </div>
    </transition> <!-- /floating error -->
  </div>
</template>

<script>
// import services
import SettingsService from '../settings/SettingsService';
import SessionsService from '../sessions/SessionsService';
import ConfigService from '../utils/ConfigService';
import HuntService from './HuntService';
// import components
import ToggleBtn from '@common/ToggleBtn.vue';
import ArkimeSearch from '../search/Search.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ArkimePaging from '../utils/Pagination.vue';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';
import Focus from '@common/Focus.vue';
import HuntData from './HuntData.vue';
import HuntRow from './HuntRow.vue';
import RoleDropdown from '@common/RoleDropdown.vue';
import { commaString, round } from '@common/vueFilters.js';
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
    ArkimeSearch,
    ArkimeLoading,
    ArkimeCollapsible,
    ArkimePaging,
    HuntData,
    HuntRow,
    RoleDropdown
  },
  directives: { Focus },
  emits: ['recalc-collapse'],
  data: function () {
    return {
      queuedListError: '',
      queuedListLoadingError: '',
      historyListError: '',
      historyListLoadingError: '',
      permissionDenied: false,
      floatingError: '',
      floatingSuccess: '',
      loading: true,
      results: [], // running/queued/paused hunt jobs
      historyResults: { // finished hunt jobs
        data: [],
        recordsTotal: 0,
        recordsFiltered: 0
      },
      runningJob: undefined, // the currently running hunt job obj
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
      jobType: 'reassembled',
      jobNotifier: undefined,
      jobUsers: '',
      jobDescription: '',
      jobRoles: [],
      // hunt limits
      huntWarn: this.$constants.HUNTWARN,
      huntLimit: this.$constants.HUNTLIMIT,
      anonymousMode: this.$constants.ANONYMOUS_MODE,
      // hunt configured?
      nodeInfo: undefined,
      // multiviewer enabled?
      multiviewer: this.$constants.MULTIVIEWER
    };
  },
  computed: {
    query: function () {
      return { // packet search job history search query
        sortField: 'created',
        desc: true,
        searchTerm: '',
        start: 0, // first item index
        length: Math.min(this.$route.query.length || 50, 10000),
        cluster: this.$route.query.cluster || undefined
      };
    },
    sessionsQuery: function () {
      return { // sessions query defaults
        length: this.$route.query.length || 50, // page length
        start: 0, // first item index
        facets: 0,
        date: this.$store.state.timeRange,
        startTime: this.$store.state.time.startTime,
        stopTime: this.$store.state.time.stopTime,
        bounding: this.$route.query.bounding || 'last',
        interval: this.$route.query.interval || 'auto',
        expression: this.$store.state.expression || undefined,
        view: this.$route.query.view || undefined,
        cluster: this.$route.query.cluster || undefined
      };
    },
    user: function () {
      return this.$store.state.user;
    },
    roles () {
      return this.$store.state.roles;
    },
    canEdit () {
      return HuntService.canEditHunt(this.user, this.runningJob);
    },
    canView () {
      return HuntService.canViewHunt(this.user, this.runningJob);
    },
    notifiers () {
      return this.$store.state.notifiers;
    },
    selectedCluster () {
      return this.$store.state.esCluster.selectedCluster || [];
    }
  },
  watch: {
    '$route.query.cluster' () {
      this.loadData();
      this.cancelAndLoad(true);
    }
  },
  mounted () {
    this.$nextTick(() => {
      // wait for computed queries
      this.loadData();
      this.cancelAndLoad(true);
      SettingsService.getNotifiers(); // sets notifiers in store
    });

    // interval to load jobs every 5 seconds
    interval = setInterval(() => {
      if (respondedAt && Date.now() - respondedAt >= 5000) {
        this.loadData();
      }
    }, 500);
  },
  methods: {
    round,
    commaString,
    /**
     * Cancels the pending session query (if it's still pending) and runs a new
     * query if requested
     * @param {bool} runNewQuery  Whether to run a new spigraph query after
     *                            canceling the request
     */
    cancelAndLoad: function (runNewQuery) {
      const clientCancel = () => {
        if (pendingPromise) {
          pendingPromise.controller.abort(this.$t('hunts.canceledSearch'));
          pendingPromise = null;
        }

        if (!runNewQuery) {
          this.loadingSessions = false;
          if (!this.sessions.data) {
            // show a page error if there is no data on the page
            this.loadingSessionsError = this.$t('hunts.canceledSearch');
          }
          return;
        }

        this.loadSessions();
      };

      if (pendingPromise) {
        ConfigService.cancelEsTask(pendingPromise.cancelId)
          .then((response) => {
            clientCancel();
          }).catch((error) => {
            clientCancel();
          });
      } else if (runNewQuery) {
        this.loadSessions();
      }
    },
    cancelCreateForm: function () {
      this.jobName = '';
      this.jobUsers = '';
      this.jobSearch = '';
      this.jobDescription = '';
      this.createFormError = '';
      this.createFormOpened = false;
    },
    updateNewJobRoles (roles) {
      this.jobRoles = roles;
    },
    createJob: function () {
      this.createFormError = '';

      if (!this.sessions.recordsFiltered) {
        this.createFormError = this.$t('hunts.noSessions');
        return;
      }
      if (this.sessions.recordsFiltered > this.huntLimit) {
        this.createFormError = this.$t('hunts.tooManySessions', { count: this.huntLimit });
        return;
      }
      if (!this.jobName) {
        this.createFormError = this.$t('hunts.jobNameMissing');
        return;
      }
      if (!this.jobSearch) {
        this.createFormError = this.$t('hunts.jobSearchMissing');
        return;
      }
      if (!this.jobSrc && !this.jobDst) {
        this.createFormError = this.$t('hunts.jobSrcDstMissing');
        return;
      }

      const newJob = {
        name: this.jobName,
        size: this.jobSize,
        search: this.jobSearch,
        searchType: this.jobSearchType,
        type: this.jobType,
        src: this.jobSrc,
        dst: this.jobDst,
        totalSessions: this.sessions.recordsFiltered,
        query: this.sessionsQuery,
        notifier: this.jobNotifier,
        users: this.jobUsers,
        description: this.jobDescription,
        roles: this.jobRoles
      };

      HuntService.create(newJob, this.query.cluster).then((response) => {
        this.createFormOpened = false;
        this.jobName = '';
        this.jobUsers = '';
        this.jobRoles = [];
        this.jobSearch = '';
        this.jobDescription = '';
        this.createFormError = '';
        this.jobNotifier = undefined;
        this.loadData();
      }).catch((error) => {
        this.createFormError = error.text || error;
      });
    },
    removeFromSessions: function (job) {
      if (job.loading) { return; } // it's already trying to do something

      this.setErrorForList('historyResults', '');
      job.loading = true;

      HuntService.cleanup(job.id, this.query.cluster).then((response) => {
        job.loading = false;
        job.removed = true;
        this.floatingSuccess = response.text || 'Successfully removed hunt ID and name from the matched sessions.';
        setTimeout(() => {
          this.floatingSuccess = '';
        }, 5000);
      }).catch((error) => {
        job.loading = false;
        this.setErrorForList('historyResults', error.text || error);
      });
    },
    removeJob: function (job, arrayName) {
      if (job.loading) { return; } // it's already trying to do something

      this.setErrorForList(arrayName, '');
      job.loading = true;

      HuntService.delete(job.id, this.query.cluster).then((response) => {
        job.loading = false;
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
      }).catch((error) => {
        job.loading = false;
        this.setErrorForList(arrayName, error.text || error);
      });
    },
    cancelJob: function (job) {
      if (job.loading) { return; } // it's already trying to do something

      this.setErrorForList('results', '');
      job.loading = true;

      HuntService.cancel(job.id, this.query.cluster).then((response) => {
        job.loading = false;
        this.loadData();
      }).catch((error) => {
        job.loading = false;
        this.setErrorForList('results', error.text || error);
      });
    },
    pauseJob: function (job) {
      if (job.loading) { return; } // it's already trying to do something

      this.setErrorForList('results', '');
      job.loading = true;

      HuntService.pause(job.id, this.query.cluster).then((response) => {
        if (job.status === 'running') {
          this.loadData();
          return;
        }
        job.status = 'paused';
        job.loading = false;
        this.calculateQueue();
      }).catch((error) => {
        job.loading = false;
        this.setErrorForList('results', error.text || error);
      });
    },
    playJob: function (job) {
      if (job.loading) { return; } // it's already trying to do something

      this.setErrorForList('results', '');
      job.loading = true;

      HuntService.play(job.id, this.query.cluster).then((response) => {
        job.status = 'queued';
        job.loading = false;
        this.calculateQueue();
      }).catch((error) => {
        job.loading = false;
        this.setErrorForList('results', error.text || error);
      });
    },
    openSessions: function (job) {
      const url = `sessions?expression=huntId == ${job.id}&stopTime=${job.query.stopTime}&startTime=${job.query.startTime}`;
      window.open(url, '_blank');
    },
    setJobType: function (val) {
      this.jobType = val;
    },
    toggleJobDetail: function (job) {
      job.expanded = !job.expanded;
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
    columnClick: function (colName) {
      if (colName === this.query.sortField) {
        // same sort field, so toggle order direction
        this.query.desc = !this.query.desc;
      } else { // new sort field, so set default order (desc)
        this.query.sortField = colName;
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
      this.jobDescription = job.description;
      this.createFormOpened = true;
    },
    repeatJob: function (job) {
      this.$store.commit('setExpression', job.query.expression);
      this.$store.commit('setTimeRange', '0');
      this.$store.commit('setTime', {
        stopTime: job.query.stopTime,
        startTime: job.query.startTime
      });
      this.$store.commit('setIssueSearch', true);
      this.rerunJob(job);
    },
    removeUser: function (user, job) {
      this.floatingError = '';

      HuntService.removeUser(job.id, user, this.query.cluster).then((response) => {
        job.users = response.users;
      }).catch((error) => {
        this.floatingError = error.text || error;
      });
    },
    addUsers: function (users, job) {
      this.floatingError = '';

      HuntService.addUsers(job.id, users, this.query.cluster).then((response) => {
        job.users = response.users;
      }).catch((error) => {
        this.floatingError = error.text || error;
      });
    },
    updateHunt: function (job) {
      this.floatingError = '';
      this.floatingSuccess = '';

      const data = {
        roles: job.roles,
        description: job.description
      };

      HuntService.updateHunt(job.id, data, this.query.cluster).then((response) => {
        this.floatingSuccess = response.text;
        setTimeout(() => {
          this.floatingSuccess = '';
        }, 5000);
      }).catch((error) => {
        this.floatingError = error.text || error;
      });
    },
    getNotifierName (id) {
      if (!this.notifiers) { return; }
      for (const notifier of this.notifiers) {
        if (notifier.id === id) {
          return notifier.name;
        }
      }
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
      for (const job of this.results) {
        if (job.status === 'queued') {
          job.queueCount = queueCount;
          queueCount++;
        }
      }
    },
    loadData: function () {
      respondedAt = undefined;
      this.permissionDenied = false;

      const loading = {};
      const expanded = {};
      const runningJobLoading = this.runningJob && this.runningJob.loading;
      const runningJobExpanded = this.runningJob && this.runningJob.expanded;
      if (this.results && this.results.length) {
        // save the expanded and loading ones
        for (const result of this.results) {
          if (result.expanded) {
            expanded[result.id] = true;
          }
          if (result.loading) {
            loading[result.id] = true;
          }
        }
      }
      if (this.historyResults.data && this.historyResults.data.length) {
        // save the expanded and loading ones
        for (const result of this.historyResults.data) {
          if (result.expanded) {
            expanded[result.id] = true;
          }
          if (result.loading) {
            loading[result.id] = true;
          }
        }
      }

      // get the hunt history
      const historyReq = HuntService.get(this.query, true);
      historyReq.then((response) => {
        this.historyListLoadingError = '';

        if (Object.keys(expanded).length || Object.keys(loading).length) {
          for (const result of response.data) {
            // make sure expanded ones are still expanded
            if (Object.keys(expanded).length) {
              if (expanded[result.id]) {
                result.expanded = true;
              }
            }
            // make sure the loading ones are still loading
            if (Object.keys(loading).length) {
              if (loading[result.id]) {
                result.loading = true;
              }
            }
          }
        }

        this.nodeInfo = response.nodeInfo;
        this.historyResults = response;
      }).catch((error) => {
        if (error.status === 403) { this.permissionDenied = true; }
        this.historyListLoadingError = error.text || error;
      });

      // get the running, queued, paused hunts
      const queuedQuery = JSON.parse(JSON.stringify(this.query));
      queuedQuery.desc = false;
      queuedQuery.length = undefined;
      queuedQuery.start = 0;
      const queueReq = HuntService.get(queuedQuery);
      queueReq.then((response) => {
        this.queuedListLoadingError = '';

        if (Object.keys(expanded).length || Object.keys(loading).length) {
          for (const result of response.data) {
            // make sure expanded ones are still expanded
            if (Object.keys(expanded).length) {
              if (expanded[result.id]) {
                result.expanded = true;
              }
            }
            // make sure the loading ones are still loading
            if (Object.keys(loading).length) {
              if (loading[result.id]) {
                result.loading = true;
              }
            }
          }
        }

        this.results = response.data;
        this.runningJob = response.runningJob;
        if (this.runningJob) {
          this.runningJob.loading = runningJobLoading;
          this.runningJob.expanded = runningJobExpanded;
          let searchedSessions = this.runningJob.searchedSessions;
          if (this.runningJob.failedSessionIds && this.runningJob.failedSessionIds.length) {
            searchedSessions = searchedSessions - this.runningJob.failedSessionIds.length;
          }
          this.runningJob.progress = (searchedSessions / this.runningJob.totalSessions) * 100;
        }
        this.calculateQueue();
      }).catch((error) => {
        this.queuedListLoadingError = error.text || error;
      });

      // stop loading when both requests are done
      Promise.all([historyReq, queueReq]).then((values) => {
        respondedAt = Date.now();
        this.loading = false;
      }).catch(() => {
        this.loading = false;
        respondedAt = undefined;
      });
    },
    async loadSessions () {
      this.loadingSessions = true;
      this.loadingSessionsError = '';

      // create unique cancel id to make cancel req for corresponding es task
      const cancelId = Utils.createRandomString();
      this.sessionsQuery.cancelId = cancelId;

      try {
        const { controller, fetcher } = SessionsService.get(this.sessionsQuery);
        pendingPromise = { controller, cancelId };

        const response = await fetcher; // do the fetch
        if (response.error) {
          throw new Error(response.error);
        }

        pendingPromise = null;
        this.sessions = response;
        this.loadingSessions = false;
      } catch (error) {
        pendingPromise = null;
        this.sessions = {};
        this.loadingSessions = false;
        this.loadingSessionsError = this.$t('hunts.problemLoading');
      }
    }
  },
  beforeUnmount () {
    if (pendingPromise) {
      pendingPromise.controller.abort(this.$t('hunts.closePage'));
      pendingPromise = null;
    }

    if (interval) { clearInterval(interval); }
  }
};
</script>

<style scoped>
.packet-search-content {
  margin-top: 10px;
}

.info-area {
  font-size: var(--px-xxlg);
}

.hunt-create-navbar {
  z-index: 4;
  height: 38px;
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
.slide-enter-from,
.slide-leave-active {
  max-height: 0px;
}
.slide-leave-from {
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
.grow-enter-from,
.grow-leave-active {
  max-height: 0px;
}
.grow-leave-from {
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
.list-enter-from {
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

/* floating message */
.floating-msg {
  position: fixed;
  bottom: 15px;
  left: 10px;
  z-index: 999;
  width: 350px;
}

.floating-msg .card {
  background-color: var(--color-gray-lighter);
  border: 1px solid var(--color-gray-light);
  -webkit-box-shadow: 4px 4px 16px -2px black;
     -moz-box-shadow: 4px 4px 16px -2px black;
          box-shadow: 4px 4px 16px -2px black;
}
.floating-msg .card > .card-body {
  padding: 0.8rem;
}

.slide-fade-enter-active {
  transition: all .8s ease;
}
.slide-fade-leave-active {
  transition: all .8s ease;
}
.slide-fade-enter-from, .slide-fade-leave-to {
  transform: translateX(-365px);
  opacity: 0;
}
</style>
