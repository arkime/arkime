<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-row no-gutters class="d-flex flex-row flex-grow-1 overflow-hidden">
    <!-- navigation -->
    <v-col xl="2" lg="3" md="3" sm="4" xs="12"
      role="tablist"
      aria-orientation="vertical"
      class="h-100 overflow-auto no-overflow-x"
    >
      <div class="nav d-flex flex-column nav-pills pt-3 pb-4 px-4">
        <v-btn @click="openView('views')"
          block
          class="nav-link cursor-pointer btn-space-between"
          color="primary"
          variant="text"
          :active="visibleTab === 'views'">
          <span>
            <v-icon icon="mdi-eye" class="mr-1" />
            Views
          </span>
          <v-btn
            size="x-small"
            class="float-right"
            color="secondary"
            v-if="visibleTab === 'views'"
            @click.stop.prevent="openViewForm"
            v-tooltip="'Create a new view'">
            <v-icon icon="mdi-plus-circle" />
          </v-btn>
        </v-btn>
        <v-btn @click="openView('integrations')"
          block
          class="nav-link cursor-pointer justify-start"
          color="primary"
          variant="text"
          :active="visibleTab === 'integrations'">
          <v-icon icon="mdi-key" class="mr-1" />
          Integrations
        </v-btn>
        <v-btn @click="openView('overviews')"
          block
          class="nav-link cursor-pointer btn-space-between"
          :class="{ 'mb-1': visibleTab === 'overviews' }"
          color="primary"
          variant="text"
          :active="visibleTab === 'overviews'">
          <span>
            <v-icon icon="mdi-file" class="mr-1" />
            Overviews
          </span>
          <v-btn
              size="x-small"
              class="float-right"
              color="secondary"
              v-if="visibleTab === 'overviews'"
              @click.stop.prevent="openOverviewForm"
              v-tooltip="'Create a new overview'">
            <v-icon icon="mdi-plus-circle" />
          </v-btn>
        </v-btn>
        <template v-if="visibleTab === 'overviews'">
          <!-- overviews -->
          <div v-for="iType in iTypes" :key="iType"
            class="itype-group-container" :style="{ 'border-color': iTypeColorMap[iType] }"
          >
            <v-btn
                v-for="overview in getSortedOverviews.filter(o => o.iType === iType)"
                size="small"
                :key="overview._id"
                :title="overview.name"
                @click="setActiveOverviewId(overview._id)"
                block
                class="nav-link cursor-pointer btn-space-between"
                color="primary"
                variant="text"
                :active="activeOverviewId ===overview._id">
              <overview-selector-line :overview="overview" />
            </v-btn>
          </div>
        </template>
        <v-btn @click="openView('linkgroups')"
          block
          class="nav-link cursor-pointer btn-space-between"
          color="primary"
          variant="text"
          :active="visibleTab === 'linkgroups'">
          <span>
            <v-icon icon="mdi-link" class="mr-1" />
            Link Groups
          </span>
          <v-btn
            size="x-small"
            class="float-right"
            color="secondary"
            v-if="visibleTab === 'linkgroups'"
            @click.stop.prevent="openLinkGroupForm"
            v-tooltip="'Create a new link group'">
            <v-icon icon="mdi-plus-circle" />
          </v-btn>
        </v-btn>
        <template v-if="visibleTab === 'linkgroups'">
          <drag-update-list
            class="d-flex flex-column"
            style="margin-left: 1rem"
            :value="getLinkGroups"
            @update="updateList"
          >
            <v-btn
              v-for="(lg, i) in getLinkGroups"
              :key="lg._id"
              block
              size="small"
              variant="text"
              color="primary"
              class="justify-start mt-1"
              @click="selectedLinkGroup = i"
              :title="lg.name"
              :active="selectedLinkGroup === i"
            >
              <v-icon
                icon="mdi-menu"
                :id="`${lg._id}-tt`"
                class="drag-handle mr-2"
              />
              <id-tooltip :target="`${lg._id}-tt`">
                Drag &amp; drop to reorder Link Groups
              </id-tooltip>
              {{ lg.name }}
            </v-btn>
          </drag-update-list>
        </template>
        <v-btn v-if="!disablePassword"
          @click="openView('password')"
          block
          class="nav-link cursor-pointer justify-start"
          color="primary"
          variant="text"
          :active="visibleTab === 'password'">
          <v-icon icon="mdi-lock" class="mr-1" />
          Password
        </v-btn>
      </div>
    </v-col>

    <v-col xl="10" lg="9" md="9" sm="8" xs="12"
      class="overflow-auto h-100 pt-3 pb-4 pr-4"
    >
      <!-- view settings -->
      <div v-if="visibleTab === 'views'">
        <!-- view create form -->
        <create-view-modal v-model="viewModalOpen" />
        <div class="mr-3 w-100 d-flex justify-space-between align-center">
          <h1>
            Views
          </h1>
          <v-text-field
            class="ml-4 mr-2 flex-grow-1 medium-input"
            autofocus
            prepend-inner-icon="mdi-magnify"
            v-debounce="val => searchTerm = val"
            clearable
          />
          <v-btn
            class="no-wrap search-row-btn"
            @click="openViewForm"
            variant="outlined"
            color="success"
            >
            <v-icon icon="mdi-plus-circle" class="mr-1" />
            New View
          </v-btn>

          <v-btn
            role="checkbox"
            class="mx-2 no-wrap search-row-btn"
            color="secondary"
            flat
            @click="seeAllViews = !seeAllViews"
            @input="seeAllViewsChanged"
            v-tooltip="seeAllViews ? 'Just show the views created from your activity or shared with you' : 'See all the views that exist for all users (you can because you are an ADMIN!)'"
            v-if="roles.includes('cont3xtAdmin')"
            :title="seeAllViews ? 'Just show the views created from your activity or shared with you' : 'See all the views that exist for all users (you can because you are an ADMIN!)'"
          >
            <v-icon class="mr-1" icon="mdi-account-circle" />
            See {{ seeAllViews ? ' MY ' : ' ALL ' }} Views
          </v-btn>
        </div>
        <div class="d-flex flex-wrap">
          <!-- no views -->
          <div class="row lead mt-4"
            v-if="!viewSearchTerm && (!filteredViews.length || !filteredViews.filter(v => v._editable).length)">
            <div class="col">
              No Views are configured or shared for you to edit.
              <v-btn
                variant="text"
                color="primary"
                @click="openViewForm">
                Create one!
              </v-btn>
            </div>
          </div> <!-- /no views -->
          <!-- no view results -->
          <div class="row lead mt-4"
            v-else-if="viewSearchTerm && (!filteredViews.length || !filteredViews.filter(v => v._editable).length)">
            <div class="col">
              No Views match your search.
            </div>
          </div> <!-- /no view results -->
          <!-- views -->
          <div class="d-flex flex-row flex-wrap align-stretch justify-space-between">
            <template v-for="view in filteredViews">
              <div
                class="px-2 pb-4 flex-grow-1"
                :id="view._id"
                :key="`${view._id}`"
                v-if="view._editable || roles.includes('cont3xtAdmin')">
                <v-card variant="tonal" elevation="4">
                  <template #title>
                    <div class="w-100 d-flex justify-space-between align-start">
                      <div class="d-flex ga-1">
                        <v-btn
                          class="square-btn-sm"
                          size="small"
                          color="primary"
                          v-tooltip="'Transfer ownership of this view'"
                          title="Transfer ownership of this view"
                          v-if="canTransferView(view)"
                          @click="openTransferResource(view)">
                          <v-icon icon="mdi-share" />
                        </v-btn>
                        <!-- delete button -->
                        <transition name="buttons">
                          <v-btn
                            class="square-btn-sm"
                            size="small"
                            color="error"
                            v-if="!confirmDeleteView[view._id]"
                            v-tooltip:top="'Delete this view.'"
                            @click.stop.prevent="toggleDeleteView(view._id)">
                            <v-icon icon="mdi-trash-can" />
                          </v-btn>
                        </transition> <!-- /delete button -->
                        <!-- cancel confirm delete button -->
                        <transition name="buttons">
                          <v-btn
                            class="square-btn-sm"
                            size="small"
                            color="warning"
                            v-tooltip="'Cancel'"
                            title="Cancel"
                            v-if="confirmDeleteView[view._id]"
                            @click.stop.prevent="toggleDeleteView(view._id)">
                            <v-icon icon="mdi-cancel" />
                          </v-btn>
                        </transition> <!-- /cancel confirm delete button -->
                        <!-- confirm delete button -->
                        <transition name="buttons">
                          <v-btn
                            class="square-btn-sm"
                            size="small"
                            color="error"
                            v-tooltip="'Are you sure?'"
                            title="Are you sure?"
                            v-if="confirmDeleteView[view._id]"
                            @click.stop.prevent="deleteView(view)">
                            <v-icon icon="mdi-check-bold" />
                          </v-btn>
                        </transition> <!-- /confirm delete button -->
                      </div>
                      <v-alert
                        color="success"
                        height="32px"
                        v-if="view.success"
                        class="mb-0 mt-0 alert-sm mr-1 ml-1">
                        <v-icon icon="mdi-check-bold" class="mr-2" />
                        Saved!
                      </v-alert>
                      <v-alert
                        color="error"
                        height="32px"
                        v-if="view.error"
                        class="mb-0 mt-0 alert-sm mr-1 ml-1">
                        <v-icon icon="mdi-alert" class="mr-2" />
                        Error!
                      </v-alert>
                      <div class="d-flex ga-1">
                        <transition name="buttons">
                          <v-btn
                            v-if="updatedViewMap[view._id]"
                            class="square-btn-sm"
                            size="small"
                            color="warning"
                            @click="cancelUpdateView(view)"
                            v-tooltip="'Cancel changes to this view'">
                            <v-icon icon="mdi-cancel" />
                          </v-btn>
                        </transition>
                        <transition name="buttons">
                          <v-btn
                            v-if="updatedViewMap[view._id]"
                            class="square-btn-sm"
                            size="small"
                            color="success"
                            @click="saveView(view)"
                            v-tooltip="'Save this view'">
                            <v-icon icon="mdi-content-save" />
                          </v-btn>
                        </transition>
                      </div>
                    </div>
                  </template>
                  <ViewForm
                    class="ma-4"
                    :view="view"
                    @update-view="updateView"
                  />
                </v-card>
              </div>
            </template> <!-- /views -->
          </div>
        </div>
      </div> <!-- /view settings -->

      <!-- integrations settings -->
      <div v-if="visibleTab === 'integrations'">
        <div class="ml-2 mr-3 w-100 d-flex justify-space-between align-center">
          <h1>
            Integrations
          </h1>
          <v-text-field
            autofocus
            class="ml-4 mr-2 medium-input"
            prepend-inner-icon="mdi-magnify"
            variant="outlined"
            v-debounce="updateIntegrationSearchTerm"
            placeholder="Search integrations"
            clearable
          />
          <div class="mr-3 no-wrap">
            <v-btn
              class="mr-1"
              variant="outlined"
              color="warning"
              @click="toggleRawIntegrationSettings">
              <v-icon icon="mdi-pencil-box" class="mr-2" />
              Raw Edit
            </v-btn>
            <v-btn
              variant="outlined"
              color="success"
              @click="saveIntegrationSettings">
              <v-icon icon="mdi-content-save" class="mr-2" />
              Save
            </v-btn>
          </div>
        </div>
        <div class="d-flex flex-wrap" :class="{ 'flex-column': rawIntegrationSettings }">
          <template v-if="!rawIntegrationSettings">
            <div class="row lead mt-4"
              v-if="Object.keys(sortedFilteredIntegrationSettings).length === 0">
              <div class="col">
                No Integrations match your search.
              </div>
            </div>
            <div
              :key="key"
              class="px-2 pb-4 flex-grow-1"
              v-for="([key, setting]) in sortedFilteredIntegrationSettings">
              <v-card variant="tonal">
                <v-card-title class="align-center d-flex flex-row justify-space-between bg-well mb-2">
                  <img
                    v-if="getIntegrations[key]"
                    class="integration-setting-img"
                    :src="getIntegrations[key].icon"
                  />
                  <h4 class="ml-1 text-truncate">{{ key }}</h4>
                  <div class="mb-2">
                    <v-icon
                      size="large"
                      icon="mdi-lock"
                      v-if="setting.locked"
                      class="mr-2 cursor-help"
                      v-tooltip="'This integration has been locked by your administrator. You cannot update this integration. Any previously configured settings for this integration will be ignored in favor of the global configuration.'"
                    />
                    <span
                      size="large"
                      icon="mdi-earth"
                      class="mr-2 cursor-help"
                      v-if="setting.globalConfiged"
                      v-tooltip="'This integration has been globally configured by the administrator with a shared account. If you fill out the account fields below, it will override that configuration.'"
                    />
                    <a target="_blank"
                      :href="setting.homePage"
                      v-if="!!setting.homePage"
                      v-tooltip="`${key} home page`">
                      <v-icon icon="mdi-home" size="large" />
                    </a>
                  </div>
                </v-card-title>
                <div class="d-flex flex-column ga-2 mb-2">
                  <template v-for="(field, name) in setting.settings"
                      :key="name"
                    >
                    <v-checkbox
                      slim
                      density="compact"
                      class="ml-1"
                      v-if="field.type === 'boolean'"
                      v-model="setting.values[name]">
                      <template #label>
                        <span class="ma-0">{{ name }}</span>
                      </template>
                    </v-checkbox>
                    <v-text-field
                      v-else
                      class="ml-2 mr-2"
                      variant="outlined"
                      :disabled="setting.locked"
                      v-model="setting.values[name]"
                      :rules="[(value) => !field.required || !!value?.length]"
                      :type="field.password && !field.showValue ? 'password' : 'text'"
                    >
                    <template #label>
                      {{ name }}<span
                        class="text-info" v-if="field.required">*</span>
                    </template>
                    </v-text-field>
                  </template>
                </div>
              </v-card>
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
        <!-- overview create form -->
        <create-overview-modal v-model="overviewModalOpen" />
        <div class="ml-2 mr-3 w-100 d-flex flex-row justify-space-between align-center">
          <h1>
            Overviews
          </h1>
          <div class="d-flex flex-row">
            <v-btn
                variant="outlined"
                color="primary"
                @click="openOverviewForm"
            >
              <v-icon icon="mdi-plus-circle" />
              New Overview
            </v-btn>
            <v-btn
              role="checkbox"
              class="mx-2 no-wrap"
              color="secondary"
              flat
              @click="seeAllOverviews = !seeAllOverviews"
              @input="seeAllOverviewsChanged"
              v-tooltip="seeAllOverviews ? 'Just show the overviews created from your activity or shared with you' : 'See all the overviews that exist for all users (you can because you are an ADMIN!)'"
              v-if="roles.includes('cont3xtAdmin')"
              :title="seeAllOverviews ? 'Just show the overviews created from your activity or shared with you' : 'See all the overviews that exist for all users (you can because you are an ADMIN!)'"
            >
              <v-icon class="mr-1" icon="mdi-account-circle" />
              See {{ seeAllOverviews ? ' MY ' : ' ALL ' }} Overviews
            </v-btn>
          </div>
        </div>

        <!-- overview error -->
        <v-alert
            closable
            color="error"
            style="z-index: 2000;"
            v-model="overviewsError"
            class="position-fixed bottom-0 mb-2 ml-2 left-0">
          {{ getOverviewsError }}
        </v-alert> <!-- /overview error -->

        <div class="d-flex flex-wrap pl-4">
          <!-- overview-form-card uses :key to reset form when swapping active overview -->
          <overview-form-card
              v-if="activeOverviewId && activeUnModifiedOverview"
              :key="activeOverviewId"
              :overview="activeUnModifiedOverview"
              :modifiedOverview="activeModifiedOverview"
              @update-modified-overview="updateModifiedOverview"
              @overview-deleted="activeOverviewDeleted"
              @open-transfer-resource="openTransferResource"
          />
          <div v-else
               class="d-flex flex-column">
            <span>
              No Overviews configured.
            </span>
            <v-btn
                variant="outlined"
                color="primary"
                @click="openOverviewForm">
              Create one!
            </v-btn>
          </div>
        </div>
      </div> <!-- /overviews settings -->

      <!-- link group settings -->
      <div v-if="visibleTab === 'linkgroups'">
        <!-- link group create form -->
        <create-link-group-modal v-model="linkgroupModalOpen" />
        <!-- link groups -->
        <div class="ml-2 mr-3 w-100 d-flex flex-row justify-space-between align-center">
          <h1>
            Link Groups
          </h1>
          <span class="d-flex flex-row">
            <v-btn
                class="search-row-btn"
                variant="outlined"
                color="primary"
                @click="openLinkGroupForm">
              <v-icon icon="mdi-plus-circle" class="mr-1" />
              New Group
            </v-btn>
            <v-btn
              role="checkbox"
              class="mx-2 no-wrap search-row-btn"
              color="secondary"
              flat
              @click="seeAllLinkGroups = !seeAllLinkGroups"
              @input="seeAllLinkGroupsChanged"
              v-tooltip="seeAllLinkGroups ? 'Just show the link groups created from your activity or shared with you' : 'See all the link groups that exist for all users (you can because you are an ADMIN!)'"
              v-if="roles.includes('cont3xtAdmin')"
              :title="seeAllLinkGroups ? 'Just show the link groups created from your activity or shared with you' : 'See all the link groups that exist for all users (you can because you are an ADMIN!)'"
            >
                <v-icon class="mr-1" icon="mdi-account-circle" />
                See {{ seeAllLinkGroups ? ' MY ' : ' ALL ' }} Groups
            </v-btn>
          </span>
        </div>

        <!-- link group error -->
        <v-alert
          closable
          color="error"
          style="z-index: 2000;"
          v-model="linkGroupsError"
          class="position-fixed bottom-0 mb-2 ml-2 left-0">
          {{ getLinkGroupsError }}
        </v-alert> <!-- /link group error -->

        <!-- link groups -->
        <link-group-card
          v-if="getLinkGroups && getLinkGroups.length && getLinkGroups[selectedLinkGroup]"
          :link-group="getLinkGroups[selectedLinkGroup]"
          :key="getLinkGroups[selectedLinkGroup]._id"
          :pre-updated-link-group="updatedLinkGroupMap[getLinkGroups[selectedLinkGroup]._id]"
          @update-link-group="updateLinkGroup"
          @open-transfer-resource="openTransferResource"
        /> <!-- /link groups -->
        <!-- no link groups -->
        <div
          class="row lead mt-4"
          v-if="getLinkGroups && !getLinkGroups.length">
          <div class="col">
            No Link Groups are configured.
            <v-btn
              variant="text"
              color="primary"
              @click="openLinkGroupForm">
              Create one!
            </v-btn>
          </div>
        </div> <!-- /no link groups -->
      </div> <!-- /link group settings -->

      <!-- password settings -->
      <div v-if="visibleTab === 'password' && !disablePassword">
        <h1>
          Change Password
        </h1>

        <v-form>
          <v-row no-gutters>
            <v-col cols="9" class="mt-4">
              <!-- current password -->
              <v-text-field
                type="password"
                v-model="currentPassword"
                @keydown.enter="changePassword"
                label="Current Password"
                placeholder="Enter your current password"
              />
              <!-- new password -->
              <v-text-field
                class="mt-2"
                type="password"
                v-model="newPassword"
                @keydown.enter="changePassword"
                label="New Password"
                placeholder="Enter a new password"
              />
              <!-- confirm new password -->
              <v-text-field
                class="mt-2"
                type="password"
                v-model="confirmNewPassword"
                @keydown.enter="changePassword"
                label="Confirm New Password"
                placeholder="Confirm your new password"
              />
              <!-- change password button -->
              <v-btn type="button"
                color="success"
                class="mt-2"
                @click="changePassword">
                Change Password
              </v-btn>
            </v-col>
          </v-row>
        </v-form>
      </div> <!-- /password settings -->
    </v-col>
    <!-- messages -->
    <v-alert
      v-if="!!msg"
      class="position-fixed bottom-0 mb-2 ml-2"
      style="z-index: 2000;"
      :color="msgType"
      dismissible>
      {{ msg }}
    </v-alert> <!-- messages -->

    <transfer-resource
      v-model="transferResourceModalOpen"
      @transfer-resource="submitTransfer"
    />
  </v-row>
</template>

<script>
import { mapGetters } from 'vuex';

import IdTooltip from '@/utils/IdTooltip.vue';
import ViewForm from '@/components/views/ViewForm.vue';
import UserService from '@/components/services/UserService';
import LinkGroupCard from '@/components/links/LinkGroupCard.vue';
import CreateViewModal from '@/components/views/CreateViewModal.vue';
import Cont3xtService from '@/components/services/Cont3xtService';
import CreateLinkGroupModal from '@/components/links/CreateLinkGroupModal.vue';
import LinkService from '@/components/services/LinkService';
import OverviewService from '@/components/services/OverviewService';
import OverviewFormCard from '@/components/overviews/OverviewFormCard.vue';
import CreateOverviewModal from '@/components/overviews/CreateOverviewModal.vue';
import OverviewSelectorLine from '@/components/overviews/OverviewSelectorLine.vue';
import { iTypes, iTypeIconMap, iTypeColorMap } from '@/utils/iTypes';
import CommonUserService from '@real_common/UserService';
import TransferResource from '@common/TransferResource.vue';
import DragUpdateList from '@/utils/DragUpdateList.vue';

let timeout;

export default {
  name: 'Cont3xtSettings',
  components: {
    OverviewSelectorLine,
    CreateOverviewModal,
    OverviewFormCard,
    ViewForm,
    IdTooltip,
    LinkGroupCard,
    CreateViewModal,
    CreateLinkGroupModal,
    TransferResource,
    DragUpdateList
  },
  data () {
    return {
      // page vars
      msg: '',
      msgType: '',
      visibleTab: 'views',
      // integrations
      integrationSettings: {},
      filteredIntegrationSettings: {},
      rawIntegrationSettings: undefined,
      // overviews
      overviewModalOpen: false,
      iTypes,
      iTypeIconMap,
      iTypeColorMap,
      activeOverviewId: undefined,
      modifiedOverviewMap: {},
      // link groups
      linkgroupModalOpen: false,
      selectedLinkGroup: 0,
      updatedLinkGroupMap: {},
      // views
      viewModalOpen: false,
      filteredViews: undefined,
      viewSearchTerm: '',
      viewForm: false,
      updatedViewMap: {},
      confirmDeleteView: {},
      // password
      currentPassword: '',
      newPassword: '',
      confirmNewPassword: '',
      // transfers
      transferResourceModalOpen: false,
      transferResource: undefined
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
      this.showError(err);
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
      return !!this.$constants.DEMO_MODE || (!!this.$constants.DISABLE_USER_PASSWORD_UI && !!this.getUser.headerAuthEnabled);
    }
  },
  watch: {
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
        hash: `#${tabName}`
      });
    },
    /* TRANSFERS! ----------------------------- */
    openTransferResource (resource) {
      this.transferResource = resource;
      this.transferResourceModalOpen = true;
    },
    /**
     * Submits the transfer resource modal contents and updates the resources
     * @param {Object} userId The user id to transfer the resource to
     */
    submitTransfer ({ userId }) {
      if (!userId) {
        this.transferResource = undefined;
        return;
      }

      const data = JSON.parse(JSON.stringify(this.transferResource));
      data.creator = userId;

      if (data.links) { // if it's a link group
        LinkService.updateLinkGroup(data).then((response) => {
          this.transferResourceModalOpen = false;
          LinkService.getLinkGroups(this.seeAllLinkGroups);
          this.showMessage({ variant: 'success', message: response.text });
        }); // store deals with failure
      } else if (data.integrations) { // it's a view
        UserService.updateIntegrationsView(data).then((response) => {
          this.transferResourceModalOpen = false;
          UserService.getIntegrationViews(this.seeAllViews);
          this.showMessage({ variant: 'success', message: response.text });
        }).catch((err) => {
          this.showError(err);
        });
      } else if (data.fields) {
        OverviewService.updateOverview(data).then((response) => {
          this.transferResourceModalOpen = false;
          OverviewService.getOverviews();
          this.showMessage({ variant: 'success', message: response.text });
        }); // store deals with failure
      } else {
        this.showError('Cannot parse the resource you want to transfer');
      }
    },
    /* INTEGRATIONS! ------------------------- */
    /* toggles the visibility of the value of password fields */
    toggleVisiblePasswordField (field) {
      field.showValue = !field.showValue;
    },
    saveIntegrationSettings () {
      const settings = this.getIntegrationSettingValues();

      UserService.setIntegrationSettings({ settings }).then((response) => {
        this.showMessage({ variant: 'success', message: 'Saved!' });
        // NOTE: don't need to do anything with the data (the store does it)
        Cont3xtService.getIntegrations();
      }).catch((err) => {
        this.showError(err);
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
          this.integrationSettings[s].values = rawIntegrationSettings[s];
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
        this.modifiedOverviewMap[newActiveOverviewId] = JSON.parse(JSON.stringify(newActiveOverview));
      }
      this.activeOverviewId = newActiveOverviewId;
    },
    setDefaultOverview (id, iType) {
      this.$store.commit('SET_SELECTED_OVERVIEW_ID_FOR_ITYPE', { iType, id });
      UserService.setUserSettings({ selectedOverviews: this.getCorrectedSelectedOverviewIdMap });
    },
    updateModifiedOverview (newOverview) {
      this.modifiedOverviewMap[this.activeOverviewId] = newOverview;
    },
    activeOverviewDeleted () {
      this.modifiedOverviewMap[this.activeOverviewId] = undefined;
      this.setActiveOverviewToFirst();
    },
    openOverviewForm () {
      this.overviewModalOpen = true;
    },
    /* LINK GROUPS! -------------------------- */
    updateLinkGroup (linkGroup) {
      this.updatedLinkGroupMap[linkGroup._id] = linkGroup;
    },
    openLinkGroupForm () {
      this.linkgroupModalOpen = true;
    },
    // re-fetch link groups when changing see-all for link groups
    seeAllLinkGroupsChanged () {
      LinkService.getLinkGroups(this.seeAllLinkGroups);
    },
    // re-fetch overviews when changing see-all for overviews
    seeAllOverviewsChanged () {
      OverviewService.getOverviews();
    },
    updateList ({ newList, oldList }) {
      // const list = this.getLinkGroups;

      const ids = [];
      for (const group of newList) {
        ids.push(group._id);
      }

      this.$store.commit('SET_LINK_GROUPS', newList); // optimistic update, to avoid stutter
      UserService.setUserSettings({ linkGroup: { order: ids } }).then((response) => {
        // nothing to do, since we've already updated the list
      }).catch((err) => {
        this.$store.commit('SET_LINK_GROUPS_ERROR', err);
        this.$store.commit('SET_LINK_GROUPS', oldList); // roll-back list
      });

      // NOTE: need to toggle selectedLinkGroup so that the children that use it
      // (LinkGroupCard & LinkGroupForm) can update their data based on the value
      // For example: the selectedLinkGroup index doesn't change when the items
      // are reordered, but the data associated with that index does if the
      // selected link group is either the dragged item or the target item
      const oldSelectedId = oldList[this.selectedLinkGroup]?._id;
      const newSelectedIndex = newList.findIndex(elem => elem._id === oldSelectedId);

      if (newSelectedIndex !== this.selectedLinkGroup) {
        this.selectedLinkGroup = undefined;
        setTimeout(() => {
          this.selectedLinkGroup = newSelectedIndex;
        }, 100);
      }
    },
    /* VIEWS! -------------------------------- */
    canTransferView (view) {
      return this.getUser?.roles.includes('cont3xtAdmin') ||
        (view?.creator && view?.creator === this.getUser?.userId);
    },
    openViewForm () {
      this.viewModalOpen = true;
    },
    // re-fetch views when changing see-all for views
    seeAllViewsChanged () {
      UserService.getIntegrationViews();
    },
    setFilteredView (view) {
      const index = this.filteredViews.findIndex(v => v._id === view._id);
      if (index !== -1) {
        this.filteredViews[index] = view;
      }
    },
    normalizeView (unNormalizedView) {
      const view = JSON.parse(JSON.stringify(unNormalizedView));

      // sort these fields to make order not affect result of comparison, because their orders are not meaningful
      view.editRoles?.sort();
      view.viewRoles?.sort();
      view.integrations?.sort();
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
        view.success = true;
      }).catch((error) => {
        view.error = true;
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
      this.confirmDeleteView[viewId] = !this.confirmDeleteView[viewId];
    },
    deleteView (view) {
      // NOTE: this function handles fetching the updated view list and storing it
      UserService.deleteIntegrationsView(view._id).catch((error) => {
        view.error = true;
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
        this.showError('You must enter your current password');
        return;
      }

      if (!this.newPassword) {
        this.showError('You must enter a new password');
        return;
      }

      if (!this.confirmNewPassword) {
        this.showError('You must confirm your new password');
        return;
      }

      if (this.newPassword !== this.confirmNewPassword) {
        this.showError("Your passwords don't match");
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
        this.showError(error.text || error);
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
    showSuccess (message) {
      this.showMessage({ variant: 'success', message });
    },
    showError (message) {
      this.showMessage({ variant: 'error', message });
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
    updateIntegrationSearchTerm (searchTerm) {
      if (!searchTerm) {
        this.filteredIntegrationSettings = JSON.parse(JSON.stringify(this.integrationSettings));
        return;
      }

      const query = searchTerm.toLowerCase();

      for (const key in this.integrationSettings) {
        if (key.toString().toLowerCase().match(query)?.length > 0) {
          this.filteredIntegrationSettings[key] = JSON.parse(JSON.stringify(this.integrationSettings[key]));
          continue;
        }
        delete this.filteredIntegrationSettings[key];
      }
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
