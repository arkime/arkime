<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template v-if="linkGroup">
  <!-- view (for con3xt page and users who can view but not edit) -->
  <v-card
    v-if="itype || !(getUser && (getUser.userId === linkGroup.creator || linkGroup._editable || (getUser.roles && getUser.roles.includes('cont3xtAdmin'))))"
    variant="tonal"
    class="h-100 align-self-stretch">
    <v-card-title>
      <h6 class="mb-0 link-header">
        <span
          class="fa mr-1 cursor-pointer"
          @click="toggleLinkGroup(linkGroup)"
          :class="collapsedLinkGroups[linkGroup._id] ? 'fa-chevron-down' : 'fa-chevron-up'"
        />
        <span
          class="fa fa-share-alt mr-1 cursor-help"
          v-if="getUser && linkGroup.creator !== getUser.userId"
          v-tooltip="`Shared with you by ${linkGroup.creator}`"
        />
        {{ linkGroup.name }}
        <div v-if="!itype && getUser && linkGroup.creator !== getUser.userId" class="pull-right">
          <small>
            You can only view this Link Group
          </small>
          <v-btn
              size="small"
              color="secondary"
              variant="elevated"
              @click="rawEditMode = !rawEditMode"
              v-tooltip="`View ${rawEditMode ? 'form' : 'raw'} configuration for this link group`">
            <span class="fa fa-fw" :class="{'fa-file-text-o': rawEditMode, 'fa-pencil-square-o': !rawEditMode}" />
          </v-btn>
        </div>
      </h6>
    </v-card-title>
    <v-card-text class="py-0">
      <div v-show="!collapsedLinkGroups[linkGroup._id]">
        <template v-if="!rawEditMode">
          <template
              v-for="(link, i) in filteredLinks">
            <!-- display link to click -->
            <div class="link-display d-flex flex-row align-center"
                 :key="link.url + i + 'click'"
                 v-if="itype && link.name !== '----------'">
              <v-checkbox
                  inline
                  tabindex="-1"
                  class="link-checkbox"
                  @change="$store.commit('TOGGLE_CHECK_LINK', { lgId: linkGroup._id, lname: link.name })"
                  :checked="getCheckedLinks[linkGroup._id] && getCheckedLinks[linkGroup._id][link.name]"
              />
              <a tabindex="-1"
                 target="_blank"
                 class="link"
                 :title="link.name"
                 :href="getUrl(link.url)"
                 :style="link.color ? `color:${link.color}` : ''">
                {{ link.name }}
              </a>
              <link-guidance class="ml-1" :link="link" :element-id="`${linkGroup._id}-${i}`" />
            </div> <!-- /display link to click -->
            <!-- display link to view -->
            <div :title="link.name"
                 :key="link.url + i + 'view'"
                 v-else-if="!itype && link.name !== '----------'">
              <strong class="text-warning">
                {{ link.name }}
              </strong>
              <a tabindex="-1"
                 class="link"
                 href="javascript:void(0)"
                 :style="link.color ? `color:${link.color}` : ''">
                {{ link.url }}
              </a>
              <link-guidance class="ml-1" :link="link" :element-id="`${linkGroup.name}-${i}`" />
            </div> <!-- /display link to view -->
            <!-- separator -->
            <hr class="link-separator-display"
                :key="link.url + i + 'separator'"
                v-else-if="link.name === '----------'"
                :style="`border-color: ${link.color || '#777'}`"
            >
          </template>
        </template>
        <link-group-form
            v-else
            :raw-edit-mode="rawEditMode"
            :link-group="updatedLinkGroup"
            :no-edit="true"
            @display-message="displayMessage"
            @update-link-group="updateLinkGroup"
        />
      </div>
    </v-card-text>
    <template #actions v-if="itype && !collapsedLinkGroups[linkGroup._id]">
      <div class="w-100 d-flex justify-space-between align-center">
        <v-checkbox
          tabindex="-1"
          role="checkbox"
          class="ml-2"
          v-tooltip="'Select All'"
          :checked="allLinksChecked(linkGroup)"
          @change="e => toggleAllLinks(linkGroup, e)">
        </v-checkbox>
        <v-btn
          size="small"
          tabindex="-1"
          class="flex-grow-1"
          color="secondary"
          variant="elevated"
          @click="openAllLinks(linkGroup)"
          v-tooltip="'Open all selected links in this group'">
          Open Selected
        </v-btn>
      </div>
    </template>
  </v-card> <!-- /view -->
  <!-- edit -->
  <v-card v-else
    variant="tonal"
    class="h-100 align-self-stretch">
    <template #title>
      <div class="w-100 d-flex justify-space-between">
        <div class="d-flex flex-row ga-1">
          <!-- delete button -->
          <transition name="buttons">
            <v-btn
              size="small"
              color="error"
              variant="elevated"
              v-if="!confirmDelete"
              @click="confirmDelete = true"
              v-tooltip="'Delete this link group TOP'">
              <span class="fa fa-trash" />
            </v-btn>
          </transition> <!-- /delete button -->
          <!-- cancel confirm delete button -->
          <transition name="buttons">
            <v-btn
              size="small"
              v-tooltip="'Cancel'"
              title="Cancel"
              color="warning"
              variant="elevated"
              v-if="confirmDelete"
              @click="confirmDelete = false">
              <span class="fa fa-ban" />
            </v-btn>
          </transition> <!-- /cancel confirm delete button -->
          <!-- confirm delete button -->
          <transition name="buttons">
            <v-btn
              size="small"
              color="error"
              variant="elevated"
              v-tooltip="'Are you sure?'"
              title="Are you sure?"
              v-if="confirmDelete"
              @click="deleteLinkGroup(linkGroup._id)">
              <span class="fa fa-check" />
            </v-btn>
          </transition> <!-- /confirm delete button -->
        </div>
        <v-alert
          v-if="success"
          color="success"
          class="mb-0 mt-0 alert-sm mr-1 ml-1">
          <span class="fa fa-check mr-2" />
          <template v-if="message">
            {{ message }}
          </template>
          <template v-else>
            Saved!
          </template>
        </v-alert>
        <div class="d-flex flex-row ga-1">
          <v-btn
            size="small"
            color="info"
            variant="elevated"
            v-if="canTransfer(linkGroup)"
            v-tooltip="'Transfer ownership of this link group'"
            title="Transfer ownership of this link group"
            @click="$emit('open-transfer-resource', linkGroup)">
            <span class="fa fa-share fa-fw" />
          </v-btn>
          <transition name="buttons">
            <v-btn
              size="small"
              color="secondary"
              variant="elevated"
              @click="rawEditMode = !rawEditMode"
              v-tooltip="`Edit ${rawEditMode ? 'form' : 'raw'} configuration for this link group`">
              <span class="fa fa-fw" :class="{'fa-file-text-o': rawEditMode, 'fa-pencil-square-o': !rawEditMode}" />
            </v-btn>
          </transition>
          <transition name="buttons">
            <v-btn
              size="small"
              color="warning"
              v-if="changesMade"
              variant="elevated"
              @click="cancelUpdateLinkGroup(linkGroup)"
              v-tooltip="'Cancel unsaved updates'">
              <span class="fa fa-ban" />
            </v-btn>
          </transition>
          <transition name="buttons">
            <v-btn
              size="small"
              color="success"
              v-if="changesMade"
              variant="elevated"
              @click="saveLinkGroup(linkGroup)"
              v-tooltip="'Save this link group'">
              <span class="fa fa-save" />
            </v-btn>
          </transition>
        </div>
      </div>
    </template>
    <v-card-text>
      <link-group-form
        :raw-edit-mode="rawEditMode"
        :link-group="updatedLinkGroup"
        @display-message="displayMessage"
        @update-link-group="updateLinkGroup"
      />
    </v-card-text>
    <template #actions>
      <div class="w-100 d-flex justify-space-between">
        <div class="d-flex flex-row ga-1">
          <!-- delete button -->
          <transition name="buttons">
            <v-btn
              size="small"
              color="error"
              variant="elevated"
              v-if="!confirmDelete"
              @click="confirmDelete = true"
              v-tooltip="'Delete this link group BOTTOM'">
              <span class="fa fa-trash" />
            </v-btn>
          </transition> <!-- /delete button -->
          <!-- cancel confirm delete button -->
          <transition name="buttons">
            <v-btn
              size="small"
              v-tooltip="'Cancel'"
              title="Cancel"
              color="warning"
              variant="elevated"
              v-if="confirmDelete"
              @click="confirmDelete = false">
              <span class="fa fa-ban" />
            </v-btn>
          </transition> <!-- /cancel confirm delete button -->
          <!-- confirm delete button -->
          <transition name="buttons">
            <v-btn
              class="ml-0"
              size="small"
              color="error"
              variant="elevated"
              v-tooltip="'Are you sure?'"
              title="Are you sure?"
              v-if="confirmDelete"
              @click="deleteLinkGroup(linkGroup._id)">
              <span class="fa fa-check" />
            </v-btn>
          </transition> <!-- /confirm delete button -->
        </div>
        <v-alert
          color="success"
          v-if="success"
          class="mb-0 mt-0 alert-sm mr-1 ml-1">
          <span class="fa fa-check mr-2" />
          Saved!
        </v-alert>
        <div class="d-flex flex-row ga-1">
          <v-btn
            size="small"
            color="info"
            variant="elevated"
            v-if="canTransfer(linkGroup)"
            v-tooltip="'Transfer ownership of this link group'"
            title="Transfer ownership of this link group"
            @click="$emit('open-transfer-resource', linkGroup)">
            <span class="fa fa-share fa-fw" />
          </v-btn>
          <transition name="buttons">
            <v-btn
              class="ml-0"
              size="small"
              color="secondary"
              variant="elevated"
              @click="rawEditMode = !rawEditMode"
              v-tooltip="'Toggle raw configuration for this link group'">
              <span class="fa fa-pencil-square-o" />
            </v-btn>
          </transition>
          <transition name="buttons">
            <v-btn
              size="small"
              color="warning"
              v-if="changesMade"
              variant="elevated"
              @click="cancelUpdateLinkGroup(linkGroup)"
              v-tooltip="'Cancel unsaved updates'">
              <span class="fa fa-ban" />
            </v-btn>
          </transition>
          <transition name="buttons">
            <v-btn
              size="small"
              color="success"
              v-if="changesMade"
              variant="elevated"
              @click="saveLinkGroup(linkGroup)"
              v-tooltip="'Save this link group'">
              <span class="fa fa-save" />
            </v-btn>
          </transition>
        </div>
      </div>
    </template>
  </v-card> <!-- /edit -->
</template>

<script>
import moment from 'moment';
import dr from 'defang-refang';
import { mapGetters } from 'vuex';

import LinkService from '@/components/services/LinkService';
import LinkGroupForm from '@/components/links/LinkGroupForm.vue';
import LinkGuidance from '@/utils/LinkGuidance.vue';
import { Cont3xtIndicatorProp } from '@/utils/cont3xtUtil';

export default {
  name: 'LinkGroupCard',
  components: { LinkGroupForm, LinkGuidance },
  props: {
    indicator: Cont3xtIndicatorProp, // the indicator { query, itype } to display links for
    numDays: [Number, String], // the number of days to apply to urls
    numHours: [Number, String], // the number of hours to apply to urls
    stopDate: String, // the stop date to apply to urls
    startDate: String, // the start date to apply to urls
    hideLinks: Object, // which links to hide when a user is searching links in link groups
    linkGroup: { // the link group object to generate links
      type: Object,
      required: true
    },
    preUpdatedLinkGroup: { // persists unsaved changes between switching the actively-edited link group
      type: Object,
      required: false
    },
    itype: String // the itype to filter links by
  },
  data () {
    return {
      message: '',
      success: false,
      rawEditMode: false,
      changesMade: false,
      confirmDelete: false,
      updatedLinkGroup: this.preUpdatedLinkGroup ?? JSON.parse(JSON.stringify(this.linkGroup))
    };
  },
  computed: {
    ...mapGetters([
      'getUser', 'getCheckedLinks'
    ]),
    query () {
      return this.indicator?.query;
    },
    collapsedLinkGroups () {
      return this.$store.state.collapsedLinkGroups;
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
    canTransfer (lg) {
      return this.getUser.roles.includes('cont3xtAdmin') ||
        (lg.creator && lg.creator === this.getUser.userId);
    },
    displayMessage (msg) {
      this.message = msg;
      this.success = true;

      setTimeout(() => {
        this.message = '';
        this.success = false;
      }, 4000);
    },
    updateLinkGroup (updated) {
      this.updatedLinkGroup = JSON.parse(JSON.stringify(updated));

      // determine whether there are unsaved changes
      const normalizedInitial = this.normalizeLinkGroup(this.linkGroup);
      const normalizedUpdated = this.normalizeLinkGroup(this.updatedLinkGroup);
      this.changesMade = JSON.stringify(normalizedInitial) !== JSON.stringify(normalizedUpdated);
      // persist these changes to the scope of the Settings page
      this.$emit('update-link-group', this.updatedLinkGroup);
    },
    deleteLinkGroup (id) {
      LinkService.deleteLinkGroup(id);
    },
    cancelUpdateLinkGroup () {
      this.updatedLinkGroup = JSON.parse(JSON.stringify(this.linkGroup));
      this.$emit('update-link-group', this.updatedLinkGroup);
      this.changesMade = false; // make sure save button is hidden
    },
    saveLinkGroup () {
      this.success = false;

      LinkService.updateLinkGroup(this.updatedLinkGroup).then(() => {
        this.displayMessage();
        this.changesMade = false;
      }); // store deals with failure
    },
    normalizeLinkGroup (unNormalizedLinkGroup) {
      const normalizedLinkGroup = JSON.parse(JSON.stringify(unNormalizedLinkGroup));

      // use falsy undefined defaults to ensure that all links have all fields
      normalizedLinkGroup.links = normalizedLinkGroup.links?.map(link => ({
        ...link,
        externalDocName: link.externalDocName || undefined,
        externalDocUrl: link.externalDocUrl || undefined,
        infoField: link.infoField || undefined,
        expanded: undefined // don't care about expanded (only used for UI)
      }));

      // sort edit/view roles to make order not matter for the comparison of these fields (as it is not meaningful)
      normalizedLinkGroup.viewRoles.sort();
      normalizedLinkGroup.editRoles.sort();

      return normalizedLinkGroup;
    },
    /**
     * Replace the array placeholder in the url with the array of values
     * placeholder looks like this: ${array,{iType:"ip",include:"top",sep:"OR",quote:"\""}}
     * @param {string} url - the url to parse
     * @param {object} options - the options for the array substitution
     * @param {array} match - the match array from the regex
     * @param {number} begin - the index of the start of the match
     * @param {number} end - the index of the end of the match
     * @returns {string} the url with the array placeholder replaced/removed
     */
    replaceArray (url, options, match, begin, end) {
      if (!options.iType) {
        console.error('getUrl: array requires an iType');
        url = url.substring(0, begin - match[0].length) + url.substring(end + 1);
        return url;
      }

      options.include ??= 'all'; // default to all
      options.quote ??= ''; // default to no quote
      options.sep ??= ','; // default to comma

      if (['domain', 'ip', 'url', 'email', 'hash', 'phone', 'text'].indexOf(options.iType) === -1) {
        console.error('getUrl: array requires a valid iType');
        url = url.substring(0, begin - match[0].length) + url.substring(end + 1);
        return url;
      }

      const indicators = [];
      for (const key in this.$store.state.indicatorGraph) {
        const indicator = this.$store.state.indicatorGraph[key];
        if (indicator.indicator.itype === options.iType) {
          if (options.include === 'all' ||
            (options.include === 'top' && (indicator.parentIds.has(undefined) || indicator.parentIds.size === 0))) {
            indicators.push(indicator.indicator.query);
          }
        }
      }

      if (indicators.length) {
        const indicatorStr = options.quote + indicators.join(`${options.quote}${options.sep}${options.quote}`) + options.quote;
        url = url.substring(0, begin - match[0].length) + indicatorStr + url.substring(end + 1);
      } else {
        url = url.substring(0, begin - match[0].length) + url.substring(end + 1);
      }

      return url;
    },
    /**
     * Replace the start and end placeholders in the url with the formatted date
     * placeholder looks like this: ${start,{"format":"YYYY-MM-DD","timeSnap":"1d"}}
     * @param {string} url - the url to parse
     * @param {object} options - the options for the date formatting
     * @param {array} match - the match array from the regex
     * @param {number} begin - the index of the start of the match
     * @param {number} end - the index of the end of the match
     * @returns {string} the url with the start|end placeholder replaced
     */
    replaceDate (url, options, match, begin, end) {
      const format = options.format ?? 'YYYY-MM-DD';
      let date = match.includes('end') ? this.stopDate : this.startDate;

      if (options.timeSnap) {
        date = this.$options.filters.parseSeconds(options.timeSnap, date) * 1000;
      }

      const formattedDate = moment(date).format(format);
      return url.substring(0, begin - match[0].length) + formattedDate + url.substring(end + 1);
    },
    /**
     * Replaces the JSON parsable values in the url
     * if it can't parse the JSON options, it removes the placeholder
     * @param {string} url - the url to parse
     * @returns {string} the url with the JSON parsable values replaced/removed
     */
    replaceJSON (url) {
      const regexp = /\$\{(array|start|end),/g;

      if (url.match(regexp)) {
        const matches = Array.from(url.matchAll(regexp));
        for (let i = matches.length - 1; i >= 0; i--) {
          const match = matches[i];
          const begin = match.index + match[0].length;
          const end = url.indexOf('}', begin) + 1;
          let options = url.substring(begin, end);

          try {
            options = JSON.parse(options);
          } catch (e) {
            console.error('Error trying to parse', options);
            console.error(e);
            // remove the placeholder so the url isn't wonky
            url = url.substring(0, begin - match[0].length) + url.substring(end + 1);
            return url;
          }

          if (match[1] === 'array') { // the type of match - array|start|end
            url = this.replaceArray(url, options, match, begin, end);
          } else if (match[1] === 'start' || match[1] === 'end') {
            url = this.replaceDate(url, options, match, begin, end);
          }
        }
      }

      return url;
    },
    getUrl (url) {
      // replaces json parsable values
      url = this.replaceJSON(url);

      return url.replace(/\${indicator}/g, dr.refang(this.query))
        .replace(/\${type}/g, this.itype)
        .replace(/\${numDays}/g, this.numDays)
        .replace(/\${numHours}/g, this.numHours)
        .replace(/\${(stopTs|endTs)}/g, this.stopDate)
        .replace(/\${startTS}/g, this.startDate)
        .replace(/\${(stopDate|endDate)}/g, this.stopDate.split('T')[0])
        .replace(/\${startDate}/g, this.startDate.split('T')[0])
        .replace(/\${(stopEpoch|endEpoch)}/g, new Date(this.stopDate).getTime() / 1000)
        .replace(/\${startEpoch}/g, new Date(this.startDate).getTime() / 1000)
        .replace(/\${(stopSplunk|endSplunk)}/g, moment(this.stopDate).format('MM/DD/YYYY:HH:mm:ss'))
        .replace(/\${startSplunk}/g, moment(this.startDate).format('MM/DD/YYYY:HH:mm:ss'));
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
      this.collapsedLinkGroups[linkGroup._id] = !this.collapsedLinkGroups[linkGroup._id];
      this.$store.commit('SET_COLLAPSED_LINK_GROUPS', this.collapsedLinkGroups);
    }
  },
  mounted () {
    if (this.preUpdatedLinkGroup != null) {
      this.updateLinkGroup(this.updatedLinkGroup);
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
