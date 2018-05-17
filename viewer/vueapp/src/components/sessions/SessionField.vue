<template>

  <span>

    <span v-if="!field.children && parsed !== undefined">
      <span v-for="pd of parsed"
        :key="pd.id">

        <!-- normal parsed value -->
        <span v-if="!time"
          class="field cursor-pointer">
          <a @click="toggleDropdown"
            class="value">
            <span class="all-copy">{{ pd.value }}</span><span class="fa fa-caret-down"></span>
          </a>
          <!-- clickable field menu -->
          <div v-if="isOpen"
            class="session-field-dropdown"
            :class="{'pull-right':!pullLeft,'pull-left':pullLeft}">
            <b-dropdown-item
              @click.prevent.stop="fieldClick(expr, pd.queryVal, '==', '&&')"
              :title="'&& ' + expr + ' == ' + pd.value">
              <strong>and</strong>
              {{ pd.value }}
            </b-dropdown-item>
            <b-dropdown-item
              @click.prevent.stop="fieldClick(expr, pd.queryVal, '!=', '&&')"
              :title="'&& ' + expr + ' != ' + pd.value">
              <strong>and not</strong>
              {{ pd.value }}
            </b-dropdown-item>
            <b-dropdown-item
              @click.prevent.stop="fieldClick(expr, pd.queryVal, '==', '||')"
              :title="'|| ' + expr + ' == ' + pd.value">
              <strong>or</strong>
              {{ pd.value }}
            </b-dropdown-item>
            <b-dropdown-item
              @click.prevent.stop="fieldClick(expr, pd.queryVal, '!=', '||')"
              :title="'|| ' + expr + ' != ' + pd.value">
              <strong>or not</strong>
              {{ pd.value }}
            </b-dropdown-item>
            <span v-if="session && field.portField && session[field.portField] !== undefined">
              <b-dropdown-item
                @click.prevent.stop="fieldClick(expr, pd.queryVal + ':' + session[field.portField], '==', '&&')"
                :title="'&& ' + expr + ' == ' + pd.value">
                <strong>and</strong>
                {{ pd.value }}:{{ session[field.portField] }}
              </b-dropdown-item>
              <b-dropdown-item
                @click.prevent.stop="fieldClick(expr, pd.queryVal + ':' + session[field.portField], '!=', '&&')"
                :title="'&& ' + expr + ' != ' + pd.value">
                <strong>and not</strong>
                {{ pd.value }}:{{ session[field.portField] }}
              </b-dropdown-item>
              <b-dropdown-item
                @click.prevent.stop="fieldClick(expr, pd.queryVal + ':' + session[field.portField], '==', '||')"
                :title="'|| ' + expr + ' == ' + pd.value">
                <strong>or</strong>
                {{ pd.value }}:{{ session[field.portField] }}
              </b-dropdown-item>
              <b-dropdown-item
                @click.prevent.stop="fieldClick(expr, pd.queryVal + ':' + session[field.portField], '!=', '||')"
                :title="'|| ' + expr + ' != ' + pd.value">
                <strong>or not</strong>
                {{ pd.value }}:{{ session[field.portField] }}
              </b-dropdown-item>
            </span>
            <b-dropdown-divider></b-dropdown-divider>
            <b-dropdown-item
              v-for="(item, key) in menuItems"
              :key="key"
              :title="item.name + ' ' + item.value"
              :href="item.url"
              target="_blank">
              <strong>{{ item.name }}</strong>
              {{ item.value }}
            </b-dropdown-item>
            <b-dropdown-item
              v-if="sessionBtn"
              @click.prevent.stop="goToSessions(expr, pd.queryVal, '==')"
              :title="'Open in Sessions with ' + expr + ' == ' + pd.queryVal + ' added to the search expression'">
              <span class="fa fa-folder-open-o"></span>&nbsp;
              Open Sessions
            </b-dropdown-item>
            <b-dropdown-item
              @click.prevent.stop="newTabSessions(expr, pd.queryVal, '==')"
              :title="'Open in new Sessions tab with ' + expr + ' == ' + pd.queryVal + ' added to the search expression'">
              <span class="fa fa-external-link-square"></span>&nbsp;
              Open New Sessions Tab
            </b-dropdown-item>
            <b-dropdown-item
              @click="isOpen = false"
              v-clipboard:copy="pd.value"
              title="Copy value to clipboard">
              <span class="fa fa-clipboard"></span>&nbsp;
              Copy value
            </b-dropdown-item>
          </div>
          <!-- /clickable field menu -->
        </span><!-- /normal parsed value -->

        <!-- time value -->
        <span v-else
          class="field time-field cursor-pointer"
          :title="'Click to apply ' + field.friendlyName"
          v-b-tooltip.hover
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
        <moloch-session-info
          :session="session"
          :field="field">
        </moloch-session-info>
      </span> <!-- /info column -->
      <!-- recurse on child fields -->
      <span v-else>
        <div class="field-children"
          v-for="(child, index) of field.children"
          :key="child.dbField + '-' + index">
          <moloch-session-field
            :field="child"
            :expr="child.exp"
            :value="session[child.dbField]"
            :parse="parse"
            :session="session">
          </moloch-session-field>
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
import ConfigService from '../utils/ConfigService';
import MolochSessionInfo from './SessionInfo';

export default {
  name: 'MolochSessionField',
  components: { MolochSessionInfo },
  props: [
    'field', // the field object that describes the field
    'expr', // the query expression to be put in the search expression
    'value', // the value of the session field (undefined if the field has children)
    'session', // the session object
    'parse', // whether to parse the value
    'timezone', // what timezone date fields should be in ('gmt' or 'local')
    'sessionBtn', // whether to display a button to add the value to the expression and go to sessions page
    'pullLeft' // whether the dropdown should drop down from the left
  ],
  data: function () {
    return {
      isOpen: false,
      menuItems: {},
      molochClickables: undefined
    };
  },
  computed: {
    expression: function () {
      return this.$store.state.expression;
    },
    time: function () {
      if (this.field.type === 'seconds' &&
        (this.expr === 'starttime' || this.expr === 'stoptime')) {
        return true;
      }
      return false;
    },
    parsed: function () {
      if (!this.field || this.value === undefined) { return; }

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
          case 'seconds':
            qVal = val; // save original value as the query value
            val = this.$options.filters.timezoneDateString(
              Math.floor(val / 1000),
              this.timezone,
              'YYYY/MM/DD HH:mm:ss z'
            );

            if (this.expr !== 'starttime' && this.expr !== 'stoptime') {
              // only starttime and stoptime fields are applied to time inputs
              qVal = val;
            }
            break;
          case 'lotermfield':
            if (this.field.transform === 'ipv6ToHex') {
              val = this.$options.filters.extractIPv6String(val);
              qVal = val; // don't save original value (parsed val is query val)
            } else if (this.field.transform === 'ipProtocolLookup') {
              val = this.$options.filters.protocol(val);
              qVal = val; // don't save original value (parsed val is query val)
            }
            break;
          case 'integer':
            if (this.field.category !== 'port') {
              qVal = val; // save original value as the query value
              val = this.$options.filters.commaString(val);
            }
            break;
        }

        result[i].value = val; // update parsed value in array
        result[i].queryVal = qVal; // update query value in array
        result[i].id = `${val}-${this.field.dbField}`;

        return result;
      }
    }
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    /**
     * Triggered when a time field is clicked
     * Updates the vuex store start/stop time in the search bar
     * @param {string} field The field name ('starttime' || 'stoptime')
     * @param {string} value The value of the field
     */
    timeClick: function (field, value) {
      value = Math.floor(value / 1000); // seconds not milliseconds
      if (field === 'starttime') {
        this.$store.commit('setTime', { startTime: value });
      } else {
        this.$store.commit('setTime', { stopTime: value });
      }
    },
    /**
     * Toggles the dropdown menu for a field
     * If the dropdown menu is opened for the first time, get more menu options
     */
    toggleDropdown: function () {
      this.isOpen = !this.isOpen;

      if (this.isOpen && !this.molochClickables) {
        ConfigService.getMolochClickables()
          .then((response) => {
            this.molochClickables = response;

            if (Object.keys(this.molochClickables).length !== 0) {
              // add items to the menu if they exist
              this.buildMenu();
            }
          });
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
      this.isOpen = false; // close the dropdown

      let fullExpression = this.buildExpression(field, value, op);

      this.$store.commit('addToExpression', { expression: fullExpression, op: andor });
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
     */
    newTabSessions: function (field, value, op) {
      this.isOpen = false; // close the dropdown

      let appendExpression = this.buildExpression(field, value, op);

      // build new expression
      let newExpression = this.expression || '';
      if (newExpression) { newExpression += ' && '; }
      newExpression += appendExpression;

      let routeData = this.$router.resolve({
        query: {
          ...this.$route.query,
          expression: newExpression
        }
      });

      window.open(routeData.href, '_blank');
    },
    /* helper functions ---------------------------------------------------- */
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
     * Builds an expression for search.
     * Stringifies necessary values and escapes necessary characters
     * @param {string} field  The field name
     * @param {string} value  The field value
     * @param {string} op     The relational operator
     * @returns {string}      The fully built expression
     */
    buildExpression: function (field, value, op) {
      // for values required to be strings in the search expression
      let str = /[^-+a-zA-Z0-9_.@:*?/]+/.test(value);
      // escape unescaped quotes
      value = value.toString().replace(/\\([\s\S])|(")/g, '\\$1$2');
      if (str) { value = `"${value}"`; }

      return `${field} ${op} ${value}`;
    },
    /* Builds the dropdown menu items to display */
    buildMenu: function () {
      if (!this.parsed[0].value || !this.molochClickables) { return; }

      let info = this.getInfo();
      let text = this.parsed[0].queryVal.toString();
      let url = text.indexOf('?') === -1 ? text : text.substring(0, text.indexOf('?'));
      let host = url;
      let pos = url.indexOf('//');

      if (pos >= 0) { host = url.substring(pos + 2); }
      pos = host.indexOf('/');
      if (pos >= 0) { host = host.substring(0, pos); }
      pos = host.indexOf(':');
      if (pos >= 0) { host = host.substring(0, pos); }

      let urlParams = this.$route.query;
      let dateparams, isostart, isostop;
      this.menuItems = {};

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

      for (let key in this.molochClickables) {
        if (this.molochClickables.hasOwnProperty(key)) {
          let rc = this.molochClickables[key];
          if ((!rc.category || info.category.indexOf(rc.category) === -1) &&
             (!rc.fields || rc.fields.indexOf(info.field) === -1)) {
            continue;
          }

          if (this.molochClickables[key].func !== undefined) {
            let v = this.molochClickables[key].func(key, text);
            if (v !== undefined) {
              this.menuItems[key] = v;
            }
            continue;
          }

          let result = this.molochClickables[key].url
            .replace('%EXPRESSION%', encodeURIComponent(urlParams.expression))
            .replace('%DATE%', dateparams)
            .replace('%ISOSTART%', isostart.toISOString())
            .replace('%ISOSTOP%', isostop.toISOString())
            .replace('%FIELD%', info.field)
            .replace('%TEXT%', text)
            .replace('%UCTEXT%', text.toUpperCase())
            .replace('%HOST%', host)
            .replace('%URL%', encodeURIComponent('http:' + url));

          let name = this.molochClickables[key].name || key;

          name = (name)
            .replace('%FIELD%', info.field)
            .replace('%TEXT%', text)
            .replace('%HOST%', host)
            .replace('%URL%', url);

          let value = '%URL%';
          if (rc.category === 'host') { value = '%HOST%'; }

          value = (value)
            .replace('%FIELD%', info.field)
            .replace('%TEXT%', text)
            .replace('%HOST%', host)
            .replace('%URL%', url);

          if (rc.regex) {
            if (!rc.cregex) { rc.cregex = new RegExp(rc.regex); }
            let matches = text.match(rc.cregex);
            if (matches && matches[1]) {
              result = result.replace('%REGEX%', matches[1]);
            } else {
              continue;
            }
          }

          this.menuItems[key] = { name: name, value: value, url: result };
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

.field:hover ul.session-field-dropdown {
  opacity: 1;
  visibility: visible;
}

.field:hover .fa {
  opacity: 1;
  visibility: visible;
}

.field-children:not(:first-child) {
  margin-top: -3px;
}

/* custom session field dropdown styles because we can't use the dropdown-menu
 * class as it is specific to bootstraps dropdown implementation
 * this class is the same as dropdown-menu, but LESS whitespace */
.session-field-dropdown {
  font-size: 12px;
  position: absolute;
  opacity: 0;
  visibility: hidden;
  max-width: 700px;
  min-width: 160px;
  max-height: 300px;
  overflow-y: auto;
  position: absolute;
  z-index: 1000;
  display: block;
  padding: 5px 0;
  text-align: left;
  list-style: none;
  border-radius: 4px;
  background-color: var(--color-white);
  border: 1px solid var(--color-gray-light);
  margin-top: 0;
  margin-left: -2px;

          background-clip: padding-box;
  -webkit-background-clip: padding-box;

          box-shadow: 0 6px 12px -3px #333;
  -webkit-box-shadow: 0 6px 12px -3px #333;
}

.field:hover .session-field-dropdown {
  opacity: 1;
  visibility: visible;;
}

.session-field-dropdown.pull-right {
  right: 0;
  left: auto;
}
.session-field-dropdown.pull-left {
  left: 0;
  right: auto;
}

.session-field-dropdown a {
  overflow: hidden;
  text-overflow: ellipsis;
  display: block;
  padding: 2px 8px;
  clear: both;
  font-weight: normal;
  line-height: 1.42857143;
  color: var(--color-black);
  white-space: nowrap;
}

.session-field-dropdown a:hover {
  text-decoration: none;
  color: var(--color-black);
  background-color: var(--color-gray-lighter);
}
</style>
