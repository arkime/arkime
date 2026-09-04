<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <page-layout class="packet-search-page">
    <template #chrome>
      <ArkimeCollapsible>
        <div class="page-toolbar">
          <!-- search navbar -->
          <arkime-search
            v-if="user.settings"
            :start="sessionsQuery.start"
            :hide-actions="true"
            :hide-interval="true"
            @change-search="cancelAndLoad(true)" /> <!-- /search navbar -->

          <!-- hunt create navbar -->
          <v-row class="g-1 hunt-create-navbar ps-2 pe-2 justify-space-between align-center page-subnav">
            <v-col cols="auto">
              <span v-if="loadingSessions">
                <div
                  class="mt-1"
                  style="display:inline-block;">
                  <v-icon
                    icon="mdi-loading"
                    class="mdi-spin" />
                  {{ $t('common.loading') }}
                </div>
                <v-btn
                  color="warning"
                  variant="flat"
                  size="small"
                  density="comfortable"
                  class="ms-3"
                  @click="cancelAndLoad">
                  <v-icon icon="mdi-cancel" />&nbsp;
                  {{ $t('common.cancel') }}
                </v-btn>
              </span>
              <span v-else-if="loadingSessionsError">
                <div
                  class="mt-1"
                  style="display:inline-block;">
                  <v-icon
                    icon="mdi-alert"
                    class="text-danger" />
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
            </v-col>
            <v-col cols="auto">
              <v-btn
                variant="flat"
                size="small"
                density="comfortable"
                :style="tertiaryBtnStyle"
                :disabled="loadingSessions"
                v-if="!createFormOpened"
                @click="createFormOpened = true">
                {{ $t('hunts.createJob') }}
              </v-btn>
              <v-btn
                color="warning"
                variant="flat"
                size="small"
                density="comfortable"
                v-if="createFormOpened && loadingSessionsDetailError"
                @click="createFormOpened = false">
                <v-icon icon="mdi-cancel" />&nbsp;
                {{ $t('common.cancel') }}
              </v-btn>
            </v-col>
          </v-row> <!-- /hunt create navbar -->
        </div>
      </ArkimeCollapsible>
    </template>

    <div class="ms-2 me-2">
      <!-- loading overlay -->
      <arkime-loading
        v-if="loading" /> <!-- /loading overlay -->

      <!-- page error -->
      <arkime-error
        v-if="loadingSessionsDetailError && createFormOpened"
        :message="loadingSessionsDetailError"
        class="mt-2 mb-2" /> <!-- /page error -->

      <!-- configuration error -->
      <v-alert
        v-if="nodeInfo && !nodeInfo.node"
        type="error"
        variant="tonal"
        density="compact"
        style="z-index: 2000;"
        class="position-fixed fixed-bottom m-0 rounded-0">
        <v-icon
          icon="mdi-alert"
          class="me-2" />
        <span v-html="$t('hunts.notConfiguredHtml')" />
      </v-alert> <!-- /configuration error -->

      <!-- permission error -->
      <v-alert
        v-if="permissionDenied"
        type="error"
        variant="tonal"
        density="compact"
        class="mt-4">
        <p class="mb-0">
          <v-icon
            icon="mdi-alert"
            class="me-2" />
          <strong>{{ $t('common.permisionDenied') }}</strong>
        </p>
        <p class="mb-0">
          <v-icon
            icon="mdi-information"
            class="me-2" />
          <template v-if="user.roles.includes('usersAdmin')">
            {{ $t('hunts.selfEnable') }}
          </template>
          <template v-else>
            {{ $t('hunts.adminEnable') }}
          </template>
        </p>
      </v-alert> <!-- /permission error -->

      <!-- packet search jobs content -->
      <div
        v-if="!permissionDenied"
        class="packet-search-content ms-2 me-2">
        <!-- create new packet search job -->
        <div class="mb-3">
          <transition name="slide">
            <v-card
              v-if="createFormOpened && !loadingSessionsDetailError"
              variant="outlined">
              <form
                class="pa-3"
                @keyup.enter="createJob">
                <v-row>
                  <v-col cols="12">
                    <v-alert
                      :type="sessions.recordsFiltered >= huntWarn ? 'error' : 'info'"
                      :icon="false"
                      variant="tonal"
                      density="compact">
                      <em v-if="sessions.recordsFiltered > huntWarn && !loadingSessions">
                        <span v-html="$t('hunts.lotOfSessionsHtml')" />
                        <br>
                      </em>
                      <em v-if="loadingSessions">
                        <v-icon
                          icon="mdi-loading"
                          class="mdi-spin me-1" />
                        {{ $t('hunts.waitForCalculation') }}
                        <br>
                      </em>
                      <span v-if="!loadingSessions">
                        <v-icon
                          icon="mdi-alert"
                          class="me-1" />
                        {{ $t('hunts.doubleCheckSessions') }}
                      </span>
                      <span v-if="multiviewer">
                        <br>
                        <v-icon
                          icon="mdi-information"
                          class="me-2" />
                        {{ $t('hunts.multiViewerHtml', { cluster: selectedCluster[0] }) }}
                      </span>
                    </v-alert>
                  </v-col>
                </v-row>
                <v-row class="g-1">
                  <v-col
                    cols="auto"
                    class="mb-2 flex-grow-1">
                    <!-- packet search job name -->
                    <div class="arkime-input-group arkime-input-group--fluid">
                      <span
                        id="jobName"
                        class="arkime-input-label cursor-help">
                        {{ $t('hunts.jobName') }}
                        <v-tooltip activator="#jobName">
                          {{ $t('hunts.jobNameTip') }}
                        </v-tooltip>
                      </span>
                      <input
                        type="text"
                        v-model="jobName"
                        v-focus="true"
                        class="arkime-input-control"
                        :placeholder="$t('hunts.jobNamePlaceholder')"
                        maxlength="40">
                    </div> <!-- /packet search job name -->
                  </v-col>
                  <!-- packet search size -->
                  <v-col cols="auto">
                    <div class="arkime-input-group arkime-input-group--fluid">
                      <span class="arkime-input-label">
                        {{ $t('hunts.jobSize') }}
                      </span>
                      <select
                        class="arkime-input-control"
                        v-model="jobSize">
                        <option
                          v-for="size in [50, 500, 5000, 10000]"
                          :key="size"
                          :value="size">
                          {{ size }}
                        </option>
                      </select>
                    </div>
                  </v-col> <!-- /packet search size -->
                  <!-- notifier -->
                  <v-col cols="auto">
                    <NotifierDropdown
                      :notifiers="notifiers"
                      :selected-notifiers="jobNotifier"
                      @selected-notifiers-updated="updateJobNotifiers"
                      :display-text="jobNotifier.length > 0 ? undefined : $t('settings.cron.selectNotifier')" />
                  </v-col> <!-- /notifier -->
                </v-row>
                <v-row class="g-1 mb-2">
                  <v-col cols="auto">
                    <div class="arkime-input-group arkime-input-group--fluid">
                      <span
                        id="jobDescription"
                        class="arkime-input-label cursor-help">
                        {{ $t('hunts.jobDescription') }}
                        <v-tooltip activator="#jobDescription">
                          {{ $t('hunts.jobDescriptionTip') }}
                        </v-tooltip>
                      </span>
                      <input
                        type="text"
                        class="arkime-input-control"
                        v-model="jobDescription"
                        :placeholder="$t('hunts.jobDescriptionPlaceholder')">
                    </div>
                  </v-col>
                </v-row>
                <v-row>
                  <v-col cols="auto">
                    <div class="arkime-input-group arkime-input-group--fluid">
                      <span
                        id="jobSearch"
                        class="arkime-input-label cursor-help">
                        <v-icon icon="mdi-magnify" />
                        <v-tooltip activator="#jobSearch">
                          {{ $t('hunts.jobSearchTip') }}
                        </v-tooltip>
                      </span>
                      <input
                        type="text"
                        v-model="jobSearch"
                        :placeholder="$t('hunts.jobSearchPlaceholder')"
                        class="arkime-input-control">
                    </div>
                  </v-col>
                </v-row>
                <v-row class="g-1 justify-start">
                  <!-- packet search text & text type -->
                  <v-col cols="auto">
                    <v-btn-toggle
                      class="d-inline-flex"
                      density="compact"
                      divided
                      variant="outlined"
                      color="secondary"
                      v-model="jobSearchType"
                      mandatory>
                      <v-btn value="ascii">
                        ascii
                      </v-btn>
                      <v-btn value="asciicase">
                        ascii (case sensitive)
                      </v-btn>
                      <v-btn value="hex">
                        hex
                      </v-btn>
                      <v-btn value="regex">
                        safe regex
                      </v-btn>
                      <v-btn value="hexregex">
                        safe hex regex
                      </v-btn>
                    </v-btn-toggle>
                    <a
                      href="https://github.com/google/re2/wiki/Syntax"
                      target="_blank"
                      id="safeRegexHelp"
                      class="ms-2">
                      <v-icon
                        icon="mdi-help-circle"
                        size="small" />
                      <v-tooltip activator="#safeRegexHelp">{{ $t('hunts.safeRegexHelpTip') }}</v-tooltip>
                    </a>
                  </v-col>
                </v-row>
                <v-row class="g-1">
                  <!-- packet search direction -->
                  <v-col
                    cols="12"
                    lg="3"
                    md="12">
                    <v-checkbox
                      :model-value="jobSrc"
                      @update:model-value="jobSrc = $event"
                      :label="$t('hunts.jobSrc')"
                      density="compact"
                      hide-details
                      color="primary" />
                    <v-checkbox
                      :model-value="jobDst"
                      @update:model-value="jobDst = $event"
                      :label="$t('hunts.jobDst')"
                      density="compact"
                      hide-details
                      color="primary" />
                  </v-col> <!-- /packet search direction -->
                  <!-- packet search type -->
                  <v-col
                    cols="12"
                    lg="3"
                    md="12">
                    <v-radio-group
                      :model-value="jobType"
                      @update:model-value="setJobType($event)"
                      density="compact"
                      hide-details>
                      <v-radio
                        :label="$t('hunts.jobType-raw')"
                        value="raw"
                        color="primary" />
                      <v-radio
                        :label="$t('hunts.jobType-reassembled')"
                        value="reassembled"
                        color="primary" />
                    </v-radio-group>
                  </v-col> <!-- /packet search type -->
                  <!-- sharing with users/roles -->
                  <v-col
                    cols="12"
                    lg="6"
                    md="12"
                    class="d-flex"
                    v-if="!anonymousMode">
                    <div class="align-self-start">
                      <RoleDropdown
                        size="large"
                        :roles="roles"
                        :selected-roles="jobRoles"
                        :display-text="$t('common.shareWithRoles')"
                        @selected-roles-updated="updateNewJobRoles" />
                    </div>
                    <div class="flex-grow-1 ms-2">
                      <div class="arkime-input-group arkime-input-group--fluid">
                        <span
                          id="jobUsers"
                          class="arkime-input-label cursor-help">
                          <v-icon icon="mdi-account" />
                          <v-tooltip activator="#jobUsers">
                            {{ $t('hunts.jobUsersTip') }}
                          </v-tooltip>
                        </span>
                        <input
                          type="text"
                          v-model="jobUsers"
                          :placeholder="$t('hunts.jobUsersPlaceholder')"
                          class="arkime-input-control">
                      </div>
                    </div>
                  </v-col> <!-- /sharing with users/roles -->
                </v-row>
                <v-row>
                  <v-col
                    cols="12"
                    class="mt-1 d-flex align-center gap-2">
                    <v-alert
                      v-if="createFormError"
                      type="error"
                      variant="tonal"
                      density="compact"
                      class="flex-grow-1">
                      <v-icon icon="mdi-alert" />&nbsp;
                      {{ createFormError }}
                    </v-alert>
                    <v-spacer v-else />
                    <!-- cancel create search job button -->
                    <v-btn
                      color="warning"
                      variant="flat"
                      size="small"
                      density="comfortable"
                      @click="cancelCreateForm"
                      title="Cancel creating this hunt">
                      <v-icon icon="mdi-cancel" />&nbsp;
                      {{ $t('common.cancel') }}
                    </v-btn> <!-- /cancel create search job button -->
                    <!-- create search job button -->
                    <v-btn
                      variant="flat"
                      size="small"
                      density="comfortable"
                      :style="tertiaryBtnStyle"
                      @click="createJob"
                      :disabled="loadingSessions || !!loadingSessionsError"
                      title="Create this hunt">
                      <v-icon icon="mdi-plus" />&nbsp;
                      {{ $t('common.create') }}
                    </v-btn> <!-- /create search job button -->
                  </v-col>
                </v-row>
              </form>
            </v-card>
          </transition>
        </div> <!-- /create new packet search job -->

        <!-- running job -->
        <transition name="slide">
          <v-card
            v-if="runningJob"
            variant="outlined"
            class="mb-3">
            <div class="pa-3">
              <h5 class="d-flex align-center gap-1 mb-2">
                <span class="flex-grow-1">
                  {{ $t('hunts.runningHuntJob', { name: runningJob.name, user: runningJob.userId }) }}
                </span>
                <template v-if="canEdit">
                  <v-btn
                    :id="`pause${runningJob.id}`"
                    @click="pauseJob(runningJob)"
                    :disabled="runningJob.loading"
                    :aria-label="$t('hunts.pauseTip')"
                    icon
                    color="warning"
                    variant="flat"
                    size="small"
                    density="comfortable"
                    class="ms-1">
                    <v-icon
                      icon="mdi-pause"
                      v-if="!runningJob.loading" />
                    <v-icon
                      icon="mdi-loading"
                      class="mdi-spin"
                      v-else />
                    <v-tooltip :activator="`[id='pause${runningJob.id}']`">
                      {{ $t('hunts.pauseTip') }}
                    </v-tooltip>
                  </v-btn>
                  <v-btn
                    :id="`cancel${runningJob.id}`"
                    @click="cancelJob(runningJob)"
                    :disabled="runningJob.disabled"
                    :aria-label="$t('hunts.cancelTip')"
                    icon
                    color="error"
                    variant="flat"
                    size="small"
                    density="comfortable"
                    class="ms-1">
                    <v-icon
                      icon="mdi-cancel"
                      v-if="!runningJob.loading" />
                    <v-icon
                      icon="mdi-loading"
                      class="mdi-spin"
                      v-else />
                    <v-tooltip :activator="`[id='cancel${runningJob.id}']`">
                      {{ $t('hunts.cancelTip') }}
                    </v-tooltip>
                  </v-btn>
                </template>
                <template v-if="canView">
                  <v-btn
                    @click="openSessions(runningJob)"
                    v-if="runningJob.matchedSessions"
                    :id="`openresults${runningJob.id}`"
                    :aria-label="$t('common.open')"
                    icon
                    variant="flat"
                    size="small"
                    density="comfortable"
                    :style="primaryBtnStyle"
                    class="ms-1">
                    <v-icon icon="mdi-folder-open" />
                    <v-tooltip :activator="`[id='openresults${runningJob.id}']`">
                      <span v-html="$t('hunts.openResultsTipHtml')" />
                    </v-tooltip>
                  </v-btn>
                </template>
                <v-btn
                  v-if="canEdit"
                  :id="`remove${runningJob.id}`"
                  @click="removeJob(runningJob, 'results')"
                  :disabled="runningJob.disabled"
                  :aria-label="$t('hunts.removeHuntTip')"
                  icon
                  color="error"
                  variant="flat"
                  size="small"
                  density="comfortable"
                  class="ms-1">
                  <v-icon
                    icon="mdi-trash-can-outline"
                    v-if="!runningJob.loading" />
                  <v-icon
                    icon="mdi-loading"
                    class="mdi-spin"
                    v-else />
                  <v-tooltip :activator="`[id='remove${runningJob.id}']`">
                    {{ $t('hunts.cancelAndRemoveTip') }}
                  </v-tooltip>
                </v-btn>
              </h5>
              <div>
                <v-row>
                  <v-col
                    cols="auto"
                    class="d-flex align-center gap-2">
                    <toggle-btn
                      v-if="canView"
                      :opened="runningJob.expanded"
                      @toggle="toggleJobDetail(runningJob)" />
                    <v-progress-linear
                      id="runningJob"
                      color="success"
                      striped
                      height="26"
                      :model-value="runningJob.progress"
                      class="cursor-help flex-grow-1">
                      <template #default>
                        {{ round(runningJob.progress, 1) }}%
                      </template>
                    </v-progress-linear>
                    <v-tooltip activator="#runningJob">
                      <div class="mt-2">
                        <span v-html="$t('hunts.runningJob-headHtml', { matched: commaString(runningJob.matchedSessions) })" />
                        <span
                          v-if="canView"
                          v-html="$t('hunts.runningJob-canViewHtml', { search: escapeHtml(runningJob.search), searchType: runningJob.searchType })" />
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
                    </v-tooltip>
                  </v-col>
                </v-row>
                <transition name="grow">
                  <div
                    v-if="runningJob.expanded"
                    class="mt-3">
                    <v-row>
                      <v-col cols="12">
                        <v-icon icon="mdi-card-account-details" />&nbsp;
                        {{ $t('hunts.huntJobId', { id: runningJob.id }) }}:
                      </v-col>
                    </v-row>
                    <hunt-data
                      :job="runningJob"
                      @remove-user="removeUser"
                      @add-users="addUsers"
                      :user="user"
                      @update-hunt="updateHunt" />
                  </div>
                </transition>
              </div>
            </div>
          </v-card>
        </transition> <!-- /running job -->

        <h4 v-if="results.length">
          <v-icon icon="mdi-format-list-numbered" />&nbsp;
          {{ $t('hunts.huntJobQueue') }}:
        </h4>

        <!-- hunt job queue errors -->
        <v-alert
          v-if="queuedListError"
          type="error"
          variant="tonal"
          density="compact">
          {{ queuedListError }}
        </v-alert>
        <v-alert
          v-if="queuedListLoadingError"
          type="error"
          variant="tonal"
          density="compact">
          {{ $t('hunts.errorLoadingQueue') }}:
          {{ queuedListLoadingError }}
        </v-alert> <!-- /hunt job queue errors -->

        <table
          v-if="results.length"
          class="arkime-table mb-4">
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
                @open-sessions="openSessions" />
              <tr
                :key="`${job.id}-detail`"
                v-if="job.expanded">
                <td colspan="10">
                  <hunt-data
                    :job="job"
                    @remove-user="removeUser"
                    @add-users="addUsers"
                    :user="user"
                    @update-hunt="updateHunt" />
                </td>
              </tr>
            </template> <!-- /packet search jobs -->
          </transition-group>
        </table>

        <!-- hunt job history errors -->
        <v-alert
          v-if="historyListError"
          type="error"
          variant="tonal"
          density="compact">
          <v-icon
            icon="mdi-alert"
            class="me-2" />
          {{ historyListError }}
        </v-alert>
        <v-alert
          v-if="historyListLoadingError"
          type="error"
          variant="tonal"
          density="compact">
          <v-icon
            icon="mdi-alert"
            class="me-2" />
          {{ $t('hunts.errorLoadingHistory') }}:
          {{ historyListLoadingError }}
        </v-alert> <!-- /hunt job history errors -->

        <template v-if="!historyListLoadingError">
          <h4>
            <v-icon
              icon="mdi-clock-outline"
              class="me-2" />
            {{ $t('hunts.title') }}
          </h4>
          <v-row class="g-1 justify-start">
            <v-col cols="auto">
              <!-- search packet search jobs -->
              <div class="arkime-input-group arkime-input-group--fluid">
                <span class="arkime-input-label arkime-input-label-fw">
                  <v-icon icon="mdi-magnify" />
                </span>
                <input
                  type="text"
                  v-model="query.searchTerm"
                  @input="debounceSearch"
                  :placeholder="$t('hunts.querySearchTermPlaceholder')"
                  class="arkime-input-control">
                <v-btn
                  v-if="query.searchTerm"
                  variant="text"
                  size="x-small"
                  density="comfortable"
                  icon
                  class="arkime-input-append-btn"
                  :disabled="!query.searchTerm"
                  @click="clear">
                  <v-icon icon="mdi-close" />
                </v-btn>
              </div> <!-- /search packet search jobs -->
            </v-col>
            <v-col cols="auto">
              <!-- job history paging -->
              <arkime-paging
                :records-total="historyResults.recordsTotal"
                :records-filtered="historyResults.recordsFiltered"
                @change-paging="changePaging" /> <!-- /job history paging -->
            </v-col>
          </v-row>

          <table class="arkime-table">
            <thead>
              <tr>
                <th width="40px">
&nbsp;
                </th>
                <th
                  class="cursor-pointer"
                  @click="columnClick('status')">
                  {{ $t('hunts.jobStatus') }}
                  <v-icon
                    icon="mdi-chevron-up"
                    v-show="query.sortField === 'status' && !query.desc" />
                  <v-icon
                    icon="mdi-chevron-down"
                    v-show="query.sortField === 'status' && query.desc" />
                </th>
                <th>
                  {{ $t('hunts.jobMatches') }}
                </th>
                <th
                  class="cursor-pointer"
                  @click="columnClick('name')">
                  {{ $t('hunts.jobName') }}
                  <v-icon
                    icon="mdi-chevron-up"
                    v-show="query.sortField === 'name' && !query.desc" />
                  <v-icon
                    icon="mdi-chevron-down"
                    v-show="query.sortField === 'name' && query.desc" />
                </th>
                <th
                  class="cursor-pointer no-wrap"
                  @click="columnClick('userId')">
                  {{ $t('hunts.jobUser') }}
                  <v-icon
                    icon="mdi-chevron-up"
                    v-show="query.sortField === 'userId' && !query.desc" />
                  <v-icon
                    icon="mdi-chevron-down"
                    v-show="query.sortField === 'userId' && query.desc" />
                </th>
                <th>
                  {{ $t('hunts.jobSearch') }}
                </th>
                <th
                  class="cursor-pointer"
                  @click="columnClick('created')">
                  {{ $t('common.created') }}
                  <v-icon
                    icon="mdi-chevron-up"
                    v-show="query.sortField === 'created' && !query.desc" />
                  <v-icon
                    icon="mdi-chevron-down"
                    v-show="query.sortField === 'created' && query.desc" />
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
                  @remove-from-sessions="removeFromSessions" />
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
                      @update-hunt="updateHunt" />
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
                <v-icon
                  icon="mdi-folder-open"
                  size="x-large"
                  class="text-muted-more" />&nbsp;
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
        <v-card
          v-if="floatingError || floatingSuccess"
          variant="outlined"
          class="floating-msg">
          <div class="pa-3">
            <a
              @click="floatingError = false; floatingSuccess = false"
              id="dismissError"
              class="no-decoration cursor-pointer float-right">
              <v-icon icon="mdi-close" />
              <v-tooltip activator="#dismissError">{{ $t('common.dismiss') }}</v-tooltip>
            </a>
            <span :class="floatingError ? 'text-danger' : 'text-success'">
              <v-icon
                icon="mdi-alert"
                class="me-2"
                v-if="floatingError" />
              <v-icon
                icon="mdi-check"
                class="me-2"
                v-else />
              {{ floatingError || floatingSuccess }}
            </span>
          </div>
        </v-card>
      </transition> <!-- /floating error -->
    </div>
  </page-layout>
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
import ArkimeError from '../utils/Error.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ArkimePaging from '@common/Pagination.vue';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';
import PageLayout from '../utils/PageLayout.vue';
import Focus from '@common/Focus.vue';
import HuntData from './HuntData.vue';
import HuntRow from './HuntRow.vue';
import RoleDropdown from '@common/RoleDropdown.vue';
import NotifierDropdown from '@common/NotifierDropdown.vue';
import { commaString, round, escapeHtml } from '@common/vueFilters.js';
import { resolveMessage } from '@common/resolveI18nMessage';
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
    ArkimeError,
    ArkimeSearch,
    ArkimeLoading,
    ArkimeCollapsible,
    PageLayout,
    ArkimePaging,
    HuntData,
    HuntRow,
    RoleDropdown,
    NotifierDropdown
  },
  directives: { Focus },
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
      loadingSessionsDetailError: '',
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
      jobNotifier: [],
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
      multiviewer: this.$constants.MULTIVIEWER,
      // Arkime theme-color v-btn styles. Vuetify :color can't take CSS vars.
      primaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-primary))',
        color: 'rgb(var(--v-theme-button-fg))'
      },
      tertiaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-tertiary))',
        color: 'rgb(var(--v-theme-button-fg))'
      }
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
    escapeHtml,
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
    updateJobNotifiers (notifiers) {
      this.jobNotifier = notifiers;
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
        this.jobNotifier = [];
        this.loadData();
      }).catch((error) => {
        this.createFormError = resolveMessage(error, this.$t);
      });
    },
    removeFromSessions: function (job) {
      if (job.loading) { return; } // it's already trying to do something

      this.setErrorForList('historyResults', '');
      job.loading = true;

      HuntService.cleanup(job.id, this.query.cluster).then((response) => {
        job.loading = false;
        job.removed = true;
        this.floatingSuccess = resolveMessage(response, this.$t) || 'Successfully removed hunt ID and name from the matched sessions.';
        setTimeout(() => {
          this.floatingSuccess = '';
        }, 5000);
      }).catch((error) => {
        job.loading = false;
        this.setErrorForList('historyResults', resolveMessage(error, this.$t));
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
        this.setErrorForList(arrayName, resolveMessage(error, this.$t));
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
        this.setErrorForList('results', resolveMessage(error, this.$t));
      });
    },
    pauseJob: function (job) {
      if (job.loading) { return; } // it's already trying to do something

      this.setErrorForList('results', '');
      job.loading = true;

      HuntService.pause(job.id, this.query.cluster).then((response) => {
        this.loadData();
      }).catch((error) => {
        job.loading = false;
        this.setErrorForList('results', resolveMessage(error, this.$t));
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
        this.setErrorForList('results', resolveMessage(error, this.$t));
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
      this.jobUsers = job.users?.join(',') || '';
      this.jobRoles = job.roles || [];
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
        this.floatingError = resolveMessage(error, this.$t);
      });
    },
    addUsers: function (users, job) {
      this.floatingError = '';

      HuntService.addUsers(job.id, users, this.query.cluster).then((response) => {
        job.users = response.users;
      }).catch((error) => {
        this.floatingError = resolveMessage(error, this.$t);
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
        this.floatingSuccess = resolveMessage(response, this.$t);
        setTimeout(() => {
          this.floatingSuccess = '';
        }, 5000);
      }).catch((error) => {
        this.floatingError = resolveMessage(error, this.$t);
      });
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
        this.historyListLoadingError = resolveMessage(error, this.$t);
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
        this.queuedListLoadingError = resolveMessage(error, this.$t);
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
      this.loadingSessionsDetailError = '';

      // create unique cancel id to make cancel req for corresponding es task
      const cancelId = Utils.createRandomString();
      this.sessionsQuery.cancelId = cancelId;

      try {
        const { controller, fetcher } = SessionsService.get(this.sessionsQuery, false);
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
        this.loadingSessionsDetailError = resolveMessage(error, this.$t) || '';
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
  background-color: rgb(var(--v-theme-quaternary-lightest));

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
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

/* floating message: v-card with Arkime-themed surface + drop shadow */
.floating-msg {
  position: fixed;
  bottom: 15px;
  left: 10px;
  z-index: 999;
  width: 350px;
  background-color: rgb(var(--v-theme-neutral-lighter)) !important;
  border: 1px solid rgb(var(--v-theme-neutral-light)) !important;
  box-shadow: 4px 4px 16px -2px black;
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
