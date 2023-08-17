<template v-if="linkGroup">
  <!-- view (for con3xt page and users who can view but not edit) -->
  <b-card
    v-if="itype || !(getUser && (getUser.userId === linkGroup.creator || linkGroup._editable || (getUser.roles && getUser.roles.includes('cont3xtAdmin'))))"
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
        <div v-if="!itype && getUser && linkGroup.creator !== getUser.userId" class="pull-right">
          <small>
            You can only view this Link Group
          </small>
          <b-button
              class="ml-1"
              size="sm"
              variant="secondary"
              @click="rawEditMode = !rawEditMode"
              v-b-tooltip.hover="`View ${rawEditMode ? 'form' : 'raw'} configuration for this link group`">
            <span class="fa fa-fw" :class="{'fa-file-text-o': rawEditMode, 'fa-pencil-square-o': !rawEditMode}" />
          </b-button>
        </div>
      </h6>
    </template>
    <b-card-body>
      <div v-show="!collapsedLinkGroups[linkGroup._id]">
        <template v-if="!rawEditMode">
          <template
              v-for="(link, i) in filteredLinks">
            <!-- display link to click -->
            <div class="link-display"
                 :key="link.url + i + 'click'"
                 v-if="itype && link.name !== '----------'">
              <b-form-checkbox
                  inline
                  tabindex="-1"
                  class="link-checkbox"
                  @change="$store.commit('TOGGLE_CHECK_LINK', { lgId: linkGroup._id, lname: link.name })"
                  :checked="getCheckedLinks[linkGroup._id] && getCheckedLinks[linkGroup._id][link.name]"
              />
              <a tabindex="-1"
                 target="_blank"
                 :title="link.name"
                 :href="getUrl(link.url)"
                 :style="link.color ? `color:${link.color}` : ''">
                {{ link.name }}
              </a>
              <link-guidance :link="link" :element-id="`${linkGroup._id}-${i}`" />
            </div> <!-- /display link to click -->
            <!-- display link to view -->
            <div :title="link.name"
                 :key="link.url + i + 'view'"
                 v-else-if="!itype && link.name !== '----------'">
              <strong class="text-warning">
                {{ link.name }}
              </strong>
              <a tabindex="-1"
                 href="javascript:void(0)"
                 :style="link.color ? `color:${link.color}` : ''">
                {{ link.url }}
              </a>
              <link-guidance :link="link" :element-id="`${linkGroup.name}-${i}`" />
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
    </b-card-body>
    <template #footer v-if="itype && !collapsedLinkGroups[linkGroup._id]">
      <div class="w-100 d-flex justify-content-between align-items-start">
        <b-form-checkbox
          tabindex="-1"
          role="checkbox"
          class="mr-2 mt-1"
          v-b-tooltip.hover="'Select All'"
          :checked="allLinksChecked(linkGroup)"
          @change="e => toggleAllLinks(linkGroup, e)">
        </b-form-checkbox>
        <b-button
          block
          size="sm"
          tabindex="-1"
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
      <div class="w-100 d-flex justify-content-between">
        <div>
          <!-- delete button -->
          <transition name="buttons">
            <b-button
              size="sm"
              variant="danger"
              v-if="!confirmDelete"
              @click="confirmDelete = true"
              v-b-tooltip.hover="'Delete this link group'">
              <span class="fa fa-trash" />
            </b-button>
          </transition> <!-- /delete button -->
          <!-- cancel confirm delete button -->
          <transition name="buttons">
            <b-button
              size="sm"
              title="Cancel"
              variant="warning"
              v-b-tooltip.hover
              v-if="confirmDelete"
              @click="confirmDelete = false">
              <span class="fa fa-ban" />
            </b-button>
          </transition> <!-- /cancel confirm delete button -->
          <!-- confirm delete button -->
          <transition name="buttons">
            <b-button
              size="sm"
              variant="danger"
              v-b-tooltip.hover
              title="Are you sure?"
              v-if="confirmDelete"
              @click="deleteLinkGroup(linkGroup._id)">
              <span class="fa fa-check" />
            </b-button>
          </transition> <!-- /confirm delete button -->
        </div>
        <b-alert
          :show="success"
          variant="success"
          class="mb-0 mt-0 alert-sm mr-1 ml-1">
          <span class="fa fa-check mr-2" />
          <template v-if="message">
            {{ message }}
          </template>
          <template v-else>
            Saved!
          </template>
        </b-alert>
        <div>
          <transition name="buttons">
            <b-button
              size="sm"
              variant="secondary"
              @click="rawEditMode = !rawEditMode"
              v-b-tooltip.hover="`Edit ${rawEditMode ? 'form' : 'raw'} configuration for this link group`">
              <span class="fa fa-fw" :class="{'fa-file-text-o': rawEditMode, 'fa-pencil-square-o': !rawEditMode}" />
            </b-button>
          </transition>
          <transition name="buttons">
            <b-button
              size="sm"
              variant="warning"
              v-if="changesMade"
              @click="cancelUpdateLinkGroup(linkGroup)"
              v-b-tooltip.hover="'Cancel unsaved updates'">
              <span class="fa fa-ban" />
            </b-button>
          </transition>
          <transition name="buttons">
            <b-button
              size="sm"
              variant="success"
              v-if="changesMade"
              @click="saveLinkGroup(linkGroup)"
              v-b-tooltip.hover="'Save this link group'">
              <span class="fa fa-save" />
            </b-button>
          </transition>
        </div>
      </div>
    </template>
    <b-card-body>
      <link-group-form
        :raw-edit-mode="rawEditMode"
        :link-group="updatedLinkGroup"
        @display-message="displayMessage"
        @update-link-group="updateLinkGroup"
      />
    </b-card-body>
    <template slot="footer">
      <div class="w-100 d-flex justify-content-between align-items-start">
        <div>
          <!-- delete button -->
          <transition name="buttons">
            <b-button
              size="sm"
              variant="danger"
              v-if="!confirmDelete"
              @click="confirmDelete = true"
              v-b-tooltip.hover="'Delete this link group'">
              <span class="fa fa-trash" />
            </b-button>
          </transition> <!-- /delete button -->
          <!-- cancel confirm delete button -->
          <transition name="buttons">
            <b-button
              size="sm"
              title="Cancel"
              variant="warning"
              v-b-tooltip.hover
              v-if="confirmDelete"
              @click="confirmDelete = false">
              <span class="fa fa-ban" />
            </b-button>
          </transition> <!-- /cancel confirm delete button -->
          <!-- confirm delete button -->
          <transition name="buttons">
            <b-button
              size="sm"
              variant="danger"
              v-b-tooltip.hover
              title="Are you sure?"
              v-if="confirmDelete"
              @click="deleteLinkGroup(linkGroup._id)">
              <span class="fa fa-check" />
            </b-button>
          </transition> <!-- /confirm delete button -->
        </div>
        <b-alert
          variant="success"
          :show="success"
          class="mb-0 mt-0 alert-sm mr-1 ml-1">
          <span class="fa fa-check mr-2" />
          Saved!
        </b-alert>
        <div>
          <transition name="buttons">
            <b-button
              size="sm"
              variant="secondary"
              @click="rawEditMode = !rawEditMode"
              v-b-tooltip.hover="'Toggle raw configuration for this link group'">
              <span class="fa fa-pencil-square-o" />
            </b-button>
          </transition>
          <transition name="buttons">
            <b-button
              size="sm"
              variant="warning"
              v-if="changesMade"
              @click="cancelUpdateLinkGroup(linkGroup)"
              v-b-tooltip.hover="'Cancel unsaved updates'">
              <span class="fa fa-ban" />
            </b-button>
          </transition>
          <transition name="buttons">
            <b-button
              size="sm"
              variant="success"
              v-if="changesMade"
              @click="saveLinkGroup(linkGroup)"
              v-b-tooltip.hover="'Save this link group'">
              <span class="fa fa-save" />
            </b-button>
          </transition>
        </div>
      </div>
    </template>
  </b-card> <!-- /edit -->
</template>

<script>
import moment from 'moment';
import dr from 'defang-refang';
import { mapGetters } from 'vuex';

import LinkService from '@/components/services/LinkService';
import LinkGroupForm from '@/components/links/LinkGroupForm';
import LinkGuidance from '@/utils/LinkGuidance';
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
    }
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
    itype () {
      return this.indicator?.itype;
    },
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
    getUrl (url) {
      const types = ['domain', 'ip', 'url', 'email', 'hash', 'phone', 'text'];
      // replaces ${arrayType} with a comma-separated list of all indicators of that type
      // for example ${arrayIp} gets replaced with all the IPs in the results
      for (const type of types) {
        const matchStr = `\\$\\{array${type.charAt(0).toUpperCase() + type.slice(1)}\\}`;
        if (url.match(new RegExp(matchStr, 'g'))) {
          const array = [];
          for (const key in this.$store.state.indicatorGraph) {
            const indicator = this.$store.state.indicatorGraph[key];
            if (indicator.indicator.itype === type) {
              array.push(indicator.indicator.query);
            }
          }
          if (array.length) {
            url = url.replace(new RegExp(matchStr, 'g'), array.join(','));
          }
        }
      }

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
      this.$set(this.collapsedLinkGroups, linkGroup._id, !this.collapsedLinkGroups[linkGroup._id]);
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
