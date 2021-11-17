<template>
  <div class="d-flex flex-wrap link-group-cards-wrapper">
    <div
      :key="linkGroup._id"
      class="w-25 p-2"
      v-for="(linkGroup, index) in getLinkGroups">
      <b-card class="h-100 align-self-stretch">
        <template #header>
          <h6 class="mb-0">
            {{ linkGroup.name }}
          </h6>
        </template>
        <b-card-body>
          <template
            v-for="(link, i) in linkGroup.links">
            <!-- display link data -->
            <div
              v-if="!itype"
              :key="link.url + i">
              <hr v-if="i > 0" class="hr-small">
              <div>
                <strong class="text-info">Name</strong>
                {{ link.name }}
              <div>
              </div>
                <strong class="text-info">URL</strong>
                <span v-b-tooltip.hover="link.url">
                  {{ link.url.length > 80 ? `${link.url.substring(0, 100)}...` : link.url }}
                </span>
              </div>
              <div>
                <strong class="text-info">Types</strong>
                {{ link.itypes.join(', ') }}
              </div>
            </div> <!-- /display link data -->
            <!-- display link -->
            <div
              :key="link.url + i"
              v-else-if="itype && link.itypes.indexOf(itype) > -1">
              <a target="_blank"
                :href="getUrl(link.url)">
                {{ link.name }}
              </a>
            </div> <!-- /display link -->
          </template>
        </b-card-body>
        <template
          #footer
          v-if="!itype">
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
        <template
          v-else
          #footer>
          <b-button
            block
            size="sm"
            variant="secondary"
            @click="openAllLinks(linkGroup)"
            v-b-tooltip.hover="'Open all links in this group'">
            Open All
          </b-button>
        </template>
      </b-card>
    </div>
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import LinkService from '@/components/services/LinkService';

export default {
  name: 'LinkGroupCards',
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
    deleteLinkGroup (id, index) {
      LinkService.deleteLinkGroup(id, index);
    },
    updateLinkGroup (linkGroup) { // TODO
      console.log('coming soon!');
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
</style>
