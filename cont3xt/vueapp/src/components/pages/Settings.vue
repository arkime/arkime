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
        <h1 class="ml-2 w-100 d-flex justify-content-between align-items-start">
          Keys
          <b-alert
            variant="success"
            :show="!!saveIntegrationSettingsSuccess.length">
            <span class="fa fa-check mr-2" />
            {{ saveIntegrationSettingsSuccess }}
          </b-alert>
          <div class="mt-2 mr-3">
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
                      v-b-tooltip.hover="'This setting is globally configured'"
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

let timeout;

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
      saveIntegrationSettingsSuccess: '',
      rawIntegrationSettings: undefined
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
        return false;
      }

      return setting.values[sname] ? setting.values[sname].length > 0 : false;
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
