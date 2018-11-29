<template>

  <div>

    <div v-for="(infoField, index) in infoFieldsClone"
      :key="infoField.dbField + index">
      <div v-if="session[infoField.dbField]">
        <strong>
          {{ infoField.friendlyName }}:
        </strong>
        <span v-if="Array.isArray(session[infoField.dbField])">
          <span v-for="(value, index) in limitArrayLength(session[infoField.dbField], infoField.limit)"
            :key="value + index">
            <moloch-session-field
              :value="value"
              :session="session"
              :expr="infoField.exp"
              :field="infoField.dbField">
            </moloch-session-field>
          </span>
          <a class="cursor-pointer"
            href="javascript:void(0)"
            style="text-decoration:none;"
            v-if="session[infoField.dbField].length > initialLimit"
            @click="toggleShowAll(infoField)">
            <span v-if="!infoField.showAll">
              more...
            </span>
            <span v-else>
              ...less
            </span>
          </a>
        </span>
        <span v-else>
          <moloch-session-field
            :value="session[infoField.dbField]"
            :session="session"
            :expr="infoField.exp"
            :field="infoField.dbField">
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
    'session', // the session object
    'infoFields' // the fields to display as info
  ],
  data: function () {
    return {
      initialLimit: 3,
      infoFieldsClone: JSON.parse(JSON.stringify(this.infoFields))
    };
  },
  methods: {
    toggleShowAll: function (infoField) {
      this.$set(infoField, 'showAll', !infoField.showAll);

      if (infoField.showAll) {
        this.$set(infoField, 'limit', 9999);
      } else {
        this.$set(infoField, 'limit', this.initialLimit);
      }
    },
    limitArrayLength: function (array, length) {
      if (!length) { length = this.initialLimit; }

      let limitCount = parseInt(length, 10);

      if (limitCount <= 0) { return array; }

      return array.slice(0, limitCount);
    }
  }
};
</script>
