<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span>

    <span v-if="!field">
      <span
        class="cursor-help text-danger"
        :id="`field-tooltip-${expr}-${uuid}`">
        <span class="fa fa-exclamation-triangle fa-fw" />
        {{ missingFieldValue }}
        <v-tooltip
          :activator="`#field-tooltip-${expr}-${uuid}`">
          <h6 class="mb-1">
            {{ $t('sessions.field.cantLocate') }}: <strong>{{ expr }}</strong>
          </h6>
          {{ $t('sessions.field.viewerCrashed') }}
          <a
            target="_blank"
            rel="noreferrer noopener nofollow"
            href="https://arkime.com/faq#what-browsers-are-supported">
            {{ $t('sessions.field.unsupportedBrowser') }}</a>?
          <br>
          <em>{{ $t('sessions.field.contactAdmin') }}</em>
        </v-tooltip>
      </span>
    </span>

    <span v-else-if="!field.children && parsed !== undefined">
      <span
        v-for="pd of parsed"
        :key="pd.id">

        <!-- normal parsed value -->
        <span
          v-if="!time"
          class="field cursor-pointer">
          <v-menu
            :location="pullLeft ? 'bottom start' : 'bottom end'"
            :max-width="700"
            @update:model-value="(open) => onMenuToggle(open, pd)">
            <template #activator="{ props: activatorProps }">
              <a
                v-bind="activatorProps"
                class="value">
                <span class="all-copy">{{ pd.value }}</span><span class="fa fa-caret-down" />
              </a>
            </template>
            <v-list
              density="compact"
              class="session-field-list">
              <v-list-item
                @click.stop="fieldClick(expr, pd.queryVal, '==', '&&')"
                :title="'&& ' + expr + ' == ' + pd.value">
                <strong>and</strong>
                {{ pd.value }}
              </v-list-item>
              <v-list-item
                @click.stop="fieldClick(expr, pd.queryVal, '!=', '&&')"
                :title="'&& ' + expr + ' != ' + pd.value">
                <strong>and not</strong>
                {{ pd.value }}
              </v-list-item>
              <v-list-item
                @click.stop="fieldClick(expr, pd.queryVal, '==', '||')"
                :title="'|| ' + expr + ' == ' + pd.value">
                <strong>or</strong>
                {{ pd.value }}
              </v-list-item>
              <v-list-item
                @click.stop="fieldClick(expr, pd.queryVal, '!=', '||')"
                :title="'|| ' + expr + ' != ' + pd.value">
                <strong>or not</strong>
                {{ pd.value }}
              </v-list-item>
              <template v-if="session && field.portField && session[field.portField] !== undefined">
                <v-list-item
                  @click.stop="fieldClick(expr, pd.queryVal + sep + session[field.portField], '==', '&&')"
                  :title="'&& ' + expr + ' == ' + pd.value">
                  <strong>and</strong>
                  {{ pd.value }}{{ sep }}{{ session[field.portField] }}
                </v-list-item>
                <v-list-item
                  @click.stop="fieldClick(expr, pd.queryVal + sep + session[field.portField], '!=', '&&')"
                  :title="'&& ' + expr + ' != ' + pd.value">
                  <strong>and not</strong>
                  {{ pd.value }}{{ sep }}{{ session[field.portField] }}
                </v-list-item>
                <v-list-item
                  @click.stop="fieldClick(expr, pd.queryVal + sep + session[field.portField], '==', '||')"
                  :title="'|| ' + expr + ' == ' + pd.value">
                  <strong>or</strong>
                  {{ pd.value }}{{ sep }}{{ session[field.portField] }}
                </v-list-item>
                <v-list-item
                  @click.stop="fieldClick(expr, pd.queryVal + sep + session[field.portField], '!=', '||')"
                  :title="'|| ' + expr + ' != ' + pd.value">
                  <strong>or not</strong>
                  {{ pd.value }}{{ sep }}{{ session[field.portField] }}
                </v-list-item>
              </template>
              <v-divider />
              <v-list-item
                v-for="(item, key) in menuItems"
                :key="'sync-item-' + key"
                :title="item.name + ' ' + item.value"
                :href="item.url"
                target="_blank">
                <strong>{{ item.name }}</strong>
                {{ item.value }}
              </v-list-item>
              <v-list-item
                v-for="(item, key) in asyncMenuItems"
                :key="'async-item-' + key"
                :title="item.name"
                @click="fetchMenuData(item.url, key)">
                <strong>{{ item.name }}</strong>
                {{ item.value }}
              </v-list-item>
              <v-list-item
                v-if="sessionBtn"
                @click.stop="goToSessions(expr, pd.queryVal, '==')"
                :title="$t('sessions.field.openSessionsTip', { query: expr + ' == ' + pd.queryVal})">
                <span class="fa fa-folder-open-o fa-fw" />
                {{ $t('sessions.field.openSessions') }}
              </v-list-item>
              <v-list-item
                @click.stop="newTabSessions(expr, pd.queryVal, '==')"
                :title="$t('sessions.field.newSessionsTip', { query: expr + ' == ' + pd.queryVal})">
                <span class="fa fa-external-link-square fa-fw" />
                {{ $t('sessions.field.newSessions') }}
              </v-list-item>
              <v-list-item
                v-if="expression"
                class="no-wrap"
                @click.stop="newTabSessions(expr, pd.queryVal, '==', true)"
                :title="$t('sessions.field.newSessionsOnlyTip', { query: expr + ' == ' + pd.queryVal})">
                <span class="fa fa-external-link fa-fw" />
                {{ $t('sessions.field.newSessionsOnly') }}
              </v-list-item>
              <v-list-item
                @click="doCopy(pd.value)"
                :title="$t('common.copyValueTip')">
                <span class="fa fa-clipboard fa-fw" />
                {{ $t('common.copyValue') }}
              </v-list-item>
            </v-list>
          </v-menu>
        </span> <!-- /normal parsed value -->

        <!-- time value -->
        <span
          v-else
          :title="`Click to apply ${field.friendlyName}`"
          class="field time-field cursor-pointer"
          @click="timeClick(expr, pd.queryVal)">
          <a class="value">
            <span class="all-copy">
              {{ pd.value }}
            </span>
          </a>
        </span> <!-- /time value -->

      </span>
    </span>

    <!-- multi-field column -->
    <span v-else-if="session && field.children">
      <!-- info column (is super special & has its own template) -->
      <span v-if="field.dbField === 'info'">
        <arkime-session-info
          :session="session"
          :info-fields="infoFields" />
      </span> <!-- /info column -->
      <!-- recurse on child fields -->
      <span v-else>
        <div
          class="field-children"
          v-for="(child, index) of field.children"
          :key="child.dbField + '-' + index">
          <arkime-session-field
            :field="child"
            :expr="child.exp"
            :value="session[child.dbField]"
            :parse="parse"
            :session="session" />
        </div>
      </span> <!-- /recurse on child fields -->
    </span> <!-- /multi-field column -->

    <!-- just a value (does not correspond to a field; display it but it's not clickable) -->
    <span v-else>
      {{ value }}
    </span> <!-- /just a value -->

  </span>
</template>

<script>
// external imports
import store from '@/store';
// internal imports
import ConfigService from '../utils/ConfigService';
import ArkimeSessionInfo from './SessionInfo.vue';
import Utils from '../utils/utils';
import {
  commaString, timezoneDateString, buildExpression, extractIPv6String, protocol
} from '@common/vueFilters.js';
import { fetchWrapper } from '@common/fetchWrapper.js';

const noCommas = { vlan: true, 'suricata.signatureId': true };

export default {
  name: 'ArkimeSessionField',
  components: { ArkimeSessionInfo },
  props: {
    field: {
      type: Object,
      default: () => ({})
    }, // the field object that describes the field
    expr: {
      type: String,
      default: ''
    }, // the query expression to be put in the search expression
    value: {
      type: [String, Number, Array, Object],
      default: undefined
    }, // the value of the session field (undefined if the field has children)
    session: {
      type: Object,
      default: () => ({})
    }, // the session object
    parse: {
      type: Boolean,
      default: false
    }, // whether to parse the value
    sessionBtn: {
      type: Boolean,
      default: false
    }, // whether to display a button to add the value to the expression and go to sessions page
    pullLeft: {
      type: Boolean,
      default: false
    }, // whether the dropdown should drop down from the left
    infoFields: {
      type: Array,
      default: () => []
    } // info fields to display
  },
  data: function () {
    return {
      openDropdownPd: null,
      menuItems: {},
      asyncMenuItems: {},
      arkimeClickables: undefined,
      menuItemTimeout: null
    };
  },
  watch: {
    // watch route update of time params to rebuild the menu
    '$route.query.date': function (newVal, oldVal) {
      if (this.arkimeClickables) { this.buildMenu(); }
    },
    '$route.query.startTime': function (newVal, oldVal) {
      if (this.arkimeClickables) { this.buildMenu(); }
    },
    '$route.query.stopTime': function (newVal, oldVal) {
      if (this.arkimeClickables) { this.buildMenu(); }
    }
  },
  computed: {
    expression: function () {
      return store.state.expression;
    },
    time: function () {
      if (this.field.type === 'seconds' &&
        (this.expr === 'starttime' || this.expr === 'stoptime')) {
        return true;
      }
      return false;
    },
    sep () {
      const val = this.session?.[this.field?.dbField];
      return val && val.includes(':') ? '.' : ':';
    },
    parsed: function () {
      if (!this.field || this.value === '' || this.value === undefined) { return; }

      let result = {
        queryVal: this.value,
        value: this.value
      };

      // the parsed value is always an array
      if (!Array.isArray(this.value)) { result = [result]; }

      for (let i = 0, len = result.length; i < len; i++) {
        let val = result[i].value;
        let qVal = result[i].queryVal;

        switch (this.field.type) {
        case 'date':
        case 'seconds':
          qVal = val; // save original value as the query value
          val = timezoneDateString(
            parseInt(val),
            store.state.user.settings.timezone,
            store.state.user.settings.ms
          );

          if (this.expr !== 'starttime' && this.expr !== 'stoptime') {
            // only starttime and stoptime fields are applied to time inputs
            qVal = val;
          }
          break;
        case 'lotermfield':
          if (this.field.transform === 'ipv6ToHex') {
            val = extractIPv6String(val);
            qVal = val; // don't save original value (parsed val is query val)
          } else if (this.field.transform === 'ipProtocolLookup') {
            val = protocol(val);
            qVal = val; // don't save original value (parsed val is query val)
          }
          break;
        case 'integer':
          if (this.field.category !== 'port' && !noCommas[this.field.exp]) {
            qVal = val; // save original value as the query value
            val = commaString(val);
          }
          break;
        }

        result[i].value = val; // update parsed value in array
        result[i].queryVal = qVal; // update query value in array
        result[i].id = `${val}-${this.field.dbField}`;
      }

      return result;
    },
    missingFieldValue () {
      return this.value ?? 'unknown';
    },
    uuid () {
      return Utils.createRandomString();
    }
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    /**
     * Triggered when v-menu opens or closes for a parsed datum.
     * On open: track which pd is active and build clickable menu items.
     * On close: clear the menu items.
     * @param {boolean} open  Whether the menu is opening or closing
     * @param {Object} pd     The parsed data object for the clicked value
     */
    onMenuToggle (open, pd) {
      if (open) {
        this.openDropdownPd = pd;
        if (!this.arkimeClickables) {
          ConfigService.getArkimeClickables()
            .then((response) => {
              this.arkimeClickables = response;
              if (!this.arkimeClickables) { return; }
              if (Object.keys(this.arkimeClickables).length !== 0) {
                this.buildMenu();
              }
            });
        } else if (Object.keys(this.arkimeClickables).length !== 0) {
          this.buildMenu();
        }
      } else {
        this.openDropdownPd = null;
        this.clearMenuItems();
      }
    },
    /**
     * Triggered when a time field is clicked
     * Updates the vuex store start/stop time in the search bar
     * @param {string} field The field name ('starttime' || 'stoptime')
     * @param {string} value The value of the field
     */
    timeClick: function (field, value) {
      if (field === 'starttime') {
        value = Math.floor(value / 1000); // seconds not milliseconds
        store.commit('setTime', { startTime: value });
        store.commit('setTimeRange', '0'); // custom time range
      } else {
        value = Math.ceil(value / 1000); // seconds not milliseconds
        store.commit('setTime', { stopTime: value });
        store.commit('setTimeRange', '0'); // custom time range
      }
    },
    /**
     * Triggered when a field's menu item is clicked
     * Emits an event to add an expression to the query in the search bar
     * @param {string} field  The field name
     * @param {string} value  The field value
     * @param {string} op     The relational operator
     */
    fieldClick: function (field, value, op, andor) {
      // v-menu closes automatically on item click; openDropdownPd / menuItems
      // are reset by onMenuToggle when @update:model-value(false) fires.
      value = value.toString();

      const fullExpression = buildExpression(field, value, op);

      store.commit('addToExpression', { expression: fullExpression, op: andor });
    },
    /**
     * Triggered when a the Open in Sessions menu item is clicked for a field
     * Redirects the user to the sessions page with the new expression
     * The url needs to be constructed so there is only one browser history entry
     * @param {string} field  The field name
     * @param {string} value  The field value
     * @param {string} op     The relational operator
     */
    goToSessions: function (field, value, op) {
      this.fieldClick(field, value, op, null);

      this.$router.push({
        path: '/sessions',
        query: {
          ...this.$route.query,
          expression: this.expression
        }
      });
    },
    /**
     * Triggered when a the Open in Sessions New Tab menu item is clicked for a field
     * Opens a new tab of the sessions page with the new expression
     * @param {string} field  The field name
     * @param {string} value  The field value
     * @param {string} op     The relational operator
     * @param {boolean} root  Whether the expression should be added as the root expression
     */
    newTabSessions: function (field, value, op, root) {
      value = value.toString();

      const appendExpression = buildExpression(field, value, op);

      // build new expression
      let newExpression;
      if (!root) {
        newExpression = this.expression || '';
        if (newExpression) { newExpression += ' && '; }
        newExpression += appendExpression;
      } else {
        newExpression = appendExpression;
      }

      const routeData = this.$router.resolve({
        path: '/sessions',
        query: {
          ...this.$route.query,
          expression: newExpression
        }
      });

      window.open(routeData.href, '_blank');
    },
    /**
     * Triggered when a the Copy menu item is clicked for a field
     * Copies the value provided to the user's clipboard and closes the menu
     * @param {string} value The field value
     */
    doCopy: function (value) {
      if (!navigator.clipboard) {
        alert(this.$t('common.clipboardNotSupported', { value: value }));
        return;
      }
      navigator.clipboard.writeText(value);
    },
    /* helper functions ---------------------------------------------------- */
    /**
     * Clears menu items and cancels any pending async menu timeout
     */
    clearMenuItems: function () {
      this.menuItems = {};
      this.asyncMenuItems = {};
      if (this.menuItemTimeout) {
        clearTimeout(this.menuItemTimeout);
        this.menuItemTimeout = null;
      }
    },
    /**
     * Gets info to display the menu for a field
     * @returns {Object} The info to be displayed in the menu
     */
    getInfo: function () {
      if (!this.field) { return { category: [] }; }

      if (Array.isArray(this.field.category)) {
        return { field: this.expr, category: this.field.category, info: this.field };
      } else {
        return { field: this.expr, category: [this.field.category], info: this.field };
      }
    },
    /**
     * Gets Session object
     * @returns {Object} The Session object
     */
    getSession: function () {
      if (!this.session) { return {}; }

      if (this.session instanceof Object) {
        return this.session;
      } else {
        try {
          return JSON.parse(this.session);
        } catch (err) {
          return {};
        }
      }
    },
    /* Briefly replaces menu value with fetched api data */
    fetchMenuData: function (url, key) {
      if (this.menuItemTimeout) {
        return;
      }

      const oldValue = this.asyncMenuItems[key].value;

      fetchWrapper({ url }).then((response) => {
        this.asyncMenuItems[key].value = response;
        this.menuItemTimeout = setTimeout(() => {
          this.asyncMenuItems[key].value = oldValue; // reset the url
          this.menuItemTimeout = null;
        }, 5000);
      }).catch((error) => {
        this.asyncMenuItems[key].value = this.$t('errors.fetchError');
        this.menuItemTimeout = setTimeout(() => {
          this.asyncMenuItems[key].value = oldValue; // reset the url
          this.menuItemTimeout = null;
        }, 5000);
        console.log(error);
      });
    },
    /* Builds the dropdown menu items to display */
    buildMenu: function () {
      const pd = this.openDropdownPd;
      if (!pd || !pd.value || !this.arkimeClickables) { return; }
      // Clear previous menu items before rebuilding for current value
      this.clearMenuItems();
      const info = this.getInfo();
      const session = this.getSession();
      const text = pd.queryVal.toString();
      const url = text.indexOf('?') === -1 ? text : text.substring(0, text.indexOf('?'));
      let host = url;
      let pos = url.indexOf('//');
      const nodehost = session.nodehost || '';
      const nodename = session.node || '';
      const sessionid = session.id || '';

      if (pos >= 0) { host = url.substring(pos + 2); }
      pos = host.indexOf('/');
      if (pos >= 0) { host = host.substring(0, pos); }
      pos = host.indexOf(':');
      if (pos >= 0) { host = host.substring(0, pos); }

      const urlParams = this.$route.query;
      let dateparams, isostart, isostop;

      if (urlParams.startTime && urlParams.stopTime) {
        dateparams = `startTime=${urlParams.startTime}&stopTime=${urlParams.stopTime}`;
        isostart = new Date(parseInt(urlParams.startTime) * 1000);
        isostop = new Date(parseInt(urlParams.stopTime) * 1000);
      } else {
        isostart = new Date();
        isostop = new Date();
        if (urlParams.date) {
          isostart.setHours(isostart.getHours() - parseInt(urlParams.date));
        } else {
          isostart.setHours(isostart.getHours() - 1);
        }
        dateparams = `date=${urlParams.date}`;
      }
      for (const key in this.arkimeClickables) {
        if (this.arkimeClickables[key]) {
          const rc = this.arkimeClickables[key];
          if (rc.all !== true &&
             (!rc.category || !info.category || info.category.filter(x => rc.category.includes(x)).length === 0) &&
             (!rc.fields || rc.fields.indexOf(info.field) === -1)) {
            continue;
          }

          if (this.arkimeClickables[key].func !== undefined) {
            const v = this.arkimeClickables[key].func(key, text);
            if (v !== undefined) {
              this.menuItems[key] = v;
            }
            continue;
          }

          let result = this.arkimeClickables[key].url
            .replace('%EXPRESSION%', encodeURIComponent(urlParams.expression))
            .replace('%DATE%', dateparams)
            .replace('%ISOSTART%', isostart.toISOString())
            .replace('%ISOSTOP%', isostop.toISOString())
            .replace('%FIELD%', info.field)
            .replace('%DBFIELD%', info.info.dbField)
            .replace('%TEXT%', text)
            .replace('%URIEncodedText%', encodeURIComponent(text))
            .replace('%UCTEXT%', text.toUpperCase())
            .replace('%HOST%', host)
            .replace('%URL%', encodeURIComponent('http:' + url))
            .replace('%NODE%', nodename)
            .replace('%NODEHOST%', nodehost)
            .replace('%ID%', sessionid);

          let clickableName = this.arkimeClickables[key].name || key;

          clickableName = (clickableName)
            .replace('%FIELD%', info.field)
            .replace('%DBFIELD%', info.info.dbField)
            .replace('%TEXT%', text)
            .replace('%HOST%', host)
            .replace('%URL%', url);

          let value = '%URL%';
          if (rc.category === 'host') { value = '%HOST%'; }

          value = (value)
            .replace('%FIELD%', info.field)
            .replace('%DBFIELD%', info.info.dbField)
            .replace('%TEXT%', text)
            .replace('%HOST%', host)
            .replace('%URL%', url);

          if (rc.regex) {
            if (!rc.cregex) { rc.cregex = new RegExp(rc.regex); }
            const matches = text.match(rc.cregex);
            if (matches && matches[1]) {
              result = result.replace('%REGEX%', matches[1]);
            } else {
              continue;
            }
          }

          if (this.arkimeClickables[key].actionType !== undefined) {
            if (this.arkimeClickables[key].actionType === 'fetch') {
              this.asyncMenuItems[key] = { name: clickableName, value, url: result };
              continue;
            }
          }

          this.menuItems[key] = { name: clickableName, value, url: result };
        }
      }
    }
  }
};
</script>

<style scoped>
.detail-field .field {
  margin: 0 -6px -2px 2px;
  padding: 2px;
}

.detail-field .field a {
  color: var(--color-foreground-accent);
  word-break: break-word;
}

.field {
  position: relative;
  cursor: pointer;
  z-index: 1;
  display: inline-block;
  padding: 0 1px;
  margin: 0 -4px 0 0;
  border-radius: 3px;
  border: 1px solid transparent;
  max-width: 98%;
  line-height: 1.3;
}

.field:not(.time-field) {
  word-break: break-all;
}

.field a {
  color: var(--color-foreground-accent);
  text-decoration: none;
}

.field a .fa {
  opacity: 0;
  visibility: hidden;
  margin-left: var(--px-xs);
}

.field.time-field {
  display: inline-block;
  margin-right: 6px;
}

.field:hover {
  z-index: 4;
  background-color: var(--color-white);
  border: 1px solid var(--color-gray-light);
}

.field:hover a {
  color: var(--color-black);
}

/* if a user right clicks a value, highlight the entire value */
.field a .all-copy {
  -webkit-user-select: all;
     -moz-user-select: all;
      -ms-user-select: all;
          user-select: all;
}

.field:hover .fa {
  opacity: 1;
  visibility: visible;
}

.field-children:not(:first-child) {
  margin-top: -3px;
}
</style>

<style>
/* The session field menu is rendered via v-menu, which teleports to body --
   scoped styles can't reach it. Global styling lives in overrides.css under
   the Vuetify section (.v-list / .v-list-item). Per-instance overrides here
   only if they need to apply to this specific menu. */
.session-field-list .v-list-item {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
</style>
