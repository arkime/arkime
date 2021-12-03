<template>
  <div class="d-flex flex-wrap link-group-cards-wrapper">
    <div
      class="w-25 p-2"
      :key="linkGroup._id"
      v-for="(linkGroup, index) in getLinkGroups">
      <!-- view (for con3xt page and users who can view but not edit) -->
      <b-card
        v-if="itype || !(getUser && (getUser.userId === linkGroup.creator || hasRole(linkGroup.editRoles, getUser.roles)))"
        class="h-100 align-self-stretch">
        <template #header>
          <h6 class="mb-0">
            {{ linkGroup.name }}
          </h6>
        </template>
        <b-card-body>
          <template
            v-for="(link, i) in linkGroup.links">
            <!-- display link to click -->
            <div :key="link.url + i + 'click'"
              v-if="itype && link.itypes.indexOf(itype) > -1">
              <b-form-checkbox
                inline
                class="link-checkbox"
                @change="$store.commit('TOGGLE_CHECK_LINK', { lgId: linkGroup._id, lname: link.name })"
                :checked="getCheckedLinks[linkGroup._id] && getCheckedLinks[linkGroup._id][link.name]"
              />
              <a target="_blank"
                :href="getUrl(link.url)"
                :style="link.color ? `color:${link.color}` : ''">
                {{ link.name }}
              </a>
            </div> <!-- /display link to click -->
            <!-- display link to view -->
            <div v-else-if="!itype"
              :key="link.url + i + 'view'">
              <strong class="text-warning">
                {{ link.name }}
              </strong>
              <a href="javascript:void(0)"
                :style="link.color ? `color:${link.color}` : ''">
                {{ link.url }}
              </a>
            </div> <!-- /display link to view -->
          </template>
        </b-card-body>
        <template #footer v-if="itype">
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
              v-b-tooltip.hover="'Open all links in this group'">
              Open All
            </b-button>
          </div>
        </template>
      </b-card> <!-- /view -->
      <!-- edit -->
      <b-card v-else
        class="h-100 align-self-stretch">
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
            :link-group="linkGroup"
            @update-link-group="updateLinkGroup"
          />
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
    </div>
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import LinkService from '@/components/services/LinkService';
import LinkGroupForm from '@/components/links/LinkGroupForm';

let timeout;

export default {
  name: 'LinkGroupCards',
  components: { LinkGroupForm },
  props: {
    itype: String, // the itype of the search to display links for
    query: String, // the query in the search bar to apply to urls
    numDays: [Number, String], // the number of days to apply to urls
    numHours: [Number, String], // the number of hours to apply to urls
    stopDate: String, // the stop date to apply to urls
    startDate: String // the start date to apply to urls
  },
  computed: {
    ...mapGetters(['getLinkGroups', 'getUser', 'getCheckedLinks'])
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    updateLinkGroup (linkGroup) {
      this.$store.commit('UPDATE_LINK_GROUP', linkGroup);
    },
    deleteLinkGroup (id, index) {
      LinkService.deleteLinkGroup(id, index);
    },
    saveLinkGroup (linkGroup) {
      this.$set(linkGroup, 'rawEdit', undefined);

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
      return url.replace(/\${indicator}/g, this.query)
        .replace(/\${type}/g, this.itype)
        .replace(/\${numDays}/g, this.numDays)
        .replace(/\${numHours}/g, this.numHours)
        .replace(/\${stopDate}/g, this.stopDate)
        .replace(/\${startDate}/g, this.startDate);
    },
    openAllLinks (linkGroup) {
      for (const link of linkGroup.links) {
        if (link.url && this.getCheckedLinks[linkGroup._id] && this.getCheckedLinks[linkGroup._id][link.name]) {
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
    hasRole (roles, userRoles) {
      return this.$options.filters.hasRole(roles, userRoles);
    }
  }
};
</script>

<style scoped>
.link-group-cards-wrapper {
  margin-left: -0.5rem !important;
  margin-right: -0.5rem !important;
}

/* small alerts */
.alert.alert-sm {
  padding: 0.2rem 0.8rem;
}

.link-checkbox {
  margin-right: 0;
  min-height: 1rem;
}
</style>
