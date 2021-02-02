<template>
  <table class="table table-borderless table-condensed table-sm">
    <thead>
      <tr>
        <th>
          Field
        </th>
        <th>
          Value
        </th>
        <th>
          <a class="pull-right cursor-pointer no-decoration"
            @click="closeInfo">
            <span class="fa fa-close"></span>
          </a>
        </th>
      </tr>
    </thead>
    <tbody>
      <template v-for="(field, index) of fieldList">
        <tr :key="field.exp"
          v-if="index < popupInfo.depth">
          <td>
            {{ field.friendlyName }}
          </td>
          <td>
            <moloch-session-field
              :field="field"
              :value="getPopupInfoValue(index)"
              :expr="field.exp"
              :parse="true"
              :session-btn="true">
            </moloch-session-field>
          </td>
          <td>
            <strong>
              {{ getPopupInfoSize(index) | commaString }}
            </strong>
          </td>
        </tr>
      </template>
    </tbody>
  </table>
</template>

<script>
export default {
  name: 'Popup',
  props: {
    fieldList: Array,
    popupInfo: Object
  },
  methods: {
    closeInfo () {
      this.$emit('closeInfo');
    },
    getPopupInfoValue (index) {
      let info = this.popupInfo;
      let i = index + 1;
      while (info.parent) {
        if (i === info.depth) {
          return info.data.name;
        }
        info = info.parent;
      }
    },
    getPopupInfoSize (index) {
      let info = this.popupInfo;
      let i = index + 1;
      while (info.parent) {
        if (i === info.depth) {
          return info.data.size || info.data.sizeValue;
        }
        info = info.parent;
      }
    }
  }
};
</script>
