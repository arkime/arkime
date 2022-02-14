<template>
  <!-- form -->
  <!-- TODO clicking items in form collapses the form because it's updating the link group and removing expanded prop
  don't use expanded prop on links? use another object instead? -->
  <b-form v-if="lg">
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
      text="Who Can View"
      class="roles-dropdown mb-2">
      <b-dropdown-form>
        <b-form-checkbox-group
          v-model="lg.viewRoles"
          @change="$emit('update-link-group', lg)">
          <b-form-checkbox
            v-for="role in getRoles"
            :value="role.value"
            :key="role.value">
            {{ role.text }}
            <span
              v-if="role.userDefined"
              class="fa fa-user cursor-help ml-2"
              v-b-tooltip.hover="'User defined role'"
            />
          </b-form-checkbox>
          <template v-for="role in lg.viewRoles">
            <b-form-checkbox
              :key="role"
              :value="role"
              v-if="!getRoles.find(r => r.value === role)">
              {{ role }}
              <span
                class="fa fa-times-circle cursor-help ml-2"
                v-b-tooltip.hover="'This role no longer exists'"
              />
            </b-form-checkbox>
          </template>
        </b-form-checkbox-group>
      </b-dropdown-form>
    </b-dropdown>
    <b-dropdown
      size="sm"
      text="Who Can Edit"
      class="mb-2 roles-dropdown">
      <b-dropdown-form>
        <b-form-checkbox-group
          v-model="lg.editRoles"
          @change="$emit('update-link-group', lg)">
          <b-form-checkbox
            v-for="role in getRoles"
            :value="role.value"
            :key="role.value">
            {{ role.text }}
            <span
              v-if="role.userDefined"
              class="fa fa-user cursor-help ml-2"
              v-b-tooltip.hover="'User defined role'"
            />
          </b-form-checkbox>
          <template v-for="role in lg.editRoles">
            <b-form-checkbox
              :key="role"
              :value="role"
              v-if="!getRoles.find(r => r.value === role)">
              {{ role }}
              <span
                class="fa fa-times-circle cursor-help ml-2"
                v-b-tooltip.hover="'This role no longer exists'"
              />
            </b-form-checkbox>
          </template>
        </b-form-checkbox-group>
      </b-dropdown-form>
    </b-dropdown>
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
        <b-card v-if="link.name != '----------'">
          <div class="d-flex justify-content-between align-items-center">
            <div class="mr-4 flex-grow-1">
              <b-input-group
                size="sm"
                class="mb-2">
                <template #prepend>
                  <b-input-group-text>
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
                @removeLink="removeLink"
                @expandLink="expandLink"
                @addSeparator="addSeparator"
              />
            </div>
          </div>
          <div v-show="link.expanded">
            <b-form-checkbox-group
              v-model="link.itypes"
              :options="itypeOptions"
              @change="$emit('update-link-group', lg)"
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
                @change="$emit('update-link-group', lg)"
              />
              <template #append>
                <b-input-group-text
                  class="cursor-help"
                  v-b-tooltip.hover.html="linkTip">
                  <span class="fa fa-info-circle" />
                </b-input-group-text>
              </template>
            </b-input-group>
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
                @change="$emit('update-link-group', lg)"
                class="text-center link-separator-checkbox-group"
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
                @removeLink="removeLink"
                @expandLink="expandLink"
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
</template>

<script>
import { mapGetters } from 'vuex';

import ColorPicker from '@/utils/ColorPicker';
import ReorderList from '@/utils/ReorderList';

import LinkBtns from '@/components/links/LinkBtns';

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
    ColorPicker,
    ReorderList
  },
  props: {
    linkGroupIndex: {
      type: Number
    }
  },
  data () {
    return {
      lg: this.linkGroupIndex !== undefined ? JSON.parse(JSON.stringify(this.$store.getters.getLinkGroups[this.linkGroupIndex])) : undefined,
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
        title: 'These values within links will be filled in <code>${indicator}</code>, <code>${startDate}</code>, <code>${stopDate}</code>, <code>${startTS}</code>, <code>${stopTS}</code>, <code>${numDays}</code>, <code>${numHours}</code>, <code>${type}</code><br><a target="_blank" href="help#linkgroups">more info</a>'
      }
    };
  },
  computed: {
    ...mapGetters(['getRoles', 'getLinkGroups', 'getUser'])
  },
  watch: {
    linkGroupIndex (oldVal, newVal) {
      if (newVal === undefined) { return undefined; }
      this.lg = JSON.parse(JSON.stringify(this.getLinkGroups[this.linkGroupIndex]));
    }
  },
  created () {
    if (!this.lg) { // creating new link group
      this.lg = { name: '', links: [], viewRoles: [], editRoles: [] };
      this.addLink();
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    addLink (index) {
      this.lg.links.splice(index + 1, 0, JSON.parse(JSON.stringify(defaultLink)));
    },
    addSeparator (index) { // TODO this doesn't always work
      const link = JSON.parse(JSON.stringify(defaultLink));
      link.url = '----------';
      link.name = '----------';
      this.lg.links.splice(index + 1, 0, link);
    },
    pushLink ({ index, target }) {
      // remove the link from the list
      const link = this.lg.links.splice(index, 1)[0];
      // and replace it in the first position
      this.lg.links.splice(target, 0, link);
      this.updateList({ list: this.lg.links });
    },
    removeLink (index) {
      this.lg.links.splice(index, 1);
      this.$emit('update-link-group', this.lg);
    },
    expandLink (index) {
      this.$set(this.lg.links[index], 'expanded', !this.lg.links[index].expanded);
    },
    changeColor ({ linkName, color, index }) {
      const link = this.lg.links[index];
      this.$set(link, 'color', color);
      this.$emit('update-link-group', this.lg);
    },
    updateList ({ list }) {
      this.$set(this.lg, 'links', list);
      this.$emit('update-link-group', this.lg);
      this.$emit('save-link-group', this.lg);
    }
  }
};
</script>

<style scoped>
.alert.alert-sm {
  padding: 0.4rem 0.8rem;
}

.link-handle {
  top: 18px;
  left: -9px;
  z-index: 10;
  padding: 5px 6px;
  position: relative;
  border-radius: 14px;
  background: var(--secondary);
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
</style>
