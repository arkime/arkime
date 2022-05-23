<template>
  <span>
    <b-dropdown-divider
      data-testid="separator"
      v-if="Object.keys(menuItems).length && separator"
    />
    <b-dropdown-item
      v-for="(item, key) in menuItems"
      :key="'sync-item-' + key"
      :title="item.name"
      :href="item.url"
      target="_blank">
      {{ item.name }}
    </b-dropdown-item>
  </span>
</template>

<script>
export default {
  name: 'FieldActions',
  props: {
    expr: { // the field object that describes the field
      type: String,
      required: true
    },
    separator: {
      type: Boolean,
      default: true
    }
  },
  data () {
    return {
      menuItems: {},
      asyncMenuItems: {}
    };
  },
  watch: {
    // watch route update of time params to rebuild the menu
    '$route.query.date' () { this.buildMenu(); },
    '$route.query.stopTime' () { this.buildMenu(); },
    '$route.query.startTime' () { this.buildMenu(); },
    '$route.query.expression' () { this.buildMenu(); },
    fieldActions (value) {
      // build the menu if the field actions have been updated with actions and the menu hasn't already been built
      if (Object.keys(value).length && !Object.keys(this.menuItems).length) {
        this.buildMenu();
      }
    }
  },
  computed: {
    fields () { return this.$store.state.fieldsMap; },
    fieldActions () { return this.$store.state.fieldActions || {}; }
  },
  mounted () {
    this.buildMenu();
  },
  methods: {
    getField (expr) {
      if (!this.fields[expr]) { return; }
      return this.fields[expr];
    },
    /* Builds the dropdown menu items to display */
    buildMenu () {
      const field = this.getField(this.expr);

      // nothing to build if we can't find the field or there are no field actions
      if (!field || Object.keys(this.fieldActions).length === 0) { return; }

      // make field category an array if it isn't
      field.category = Array.isArray(field.category) ? field.category : [field.category];

      // parse url params to get date/start/stop
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
        dateparams = urlParams.date;
      }

      for (const key in this.fieldActions) {
        const action = this.fieldActions[key];

        const fieldNotInActionCategory = (
          !action.category || !field.category ||
          field.category.filter(x => action.category.includes(x)).length === 0
        );
        const fieldNotInActionFields = !action.fields || action.fields.indexOf(field.exp) === -1;

        // field action is not applicable to this field because it is not in the action's category or field lists
        if (fieldNotInActionCategory && fieldNotInActionFields) {
          continue;
        }

        // replace placeholder strings in the action url
        const result = action.url
          .replace('%EXPRESSION%', encodeURIComponent(urlParams.expression || ''))
          .replace('%DATE%', dateparams)
          .replace('%ISOSTART%', isostart.toISOString())
          .replace('%ISOSTOP%', isostop.toISOString())
          .replace('%FIELD%', field.exp)
          .replace('%DBFIELD%', field.dbField);

        // set the menu item name for display
        const menuItemName = (action.name || key)
          .replace('%FIELD%', field.exp)
          .replace('%DBFIELD%', field.dbField)
          .replace('%FIELDNAME%', field.friendlyName);

        this.$set(this.menuItems, key, { name: menuItemName, url: result });
      }
    }
  }
};
</script>
