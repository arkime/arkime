<template>
  <div class="container-fluid">
    <!-- link group create form -->
    <create-link-group />
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
      variant="danger"
      :show="!!getLinkGroupsError.length">
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
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import LinkGroupCards from '@/components/links/LinkGroupCards';
import CreateLinkGroup from '@/components/links/CreateLinkGroup';

export default {
  name: 'Cont3xtSettings',
  components: {
    LinkGroupCards,
    CreateLinkGroup
  },
  computed: {
    ...mapGetters(['getLinkGroups', 'getLinkGroupsError'])
  }
};
</script>
