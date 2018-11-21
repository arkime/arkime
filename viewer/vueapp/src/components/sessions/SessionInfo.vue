<template>

  <div>

    <div v-for="field in infoFields"
      :key="field">
      <div v-if="session[field]">
        <strong>
          {{ fieldMap[field].friendlyName }}:
        </strong>
        <span v-for="value in limitArrayLength(session[field], limit)"
          :key="value">
          <moloch-session-field
            :value="value"
            :session="session"
            :expr="field"
            :field="fieldMap[field]">
          </moloch-session-field>
        </span>
        <a class="cursor-pointer"
          style="text-decoration:none;"
          v-if="session[field].length > initialLimit"
          @click="toggleShowAll">
          <span v-if="!showAll">
            more...
          </span>
          <span v-else>
            ...less
          </span>
        </a>
      </div>
    </div>

  </div>

</template>

<script>
export default {
  name: 'MolochSessionInfo',
  props: [
    'field', // the field object that describes the field
    'session', // the session object
    'infoFields' // the fields to display as info
  ],
  data: function () {
    return {
      limit: 3,
      initialLimit: 3,
      showAll: false
    };
  },
  computed: {
    fieldMap: function () {
      if (!this.field || !this.field.children) { return {}; }

      let map = {};
      for (let field of this.field.children) {
        map[field.exp] = field;
        if (field.aliases) { // map aliases too
          for (let f of field.aliases) {
            map[f] = field;
          }
        }
      }

      return map;
    }
  },
  methods: {
    toggleShowAll: function () {
      this.showAll = !this.showAll;

      if (this.showAll) {
        this.limit = 9999;
      } else {
        this.limit = 3;
      }
    },
    limitArrayLength: function (array, length) {
      let limitCount = parseInt(length, 10);

      if (limitCount <= 0) {
        return array;
      }

      return array.slice(0, limitCount);
    }
  }
};
</script>
