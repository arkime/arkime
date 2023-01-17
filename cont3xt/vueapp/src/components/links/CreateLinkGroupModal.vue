<template>
  <b-modal
    size="xl"
    scrollable
    id="link-group-form">
    <!-- header -->
    <template #modal-title>
      <h4 class="mb-0">
        Create New Link Group
      </h4>
    </template> <!-- /header -->
    <!-- form -->
    <link-group-form
      :raw-edit-mode="rawEditMode"
      @update-link-group="update"
    />
    <!-- footer -->
    <template #modal-footer>
      <div class="w-100 d-flex justify-content-between align-items-start">
        <b-button
          @click="close"
          variant="warning">
          Cancel
        </b-button>
        <b-alert
          variant="danger"
          :show="!!error.length"
          class="mb-0 alert-sm mr-1 ml-1">
          {{ error }}
        </b-alert>
        <b-button
          variant="warning"
          @click="rawEditMode = !rawEditMode"
          v-b-tooltip.hover="'Edit the raw config for this link group'">
          <span class="fa fa-pencil-square-o" />
        </b-button>
        <b-button
          @click="create"
          variant="success">
          Create
        </b-button>
      </div>
    </template> <!-- /footer -->
  </b-modal>
</template>

<script>
import LinkService from '@/components/services/LinkService';
import LinkGroupForm from '@/components/links/LinkGroupForm';

export default {
  name: 'CreateLinkGroupModal',
  components: { LinkGroupForm },
  data () {
    return {
      error: '',
      linkGroup: {},
      rawEditMode: false
    };
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    update (linkGroup) {
      this.linkGroup = linkGroup;
    },
    close () {
      this.error = '';
      this.linkGroup = {};
      this.rawEditMode = false;
      this.$bvModal.hide('link-group-form');
    },
    create () {
      if (!this.linkGroup.name || !this.linkGroup.name.length) {
        this.error = 'Group Name is required';
        return;
      }

      // validate the links
      for (const link of this.linkGroup.links) {
        if (!link.name.length) {
          this.error = 'Link Names are required';
          return;
        }
        if (!link.url.length) {
          this.error = 'Link URLs are required';
          return;
        }

        if (!link.itypes.length) {
          this.error = 'Must have at least one type per link';
          return;
        }
      }

      LinkService.createLinkGroup(this.linkGroup).then(() => {
        this.close();
      }).catch((err) => {
        this.error = err;
      });
    }
  }
};
</script>

<style scoped>
.alert.alert-sm {
  padding: 0.4rem 0.8rem;
}
</style>
