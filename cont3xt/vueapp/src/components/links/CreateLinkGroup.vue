<template>
  <b-modal
    scrollable
    @shown="modalShown"
    id="link-group-form">
    <!-- header -->
    <template #modal-title>
      <h4 class="mb-0">
        Create New Link Group
      </h4>
    </template> <!-- /header -->
    <!-- form -->
    <b-form>
      <!-- group name -->
      <b-input-group
        size="sm"
        class="mb-2">
        <template #prepend>
          <b-input-group-text>
            Group Name
          </b-input-group-text>
        </template>
        <b-form-input
          trim
          required
          v-focus="setFocus"
          v-model="groupName"
          :state="groupName.length > 0"
        />
      </b-input-group> <!-- /group name -->
      <hr>
      <!-- group links -->
      <h5>
        <span class="fa fa-link mr-2" />
        Links
      </h5>
      <span :key="i"
        v-for="(link, i) in links">
        <hr>
        <b-input-group
          size="sm"
          class="mb-2">
          <template #prepend>
            <b-input-group-text>
              Name
            </b-input-group-text>
          </template>
          <b-form-input
            trim
            v-model="link.name"
            :state="link.name.length > 0"
          />
        </b-input-group>
        <b-input-group
          size="sm"
          class="mb-2">
          <template #prepend>
            <b-input-group-text>
              URL
            </b-input-group-text>
          </template>
          <b-form-input
            trim
            v-model="link.url"
            :state="link.url.length > 0"
          />
        </b-input-group>
        <b-button
          variant="danger"
          @click="removeLink(i)"
          class="pull-right mt-1"
          v-b-tooltip.hover.left="'Remove this link'">
          <span class="fa fa-times-circle" />
        </b-button>
        <template v-for="(itype, key) in link.itypes">
          <b-form-checkbox
            inline
            :key="key"
            v-model="itype.value">
            {{ itype.name }}
          </b-form-checkbox>
        </template>
      </span> <!-- /group links -->
      <hr>
      <b-button
        block
        size="sm"
        class="mt-2"
        variant="info"
        @click="addLink">
        <span class="fa fa-link mr-2" />
        Add another
      </b-button>
    </b-form> <!-- /form -->
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
          @click="create"
          variant="success">
          Create
        </b-button>
      </div>
    </template> <!-- /footer -->
  </b-modal>
</template>

<script>
import Focus from '@/utils/Focus';
import LinkService from '@/components/services/LinkService';

const defaultLink = {
  url: '',
  name: '',
  itypes: {
    domain: {
      name: 'Domain',
      value: false
    },
    ip: {
      name: 'IP',
      value: false
    },
    url: {
      name: 'URL',
      value: false
    },
    email: {
      name: 'Email',
      value: false
    },
    hash: {
      name: 'Hash',
      value: false
    },
    phone: {
      name: 'Phone',
      value: false
    },
    text: {
      name: 'Text',
      value: false
    }
  }
};

export default {
  name: 'CreateLinkGroup',
  directives: { Focus },
  data () {
    return {
      setFocus: false,
      error: '',
      groupName: '',
      links: [
        JSON.parse(JSON.stringify(defaultLink))
      ]
    };
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    addLink () {
      this.links.push(JSON.parse(JSON.stringify(defaultLink)));
    },
    removeLink (index) {
      this.links.splice(index, 1);
    },
    close () {
      this.links = [];
      this.addLink();
      this.error = '';
      this.groupName = '';
      this.setFocus = false;
      this.$bvModal.hide('link-group-form');
    },
    create () {
      if (!this.groupName.length) {
        this.error = 'Group Name is required';
        return;
      }

      const linkGroup = {
        name: this.groupName,
        links: []
      };

      const links = JSON.parse(JSON.stringify(this.links));
      for (const link of links) {
        if (!link.name.length) {
          this.error = 'Link Names are required';
          return;
        }
        if (!link.url.length) {
          this.error = 'Link URLs are required';
          return;
        }

        const itypes = [];
        for (const key in link.itypes) {
          if (link.itypes[key].value) {
            itypes.push(key);
          }
        }

        if (!itypes.length) {
          this.error = 'Must have at least one type per link';
          return;
        }

        link.itypes = itypes;
        linkGroup.links.push(link);
      }

      LinkService.createLinkGroup(linkGroup).then((response) => {
        this.$emit('update-link-groups');
        this.close();
      }).catch((err) => {
        this.error = err;
      });
    },
    /* helpers ------------------------------------------------------------- */
    modalShown () {
      this.setFocus = true;
    }
  }
};
</script>

<style scoped>
.alert.alert-sm {
  padding: 0.4rem 0.8rem;
}
</style>
