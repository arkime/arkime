<template>
  <div class="container-fluid">
    <!-- link group create form -->
    <create-link-group
      @update-link-groups="getLinkGroups"
    />
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
    <div class="d-flex flex-wrap">
      <div
        :key="linkGroup._id"
        class="w-25 p-2"
        v-for="(linkGroup, index) in linkGroups">
        <b-card class="h-100 align-self-stretch">
          <template #header>
            <h6 class="mb-0">
              {{ linkGroup.name }}
            </h6>
          </template>
          <b-card-body>
            <div
              :key="link.url + i"
              v-for="(link, i) in linkGroup.links">
              <hr v-if="i > 0" class="hr-small">
              <div>
                <strong class="text-info">Name</strong>
                {{ link.name }}
              <div>
              </div>
                <strong class="text-info">URL</strong>
                {{ link.url }}
              </div>
              <div>
                <strong class="text-info">Types</strong>
                {{ link.itypes.join(', ') }}
              </div>
            </div>
          </b-card-body>
          <template #footer>
            <div class="w-100 d-flex justify-content-between align-items-start">
              <b-button
                size="sm"
                variant="danger"
                v-b-tooltip.hover="'Delete this link group'"
                @click="deleteLinkGroup(linkGroup._id, index)">
                <span class="fa fa-trash" />
              </b-button>
              <b-button
                size="sm"
                class="disabled"
                variant="warning"
                @click="updateLinkGroup(linkGroup)"
                v-b-tooltip.hover="'Edit this link group'">
                <span class="fa fa-pencil" />
              </b-button>
            </div>
          </template>
        </b-card>
      </div>
    </div> <!-- /link groups -->
    <!-- no link groups -->
    <div
      class="row lead mt-4"
      v-if="!linkGroups.length">
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
import LinkService from '@/components/services/LinkService';
import CreateLinkGroup from '@/components/links/CreateLinkGroup';

export default {
  name: 'Cont3xtSettings',
  components: { CreateLinkGroup },
  data () {
    return {
      err: '',
      linkGroups: []
    };
  },
  mounted () {
    this.getLinkGroups();
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    deleteLinkGroup (id, index) {
      LinkService.deleteLinkGroup(id).then((response) => {
        this.linkGroups.splice(index, 1);
      }).catch((err) => {
        this.err = err;
      });
    },
    updateLinkGroup (linkGroup) {
      // TODO
      console.log('coming soon!');
    },
    /* helpers ------------------------------------------------------------- */
    getLinkGroups () {
      LinkService.getLinkGroups().then((response) => {
        this.linkGroups = response;
      }).catch((err) => {
        this.err = err;
      });
    }
  }
};
</script>
