<template>
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
        autofocus
        v-model="lg.name"
        :state="lg.name.length > 0"
        @change="$emit('update-link-group', lg)"
      />
    </b-input-group> <!-- /group name -->
    <!-- group roles -->
    <b-dropdown
      size="sm"
      class="mb-2"
      text="Who Can View">
      <b-dropdown-form>
        <b-form-checkbox-group
          :options="getRoles"
          v-model="lg.viewRoles"
          @change="$emit('update-link-group', lg)"
        />
      </b-dropdown-form>
    </b-dropdown>
    <b-dropdown
      size="sm"
      class="mb-2"
      text="Who Can Edit">
      <b-dropdown-form>
        <b-form-checkbox-group
          :options="getRoles"
          v-model="lg.editRoles"
          @change="$emit('update-link-group', lg)"
        />
      </b-dropdown-form>
    </b-dropdown>
    <span
      class="fa fa-info-circle fa-lg cursor-help ml-2"
      v-b-tooltip.hover="'Creators will always be able to view and edit their link groups regardless of the roles selected here.'"
    /> <!-- /group roles -->
    <!-- group links -->
    <b-card
      :key="i"
      class="mb-2"
      v-for="(link, i) in lg.links">
      <b-input-group
        size="sm"
        class="mb-2">
        <template #prepend>
          <b-input-group-text>
            <span class="fa fa-link mr-2" />
            Name
          </b-input-group-text>
        </template>
        <b-form-input
          trim
          v-model="link.name"
          :state="link.name.length > 0"
          @change="$emit('update-link-group', lg)"
        />
        <template #append>
          <color-picker
            :color="link.color"
            :link-name="link.name"
            @colorSelected="changeColor"
          />
        </template>
      </b-input-group>
      <b-input-group
        size="sm"
        class="mb-2">
        <template #prepend>
          <b-input-group-text>
            <span class="fa fa-link mr-2" />
            URL
          </b-input-group-text>
        </template>
        <b-form-input
          trim
          v-model="link.url"
          :state="link.url.length > 0"
          @change="$emit('update-link-group', lg)"
        />
        <template #append>
          <b-input-group-text
            class="cursor-help"
            v-b-tooltip.hover="'These values within links will be filled in \'${indicator}\' (your search query), \'${startDate}\', \'${stopDate}\', \'${numDays}\', \'${numHours}\', \'${type}\''">
            <span class="fa fa-info-circle" />
          </b-input-group-text>
        </template>
      </b-input-group>
      <b-button
        size="sm"
        variant="danger"
        @click="removeLink(i)"
        class="pull-right mt-1"
        v-b-tooltip.hover.left="'Remove this link'">
        <span class="fa fa-times-circle" />
      </b-button>
      <b-form-checkbox-group
        v-model="link.itypes"
        :options="itypeOptions"
        @change="$emit('update-link-group', lg)"
      />
    </b-card> <!-- /group links -->
    <b-button
      block
      size="sm"
      class="mt-2"
      variant="info"
      @click="addLink">
      <span class="fa fa-link mr-2" />
      Add another
    </b-button>
    <div
      class="mt-2"
      v-if="lg.creator">
      Created by
      <span class="text-info">
        {{ lg.creator }}
      </span>
    </div>
  </b-form> <!-- /form -->
</template>

<script>
import { mapGetters } from 'vuex';

import ColorPicker from '@/utils/ColorPicker';

const defaultLink = {
  url: '',
  name: '',
  itypes: []
};

export default {
  name: 'CreateLinkGroup',
  components: {
    ColorPicker
  },
  props: {
    linkGroup: {
      type: Object
    }
  },
  data () {
    return {
      lg: this.linkGroup ? JSON.parse(JSON.stringify(this.linkGroup)) : undefined,
      itypeOptions: [
        { text: 'Domain', value: 'domain' },
        { text: 'IP', value: 'ip' },
        { text: 'URL', value: 'url' },
        { text: 'Email', value: 'email' },
        { text: 'Hash', value: 'hash' },
        { text: 'Phone', value: 'phone' },
        { text: 'Text', value: 'text' }
      ]
    };
  },
  computed: {
    ...mapGetters(['getRoles'])
  },
  created () {
    if (!this.lg) { // creating new link group
      this.lg = { name: '', links: [], viewRoles: [], editRoles: [] };
      this.addLink();
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    addLink () {
      this.lg.links.push(JSON.parse(JSON.stringify(defaultLink)));
    },
    removeLink (index) {
      this.lg.links.splice(index, 1);
      this.$emit('update-link-group', this.lg);
    },
    changeColor ({ linkName, color }) {
      for (const link of this.lg.links) {
        if (link.name === linkName) {
          this.$set(link, 'color', color);
          this.$emit('update-link-group', this.lg);
          return;
        }
      }
    }
  }
};
</script>

<style scoped>
.alert.alert-sm {
  padding: 0.4rem 0.8rem;
}
</style>
