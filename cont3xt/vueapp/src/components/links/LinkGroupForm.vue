<template>
  <!-- form -->
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
        <b-card class="mb-2"
          v-if="link.name != '----------'">
          <div class="d-flex justify-content-between align-items-center">
            <div class="w-40 mr-4">
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
                    :color="link.color"
                    :link-name="link.name"
                    @colorSelected="changeColor"
                  />
                </template>
              </b-input-group>
            </div>
            <b-form-checkbox-group
              v-model="link.itypes"
              :options="itypeOptions"
              @change="$emit('update-link-group', lg)"
            />
            <b-button
              size="sm"
              variant="danger"
              @click="removeLink(i)"
              v-b-tooltip.hover.left="'Remove this link'">
              <span class="fa fa-times-circle" />
            </b-button>
          </div>
          <b-input-group
            size="sm"
            class="mb-2">
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
        </b-card>
        <template v-else>
          <hr class="link-separator">
          <b-button
            size="sm"
            variant="danger"
            @click="removeLink(i)"
            class="remove-separator"
            v-b-tooltip.hover.left="'Remove this separator'">
            <span class="fa fa-times-circle" />
          </b-button>
        </template>
      </template>
      <div class="d-flex">
        <b-button
          block
          size="sm"
          variant="danger"
          class="mr-1 mt-0"
          style="width:30px;"
          @click="pushLink({ index: i, target: lg.links.length })"
          v-b-tooltip.hover="'Push to the BOTTOM'">
          <span class="fa fa-arrow-circle-down mr-2" />
        </b-button>
        <b-button
          block
          size="sm"
          class="ml-1 mr-1 mt-0"
          variant="secondary"
          @click="addSeparator(i)">
          <span class="fa fa-underline mr-2" />
          Add a Separator
        </b-button>
        <b-button
          block
          size="sm"
          variant="info"
          class="mr-1 ml-1 mt-0"
          @click="addLink(i)">
          <span class="fa fa-link mr-2" />
          Add Another Link
        </b-button>
        <b-button
          block
          size="sm"
          class="ml-1 mt-0"
          variant="warning"
          style="width:30px;"
          @click="pushLink({ index: i, target: 0 })"
          v-b-tooltip.hover="'Push to the TOP'">
          <span class="fa fa-arrow-circle-up mr-2" />
        </b-button>
      </div>
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

const defaultLink = {
  url: '',
  name: '',
  itypes: []
};

export default {
  name: 'CreateLinkGroup',
  components: {
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
    addSeparator (index) {
      const link = JSON.parse(JSON.stringify(defaultLink));
      link.url = '----------';
      link.name = '----------';
      link.itypes = ['domain', 'ip', 'url', 'email', 'hash', 'phone', 'text'];
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
    changeColor ({ linkName, color }) {
      for (const link of this.lg.links) {
        if (link.name === linkName) {
          this.$set(link, 'color', color);
          this.$emit('update-link-group', this.lg);
          return;
        }
      }
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

.remove-separator {
  float: right;
  display: inline;
  margin-top: -34px;
}
</style>
