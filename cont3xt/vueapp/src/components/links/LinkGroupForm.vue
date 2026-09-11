<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-flex flex-column">
    <textarea
      rows="20"
      size="sm"
      v-if="rawEditMode"
      :value="rawEditText"
      :disabled="noEdit"
      @input="e => debounceRawEdit(e)"
      class="cont3xt-textarea" />
    <!-- form -->
    <v-form v-if="lg && !rawEditMode">
      <!-- group name -->
      <trimmed-text-field
        class="mb-2"
        :label="$t('cont3xt.linkGroups.groupName')"
        v-model="lg.name"
        :rules="[lg.name.length > 0]" /> <!-- /group name -->
      <!-- group roles -->
      <div class="d-flex align-center">
        <RoleDropdown
          :roles="getRoles"
          :display-text="$t('cont3xt.whoCanView')"
          :selected-roles="lg.viewRoles"
          @selected-roles-updated="updateViewRoles" />
        <RoleDropdown
          class="ms-1"
          :roles="getRoles"
          :display-text="$t('cont3xt.whoCanEdit')"
          :selected-roles="lg.editRoles"
          @selected-roles-updated="updateEditRoles" />
        <v-icon
          size="large"
          icon="mdi-information"
          class="cursor-help ms-2 me-1"
          v-tooltip="$t('cont3xt.linkGroups.rolesTip')" />
        <span v-if="!lg.creator || lg.creator === getUser.userId">
          {{ $t('cont3xt.linkGroups.creatorNote') }}
        </span>
      </div>
      <!-- /group roles -->
      <!-- group links -->
      <drag-update-list
        class="d-flex flex-column ga-3 mt-3"
        :value="lg.links"
        @update="updateList">
        <div
          v-for="(link, i) in lg.links"
          :key="i"
          class="position-relative">
          <v-icon
            icon="mdi-menu"
            class="d-inline link-handle drag-handle" />

          <v-card
            v-if="link.name !== '----------'"
            variant="tonal"
            class="pa-2">
            <div class="d-flex justify-space-between align-center">
              <div class="me-2">
                <ToggleBtn
                  class="lg-toggle-btn"
                  @toggle="expandLink(i)"
                  :opened="lg.links[i].expanded"
                  :class="{expanded: lg.links[i].expanded}" />
              </div>
              <div class="me-2 flex-grow-1 d-flex flex-row">
                <trimmed-text-field
                  class="input-connect-right small-input"
                  :label="$t('cont3xt.linkGroups.linkName')"
                  v-model="link.name"
                  :rules="[link.name.length > 0]"
                  @update:model-value="val => linkChange(i, { name: val })" />
                <color-picker
                  style="height: 32px !important;"
                  class="btn-connect-left"
                  flat
                  :index="i"
                  :color="link.color"
                  :link-name="link.name"
                  @color-selected="changeColor" />
              </div>
              <div>
                <LinkBtns
                  :index="i"
                  :link-group="lg"
                  @add-link="addLink"
                  @push-link="pushLink"
                  @copy-link="copyLink"
                  @remove-link="removeLink"
                  @add-separator="addSeparator" />
              </div>
            </div>
            <div
              v-if="link.expanded"
              class="d-flex flex-column ga-2">
              <div class="d-flex flex-row ga-2">
                <v-checkbox
                  v-for="itypeOption in itypeOptions"
                  :key="itypeOption.value"
                  v-model="link.itypes"
                  :value="itypeOption.value"
                  :label="itypeOption.text"
                  @update:model-value="val => linkChange(i, { itypes: val })"
                  class="text-center mt-1" />
              </div>
              <trimmed-text-field
                :label="$t('cont3xt.linkGroups.url')"
                class="small-input"
                v-model="link.url"
                :rules="[link.url.length > 0]"
                @update:model-value="val => linkChange(i, { url: val })">
                <template #append-inner>
                  <html-tooltip :html="linkTip" />
                  <v-icon
                    icon="mdi-information"
                    class="cursor-help" />
                </template>
              </trimmed-text-field>
              <trimmed-text-field
                :label="$t('cont3xt.linkGroups.description')"
                class="small-input"
                v-model="link.infoField"
                @update:model-value="val => linkChange(i, { infoField: val })">
                <template #append-inner>
                  <html-tooltip :html="linkInfoTip" />
                  <v-icon
                    icon="mdi-information"
                    class="cursor-help" />
                </template>
              </trimmed-text-field>
              <div class="d-flex flex-row ga-1">
                <trimmed-text-field
                  :label="$t('cont3xt.linkGroups.externalDocName')"
                  class="flex-grow-1 small-input"
                  v-model="link.externalDocName"
                  @update:model-value="val => linkChange(i, { externalDocName: val })">
                  <template #append-inner>
                    <html-tooltip :html="linkExternalDocNameTip" />
                    <v-icon
                      icon="mdi-information"
                      class="cursor-help" />
                  </template>
                </trimmed-text-field>
                <trimmed-text-field
                  :label="$t('cont3xt.linkGroups.externalDocUrl')"
                  class="flew-grow-1 small-input"
                  v-model="link.externalDocUrl"
                  @update:model-value="val => linkChange(i, { externalDocUrl: val })">
                  <template #append-inner>
                    <html-tooltip :html="linkExternalDocUrlTip" />
                    <v-icon
                      icon="mdi-information"
                      class="cursor-help" />
                  </template>
                </trimmed-text-field>
              </div>
            </div>
          </v-card>
          <template v-else>
            <div class="d-flex justify-space-between align-center me-2">
              <div class="me-4 flex-grow-1">
                <hr
                  class="link-separator"
                  :style="`border-color: ${link.color || '#777'}`">
                <div class="d-flex flex-row ga-2 justify-center">
                  <v-checkbox
                    v-for="itypeOption in itypeOptions"
                    :key="itypeOption.value"
                    v-model="link.itypes"
                    :value="itypeOption.value"
                    :label="itypeOption.text"
                    @update:model-value="e => linkChange(i, { itypes: e })"
                    class="text-center mt-1" />
                </div>
              </div>
              <div class="d-flex nowrap">
                <color-picker
                  :index="i"
                  class="d-inline me-2"
                  :link-name="link.name"
                  @color-selected="changeColor"
                  :color="link.color || '#777'" />
                <LinkBtns
                  :index="i"
                  :link-group="lg"
                  @add-link="addLink"
                  @push-link="pushLink"
                  @copy-link="copyLink"
                  @remove-link="removeLink"
                  @add-separator="addSeparator" />
              </div>
            </div>
          </template>
        </div>
      </drag-update-list>

      <div
        class="mt-2"
        v-if="lg.creator">
        {{ $t('cont3xt.createdBy') }}
        <span class="text-info">
          {{ lg.creator }}
        </span>
      </div>
    </v-form> <!-- /form -->
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import ColorPicker from '@/utils/ColorPicker.vue';
import DragUpdateList from '@/utils/DragUpdateList.vue';
import TrimmedTextField from '@/utils/TrimmedTextField.vue';

import HtmlTooltip from '@common/HtmlTooltip.vue';

import LinkBtns from '@/components/links/LinkBtns.vue';
import LinkService from '@/components/services/LinkService';
import RoleDropdown from '@common/RoleDropdown.vue';
import ToggleBtn from '@common/ToggleBtn.vue';

let timeout;
const defaultLink = {
  url: '',
  name: '',
  itypes: [],
  expanded: true
};

export default {
  name: 'CreateLinkGroup',
  emits: ['update-link-group', 'display-message'],
  components: {
    LinkBtns,
    ToggleBtn,
    TrimmedTextField,
    HtmlTooltip,
    ColorPicker,
    DragUpdateList,
    RoleDropdown
  },
  props: {
    linkGroup: {
      type: Object,
      default: () => ({})
    },
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
      lg: (!this.linkGroup || !this.linkGroup._id) ? undefined : JSON.parse(JSON.stringify(this.linkGroup)),
      dragging: -1,
      draggedOver: undefined
    };
  },
  computed: {
    ...mapGetters(['getRoles', 'getUser', 'getLinkGroups']),
    itypeOptions () {
      // explicit order -- not iTypes, which lists phone before hash
      return ['domain', 'ip', 'url', 'email', 'hash', 'phone', 'text']
        .map(itype => ({ text: this.$t(`cont3xt.itypeNames.${itype}`), value: itype }));
    },
    linkTip () {
      return { title: this.$t('cont3xt.linkGroups.urlTipHtml') };
    },
    linkInfoTip () {
      return { title: this.$t('cont3xt.linkGroups.descriptionTipHtml') };
    },
    linkExternalDocUrlTip () {
      return { title: this.$t('cont3xt.linkGroups.externalDocUrlTipHtml') };
    },
    linkExternalDocNameTip () {
      return { title: this.$t('cont3xt.linkGroups.externalDocNameTipHtml') };
    }
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
            this.$store.commit('SET_LINK_GROUPS_ERROR', this.$t('cont3xt.invalidJson'));
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
      this.updateList({ newList: this.lg.links });
    },
    copyLink ({ link, groupId }) {
      const linkGroup = this.getLinkGroups.find((group) => group._id === groupId);
      linkGroup.links.push(link);

      this.$store.commit('UPDATE_LINK_GROUP', linkGroup);

      LinkService.updateLinkGroup(linkGroup).then(() => {
        this.$emit('display-message', this.$t('cont3xt.linkGroups.linkCopied', { name: linkGroup.name }));
      }); // store deals with failure
    },
    removeLink (index) {
      this.lg.links.splice(index, 1);
      this.$emit('update-link-group', this.lg);
    },
    expandLink (index) {
      this.lg.links[index].expanded = !this.lg.links[index].expanded;
    },
    changeColor ({ color, index }) {
      const link = this.lg.links[index];
      link.color = color;
      this.$emit('update-link-group', this.lg);
    },
    updateList ({ newList }) {
      this.lg.links = newList;
      this.$emit('update-link-group', this.lg);
    },
    updateViewRoles (roles) {
      this.lg.viewRoles = roles;
      this.$emit('update-link-group', this.lg);
    },
    updateEditRoles (roles) {
      this.lg.editRoles = roles;
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
        this.$store.commit('SET_LINK_GROUPS_ERROR', this.$t('cont3xt.invalidJson'));
      }
    }
  }
};
</script>

<style scoped>
.link-separator {
  border: 0;
  margin-block: 0;
  border-top: 6px solid rgba(0, 0, 0, 0.7);
}

.lg-toggle-btn {
  font-size: 1rem;
  line-height: 1.5;
  padding: 0.1rem 0.5rem;
}
</style>
