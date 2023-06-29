<template>
  <div class="container-fluid mb-4 row">

    <!-- navigation -->
    <div
      role="tablist"
      aria-orientation="vertical"
      class="col-xl-2 col-lg-3 col-md-3 col-sm-4 col-xs-12 no-overflow">
      <div class="nav flex-column nav-pills">
        <a @click="openView('views')"
          class="nav-link cursor-pointer"
          :class="{'active':visibleTab === 'views'}">
          <span class="fa fa-fw fa-eye mr-1" />
          Views
          <b-button
            size="xs"
            class="float-right"
            variant="secondary"
            v-if="visibleTab === 'views'"
            @click.stop.prevent="openViewForm"
            v-b-tooltip.hover="'Create a new view'">
            <span class="fa fa-fw fa-plus-circle" />
          </b-button>
        </a>
        <a @click="openView('integrations')"
          class="nav-link cursor-pointer"
          :class="{'active':visibleTab === 'integrations'}">
          <span class="fa fa-fw fa-key mr-1" />
          Integrations
        </a>
        <a @click="openView('overviews')"
           class="nav-link cursor-pointer mb-1"
           :class="{'active':visibleTab === 'overviews'}">
          <span class="fa fa-fw fa-file-o mr-1" />
          Overviews
          <b-button
              size="xs"
              class="float-right"
              variant="secondary"
              v-if="visibleTab === 'overviews'"
              @click.stop.prevent="openOverviewForm"
              v-b-tooltip.hover="'Create a new overview'">
            <span class="fa fa-fw fa-plus-circle" />
          </b-button>
        </a>
        <template v-if="visibleTab === 'overviews'">
          <!-- overview create form -->
          <create-overview-modal />
          <!-- overviews -->
          <div v-for="iType in iTypes" :key="iType"
            class="itype-group-container" :style="{ 'border-color': iTypeColorMap[iType] }"
          >
            <a
                v-for="overview in getSortedOverviews.filter(o => o.iType === iType)"
                :key="overview._id"
                :title="overview.name"
                @click="setActiveOverviewId(overview._id)"
                :class="{ 'active':activeOverviewId === overview._id }"
                class="nav-link cursor-pointer w-100">
              <overview-selector-line :overview="overview" />
            </a>
          </div>
        </template>
        <a @click="openView('linkgroups')"
          class="nav-link cursor-pointer mb-1"
          :class="{'active':visibleTab === 'linkgroups'}">
          <span class="fa fa-fw fa-link mr-1" />
          Link Groups
          <b-button
            size="xs"
            class="float-right"
            variant="secondary"
            v-if="visibleTab === 'linkgroups'"
            @click.stop.prevent="openLinkGroupForm"
            v-b-tooltip.hover="'Create a new link group'">
            <span class="fa fa-fw fa-plus-circle" />
          </b-button>
        </a>
        <template v-if="visibleTab === 'linkgroups'">
          <reorder-list
            :index="i"
            :key="lg._id"
            @update="updateList"
            :list="getLinkGroups"
            v-for="(lg, i) in getLinkGroups"
            style="position:relative; max-width:calc(100% - 1rem); margin-left:1rem;">
            <template slot="handle">
              <span
                :id="`${lg._id}-tt`"
                class="fa fa-bars d-inline sub-nav-handle">
              </span>
              <b-tooltip
                noninteractive
                :target="`${lg._id}-tt`">
                Drag &amp; drop to reorder Link Groups
              </b-tooltip>
            </template>
            <template slot="default">
              <a :title="lg.name"
                @click="selectedLinkGroup = i"
                :class="{'active':selectedLinkGroup === i}"
                class="nav-link sub-nav-link cursor-pointer">
                {{ lg.name }}
              </a>
            </template>
          </reorder-list>
        </template>
        <a v-if="!disablePassword"
          class="nav-link cursor-pointer"
          @click="openView('password')"
          :class="{'active':visibleTab === 'password'}">
          <span class="fa fa-fw fa-lock mr-1" />
          Password
        </a>
      </div>
    </div> <!-- /navigation -->

    <div class="col-xl-10 col-lg-9 col-md-9 col-sm-8 col-xs-12 settings-right-panel">
      <!-- view settings -->
      <div v-if="visibleTab === 'views'">
        <!-- view create form -->
        <create-view-modal />
        <div class="mr-3 w-100 d-flex justify-content-between align-items-center">
          <h1>
            Views
          </h1>
          <b-input-group class="ml-4 mr-2">
            <template #prepend>
              <b-input-group-text>
                <span class="fa fa-search" />
              </b-input-group-text>
            </template>
            <b-form-input
              autofocus
              debounce="400"
              v-model="viewSearchTerm"
            />
          </b-input-group>
          <b-button
            class="no-wrap"
            v-b-modal.view-form
            variant="outline-success">
            <span class="fa fa-plus-circle mr-2" />
            New View
          </b-button>

          <b-form-checkbox
              button
              class="ml-2 no-wrap"
              v-model="seeAllViews"
              v-b-tooltip.hover
              @input="seeAllViewsChanged"
              v-if="roles.includes('cont3xtAdmin')"
              :title="seeAllViews ? 'Just show the views created from your activity or shared with you' : 'See all the views that exist for all users (you can because you are an ADMIN!)'">
            <span class="fa fa-user-circle mr-1" />
            See {{ seeAllViews ? ' MY ' : ' ALL ' }} Views
          </b-form-checkbox>
        </div>
        <div class="d-flex flex-wrap">
          <!-- no views -->
          <div class="row lead mt-4"
            v-if="getViews && (!getViews.length || !getViews.filter(v => v._editable).length)">
            <div class="col">
              No Views are configured or shared for you to edit.
              <b-button
                variant="link"
                v-b-modal.view-form>
                Create one!
              </b-button>
            </div>
          </div> <!-- /no views -->
          <!-- no view results -->
          <div class="row lead mt-4"
            v-else-if="viewSearchTerm && !filteredViews.length">
            <div class="col">
              No Views match your search.
            </div>
          </div> <!-- /no view results -->
          <!-- views -->
          <template v-for="view in filteredViews">
            <div
              :key="`${view._id}`"
              :id="view._id"
              class="w-25 p-2"
              v-if="view._editable || roles.includes('cont3xtAdmin')">
              <b-card>
                <template #header>
                  <div class="w-100 d-flex justify-content-between align-items-start">
                    <div>
                      <!-- delete button -->
                      <transition name="buttons">
                        <b-button
                          size="sm"
                          variant="danger"
                          v-if="!confirmDeleteView[view._id]"
                          v-b-tooltip.hover.top="'Delete this view.'"
                          @click.stop.prevent="toggleDeleteView(view._id)">
                          <span class="fa fa-trash-o" />
                        </b-button>
                      </transition> <!-- /delete button -->
                      <!-- cancel confirm delete button -->
                      <transition name="buttons">
                        <b-button
                          size="sm"
                          title="Cancel"
                          variant="warning"
                          v-b-tooltip.hover
                          v-if="confirmDeleteView[view._id]"
                          @click.stop.prevent="toggleDeleteView(view._id)">
                          <span class="fa fa-ban" />
                        </b-button>
                      </transition> <!-- /cancel confirm delete button -->
                      <!-- confirm delete button -->
                      <transition name="buttons">
                        <b-button
                          size="sm"
                          variant="danger"
                          v-b-tooltip.hover
                          title="Are you sure?"
                          v-if="confirmDeleteView[view._id]"
                          @click.stop.prevent="deleteView(view)">
                          <span class="fa fa-check" />
                        </b-button>
                      </transition> <!-- /confirm delete button -->
                    </div>
                    <b-alert
                      variant="success"
                      :show="view.success"
                      class="mb-0 mt-0 alert-sm mr-1 ml-1">
                      <span class="fa fa-check mr-2" />
                      Saved!
                    </b-alert>
                    <b-alert
                      variant="danger"
                      :show="view.error"
                      class="mb-0 mt-0 alert-sm mr-1 ml-1">
                      <span class="fa fa-check mr-2" />
                      Error!
                    </b-alert>
                    <div>
                      <transition name="buttons">
                        <b-button
                          :class="{'invisible': !updatedViewMap[view._id]}"
                          size="sm"
                          variant="warning"
                          @click="cancelUpdateView(view)"
                          v-b-tooltip.hover="'Cancel changes to this view'">
                          <span class="fa fa-ban" />
                        </b-button>
                      </transition>
                      <transition name="buttons">
                        <b-button
                          :class="{'invisible': !updatedViewMap[view._id]}"
                          size="sm"
                          variant="success"
                          @click="saveView(view)"
                          v-b-tooltip.hover="'Save this view'">
                          <span class="fa fa-save" />
                        </b-button>
                      </transition>
                    </div>
                  </div>
                </template>
                <ViewForm
                  :view="view"
                  @update-view="updateView"
                />
              </b-card>
            </div>
          </template> <!-- /views -->
        </div>
      </div> <!-- /view settings -->

      <!-- integrations settings -->
      <div v-if="visibleTab === 'integrations'">
        <div class="ml-2 mr-3 w-100 d-flex justify-content-between align-items-center">
          <h1>
            Integrations
          </h1>
          <b-input-group class="ml-4 mr-2">
            <template #prepend>
              <b-input-group-text>
                <span class="fa fa-search" />
              </b-input-group-text>
            </template>
            <b-form-input
              autofocus
              debounce="400"
              v-model="integrationSearchTerm"
            />
          </b-input-group>
          <div class="mr-3 no-wrap">
            <b-button
              class="mr-1"
              variant="outline-warning"
              @click="toggleRawIntegrationSettings">
              <span class="fa fa-pencil mr-2" />
              Raw Edit
            </b-button>
            <b-button
              variant="outline-success"
              @click="saveIntegrationSettings">
              <span class="fa fa-save mr-2" />
              Save
            </b-button>
          </div>
        </div>
        <div class="d-flex flex-wrap">
          <template v-if="!rawIntegrationSettings">
            <div class="row lead mt-4"
              v-if="Object.keys(sortedFilteredIntegrationSettings).length === 0">
              <div class="col">
                No Integrations match your search.
              </div>
            </div>
            <div
              :key="key"
              class="w-25 p-2"
              v-for="([key, setting]) in sortedFilteredIntegrationSettings">
              <b-card>
                <template #header>
                  <h4 class="mb-0 d-inline">
                    <img
                      v-if="getIntegrations[key]"
                      class="integration-setting-img"
                      :src="getIntegrations[key].icon"
                    />
                    {{ key }}
                  </h4>
                  <div class="pull-right mt-1">
                    <span
                      v-if="setting.globalConfiged"
                      class="fa fa-globe fa-lg mr-2 cursor-help"
                      v-b-tooltip.hover="'This intergration has been globally configured by the admin with a shared account. If you fill out the account fields below, it will override that configuration.'"
                    />
                    <a target="_blank"
                      :href="setting.homePage"
                      v-if="!!setting.homePage"
                      v-b-tooltip.hover="`${key} home page`">
                      <span class="fa fa-home fa-lg" />
                    </a>
                  </div>
                </template>
                <template v-for="(field, name) in setting.settings">
                  <b-form-checkbox
                    :key="name"
                    v-if="field.type === 'boolean'"
                    v-model="setting.values[name]">
                    {{ name }}
                  </b-form-checkbox>
                  <b-input-group
                    v-else
                    size="sm"
                    :key="name"
                    class="mb-1 mt-1">
                    <b-input-group-prepend
                      class="cursor-help"
                      v-b-tooltip.hover="field.help">
                      <b-input-group-text>
                        {{ name }}
                        <span class="text-info"
                          v-if="field.required">*</span>
                      </b-input-group-text>
                    </b-input-group-prepend>
                    <b-form-input
                      v-model="setting.values[name]"
                      :state="getState(field, setting, name)"
                      :type="field.password && !field.showValue ? 'password' : 'text'"
                    />
                    <b-input-group-append
                      v-if="field.password"
                      @click="toggleVisiblePasswordField(field)">
                      <b-input-group-text>
                        <span class="fa"
                          :class="{'fa-eye':field.password && !field.showValue, 'fa-eye-slash':field.password && field.showValue}">
                        </span>
                      </b-input-group-text>
                    </b-input-group-append>
                  </b-input-group>
                </template>
              </b-card>
            </div>
          </template>
          <textarea
            v-else
            rows="20"
            size="sm"
            @input="e => debounceRawEdit(e)"
            class="form-control form-control-sm"
            :value="createINI(rawIntegrationSettings)"
          />
        </div>
      </div> <!-- /integrations settings -->

      <!-- overviews settings -->
      <div v-if="visibleTab === 'overviews'">
        <div class="ml-2 mr-3 w-100 d-flex justify-content-between align-items-center">
          <h1>
            Overviews
          </h1>
          <span class="pull-right">
            <b-button
                variant="outline-primary"
                v-b-modal.overview-form
            >
              <span class="fa fa-plus-circle" />
              New Overview
            </b-button>
            <b-form-checkbox
                button
                class="ml-2 no-wrap"
                v-model="seeAllOverviews"
                v-b-tooltip.hover
                @input="seeAllOverviewsChanged"
                v-if="roles.includes('cont3xtAdmin')"
                :title="seeAllOverviews ? 'Just show the overviews created from your activity or shared with you' : 'See all the overviews that exist for all users (you can because you are an ADMIN!)'">
              <span class="fa fa-user-circle mr-1" />
              See {{ seeAllOverviews ? ' MY ' : ' ALL ' }} Overviews
            </b-form-checkbox>
          </span>
        </div>

        <!-- overview error -->
        <b-alert
            dismissible
            variant="danger"
            style="z-index: 2000;"
            v-model="overviewsError"
            class="position-fixed fixed-bottom m-0 rounded-0">
          {{ getOverviewsError }}
        </b-alert> <!-- /overview error -->

        <div class="d-flex flex-wrap">
          <!-- overview-form-card uses :key to reset form when swapping active overview -->
          <overview-form-card
              v-if="activeOverviewId && activeUnModifiedOverview"
              :key="activeOverviewId"
              :overview="activeUnModifiedOverview"
              :modifiedOverview="activeModifiedOverview"
              @update-modified-overview="updateModifiedOverview"
              @overview-deleted="activeOverviewDeleted"
          />
          <div v-else
               class="d-flex flex-column">
            <span>
              No Overviews configured.
            </span>
            <b-button
                variant="outline-primary"
                v-b-modal.overview-form>
              Create one!
            </b-button>
          </div>
        </div>
      </div> <!-- /overviews settings -->

      <!-- link group settings -->
      <div v-if="visibleTab === 'linkgroups'">
        <!-- link group create form -->
        <create-link-group-modal />
        <!-- link groups -->
        <h1>
          Link Groups
          <span class="pull-right">
            <b-button
                variant="outline-primary"
                v-b-modal.link-group-form>
              <span class="fa fa-plus-circle" />
              New Group
            </b-button>
            <b-form-checkbox
                button
                class="ml-2 no-wrap"
                v-model="seeAllLinkGroups"
                v-b-tooltip.hover
                @input="seeAllLinkGroupsChanged"
                v-if="roles.includes('cont3xtAdmin')"
                :title="seeAllLinkGroups ? 'Just show the link groups created from your activity or shared with you' : 'See all the link groups that exist for all users (you can because you are an ADMIN!)'">
              <span class="fa fa-user-circle mr-1" />
              See {{ seeAllLinkGroups ? ' MY ' : ' ALL ' }} Groups
            </b-form-checkbox>
          </span>
        </h1>

        <!-- link group error -->
        <b-alert
          dismissible
          variant="danger"
          style="z-index: 2000;"
          v-model="linkGroupsError"
          class="position-fixed fixed-bottom m-0 rounded-0">
          {{ getLinkGroupsError }}
        </b-alert> <!-- /link group error -->
        <!-- link groups -->
        <link-group-card
          v-if="getLinkGroups && getLinkGroups.length && getLinkGroups[selectedLinkGroup]"
          :link-group="getLinkGroups[selectedLinkGroup]"
          :key="getLinkGroups[selectedLinkGroup]._id"
          :pre-updated-link-group="updatedLinkGroupMap[getLinkGroups[selectedLinkGroup]._id]"
          @update-link-group="updateLinkGroup"
        /> <!-- /link groups -->
        <!-- no link groups -->
        <div
          class="row lead mt-4"
          v-if="getLinkGroups && !getLinkGroups.length">
          <div class="col">
            No Link Groups are configured.
            <b-button
              variant="link"
              v-b-modal.link-group-form>
              Create one!
            </b-button>
          </div>
        </div> <!-- /no link groups -->
      </div> <!-- /link group settings -->

      <!-- password settings -->
      <div v-if="visibleTab === 'password' && !disablePassword">
        <h1>
          Change Password
        </h1>

        <form class="row">
          <div class="col-9 mt-4">
            <!-- current password -->
            <b-input-group
              class="mt-2"
              prepend="Current Password">
              <b-form-input
                type="password"
                v-model="currentPassword"
                @keydown.enter="changePassword"
                placeholder="Enter your current password"
              />
            </b-input-group>
            <!-- new password -->
            <b-input-group
              class="mt-2"
              prepend="New Password">
              <b-form-input
                type="password"
                v-model="newPassword"
                @keydown.enter="changePassword"
                placeholder="Enter a new password"
              />
            </b-input-group>
            <!-- confirm new password -->
            <b-input-group
              class="mt-2"
              prepend="New Password">
              <b-form-input
                type="password"
                v-model="confirmNewPassword"
                @keydown.enter="changePassword"
                placeholder="Confirm your new password"
              />
            </b-input-group>
            <!-- change password button -->
            <button type="button"
              class="btn btn-success mt-2"
              @click="changePassword">
              Change Password
            </button>
          </div>
        </form>
      </div> <!-- /password settings -->
    </div>

    <!-- messages -->
    <b-alert
      :show="!!msg"
      class="position-fixed fixed-bottom m-0 rounded-0"
      style="z-index: 2000;"
      :variant="msgType"
      dismissible>
      {{ msg }}
    </b-alert> <!-- messages -->

  </div>
</template>

<script>
import Vue from 'vue';
import { mapGetters } from 'vuex';

import ReorderList from '@/utils/ReorderList';
import ViewForm from '@/components/views/ViewForm';
import UserService from '@/components/services/UserService';
import LinkGroupCard from '@/components/links/LinkGroupCard';
import CreateViewModal from '@/components/views/CreateViewModal';
import Cont3xtService from '@/components/services/Cont3xtService';
import CreateLinkGroupModal from '@/components/links/CreateLinkGroupModal';
import LinkService from '@/components/services/LinkService';
import OverviewService from '@/components/services/OverviewService';
import OverviewFormCard from '@/components/overviews/OverviewFormCard';
import CreateOverviewModal from '@/components/overviews/CreateOverviewModal.vue';
import OverviewSelectorLine from '@/components/overviews/OverviewSelectorLine.vue';
import { iTypes, iTypeIconMap, iTypeColorMap } from '@/utils/iTypes';
import CommonUserService from '../../../../../common/vueapp/UserService';

let timeout;

export default {
  name: 'Cont3xtSettings',
  components: {
    OverviewSelectorLine,
    CreateOverviewModal,
    OverviewFormCard,
    ViewForm,
    ReorderList,
    LinkGroupCard,
    CreateViewModal,
    CreateLinkGroupModal
  },
  data () {
    return {
      // page vars
      msg: '',
      msgType: '',
      visibleTab: 'views',
      // integrations
      integrationSettings: {},
      integrationSearchTerm: '',
      filteredIntegrationSettings: {},
      rawIntegrationSettings: undefined,
      // overviews
      iTypes,
      iTypeIconMap,
      iTypeColorMap,
      activeOverviewId: undefined,
      modifiedOverviewMap: {},
      // link groups
      selectedLinkGroup: 0,
      updatedLinkGroupMap: {},
      // views
      filteredViews: undefined,
      viewSearchTerm: '',
      viewForm: false,
      updatedViewMap: {},
      confirmDeleteView: {},
      // password
      currentPassword: '',
      newPassword: '',
      confirmNewPassword: ''
    };
  },
  created () {
    let tab = window.location.hash;
    if (tab) { // if there is a tab specified and it's a valid tab
      tab = tab.replace(/^#/, '');
      if (tab === 'views' || tab === 'integrations' || tab === 'overviews' || tab === 'linkgroups' ||
        tab === 'password') {
        this.visibleTab = tab;
      }
    }

    UserService.getIntegrationSettings().then((response) => {
      this.integrationSettings = response;
      this.filteredIntegrationSettings = JSON.parse(JSON.stringify(response));
    }).catch((err) => {
      this.showMessage({ variant: 'danger', message: err });
    });

    this.filterViews(this.viewSearchTerm);

    if (this.getOverviews != null) {
      this.setActiveOverviewToFirst();
    }
  },
  computed: {
    ...mapGetters([
      'getLinkGroups', 'getLinkGroupsError', 'getIntegrations', 'getViews', 'getUser',
      'getOverviews', 'getOverviewsError', 'getSortedOverviews', 'getCorrectedSelectedOverviewIdMap'
    ]),
    seeAllViews: {
      get () { return this.$store.state.seeAllViews; },
      set (value) { this.$store.commit('SET_SEE_ALL_VIEWS', value); }
    },
    seeAllLinkGroups: {
      get () { return this.$store.state.seeAllLinkGroups; },
      set (value) { this.$store.commit('SET_SEE_ALL_LINK_GROUPS', value); }
    },
    seeAllOverviews: {
      get () { return this.$store.state.seeAllOverviews; },
      set (value) { this.$store.commit('SET_SEE_ALL_OVERVIEWS', value); }
    },
    linkGroupsError: {
      get () {
        return !!this.$store.state.linkGroupsError;
      },
      set () {
        this.$store.commit('SET_LINK_GROUPS_ERROR', '');
      }
    },
    overviewsError: {
      get () {
        return !!this.$store.state.overviewsError;
      },
      set () {
        this.$store.commit('SET_OVERVIEWS_ERROR', '');
      }
    },
    activeUnModifiedOverview () {
      return this.getOverviews?.find(overview => overview._id === this.activeOverviewId);
    },
    activeModifiedOverview () {
      return this.modifiedOverviewMap?.[this.activeOverviewId];
    },
    roles () {
      return this.getUser?.roles ?? [];
    },
    sortedFilteredIntegrationSettings () {
      const entries = Object.entries(this.filteredIntegrationSettings);
      entries.sort(([aKey], [bKey]) => aKey.localeCompare(bKey));
      return entries;
    },
    disablePassword () {
      if (!this.getUser) { return true; } // wait for user to be initialized
      return !!this.$constants.DISABLE_USER_PASSWORD_UI && !!this.getUser.headerAuthEnabled;
    }
  },
  watch: {
    integrationSearchTerm (searchTerm) {
      if (!searchTerm) {
        this.filteredIntegrationSettings = JSON.parse(JSON.stringify(this.integrationSettings));
        return;
      }

      const query = searchTerm.toLowerCase();

      for (const key in this.integrationSettings) {
        if (key.toString().toLowerCase().match(query)?.length > 0) {
          this.$set(this.filteredIntegrationSettings, key, JSON.parse(JSON.stringify(this.integrationSettings[key])));
          continue;
        }
        Vue.delete(this.filteredIntegrationSettings, key);
      }
    },
    viewSearchTerm (searchTerm) {
      this.filterViews(searchTerm);
    },
    linkGroups (newValue, oldValue) { // changes the selected index when linkGroups changes
      const findNextBestIndex = () => {
        const newLength = newValue?.length;
        // undefined if the new list does not exist or is empty
        if (!newLength) { return undefined; }
        // remain on the current index, or use the last element in the list
        return Math.min(this.selectedLinkGroup ?? 0, newLength - 1);
      };

      // do not try to link old to new index, if old does not exist
      if (!oldValue?.length) {
        this.selectedLinkGroup = findNextBestIndex();
        return;
      }

      // using the old id, try to match to the index of that link group in the new list -- if existent
      const previousSelectedLinkGroupId = oldValue[this.selectedLinkGroup]?._id;
      if (previousSelectedLinkGroupId == null) {
        this.selectedLinkGroup = findNextBestIndex();
        return;
      }
      const newIndex = newValue.findIndex(v => v._id === previousSelectedLinkGroupId);
      this.selectedLinkGroup = (newIndex !== -1) ? newIndex : findNextBestIndex();
    },
    getViews () {
      this.filterViews(this.viewSearchTerm);
    },
    getOverviews (newValue, oldValue) {
      if (oldValue == null) {
        this.setActiveOverviewToFirst();
      }
    },
    activeUnModifiedOverview () {
      if (this.activeUnModifiedOverview === undefined) {
        this.setActiveOverviewToFirst();
      }
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    /* MISC! --------------------------------- */
    /* opens a specific settings tab */
    openView (tabName) {
      if (this.visibleTab === tabName) { return; }
      this.visibleTab = tabName;
      this.$router.push({
        hash: tabName
      });
    },
    /* INTEGRATIONS! ------------------------- */
    /* toggles the visibility of the value of password fields */
    toggleVisiblePasswordField (field) {
      this.$set(field, 'showValue', !field.showValue);
    },
    saveIntegrationSettings () {
      const settings = this.getIntegrationSettingValues();

      UserService.setIntegrationSettings({ settings }).then((response) => {
        this.showMessage({ variant: 'success', message: 'Saved!' });
        // NOTE: don't need to do anything with the data (the store does it)
        Cont3xtService.getIntegrations();
      }).catch((err) => {
        this.showMessage({ variant: 'danger', message: err });
      });
    },
    toggleRawIntegrationSettings () {
      if (this.rawIntegrationSettings) {
        this.rawIntegrationSettings = undefined;
        return;
      }
      const settings = this.getIntegrationSettingValues();
      this.rawIntegrationSettings = settings;
    },
    debounceRawEdit (e) {
      if (timeout) { clearTimeout(timeout); }
      // debounce the textarea so it only updates the integration settings after keyups cease for 400ms
      timeout = setTimeout(() => {
        timeout = null;
        this.updateRawIntegrationSettings(e);
      }, 400);
    },
    updateRawIntegrationSettings (e) {
      const rawIntegrationSettings = this.parseINI(e.target.value);

      for (const s in this.integrationSettings) {
        if (rawIntegrationSettings[s] && this.integrationSettings[s]) {
          this.$set(this.integrationSettings[s], 'values', rawIntegrationSettings[s]);
        }
      }

      this.filteredIntegrationSettings = JSON.parse(JSON.stringify(this.integrationSettings));
    },
    getState (field, setting, sname) {
      if (!field.required) {
        return undefined;
      }

      return setting.values[sname] ? setting.values[sname].length > 0 : false;
    },
    /* OVERVIEWS! ---------------------------- */
    setActiveOverviewToFirst () {
      this.setActiveOverviewId(this.getSortedOverviews[0]?._id);
    },
    setActiveOverviewId (newActiveOverviewId) {
      const newActiveOverview = this.getOverviews?.find(overview => overview._id === newActiveOverviewId);
      if (newActiveOverview == null) { return; }

      if (this.modifiedOverviewMap[newActiveOverviewId] == null) {
        Vue.set(this.modifiedOverviewMap, newActiveOverviewId, JSON.parse(JSON.stringify(newActiveOverview)));
      }
      this.activeOverviewId = newActiveOverviewId;
    },
    setDefaultOverview (id, iType) {
      this.$store.commit('SET_SELECTED_OVERVIEW_ID_FOR_ITYPE', { iType, id });
      UserService.setUserSettings({ selectedOverviews: this.getCorrectedSelectedOverviewIdMap });
    },
    updateModifiedOverview (newOverview) {
      Vue.set(this.modifiedOverviewMap, this.activeOverviewId, newOverview);
    },
    activeOverviewDeleted () {
      Vue.set(this.modifiedOverviewMap, this.activeOverviewId, undefined);
      this.setActiveOverviewToFirst();
    },
    openOverviewForm () {
      this.$bvModal.show('overview-form');
    },
    /* LINK GROUPS! -------------------------- */
    updateLinkGroup (linkGroup) {
      this.$set(this.updatedLinkGroupMap, linkGroup._id, linkGroup);
    },
    openLinkGroupForm () {
      this.$bvModal.show('link-group-form');
    },
    // re-fetch link groups when changing see-all for link groups
    seeAllLinkGroupsChanged () {
      LinkService.getLinkGroups(this.seeAllLinkGroups);
    },
    // re-fetch overviews when changing see-all for overviews
    seeAllOverviewsChanged () {
      OverviewService.getOverviews();
    },
    updateList ({ list, from, to }) {
      const ids = [];
      for (const group of list) {
        ids.push(group._id);
      }

      UserService.setUserSettings({ linkGroup: { order: ids } }).then((response) => {
        this.$store.commit('SET_LINK_GROUPS', list); // update list order
      }).catch((err) => {
        this.$store.commit('SET_LINK_GROUPS_ERROR', err);
      });

      // NOTE: need to toggle selectedLinkGroup so that the children that use it
      // (LinkGroupCard & LinkGroupForm) can update their data based on the value
      // For example: the selectedLinkGroup index doesn't change when the items
      // are reordered, but the data associated with that index does if the
      // selected link group is either the dragged item or the target item
      if (this.selectedLinkGroup === from || this.selectedLinkGroup === to) {
        const index = this.selectedLinkGroup;
        this.selectedLinkGroup = undefined;
        setTimeout(() => {
          this.selectedLinkGroup = index;
        }, 100);
      }
    },
    /* VIEWS! -------------------------------- */
    openViewForm () {
      this.$bvModal.show('view-form');
    },
    // re-fetch views when changing see-all for views
    seeAllViewsChanged () {
      UserService.getIntegrationViews();
    },
    setFilteredView (view) {
      const index = this.filteredViews.findIndex(v => v._id === view._id);
      if (index !== -1) {
        this.$set(this.filteredViews, index, view);
      }
    },
    normalizeView (unNormalizedView) {
      const view = JSON.parse(JSON.stringify(unNormalizedView));

      // sort these fields to make order not affect result of comparison, because their orders are not meaningful
      view.editRoles.sort();
      view.viewRoles.sort();
      view.integrations.sort();
      return view;
    },
    updateView (view) {
      // see if the view has unsaved changes
      const initialView = this.getViews.find(v => view._id === v._id);
      const hasChanged = JSON.stringify(this.normalizeView(initialView)) !== JSON.stringify(this.normalizeView(view));

      // if the view has changed from what is in the database, store the updated state, otherwise undefined
      this.updatedViewMap[view._id] = hasChanged ? view : undefined;

      // update the filteredViews array with this value
      this.setFilteredView(view);
    },
    cancelUpdateView (view) {
      const unchangedView = this.getViews.find((v) => v._id === view._id);
      this.updatedViewMap[view._id] = JSON.parse(JSON.stringify(unchangedView));
      this.updateView(unchangedView);
    },
    saveView (view) {
      delete view.error;
      delete view.success;
      // NOTE: this function handles fetching the updated view list and storing it
      UserService.updateIntegrationsView(view).then((response) => {
        this.$set(view, 'success', true);
      }).catch((error) => {
        this.$set(view, 'error', true);
      }).finally(() => {
        setTimeout(() => {
          delete view.error;
          delete view.success;
          this.updatedViewMap[view._id] = undefined;
          this.setFilteredView(view);
        }, 4000);
      });
    },
    toggleDeleteView (viewId) {
      this.$set(this.confirmDeleteView, viewId, !this.confirmDeleteView[viewId]);
    },
    deleteView (view) {
      // NOTE: this function handles fetching the updated view list and storing it
      UserService.deleteIntegrationsView(view._id).catch((error) => {
        this.$set(view, 'error', true);
        setTimeout(() => {
          delete view.error;
          this.setFilteredView(view);
        }, 4000);
      });
    },
    /* PASSWORD! ----------------------------- */
    changePassword () {
      this.msg = '';

      if (!this.currentPassword) {
        this.showMessage({
          variant: 'danger',
          message: 'You must enter your current password'
        });
        return;
      }

      if (!this.newPassword) {
        this.showMessage({
          variant: 'danger',
          message: 'You must enter a new password'
        });
        return;
      }

      if (!this.confirmNewPassword) {
        this.showMessage({
          variant: 'danger',
          message: 'You must confirm your new password'
        });
        return;
      }

      if (this.newPassword !== this.confirmNewPassword) {
        this.showMessage({
          variant: 'danger',
          message: "Your passwords don't match"
        });
        return;
      }

      const data = {
        newPassword: this.newPassword,
        currentPassword: this.currentPassword
      };

      CommonUserService.changePassword(data).then((response) => {
        this.newPassword = null;
        this.currentPassword = null;
        this.confirmNewPassword = null;
        // display success message to user
        this.showMessage({ variant: 'success', message: response.text || 'Updated password!' });
      }).catch((error) => {
        // display error message to user
        this.showMessage({ variant: 'danger', message: error.text || error });
      });
    },

    /* helpers ------------------------------------------------------------- */
    /* MISC! --------------------------------- */
    showMessage ({ variant, message }) {
      this.msg = message;
      this.msgType = variant;
      setTimeout(() => {
        this.msg = '';
        this.msgType = '';
      }, 10000);
    },
    /* INTEGRATIONS! ------------------------- */
    getIntegrationSettingValues () {
      const settings = {};
      for (const setting in this.integrationSettings) {
        let values = this.integrationSettings[setting].values;
        if (this.filteredIntegrationSettings[setting]) {
          values = this.filteredIntegrationSettings[setting].values;
        }
        settings[setting] = values;
      }
      return settings;
    },
    parseINI: function (data) {
      // This code is from node-iniparser, MIT license
      const regex = {
        section: /^\s*\[\s*([^\]]*)\s*\]\s*$/,
        param: /^\s*([\w.\-_]+)\s*=\s*(.*?)\s*$/,
        comment: /^\s*[;#].*$/
      };
      const json = {};
      const lines = data.split(/\r\n|\r|\n/);
      let section = null;
      lines.forEach(function (line) {
        if (regex.comment.test(line)) {
          return;
        } else if (regex.param.test(line)) {
          const match = line.match(regex.param);
          if (section) {
            json[section][match[1]] = match[2];
          } else {
            json[match[1]] = match[2];
          }
        } else if (regex.section.test(line)) {
          const match = line.match(regex.section);
          json[match[1]] = {};
          section = match[1];
        }
      });
      return json;
    },
    createINI: function (json) {
      let data = '';
      for (const section in json) {
        if (Object.keys(json[section]).length === 0) { continue; }
        data += `[${section}]\n`;
        for (const setting in json[section]) {
          data += `${setting}=${json[section][setting]}\n`;
        }
        data += '\n';
      }
      return data;
    },
    /* VIEWS! -------------------------------- */
    // NOTE: this filters/orders views while preserving keeping any updates made previously
    filterViews (searchTerm) {
      const viewsCopy = JSON.parse(JSON.stringify(this.getViews));
      viewsCopy.sort((a, b) => a.name.localeCompare(b.name)); // sort alphabetically
      // use updated versions of views
      const editedViews = viewsCopy.map((view) => this.updatedViewMap[view._id] ?? view);

      // no filter
      if (!searchTerm) {
        this.filteredViews = editedViews;
        return;
      }

      // filter by searchTerm
      const query = searchTerm.toLowerCase();
      this.filteredViews = editedViews.filter((view) => {
        return view.name.toString().toLowerCase().match(query)?.length > 0;
      });
    }
  }
};
</script>

<style scoped>
.nav-pills {
  max-width: 100%;
  position: relative;
}
.nav-pills .nav-link {
  max-width: 100%;
  overflow: hidden;
  position: relative;
  white-space: nowrap;
  text-overflow: ellipsis;
}

.sub-nav-link {
  padding-left: 32px !important;
}
.sub-nav-handle {
  top: 12px;
  left: 1.4rem;
  float: left;
  z-index: 10;
  position: relative;
}

.alert.alert-sm {
  padding: 0.2rem 0.8rem;
}

.integration-setting-img {
  height:27px;
  margin-left: -8px;
}

.itype-group-container {
  position: relative;
  max-width: calc(100% - 1rem);
  margin-left: 1rem;
  border-left-width: 2px;
  border-left-style: solid;
  border-radius: 5px;
  padding-left: 4px;
  margin-bottom: 4px
}
</style>
