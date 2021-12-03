<template>
  <div class="container-fluid mb-4 row">

    <!-- navigation -->
    <div
      role="tablist"
      aria-orientation="vertical"
      class="col-xl-2 col-lg-3 col-md-3 col-sm-4 col-xs-12">
      <div class="nav flex-column nav-pills">
        <a @click="openView('general')"
          class="nav-link cursor-pointer"
          :class="{'active':visibleTab === 'general'}">
          <span class="fa fa-fw fa-cog">
          </span>&nbsp;
          General
        </a>
        <a @click="openView('keys')"
          class="nav-link cursor-pointer"
          :class="{'active':visibleTab === 'keys'}">
          <span class="fa fa-fw fa-key">
          </span>&nbsp;
          Keys
        </a>
        <a @click="openView('linkgroups')"
          class="nav-link cursor-pointer"
          :class="{'active':visibleTab === 'linkgroups'}">
          <span class="fa fa-fw fa-link">
          </span>&nbsp;
          Link Groups
        </a>
      </div>
    </div> <!-- /navigation -->

    <div class="col-xl-10 col-lg-9 col-md-9 col-sm-8 col-xs-12 settings-right-panel">
      <!-- general settings -->
      <div v-if="visibleTab === 'general'">
        <div class="text-center">
          <b-card>
            <h1>Coming soon!</h1>
          </b-card>
        </div>
      </div> <!-- /general settings -->

      <!-- keys settings -->
      <div v-if="visibleTab === 'keys'">
        <h1 class="mr-2 ml-2 w-100 d-flex justify-content-between align-items-start">
          Link Groups
          <b-alert
            variant="success"
            :show="!!saveIntegrationSettingsSuccess.length">
            <span class="fa fa-check mr-2" />
            {{ saveIntegrationSettingsSuccess }}
          </b-alert>
          <b-button
            class="mt-2"
            variant="outline-success"
            @click="saveIntegrationSettings">
            <span class="fa fa-save mr-2" />
            Save
          </b-button>
        </h1>
        <div class="d-flex flex-wrap">
          <!-- integration settings error -->
          <b-alert
            dismissible
            variant="danger"
            style="z-index: 2000;"
            :show="!!integrationSettingsErr"
            class="position-fixed fixed-bottom m-0 rounded-0">
            {{ integrationSettingsErr }}
          </b-alert> <!-- /integration settings error -->
          <div
            :key="key"
            class="w-25 p-2"
            v-for="(setting, key) in integrationSettings">
            <b-card>
              <b-input-group
                size="sm"
                :key="name"
                class="mb-1 mt-1"
                v-for="(field, name) in setting.settings">
                <b-input-group-prepend
                  class="cursor-help"
                  v-b-tooltip.hover="field.help">
                  <b-input-group-text>
                    {{ name }}
                  </b-input-group-text>
                </b-input-group-prepend>
                <b-form-input
                  v-model="setting.values[name]"
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
            </b-card>
          </div>
        </div>
      </div> <!-- /keys settings -->

      <!-- link group settings -->
      <div v-if="visibleTab === 'linkgroups'">
        <!-- link group create form -->
        <create-link-group-modal />
        <!-- link groups -->
        <h1>
          Link Groups
          <b-button
            class="pull-right"
            variant="outline-primary"
            v-b-modal.link-group-form>
            <span class="fa fa-plus-circle" />
            New Group
          </b-button>
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
        <link-group-cards /> <!-- /link groups -->
        <!-- no link groups -->
        <div
          class="row lead mt-4"
          v-if="!getLinkGroups.length">
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
    </div>

  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import UserService from '@/components/services/UserService';
import LinkGroupCards from '@/components/links/LinkGroupCards';
import CreateLinkGroupModal from '@/components/links/CreateLinkGroupModal';

export default {
  name: 'Cont3xtSettings',
  components: {
    LinkGroupCards,
    CreateLinkGroupModal
  },
  data () {
    return {
      visibleTab: 'linkgroups',
      integrationSettings: {},
      integrationSettingsErr: '',
      saveIntegrationSettingsSuccess: ''
    };
  },
  created () {
    let tab = window.location.hash;
    if (tab) { // if there is a tab specified and it's a valid tab
      tab = tab.replace(/^#/, '');
      if (tab === 'general' || tab === 'keys' || tab === 'linkgroups') {
        this.visibleTab = tab;
      }
    }

    UserService.getIntegrationSettings().then((response) => {
      this.integrationSettings = response;
    }).catch((err) => {
      this.integrationSettingsErr = err;
    });
  },
  computed: {
    ...mapGetters(['getLinkGroups', 'getLinkGroupsError']),
    linkGroupsError: {
      get () {
        return !!this.$store.state.linkGroupsError;
      },
      set (value) {
        this.$store.commit('SET_LINK_GROUPS_ERROR', '');
      }
    }
  },
  methods: {
    /* opens a specific settings tab */
    openView (tabName) {
      this.visibleTab = tabName;
      this.$router.push({
        hash: tabName
      });
    },
    /* toggles the visibility of the value of password fields */
    toggleVisiblePasswordField (field) {
      this.$set(field, 'showValue', !field.showValue);
    },
    saveIntegrationSettings () {
      const settings = {};
      for (const setting in this.integrationSettings) {
        settings[setting] = this.integrationSettings[setting].values;
      }

      UserService.setIntegrationSettings({ settings }).then((response) => {
        // TODO ECR success
        this.saveIntegrationSettingsSuccess = 'Saved!';
      }).catch((err) => {
        this.integrationSettingsErr = err;
      });
    }
  }
};
</script>
