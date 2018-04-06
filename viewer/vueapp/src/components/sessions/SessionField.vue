<template>

  <span>

    <span v-if="!field.children && parsed !== undefined">
      <span v-for="pd of parsed"
        :key="pd.id">
        <!-- normal parsed value -->
        <span v-if="!time" class="field dropdown">
          <a href class="value"
            :class="{'small':(field.exp === 'ip.dst' || field.exp === 'ip.src') && pd.value.includes(':')}">
            <span class="all-copy">{{ pd.value }}</span><span class="fa fa-caret-down"></span>
          </a>
        </span> <!-- /normal parsed value -->
        <!-- time value -->
        <span v-else
          class="field time-field cursor-pointer"
          title="Click to apply time constraint"
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
    'timezone' // what timezone date fields should be in ('gmt' or 'local')
    // TODO pullLeft? sessionBtn?
  ],
  computed: {
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
        result[i].id = `${this.session.id}-${this.field.dbField}`;

        return result;
      }
    }
  },
  methods: {
    /**
     * Triggered when a time field is clicked
     * Updates the vuex store start/stop time in the search bar
     * @param {string} field The field name ('starttime' || 'stoptime')
     * @param {string} value The value of the field
     */
    timeClick: function (field, value) {
      value = Math.floor(value / 1000); // seconds not milliseconds
      if (field === 'starttime') {
        this.$store.commit('setStartTime', value);
      } else {
        this.$store.commit('setStopTime', value);
      }
    }
  }
};
</script>

<style scoped>
.detail-field .field {
  margin: 2px -6px 0 -1px;
  padding: 2px;
}

.detail-field .field a {
  color: var(--color-foreground-accent);
  word-break: break-word;
}

.field {
  cursor: pointer;
  z-index: 1;
  display: inline-block;
  padding: 0 1px;
  margin: 0 -4px 0 0;
  border-radius: 3px;
  border: 1px solid transparent;
  max-width: 100%;
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
ul.session-field-dropdown {
  opacity: 0;
  visibility: hidden;
  max-width: 700px;
  min-width: 160px;
  max-height: 300px;
  overflow-y: auto;
  font-size: 1.25rem;
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

          background-clip: padding-box;
  -webkit-background-clip: padding-box;

          box-shadow: 0 6px 12px -3px #333;
  -webkit-box-shadow: 0 6px 12px -3px #333;
}

ul.session-field-dropdown.pull-right {
  right: 0;
  left: auto;
}
ul.session-field-dropdown.pull-left {
  left: 0;
  right: auto;
}

ul.session-field-dropdown div > li > a {
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

ul.session-field-dropdown div > li > a:hover {
  text-decoration: none;
  color: var(--color-black);
  background-color: var(--color-gray-lighter);
}
</style>
