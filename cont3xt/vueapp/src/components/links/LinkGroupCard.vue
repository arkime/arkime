<template v-if="linkGroup">
  <!-- view (for con3xt page and users who can view but not edit) -->
  <b-card
    v-if="itype || !(getUser && (getUser.userId === linkGroup.creator || linkGroup._editable))"
    class="h-100 align-self-stretch">
    <template #header>
      <h6 class="mb-0 link-header">
        <span
          class="fa mr-1 cursor-pointer"
          @click="toggleLinkGroup(linkGroup)"
          :class="collapsedLinkGroups[linkGroup._id] ? 'fa-chevron-down' : 'fa-chevron-up'"
        />
        <span
          class="fa fa-share-alt mr-1 cursor-help"
          v-if="getUser && linkGroup.creator !== getUser.userId"
          v-b-tooltip.hover="`Shared with you by ${linkGroup.creator}`"
        />
        {{ linkGroup.name }}
      </h6>
    </template>
    <b-card-body>
      <div v-show="!collapsedLinkGroups[linkGroup._id]">
        <template
          v-for="(link, i) in filteredLinks">
          <!-- display link to click -->
          <div class="link-display"
            :key="link.url + i + 'click'"
            v-if="itype && link.name !== '----------'">
            <b-form-checkbox
              inline
              class="link-checkbox"
              @change="$store.commit('TOGGLE_CHECK_LINK', { lgId: linkGroup._id, lname: link.name })"
              :checked="getCheckedLinks[linkGroup._id] && getCheckedLinks[linkGroup._id][link.name]"
            />
            <a target="_blank"
              :title="link.name"
              :href="getUrl(link.url)"
              :style="link.color ? `color:${link.color}` : ''">
              {{ link.name }}
            </a>
          </div> <!-- /display link to click -->
          <!-- display link to view -->
          <div :title="link.name"
            :key="link.url + i + 'view'"
            v-else-if="!itype && link.name !== '----------'">
            <strong class="text-warning">
              {{ link.name }}
            </strong>
            <a href="javascript:void(0)"
              :style="link.color ? `color:${link.color}` : ''">
              {{ link.url }}
            </a>
          </div> <!-- /display link to view -->
          <!-- separator -->
          <hr class="link-separator-display"
            :key="link.url + i + 'separator'"
            v-else-if="link.name === '----------'"
            :style="`border-color: ${link.color || '#777'}`"
          >
        </template>
      </div>
    </b-card-body>
    <template #footer v-if="itype && !collapsedLinkGroups[linkGroup._id]">
      <div class="w-100 d-flex justify-content-between align-items-start">
        <b-form-checkbox
          role="checkbox"
          class="mr-2 mt-1"
          v-b-tooltip.hover="'Select All'"
          :checked="allLinksChecked(linkGroup)"
          @change="e => toggleAllLinks(linkGroup, e)">
        </b-form-checkbox>
        <b-button
          block
          size="sm"
          variant="secondary"
          @click="openAllLinks(linkGroup)"
          v-b-tooltip.hover="'Open all selected links in this group'">
          Open Selected
        </b-button>
      </div>
    </template>
  </b-card> <!-- /view -->
  <!-- edit -->
  <b-card v-else
    class="h-100 align-self-stretch">
    <template slot="header">
      <div class="w-100 d-flex justify-content-between align-items-start">
        <b-button
          size="sm"
          variant="danger"
          v-b-tooltip.hover="'Delete this link group'"
          @click="deleteLinkGroup(linkGroup._id)">
          <span class="fa fa-trash" />
        </b-button>
        <b-alert
          variant="success"
          :show="linkGroup.success"
          class="mb-0 mt-0 alert-sm mr-1 ml-1">
          <span class="fa fa-check mr-2" />
          Saved!
        </b-alert>
        <div>
          <b-button
            size="sm"
            variant="warning"
            @click="rawConfigLinkGroup(linkGroup)"
            v-b-tooltip.hover="'Edit the raw config for this link group'">
            <span class="fa fa-pencil-square-o" />
          </b-button>
          <b-button
            size="sm"
            variant="success"
            @click="saveLinkGroup(linkGroup)"
            v-b-tooltip.hover="'Save this link group'">
            <span class="fa fa-save" />
          </b-button>
        </div>
      </div>
    </template>
    <b-card-body>
      <textarea
        rows="20"
        size="sm"
        v-if="linkGroup.rawEdit"
        @input="e => debounceRawEdit(e, linkGroup)"
        class="form-control form-control-sm"
        :value="JSON.stringify(linkGroup.rawEdit, null, 2)"
      />
      <link-group-form
        v-else
        @save-link-group="saveLinkGroup"
        :link-group-index="linkGroupIndex"
        @update-link-group="updateLinkGroup"
      />
    </b-card-body>
    <template slot="footer">
      <div class="w-100 d-flex justify-content-between align-items-start">
        <b-button
          size="sm"
          variant="danger"
          v-b-tooltip.hover="'Delete this link group'"
          @click="deleteLinkGroup(linkGroup._id)">
          <span class="fa fa-trash" />
        </b-button>
        <b-alert
          variant="success"
          :show="linkGroup.success"
          class="mb-0 mt-0 alert-sm mr-1 ml-1">
          <span class="fa fa-check mr-2" />
          Saved!
        </b-alert>
        <div>
          <b-button
            size="sm"
            variant="warning"
            @click="rawConfigLinkGroup(linkGroup)"
            v-b-tooltip.hover="'Edit the raw config for this link group'">
            <span class="fa fa-pencil-square-o" />
          </b-button>
          <b-button
            size="sm"
            variant="success"
            @click="saveLinkGroup(linkGroup)"
            v-b-tooltip.hover="'Save this link group'">
            <span class="fa fa-save" />
          </b-button>
        </div>
      </div>
    </template>
  </b-card> <!-- /edit -->
</template>

<script>
import dr from 'defang-refang';
import { mapGetters } from 'vuex';

import LinkService from '@/components/services/LinkService';
import LinkGroupForm from '@/components/links/LinkGroupForm';

let timeout;

export default {
  name: 'LinkGroupCard',
  components: { LinkGroupForm },
  props: {
    itype: String, // the itype of the search to display links for
    query: String, // the query in the search bar to apply to urls
    numDays: [Number, String], // the number of days to apply to urls
    numHours: [Number, String], // the number of hours to apply to urls
    stopDate: String, // the stop date to apply to urls
    startDate: String, // the start date to apply to urls
    linkGroupIndex: Number, // the index of the link group to display in the array of link groups
    hideLinks: Object // which links to hide when a user is searching links in link groups
  },
  data () {
    return {
      collapsedLinkGroups: this.$store.state.collapsedLinkGroups
    };
  },
  computed: {
    ...mapGetters([
      'getUser', 'getCheckedLinks', 'getLinkGroups'
    ]),
    linkGroup () {
      if (this.linkGroupIndex === undefined) { return {}; }
      return this.getLinkGroups.length ? this.getLinkGroups[this.linkGroupIndex] : {};
    },
    filteredLinks () {
      const links = [];

      for (let i = 0, len = this.linkGroup.links.length; i < len; i++) {
        const link = this.linkGroup.links[i];
        // first, does it match the itype of the indicator searched?
        if (!this.itype || link.itypes.indexOf(this.itype) > -1) {
          // then, is it visible if the user is searching for links that match
          // AND it's not a separator (separators aren't filtered out of search)
          if (link.url !== '----------' && (!this.hideLinks || !this.hideLinks[i])) {
            links.push(link);
          } else if (links.length > 0 && // don't show multiple separators in a row
            link.url === '----------' &&
            links[links.length - 1].url !== '----------') {
            links.push(link);
          }
        }
      }

      if (links.length && links[links.length - 1].url === '----------') {
        links.pop(); // don't end with a separator
      }

      return links;
    }
  },
  methods: {
    updateLinkGroup (linkGroup) {
      this.$store.commit('UPDATE_LINK_GROUP', linkGroup);
    },
    deleteLinkGroup (id) {
      LinkService.deleteLinkGroup(id, this.linkGroupIndex);
      this.$emit('delete-link-group', { index: this.linkGroupIndex });
    },
    saveLinkGroup (linkGroup) {
      this.$set(linkGroup, 'rawEdit', undefined);
      this.$set(linkGroup, 'success', undefined);

      LinkService.updateLinkGroup(linkGroup).then((response) => {
        linkGroup.success = true;
        this.$store.commit('UPDATE_LINK_GROUP', linkGroup);
        setTimeout(() => {
          linkGroup.success = false;
          this.$store.commit('UPDATE_LINK_GROUP', linkGroup);
        }, 4000);
      }); // store deals with failure
    },
    rawConfigLinkGroup (linkGroup) {
      if (linkGroup.rawEdit) {
        this.$set(linkGroup, 'rawEdit', undefined);
        return;
      }

      // remove uneditable fields
      const clone = JSON.parse(JSON.stringify(linkGroup));
      delete clone._id;
      delete clone.success;
      delete clone.creator;
      delete clone.rawEdit;
      delete clone._editable;

      this.$set(linkGroup, 'rawEdit', clone);
    },
    debounceRawEdit (e, linkGroup) {
      if (timeout) { clearTimeout(timeout); }
      // debounce the textarea so it only updates the link group after keyups cease for 400ms
      timeout = setTimeout(() => {
        timeout = null;
        this.updateRawLinkGroup(e, linkGroup);
      }, 400);
    },
    updateRawLinkGroup (e, linkGroup) {
      const updatedLinkGroup = JSON.parse(e.target.value);
      this.$set(linkGroup, 'name', updatedLinkGroup.name);
      this.$set(linkGroup, 'links', updatedLinkGroup.links || []);
    },
    getUrl (url) {
      return url.replace(/\${indicator}/g, dr.refang(this.query))
        .replace(/\${type}/g, this.itype)
        .replace(/\${numDays}/g, this.numDays)
        .replace(/\${numHours}/g, this.numHours)
        .replace(/\${stopTS}/g, this.stopDate)
        .replace(/\${startTS}/g, this.startDate)
        .replace(/\${stopDate}/g, this.stopDate.split('T')[0])
        .replace(/\${startDate}/g, this.startDate.split('T')[0]);
    },
    openAllLinks (linkGroup) {
      for (const link of linkGroup.links) {
        if (link.url &&
          link.itypes.includes(this.itype) &&
          this.getCheckedLinks[linkGroup._id] &&
          this.getCheckedLinks[linkGroup._id][link.name]) {
          window.open(this.getUrl(link.url), '_blank');
        }
      }
    },
    toggleAllLinks (linkGroup, checked) {
      this.$store.commit('TOGGLE_CHECK_ALL_LINKS', { lgId: linkGroup._id, checked });
    },
    allLinksChecked (linkGroup) {
      if (!this.getCheckedLinks[linkGroup._id]) {
        return false;
      }

      let count = 0;
      for (const link in this.getCheckedLinks[linkGroup._id]) {
        if (this.getCheckedLinks[linkGroup._id][link]) {
          count++;
        }
      }

      return count === linkGroup.links.length;
    },
    toggleLinkGroup (linkGroup) {
      this.$set(this.collapsedLinkGroups, linkGroup._id, !this.collapsedLinkGroups[linkGroup._id]);
      this.$store.commit('SET_COLLAPSED_LINK_GROUPS', this.collapsedLinkGroups);
    }
  }
};
</script>

<style scoped>
/* small alerts */
.alert.alert-sm {
  padding: 0.2rem 0.8rem;
}

.link-checkbox {
  margin-right: 0;
  min-height: 1rem;
}

.link-header {
  overflow: hidden;
  margin-right: 1rem;
  white-space: nowrap;
  text-overflow: ellipsis;
}

.link-display {
  overflow: hidden;
  white-space: nowrap;
  text-overflow: ellipsis;
}

.link-separator-display {
  border-width: 2px;
  margin-top: 0.5rem;
  margin-bottom: 0.5rem;
}
</style>
