<template>
  <div class="container-fluid mb-4 row">

    <!-- navigation -->
    <div
      role="tablist"
      aria-orientation="vertical"
      class="col-xl-2 col-lg-3 col-md-3 col-sm-4 col-xs-12 no-overflow">
      <div class="nav flex-column nav-pills">
        <a @click="openView('integrations')"
          class="nav-link cursor-pointer"
          :class="{'active':visibleTab === 'integrations'}">
          <span class="fa fa-fw fa-key">
          </span>&nbsp;
          Integrations
        </a>
        <a @click="openView('linkgroups')"
          class="nav-link cursor-pointer mb-1"
          :class="{'active':visibleTab === 'linkgroups'}">
          <span class="fa fa-fw fa-link">
          </span>&nbsp;
          Link Groups
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
              <span class="fa fa-bars d-inline sub-nav-handle" />
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
      </div>
    </div> <!-- /navigation -->

    <div class="col-xl-10 col-lg-9 col-md-9 col-sm-8 col-xs-12 settings-right-panel">
      <!-- integrations settings -->
      <div v-if="visibleTab === 'integrations'">
        <div class="ml-2 mr-3 w-100 d-flex justify-content-between align-items-center">
          <h1>
            Integrations
          </h1>
          <b-alert
            variant="success"
            class="alert-sm mt-1"
            :show="!!saveIntegrationSettingsSuccess.length">
            <span class="fa fa-check mr-2" />
            {{ saveIntegrationSettingsSuccess }}
          </b-alert>
          <div class="mr-3">
            <b-button
              class="mr-2"
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
          <!-- integration settings error -->
          <b-alert
            dismissible
            variant="danger"
            style="z-index: 2000;"
            :show="!!integrationSettingsErr"
            class="position-fixed fixed-bottom m-0 rounded-0">
            {{ integrationSettingsErr }}
          </b-alert> <!-- /integration settings error -->
          <template v-if="!rawIntegrationSettings">
            <div
              :key="key"
              class="w-25 p-2"
              v-for="(setting, key) in integrationSettings">
              <b-card>
                <template #header>
                  <h4 class="mb-0 d-inline">
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
        <link-group-card
          v-if="getLinkGroups && getLinkGroups.length"
          :link-group-index="selectedLinkGroup"
          @delete-link-group="deleteLinkGroup"
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
    </div>

  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import ReorderList from '@/utils/ReorderList';
import UserService from '@/components/services/UserService';
import LinkGroupCard from '@/components/links/LinkGroupCard';
import Cont3xtService from '@/components/services/Cont3xtService';
import CreateLinkGroupModal from '@/components/links/CreateLinkGroupModal';

let timeout;

export default {
  name: 'Cont3xtSettings',
  components: {
    ReorderList,
    LinkGroupCard,
    CreateLinkGroupModal
  },
  data () {
    return {
      visibleTab: 'integrations',
      integrationSettings: {},
      integrationSettingsErr: '',
      saveIntegrationSettingsSuccess: '',
      rawIntegrationSettings: undefined,
      selectedLinkGroup: 0
    };
  },
  created () {
    let tab = window.location.hash;
    if (tab) { // if there is a tab specified and it's a valid tab
      tab = tab.replace(/^#/, '');
      if (tab === 'integrations' || tab === 'linkgroups') {
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
    /* page functions ------------------------------------------------------ */
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
      const settings = this.getIntegrationSettingValues();

      UserService.setIntegrationSettings({ settings }).then((response) => {
        this.saveIntegrationSettingsSuccess = 'Saved!';
        // NOTE: don't need to do anything with the data (the store does it)
        Cont3xtService.getIntegrations();
      }).catch((err) => {
        this.integrationSettingsErr = err;
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
    },
    getState (field, setting, sname) {
      if (!field.required) {
        return undefined;
      }

      return setting.values[sname] ? setting.values[sname].length > 0 : false;
    },
    // NOTE: need to toggle selectedLinkGroup so that the children that use it
    // (LinkGroupCard & LinkGroupForm) can update their data based on the value
    // For example: the selectedLinkGroup index doesn't change when an item in
    // the list is deleted, but the data associated with that index does
    deleteLinkGroup ({ index }) {
      this.selectedLinkGroup = undefined;
      setTimeout(() => {
        if (index >= this.getLinkGroups.length - 1) {
          this.selectedLinkGroup = Math.max(this.getLinkGroups.length - 1, 0);
        } else {
          this.selectedLinkGroup = index;
        }
      }, 100);
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
    /* helpers ------------------------------------------------------------- */
    getIntegrationSettingValues () {
      const settings = {};
      for (const setting in this.integrationSettings) {
        settings[setting] = this.integrationSettings[setting].values;
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
        };
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
  padding: 0.4rem 0.8rem;
}
</style>
