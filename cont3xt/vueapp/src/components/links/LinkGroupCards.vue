<template>
  <div class="d-flex flex-wrap link-group-cards-wrapper">
    <div
      :key="linkGroup._id"
      class="w-25 p-2"
      v-for="(linkGroup, index) in getLinkGroups">
      <!-- view -->
      <b-card v-if="itype"
        class="h-100 align-self-stretch">
        <template #header>
          <h6 class="mb-0">
            {{ linkGroup.name }}
          </h6>
        </template>
        <b-card-body>
          <template
            v-for="(link, i) in linkGroup.links">
            <!-- display link -->
            <div :key="link.url + i"
              v-if="itype && link.itypes.indexOf(itype) > -1">
              <a target="_blank"
                :href="getUrl(link.url)">
                {{ link.name }}
              </a>
            </div> <!-- /display link -->
          </template>
        </b-card-body>
        <template #footer>
          <b-button
            block
            size="sm"
            variant="secondary"
            @click="openAllLinks(linkGroup)"
            v-b-tooltip.hover="'Open all links in this group'">
            Open All
          </b-button>
        </template>
      </b-card> <!-- /view -->
      <!-- edit -->
      <b-card v-else
        class="h-100 align-self-stretch">
        <b-card-body>
          <link-group-form
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
            <b-button
              size="sm"
              variant="success"
              @click="saveLinkGroup(linkGroup)"
              v-b-tooltip.hover="'Save this link group'">
              <span class="fa fa-save" />
            </b-button>
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
    ...mapGetters(['getLinkGroups'])
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
      LinkService.updateLinkGroup(linkGroup).then((response) => {
        linkGroup.success = true;
        this.$store.commit('UPDATE_LINK_GROUP', linkGroup);
        setTimeout(() => {
          linkGroup.success = false;
          this.$store.commit('UPDATE_LINK_GROUP', linkGroup);
        }, 4000);
      }); // store deals with failure
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
        if (link.url) {
          window.open(this.getUrl(link.url), '_blank');
        }
      }
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
</style>
