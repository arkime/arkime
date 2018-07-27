<template>

  <div class="container-fluid">
    <!-- page error -->
    <div v-if="error"
      class="alert alert-danger">
      <span class="fa fa-exclamation-triangle">
      </span>&nbsp;
      {{ error }}
      <button type="button"
        class="close cursor-pointer"
        @click="error = false">
        <span>&times;</span>
      </button>
    </div> <!-- /page error -->

    <!-- search & create group -->
    <div class="row">
      <!-- search -->
      <div class="col-md-8">
        <div class="input-group">
          <span class="input-group-prepend">
            <span class="input-group-text">
              <span class="fa fa-search">
              </span>
            </span>
          </span>
          <input type="text"
            tabindex="8"
            v-model="searchTerm"
            class="form-control"
            placeholder="Search clusters"
            @keyup="debounceSearch()"
          />
        </div>
      </div> <!-- /search -->
      <!-- create group -->
      <div v-if="loggedIn"
        class="col-md-4">
        <a v-if="!showNewGroupForm"
          @click="showNewGroupForm = true"
          class="btn btn-outline-primary cursor-pointer pull-right">
          <span class="fa fa-plus-circle">
          </span>&nbsp;
          New Group
        </a>
        <span v-else>
          <a @click="createNewGroup"
            class="btn btn-outline-success cursor-pointer pull-right">
            <span class="fa fa-plus-circle"></span>&nbsp;
            Create
          </a>
          <a @click="cancelCreateNewGroup"
            class="btn btn-outline-warning cursor-pointer pull-right mr-1">
            <span class="fa fa-ban">
            </span>&nbsp;
            Cancel
          </a>
        </span>
      </div> <!-- /create group -->
    </div> <!-- /search & create group -->

    <hr>

    <!-- new group form -->
    <div v-if="showNewGroupForm && loggedIn"
      class="row">
      <div class="col-md-12">
        <div @keyup.enter="createNewGroup">
          <div class="form-group row">
            <label for="newGroupTitle"
              class="col-sm-2 col-form-label">
              Title<sup class="text-danger">*</sup>
            </label>
            <div class="col-sm-10">
              <input type="text"
                v-model="newGroupTitle"
                class="form-control"
                id="newGroupTitle"
                placeholder="Group title"
              />
            </div>
          </div>
          <div class="form-group row">
            <label for="newGroupDescription"
              class="col-sm-2 col-form-label">
              Description
            </label>
            <div class="col-sm-10">
              <textarea rows="3"
                v-model="newGroupDescription"
                class="form-control"
                id="newGroupDescription"
                placeholder="Group description">
              </textarea>
            </div>
          </div>
        </div>
        <hr>
      </div>
    </div> <!-- /new group form -->

    <!-- no results for searchTerm filter -->
    <div v-if="searchTerm && !numFilteredClusters && parliament.groups.length"
      class="info-area vertical-center">
      <div class="text-muted">
        <span class="fa fa-3x fa-folder-open text-muted-more">
        </span>
        No clusters match your search
      </div>
    </div> <!-- /no results for searchTerm filter -->

    <!-- no groups -->
    <div v-if="parliament.groups && !parliament.groups.length && !showNewGroupForm"
      class="info-area vertical-center">
      <div class="text-muted">
        <span class="fa fa-3x fa-folder-open text-muted-more">
        </span>
        No groups in your parliament.
        <a v-if="loggedIn"
          @click="showNewGroupForm = true"
          class="cursor-pointer no-href no-decoration">
          Create one
        </a>
      </div>
    </div> <!-- /no groups -->

    <!-- groups -->
    <div ref="draggableGroups">
      <div v-for="group of parliament.groups"
        :id="group.id"
        :key="group.id"
        class="mb-4">

        <!-- group title/description -->
        <div class="row"
          v-if="group.filteredClusters.length > 0 || (!group.clusters.length && !searchTerm)">
          <div class="col-md-12">
            <h5 class="mb-1">
              <!-- group action buttons -->
              <span v-if="loggedIn">
                <a v-if="groupAddingCluster !== group.id && groupBeingEdited !== group.id"
                  @click="displayNewClusterForm(group)"
                  class="btn btn-sm btn-outline-info pull-right cursor-pointer mb-1">
                  <span class="fa fa-plus-circle">
                  </span>&nbsp;
                  New Cluster
                </a>
                <a v-if="groupAddingCluster === group.id"
                  @click="createNewCluster(group)"
                  class="btn btn-sm btn-outline-success cursor-pointer pull-right mb-1">
                  <span class="fa fa-plus-circle">
                  </span>&nbsp;
                  Create
                </a>
                <a v-if="groupBeingEdited === group.id"
                  @click="editGroup(group)"
                  class="btn btn-sm btn-outline-success pull-right cursor-pointer mr-1 mb-1">
                  <span class="fa fa-save">
                  </span>&nbsp;
                  Save
                </a>
                <a v-if="groupAddingCluster === group.id || groupBeingEdited === group.id"
                  @click="cancelUpdateGroup(group)"
                  class="btn btn-sm btn-outline-warning cursor-pointer pull-right mr-1 mb-1">
                  <span class="fa fa-ban">
                  </span>&nbsp;
                  Cancel
                </a>
                <a v-if="groupBeingEdited !== group.id && groupAddingCluster !== group.id"
                  @click="displayEditGroupForm(group)"
                  class="btn btn-sm btn-outline-warning pull-right cursor-pointer mr-1 mb-1">
                  <span class="fa fa-pencil">
                  </span>&nbsp;
                  Edit Group
                </a>
              </span> <!-- /group action buttons -->
              {{ group.title }}&nbsp;
              <a v-if="loggedIn && groupAddingCluster !== group.id && groupBeingEdited === group.id"
                @click="deleteGroup(group)"
                v-b-tooltip.hover.top
                title="Delete Group"
                class="btn btn-sm btn-outline-danger cursor-pointer ml-2">
                <span class="fa fa-trash-o">
                </span>
              </a>
            </h5>
            <p class="mb-2">
              {{ group.description }}
            </p>
            <div v-if="group.error"
              class="alert alert-danger alert-sm">
              <span class="fa fa-exclamation-triangle">
              </span>&nbsp;
              {{ group.error }}
              <button type="button"
                class="close cursor-pointer"
                @click="group.error = false">
                <span>&times;</span>
              </button>
            </div>
          </div>
        </div> <!-- /group title/description -->

        <!-- edit group form -->
        <div v-if="loggedIn && groupBeingEdited === group.id"
          class="row">
          <div class="col-md-12">
            <form>
              <div class="form-group row">
                <label for="editGroupTitle"
                  class="col-sm-2 col-form-label">
                  Title<sup class="text-danger">*</sup>
                </label>
                <div class="col-sm-10">
                  <input type="text"
                    v-model="group.newTitle"
                    class="form-control"
                    id="editGroupTitle"
                    placeholder="Group title"
                  />
                </div>
              </div>
              <div class="form-group row">
                <label for="editGroupDescription"
                  class="col-sm-2 col-form-label">
                  Description
                </label>
                <div class="col-sm-10">
                  <textarea rows="3"
                    v-model="group.newDescription"
                    class="form-control"
                    id="editGroupDescription"
                    placeholder="Group description">
                  </textarea>
                </div>
              </div>
            </form>
            <hr>
          </div>
        </div> <!-- /edit group form -->

        <!-- create cluster form -->
        <div v-if="loggedIn && groupAddingCluster === group.id">
          <div class="col-md-12">
            <hr>
            <form @keyup.enter="createCluster">
              <div class="form-group row">
                <label for="newClusterTitle"
                  class="col-sm-2 col-form-label">
                  Title<sup class="text-danger">*</sup>
                </label>
                <div class="col-sm-10">
                  <input type="text"
                    v-model="group.newClusterTitle"
                    class="form-control"
                    id="newClusterTitle"
                    placeholder="Cluster title"
                  />
                </div>
              </div>
              <div class="form-group row">
                <label for="newClusterDescription"
                  class="col-sm-2 col-form-label">
                  Description
                </label>
                <div class="col-sm-10">
                  <textarea rows="3"
                    v-model="group.newClusterDescription"
                    class="form-control"
                    id="newClusterDescription"
                    placeholder="Cluster description">
                  </textarea>
                </div>
              </div>
              <div class="form-group row">
                <label for="newClusterUrl"
                  class="col-sm-2 col-form-label">
                  Url<sup class="text-danger">*</sup>
                </label>
                <div class="col-sm-10">
                  <input type="text"
                    v-model="group.newClusterUrl"
                    class="form-control"
                    id="newClusterUrl"
                    placeholder="Cluster url"
                  />
                </div>
              </div>
              <div class="form-group row">
                <label for="newClusterLocalUrl"
                  class="col-sm-2 col-form-label">
                  Local Url
                </label>
                <div class="col-sm-10">
                  <input type="text"
                    v-model="group.newClusterLocalUrl"
                    class="form-control"
                    id="newClusterLocalUrl"
                    placeholder="Cluster local url"
                  />
                </div>
              </div>
              <div class="row">
                <div class="col-sm-10 offset-sm-2">
                  <div class="form-check form-check-inline">
                    <label class="form-check-label">
                      <input class="form-check-input"
                        v-model="group.newClusterMultiviewer"
                        type="checkbox"
                        id="newClusterMultiviewer"
                      />
                      Multiviewer
                    </label>
                  </div>
                  <div class="form-check form-check-inline">
                    <label class="form-check-label">
                      <input class="form-check-input"
                        type="checkbox"
                        id="newClusterDisabled"
                        v-model="group.newClusterDisabled"
                      />
                      Disabled
                    </label>
                  </div>
                </div>
              </div>
            </form>
            <hr>
          </div>
        </div> <!-- /create cluster form -->

        <!-- clusters -->
        <ul v-if="group.filteredClusters.length"
          :id="group.id"
          ref="draggableClusters"
          class="cluster-group d-flex flex-wrap row mb-4">
          <li v-for="cluster in group.filteredClusters"
            :key="cluster.id"
            :id="cluster.id"
            class="cluster col-12 col-sm-6 col-md-6 col-lg-4 col-xl-3 col-xxl-2 mb-1">
            <div class="card bg-light">
              <div class="card-body">
                <!-- cluster title -->
                <span v-if="!cluster.disabled"
                  class="badge badge-pill badge-secondary cursor-help pull-right"
                  :class="{'badge-success':cluster.status === 'green','badge-warning':cluster.status === 'yellow','badge-danger':cluster.status === 'red'}"
                  v-b-tooltip.hover.top
                  :title="`Moloch ES Status ${cluster.healthError}`">
                  <span v-if="cluster.status">
                    {{ cluster.status }}
                  </span>
                  <span v-if="cluster.healthError"
                    class="fa fa-exclamation-triangle">
                  </span>
                  <span v-if="!cluster.status && !cluster.healthError">
                    ????
                  </span>
                </span>
                <h6>
                  <span v-if="cluster.multiviewer"
                    class="fa fa-sitemap text-muted cursor-help"
                    v-b-tooltip.hover.top
                    title="Mutiviewer cluster">
                  </span>
                  <span v-if="cluster.disabled"
                    class="text-muted fa fa-eye-slash cursor-help"
                    v-b-tooltip.hover.top
                    title="Disabled cluster">
                  </span>
                  <a v-if="!cluster.disabled"
                    class="no-decoration"
                    :href="`${cluster.url}/sessions`">
                    {{ cluster.title }}
                  </a>
                  <span v-else>
                    {{ cluster.title }}
                  </span>
                  <a :href="`${cluster.url}/stats?statsTab=0`"
                    class="no-decoration ml-2"
                    v-b-tooltip.hover.top
                    title="Go to the Stats page of this cluster">
                    <span class="fa fa-bar-chart">
                    </span>
                  </a>
                </h6> <!-- /cluster title -->
                <hr>
                <!-- cluster description -->
                <p class="text-muted small mb-2">
                  {{ cluster.description }}
                </p> <!-- /cluster description -->
                <!-- cluster error -->
                <div v-if="cluster.error"
                  class="alert alert-danger alert-sm">
                  <button type="button"
                    class="close cursor-pointer"
                    @click="cluster.error = false">
                    <span>&times;</span>
                  </button>
                  <span class="fa fa-exclamation-triangle">
                  </span>&nbsp;
                  {{ cluster.error }}
                </div> <!-- /cluster error -->
                <!-- cluster stats -->
                <small v-if="(!cluster.statsError && cluster.id !== clusterBeingEdited && !cluster.disabled && !cluster.multiviewer) || (cluster.id === clusterBeingEdited && !cluster.newDisabled && !cluster.newMultiviewer)">
                  <div class="row">
                    <div v-if="cluster.id === clusterBeingEdited || !cluster.hideDeltaBPS"
                      class="col-6">
                      <label :class="{'form-check-label':cluster.id === clusterBeingEdited}">
                        <input v-if="loggedIn && cluster.id === clusterBeingEdited"
                          type="checkbox"
                          :checked="!cluster.hideDeltaBPS"
                          @change="cluster.hideDeltaBPS = !cluster.hideDeltaBPS"
                        />
                        <strong class="d-inline-block">
                          {{ cluster.deltaBPS | commaString }}
                        </strong>
                        <small class="d-inline-block">
                          Bytes/Sec
                        </small>
                      </label>
                    </div>
                    <div v-if="cluster.id === clusterBeingEdited || !cluster.hideDeltaTDPS"
                      class="col-6">
                      <label :class="{'form-check-label':cluster.id === clusterBeingEdited}">
                        <input v-if="loggedIn && cluster.id === clusterBeingEdited"
                          type="checkbox"
                          :checked="!cluster.hideDeltaTDPS"
                          @change="cluster.hideDeltaTDPS = !cluster.hideDeltaTDPS"
                        />
                        <strong class="d-inline-block">
                          {{ cluster.deltaTDPS | commaString }}
                        </strong>
                        <small class="d-inline-block">
                          Packet Drops/Sec
                        </small>
                      </label>
                    </div>
                    <div v-if="cluster.id === clusterBeingEdited || !cluster.hideDataNodes"
                      class="col-6">
                      <label :class="{'form-check-label':cluster.id === clusterBeingEdited}">
                        <input v-if="loggedIn && cluster.id === clusterBeingEdited"
                          type="checkbox"
                          :checked="!cluster.hideDataNodes"
                          @change="cluster.hideDataNodes = !cluster.hideDataNodes"
                        />
                        <strong class="d-inline-block">
                          {{ cluster.dataNodes | commaString }}
                        </strong>
                        <small class="d-inline-block">
                          Data Nodes
                        </small>
                      </label>
                    </div>
                    <div v-if="cluster.id === clusterBeingEdited || !cluster.hideTotalNodes"
                      class="col-6">
                      <label :class="{'form-check-label':cluster.id === clusterBeingEdited}">
                        <input v-if="loggedIn && cluster.id === clusterBeingEdited"
                          type="checkbox"
                          :checked="!cluster.hideTotalNodes"
                          @change="cluster.hideTotalNodes = !cluster.hideTotalNodes"
                        />
                        <strong class="d-inline-block">
                          {{ cluster.totalNodes | commaString }}
                        </strong>
                        <small class="d-inline-block">
                          Total Nodes
                        </small>
                      </label>
                    </div>
                  </div>
                </small> <!-- /cluster stats -->
                <!-- cluster issues -->
                <small v-if="cluster.activeIssues">
                  <template v-if="showMoreIssuesFor.indexOf(cluster.id) > -1">
                    <div v-for="(issue, index) in cluster.activeIssues"
                      :key="getIssueTrackingId(issue)">
                      <issue :issue="issue"
                        :group-id="group.id"
                        :cluster-id="cluster.id"
                        :logged-in="loggedIn"
                        :index="index"
                        @issueChange="issueChange">
                      </issue>
                    </div>
                    <a v-if="cluster.activeIssues.length > 5"
                      href="javascript:void(0)"
                      class="no-decoration"
                      @click="showLessIssues(cluster)">
                      show less issues...
                    </a>
                  </template>
                  <template v-else>
                    <div v-for="(issue, index) in cluster.activeIssues.slice(0, 4)"
                      :key="getIssueTrackingId(issue)">
                      <issue :issue="issue"
                        :group-id="group.id"
                        :cluster-id="cluster.id"
                        :logged-in="loggedIn"
                        :index="index"
                        @issueChange="issueChange">
                      </issue>
                    </div>
                    <a v-if="cluster.activeIssues.length > 5"
                      href="javascript:void(0)"
                      class="no-decoration"
                      @click="showMoreIssues(cluster)">
                      show more issues...
                    </a>
                  </template>
                </small> <!-- /cluster issues -->
                <!-- edit cluster form -->
                <div v-if="loggedIn && cluster.id === clusterBeingEdited"
                  class="small">
                  <hr class="mt-2">
                  <form class="edit-cluster">
                    <div class="form-group">
                      <label for="newClusterTitle">
                        Title<sup class="text-danger">*</sup>
                      </label>
                      <input type="text"
                        v-model="cluster.newTitle"
                        class="form-control form-control-sm"
                        id="newClusterTitle"
                        placeholder="Cluster title"
                      />
                    </div>
                    <div class="form-group">
                      <label for="newClusterDescription">
                        Description
                      </label>
                      <textarea rows="3"
                        v-model="cluster.newDescription"
                        class="form-control form-control-sm"
                        id="newClusterDescription"
                        placeholder="Cluster description">
                      </textarea>
                    </div>
                    <div class="form-group">
                      <label for="newClusterUrl">
                        Url<sup class="text-danger">*</sup>
                      </label>
                      <input type="text"
                        v-model="cluster.newUrl"
                        class="form-control form-control-sm"
                        id="newClusterUrl"
                        placeholder="Cluster url"
                      />
                    </div>
                    <div class="form-group">
                      <label for="newClusterLocalUrl">
                        Local Url
                      </label>
                      <input type="text"
                        v-model="cluster.newLocalUrl"
                        class="form-control form-control-sm"
                        id="newClusterLocalUrl"
                        placeholder="Cluster local url"
                      />
                    </div>
                    <div class="form-check form-check-inline">
                      <label class="form-check-label">
                        <input type="checkbox"
                          v-model="cluster.newMultiviewer"
                          class="form-check-input"
                          id="newMultiviewer">
                        Multiviewer
                      </label>
                    </div>
                    <div class="form-check form-check-inline">
                      <label class="form-check-label">
                        <input type="checkbox"
                          v-model="cluster.newDisabled"
                          class="form-check-input"
                          id="newDisabled">
                        Disabled
                      </label>
                    </div>
                  </form>
                </div> <!-- /edit cluster form -->
              </div>
              <!-- edit cluster buttons -->
              <div v-if="loggedIn"
                class="card-footer small">
                <span v-show="cluster.id !== clusterBeingEdited">
                  <a @click="dismissAllIssues(group.id, cluster)"
                    class="btn btn-sm btn-outline-danger pull-right cursor-pointer"
                    title="Dismiss all isues for this cluster"
                    v-b-tooltip.hover.left>
                    <span class="fa fa-check">
                    </span>
                  </a>
                  <a class="btn btn-sm btn-outline-warning cursor-pointer"
                    @click="displayEditClusterForm(cluster)"
                    title="Edit cluster"
                    v-b-tooltip.hover.right>
                    <span class="fa fa-pencil">
                    </span>
                  </a>
                </span>
                <span v-show="cluster.id === clusterBeingEdited">
                  <a class="btn btn-sm btn-outline-success pull-right cursor-pointer"
                    @click="editCluster(group, cluster)"
                    title="Save cluster"
                    v-b-tooltip.hover.top>
                    <span class="fa fa-save">
                    </span>&nbsp;
                    Save
                  </a>
                  <a class="btn btn-sm btn-outline-warning pull-right cursor-pointer mr-1"
                    @click="cancelEditCluster(cluster)"
                    title="Cancel"
                    v-b-tooltip.hover>
                    <span class="fa fa-ban">
                    </span>
                  </a>
                  <a class="btn btn-sm btn-outline-danger cursor-pointer mr-1"
                    @click="deleteCluster(group, cluster)"
                    title="Delete cluster"
                    v-b-tooltip.hover.top>
                    <span class="fa fa-trash-o">
                    </span>
                  </a>
                </span>
              </div> <!-- /edit cluster buttons -->
            </div>
          </li>
        </ul> <!-- /clusters -->

        <!-- no clusters -->
        <div v-if="!group.clusters.length && !searchTerm && groupAddingCluster !== group.id">
          <strong>
            No clusters in this group.
            <a @click="displayNewClusterForm(group)"
              v-if="loggedIn"
              class="no-decoration cursor-pointer no-href">
              Create one
            </a>
          </strong>
        </div> <!-- no clusters -->

      </div>
    </div> <!-- /groups -->

  </div>

</template>

<script>
import ParliamentService from './parliament.service';
import Issue from './Issue';

import Sortable from 'sortablejs';

let timeout;
let interval;
let draggableGroups;
let draggableClusters;

export default {
  name: 'Parliament',
  components: {
    Issue
  },
  data: function () {
    return {
      // page error(s)
      error: '',
      initialized: false,
      // page data
      parliament: {},
      groupBeingEdited: undefined,
      groupAddingCluster: undefined,
      clusterBeingEdited: undefined,
      showMoreIssuesFor: [],
      // old parliament order to undo reordering
      oldParliamentOrder: {},
      // search vars
      searchTerm: '',
      numFilteredClusters: 0,
      // group form vars
      showNewGroupForm: false,
      newGroupTitle: '',
      newGroupDescription: ''
    };
  },
  computed: {
    loggedIn: function () {
      return this.$store.state.loggedIn;
    },
    // data refresh interval
    refreshInterval: function () {
      return this.$store.state.refreshInterval;
    }
  },
  watch: {
    refreshInterval: function (newVal, oldVal) {
      this.stopAutoRefresh();
      if (newVal) {
        this.loadData();
        this.startAutoRefresh();
      }
    }
  },
  mounted: function () {
    this.startAutoRefresh();
    this.loadData();

    setTimeout(() => {
      this.initializeGroupDragDrop();
      this.initializeClusterDragDrop();
    });
  },
  methods: {
    /* page functions -------------------------------------------------------- */
    debounceSearch: function () {
      if (timeout) { clearTimeout(timeout); }
      timeout = setTimeout(() => {
        this.filterClusters();
      }, 400);
    },
    cancelCreateNewGroup: function () {
      this.error = '';
      this.newGroupTitle = '';
      this.newGroupDescription = '';
      this.showNewGroupForm = false;
    },
    createNewGroup: function () {
      this.error = '';

      if (!this.newGroupTitle) {
        this.error = 'A group must have a title';
        return;
      }

      const newGroup = {
        title: this.newGroupTitle,
        description: this.newGroupDescription
      };

      ParliamentService.createGroup(newGroup)
        .then((data) => {
          this.error = '';
          this.newGroupTitle = '';
          this.newGroupDescription = '';
          this.showNewGroupForm = false;
          this.parliament.groups.push(data.group);
          this.filterClusters();
        })
        .catch((error) => {
          this.error = error.text || 'Unable to create group';
        });
    },
    displayEditGroupForm: function (group) {
      this.groupBeingEdited = group.id;
      group.newTitle = group.title;
      group.newDescription = group.description;
    },
    editGroup: function (group) {
      group.error = '';

      if (!group.newTitle) {
        group.error = 'A group must have a title';
        return;
      }

      const updatedGroup = {
        title: group.newTitle,
        description: group.newDescription
      };

      ParliamentService.editGroup(group.id, updatedGroup)
        .then((data) => {
          // update group with new values and close form
          group.error = '';
          group.title = group.newTitle;
          group.description = group.newDescription;
          this.groupBeingEdited = undefined;
        })
        .catch((error) => {
          group.error = error.text || 'Unable to udpate this group';
        });
    },
    deleteGroup: function (group) {
      group.error = '';

      ParliamentService.deleteGroup(group.id)
        .then((data) => {
          let index = 0; // remove the group from the parliament
          for (const g of this.parliament.groups) {
            if (g.title === group.title) {
              this.parliament.groups.splice(index, 1);
              break;
            }
            ++index;
          }
        })
        .catch((error) => {
          group.error = error.text || 'Unable to delete this group';
        });
    },
    cancelUpdateGroup: function (group) {
      this.groupBeingEdited = undefined;
      this.groupAddingCluster = undefined;
      group.newClusterTitle = '';
      group.newClusterDescription = '';
      group.newClusterUrl = '';
      group.newClusterLocalUrl = '';
      group.newClusterMultiviewer = false;
      group.newClusterDisabled = false;
    },
    displayNewClusterForm: function (group) {
      this.groupAddingCluster = group.id;
    },
    createNewCluster: function (group) {
      group.error = '';

      if (!group.newClusterTitle) {
        group.error = 'A cluster must have a title';
        return;
      }
      if (!group.newClusterUrl) {
        group.error = 'A cluster must have a url';
        return;
      }

      const newCluster = {
        title: group.newClusterTitle,
        description: group.newClusterDescription,
        url: group.newClusterUrl,
        localUrl: group.newClusterLocalUrl,
        multiviewer: group.newClusterMultiviewer,
        disabled: group.newClusterDisabled
      };

      ParliamentService.createCluster(group.id, newCluster)
        .then((data) => {
          group.error = '';
          group.clusters.push(data.cluster);
          this.cancelUpdateGroup(group);
          this.updateParliament(data.parliament);
          this.filterClusters();
        })
        .catch((error) => {
          group.error = error.text || 'Unable to add a cluster to this group';
        });
    },
    displayEditClusterForm: function (cluster) {
      this.clusterBeingEdited = cluster.id;
      cluster.newTitle = cluster.title;
      cluster.newDescription = cluster.description;
      cluster.newUrl = cluster.url;
      cluster.newLocalUrl = cluster.localUrl;
      cluster.newMultiviewer = cluster.multiviewer;
      cluster.newDisabled = cluster.disabled;
    },
    cancelEditCluster: function (cluster) {
      this.clusterBeingEdited = undefined;
    },
    editCluster: function (group, cluster) {
      cluster.error = '';

      if (!cluster.newTitle) {
        cluster.error = 'A cluster must have a title';
        return;
      }
      if (!cluster.newUrl) {
        cluster.error = 'A cluster must have a url';
        return;
      }

      const updatedCluster = {
        title: cluster.newTitle,
        description: cluster.newDescription,
        url: cluster.newUrl,
        localUrl: cluster.newLocalUrl,
        multiviewer: cluster.newMultiviewer,
        disabled: cluster.newDisabled,
        hideDeltaBPS: cluster.hideDeltaBPS,
        hideDataNodes: cluster.hideDataNodes,
        hideDeltaTDPS: cluster.hideDeltaTDPS,
        hideTotalNodes: cluster.hideTotalNodes
      };

      ParliamentService.editCluster(group.id, cluster.id, updatedCluster)
        .then((data) => {
          cluster.error = '';
          cluster.title = cluster.newTitle;
          cluster.description = cluster.newDescription;
          cluster.url = cluster.newUrl;
          cluster.localUrl = cluster.newLocalUrl;
          cluster.multiviewer = cluster.newMultiviewer;
          cluster.disabled = cluster.newDisabled;
          this.cancelEditCluster();
        })
        .catch((error) => {
          cluster.error = error.text || 'Unable to update this cluster';
        });
    },
    deleteCluster: function (group, cluster) {
      group.error = '';

      ParliamentService.deleteCluster(group.id, cluster.id)
        .then((data) => {
          group.error = '';
          let index = 0;
          for (const c of group.clusters) {
            if (c.id === cluster.id) {
              group.clusters.splice(index, 1);
              break;
            }
            ++index;
          }
          this.filterClusters();
        })
        .catch((error) => {
          group.error = error.text || 'Unable to remove cluster from this group';
        });
    },
    showMoreIssues: function (cluster) {
      this.showMoreIssuesFor.push(cluster.id);
    },
    showLessIssues: function (cluster) {
      let i = this.showMoreIssuesFor.indexOf(cluster.id);
      if (i > -1) { this.showMoreIssuesFor.splice(i, 1); }
    },
    dismissAllIssues: function (groupId, cluster) {
      ParliamentService.dismissAllIssues(groupId, cluster.id)
        .then((data) => {
          cluster.activeIssues = [];
        })
        .catch((error) => {
          this.error = error.text || 'Unable to dismiss all of the issues in this cluster';
        });
    },
    getIssueTrackingId: function (issue) {
      if (issue.node) {
        return `${issue.node.replace(/\s/g, '')}-${issue.type}`;
      } else {
        return issue.type;
      }
    },
    issueChange: function (changeEvent) {
      // find the cluster to display/hide error
      let cluster = this.getCluster(changeEvent.groupId, changeEvent.clusterId);

      if (!cluster && !changeEvent.success) {
        this.error = changeEvent.message;
        return;
      }

      if (changeEvent.success) {
        cluster.error = '';
        cluster.activeIssues.splice(changeEvent.index, 1);
      } else {
        cluster.error = changeEvent.message;
      }
    },
    /* helper functions ---------------------------------------------------- */
    loadData: function () {
      ParliamentService.getParliament()
        .then((data) => {
          this.error = '';
          this.updateParliament(data);
          this.filterClusters();
          this.oldParliamentOrder = JSON.parse(JSON.stringify(data));
        })
        .catch((error) => {
          this.error = error.text ||
            `Error fetching information about Molochs in your parliament.
             The information displayed below is likely out of date.`;
        });
    },
    startAutoRefresh: function () {
      if (!this.refreshInterval) { return; }
      interval = setInterval(() => {
        this.loadData();
      }, this.refreshInterval);
    },
    stopAutoRefresh: function () {
      clearInterval(interval);
    },
    // Updates fetched parliament with current view flags and values
    // Assumes that groups and clusters within groups are in the same order
    updateParliament: function (data) {
      if (!this.initialized) {
        this.parliament = data;
        this.initialized = true;
        return;
      }

      for (let g = 0, glen = data.groups.length; g < glen; ++g) {
        const newGroup = data.groups[g];
        const oldGroup = this.parliament.groups[g];

        newGroup.error = oldGroup.error;
        newGroup.newTitle = oldGroup.newTitle;
        newGroup.newDescription = oldGroup.newDescription;
        newGroup.filteredClusters = oldGroup.filteredClusters;
        newGroup.newClusterTitle = oldGroup.newClusterTitle;
        newGroup.newClusterDescription = oldGroup.newClusterDescription;
        newGroup.newClusterUrl = oldGroup.newClusterUrl;
        newGroup.newClusterLocalUrl = oldGroup.newClusterLocalUrl;
        newGroup.newClusterMultiviewer = oldGroup.newClusterMultiviewer;
        newGroup.newClusterDisabled = oldGroup.newClusterDisabled;

        for (let c = 0, clen = newGroup.clusters.length; c < clen; ++c) {
          const newCluster = newGroup.clusters[c];
          const oldCluster = oldGroup.clusters[c];

          newCluster.error = oldCluster.error;
          newCluster.newTitle = oldCluster.newTitle;
          newCluster.newDescription = oldCluster.newDescription;
          newCluster.newUrl = oldCluster.newUrl;
          newCluster.newLocalUrl = oldCluster.newLocalUrl;
          newCluster.newMultiviewer = oldCluster.newMultiviewer;
          newCluster.newDisabled = oldCluster.newDisabled;
          newCluster.activeIssues = oldCluster.activeIssues;
        }
      }

      this.parliament = data;
    },
    // Remove UI only properties from groups and clusters in a parliament
    sanitizeParliament: function (data) {
      for (const group of data.groups) {
        group.error = undefined;
        group.newTitle = undefined;
        group.newDescription = undefined;
        group.filteredClusters = undefined;
        group.newClusterTitle = undefined;
        group.newClusterDescription = undefined;
        group.newClusterUrl = undefined;
        group.newClusterLocalUrl = undefined;
        group.newClusterMultiviewer = undefined;
        group.newClusterDisabled = undefined;
        group.filteredClusters = undefined;

        for (const cluster of group.clusters) {
          cluster.error = undefined;
          cluster.newTitle = undefined;
          cluster.newDescription = undefined;
          cluster.newUrl = undefined;
          cluster.newLocalUrl = undefined;
          cluster.newMultiviewer = undefined;
          cluster.newDisabled = undefined;
        }
      }
    },
    // Removes clusters that don't match the search term provided
    filterClusters: function () {
      this.numFilteredClusters = 0;

      for (const group of this.parliament.groups) {
        if (!this.searchTerm) {
          group.filteredClusters = Object.assign([], group.clusters);
          this.numFilteredClusters += group.filteredClusters.length;
          continue;
        }

        group.filteredClusters = Object.assign([], group.clusters)
          .filter((item) => {
            return item.title.toLowerCase().indexOf(this.searchTerm.toLowerCase()) > -1;
          });

        this.numFilteredClusters += group.filteredClusters.length;
      }
    },
    getCluster: function (groupId, clusterId) {
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
    getGroup: function (groupId) {
      for (const group of this.parliament.groups) {
        if (group.id === groupId) {
          return group;
        }
      }
      return undefined;
    },
    updateOrder: function (errorText) {
      const parliamentClone = JSON.parse(JSON.stringify(this.parliament));
      this.sanitizeParliament(parliamentClone);

      ParliamentService.updateParliamentOrder(parliamentClone)
        .then((data) => {
          this.oldParliamentOrder = parliamentClone;
        })
        .catch((error) => {
          this.parliament = JSON.parse(JSON.stringify(this.oldParliamentOrder));
          this.error = error.text || errorText;
        });
    },
    initializeGroupDragDrop: function () {
      draggableGroups = Sortable.create(this.$refs.draggableGroups, {
        animation: 100,
        onMove: (event) => { // don't allow drag/drop if clusters are filtered
          if (this.searchTerm) { return false; }
          this.stopAutoRefresh();
        },
        onEnd: (event) => { // dragged group was dropped
          let newIdx = event.newIndex;
          let oldIdx = event.oldIndex;

          if (newIdx === oldIdx) {
            return;
          }

          let group = this.parliament.groups[oldIdx];
          let errorText = 'Error moving this group.';

          if (!group) {
            this.error = errorText;
            return;
          }

          this.parliament.groups.splice(oldIdx, 1);
          this.parliament.groups.splice(newIdx, 0, group);

          this.updateOrder(errorText);
          this.startAutoRefresh();
        }
      });
    },
    initializeClusterDragDrop: function () {
      for (let clusterGroup of this.$refs.draggableClusters) {
        draggableClusters = Sortable.create(clusterGroup, {
          group: 'clusters',
          animation: 100,
          onMove: (event) => { // don't allow drag/drop if clusters are filtered
            if (this.searchTerm) { return false; }
            this.stopAutoRefresh();
          },
          onEnd: (event) => { // dragged cluster was dropped
            let newIdx = event.newIndex;
            let oldIdx = event.oldIndex;
            let newGroupId = parseInt(event.to.id);
            let oldGroupId = parseInt(event.from.id);

            if (newIdx === oldIdx && newGroupId === oldGroupId) {
              return;
            }

            let clusterId = parseInt(event.item.id);
            let oldGroup = this.getGroup(oldGroupId);
            let newGroup = this.getGroup(newGroupId);
            let cluster = this.getCluster(oldGroupId, clusterId);
            let errorText = 'Error moving this cluster.';

            if (!oldGroup || !cluster) {
              this.error = errorText;
              return;
            }

            oldGroup.clusters.splice(oldIdx, 1);
            newGroup.clusters.splice(newIdx, 0, cluster);

            this.updateOrder(errorText);
            this.startAutoRefresh();
          }
        });
      }
    }
  },
  beforeDestroy: function () {
    if (draggableGroups) { draggableGroups.destroy(); }
    if (draggableClusters) { draggableClusters.destroy(); }
  }
};
</script>

<style scoped>
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
.cluster-group .card .card-body hr {
  margin-top: 0;
  margin-bottom: .3rem;
}
.cluster-group .card .card-footer {
  padding: 0.2rem 0.5rem;
}

/* compact form for editing clusters */
form.edit-cluster label {
  margin:0;
}
form.edit-cluster .form-group {
  margin-bottom: .25rem;
}
</style>
