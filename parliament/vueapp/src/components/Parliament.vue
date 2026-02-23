<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="container-fluid">
    <!-- page error -->
    <b-alert
      dismissible
      :show="!!error"
      variant="danger"
      style="z-index: 2000;"
      class="position-fixed fixed-bottom m-0 rounded-0">
      <span class="fa fa-exclamation-triangle me-2" />
      {{ error }}
    </b-alert> <!-- /page error -->

    <!-- search & create group -->
    <div class="d-flex flex-row justify-content-between align-items-center flex-nowrap">
      <!-- search -->
      <b-input-group class="me-2">
        <b-input-group-text>
          <span class="fa fa-search" />
        </b-input-group-text>
        <b-form-input
          tabindex="8"
          debounce="400"
          v-model="searchTerm"
          placeholder="Search clusters" />
        <button
          type="button"
          @click="clear"
          :disabled="!searchTerm"
          class="btn btn-outline-secondary btn-clear-input">
          <span class="fa fa-close" />
        </button>
      </b-input-group>  <!-- /search -->
      <div
        v-if="isAdmin"
        class="no-wrap d-flex">
        <!-- create group -->
        <a
          v-if="!showNewGroupForm && editMode"
          @click="openNewGroupForm"
          class="btn btn-outline-primary cursor-pointer me-1">
          <span class="fa fa-plus-circle" />&nbsp;
          {{ $t('parliament.newGroup') }}
        </a>
        <template v-else-if="editMode">
          <a
            @click="cancelCreateNewGroup"
            class="btn btn-outline-warning cursor-pointer me-1">
            <span class="fa fa-ban" />&nbsp;
            {{ $t('common.cancel') }}
          </a>
          <a
            @click="createNewGroup"
            class="btn btn-outline-success cursor-pointer me-1">
            <span class="fa fa-plus-circle" />&nbsp;
            {{ $t('common.create') }}
          </a>
        </template> <!-- /create group -->
        <!-- hide/show issues button -->
        <button
          @click="toggleHideAllIssues"
          class="btn btn-sm btn-outline-secondary me-2"
          id="hideIssuesBtn"
          :aria-label="$t(hideAllIssues ? 'parliament.showAllIssues' : 'parliament.hideAllIssues')">
          <span
            v-if="hideAllIssues"
            class="fa fa-eye" />
          <span
            v-else
            class="fa fa-eye-slash" />
        </button>
        <BTooltip
          target="hideIssuesBtn"
          placement="bottom">
          {{ $t(hideAllIssues ? 'parliament.showAllIssues' : 'parliament.hideAllIssues') }}
        </BTooltip>
        <!-- /hide/show issues button -->
        <!-- edit mode toggle -->
        <span
          @click="toggleEditMode"
          class="fa fa-toggle-off fa-2x cursor-pointer mt-1"
          :class="{'fa-toggle-off':!editMode, 'fa-toggle-on text-success':editMode}"
          id="editModeToggle" />
        <BTooltip
          target="editModeToggle"
          placement="bottom">
          {{ $t('parliament.editModeToggleTip') }}
        </BTooltip>
        <!-- /edit mode toggle -->
      </div>
    </div> <!-- /search & create group -->

    <hr>

    <!-- new group form -->
    <div
      v-if="showNewGroupForm && isAdmin && editMode"
      class="row">
      <div class="col-md-12">
        <div @keyup.enter="createNewGroup">
          <div class="form-group row mb-1">
            <label
              for="newGroupTitle"
              class="col-sm-2 col-form-label">
              {{ $t('parliament.groupTitle') }}<sup class="text-danger">*</sup>
            </label>
            <div class="col-sm-10">
              <input
                type="text"
                v-model="newGroupTitle"
                class="form-control"
                id="newGroupTitle"
                :placeholder="$t('parliament.groupTitlePlaceholder')"
                v-focus="focusGroupInput">
            </div>
          </div>
          <div class="form-group row mb-1">
            <label
              for="newGroupDescription"
              class="col-sm-2 col-form-label">
              {{ $t('parliament.groupDescription') }}
            </label>
            <div class="col-sm-10">
              <textarea
                rows="3"
                v-model="newGroupDescription"
                class="form-control"
                id="newGroupDescription"
                :placeholder="$t('parliament.groupDescriptionPlaceholder')" />
            </div>
          </div>
        </div>
        <hr>
      </div>
    </div> <!-- /new group form -->

    <!-- no results for searchTerm filter -->
    <div
      v-if="searchTerm && !numFilteredClusters && parliament.groups && parliament.groups.length"
      class="info-area vertical-center">
      <div class="text-muted">
        <span class="fa fa-3x fa-folder-open text-muted-more" />
        {{ $t('parliament.noClustersMatch') }}
      </div>
    </div> <!-- /no results for searchTerm filter -->

    <!-- no groups -->
    <div
      v-if="parliament.groups && !parliament.groups.length && !showNewGroupForm"
      class="info-area text-center vertical-center">
      <div class="text-muted mt-5">
        <span class="fa fa-3x fa-folder-open text-muted-more" />
        {{ $t('parliament.noClusters') }}
        <a
          v-if="isAdmin && editMode"
          @click="showNewGroupForm = true"
          class="cursor-pointer no-href no-decoration">
          {{ $t('parliament.createGroup') }}
        </a>
        <template v-else-if="isAdmin">
          <p class="d-flex justify-content-between align-items-center mb-0 mt-3">
            <span class="fa fa-lock text-muted-more me-3" />
            {{ $t('parliament.needEditMode') }}
            <span class="fa fa-lock text-muted-more ms-3" />
          </p>
          <a
            @click="toggleEditMode"
            class="cursor-pointer no-href no-decoration">
            {{ $t('parliament.enableEditMode') }}
          </a>
        </template>
      </div>
    </div> <!-- /no groups -->

    <!-- groups -->
    <div ref="draggableGroups">
      <div
        v-for="group of filteredParliament.groups"
        :id="group.id"
        :key="group.id"
        class="mb-4">
        <!-- group title/description -->
        <div
          class="row group"
          v-if="!group.clusters || (group.clusters.length > 0 || (!group.clusters.length && !searchTerm))">
          <div class="col-md-12">
            <h5 class="mb-1 pb-1">
              <span
                v-if="isAdmin && !searchTerm && editMode"
                class="group-handle fa fa-th" />
              <!-- group action buttons -->
              <span v-if="isAdmin && editMode">
                <a
                  v-if="groupAddingCluster !== group.id && groupBeingEdited !== group.id"
                  @click="displayNewClusterForm(group)"
                  class="btn btn-sm btn-outline-info pull-right cursor-pointer mb-1">
                  <span class="fa fa-plus-circle" />&nbsp;
                  {{ $t('parliament.newCluster') }}
                </a>
                <a
                  v-if="groupAddingCluster === group.id"
                  @click="createNewCluster(group)"
                  class="btn btn-sm btn-outline-success cursor-pointer pull-right mb-1">
                  <span class="fa fa-plus-circle" />&nbsp;
                  {{ $t('common.create') }}
                </a>
                <a
                  v-if="groupBeingEdited === group.id"
                  @click="editGroup(group)"
                  class="btn btn-sm btn-outline-success pull-right cursor-pointer me-1 mb-1">
                  <span class="fa fa-save" />&nbsp;
                  {{ $t('common.save') }}
                </a>
                <a
                  v-if="groupAddingCluster === group.id || groupBeingEdited === group.id"
                  @click="cancelUpdateGroup(group)"
                  class="btn btn-sm btn-outline-warning cursor-pointer pull-right me-1 mb-1">
                  <span class="fa fa-ban" />&nbsp;
                  {{ $t('common.cancel') }}
                </a>
                <a
                  v-if="groupBeingEdited !== group.id && groupAddingCluster !== group.id"
                  @click="displayEditGroupForm(group)"
                  class="btn btn-sm btn-outline-warning pull-right cursor-pointer me-1 mb-1">
                  <span class="fa fa-pencil" />&nbsp;
                  {{ $t('parliament.editGroup') }}
                </a>
              </span> <!-- /group action buttons -->
              {{ group.title }}&nbsp;
              <template v-if="isAdmin && groupAddingCluster !== group.id && groupBeingEdited === group.id && editMode">
                <a
                  @click="deleteGroup(group)"
                  :id="`deleteGroupTooltip-${group.id}`"
                  class="btn btn-sm btn-outline-danger cursor-pointer ms-2">
                  <span class="fa fa-trash-o" />
                </a>
                <BTooltip
                  :target="`deleteGroupTooltip-${group.id}`"
                  placement="top">
                  {{ $t('parliament.deleteGroupTip') }}
                </BTooltip>
              </template>
            </h5>
            <p class="mb-2">
              {{ group.description }}
            </p>
          </div>
        </div> <!-- /group title/description -->

        <!-- edit group form -->
        <div
          v-if="isAdmin && groupBeingEdited === group.id && editMode"
          class="row">
          <div class="col-md-12">
            <form>
              <div class="form-group row mb-1">
                <label
                  for="editGroupTitle"
                  class="col-sm-2 col-form-label">
                  {{ $t('parliament.groupTitle') }}<sup class="text-danger">*</sup>
                </label>
                <div class="col-sm-10">
                  <input
                    type="text"
                    v-model="group.newTitle"
                    class="form-control"
                    id="editGroupTitle"
                    :placeholder="$t('parliament.groupTitlePlaceholder')"
                    v-focus="focusGroupInput">
                </div>
              </div>
              <div class="form-group row mb-1">
                <label
                  for="editGroupDescription"
                  class="col-sm-2 col-form-label">
                  {{ $t('parliament.groupDescription') }}
                </label>
                <div class="col-sm-10">
                  <textarea
                    rows="3"
                    v-model="group.newDescription"
                    class="form-control"
                    id="editGroupDescription"
                    :placeholder="$t('parliament.groupDescriptionPlaceholder')" />
                </div>
              </div>
            </form>
            <hr>
          </div>
        </div> <!-- /edit group form -->

        <!-- create cluster form -->
        <div v-if="isAdmin && groupAddingCluster === group.id && editMode">
          <div class="col-md-12">
            <hr>
            <form @keyup.enter="createNewCluster(group)">
              <div class="form-group row mb-1">
                <label
                  for="newClusterTitle"
                  class="col-sm-2 col-form-label">
                  {{ $t('parliament.clusterTitle') }}<sup class="text-danger">*</sup>
                </label>
                <div class="col-sm-10">
                  <input
                    type="text"
                    v-model="group.newClusterTitle"
                    class="form-control"
                    id="newClusterTitle"
                    :placeholder="$t('parliament.clusterTitlePlaceholder')"
                    v-focus="focusClusterInput">
                </div>
              </div>
              <div class="form-group row mb-1">
                <label
                  for="newClusterDescription"
                  class="col-sm-2 col-form-label">
                  {{ $t('parliament.clusterDescription') }}
                </label>
                <div class="col-sm-10">
                  <textarea
                    rows="3"
                    v-model="group.newClusterDescription"
                    class="form-control"
                    id="newClusterDescription"
                    :placeholder="$t('parliament.clusterDescriptionPlaceholder')" />
                </div>
              </div>
              <div class="form-group row mb-1">
                <label
                  for="newClusterUrl"
                  class="col-sm-2 col-form-label">
                  {{ $t('parliament.clusterUrl') }}<sup class="text-danger">*</sup>
                </label>
                <div class="col-sm-10">
                  <input
                    type="text"
                    v-model="group.newClusterUrl"
                    class="form-control"
                    id="newClusterUrl"
                    :placeholder="$t('parliament.clusterUrlPlaceholder')">
                </div>
              </div>
              <div class="form-group row mb-1">
                <label
                  for="newClusterLocalUrl"
                  class="col-sm-2 col-form-label">
                  {{ $t('parliament.clusterLocalUrl') }}
                </label>
                <div class="col-sm-10">
                  <input
                    type="text"
                    v-model="group.newClusterLocalUrl"
                    class="form-control"
                    id="newClusterLocalUrl"
                    :placeholder="$t('parliament.clusterLocalUrlPlaceholder')">
                </div>
              </div>
              <div class="row">
                <label
                  for="newClusterType"
                  class="col-sm-2 col-form-label">
                  {{ $t('parliament.clusterType') }}<sup class="text-danger">*</sup>
                </label>
                <div class="col-sm-10">
                  <select
                    v-model="group.newClusterType"
                    class="form-control"
                    id="newClusterType">
                    <option value="undefined">
                      {{ $t('parliament.clusterType-normal') }}
                    </option>
                    <option value="noAlerts">
                      {{ $t('parliament.clusterType-noAlerts') }}
                    </option>
                    <option value="multiviewer">
                      {{ $t('parliament.clusterType-multiviewer') }}
                    </option>
                    <option value="disabled">
                      {{ $t('parliament.clusterType-disabled') }}
                    </option>
                  </select>
                </div>
              </div>
            </form>
            <hr>
          </div>
        </div> <!-- /create cluster form -->

        <!-- clusters -->
        <ul
          v-if="group.clusters && group.clusters.length"
          :id="group.id"
          ref="draggableClusters"
          class="cluster-group d-flex flex-wrap row mb-4">
          <li
            v-for="cluster in group.clusters"
            :key="cluster.id"
            :id="`cluster-${cluster.id}`"
            class="cluster col-12 col-sm-6 col-md-6 col-lg-4 col-xl-3 col-xxl-2 mb-1">
            <div
              class="card"
              :class="{'cluster-highlight': highlightedClusterId === cluster.id}">
              <div class="card-body">
                <!-- cluster title -->
                <a
                  v-if="cluster.type !== 'disabled' && stats[cluster.id]"
                  class="badge badge-pill bg-secondary cursor-pointer pull-right no-decoration"
                  :href="`${cluster.url}/stats?statsTab=2`"
                  :class="{'bg-success':stats[cluster.id].status === 'green','bg-warning':stats[cluster.id].status === 'yellow','bg-danger':stats[cluster.id].status === 'red'}"
                  :id="`clusterStatsTooltip-${cluster.id}`">
                  <span v-if="stats[cluster.id].status">
                    {{ stats[cluster.id].status }}
                  </span>
                  <span
                    v-if="stats[cluster.id].healthError"
                    class="fa fa-exclamation-triangle" />
                  <span v-if="!stats[cluster.id].status && !stats[cluster.id].healthError">
                    ????
                  </span>
                  <BTooltip
                    :target="`clusterStatsTooltip-${cluster.id}`"
                    placement="top">
                    <span>{{ $t('parliament.esStatus') }}: <strong>{{ stats[cluster.id].healthError || stats[cluster.id].status || 'unreachable' }}</strong></span>
                    <span v-if="stats[cluster.id].esVersion">
                      <br>{{ $t('parliament.esVersion') }}: <strong>{{ stats[cluster.id].esVersion }}</strong>
                    </span>
                  </BTooltip>
                </a>
                <h6>
                  <span
                    v-if="isAdmin && !searchTerm && editMode"
                    class="cluster-handle">
                    <span class="fa fa-th" />
                  </span>
                  <template v-if="cluster.type === 'multiviewer'">
                    <span
                      :id="`multiviewer-${cluster.id}`"
                      class="fa fa-sitemap text-muted cursor-help me-2" />
                    <BTooltip
                      :target="`multiviewer-${cluster.id}`"
                      placement="top">
                      {{ $t('parliament.clusterType-multiviewerTip') }}
                    </BTooltip>
                  </template>
                  <template v-if="cluster.type === 'disabled'">
                    <span
                      :id="`disabled-${cluster.id}`"
                      class="text-muted fa fa-eye-slash cursor-help me-2" />
                    <BTooltip
                      :target="`disabled-${cluster.id}`"
                      placement="top">
                      {{ $t('parliament.clusterType-disabledTip') }}
                    </BTooltip>
                  </template>
                  <template v-if="cluster.type === 'noAlerts'">
                    <span
                      :id="`silent-${cluster.id}`"
                      class="text-muted cursor-help fa fa-bell-slash me-2" />
                    <BTooltip
                      :target="`silent-${cluster.id}`"
                      placement="top">
                      {{ $t('parliament.clusterType-noAlertsTip') }}
                    </BTooltip>
                  </template>
                  <a
                    v-if="cluster.type !== 'disabled'"
                    class="no-decoration"
                    :href="`${cluster.url}/sessions`">
                    {{ cluster.title }}
                  </a>
                  <span v-else>
                    {{ cluster.title }}
                  </span>
                  <a
                    :href="`${cluster.url}/stats?statsTab=0`"
                    class="no-decoration ms-2"
                    :id="`clusterStatsLink-${cluster.id}`">
                    <span class="fa fa-bar-chart" />
                  </a>
                  <BTooltip
                    :target="`clusterStatsLink-${cluster.id}`"
                    placement="top">
                    {{ $t('parliament.statsLinkTip') }}
                  </BTooltip>
                </h6> <!-- /cluster title -->
                <!-- cluster description -->
                <p
                  class="text-muted small mb-2"
                  v-if="cluster.description">
                  {{ cluster.description }}
                </p> <!-- /cluster description -->

                <!-- cluster stats -->
                <template v-if="stats[cluster.id] && (!stats[cluster.id].statsError && cluster.id !== clusterBeingEdited && cluster.type !== 'disabled' && cluster.type !== 'multiviewer') || (cluster.id === clusterBeingEdited && cluster.newType !== 'disabled' && cluster.newType !== 'multiviewer')">
                  <div
                    class="d-flex flex-wrap mt-3"
                    :class="{'align-items-stretch': cluster.id !== clusterBeingEdited, 'flex-column': cluster.id === clusterBeingEdited}">
                    <div
                      v-if="cluster.id === clusterBeingEdited || !cluster.hideDeltaBPS"
                      :id="'deltaBPS-' + cluster.id"
                      class="flex-fill me-1 ms-1"
                      :class="{'badge bg-primary mb-1': cluster.id !== clusterBeingEdited}">
                      <label :class="{'form-check-label':cluster.id === clusterBeingEdited}">
                        <input
                          v-if="isAdmin && cluster.id === clusterBeingEdited && editMode"
                          class="me-1"
                          type="checkbox"
                          :checked="!cluster.hideDeltaBPS"
                          @change="cluster.hideDeltaBPS = !cluster.hideDeltaBPS">
                        <strong class="d-inline-block pe-1">
                          {{ humanReadableBits(stats[cluster.id].deltaBPS) }}
                        </strong>
                        {{ $t('parliament.bitsPerSec') }}
                      </label>
                      <BTooltip
                        :target="'deltaBPS-' + cluster.id"
                        :title="$t('parliament.bitsPerSecTip', {count: humanReadableBits(stats[cluster.id].deltaBPS)})" />
                    </div>

                    <div
                      v-if="cluster.id === clusterBeingEdited || !cluster.hideDeltaTDPS"
                      :id="'deltaTDPS-' + cluster.id"
                      class="flex-fill me-1 ms-1"
                      :class="getDeltaTDPSClass(cluster.id)">
                      <label :class="{'form-check-label':cluster.id === clusterBeingEdited}">
                        <input
                          v-if="isAdmin && cluster.id === clusterBeingEdited && editMode"
                          class="me-1"
                          type="checkbox"
                          :checked="!cluster.hideDeltaTDPS"
                          @change="cluster.hideDeltaTDPS = !cluster.hideDeltaTDPS">
                        <strong class="d-inline-block pe-1">
                          {{ humanReadableNumber(stats[cluster.id].deltaTDPS) }}
                        </strong>
                        {{ $t('parliament.dropsPerSec') }}
                      </label>
                      <BTooltip
                        :target="'deltaTDPS-' + cluster.id"
                        :title="$t('parliament.bitsPerSecTip', {count: commaString(stats[cluster.id].deltaTDPS)})" />
                    </div>

                    <div
                      v-if="cluster.id === clusterBeingEdited || !cluster.hideMonitoring"
                      :id="'monitoring-' + cluster.id"
                      class="flex-fill me-1 ms-1"
                      :class="getMonitoringClass(cluster.id)">
                      <label :class="{'form-check-label':cluster.id === clusterBeingEdited}">
                        <input
                          v-if="isAdmin && cluster.id === clusterBeingEdited && editMode"
                          class="me-1"
                          type="checkbox"
                          :checked="!cluster.hideMonitoring"
                          @change="cluster.hideMonitoring = !cluster.hideMonitoring">
                        <strong class="d-inline-block pe-1">
                          {{ humanReadableNumber(stats[cluster.id].monitoring) }}
                        </strong>
                        {{ $t('common.sessions') }}
                      </label>
                      <BTooltip
                        :target="'monitoring-' + cluster.id"
                        :title="$t('parliament.sessionsTip', {count: commaString(stats[cluster.id].monitoring)})" />
                    </div>

                    <div
                      v-if="cluster.id === clusterBeingEdited || !cluster.hideArkimeNodes"
                      :id="'arkimeNodes-' + cluster.id"
                      class="flex-fill me-1 ms-1"
                      :class="{'badge bg-secondary mb-1': cluster.id !== clusterBeingEdited}">
                      <label :class="{'form-check-label':cluster.id === clusterBeingEdited}">
                        <input
                          v-if="isAdmin && cluster.id === clusterBeingEdited && editMode"
                          class="me-1"
                          type="checkbox"
                          :checked="!cluster.hideArkimeNodes"
                          @change="cluster.hideArkimeNodes = !cluster.hideArkimeNodes">
                        <strong class="d-inline-block pe-1">
                          {{ commaString(stats[cluster.id].arkimeNodes) }}
                        </strong>
                        {{ $t('parliament.captureNodes') }}
                      </label>
                      <BTooltip
                        :target="'arkimeNodes-' + cluster.id"
                        :title="$t('parliament.captureNodesTip', {count: commaString(stats[cluster.id].arkimeNodes)})" />
                    </div>

                    <div
                      v-if="cluster.id === clusterBeingEdited || !cluster.hideDataNodes || !cluster.hideTotalNodes"
                      :id="'dataNodes-' + cluster.id"
                      class="flex-fill me-1 ms-1"
                      :class="{'badge bg-dark mb-1': cluster.id !== clusterBeingEdited}">
                      <label :class="{'form-check-label':cluster.id === clusterBeingEdited}">
                        <input
                          v-if="isAdmin && cluster.id === clusterBeingEdited && editMode"
                          class="me-1"
                          type="checkbox"
                          :checked="!cluster.hideDataNodes"
                          @change="cluster.hideDataNodes = !cluster.hideDataNodes">
                        <strong
                          class="d-inline-block pe-1"
                          v-if="!cluster.hideDataNodes || cluster.id === clusterBeingEdited">
                          {{ commaString(stats[cluster.id].dataNodes) }}
                        </strong>
                        <span v-if="cluster.id === clusterBeingEdited || (!cluster.hideDataNodes && !cluster.hideTotalNodes)">/</span>
                        <input
                          v-if="isAdmin && cluster.id === clusterBeingEdited && editMode"
                          class="ms-1 me-1"
                          type="checkbox"
                          :checked="!cluster.hideTotalNodes"
                          @change="cluster.hideTotalNodes = !cluster.hideTotalNodes">
                        <strong
                          class="d-inline-block ps-1"
                          v-if="!cluster.hideTotalNodes || cluster.id === clusterBeingEdited">
                          {{ commaString(stats[cluster.id].totalNodes) }}
                        </strong>
                      </label>
                      <BTooltip
                        :target="'dataNodes-' + cluster.id"
                        :title="getDataNodesTooltip(cluster.id)" />
                      {{ $t('parliament.dbNodes') }}
                    </div>
                  </div>
                </template>
                <!-- /cluster stats -->

                <!-- cluster issues -->
                <small v-if="!hideAllIssues && issues[cluster.id]">
                  <template v-if="showMoreIssuesFor.indexOf(cluster.id) > -1">
                    <div
                      v-for="(issue, index) in issues[cluster.id]"
                      :key="getIssueTrackingId(issue)">
                      <issue
                        :issue="issue"
                        :group-id="group.id"
                        :cluster-id="cluster.id"
                        :index="index"
                        @issue-change="issueChange" />
                    </div>
                    <a
                      v-if="issues[cluster.id].length > 5"
                      href="javascript:void(0)"
                      class="no-decoration"
                      @click="showLessIssues(cluster)">
                      {{ $t('parliament.showFewerIssues') }}
                    </a>
                  </template>
                  <template v-else>
                    <div
                      v-for="(issue, index) in issues[cluster.id].slice(0, 4)"
                      :key="getIssueTrackingId(issue)"
                      class="pt-1">
                      <issue
                        :issue="issue"
                        :group-id="group.id"
                        :cluster-id="cluster.id"
                        :index="index"
                        @issue-change="issueChange" />
                    </div>
                    <a
                      v-if="issues[cluster.id].length > 5"
                      href="javascript:void(0)"
                      class="no-decoration"
                      @click="showMoreIssues(cluster)">
                      {{ $t('parliament.showMoreIssues') }}
                    </a>
                  </template>
                </small> <!-- /cluster issues -->
                <!-- edit cluster form -->
                <div
                  v-if="isAdmin && cluster.id === clusterBeingEdited && editMode"
                  class="small">
                  <hr class="mt-2">
                  <form class="edit-cluster">
                    <div class="form-group">
                      <label for="newClusterTitle">
                        {{ $t('parliament.clusterTitle') }}<sup class="text-danger">*</sup>
                      </label>
                      <input
                        type="text"
                        v-model="cluster.newTitle"
                        class="form-control form-control-sm"
                        id="newClusterTitle"
                        :placeholder="$t('parliament.clusterTitlePlaceholder')"
                        v-focus="focusClusterInput">
                    </div>
                    <div class="form-group">
                      <label for="newClusterDescription">
                        {{ $t('parliament.clusterDescription') }}
                      </label>
                      <textarea
                        rows="3"
                        v-model="cluster.newDescription"
                        class="form-control form-control-sm"
                        id="newClusterDescription"
                        :placeholder="$t('parliament.clusterDescriptionPlaceholder')" />
                    </div>
                    <div class="form-group">
                      <label for="newClusterUrl">
                        {{ $t('parliament.clusterUrl') }}<sup class="text-danger">*</sup>
                      </label>
                      <input
                        type="text"
                        v-model="cluster.newUrl"
                        class="form-control form-control-sm"
                        id="newClusterUrl"
                        :placeholder="$t('parliament.clusterUrlPlaceholder')">
                    </div>
                    <div class="form-group">
                      <label for="newClusterLocalUrl">
                        {{ $t('parliament.clusterLocalUrl') }}
                      </label>
                      <input
                        type="text"
                        v-model="cluster.newLocalUrl"
                        class="form-control form-control-sm"
                        id="newClusterLocalUrl"
                        :placeholder="$t('parliament.clusterLocalUrlPlaceholder')">
                    </div>
                    <div class="form-group">
                      <label for="newClusterType">
                        {{ $t('parliament.clusterType') }}<sup class="text-danger">*</sup>
                      </label>
                      <select
                        v-model="cluster.newType"
                        class="form-control form-control-sm"
                        id="newClusterType">
                        <option value="undefined">
                          {{ $t('parliament.clusterType-normal') }}
                        </option>
                        <option value="noAlerts">
                          {{ $t('parliament.clusterType-noAlerts') }}
                        </option>
                        <option value="multiviewer">
                          {{ $t('parliament.clusterType-multiviewer') }}
                        </option>
                        <option value="disabled">
                          {{ $t('parliament.clusterType-disabled') }}
                        </option>
                      </select>
                    </div>
                  </form>
                </div> <!-- /edit cluster form -->
              </div>
              <!-- edit cluster buttons -->
              <div
                v-if="isUser && ((isUser && !hideAllIssues && issues[cluster.id] && issues[cluster.id].length && cluster.id !== clusterBeingEdited) || (isAdmin && editMode))"
                class="card-footer small">
                <a
                  v-if="!hideAllIssues && issues[cluster.id] && issues[cluster.id].length && cluster.id !== clusterBeingEdited"
                  @click="acknowledgeAllIssues(cluster)"
                  :id="`ackAllIssuesTooltip-${cluster.id}`"
                  class="btn btn-sm btn-outline-success pull-right cursor-pointer">
                  <span class="fa fa-check" />
                </a>
                <BTooltip
                  :target="`ackAllIssuesTooltip-${cluster.id}`"
                  placement="top">
                  {{ $t('parliament.ackAllIssuesTip') }}
                </BTooltip>
                <span v-if="(isUser && !hideAllIssues && issues[cluster.id] && issues[cluster.id].length) || (isAdmin && editMode)">
                  <a
                    v-show="cluster.id !== clusterBeingEdited && editMode && isAdmin"
                    class="btn btn-sm btn-outline-warning cursor-pointer"
                    @click="displayEditClusterForm(cluster)">
                    <span class="fa fa-pencil" />
                  </a>
                  <span v-show="cluster.id === clusterBeingEdited && editMode && isAdmin">
                    <a
                      class="btn btn-sm btn-outline-success pull-right cursor-pointer"
                      @click="editCluster(group, cluster)">
                      <span class="fa fa-save" />&nbsp;
                      {{ $t('common.save') }}
                    </a>
                    <a
                      class="btn btn-sm btn-outline-warning pull-right cursor-pointer me-1"
                      @click="cancelEditCluster(cluster)">
                      <span class="fa fa-ban" />
                    </a>
                    <a
                      class="btn btn-sm btn-outline-danger cursor-pointer me-1"
                      @click="deleteCluster(group, cluster)">
                      <span class="fa fa-trash-o" />
                    </a>
                  </span>
                </span>
              </div> <!-- /edit cluster buttons -->
            </div>
          </li>
        </ul> <!-- /clusters -->

        <!-- no clusters -->
        <div v-if="(!group.clusters || !group.clusters.length) && !searchTerm && groupAddingCluster !== group.id">
          <strong>
            {{ $t('parliament.noClustersInGroup') }}
            <a
              @click="displayNewClusterForm(group)"
              v-if="isAdmin && editMode"
              class="no-decoration cursor-pointer no-href">
              {{ $t('parliament.createGroup') }}
            </a>
          </strong>
        </div> <!-- no clusters -->
      </div>
    </div> <!-- /groups -->
  </div>
</template>

<script>
import ParliamentService from './parliament.service.js';
import Issue from './Issue.vue';
import Focus from '@common/Focus.vue';
import { commaString, humanReadableBits, humanReadableNumber } from '@common/vueFilters.js';

import Sortable from 'sortablejs';

let timeout;
let interval;
let draggableGroups;
let draggableClusters;
let highlightTimeout;
let fallbackTimeout;

export default {
  name: 'Parliament',
  components: {
    Issue
  },
  directives: {
    Focus
  },
  data () {
    return {
      // page error(s)
      error: '',
      initialized: false,
      // page data
      stats: {},
      issues: {},
      groupBeingEdited: undefined,
      groupAddingCluster: undefined,
      clusterBeingEdited: undefined,
      showMoreIssuesFor: [],
      // old parliament order to undo reordering
      oldParliamentOrder: {},
      // search vars
      searchTerm: this.$route.query.searchTerm || '',
      // group form vars
      showNewGroupForm: false,
      newGroupTitle: '',
      newGroupDescription: '',
      focusGroupInput: false,
      // cluster form vars
      focusClusterInput: false,
      // create/edit/rearrange groups/clusters (or not)
      editMode: false,
      // hide all issues toggle
      hideAllIssues: false,
      // highlighted cluster for ES status navigation
      highlightedClusterId: null
    };
  },
  computed: {
    parliament () {
      return this.$store.state.parliament;
    },
    filteredParliament () {
      const parliamentClone = JSON.parse(JSON.stringify(this.parliament));

      if (!this.searchTerm || !parliamentClone?.groups) {
        return parliamentClone;
      }

      for (const group of parliamentClone.groups) {
        group.clusters = Object.assign([], group.clusters)
          .filter((item) => {
            return item.title.toLowerCase().indexOf(this.searchTerm.toLowerCase()) > -1;
          });
      }

      return parliamentClone;
    },
    numFilteredClusters () {
      let num = 0;

      if (!this.filteredParliament?.groups) { return num; }

      for (const group of this.filteredParliament.groups) {
        num += group.clusters.length;
      }

      return num;
    },
    isUser () {
      return this.$store.state.isUser;
    },
    isAdmin () {
      return this.$store.state.isAdmin;
    },
    refreshInterval () {
      return this.$store.state.refreshInterval;
    }
  },
  watch: {
    refreshInterval (newVal) {
      this.stopAutoRefresh();
      if (newVal) {
        this.loadStats();
        this.loadIssues();
        this.startAutoRefresh();
      }
    },
    '$store.state.scrollToClusterId' (clusterId) {
      if (clusterId) {
        this.scrollToCluster(clusterId);
        // Reset after scrolling
        this.$store.commit('setScrollToClusterId', null);
      }
    }
  },
  mounted () {
    this.startAutoRefresh();
    this.loadStats();
    this.loadIssues();

    setTimeout(() => {
      this.initializeGroupDragDrop();
      this.initializeClusterDragDrop();
    }, 400);
  },
  beforeUnmount () {
    this.stopAutoRefresh();
    if (highlightTimeout) {
      clearTimeout(highlightTimeout);
      highlightTimeout = null;
    }
  },
  methods: {
    commaString,
    humanReadableBits,
    humanReadableNumber,
    /* page functions -------------------------------------------------------- */
    getDeltaTDPSClass (clusterId) {
      if (clusterId === this.clusterBeingEdited) {
        return;
      }
      const deltaTDPS = this.stats[clusterId]?.deltaTDPS || 0;
      return [
        {
          'badge mb-1': clusterId !== this.clusterBeingEdited,
          'bg-danger': deltaTDPS > 0,
          'bg-success': deltaTDPS === 0
        }
      ];
    },
    getMonitoringClass (clusterId) {
      if (clusterId === this.clusterBeingEdited) {
        return;
      }
      const monitoring = this.stats[clusterId]?.monitoring || 0;
      return [
        {
          'badge mb-1': clusterId !== this.clusterBeingEdited,
          'bg-warning': monitoring === 0,
          'bg-info': monitoring > 0
        }
      ];
    },
    getDataNodesTooltip (clusterId) {
      const dNodes = this.stats[clusterId]?.dataNodes;
      const tNodes = this.stats[clusterId]?.totalNodes;
      let tooltip = this.$t('parliament.dbNodesTip', { data: this.commaString(dNodes), total:this.commaString(tNodes) });

      if (clusterId === this.clusterBeingEdited) {
        return tooltip;
      }

      const hideDNodes = this.filteredParliament.groups.find(g => g.clusters.find(c => c.id === clusterId))?.clusters.find(c => c.id === clusterId)?.hideDataNodes;
      const hideTNodes = this.filteredParliament.groups.find(g => g.clusters.find(c => c.id === clusterId))?.clusters.find(c => c.id === clusterId)?.hideTotalNodes;

      if (hideDNodes && hideTNodes) {
        tooltip = '';
        return tooltip;
      }

      if (hideDNodes && !hideTNodes) {
        tooltip = `Total Database Nodes: ${this.commaString(tNodes)}`;
        return tooltip;
      }

      if (!hideDNodes && hideTNodes) {
        tooltip = `Data Database Nodes: ${this.commaString(dNodes)}`;
        return tooltip;
      }

      return tooltip;
    },
    toggleEditMode () {
      this.editMode = !this.editMode;
      this.showNewGroupForm = false;
      this.groupBeingEdited = undefined;
      this.groupAddingCluster = undefined;
      this.clusterBeingEdited = undefined;
      this.focusClusterInput = false;
      this.focusGroupInput = false;
    },
    toggleHideAllIssues () {
      this.hideAllIssues = !this.hideAllIssues;
    },
    scrollToCluster (clusterId) {
      // Find the cluster element and scroll to it
      const element = document.getElementById(`cluster-${clusterId}`);
      if (element) {
        // Clear any existing highlight and fallback timeouts
        if (highlightTimeout) {
          clearTimeout(highlightTimeout);
        }
        if (fallbackTimeout) {
          clearTimeout(fallbackTimeout);
        }

        // Clear highlight during scroll
        this.highlightedClusterId = null;

        const applyHighlight = () => {
          // Highlight the cluster
          this.highlightedClusterId = clusterId;

          // Remove highlight after animation completes (1 second)
          highlightTimeout = setTimeout(() => {
            this.highlightedClusterId = null;
          }, 1000);
        };

        // Try to use scrollend event if supported
        const scrollContainer = element.closest('.parliament-content') || window;

        const onScrollEnd = () => {
          scrollContainer.removeEventListener('scrollend', onScrollEnd);
          if (fallbackTimeout) {
            clearTimeout(fallbackTimeout);
          }
          applyHighlight();
        };

        // Add scrollend listener
        scrollContainer.addEventListener('scrollend', onScrollEnd, { once: true });

        // Fallback timeout in case scrollend is not supported
        fallbackTimeout = setTimeout(() => {
          scrollContainer.removeEventListener('scrollend', onScrollEnd);
          applyHighlight();
        }, 600);

        // Start the scroll animation
        element.scrollIntoView({ behavior: 'smooth', block: 'center' });
      }
    },
    debounceSearch () {
      if (timeout) { clearTimeout(timeout); }
      timeout = setTimeout(() => {
        this.filterClusters();
      }, 400);
    },
    clear () {
      this.searchTerm = undefined;
      this.$router.replace({
        query: {
          ...this.$route.query,
          searchTerm: undefined
        }
      });
    },
    openNewGroupForm () {
      this.showNewGroupForm = true;
      this.focusGroupInput = true;
    },
    cancelCreateNewGroup () {
      this.error = '';
      this.newGroupTitle = '';
      this.newGroupDescription = '';
      this.showNewGroupForm = false;
      this.focusGroupInput = false;
    },
    createNewGroup () {
      this.error = '';

      if (!this.newGroupTitle) {
        this.error = 'A group must have a title';
        return;
      }

      const newGroup = {
        title: this.newGroupTitle,
        description: this.newGroupDescription
      };

      ParliamentService.createGroup(newGroup).then((data) => {
        this.error = '';
        this.newGroupTitle = '';
        this.newGroupDescription = '';
        this.showNewGroupForm = false;
        this.focusGroupInput = false;
        this.parliament.groups.push(data.group);
      }).catch((error) => {
        this.error = error || 'Unable to create group';
      });
    },
    displayEditGroupForm (group) {
      this.groupBeingEdited = group.id;
      group.newTitle = group.title;
      group.newDescription = group.description;
      this.focusGroupInput = true;
    },
    editGroup (group) {
      if (!group.newTitle) {
        this.error = 'A group must have a title';
        return;
      }

      const updatedGroup = {
        title: group.newTitle,
        description: group.newDescription
      };

      ParliamentService.editGroup(group.id, updatedGroup).then((data) => {
        // update group with new values and close form
        group.title = group.newTitle;
        group.description = group.newDescription;
        this.groupBeingEdited = undefined;
        this.focusGroupInput = false;
      }).catch((error) => {
        this.error = error || 'Unable to update this group';
      });
    },
    deleteGroup (group) {
      ParliamentService.deleteGroup(group.id).then((data) => {
        let index = 0; // remove the group from the parliament
        for (const g of this.parliament.groups) {
          if (g.id === group.id) {
            this.parliament.groups.splice(index, 1);
            break;
          }
          ++index;
        }
      }).catch((error) => {
        this.error = error || 'Unable to delete this group';
      });
    },
    cancelUpdateGroup (group) {
      this.groupBeingEdited = undefined;
      this.groupAddingCluster = undefined;
      this.focusGroupInput = false;
      this.focusClusterInput = false;
      group.newClusterTitle = '';
      group.newClusterDescription = '';
      group.newClusterUrl = '';
      group.newClusterLocalUrl = '';
      group.newClusterType = undefined;
    },
    displayNewClusterForm (group) {
      this.groupAddingCluster = group.id;
      this.focusClusterInput = true;
    },
    createNewCluster (group) {
      this.error = '';

      if (!group.newClusterTitle) {
        this.error = 'A cluster must have a title';
        return;
      }
      if (!group.newClusterUrl) {
        this.error = 'A cluster must have a url';
        return;
      }

      const newCluster = {
        title: group.newClusterTitle,
        description: group.newClusterDescription,
        url: group.newClusterUrl,
        localUrl: group.newClusterLocalUrl,
        type: group.newClusterType
      };

      ParliamentService.createCluster(group.id, newCluster).then((data) => {
        group.clusters.push(data.cluster);
        this.cancelUpdateGroup(group);
      }).catch((error) => {
        this.error = error || 'Unable to add a cluster to this group';
      });
    },
    displayEditClusterForm (cluster) {
      this.focusClusterInput = true;
      this.clusterBeingEdited = cluster.id;
      cluster.newTitle = cluster.title;
      cluster.newDescription = cluster.description;
      cluster.newUrl = cluster.url;
      cluster.newLocalUrl = cluster.localUrl;
      cluster.newType = cluster.type;
    },
    cancelEditCluster () {
      this.focusClusterInput = false;
      this.clusterBeingEdited = undefined;
    },
    editCluster (group, cluster) {
      if (!cluster.newTitle) {
        this.error = 'A cluster must have a title';
        return;
      }

      if (!cluster.newUrl) {
        this.error = 'A cluster must have a url';
        return;
      }

      const updatedCluster = {
        url: cluster.newUrl,
        title: cluster.newTitle,
        localUrl: cluster.newLocalUrl,
        hideDeltaBPS: cluster.hideDeltaBPS,
        description: cluster.newDescription,
        hideDataNodes: cluster.hideDataNodes,
        hideDeltaTDPS: cluster.hideDeltaTDPS,
        hideTotalNodes: cluster.hideTotalNodes,
        hideMonitoring: cluster.hideMonitoring,
        hideArkimeNodes: cluster.hideArkimeNodes
      };

      if (cluster.newType) {
        updatedCluster.type = cluster.newType;
      }

      ParliamentService.editCluster(group.id, cluster.id, updatedCluster).then((data) => {
        this.focusClusterInput = false;
        cluster.url = cluster.newUrl;
        cluster.type = cluster.newType;
        cluster.title = cluster.newTitle;
        cluster.localUrl = cluster.newLocalUrl;
        cluster.description = cluster.newDescription;
        this.cancelEditCluster();
      }).catch((error) => {
        this.error = error || 'Unable to update this cluster';
      });
    },
    deleteCluster (group, cluster) {
      ParliamentService.deleteCluster(group.id, cluster.id).then((data) => {
        const originalGroup = this.parliament.groups.find(g => g.id === group.id);
        if (!originalGroup) { return; }

        const index = originalGroup.clusters.findIndex(c => c.id === cluster.id);
        if (index === -1) { return; }

        originalGroup.clusters.splice(index, 1);
      }).catch((error) => {
        this.error = error || 'Unable to remove cluster from this group';
      });
    },
    showMoreIssues (cluster) {
      this.showMoreIssuesFor.push(cluster.id);
    },
    showLessIssues (cluster) {
      const i = this.showMoreIssuesFor.indexOf(cluster.id);
      if (i > -1) { this.showMoreIssuesFor.splice(i, 1); }
    },
    acknowledgeAllIssues (cluster) {
      ParliamentService.acknowledgeIssues(this.issues[cluster.id]).then((data) => {
        this.issues[cluster.id] = [];
      }).catch((error) => {
        this.error = error || 'Unable to acknowledge all of the issues in this cluster';
      });
    },
    getIssueTrackingId (issue) {
      if (issue.node) {
        return `${issue.node.replace(/\s/g, '')}-${issue.type}`;
      } else {
        return issue.type;
      }
    },
    issueChange (changeEvent) {
      if (changeEvent.success) {
        // find the cluster to remove the issue
        const cluster = this.getCluster(changeEvent.groupId, changeEvent.clusterId);
        this.issues[cluster.id].splice(changeEvent.index, 1);
      } else { // show a global error
        this.error = changeEvent.message;
      }
    },
    /* helper functions ---------------------------------------------------- */
    loadStats () {
      this.error = '';
      ParliamentService.getStats().then((data) => {
        this.stats = data.results;
        this.$store.commit('setStats', data.results);
      }).catch((error) => {
        this.error = error || 'Error fetching statistics for clusters in your Parliament';
      });
    },
    loadIssues () {
      this.error = '';
      ParliamentService.getIssues({ map: true }).then((data) => {
        this.issues = data.results;
      }).catch((error) => {
        this.error = error || 'Error fetching issues in your Parliament';
      });
    },
    startAutoRefresh () {
      if (!this.refreshInterval) { return; }
      interval = setInterval(() => {
        this.loadStats();
        this.loadIssues();
      }, this.refreshInterval);
    },
    stopAutoRefresh () {
      if (interval) { clearInterval(interval); }
    },
    getCluster (groupId, clusterId) {
      for (const group of this.parliament.groups) {
        if (group.id === groupId) {
          for (const cluster of group.clusters) {
            if (cluster.id === clusterId) {
              return cluster;
            }
          }
        }
      }
      return undefined;
    },
    getGroup (groupId) {
      for (const group of this.parliament.groups) {
        if (group.id === groupId) {
          return group;
        }
      }
      return undefined;
    },
    updateOrder ({ oldIdx, newIdx, oldGroupId, newGroupId, errorText }) {
      ParliamentService.updateOrder({ oldIdx, newIdx, oldGroupId, newGroupId }).catch((error) => {
        this.error = error || errorText;
        // reset the parliament order by fetching parliament
        ParliamentService.getParliament().then((data) => {
          this.$store.commit('setParliament', data);
        });
      });
    },
    initializeGroupDragDrop () {
      if (!this.$refs.draggableGroups) { return; }

      draggableGroups = Sortable.create(this.$refs.draggableGroups, {
        animation: 100,
        handle: '.group-handle',
        onMove: (e) => { // don't allow drag/drop if clusters are filtered
          // or when the user is not a parliament admin
          if (this.searchTerm || !this.isAdmin) { return false; }
          this.stopAutoRefresh();
        },
        onEnd: (e) => { // dragged group was dropped
          const newIdx = e.newIndex;
          const oldIdx = e.oldIndex;

          if (newIdx === oldIdx) {
            return;
          }

          const group = this.parliament.groups[oldIdx];
          const errorText = 'Error moving this group.';

          if (!group) {
            this.error = errorText;
            return;
          }

          this.parliament.groups.splice(oldIdx, 1);
          this.parliament.groups.splice(newIdx, 0, group);

          this.updateOrder({ oldIdx, newIdx, errorText });
          this.startAutoRefresh();
        }
      });
    },
    initializeClusterDragDrop () {
      if (!this.$refs.draggableClusters) { return; }

      for (const clusterGroup of this.$refs.draggableClusters) {
        draggableClusters = Sortable.create(clusterGroup, {
          group: 'clusters',
          handle: '.cluster-handle',
          animation: 100,
          onMove: (e) => { // don't allow drag/drop if clusters are filtered
            // or when the user is not a parliament admin
            if (this.searchTerm || !this.isAdmin) { return false; }
            this.stopAutoRefresh();
          },
          onEnd: (e) => { // dragged cluster was dropped
            const newIdx = e.newIndex;
            const oldIdx = e.oldIndex;
            const newGroupId = e.to.id;
            const oldGroupId = e.from.id;

            if (newIdx === oldIdx && newGroupId === oldGroupId) {
              return;
            }

            const clusterId = e.item.id.replace('cluster-', '');
            const oldGroup = this.getGroup(oldGroupId);
            const newGroup = this.getGroup(newGroupId);
            const cluster = this.getCluster(oldGroupId, clusterId);
            const errorText = 'Error moving this cluster.';

            if (!oldGroup || !newGroup || !cluster) {
              this.error = errorText;
              return;
            }

            oldGroup.clusters.splice(oldIdx, 1);
            newGroup.clusters.splice(newIdx, 0, cluster);

            this.updateOrder({ oldIdx, newIdx, oldGroupId, newGroupId, errorText });
            this.startAutoRefresh();
          }
        });
      }
    }
  },
  beforeUnmount () {
    this.stopAutoRefresh();
    if (draggableGroups && draggableGroups.el) {
      draggableGroups.destroy();
    }
    if (draggableClusters && draggableClusters.el) {
      draggableClusters.destroy();
    }
  }
};
</script>

<style scoped>
.btn-clear-input {
  color: #555;
  background-color: #EEE;
  border-color: #CCC;
}

/* drag/drop handle styles */
.group-handle {
  color: #C3C3C3;
  cursor: ns-resize;
}

.cluster-handle {
  visibility: hidden;
  color: #AAAAAA;
  cursor: move;
  position: absolute;
  top: -22px;
  left: -1px;
  width: 28px;
  height: 24px;
  background: #F8F9FA;
  border-radius: 4px 4px 0 0;
  border: 1px solid rgba(0, 0, 0, 0.125);
  border-bottom: none;
}
.cluster-handle .fa {
  margin-left: 5px;
}
.cluster:hover .cluster-handle {
  visibility: visible;
}

/* cluster styles */
.cluster-group {
  list-style: none;
  padding: 0;
  margin-left: 0;
  margin-right: 0;
}
.cluster-group .cluster {
  padding-left: 2px;
  padding-right: 2px;
}
.cluster-group .card {
  height:100%;
  box-shadow: 0 20px 40px -14px rgba(0,0,0,0.25);
}
.cluster-group .card .card-body {
  padding: 0.5rem;
}
.cluster-group .card .card-footer {
  padding: 0.2rem 0.5rem;
}
.cluster-group .cluster .cluster-stats-row {
  line-height: 1;
  margin-right: -2px;
  margin-left: -2px;
}
.cluster-group .cluster .cluster-stats-row > div {
  padding-right: 2px;
  padding-left: 2px;
}
.sortable-chosen, .sortable-chosen.sortable-ghost {
  /* https://github.com/RubaXa/Sortable/issues/1276 */
  z-index: 10000;
}

/* compact form for editing clusters */
form.edit-cluster label {
  margin:0;
}
form.edit-cluster .form-group {
  margin-bottom: .25rem;
}

/* cluster highlight animation for ES status navigation */
.cluster-highlight {
  animation: highlight-pulse 1s ease-in-out;
  transform-origin: center center;
  position: relative;
  z-index: 100;
}

@keyframes highlight-pulse {
  0%, 100% {
    transform: scale(1);
  }
  15% {
    transform: scale(1.08);
  }
  30% {
    transform: scale(1);
  }
  45% {
    transform: scale(1.08);
  }
  60% {
    transform: scale(1);
  }
  75% {
    transform: scale(1.08);
  }
  90% {
    transform: scale(1);
  }
}
</style>
