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
  </b-form> <!-- /form -->
</template>

<script>
const defaultLink = {
  url: '',
  name: '',
  itypes: []
};

export default {
  name: 'CreateLinkGroup',
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
  created () {
    if (!this.lg) { // creating new link group
      this.lg = { name: '', links: [] };
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
    }
  }
};
</script>

<style scoped>
.alert.alert-sm {
  padding: 0.4rem 0.8rem;
}
</style>
