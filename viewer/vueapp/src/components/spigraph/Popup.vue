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
              :value="getPopupInfo(index).name"
              :expr="field.exp"
              :parse="true"
              :session-btn="true">
            </moloch-session-field>
          </td>
          <td>
            <strong>
              {{ getPopupInfo(index).size | commaString }}
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
    getPopupInfo (index) {
      let info = this.popupInfo;
      const i = index + 1;
      while (info.parent) {
        if (i === info.depth) {
          if (info.data.sizeValue) {
            info.data.size = info.data.sizeValue;
          }
          return info.data;
        }
        info = info.parent;
      }
    }
  }
};
</script>
