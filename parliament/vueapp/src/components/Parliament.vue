<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-container fluid>
    <!-- page error -->
    <v-snackbar
      :model-value="!!error"
      color="error"
      location="bottom"
      :timeout="-1"
      @update:model-value="(v) => { if (!v) error = '' }">
      {{ error }}
      <template #actions>
        <v-btn
          variant="text"
          icon="mdi-close"
          @click="error = ''" />
      </template>
    </v-snackbar> <!-- /page error -->

    <!-- search & create group -->
    <div class="d-flex flex-row align-center flex-nowrap mb-2 arkime-toolbar ga-2">
      <!-- search -->
      <v-text-field
        v-model="searchTerm"
        prepend-inner-icon="mdi-magnify"
        placeholder="Search clusters"
        clearable
        density="compact"
        hide-details
        class="me-2 flex-grow-1"
        tabindex="8"
        @click:clear="clear" /><!-- /search -->
      <div
        v-if="isAdmin"
        class="no-wrap d-flex align-center ga-1">
        <!-- create group -->
        <v-btn
          v-if="!showNewGroupForm && editMode"
          variant="outlined"
          color="primary"
          @click="openNewGroupForm">
          <v-icon
            icon="mdi-plus-circle"
            class="me-1" />
          {{ $t('parliament.newGroup') }}
        </v-btn>
        <template v-else-if="editMode">
          <v-btn
            variant="outlined"
            color="warning"
            @click="cancelCreateNewGroup">
            <v-icon
              icon="mdi-cancel"
              class="me-1" />
            {{ $t('common.cancel') }}
          </v-btn>
          <v-btn
            variant="outlined"
            color="success"
            @click="createNewGroup">
            <v-icon
              icon="mdi-plus-circle"
              class="me-1" />
            {{ $t('common.create') }}
          </v-btn>
        </template> <!-- /create group -->
        <!-- hide/show issues button -->
        <v-btn
          variant="outlined"
          color="secondary"
          class="me-2"
          :aria-label="$t(hideAllIssues ? 'parliament.showAllIssues' : 'parliament.hideAllIssues')"
          @click="toggleHideAllIssues">
          <v-icon :icon="hideAllIssues ? 'mdi-eye' : 'mdi-eye-off'" />
          <v-tooltip
            activator="parent"
            location="bottom">
            {{ $t(hideAllIssues ? 'parliament.showAllIssues' : 'parliament.hideAllIssues') }}
          </v-tooltip>
        </v-btn>
        <!-- /hide/show issues button -->
        <!-- edit mode toggle -->
        <v-switch
          :model-value="editMode"
          color="success"
          density="compact"
          hide-details
          inset
          class="ms-2 edit-mode-switch"
          @update:model-value="toggleEditMode">
          <v-tooltip
            activator="parent"
            location="bottom">
            {{ $t('parliament.editModeToggleTip') }}
          </v-tooltip>
        </v-switch>
        <!-- /edit mode toggle -->
      </div>
    </div> <!-- /search & create group -->

    <v-divider class="mb-4" />

    <!-- new group form -->
    <div
      v-if="showNewGroupForm && isAdmin && editMode"
      class="compact-form"
      @keyup.enter="createNewGroup">
      <v-row class="mb-1">
        <v-col
          cols="12"
          sm="2"
          tag="label"
          for="newGroupTitle"
          class="form-side-label">
          {{ $t('parliament.groupTitle') }}<sup class="text-error">*</sup>
        </v-col>
        <v-col
          cols="12"
          sm="10">
          <div class="arkime-input-group arkime-input-group--fluid">
            <input
              type="text"
              v-model="newGroupTitle"
              class="arkime-input-control"
              id="newGroupTitle"
              :placeholder="$t('parliament.groupTitlePlaceholder')"
              v-focus="focusGroupInput">
          </div>
        </v-col>
      </v-row>
      <v-row class="mb-1">
        <v-col
          cols="12"
          sm="2"
          tag="label"
          for="newGroupDescription"
          class="form-side-label">
          {{ $t('parliament.groupDescription') }}
        </v-col>
        <v-col
          cols="12"
          sm="10">
          <div class="arkime-input-group arkime-input-group--fluid">
            <textarea
              rows="3"
              v-model="newGroupDescription"
              class="arkime-input-control"
              id="newGroupDescription"
              :placeholder="$t('parliament.groupDescriptionPlaceholder')" />
          </div>
        </v-col>
      </v-row>
      <v-divider class="my-3" />
    </div> <!-- /new group form -->

    <!-- no results for searchTerm filter -->
    <div
      v-if="searchTerm && !numFilteredClusters && parliament.groups && parliament.groups.length"
      class="info-area vertical-center">
      <div class="text-medium-emphasis">
        <v-icon
          icon="mdi-folder-open"
          size="x-large"
          class="text-muted-more" />
        {{ $t('parliament.noClustersMatch') }}
      </div>
    </div> <!-- /no results for searchTerm filter -->

    <!-- no groups -->
    <div
      v-if="parliament.groups && !parliament.groups.length && !showNewGroupForm"
      class="info-area text-center vertical-center">
      <div class="text-medium-emphasis mt-5">
        <v-icon
          icon="mdi-folder-open"
          size="x-large"
          class="text-muted-more" />
        {{ $t('parliament.noClusters') }}
        <a
          v-if="isAdmin && editMode"
          @click="showNewGroupForm = true"
          class="cursor-pointer no-href no-decoration">
          {{ $t('parliament.createGroup') }}
        </a>
        <template v-else-if="isAdmin">
          <p class="d-flex justify-space-between align-center mb-0 mt-3">
            <v-icon
              icon="mdi-lock"
              class="text-muted-more me-3" />
            {{ $t('parliament.needEditMode') }}
            <v-icon
              icon="mdi-lock"
              class="text-muted-more ms-3" />
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
          class="group"
          v-if="!group.clusters || (group.clusters.length > 0 || (!group.clusters.length && !searchTerm))">
          <div class="d-flex align-center mb-1 pb-1 ga-2 flex-wrap">
            <h4 class="d-flex align-center mb-0 flex-grow-1">
              <v-icon
                icon="mdi-view-grid"
                class="group-handle me-2"
                v-if="isAdmin && !searchTerm && editMode" />
              {{ group.title }}
              <template v-if="isAdmin && groupAddingCluster !== group.id && groupBeingEdited === group.id && editMode">
                <v-btn
                  variant="outlined"
                  color="error"
                  class="ms-2"
                  @click="deleteGroup(group)">
                  <v-icon icon="mdi-trash-can-outline" />
                  <v-tooltip
                    activator="parent"
                    location="top">
                    {{ $t('parliament.deleteGroupTip') }}
                  </v-tooltip>
                </v-btn>
              </template>
            </h4>
            <!-- group action buttons -->
            <div
              v-if="isAdmin && editMode"
              class="d-flex align-center ga-1">
              <v-btn
                v-if="groupBeingEdited !== group.id && groupAddingCluster !== group.id"
                variant="outlined"
                color="warning"
                @click="displayEditGroupForm(group)">
                <v-icon
                  icon="mdi-pencil"
                  class="me-1" />
                {{ $t('parliament.editGroup') }}
              </v-btn>
              <v-btn
                v-if="groupBeingEdited === group.id"
                variant="outlined"
                color="success"
                @click="editGroup(group)">
                <v-icon
                  icon="mdi-content-save"
                  class="me-1" />
                {{ $t('common.save') }}
              </v-btn>
              <v-btn
                v-if="groupAddingCluster === group.id"
                variant="outlined"
                color="success"
                @click="createNewCluster(group)">
                <v-icon
                  icon="mdi-plus-circle"
                  class="me-1" />
                {{ $t('common.create') }}
              </v-btn>
              <v-btn
                v-if="groupAddingCluster === group.id || groupBeingEdited === group.id"
                variant="outlined"
                color="warning"
                @click="cancelUpdateGroup(group)">
                <v-icon
                  icon="mdi-cancel"
                  class="me-1" />
                {{ $t('common.cancel') }}
              </v-btn>
              <v-btn
                v-if="groupAddingCluster !== group.id && groupBeingEdited !== group.id"
                variant="outlined"
                color="info"
                @click="displayNewClusterForm(group)">
                <v-icon
                  icon="mdi-plus-circle"
                  class="me-1" />
                {{ $t('parliament.newCluster') }}
              </v-btn>
            </div> <!-- /group action buttons -->
          </div>
          <p class="mb-2">
            {{ group.description }}
          </p>
        </div> <!-- /group title/description -->

        <!-- edit group form -->
        <div
          v-if="isAdmin && groupBeingEdited === group.id && editMode"
          class="compact-form">
          <form>
            <v-row class="mb-1">
              <v-col
                cols="12"
                sm="2"
                tag="label"
                for="editGroupTitle"
                class="form-side-label">
                {{ $t('parliament.groupTitle') }}<sup class="text-error">*</sup>
              </v-col>
              <v-col
                cols="12"
                sm="10">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <input
                    type="text"
                    v-model="group.newTitle"
                    class="arkime-input-control"
                    id="editGroupTitle"
                    :placeholder="$t('parliament.groupTitlePlaceholder')"
                    v-focus="focusGroupInput">
                </div>
              </v-col>
            </v-row>
            <v-row class="mb-1">
              <v-col
                cols="12"
                sm="2"
                tag="label"
                for="editGroupDescription"
                class="form-side-label">
                {{ $t('parliament.groupDescription') }}
              </v-col>
              <v-col
                cols="12"
                sm="10">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <textarea
                    rows="3"
                    v-model="group.newDescription"
                    class="arkime-input-control"
                    id="editGroupDescription"
                    :placeholder="$t('parliament.groupDescriptionPlaceholder')" />
                </div>
              </v-col>
            </v-row>
          </form>
          <v-divider class="my-3" />
        </div> <!-- /edit group form -->

        <!-- create cluster form -->
        <div
          v-if="isAdmin && groupAddingCluster === group.id && editMode"
          class="compact-form">
          <v-divider class="my-3" />
          <form @keyup.enter="createNewCluster(group)">
            <v-row class="mb-1">
              <v-col
                cols="12"
                sm="2"
                tag="label"
                for="newClusterTitle"
                class="form-side-label">
                {{ $t('parliament.clusterTitle') }}<sup class="text-error">*</sup>
              </v-col>
              <v-col
                cols="12"
                sm="10">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <input
                    type="text"
                    v-model="group.newClusterTitle"
                    class="arkime-input-control"
                    id="newClusterTitle"
                    :placeholder="$t('parliament.clusterTitlePlaceholder')"
                    v-focus="focusClusterInput">
                </div>
              </v-col>
            </v-row>
            <v-row class="mb-1">
              <v-col
                cols="12"
                sm="2"
                tag="label"
                for="newClusterDescription"
                class="form-side-label">
                {{ $t('parliament.clusterDescription') }}
              </v-col>
              <v-col
                cols="12"
                sm="10">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <textarea
                    rows="3"
                    v-model="group.newClusterDescription"
                    class="arkime-input-control"
                    id="newClusterDescription"
                    :placeholder="$t('parliament.clusterDescriptionPlaceholder')" />
                </div>
              </v-col>
            </v-row>
            <v-row class="mb-1">
              <v-col
                cols="12"
                sm="2"
                tag="label"
                for="newClusterUrl"
                class="form-side-label">
                {{ $t('parliament.clusterUrl') }}<sup class="text-error">*</sup>
              </v-col>
              <v-col
                cols="12"
                sm="10">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <input
                    type="text"
                    v-model="group.newClusterUrl"
                    class="arkime-input-control"
                    id="newClusterUrl"
                    :placeholder="$t('parliament.clusterUrlPlaceholder')">
                </div>
              </v-col>
            </v-row>
            <v-row class="mb-1">
              <v-col
                cols="12"
                sm="2"
                tag="label"
                for="newClusterLocalUrl"
                class="form-side-label">
                {{ $t('parliament.clusterLocalUrl') }}
              </v-col>
              <v-col
                cols="12"
                sm="10">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <input
                    type="text"
                    v-model="group.newClusterLocalUrl"
                    class="arkime-input-control"
                    id="newClusterLocalUrl"
                    :placeholder="$t('parliament.clusterLocalUrlPlaceholder')">
                </div>
              </v-col>
            </v-row>
            <v-row>
              <v-col
                cols="12"
                sm="2"
                tag="label"
                for="newClusterType"
                class="form-side-label">
                {{ $t('parliament.clusterType') }}<sup class="text-error">*</sup>
              </v-col>
              <v-col
                cols="12"
                sm="10">
                <div class="arkime-input-group arkime-input-group--fluid">
                  <select
                    v-model="group.newClusterType"
                    class="arkime-input-control"
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
              </v-col>
            </v-row>
          </form>
          <v-divider class="my-3" />
        </div> <!-- /create cluster form -->

        <!-- clusters -->
        <ul
          v-if="group.clusters && group.clusters.length"
          :id="group.id"
          ref="draggableClusters"
          class="cluster-group d-flex flex-wrap mb-4">
          <li
            v-for="cluster in group.clusters"
            :key="cluster.id"
            :id="`cluster-${cluster.id}`"
            class="cluster mb-1">
            <v-card
              :class="{'cluster-highlight': highlightedClusterId === cluster.id}"
              variant="elevated">
              <v-card-text class="pa-2">
                <!-- cluster ES status chip -->
                <v-chip
                  v-if="cluster.type !== 'disabled' && stats[cluster.id]"
                  :href="`${cluster.url}/stats?statsTab=2`"
                  :color="clusterStatusColor(cluster.id)"
                  size="small"
                  variant="flat"
                  class="float-right cluster-status-chip">
                  <span v-if="stats[cluster.id].status">
                    {{ stats[cluster.id].status }}
                  </span>
                  <v-icon
                    v-if="stats[cluster.id].healthError"
                    icon="mdi-alert"
                    size="small" />
                  <span v-if="!stats[cluster.id].status && !stats[cluster.id].healthError">
                    ????
                  </span>
                  <v-tooltip
                    activator="parent"
                    location="top">
                    <span>{{ $t('parliament.esStatus') }}: <strong>{{ stats[cluster.id].healthError || stats[cluster.id].status || 'unreachable' }}</strong></span>
                    <span v-if="stats[cluster.id].esVersion">
                      <br>{{ $t('parliament.esVersion') }}: <strong>{{ stats[cluster.id].esVersion }}</strong>
                    </span>
                  </v-tooltip>
                </v-chip>
                <h5>
                  <span
                    v-if="isAdmin && !searchTerm && editMode"
                    class="cluster-handle">
                    <v-icon
                      icon="mdi-view-grid"
                      size="x-small" />
                  </span>
                  <template v-if="cluster.type === 'multiviewer'">
                    <v-icon
                      icon="mdi-sitemap"
                      size="x-small"
                      class="text-medium-emphasis cursor-help me-2">
                      <v-tooltip
                        activator="parent"
                        location="top">
                        {{ $t('parliament.clusterType-multiviewerTip') }}
                      </v-tooltip>
                    </v-icon>
                  </template>
                  <template v-if="cluster.type === 'disabled'">
                    <v-icon
                      icon="mdi-eye-off"
                      size="x-small"
                      class="text-medium-emphasis cursor-help me-2">
                      <v-tooltip
                        activator="parent"
                        location="top">
                        {{ $t('parliament.clusterType-disabledTip') }}
                      </v-tooltip>
                    </v-icon>
                  </template>
                  <template v-if="cluster.type === 'noAlerts'">
                    <v-icon
                      icon="mdi-bell-off"
                      size="x-small"
                      class="text-medium-emphasis cursor-help me-2">
                      <v-tooltip
                        activator="parent"
                        location="top">
                        {{ $t('parliament.clusterType-noAlertsTip') }}
                      </v-tooltip>
                    </v-icon>
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
                    class="no-decoration ms-2">
                    <v-icon
                      icon="mdi-chart-bar"
                      size="x-small" />
                    <v-tooltip
                      activator="parent"
                      location="top">
                      {{ $t('parliament.statsLinkTip') }}
                    </v-tooltip>
                  </a>
                </h5> <!-- /cluster title -->
                <!-- cluster description -->
                <p
                  class="text-medium-emphasis text-body-2 mb-2"
                  v-if="cluster.description">
                  {{ cluster.description }}
                </p> <!-- /cluster description -->

                <!-- cluster stats -->
                <template v-if="stats[cluster.id] && (!stats[cluster.id].statsError && cluster.id !== clusterBeingEdited && cluster.type !== 'disabled' && cluster.type !== 'multiviewer') || (cluster.id === clusterBeingEdited && cluster.newType !== 'disabled' && cluster.newType !== 'multiviewer')">
                  <!-- view mode: chips per stat -->
                  <div
                    v-if="cluster.id !== clusterBeingEdited"
                    class="d-flex flex-wrap mt-3 ga-1">
                    <v-chip
                      v-if="!cluster.hideDeltaBPS"
                      :id="'deltaBPS-' + cluster.id"
                      color="primary"
                      variant="flat"
                      size="small"
                      label
                      class="flex-fill cluster-stat-chip">
                      <strong class="me-1">
                        {{ humanReadableBits(stats[cluster.id].deltaBPS) }}
                      </strong>
                      {{ $t('parliament.bitsPerSec') }}
                      <v-tooltip activator="parent">
                        {{ $t('parliament.bitsPerSecTip', {count: humanReadableBits(stats[cluster.id].deltaBPS)}) }}
                      </v-tooltip>
                    </v-chip>

                    <v-chip
                      v-if="!cluster.hideDeltaTDPS"
                      :id="'deltaTDPS-' + cluster.id"
                      :color="(stats[cluster.id]?.deltaTDPS || 0) > 0 ? 'error' : 'success'"
                      variant="flat"
                      size="small"
                      label
                      class="flex-fill cluster-stat-chip">
                      <strong class="me-1">
                        {{ humanReadableNumber(stats[cluster.id].deltaTDPS) }}
                      </strong>
                      {{ $t('parliament.dropsPerSec') }}
                      <v-tooltip activator="parent">
                        {{ $t('parliament.bitsPerSecTip', {count: commaString(stats[cluster.id].deltaTDPS)}) }}
                      </v-tooltip>
                    </v-chip>

                    <v-chip
                      v-if="!cluster.hideMonitoring"
                      :id="'monitoring-' + cluster.id"
                      :color="(stats[cluster.id]?.monitoring || 0) > 0 ? 'info' : 'warning'"
                      variant="flat"
                      size="small"
                      label
                      class="flex-fill cluster-stat-chip">
                      <strong class="me-1">
                        {{ humanReadableNumber(stats[cluster.id].monitoring) }}
                      </strong>
                      {{ $t('common.sessions') }}
                      <v-tooltip activator="parent">
                        {{ $t('parliament.sessionsTip', {count: commaString(stats[cluster.id].monitoring)}) }}
                      </v-tooltip>
                    </v-chip>

                    <v-chip
                      v-if="!cluster.hideArkimeNodes"
                      :id="'arkimeNodes-' + cluster.id"
                      color="neutral-light"
                      variant="flat"
                      size="small"
                      label
                      class="flex-fill cluster-stat-chip">
                      <strong class="me-1">
                        {{ commaString(stats[cluster.id].arkimeNodes) }}
                      </strong>
                      {{ $t('parliament.captureNodes') }}
                      <v-tooltip activator="parent">
                        {{ $t('parliament.captureNodesTip', {count: commaString(stats[cluster.id].arkimeNodes)}) }}
                      </v-tooltip>
                    </v-chip>

                    <v-chip
                      v-if="!cluster.hideDataNodes || !cluster.hideTotalNodes"
                      :id="'dataNodes-' + cluster.id"
                      color="neutral"
                      variant="flat"
                      size="small"
                      label
                      class="flex-fill cluster-stat-chip">
                      <strong
                        v-if="!cluster.hideDataNodes"
                        class="me-1">
                        {{ commaString(stats[cluster.id].dataNodes) }}
                      </strong>
                      <span
                        v-if="!cluster.hideDataNodes && !cluster.hideTotalNodes"
                        class="me-1">/</span>
                      <strong
                        v-if="!cluster.hideTotalNodes"
                        class="me-1">
                        {{ commaString(stats[cluster.id].totalNodes) }}
                      </strong>
                      {{ $t('parliament.dbNodes') }}
                      <v-tooltip activator="parent">
                        {{ getDataNodesTooltip(cluster.id) }}
                      </v-tooltip>
                    </v-chip>
                  </div>
                  <!-- edit mode: checkbox + label rows (no pill background) -->
                  <div
                    v-else
                    class="d-flex flex-column mt-3 ga-1 cluster-stats-edit">
                    <label>
                      <input
                        class="arkime-check-input me-2"
                        type="checkbox"
                        :checked="!cluster.hideDeltaBPS"
                        @change="cluster.hideDeltaBPS = !cluster.hideDeltaBPS">
                      <strong class="me-1">
                        {{ humanReadableBits(stats[cluster.id].deltaBPS) }}
                      </strong>
                      {{ $t('parliament.bitsPerSec') }}
                    </label>
                    <label>
                      <input
                        class="arkime-check-input me-2"
                        type="checkbox"
                        :checked="!cluster.hideDeltaTDPS"
                        @change="cluster.hideDeltaTDPS = !cluster.hideDeltaTDPS">
                      <strong class="me-1">
                        {{ humanReadableNumber(stats[cluster.id].deltaTDPS) }}
                      </strong>
                      {{ $t('parliament.dropsPerSec') }}
                    </label>
                    <label>
                      <input
                        class="arkime-check-input me-2"
                        type="checkbox"
                        :checked="!cluster.hideMonitoring"
                        @change="cluster.hideMonitoring = !cluster.hideMonitoring">
                      <strong class="me-1">
                        {{ humanReadableNumber(stats[cluster.id].monitoring) }}
                      </strong>
                      {{ $t('common.sessions') }}
                    </label>
                    <label>
                      <input
                        class="arkime-check-input me-2"
                        type="checkbox"
                        :checked="!cluster.hideArkimeNodes"
                        @change="cluster.hideArkimeNodes = !cluster.hideArkimeNodes">
                      <strong class="me-1">
                        {{ commaString(stats[cluster.id].arkimeNodes) }}
                      </strong>
                      {{ $t('parliament.captureNodes') }}
                    </label>
                    <label class="d-flex align-center flex-wrap">
                      <input
                        class="arkime-check-input me-2"
                        type="checkbox"
                        :checked="!cluster.hideDataNodes"
                        @change="cluster.hideDataNodes = !cluster.hideDataNodes">
                      <strong class="me-1">
                        {{ commaString(stats[cluster.id].dataNodes) }}
                      </strong>
                      <span class="me-1">/</span>
                      <input
                        class="arkime-check-input me-2"
                        type="checkbox"
                        :checked="!cluster.hideTotalNodes"
                        @change="cluster.hideTotalNodes = !cluster.hideTotalNodes">
                      <strong class="me-1">
                        {{ commaString(stats[cluster.id].totalNodes) }}
                      </strong>
                      {{ $t('parliament.dbNodes') }}
                    </label>
                  </div>
                </template>
                <!-- /cluster stats -->

                <!-- cluster issues -->
                <div v-if="!hideAllIssues && issues[cluster.id]">
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
                </div> <!-- /cluster issues -->
                <!-- edit cluster form -->
                <div
                  v-if="isAdmin && cluster.id === clusterBeingEdited && editMode"
                  class="edit-cluster text-body-2">
                  <v-divider class="mt-2 mb-2" />
                  <form>
                    <label
                      for="newClusterTitle"
                      class="d-block">
                      {{ $t('parliament.clusterTitle') }}<sup class="text-error">*</sup>
                    </label>
                    <div class="arkime-input-group arkime-input-group--fluid mb-1">
                      <input
                        type="text"
                        v-model="cluster.newTitle"
                        class="arkime-input-control"
                        id="newClusterTitle"
                        :placeholder="$t('parliament.clusterTitlePlaceholder')"
                        v-focus="focusClusterInput">
                    </div>
                    <label
                      for="newClusterDescription"
                      class="d-block">
                      {{ $t('parliament.clusterDescription') }}
                    </label>
                    <div class="arkime-input-group arkime-input-group--fluid mb-1">
                      <textarea
                        rows="3"
                        v-model="cluster.newDescription"
                        class="arkime-input-control"
                        id="newClusterDescription"
                        :placeholder="$t('parliament.clusterDescriptionPlaceholder')" />
                    </div>
                    <label
                      for="newClusterUrl"
                      class="d-block">
                      {{ $t('parliament.clusterUrl') }}<sup class="text-error">*</sup>
                    </label>
                    <div class="arkime-input-group arkime-input-group--fluid mb-1">
                      <input
                        type="text"
                        v-model="cluster.newUrl"
                        class="arkime-input-control"
                        id="newClusterUrl"
                        :placeholder="$t('parliament.clusterUrlPlaceholder')">
                    </div>
                    <label
                      for="newClusterLocalUrl"
                      class="d-block">
                      {{ $t('parliament.clusterLocalUrl') }}
                    </label>
                    <div class="arkime-input-group arkime-input-group--fluid mb-1">
                      <input
                        type="text"
                        v-model="cluster.newLocalUrl"
                        class="arkime-input-control"
                        id="newClusterLocalUrl"
                        :placeholder="$t('parliament.clusterLocalUrlPlaceholder')">
                    </div>
                    <label
                      for="newClusterType"
                      class="d-block">
                      {{ $t('parliament.clusterType') }}<sup class="text-error">*</sup>
                    </label>
                    <div class="arkime-input-group arkime-input-group--fluid mb-1">
                      <select
                        v-model="cluster.newType"
                        class="arkime-input-control"
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
              </v-card-text>
              <!-- edit cluster buttons -->
              <v-card-actions
                v-if="isUser && ((isUser && !hideAllIssues && issues[cluster.id] && issues[cluster.id].length && cluster.id !== clusterBeingEdited) || (isAdmin && editMode))"
                class="pa-2 ga-1">
                <v-btn
                  v-show="cluster.id !== clusterBeingEdited && editMode && isAdmin"
                  size="small"
                  variant="outlined"
                  color="warning"
                  @click="displayEditClusterForm(cluster)">
                  <v-icon icon="mdi-pencil" />
                </v-btn>
                <template v-if="cluster.id === clusterBeingEdited && editMode && isAdmin">
                  <v-btn
                    size="small"
                    variant="outlined"
                    color="error"
                    @click="deleteCluster(group, cluster)">
                    <v-icon icon="mdi-trash-can-outline" />
                  </v-btn>
                </template>
                <v-spacer />
                <template v-if="cluster.id === clusterBeingEdited && editMode && isAdmin">
                  <v-btn
                    size="small"
                    variant="outlined"
                    color="warning"
                    @click="cancelEditCluster(cluster)">
                    <v-icon icon="mdi-cancel" />
                  </v-btn>
                  <v-btn
                    size="small"
                    variant="outlined"
                    color="success"
                    @click="editCluster(group, cluster)">
                    <v-icon
                      icon="mdi-content-save"
                      class="me-1" />
                    {{ $t('common.save') }}
                  </v-btn>
                </template>
                <v-btn
                  v-if="!hideAllIssues && issues[cluster.id] && issues[cluster.id].length && cluster.id !== clusterBeingEdited"
                  size="small"
                  variant="outlined"
                  color="success"
                  @click="acknowledgeAllIssues(cluster)">
                  <v-icon icon="mdi-check" />
                  <v-tooltip
                    activator="parent"
                    location="top">
                    {{ $t('parliament.ackAllIssuesTip') }}
                  </v-tooltip>
                </v-btn>
              </v-card-actions> <!-- /edit cluster buttons -->
            </v-card>
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
  </v-container>
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
      hideAllIssues: this.$route.query.hideIssues === 'true',
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
    clusterStatusColor (clusterId) {
      const esStatus = this.stats[clusterId]?.status;
      if (esStatus === 'green') return 'success';
      if (esStatus === 'yellow') return 'warning';
      if (esStatus === 'red') return 'error';
      return 'neutral-light';
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
      this.$router.replace({
        query: {
          ...this.$route.query,
          hideIssues: this.hideAllIssues ? 'true' : undefined
        }
      });
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
      if (!group.newClusterUrl.startsWith('http') && !group.newClusterUrl.startsWith('/')) {
        this.error = 'A cluster URL must start with http:// or /';
        return;
      }
      if (group.newClusterLocalUrl &&
          !group.newClusterLocalUrl.startsWith('http') &&
          !group.newClusterLocalUrl.startsWith('/')) {
        this.error = 'A cluster local URL must start with http:// or /';
        return;
      }

      // The Normal type uses option value="undefined" so the v-model
      // pulls the literal string "undefined". Drop that so the cluster
      // is stored without a type field (matches the storage shape the
      // server expects for a normal cluster).
      const type = group.newClusterType && group.newClusterType !== 'undefined'
        ? group.newClusterType
        : undefined;

      const newCluster = {
        title: group.newClusterTitle,
        description: group.newClusterDescription,
        url: group.newClusterUrl,
        localUrl: group.newClusterLocalUrl,
        type
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
      if (!cluster.newUrl.startsWith('http') && !cluster.newUrl.startsWith('/')) {
        this.error = 'A cluster URL must start with http:// or /';
        return;
      }
      if (cluster.newLocalUrl &&
          !cluster.newLocalUrl.startsWith('http') &&
          !cluster.newLocalUrl.startsWith('/')) {
        this.error = 'A cluster local URL must start with http:// or /';
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
/* side-by-side form label column -- vertically centered, zero
   vertical padding so the form rows sit flush. */
.form-side-label {
  display: flex;
  align-items: center;
  font-weight: 500;
  padding-top: 0;
  padding-bottom: 0;
}
/* tight gutter for the create/edit forms. Vuetify's v-row uses
   `margin: -12px` (shorthand) and v-col uses `padding: 12px`, which
   shows through even after zeroing top/bottom via row-gap wrap.
   !important wins over Vuetify's own stylesheet specificity. */
.compact-form :deep(.v-row) {
  margin-top: 0 !important;
  margin-bottom: 0 !important;
  row-gap: 0 !important;
}
.compact-form :deep(.v-row + .v-row) {
  margin-top: 2px !important;
}
.compact-form :deep(.v-col),
.compact-form :deep([class*="v-col-"]) {
  padding-top: 2px !important;
  padding-bottom: 2px !important;
}
.compact-form :deep(.v-row.mb-1) {
  margin-bottom: 0 !important;
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
  background: rgb(var(--v-theme-surface));
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

/* cluster grid -- responsive responsive flex layout that replaces the
   former bootstrap col-12/sm-6/md-6/lg-4/xl-3/xxl-2 row markup. */
.cluster-group {
  list-style: none;
  padding: 0;
  margin: 0;
  gap: 16px;
}
.cluster-group .cluster {
  flex: 1 1 100%;
  min-width: 0;
  position: relative;
}
@media (min-width: 600px) {
  .cluster-group .cluster { flex-basis: calc(50% - 16px); max-width: calc(50% - 16px); }
}
@media (min-width: 960px) {
  .cluster-group .cluster { flex-basis: calc(33.333% - 16px); max-width: calc(33.333% - 16px); }
}
@media (min-width: 1280px) {
  .cluster-group .cluster { flex-basis: calc(25% - 16px); max-width: calc(25% - 16px); }
}
@media (min-width: 1920px) {
  .cluster-group .cluster { flex-basis: calc(16.666% - 16px); max-width: calc(16.666% - 16px); }
}
.cluster-group .v-card {
  height: 100%;
  display: flex;
  flex-direction: column;
  box-shadow: 0 20px 40px -14px rgba(0, 0, 0, 0.25);
}
/* v-card-text fills available height so the v-card-actions footer
   anchors to the bottom of every card no matter how short the
   content is. */
.cluster-group :deep(.v-card-text) {
  flex: 1 1 auto;
  font-size: 1rem;
  line-height: 1.5;
}
.cluster-group :deep(.v-card-actions) {
  flex: 0 0 auto;
  font-size: 0.875rem;
}
.sortable-chosen, .sortable-chosen.sortable-ghost {
  /* https://github.com/RubaXa/Sortable/issues/1276 */
  z-index: 10000;
}

/* compact form for editing clusters */
.edit-cluster label {
  margin: 0;
  font-weight: 500;
}

/* cluster ES status chip lives at top-right of each card; nudge size to match the old badge */
.cluster-status-chip { font-size: 0.7rem; }

/* per-cluster stat chips should distribute across the card width.
   flex: 1 1 auto so each chip is at least as wide as its content, but
   any extra horizontal space gets shared evenly. flex-wrap on the
   parent lets them wrap to the next row when the card is narrow. */
.cluster-stat-chip.v-chip {
  flex: 1 1 auto;
  justify-content: center;
}

/* edit-mode stats list (checkbox + label rows) */
.cluster-stats-edit label {
  display: inline-flex;
  align-items: center;
  cursor: pointer;
  margin: 0;
}

/* keep the edit-mode toggle compact -- v-switch's default density still
   ships a fairly tall control. */
/* edit-mode toggle -- shrink the v-switch chrome but force a contrasting
   track color so the off state is visible in both themes (Vuetify's
   default track opacity gets lost in the dark navbar bg). */
.edit-mode-switch :deep(.v-switch__track) {
  height: 18px;
  min-width: 36px;
  background-color: rgba(var(--v-theme-on-surface), 0.45);
  opacity: 1;
}
.edit-mode-switch :deep(.v-selection-control) { min-height: 24px; }
.edit-mode-switch :deep(.v-switch__thumb) {
  background-color: rgb(var(--v-theme-on-primary));
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.4);
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
