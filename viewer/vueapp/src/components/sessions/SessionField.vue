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
          class="field time-field cursor-pointer">
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
      {{ value }}&nbsp;&nbsp;
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

        // TODO case 'ip': (field.exp === 'ip.dst' || field.exp === 'ip.src') && pd.value.includes(':')
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
  }
};
</script>

<style scoped>
</style>
