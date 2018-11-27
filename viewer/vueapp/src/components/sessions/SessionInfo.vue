<template>

  <div>

    <div v-for="field in infoFields"
      :key="field.dbField">
      <div v-if="session[field.dbField]">
        <strong>
          {{ field.friendlyName }}:
        </strong>
        <span v-if="Array.isArray(session[field.dbField])">
          <span v-for="value in limitArrayLength(session[field.dbField], limit)"
            :key="value">
            <moloch-session-field
              :value="value"
              :session="session"
              :expr="field.exp"
              :field="field.dbField">
            </moloch-session-field>
          </span>
          <a class="cursor-pointer"
            style="text-decoration:none;"
            v-if="session[field.dbField].length > initialLimit"
            @click="toggleShowAll">
            <span v-if="!showAll">
              more...
            </span>
            <span v-else>
              ...less
            </span>
          </a>
        </span>
        <span v-else>
          <moloch-session-field
            :value="session[field.dbField]"
            :session="session"
            :expr="field.exp"
            :field="field.dbField">
          </moloch-session-field>
        </span>
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
