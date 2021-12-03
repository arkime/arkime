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
        <div class="text-center">
          <b-card>
            <h1>Coming soon!</h1>
          </b-card>
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
      visibleTab: 'linkgroups'
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
    }
  }
};
</script>
