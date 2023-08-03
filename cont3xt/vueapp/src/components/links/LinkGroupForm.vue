<template>
  <div>
    <textarea
      rows="20"
      size="sm"
      v-if="rawEditMode"
      :value="rawEditText"
      :disabled="noEdit"
      @input="e => debounceRawEdit(e)"
      class="form-control form-control-sm"
    />
    <!-- form -->
    <b-form v-if="lg && !rawEditMode">
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
        />
      </b-input-group> <!-- /group name -->
      <!-- group roles -->
      <RoleDropdown
        :roles="getRoles"
        display-text="Who Can View"
        :selected-roles="lg.viewRoles"
        @selected-roles-updated="updateViewRoles"
      />
      <RoleDropdown
        :roles="getRoles"
        display-text="Who Can Edit"
        :selected-roles="lg.editRoles"
        @selected-roles-updated="updateEditRoles"
      />
      <span
        class="fa fa-info-circle fa-lg cursor-help ml-2 mr-1"
        v-b-tooltip.hover="'Creators will always be able to view and edit their link groups regardless of the roles selected here.'"
      />
      <span v-if="!lg.creator || lg.creator === getUser.userId">
        As the creator, you can always view and edit your link groups.
      </span>
      <!-- /group roles -->
      <!-- group links -->
      <reorder-list
        :key="i"
        :index="i"
        :list="lg.links"
        @update="updateList"
        v-for="(link, i) in lg.links">
        <template slot="handle">
          <span class="fa fa-bars d-inline link-handle" />
        </template>
        <template slot="default">
          <b-card v-if="link.name !== '----------'">
            <div class="d-flex justify-content-between align-items-center">
              <div class="mr-2">
                <ToggleBtn
                  class="lg-toggle-btn"
                  @toggle="expandLink(i)"
                  :opened="lg.links[i].expanded"
                  :class="{expanded: lg.links[i].expanded}"
                />
              </div>
              <div class="mr-2 flex-grow-1">
                <b-input-group
                  size="sm">
                  <template #prepend>
                    <b-input-group-text>
                      Name
                    </b-input-group-text>
                  </template>
                  <b-form-input
                    trim
                    v-model="link.name"
                    :state="link.name.length > 0"
                    @input="e => linkChange(i, { name: e })"
                  />
                  <template #append>
                    <color-picker
                      :index="i"
                      :color="link.color"
                      :link-name="link.name"
                      @colorSelected="changeColor"
                    />
                  </template>
                </b-input-group>
              </div>
              <div>
                <LinkBtns
                  :index="i"
                  :link-group="lg"
                  @addLink="addLink"
                  @pushLink="pushLink"
                  @copyLink="copyLink"
                  @removeLink="removeLink"
                  @addSeparator="addSeparator"
                />
              </div>
            </div>
            <div v-show="link.expanded">
              <b-form-checkbox-group
                class="mt-1"
                v-model="link.itypes"
                :options="itypeOptions"
                @change="e => linkChange(i, { itypes: e })"
              />
              <b-input-group
                size="sm"
                class="mb-2 mt-2">
                <template #prepend>
                  <b-input-group-text>
                    URL
                  </b-input-group-text>
                </template>
                <b-form-input
                  trim
                  v-model="link.url"
                  :state="link.url.length > 0"
                  @input="e => linkChange(i, { url: e })"
                />
                <template #append>
                  <b-input-group-text
                    class="cursor-help"
                    v-b-tooltip.hover.html="linkTip">
                    <span class="fa fa-info-circle" />
                  </b-input-group-text>
                </template>
              </b-input-group>
              <b-input-group
                  size="sm"
                  class="mb-2 mt-2">
                <template #prepend>
                  <b-input-group-text>
                    Description
                  </b-input-group-text>
                </template>
                <b-form-input
                    trim
                    v-model="link.infoField"
                    :state="link.infoField ? true : undefined"
                    @input="e => linkChange(i, { infoField: e })"
                />
                <template #append>
                  <b-input-group-text
                      class="cursor-help"
                      v-b-tooltip.hover.html="linkInfoTip">
                    <span class="fa fa-info-circle" />
                  </b-input-group-text>
                </template>
              </b-input-group>
              <div class="d-flex">
                <b-input-group
                    size="sm"
                    class="mb-2 mt-2 w-40">
                  <template #prepend>
                    <b-input-group-text>
                      External Doc Name
                    </b-input-group-text>
                  </template>
                  <b-form-input
                      trim
                      v-model="link.externalDocName"
                      :state="link.externalDocName ? true : undefined"
                      @input="e => linkChange(i, { externalDocName: e })"
                  />
                  <template #append>
                    <b-input-group-text
                        class="cursor-help"
                        v-b-tooltip.hover.html="linkExternalDocNameTip">
                      <span class="fa fa-info-circle" />
                    </b-input-group-text>
                  </template>
                </b-input-group>
                <b-input-group
                    size="sm"
                    class="mb-2 mt-2 ml-2">
                  <template #prepend>
                    <b-input-group-text>
                      External Doc URL
                    </b-input-group-text>
                  </template>
                  <b-form-input
                      trim
                      v-model="link.externalDocUrl"
                      :state="externalDocWarningSuccessState(link.externalDocName, link.externalDocUrl)"
                      @change="e => linkChange(i, { externalDocUrl: e })"
                  />
                  <template #append>
                    <b-input-group-text
                        class="cursor-help"
                        v-b-tooltip.hover.html="linkExternalDocUrlTip">
                      <span class="fa fa-info-circle" />
                    </b-input-group-text>
                  </template>
                </b-input-group>
              </div>
            </div>
          </b-card>
          <template v-else>
            <div class="d-flex justify-content-between align-items-center mr-2">
              <div class="mr-4 flex-grow-1">
                <hr class="link-separator"
                  :style="`border-color: ${link.color || '#777'}`"
                >
                <b-form-checkbox-group
                  v-model="link.itypes"
                  v-show="link.expanded"
                  :options="itypeOptions"
                  @change="e => linkChange(i, { itypes: e })"
                  class="text-center link-separator-checkbox-group mt-1"
                />
              </div>
              <div class="d-flex nowrap">
                <color-picker
                  :index="i"
                  class="d-inline mr-4"
                  :link-name="link.name"
                  @colorSelected="changeColor"
                  :color="link.color || '#777'"
                />
                <LinkBtns
                  :index="i"
                  :link-group="lg"
                  @addLink="addLink"
                  @pushLink="pushLink"
                  @copyLink="copyLink"
                  @removeLink="removeLink"
                  @addSeparator="addSeparator"
                />
              </div>
            </div>
          </template>
        </template>
      </reorder-list> <!-- /group links -->
      <div
        class="mt-2"
        v-if="lg.creator">
        Created by
        <span class="text-info">
          {{ lg.creator }}
        </span>
      </div>
    </b-form> <!-- /form -->
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import ColorPicker from '@/utils/ColorPicker';
import ReorderList from '@/utils/ReorderList';

import LinkBtns from '@/components/links/LinkBtns';
import LinkService from '@/components/services/LinkService';
import RoleDropdown from '@../../../common/vueapp/RoleDropdown';
import ToggleBtn from '../../../../../common/vueapp/ToggleBtn';

let timeout;
const defaultLink = {
  url: '',
  name: '',
  itypes: [],
  expanded: true
};

export default {
  name: 'CreateLinkGroup',
  components: {
    LinkBtns,
    ToggleBtn,
    ColorPicker,
    ReorderList,
    RoleDropdown
  },
  props: {
    linkGroup: Object,
    rawEditMode: {
      type: Boolean,
      default: false
    },
    noEdit: { // for use of disabled raw edit (for view-only privileged users)
      type: Boolean,
      default: false
    }
  },
  data () {
    return {
      rawEditText: undefined,
      lg: !this.linkGroup ? undefined : JSON.parse(JSON.stringify(this.linkGroup)),
      itypeOptions: [
        { text: 'Domain', value: 'domain' },
        { text: 'IP', value: 'ip' },
        { text: 'URL', value: 'url' },
        { text: 'Email', value: 'email' },
        { text: 'Hash', value: 'hash' },
        { text: 'Phone', value: 'phone' },
        { text: 'Text', value: 'text' }
      ],
      dragging: -1,
      draggedOver: undefined,
      linkTip: {
        /* eslint-disable no-template-curly-in-string */
        title: 'These values within links will be filled in <code>${indicator}</code>, <code>${type}</code>, <code>${numDays}</code>, <code>${numHours}</code>, <code>${startDate}</code>, <code>${endDate}</code>, <code>${startTS}</code>, <code>${endTS}</code>, <code>${startEpoch}</code>, <code>${endEpoch}</code>, <code>${startSplunk}</code>, <code>${endSplunk}</code><br><a target="_blank" href="help#linkgroups">more info</a>'
      },
      linkInfoTip: {
        title: 'Use this field to provide guidance about this link. It will be shown as an <span class="fa fa-info-circle cursor-help"></span> tooltip.'
      },
      linkExternalDocUrlTip: {
        title: 'Provide a URL for external documentation relating to this link. It will be accessible via the <span class="fa fa-question-circle cursor-pointer"></span> icon.'
      },
      linkExternalDocNameTip: {
        title: 'Give a name to label the external documentation icon. This will be seen on the <span class="fa fa-question-circle cursor-pointer"></span> icon\'s tooltip. By default, this will be: "External Documentation."'
      }
    };
  },
  computed: {
    ...mapGetters(['getRoles', 'getUser', 'getLinkGroups'])
  },
  created () {
    if (!this.lg) { // creating new link group
      this.lg = { name: '', links: [], viewRoles: [], editRoles: [] };
      this.addLink();
    }
  },
  watch: {
    linkGroup () {
      if (this.linkGroup) { // link group updated from parent
        this.lg = JSON.parse(JSON.stringify(this.linkGroup));
      }
    },
    'linkGroup._id' () {
      if (this.linkGroup) {
        this.lg = JSON.parse(JSON.stringify(this.linkGroup));
      }
    },
    'lg.name' () {
      this.$emit('update-link-group', this.lg);
    },
    rawEditMode: {
      handler (newVal) {
        if (!newVal) {
          // nothing to parse (initial load)
          if (this.rawEditText == null) { return; }

          try { // need to update local lg from json input
            this.lg = JSON.parse(this.rawEditText);
            if (this.linkGroup) { // preserve uneditable/system fields for parent
              this.lg._id = this.linkGroup._id;
              this.lg.creator = this.linkGroup.creator;
              this.lg._editable = this.linkGroup._editable;
            }
          } catch (err) {
            console.warn('Invalid JSON for raw link group', err);
            this.$store.commit('SET_LINK_GROUPS_ERROR', 'Invalid JSON');
          }
          // clear rawEditText to be parsed again if rawEditMode triggered
          this.rawEditText = undefined;
          return;
        }

        // remove uneditable fields
        const clone = JSON.parse(JSON.stringify(this.lg));
        delete clone._id;
        delete clone.creator;
        delete clone._editable;

        this.rawEditText = JSON.stringify(clone, null, 2);
      },
      immediate: true
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    externalDocWarningSuccessState (docName, docUrl) {
      if (docUrl) { return true; }
      if (docName && !docUrl) { return false; }
      return undefined;
    },
    linkChange (index, changeFieldObj) {
      const links = [...this.lg.links];
      links[index] = { ...links[index], ...changeFieldObj };
      this.$emit('update-link-group', { ...this.lg, links });
    },
    addLink (index) {
      this.lg.links.splice(index + 1, 0, JSON.parse(JSON.stringify(defaultLink)));
      this.$emit('update-link-group', this.lg);
    },
    addSeparator (index) {
      const link = JSON.parse(JSON.stringify(defaultLink));
      link.url = '----------';
      link.name = '----------';
      this.lg.links.splice(index + 1, 0, link);
      this.$emit('update-link-group', this.lg);
    },
    pushLink ({ index, target }) {
      // remove the link from the list
      const link = this.lg.links.splice(index, 1)[0];
      // and replace it in the first position
      this.lg.links.splice(target, 0, link);
      this.updateList({ list: this.lg.links });
    },
    copyLink ({ link, groupId }) {
      const linkGroup = this.getLinkGroups.find((group) => group._id === groupId);
      linkGroup.links.push(link);

      this.$store.commit('UPDATE_LINK_GROUP', linkGroup);

      LinkService.updateLinkGroup(linkGroup).then(() => {
        this.$emit('display-message', `Link added to the end of ${linkGroup.name}`);
      }); // store deals with failure
    },
    removeLink (index) {
      this.lg.links.splice(index, 1);
      this.$emit('update-link-group', this.lg);
    },
    expandLink (index) {
      this.$set(this.lg.links[index], 'expanded', !this.lg.links[index].expanded);
    },
    changeColor ({ color, index }) {
      const link = this.lg.links[index];
      this.$set(link, 'color', color);
      this.$emit('update-link-group', this.lg);
    },
    updateList ({ list }) {
      this.$set(this.lg, 'links', list);
      this.$emit('update-link-group', this.lg);
    },
    updateViewRoles (roles) {
      this.$set(this.lg, 'viewRoles', roles);
      this.$emit('update-link-group', this.lg);
    },
    updateEditRoles (roles) {
      this.$set(this.lg, 'editRoles', roles);
      this.$emit('update-link-group', this.lg);
    },
    debounceRawEdit (e) {
      this.rawEditText = e.target.value;
      if (timeout) { clearTimeout(timeout); }
      // debounce the textarea so it only updates the link group after keyups cease for 400ms
      timeout = setTimeout(() => {
        timeout = null;
        this.updateRawLinkGroup();
      }, 400);
    },
    // helper functions ---------------------------------------------------- */
    updateRawLinkGroup () {
      try {
        const linkGroupFromRaw = JSON.parse(this.rawEditText);
        this.$emit('update-link-group', {
          ...this.lg,
          name: linkGroupFromRaw.name,
          links: linkGroupFromRaw.links,
          viewRoles: linkGroupFromRaw.viewRoles,
          editRoles: linkGroupFromRaw.editRoles
        });
      } catch (err) {
        console.warn('Invalid JSON for raw link group', err);
        this.$store.commit('SET_LINK_GROUPS_ERROR', 'Invalid JSON');
      }
    }
  }
};
</script>

<style scoped>
.alert.alert-sm {
  padding: 0.4rem 0.8rem;
}

.link-separator {
  border: 0;
  margin-top: -12px;
  margin-bottom: 0px;
  border-top: 6px solid rgba(0, 0, 0, 0.7);
}
.link-separator-checkbox-group {
  margin-bottom: -1.1rem;
}

.lg-toggle-btn {
  font-size: 1rem;
  line-height: 1.5;
  padding: 0.1rem 0.5rem;
}
</style>
